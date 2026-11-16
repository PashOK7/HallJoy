#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <mmsystem.h>
#pragma comment(lib, "winmm.lib")
#include "midi_output.h"
#include "midi_event_queue.h"
#include "midi_velocity_trace.h"
#include "midi_observation_engine.h"
#include "analog_observation_snapshot.h"
#include "support_log.h"
#include <atomic>
#include <bitset>
#include <mutex>
#include <process.h>

namespace {
using namespace halljoy::midi;
struct Event { std::shared_ptr<const Profile> config; std::uint64_t epoch = 0; std::uint32_t message = 0; };
struct SessionState {
    EventQueue<Event, 8192> queue;
    Engine engine;
    ObservationEngine observed;
    std::shared_ptr<VelocityTrace> velocityTrace;
    std::shared_ptr<halljoy::observation::Journal> history=std::make_shared<halljoy::observation::Journal>();
    std::shared_ptr<halljoy::observation::SnapshotTracker> snapshots=std::make_shared<halljoy::observation::SnapshotTracker>();
    std::array<halljoy::observation::Event,4096> observations{};
    std::uint64_t seenLoss=0;
    std::shared_ptr<const Profile> producerConfig;
    std::uint64_t producerEpoch = 0;
};
std::unique_ptr<SessionState> session;
// Kept after MIDI stop so Open log can include the completed playing session.
std::atomic<std::shared_ptr<VelocityTrace>> velocityTrace;
struct AttackCaptureContext {
    VelocityTrace* trace;
    std::uint64_t epoch, consumerUs;
};
void CaptureAttack(void* raw,const AttackState& key,const MotionTrace& motion,const Profile& config,const AttackContext& input) noexcept {
    auto& context=*static_cast<AttackCaptureContext*>(raw);
    VelocityTraceRecord record;
    record.epoch=context.epoch;record.consumerUs=context.consumerUs;record.context=input;
    record.motion=motion;record.touch=key.last;record.velocity=key.velocity;
    record.reference=FullVelocitySpeed(config);
    record.attack=config.attackMilli;record.release=config.releaseMilli;record.noise=config.noiseMilli;
    record.curve=config.velocityCurve;
    record.curveVersion=config.touchVersion;
    record.curveX=config.velocitySpeed;
    context.trace->Record(record);
    (void)SupportLog_RequestSnapshot(); // Explicit MIDI investigation persists without Open log.
}
void CaptureMessage(VelocityTrace& trace,std::uint64_t epoch,std::uint32_t message,
                    VelocityTraceKind kind,unsigned result) noexcept {
    const unsigned status=message&0xf0;
    if(status!=0x90 && status!=0x80)return;
    VelocityTraceRecord record;
    record.kind=kind;record.epoch=epoch;record.consumerUs=halljoy::observation::NowUs();
    record.context.code=static_cast<std::uint16_t>(Address(message)+1);
    record.velocity=(message>>16)&127;record.status=status;record.result=result;
    trace.Record(record);
    (void)SupportLog_RequestSnapshot();
}

std::atomic<bool> stopping{false}, running{false}, armed{false}, hasOwner{false}, allocated{false}, faulted{false};
std::atomic<void (*)() noexcept> lifecycleWake{nullptr};
std::atomic<std::uint64_t> stopRequestedMs{0};
std::atomic<std::uint64_t> epoch{1}, generation{1}, sentMessages{0}, errors{0}, overflows{0}, sparse{0};
std::atomic<unsigned> errorCode{0}, lastGenerated{0}, lastSent{0};
std::atomic<std::uint64_t> touchAttacks{0}, touchIntervalUs{0}, touchSequence{0};
std::atomic<unsigned> touchSpeed{0}, touchTravel{0}, touchQuality{0}, touchVelocity{0};
std::atomic<std::uint64_t> historyLosses{0},invalidObservations{0},snapshotObservations{0},historySamples{0},maximumHistoryAgeUs{0};
std::atomic<MidiOutputState> state{MidiOutputState::Paused};
std::array<std::atomic<std::uint32_t>, kAddressCount> generated{}, delivered{};
void FlushVelocityTrace() noexcept {
    const auto trace=velocityTrace.load(std::memory_order_acquire);
    if(!trace || !trace->Pending())return;
    try {
        auto batch=std::make_unique<VelocityTrace::Batch>();
        trace->Drain(*batch);
        static bool explained=false;
        if(!explained){
            SupportLog_Evidence("evidence.midi.velocity_trace revision=2 autosave=1 scope=midi_bound_input depth_units=permille speed_units=permille_per_second quality=1:publication,2:snapshot,3:sparse,4:unresolved motion_flags=1:snapshot,2:timestamp_known",false);
            explained=true;
        }
        char line[352]{};
        std::snprintf(line,sizeof line,"evidence.midi.trace_batch records=%u overwritten=%llu contention_total=%llu capacity=%u",
            batch->count,static_cast<unsigned long long>(batch->overwritten),
            static_cast<unsigned long long>(batch->contention),VelocityTrace::kCapacity);
        SupportLog_Evidence(line,false);
        std::snprintf(line,sizeof line,"evidence.midi.trace_health history_samples=%llu history_losses=%llu invalid=%llu snapshot_samples=%llu maximum_age_us=%llu queue_overflows=%llu send_errors=%llu",
            static_cast<unsigned long long>(historySamples.load()),static_cast<unsigned long long>(historyLosses.load()),
            static_cast<unsigned long long>(invalidObservations.load()),static_cast<unsigned long long>(snapshotObservations.load()),
            static_cast<unsigned long long>(maximumHistoryAgeUs.load()),static_cast<unsigned long long>(overflows.load()),
            static_cast<unsigned long long>(errors.load()));
        SupportLog_Evidence(line,false);
        for(unsigned i=0;i<batch->count;++i)
            WriteVelocityTrace(batch->records[i],[](const char* text){SupportLog_Evidence(text,false);});
    } catch (...) {
        SupportLog_Event("midi.trace_export_failed",1);
    }
}
HANDLE thread = nullptr;
volatile LONG wakeSequence = 0;
void Signal() noexcept {
    InterlockedIncrement(&wakeSequence);
    WakeByAddressSingle(const_cast<LONG*>(&wakeSequence));
}
std::mutex lifecycle, producer;
void WakeLifecycle() noexcept { if (auto wake = lifecycleWake.load(std::memory_order_acquire)) wake(); }
#if defined(HALLJOY_MIDI_OUTPUT_TEST)
MidiTestApi testApi{};
std::atomic<std::uint64_t> testStarts{0}, testScans{0}, testTicks{0}, testEnumerations{0};
#endif
unsigned Open(const std::wstring& name, std::uintptr_t& result) {
#if defined(HALLJOY_MIDI_OUTPUT_TEST)
    if (testApi.open) return testApi.open(name, result);
#endif
    unsigned matches = 0; UINT id = 0;
    for (UINT i = 0, count = midiOutGetNumDevs(); i < count; ++i) {
        MIDIOUTCAPSW caps{};
        if (midiOutGetDevCapsW(i, &caps, sizeof(caps)) == MMSYSERR_NOERROR && name == caps.szPname) {
            id = i; ++matches;
        }
    }
    if (matches != 1) return MMSYSERR_BADDEVICEID;
    HMIDIOUT port = nullptr;
    const auto error = midiOutOpen(&port, id, 0, 0, CALLBACK_NULL);
    result = reinterpret_cast<std::uintptr_t>(port);
    return error;
}
unsigned Send(std::uintptr_t port, std::uint32_t message) {
#if defined(HALLJOY_MIDI_OUTPUT_TEST)
    if (testApi.send) return testApi.send(port, message);
#endif
    return midiOutShortMsg(reinterpret_cast<HMIDIOUT>(port), message);
}
void Close(std::uintptr_t port) {
#if defined(HALLJOY_MIDI_OUTPUT_TEST)
    if (testApi.close) { testApi.close(port); return; }
#endif
    (void)midiOutClose(reinterpret_cast<HMIDIOUT>(port));
}
void SetState(MidiOutputState next, unsigned error = 0) noexcept {
    if (state.exchange(next) != next || errorCode.load() != error) {
        errorCode.store(error);
        generation.fetch_add(1, std::memory_order_release);
        SupportLog_Event("midi.output_state", static_cast<unsigned>(next), SupportLog_Data(error));
    }
}
void ClearGenerated() noexcept {
#if defined(HALLJOY_MIDI_OUTPUT_TEST)
    ++testScans;
#endif
    bool changed = false;
    for (auto& value : generated) changed = value.exchange(0, std::memory_order_relaxed) != 0 || changed;
    if (changed) generation.fetch_add(1, std::memory_order_release);
}
unsigned __stdcall Worker(void* context) noexcept {
    auto& work = *static_cast<SessionState*>(context);
#if defined(HALLJOY_MIDI_OUTPUT_TEST)
    ++testStarts;
#endif
    std::uintptr_t port = 0;
    std::shared_ptr<const Profile> current;
    std::bitset<kAddressCount> sent;
    std::bitset<16> usedChannels;
    auto seenEpoch = epoch.load(std::memory_order_acquire);
    std::uint64_t nextRetry = 0;
    auto send = [&](std::uint32_t message) {
        const unsigned intentType = message & 0xF0;
        if (intentType == 0x90) { sent.set(Address(message)); usedChannels.set(Address(message) / 128); }
        const auto error = Send(port, message);
        CaptureMessage(*work.velocityTrace,seenEpoch,message,VelocityTraceKind::Sent,error);
        if (error) { errorCode.store(error); errors.fetch_add(1); return false; }
        sentMessages.fetch_add(1, std::memory_order_relaxed);
        const unsigned address = Address(message), type = message & 0xF0, value = (message >> 16) & 127;
        if (type == 0x90) {
            sent.set(address); usedChannels.set(address / 128);
            lastSent.store((address + 1) | (value << 12), std::memory_order_relaxed);
            delivered[address].store(value, std::memory_order_relaxed);
        } else if (type == 0x80) {
            sent.reset(address); delivered[address].store(0, std::memory_order_relaxed);
        } else if (type == 0xA0) {
            const auto old = delivered[address].load(std::memory_order_relaxed);
            delivered[address].store((old & 255) | (value << 8), std::memory_order_relaxed);
        }
        generation.fetch_add(1, std::memory_order_release);
        return true;
    };
    auto release = [&] {
        if (port) {
            for (unsigned a = 0; a < kAddressCount; ++a)
                if (sent.test(a)) (void)send(Message(0x80, a, 0));
            for (unsigned channel = 0; channel < 16; ++channel)
                if (usedChannels.test(channel)) (void)send(0xB0 | channel | (64u << 8));
        }
        sent.reset(); usedChannels.reset();
        for (auto& value : delivered) value.store(0, std::memory_order_relaxed);
        generation.fetch_add(1, std::memory_order_release);
    };
    auto close = [&] { release(); if (port) { Close(port); port = 0; } };
    try {
        while (!stopping.load(std::memory_order_acquire)) {
            LONG observedWake = InterlockedCompareExchange(&wakeSequence, 0, 0);
            const auto nextEpoch = epoch.load(std::memory_order_acquire);
            if (nextEpoch != seenEpoch) { release(); seenEpoch = nextEpoch; }
            Event event;
            if (work.queue.Pop(event)) {
                // Recheck immediately before I/O; Panic invalidates every queued old attack.
                if (event.epoch != epoch.load(std::memory_order_acquire)) continue;
                if (!current || !SamePerformance(*current, *event.config)) {
                    close(); current = event.config; nextRetry = 0;
                }
                if (!current->enabled) { SetState(MidiOutputState::Disabled); continue; }
                if (current->port.empty()) { SetState(MidiOutputState::Monitor); continue; }
                if (!port && GetTickCount64() >= nextRetry) {
                    const auto error = Open(current->port, port);
                    if (error) {
                        port = 0; nextRetry = GetTickCount64() + 1000;
                        SetState(error == MMSYSERR_BADDEVICEID ? MidiOutputState::MissingPort : MidiOutputState::Error, error);
                    } else {
                        SetState(MidiOutputState::Connected);
                        // Fresh endpoint: do not replay a held key or old queued attacks.
                        epoch.fetch_add(1, std::memory_order_acq_rel);
                        continue;
                    }
                }
                if (port && event.message && event.epoch == epoch.load(std::memory_order_acquire)) {
                    if (!send(event.message)) {
                        const auto error = errorCode.load();
                        close(); SetState(MidiOutputState::Error, error);
                        nextRetry = GetTickCount64() + 1000;
                        epoch.fetch_add(1, std::memory_order_acq_rel);
                    }
                }
            } else {
                // Recovery continues while keys are idle; never reopen from the realtime owner.
                if (current && current->enabled && !current->port.empty() && !port && GetTickCount64() >= nextRetry) {
                    const auto error = Open(current->port, port);
                    nextRetry = GetTickCount64() + 1000;
                    if (!error) { SetState(MidiOutputState::Connected); epoch.fetch_add(1, std::memory_order_acq_rel); }
                    else { port = 0; SetState(error == MMSYSERR_BADDEVICEID ? MidiOutputState::MissingPort : MidiOutputState::Error, error); }
                }
                const BOOL woke = WaitOnAddress(&wakeSequence, &observedWake, sizeof(observedWake), 1000);
                if (!woke && GetLastError() != ERROR_TIMEOUT) throw 1;
            }
        }
    } catch (...) {
        errors.fetch_add(1); SetState(MidiOutputState::Error, ERROR_UNHANDLED_EXCEPTION);
        faulted.store(true, std::memory_order_release);
        SupportLog_Event("midi.worker_failed", 1);
    }
    close();
    running.store(false, std::memory_order_release);
    WakeLifecycle();
    return 0;
}
bool Push(SessionState& work, const std::shared_ptr<const Profile>& config, std::uint64_t value, std::uint32_t message) noexcept {
    if (work.queue.Push({config, value, message})) {
        CaptureMessage(*work.velocityTrace,value,message,VelocityTraceKind::Queued,0);
        Signal(); return true;
    }
    overflows.fetch_add(1, std::memory_order_relaxed);
    epoch.fetch_add(1, std::memory_order_acq_rel);
    Signal();
    SupportLog_Event("midi.queue_overflow", overflows.load());
    return false;
}
}

// Only control owners hold lifecycle. Realtime tries producer without waiting.
namespace {
void ReleaseSession() noexcept {
    CloseHandle(thread); thread = nullptr;
    halljoy::observation::SetCapture(false);
    halljoy::observation::current.store(nullptr,std::memory_order_release);
    halljoy::observation::uap.store(nullptr);
    session.reset(); allocated.store(false, std::memory_order_release);
    hasOwner.store(false, std::memory_order_release);
    ClearGenerated();
}
bool StartSession() noexcept {
    if (!armed.load() || !Get()->enabled || faulted.load()) return !faulted.load();
    try {
#if defined(HALLJOY_MIDI_OUTPUT_TEST)
        if (testApi.rejectStart && testApi.rejectStart()) throw 1;
#endif
        session = std::make_unique<SessionState>();
        auto trace=velocityTrace.load(std::memory_order_acquire);
        if(!trace){
            trace=std::make_shared<VelocityTrace>();
            velocityTrace.store(trace,std::memory_order_release);
        }
        session->velocityTrace=std::move(trace);
        session->engine.EnableDiagnostics();
        session->observed.EnableDiagnostics();
        SupportLog_SetMidiEvidenceProvider(&FlushVelocityTrace);
        halljoy::observation::uap.store(session->snapshots);
        allocated.store(true, std::memory_order_release);
        session->queue.Push({Get(), epoch.load(), 0}); // Connect without requiring an input tick.
        stopping.store(false); stopRequestedMs.store(0); running.store(true);
        thread = reinterpret_cast<HANDLE>(_beginthreadex(nullptr, 0, Worker, session.get(), 0, nullptr));
        if (!thread) throw 1;
        hasOwner.store(true, std::memory_order_release);
        halljoy::observation::current.store(session->history,std::memory_order_release);
        halljoy::observation::SetCapture(true);
        return true;
    } catch (...) {
        running.store(false); session.reset(); allocated.store(false);
        halljoy::observation::uap.store(nullptr);
        faulted.store(true); SetState(MidiOutputState::Error, ERROR_NOT_ENOUGH_MEMORY);
        SupportLog_Event("midi.start_failed", ERROR_NOT_ENOUGH_MEMORY);
        return false;
    }
}
void RequestStop() noexcept {
    halljoy::observation::SetCapture(false);
    halljoy::observation::current.store(nullptr,std::memory_order_release);
    halljoy::observation::uap.store(nullptr,std::memory_order_release);
    // Publish the deadline before the stopping flag so concurrent supervision
    // cannot mistake an uninitialized deadline for a five-second timeout.
    std::uint64_t unset = 0;
    stopRequestedMs.compare_exchange_strong(unset, GetTickCount64(), std::memory_order_acq_rel);
    running.store(false, std::memory_order_release);
    if (!stopping.exchange(true, std::memory_order_acq_rel)) {
        epoch.fetch_add(1, std::memory_order_acq_rel);
        Signal();
    }
}
void ProfileEnabledChanged() noexcept {
    if (!Get()->enabled && hasOwner.load(std::memory_order_acquire)) RequestStop();
    if (armed.load(std::memory_order_acquire)) WakeLifecycle();
}
}
void MidiOutput_SetLifecycleWake(void (*wake)() noexcept) noexcept { lifecycleWake.store(wake); }
bool MidiOutput_Start() noexcept {
    std::lock_guard<std::mutex> lock(lifecycle);
    std::lock_guard<std::mutex> source(producer);
    if (thread) return running.load() && !stopping.load();
    armed.store(true, std::memory_order_release);
    onEnabledChanged.store(&ProfileEnabledChanged, std::memory_order_release);
    faulted.store(false);
    if (!Get()->enabled) { SetState(MidiOutputState::Disabled); return true; }
    return StartSession();
}
bool MidiOutput_Reconcile() noexcept {
    if (!armed.load(std::memory_order_acquire)) return true;
    const bool wanted = Get()->enabled;
    if (wanted && running.load(std::memory_order_acquire) && !stopping.load()) return true;
    if (!wanted && !hasOwner.load(std::memory_order_acquire) && !faulted.load()) return true;
    std::lock_guard<std::mutex> lock(lifecycle);
    std::lock_guard<std::mutex> source(producer);
    if (!armed.load()) return true;
    if (thread) {
        RequestStop();
        const auto wait = WaitForSingleObject(thread, 0); // Never wait on an external driver here.
        if (wait == WAIT_TIMEOUT) {
            if (GetTickCount64() - stopRequestedMs.load() >= 5000 && !faulted.exchange(true)) {
                SetState(MidiOutputState::Error, ERROR_TIMEOUT);
                SupportLog_Event("midi.stop_failed", WAIT_TIMEOUT, SupportLog_Win32(ERROR_TIMEOUT));
            }
            return !faulted.load();
        }
        if (wait != WAIT_OBJECT_0) { SetState(MidiOutputState::Error, GetLastError()); return false; }
        // A preempted HID/bridge publication may still have a short-lived lease.
        // Reap only here, so its last release never frees the large ring on HID.
        if(session->history.use_count()!=1 || session->snapshots.use_count()!=1) {
            if(GetTickCount64()-stopRequestedMs.load()>=5000 && !faulted.exchange(true)) {
                SetState(MidiOutputState::Error,ERROR_TIMEOUT);
                SupportLog_Event("midi.history_stop_failed",1,SupportLog_Win32(ERROR_TIMEOUT));
            }
            return !faulted.load();
        }
        ReleaseSession();
    }
    if (!Get()->enabled) {
        faulted.store(false); SetState(MidiOutputState::Disabled); return true;
    }
    return StartSession();
}
bool MidiOutput_Stop() noexcept {
    std::lock_guard<std::mutex> lock(lifecycle);
    std::lock_guard<std::mutex> source(producer);
    armed.store(false, std::memory_order_release);
    if (!thread) { SetState(MidiOutputState::Paused); return true; }
    RequestStop();
    const auto stopDeadline=GetTickCount64()+5000;
    const DWORD wait = WaitForSingleObject(thread, 5000);
    if (wait != WAIT_OBJECT_0) {
        SupportLog_Event("midi.stop_failed", wait, SupportLog_Win32(wait == WAIT_TIMEOUT ? ERROR_TIMEOUT : GetLastError()));
        return false; // Retain private state and handle until the owner really exits.
    }
    const auto leaseDeadline=stopDeadline;
    while(session->history.use_count()!=1 || session->snapshots.use_count()!=1) {
        if(GetTickCount64()>=leaseDeadline) {
            SupportLog_Event("midi.history_stop_failed",1,SupportLog_Win32(ERROR_TIMEOUT));
            return false; // Retain state until all acquisition references have returned.
        }
        Sleep(1);
    }
    ReleaseSession(); faulted.store(false);
    SetState(MidiOutputState::Paused);
    return true;
}
void MidiOutput_Panic() noexcept {
    if (!hasOwner.load(std::memory_order_acquire)) return;
    epoch.fetch_add(1, std::memory_order_acq_rel);
    ClearGenerated();
    Signal(); // Immediate wake, without borrowed-handle races or external I/O.
}
void MidiOutput_Retry() noexcept {
    // Port recovery is bounded; an explicitly retried start can recover allocation failure.
    if (!hasOwner.load()) faulted.store(false);
    MidiOutput_Panic();
    if (armed.load()) WakeLifecycle();
}
void MidiOutput_Tick(Input (*read)(void*, std::uint16_t), void* context,
                     void (*visit)(void*,halljoy::observation::Sink)) {
    if (!running.load(std::memory_order_acquire)) return;
    std::unique_lock<std::mutex> source(producer, std::try_to_lock);
    if (!source.owns_lock() || !running.load(std::memory_order_acquire)) return;
    const auto config = Get();
    if (!config->enabled) return;
    auto& work = *session;
    auto& engine = work.engine;
    auto& producerConfig = work.producerConfig;
    auto& producerEpoch = work.producerEpoch;
#if defined(HALLJOY_MIDI_OUTPUT_TEST)
    ++testTicks;
#endif
    if (producerConfig && !SamePerformance(*config, *producerConfig)) MidiOutput_Panic();
    const auto value = epoch.load(std::memory_order_acquire);
    if (!producerConfig || !SamePerformance(*config, *producerConfig) || producerEpoch != value) {
        engine.Reset([&](std::uint32_t message) { (void)Push(work, config, value, message); });
        work.observed.Reset([&](std::uint32_t message) { (void)Push(work, config, value, message); });
        work.history->Discard();
        if (!Push(work, config, value, 0)) return;
        producerConfig = config; producerEpoch = value;
    }
    const auto nowUs=halljoy::observation::NowUs();
    AttackCaptureContext capture{work.velocityTrace.get(),value,nowUs};
    const AttackSink diagnostics{&capture,&CaptureAttack};
    bool queueOk = true;
    auto emit=[&](std::uint32_t message) {
        if ((message & 0xF0) == 0x90)
            lastGenerated.store((Address(message) + 1) | (((message >> 16) & 127) << 12), std::memory_order_relaxed);
        if (queueOk) queueOk = Push(work, config, value, message);
    };
    if (!visit) {
        // Compatibility/test callers explicitly have latest-value timing only.
        engine.Tick(*config,nowUs,[&](std::uint16_t hid) {return read(context,hid);},emit,diagnostics);
    } else {
        const auto losses=work.history->lost.load()+work.history->malformed.load();
        if(losses!=work.seenLoss) {
            work.seenLoss=losses;historyLosses.fetch_add(1);
            work.observed.Reset(emit);work.history->Discard();
            SupportLog_Event("midi.history_loss",losses);
            MidiOutput_Panic();return; // Never replay an incomplete old attack backlog.
        }
        // Preserve existing per-key transport demand, but never use the legacy
        // cross-provider maximum as an observed MIDI sample.
        if(read) for(unsigned hid=1;hid<config->keys.size();++hid)
            if(config->keys[hid]) (void)read(context,static_cast<std::uint16_t>(hid));
        work.observed.BeginFrame();
        const auto capacityBefore=work.observed.CapacityLoss();
        struct Routes { ObservationEngine* engine;const Profile* config;decltype(emit)* output; } routes{&work.observed,config.get(),&emit};
        visit(context,{&routes,+[](void* opaque,const halljoy::observation::Event& event) {
            auto& route=*static_cast<Routes*>(opaque);route.engine->Route(event,*route.config,*route.output);
        }});
        const auto count=work.history->Drain(work.observations.data(),work.observations.size());
        for(std::size_t i=0;i<count;++i) {
            const auto& event=work.observations[i];
            if(event.timing==halljoy::observation::Timing::HostSnapshot) ++snapshotObservations;
            work.observed.Replay(event,*config,emit,diagnostics);
            const auto age=nowUs>=event.timeUs?nowUs-event.timeUs:0;
            maximumHistoryAgeUs.store(std::max(maximumHistoryAgeUs.load(),age));
        }
        if(work.observed.CapacityLoss()!=capacityBefore) {
            historyLosses.fetch_add(1);work.observed.Reset(emit);work.history->Discard();
            const auto lossCount=historyLosses.load();
            if((lossCount&(lossCount-1))==0) SupportLog_Event("midi.history_capacity",2048);
            MidiOutput_Panic();return;
        }
        historySamples.fetch_add(count);
        work.observed.EndFrame(*config,emit);
        invalidObservations.store(work.observed.Invalid()+work.observed.CapacityLoss());
    }
    bool changed = false;
    const auto& notes = visit ? work.observed.Notes() : engine.Notes();
    for (unsigned a = 0; a < kAddressCount; ++a) {
        const unsigned packed = notes[a].velocity | (unsigned(notes[a].pressure) << 8);
        changed = generated[a].exchange(packed, std::memory_order_relaxed) != packed || changed;
    }
    sparse.store(visit ? work.observed.SparseAttacks() : engine.SparseAttacks(), std::memory_order_relaxed);
    const auto& touch = visit ? work.observed.Touch() : engine.Touch();
    if (touchAttacks.load(std::memory_order_relaxed) != touch.attacks) {
        touchSequence.fetch_add(1, std::memory_order_acq_rel);
        touchSpeed.store(touch.speedMilli, std::memory_order_relaxed);
        touchTravel.store(touch.travelMilli, std::memory_order_relaxed);
        touchIntervalUs.store(touch.intervalUs, std::memory_order_relaxed);
        touchQuality.store(static_cast<unsigned>(touch.quality), std::memory_order_relaxed);
        touchVelocity.store(touch.velocity,std::memory_order_relaxed);
        touchAttacks.store(touch.attacks, std::memory_order_relaxed);
        touchSequence.fetch_add(1, std::memory_order_release);
        changed = true;
    }
    if (changed) generation.fetch_add(1, std::memory_order_release);
}
std::vector<MidiPort> MidiOutput_Ports() {
    std::vector<MidiPort> ports;
    if (!NeedsAnalog()) return ports;
#if defined(HALLJOY_MIDI_OUTPUT_TEST)
    ++testEnumerations;
#endif
    for (UINT i = 0, count = midiOutGetNumDevs(); i < count; ++i) {
        MIDIOUTCAPSW caps{};
        if (midiOutGetDevCapsW(i, &caps, sizeof(caps)) == MMSYSERR_NOERROR) ports.push_back({caps.szPname, false});
    }
    for (auto& port : ports) {
        unsigned count = 0;
        for (const auto& other : ports) if (port.name == other.name) ++count;
        port.ambiguous = count != 1;
    }
    return ports;
}
void MidiOutput_GetTelemetry(MidiTelemetry& out) noexcept {
    out = {};
    out.state = state.load(); out.error = errorCode.load();
    out.lastGenerated = lastGenerated.load(); out.lastSent = lastSent.load();
    out.sentMessages = sentMessages.load(); out.sendErrors = errors.load();
    out.overflows = overflows.load(); out.sparseAttacks = sparse.load();
    for (unsigned attempt = 0; attempt < 4; ++attempt) {
        const auto before = touchSequence.load(std::memory_order_acquire);
        if (before & 1) continue;
        TouchReport touch{};
        touch.attacks = touchAttacks.load(std::memory_order_relaxed);
        touch.sparse = out.sparseAttacks;
        touch.speedMilli = touchSpeed.load(std::memory_order_relaxed);
        touch.travelMilli = touchTravel.load(std::memory_order_relaxed);
        touch.intervalUs = touchIntervalUs.load(std::memory_order_relaxed);
        touch.quality = static_cast<TouchQuality>(touchQuality.load(std::memory_order_relaxed));
        touch.velocity = touchVelocity.load(std::memory_order_relaxed);
        std::atomic_thread_fence(std::memory_order_acquire);
        if (touchSequence.load(std::memory_order_relaxed) == before) { out.touch = touch; break; }
    }
    out.historyLosses=historyLosses.load();out.invalidObservations=invalidObservations.load();
    out.snapshotObservations=snapshotObservations.load();out.historySamples=historySamples.load();
    out.maximumHistoryAgeUs=maximumHistoryAgeUs.load();
    if(const auto tracker=halljoy::observation::uap.load()) {out.snapshotGaps=tracker->gaps.load();out.uapHistoryMode=tracker->streamMode.load();}
    for (unsigned a = 0; a < kAddressCount; ++a) {
        const auto g = generated[a].load(std::memory_order_relaxed), d = delivered[a].load(std::memory_order_relaxed);
        out.generated[a] = {static_cast<std::uint8_t>(g & 255), static_cast<std::uint8_t>((g >> 8) & 255)};
        out.sent[a] = {static_cast<std::uint8_t>(d & 255), static_cast<std::uint8_t>((d >> 8) & 255)};
        out.generatedActive += (g & 255) != 0; out.sentActive += (d & 255) != 0;
    }
}
std::uint64_t MidiOutput_Generation() noexcept { return generation.load(std::memory_order_acquire); }
std::uint64_t MidiOutput_StatusToken() noexcept { return static_cast<std::uint64_t>(errorCode.load()) << 32 | static_cast<unsigned>(state.load()); }
std::wstring MidiOutput_Status() {
    switch (state.load()) {
    case MidiOutputState::Connected: return L"Connected to selected MIDI output";
    case MidiOutputState::Monitor: return L"Monitor only - select a MIDI output port";
    case MidiOutputState::MissingPort: return L"Port missing or ambiguous - reconnect it or choose another";
    case MidiOutputState::Error: return L"MIDI output error " + std::to_wstring(errorCode.load()) +
        (faulted.load() ? L" - disable/re-enable or restart HallJoy" : L" - retrying");
    case MidiOutputState::Disabled: return L"MIDI device disabled";
    default: return L"MIDI output paused";
    }
}
#if defined(HALLJOY_MIDI_OUTPUT_TEST)
void MidiOutput_TestApi(MidiTestApi api) { testApi = api; }
MidiTestStats MidiOutput_TestStats() noexcept {
    return {hasOwner.load(), allocated.load(), testStarts.load(), testScans.load(), testTicks.load(), testEnumerations.load()};
}
#endif

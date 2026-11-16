#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include "../HallJoy/midi_output.h"
#include "../HallJoy/output_startup_policy.h"
#include "../HallJoy/physical_analog_state.h"
#include <thread>
#include "../HallJoy/support_log.h"
#include <array>
#include <atomic>
#include <cassert>
#include <iostream>
#include <mutex>
#include <vector>

namespace {
std::atomic<SupportLogDeviceEvidenceProvider> velocityProvider{nullptr};
std::atomic<std::uint64_t> velocitySaveRequests{0};
std::mutex velocityLogMutex;
std::string velocityLog;
}
void SupportLog_Event(const char*, std::uint64_t, SupportLogDetail) noexcept {}
std::uint64_t SupportLog_RequestSnapshot() noexcept {return ++velocitySaveRequests;}
void SupportLog_SetMidiEvidenceProvider(SupportLogDeviceEvidenceProvider provider) noexcept { velocityProvider.store(provider); }
void SupportLog_Evidence(const char* text,bool keep) noexcept {
    assert(!keep);
    std::lock_guard lock(velocityLogMutex);
    velocityLog+=text;velocityLog+='\n';
}
namespace {
std::mutex logMutex;
struct Sent { std::uintptr_t port; std::uint32_t message; };
std::vector<Sent> sentLog;
std::atomic<unsigned> failNext{0}, openCalls{0}, closes{0};
std::atomic<bool> missing{false}, slow{false}, rejectStart{false}, stall{false}, enteredSend{false};
std::atomic<unsigned> lifecycleWakes{0}, readCalls{0};
halljoy::physical_analog::Publication historyBoard;
void VisitHistory(void*,halljoy::observation::Sink sink) { historyBoard.Observations(sink); }
void Wake() noexcept { ++lifecycleWakes; }
bool RejectStart() { return rejectStart.load(); }
unsigned Open(const std::wstring&, std::uintptr_t& port) {
    ++openCalls;
    if (missing.load()) return 2;
    port = openCalls.load(); return 0;
}
unsigned Send(std::uintptr_t port, std::uint32_t message) {
    enteredSend.store(true);
    while (stall.load()) Sleep(1);
    if (slow.load()) Sleep(1);
    if (const auto fail = failNext.exchange(0)) return fail;
    std::lock_guard<std::mutex> lock(logMutex);
    sentLog.push_back({port, message}); return 0;
}
void Close(std::uintptr_t) { ++closes; }
halljoy::midi::Input Read(void* raw, std::uint16_t hid) { ++readCalls; return (*static_cast<std::array<halljoy::midi::Input, halljoy::keycode::kCount>*>(raw))[hid]; }
template<class Check> void Wait(Check check) {
    const auto deadline = GetTickCount64() + 4000;
    while (!check() && GetTickCount64() < deadline) Sleep(1);
    assert(check());
}
MidiTelemetry Snapshot() { MidiTelemetry out; MidiOutput_GetTelemetry(out); return out; }
}
int main() {
    using namespace halljoy::midi;
    MidiOutput_TestApi({Open, Send, Close, RejectStart});
    MidiOutput_SetLifecycleWake(Wake);
    Profile config;
    assert(!config.enabled);
    Apply(config);
    std::array<Input, halljoy::keycode::kCount> idleInput{};
    assert(MidiOutput_Start()); // Disabled start arms only; no owner/session or driver work.
    const auto beforeIdle = MidiOutput_TestStats();
    for (unsigned i=0;i<1000;++i) {
        MidiOutput_Tick(Read,&idleInput); MidiOutput_Panic();
        assert(MidiOutput_Reconcile());
    }
    assert(MidiOutput_Ports().empty());
    const auto afterIdle = MidiOutput_TestStats();
    assert(!velocityProvider.load() && !velocitySaveRequests.load());
    assert(!afterIdle.owner && !afterIdle.allocated && afterIdle.starts==beforeIdle.starts &&
        afterIdle.scans==beforeIdle.scans && afterIdle.ticks==beforeIdle.ticks &&
        afterIdle.enumerations==beforeIdle.enumerations && readCalls==0 && openCalls==0 && lifecycleWakes==0);
    assert(MidiOutput_Stop());
    config.enabled = true; config.depthPressure = false; config.keys[4] = 61;
    Apply(config);
    std::array<Input, halljoy::keycode::kCount> input{}; input[4].available = true;
    auto tick = [&] { MidiOutput_Tick(Read, &input); };
    assert(MidiOutput_Start()); tick();
    Wait([] { return Snapshot().state == MidiOutputState::Monitor; });
    input[4].milli = 500; tick();
    assert(Snapshot().generatedActive == 1 && Snapshot().sentActive == 0 && openCalls == 0);
    assert(velocitySaveRequests.load()>0); // No manual Open log was called.
    // Selecting a port does not replay the already-held monitor note.
    config.port = L"fixture output"; Apply(config); tick();
    Wait([] { return Snapshot().state == MidiOutputState::Connected; }); tick();
    assert(Snapshot().sentActive == 0);
    input[4].milli = 0; tick(); Sleep(1); input[4].milli = 500; tick();
    Wait([] { return Snapshot().sentActive == 1; });
    assert(Snapshot().lastGenerated && Snapshot().lastSent);
    // Panic releases the actual sent note even if the realtime consumer is idle.
    MidiOutput_Panic(); Wait([] { return Snapshot().sentActive == 0; });
    tick(); assert(Snapshot().generatedActive == 0);
    input[4].milli = 0; tick(); Sleep(1); input[4].milli = 800; tick();
    Wait([] { return Snapshot().sentActive == 1; });
    // Switching destination releases the old port and blocks a held-key replay.
    config.port = L"second fixture"; Apply(config); tick();
    Wait([] { return closes.load() > 0 && openCalls.load() >= 2; }); tick();
    assert(Snapshot().sentActive == 0);
    input[4].milli = 0; tick(); Sleep(1); input[4].milli = 600; tick();
    Wait([] { return Snapshot().sentActive == 1; });
    input[4].milli = 0; failNext.store(67); tick();
    Wait([] { return Snapshot().sendErrors != 0; });
    Wait([] { return Snapshot().state == MidiOutputState::Connected; }); tick();
    assert(!Snapshot().sentActive);
    // A slow external driver cannot block the producer. Overflow forces a reset,
    // never overwrites arbitrary note edges and never replays the queued attacks.
    slow.store(true);
    for (unsigned i = 0; i < 14000; ++i) { input[4].milli = i % 2 ? 0 : 700; tick(); }
    slow.store(false);
    assert(Snapshot().overflows > 0);
    MidiOutput_Panic(); Wait([] { return Snapshot().sentActive == 0; });
    assert(MidiOutput_Stop()); assert(Snapshot().state == MidiOutputState::Paused);
    // Stop is repeatable, a missing port is explicit, and recovery needs no UI poll.
    assert(MidiOutput_Stop()); missing.store(true); assert(MidiOutput_Start()); tick();
    Wait([] { return Snapshot().state == MidiOutputState::MissingPort; });
    missing.store(false); Wait([] { return Snapshot().state == MidiOutputState::Connected; });
    assert(MidiOutput_Stop());
    // Re-enable from an armed disabled session needs no realtime tick to open the port.
    config.enabled=false; Apply(config); assert(MidiOutput_Start());
    const auto startsBefore = MidiOutput_TestStats().starts;
    config.enabled=true; Apply(config); assert(lifecycleWakes>0); assert(MidiOutput_Reconcile());
    Wait([] { return Snapshot().state==MidiOutputState::Connected; });
    input[4].milli=500; tick(); assert(!Snapshot().generatedActive); // Held reconnect never replays.
    input[4].milli=0; tick(); Sleep(1); input[4].milli=500; tick();
    Wait([] { return Snapshot().sentActive==1; });
    const auto closesBefore=closes.load(); config.enabled=false; Apply(config);
    Wait([&] { assert(MidiOutput_Reconcile()); return !MidiOutput_TestStats().owner; });
    assert(!MidiOutput_TestStats().allocated && closes>closesBefore && !Snapshot().sentActive && !Snapshot().generatedActive);
    const auto noWork = MidiOutput_TestStats();
    for (unsigned i=0;i<1000;++i) tick();
    assert(MidiOutput_TestStats().ticks==noWork.ticks && MidiOutput_TestStats().scans==noWork.scans);
    assert(MidiOutput_TestStats().starts==startsBefore+1);
    // An outstanding external driver call cannot stall realtime or the supervisor.
    config.enabled=true; Apply(config); assert(MidiOutput_Reconcile());
    Wait([] { return Snapshot().state==MidiOutputState::Connected; });
    input[4].milli=0; tick(); stall=true; enteredSend=false; input[4].milli=800; tick();
    Wait([] { return enteredSend.load(); });
    config.enabled=false; Apply(config);
    auto began=GetTickCount64(); assert(MidiOutput_Reconcile());
    for (unsigned i=0;i<1000;++i) tick();
    assert(GetTickCount64()-began<250 && MidiOutput_TestStats().owner);
    stall=false;
    Wait([] { assert(MidiOutput_Reconcile()); return !MidiOutput_TestStats().owner; });
    assert(MidiOutput_Stop());
    // Exercise the production startup policy with the real MIDI owner and fake outputs.
    unsigned padStarts=0, midiStarts=0;
    auto result=halljoy::outputs::Start(false,[&] { ++padStarts;return false; },[&] { ++midiStarts;return MidiOutput_Start(); });
    assert(!result.admitted && padStarts==1 && midiStarts==0);
    config.enabled=true; Apply(config); rejectStart=true;
    result=halljoy::outputs::Start(true,[] {return true;},[] {return MidiOutput_Start();});
    assert(result.admitted && result.gamepad && !result.midi && !MidiOutput_TestStats().owner);
    assert(!MidiOutput_Reconcile()); // Failed start does not create a retry storm.
    config.enabled=false; Apply(config); assert(MidiOutput_Reconcile());
    rejectStart=false; config.enabled=true; Apply(config);
    assert(MidiOutput_Reconcile()); Wait([] {return Snapshot().state==MidiOutputState::Connected;});
    assert(MidiOutput_Stop());
    result=halljoy::outputs::Start(true,[] {return false;},[] {return MidiOutput_Start();});
    assert(result.admitted && !result.gamepad && result.midi);
    Wait([] {return Snapshot().state==MidiOutputState::Connected;});
    input[4].milli=0; tick(); Sleep(1); input[4].milli=500; tick();
    Wait([] {return Snapshot().sentActive==1;}); // A missing gamepad cannot veto notes.
    assert(MidiOutput_Stop());
    // Race rapid profile toggles/reaping against a real concurrent producer.
    config.enabled=false; Apply(config); assert(MidiOutput_Start());
    std::atomic<bool> producing{true};
    std::thread producer([&] {while(producing.load()) MidiOutput_Tick(Read,&idleInput);});
    for (unsigned i=0;i<20;++i) {
        config.enabled=true; Apply(config); assert(MidiOutput_Reconcile());
        config.enabled=false; Apply(config);
        Wait([] {assert(MidiOutput_Reconcile());return !MidiOutput_TestStats().owner;});
    }
    producing=false; producer.join(); assert(MidiOutput_Stop());
    assert(!MidiOutput_TestStats().owner && !MidiOutput_TestStats().allocated);
    // Production history consumer: no realtime reads occur during complete presses.
    config.enabled=true;Apply(config);historyBoard.Bind(1,4);historyBoard.Publish(1,0,1);
    assert(MidiOutput_Start());
    Wait([] {return Snapshot().state==MidiOutputState::Connected;});
    MidiOutput_Tick(Read,&input,VisitHistory);
    const auto beforeEdges=Snapshot().sentMessages;
    for(unsigned i=0;i<100;++i){historyBoard.Publish(1,700,2+i*2);historyBoard.Publish(1,0,3+i*2);}
    MidiOutput_Tick(Read,&input,VisitHistory);
    Wait([&] {return Snapshot().sentMessages>=beforeEdges+200;});
    assert(!Snapshot().generatedActive && !Snapshot().sentActive && Snapshot().historySamples>=200);
    // The production owner must submit the velocity measured from captured
    // timestamps even when the complete stroke precedes its consumer wake.
    halljoy::observation::Event route{};
    historyBoard.Observations({&route, +[](void* raw, const halljoy::observation::Event& event) {
        if (event.hid == 4) *static_cast<halljoy::observation::Event*>(raw) = event;
    }});
    assert(route.source && route.connection);
    const auto start = halljoy::observation::NowUs() + 1000;
    const unsigned depths[] = {0, 50, 100, 200, 0};
    const unsigned times[] = {0, 5000, 10000, 20000, 30000};
    const auto beforeTouch = Snapshot().sentMessages;
    auto journal = halljoy::observation::current.load();
    for (unsigned i = 0; i < 5; ++i) {
        auto event = route;
        event.kind = halljoy::observation::Kind::Sample;
        event.timeUs = start + times[i];
        event.milli = static_cast<std::uint16_t>(depths[i]);
        assert(journal->Push(event));
    }
    journal.reset();
    MidiOutput_Tick(Read, &input, VisitHistory);
    Wait([&] { return Snapshot().sentMessages >= beforeTouch + 2; });
    const auto measuredTouch = Snapshot();
    assert(measuredTouch.touch.speedMilli == 10000);
    assert(measuredTouch.touch.quality == TouchQuality::CapturedMotion);
    assert((measuredTouch.lastSent >> 12) == VelocityForSpeed(10000, config));
    const auto exportVelocity=velocityProvider.load();
    assert(exportVelocity);exportVelocity();
    {
        std::lock_guard lock(velocityLogMutex);
        assert(velocityLog.find("scope=midi_bound_input")!=std::string::npos &&
            velocityLog.find("speed=10000 travel=150 interval_us=15000 quality=1")!=std::string::npos &&
            velocityLog.find("stage=queued note=60 channel=1 status=144")!=std::string::npos &&
            velocityLog.find("stage=driver note=60 channel=1 status=144")!=std::string::npos &&
            velocityLog.find("evidence.midi.motion")!=std::string::npos);
    }
    // Lost acquisition history resets safely rather than playing a stale backlog.
    for(unsigned i=0;i<40000;++i)historyBoard.Publish(1,i%2?0:700,500+i);
    historyBoard.Publish(1,800,41000);
    MidiOutput_Tick(Read,&input,VisitHistory);
    assert(Snapshot().historyLosses>0);
    MidiOutput_Tick(Read,&input,VisitHistory);
    assert(!Snapshot().generatedActive);
    historyBoard.Publish(1,0,41001);MidiOutput_Tick(Read,&input,VisitHistory);
    historyBoard.Publish(1,700,41002);MidiOutput_Tick(Read,&input,VisitHistory);
    Wait([] {return Snapshot().sentActive==1;});
    auto publicationLease=halljoy::observation::current.load();
    config.enabled=false;Apply(config);
    Wait([] {assert(MidiOutput_Reconcile());return Snapshot().sentActive==0;});
    assert(MidiOutput_TestStats().owner && MidiOutput_TestStats().allocated);
    publicationLease.reset();
    Wait([] {assert(MidiOutput_Reconcile());return !MidiOutput_TestStats().owner;});
    assert(MidiOutput_Stop());
    assert(!halljoy::observation::enabled && !halljoy::observation::current.load());
    // A completed session remains exportable after disabling MIDI.
    assert(velocityProvider.load());velocityProvider.load()();
    std::size_t exportedBytes=0;
    {std::lock_guard lock(velocityLogMutex);exportedBytes=velocityLog.size();}
    velocityProvider.load()();
    {std::lock_guard lock(velocityLogMutex);assert(velocityLog.size()==exportedBytes);}
    bool attackSeen = false, releaseSeen = false;
    { std::lock_guard<std::mutex> lock(logMutex);
      for (const auto& sent : sentLog) { attackSeen |= (sent.message & 0xF0) == 0x90; releaseSeen |= (sent.message & 0xF0) == 0x80; } }
    assert(attackSeen && releaseSeen);
    std::cout << "MIDI_OUTPUT_WINDOWS_TEST=PASS monitor sent panic port_switch error recovery queue_overflow stop_restart fake_driver_only opt_in_zero_work disable_cleanup stalled_driver output_independence toggle_race observed_edges history_loss_rearm\n";
}

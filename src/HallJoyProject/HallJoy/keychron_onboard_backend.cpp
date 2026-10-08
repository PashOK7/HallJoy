#include "keychron_onboard_identity.h"
#include "keychron_onboard_gamepad.h"
#include <stdexcept>
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <process.h>
#include "keychron_onboard_backend.h"
#include "perf_trace.h"
#include "input_shortcuts_runtime.h"
#include "keychron_onboard_channel.h"
#include "keychron_onboard_host_profile.h"
#include "native_analog_routing.h"
#include "physical_analog_state.h"
#include "realtime_loop.h"
#include "settings.h"
#include "profile_runtime_gate.h"
#include "support_log.h"
#include "worker_exception_barrier.h"
#include <atomic>
#include <mutex>
using namespace halljoy::k4_onboard;
namespace {
std::mutex service;
HANDLE thread=nullptr,cancel=nullptr;
Device chosen;
std::mutex padMutex;
std::array<uint8_t,20> padReport{};
std::atomic<bool> reserved{false},present{false},connected{false},admitted{false},stopping{false};
// Pause parks the session (native interface kept, no re-enumeration); process
// exit must fully close so no idle controller outlives HallJoy.
std::atomic<bool> parkOnStop{true},parkedSession{false};
// Survives worker restarts: after resume only the changed profile bytes go out.
CommittedProfile committedProfile;
std::atomic<uint64_t> monitorUntil{0},updates{0},failures{0},lastFrame{0};
std::atomic<unsigned> state{0},telemetryLevels{241};
halljoy::physical_analog::Publication values;
void ClearPad() {
    {std::lock_guard<std::mutex> lock(padMutex);padReport={};}
    updates.fetch_add(1,std::memory_order_release);RealtimeLoop_NotifyInputChanged();
}
void Clear() {values.Clear();connected.store(false);lastFrame.store(0);ClearPad();}

void Bind() {
    values.Clear();
    for(unsigned i=0;i<HJO_SLOTS;++i) if(kSlotHid[i]) values.Bind(static_cast<uint8_t>(i+1),kSlotHid[i]);
}
// Why the onboard session could not claim the keyboard (support log only).
enum class PrepareStage : unsigned { Claimed=0, NoDevice=1, Ambiguous=2, Open=3, Status=4, Capability=5, Routing=6 };
PrepareStage TryPrepareOnce(Device& claimed) {
    const auto devices=EnumerateDevices();
    if(devices.empty()) return PrepareStage::NoDevice;
    if(devices.size()!=1) return PrepareStage::Ambiguous;
    WindowsChannel channel(devices[0]);
    if(!channel.Connect()) return PrepareStage::Open;
    Client client(channel);Packet reply{};
    // The firmware drops a request while it is busy; one silent drop must not
    // push the keyboard onto the fallback route for the whole session.
    bool status=false;
    for(unsigned attempt=0;attempt<3 && !status;++attempt) status=client.Status(reply);
    if(!status) return PrepareStage::Status;
    if(!(reply[17]&2)) return PrepareStage::Capability;
    if(!NativeAnalogRouting_Claim(0x3434,0x0e40,devices[0].path.c_str(),NativeAnalogProtocol::KeychronOnboard))
        return PrepareStage::Routing;
    claimed=devices[0];
    return PrepareStage::Claimed;
}
bool Prepare() {
    std::lock_guard<std::mutex> lock(service);
    if(thread) return reserved.load();
    reserved.store(false);present.store(false);chosen={};
    // Routing is decided once per engine generation, before UAP starts. After
    // a previous session the K4 re-enumerates (native mode off) and its vendor
    // interface can be briefly missing or still held. While the K4 is plugged
    // in, wait a bounded time instead of falling back to UAP for the session.
    // A K4 running HJO1 that was removed moments ago is re-enumerating (its
    // previous session ended) and is waited for even while no node is present.
    const bool returning=!K4UsbDevicePresent() && K4HjoRecentlyRemoved(5000);
    const auto deadline=GetTickCount64()+(returning?4000:3000);
    PrepareStage stage=PrepareStage::NoDevice;
    unsigned attempts=0;
    Device claimed{};
    for(;;) {
        ++attempts;
        stage=TryPrepareOnce(claimed);
        if(stage==PrepareStage::Claimed || stage==PrepareStage::Capability || stage==PrepareStage::Routing) break;
        if(GetTickCount64()>=deadline || (!returning && !K4UsbDevicePresent())) break;
        Sleep(100);
    }
    if(stage!=PrepareStage::Claimed) {
        if(K4UsbDevicePresent() || stage!=PrepareStage::NoDevice)
            SupportLog_Event("k4.onboard_prepare_failed",static_cast<unsigned>(stage),SupportLog_Data(attempts));
        // HJO1 firmware is served by the onboard protocol only, never by the
        // UAP fallback: reserve its vendor collection so the analog host skips
        // it. The application retries the onboard route when it can
        // (App: K4 late takeover).
        unsigned held=0;
        for(const auto& path:K4HjoVendorInterfaces())
            if(NativeAnalogRouting_Claim(0x3434,0x0e40,path.c_str(),NativeAnalogProtocol::KeychronOnboard)) ++held;
        if(held) SupportLog_Event("k4.onboard_reserved_for_retry",held);
        return false;
    }
    if(returning) SupportLog_Event("k4.onboard_prepare_waited_return",attempts);
    if(attempts>1) SupportLog_Event("k4.onboard_prepare_retried",attempts);
    chosen=claimed;reserved.store(true);present.store(true);return true;
}
// Owned by the protocol worker: its join is covered by the registry's bounded
// outer worker join. Windows gamepad reads never share the vendor HID channel.
class PadMonitor {
    HANDLE done_=nullptr,thread_=nullptr;
    static unsigned __stdcall Entry(void* context) noexcept {
        auto* self=static_cast<PadMonitor*>(context);
        return halljoy::worker::RunWorkerEntryBarrier([&]() -> unsigned {
            while(WaitForSingleObject(self->done_,0)!=WAIT_OBJECT_0) {
                const bool visible=GetTickCount64()<monitorUntil.load();
                std::array<uint8_t,20> report{};
                if(visible && admitted.load() && connected.load() && state.load()==4)
                    KeychronOnboard_ReadOsGamepad(report);
                if(!admitted.load() || !connected.load() || state.load()!=4) report={};
                {std::lock_guard<std::mutex> lock(padMutex);padReport=report;}
                // Match the real minimum of the window SetTimer; no 1 kHz OS polling.
                WaitForSingleObject(self->done_,visible?std::clamp(Settings_GetUIRefreshMs(),static_cast<UINT>(USER_TIMER_MINIMUM),200u):50u);
            }
            return 0;
        },[](const halljoy::worker::WorkerExceptionRecord&) noexcept {
            std::lock_guard<std::mutex> lock(padMutex);padReport={};
        },[](const halljoy::worker::WorkerExceptionRecord&) noexcept {},1);
    }
public:
    PadMonitor() {
        done_=CreateEventW(nullptr,TRUE,FALSE,nullptr);
        if(!done_)throw std::runtime_error("gamepad monitor event");
        thread_=reinterpret_cast<HANDLE>(_beginthreadex(nullptr,0,Entry,this,0,nullptr));
        if(!thread_){CloseHandle(done_);throw std::runtime_error("gamepad monitor thread");}
    }
    ~PadMonitor(){SetEvent(done_);WaitForSingleObject(thread_,INFINITE);CloseHandle(thread_);CloseHandle(done_);}
    PadMonitor(const PadMonitor&)=delete;
    PadMonitor& operator=(const PadMonitor&)=delete;
};
// Forwards to the real channel; with --halljoy-perf-log it also records each
// control exchange (request, ok, phase, native) and reconnect wait.
struct PerfTracedChannel final : Channel {
    WindowsChannel& inner;
    explicit PerfTracedChannel(WindowsChannel& channel):inner(channel){}
    bool Exchange(const Packet& request,Packet& reply) override {
        const auto start=halljoy::perf::Enabled()?halljoy::perf::Now():0;
        const bool ok=inner.Exchange(request,reply);
        if(start && request[1]!=0x73 && request[1]!=0x74 && request[1]!=0x76 && request[1]!=0x7A)
            halljoy::perf::Span("k4.xchg",(std::uint64_t(request[1])<<24)|(std::uint64_t(ok)<<16)|
                (std::uint64_t(reply[3])<<8)|reply[16],start);
        return ok;
    }
    bool Read(Packet& reply) override {return inner.Read(reply);}
    bool Reconnect(std::uint16_t revision) override {
        halljoy::perf::Scope scope("k4.reconnect",revision);
        return inner.Reconnect(revision);
    }
    std::uint64_t NowMs() const override {return inner.NowMs();}
    bool Cancelled() const override {return inner.Cancelled();}
};
unsigned Body() {
    PadMonitor padMonitor;

    while(!stopping.load()) {
        WindowsChannel channel(chosen,cancel);
        halljoy::perf::Mark("k4.session.connect");
        if(!channel.Connect()) {
            Clear();state.store(1);
            if(!channel.Reconnect(0x1212)) {WaitForSingleObject(cancel,500);continue;}
        }
        PerfTracedChannel traced(channel);
        Client client(traced,&committedProfile);Packet status{};
        const auto parked=[&]{return status[3]==HJO_PARKED && status[16];};
        if(!client.Status(status) || ((status[3] || status[16]) && !parked())) {
            // Never hijack an existing session. Its owner/watchdog must release it.
            Clear();state.store(1);WaitForSingleObject(cancel,500);continue;
        }
        telemetryLevels.store(client.PreciseTelemetry()?65536:241);
        present.store(true);connected.store(true);Bind();state.store(2);
        bool healthy=true;
        auto nextProfile=uint64_t{0};
        auto appliedRevision=uint64_t{0};
        while(!stopping.load() && healthy) {
            const auto now=GetTickCount64();
            const bool enabled=admitted.load() && Settings_GetVirtualGamepadsEnabled();
            if(!enabled && client.Active()) {
                state.store(2);ClearPad();
                if(client.SupportsPark()) {healthy=client.Park();parkedSession.store(true);}
                else healthy=client.Close();
            }
            const auto revision=halljoy::profile_runtime::revision.load(std::memory_order_acquire);
            if(enabled && (now>=nextProfile || revision!=appliedRevision)) {
                hjo_profile profile{};
                const auto captured=CaptureProfile(profile);
                if(captured!=ProfileResult::Busy) {nextProfile=now+100;appliedRevision=revision;}
                if(captured==ProfileResult::Ready) {
                    // Older firmware rejects unknown mapping flags; only send the
                    // Alt/Tab passthrough to firmware that advertises it.
                    if(!client.SupportsKeepAltTab()) profile.mapping.flags=static_cast<uint8_t>(profile.mapping.flags & ~HJO_KEEP_ALT_TAB);
                    state.store(client.Active()?4:3);
                    healthy=client.Active()?client.Update(profile):client.Open(profile);
                    if(healthy) parkedSession.store(false);
                    if(healthy) state.store(4);
                } else if(captured!=ProfileResult::Busy) {
                    if(client.Active()) {healthy=client.Close();ClearPad();}
                    state.store(10+static_cast<unsigned>(captured));
                }
            }
            if(!healthy) break;
            healthy=client.KeepAlive();
            const bool shortcutsNeedAnalog=halljoy::shortcuts::NeedsAnalog();
            if(healthy && (now<monitorUntil.load() || shortcutsNeedAnalog)) {
                std::array<uint16_t,HJO_SLOTS> depth{};
                healthy=client.Depth(depth);
                if(healthy) {
                    const auto stamp=GetTickCount64();
                    for(unsigned i=0;i<HJO_SLOTS;++i) if(kSlotHid[i])
                        values.Publish(static_cast<uint8_t>(i+1),static_cast<uint16_t>((uint32_t(depth[i])*1000+32767)/65535),stamp);
                    lastFrame.store(stamp);updates.fetch_add(1);RealtimeLoop_NotifyInputChanged();
                }
            }
            // Hidden UI needs no matrix reads unless a configured shortcut
            // still needs analog input. Pause/exit stop this worker entirely.
            if(healthy && GetTickCount64()>=monitorUntil.load() && !shortcutsNeedAnalog) WaitForSingleObject(cancel,20);
        }
        halljoy::perf::Mark("k4.loop_exit",healthy?1:0);
        Clear();
        if(!healthy) {failures.fetch_add(1);state.store(5);}
        // Cancel only the in-flight operation, then permit bounded orderly STOP.
        // stopping is a separate gate and prevents another activation.
        if(stopping.load()) ResetEvent(cancel);
        // The cancelled in-flight operation is not a device failure: Park
        // itself falls back to the orderly STOP when the firmware refuses.
        if(stopping.load() && parkOnStop.load() && client.SupportsPark()) {
            const bool hadSession=client.Token()!=0;
            halljoy::perf::Scope parkScope("k4.stop.park",hadSession?1:0);
            // Closing admission makes the loop park first; the stop can cancel
            // that park's retry after the firmware already parked (lost reply),
            // and the cancelled fallback closes the handle. Ask the firmware
            // before parking again: STOP + re-enumeration wait would only time
            // out (3.5 s) against a session that is already parked.
            // The cancelled request's reply may still arrive in the reopened
            // handle: each Status attempt consumes one reply, so a stale one is
            // skipped by the next attempt.
            Packet current{};
            bool known=client.Status(current);
            if(!known && channel.Connect())
                for(unsigned attempt=0;attempt<3 && !known;++attempt) known=client.Status(current);
            if(known && current[3]==HJO_PARKED && current[16]) parkedSession.store(true);
            else if(client.Park() && hadSession) parkedSession.store(true);
        }
        else {
            halljoy::perf::Scope closeScope("k4.stop.close",client.Token()!=0?1:0);
            (void)client.Close();
        }
        if(!stopping.load()) WaitForSingleObject(cancel,500);
    }
    state.store(0);return 0;
}
unsigned __stdcall Worker(void*) noexcept {
    return halljoy::worker::RunWorkerEntryBarrier([]{return Body();},
        [](const halljoy::worker::WorkerExceptionRecord&) noexcept {failures.fetch_add(1);state.store(6);},
        [](const halljoy::worker::WorkerExceptionRecord&) noexcept {Clear();},1);
}
bool Start() {
    std::lock_guard<std::mutex> lock(service);
    if(thread) return true;
    if(!reserved.load()) return true;
    stopping.store(false);cancel=CreateEventW(nullptr,TRUE,FALSE,nullptr);
    if(!cancel) return false;
    thread=reinterpret_cast<HANDLE>(_beginthreadex(nullptr,0,Worker,nullptr,0,nullptr));
    if(!thread){CloseHandle(cancel);cancel=nullptr;return false;}
    return true;
}
halljoy::lifecycle::StopResult Stop(halljoy::lifecycle::GenerationId generation) {
    std::lock_guard<std::mutex> lock(service);
    halljoy::perf::Mark("k4.stop.requested");
    stopping.store(true);admitted.store(false);
    if(cancel) SetEvent(cancel);
    if(thread) {
        const auto wait=WaitForSingleObject(thread,5000);
        if(wait!=WAIT_OBJECT_0) return NativeAnalogBackendStopFailed(generation,
            wait==WAIT_TIMEOUT?halljoy::lifecycle::LifecycleErrorCode::StopTimedOut:halljoy::lifecycle::LifecycleErrorCode::PrimitiveFailed,
            wait==WAIT_TIMEOUT?WAIT_TIMEOUT:GetLastError());
        CloseHandle(thread);thread=nullptr;
    }
    if(cancel){CloseHandle(cancel);cancel=nullptr;}
    Clear();present.store(false);reserved.store(false);
    return NativeAnalogBackendStopJoined(generation);
}
void Notify() {} // worker owns reconnect; generic USB notifications cannot restart its session
bool Present(){return present.load();}
bool Connected(){return connected.load();}
bool Owns(uint16_t hid){return reserved.load() && SlotForHid(hid)>=0 && hid!=0;}
uint16_t Get(uint16_t hid){return values.Read(hid,GetTickCount64(),250).milli;}
void Telemetry(NativeAnalogBackendTelemetry* out) {
    if(!out)return;*out={};out->present=Present();out->connected=Connected();
    out->vendorId=0x3434;out->productId=0x0e40;out->usagePage=0xff60;out->usage=0x61;
    if(out->connected) out->verifiedLayoutToken=kLayoutToken;
    out->mappedKeys=100;out->nominalRawLevels=telemetryLevels.load();out->inputReportBytes=33;out->outputReportBytes=33;
    out->successfulUpdates=updates.load();out->failedUpdates=failures.load();
    out->activeKeys=values.Active(GetTickCount64());
    const auto last=lastFrame.load();out->lastUpdateAgeMs=last?static_cast<uint32_t>(GetTickCount64()-last):0;
    const auto s=state.load();
    const wchar_t* text=s==6?L"K4 HE onboard: worker failed":s==4?L"K4 HE onboard: active":s==3?L"K4 HE onboard: loading profile":
        s>=10?L"K4 HE onboard: profile requires one pad and K4 keys, no mouse":
        s==2?L"K4 HE onboard: controller off":L"K4 HE onboard: reconnecting";
    wcsncpy_s(out->status,text,_TRUNCATE);
}
}
bool KeychronOnboard_CopyPad(std::uint8_t* destination,std::size_t size) noexcept {
    if(!destination || size!=20) return false;
    std::lock_guard<std::mutex> lock(padMutex);std::memcpy(destination,padReport.data(),20);return true;
}
std::uint64_t KeychronOnboard_MonitorGeneration() noexcept {return updates.load();}
bool KeychronOnboard_OwnsOutput() noexcept {return reserved.load();}
bool KeychronOnboard_NeedsTakeover() noexcept {
    try { return !reserved.load() && !K4HjoVendorInterfaces().empty(); } catch (...) { return false; }
}
void KeychronOnboard_SetAdmission(bool on) noexcept {admitted.store(on);}
void KeychronOnboard_SetParkOnStop(bool park) noexcept {parkOnStop.store(park);}
void KeychronOnboard_ReleaseParked() noexcept {
    if(!parkedSession.exchange(false)) return;
    try {
        const auto devices=EnumerateDevices();
        if(devices.size()!=1) return;
        WindowsChannel channel(devices[0]);
        if(!channel.Connect()) return;
        Client client(channel);
        if(!client.ReleaseParked()) SupportLog_Event("k4.onboard_park_release_failed",1);
    } catch(...) {}
}
void KeychronOnboard_MonitorVisible(bool visible) noexcept {if(visible) monitorUntil.store(GetTickCount64()+250);}
const NativeAnalogBackendDescriptor& KeychronOnboard_GetNativeBackendDescriptor() {
    static const NativeAnalogBackendDescriptor d{kNativeAnalogBackendAbiVersion,sizeof(NativeAnalogBackendDescriptor),
        "keychron-k4-onboard-hjo1",L"Keychron K4 HE onboard",NativeAnalogProtocol::KeychronOnboard,
        NativeAnalogStartPhase::BeforeUap,NativeAnalogBackendFlag_PolledTransport|NativeAnalogBackendFlag_ReadOnlyProbe,
        Prepare,Start,Stop,Notify,Present,Connected,Owns,Get,Telemetry};
    return d;
}

bool KeychronOnboard_WorkerHealthy() noexcept {return state.load()!=6;}

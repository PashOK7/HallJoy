#include "keychron_onboard_identity.h"
#include "keychron_onboard_gamepad.h"
#include <stdexcept>
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <process.h>
#include "keychron_onboard_backend.h"
#include "keychron_onboard_channel.h"
#include "keychron_onboard_host_profile.h"
#include "native_analog_routing.h"
#include "physical_analog_state.h"
#include "realtime_loop.h"
#include "settings.h"
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
std::atomic<uint64_t> monitorUntil{0},updates{0},failures{0},lastFrame{0};
std::atomic<unsigned> state{0};
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
bool Prepare() {
    std::lock_guard<std::mutex> lock(service);
    if(thread) return reserved.load();
    reserved.store(false);present.store(false);chosen={};
    const auto devices=EnumerateDevices();
    if(devices.size()!=1) return false;
    WindowsChannel channel(devices[0]);
    if(!channel.Connect()) return false;
    Client client(channel);Packet reply{};
    if(!client.Status(reply) || !(reply[17]&2)) return false;
    if(!NativeAnalogRouting_Claim(0x3434,0x0e40,devices[0].path.c_str(),NativeAnalogProtocol::KeychronOnboard)) return false;
    chosen=devices[0];reserved.store(true);present.store(true);return true;
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
unsigned Body() {
    PadMonitor padMonitor;

    while(!stopping.load()) {
        WindowsChannel channel(chosen,cancel);
        if(!channel.Connect()) {
            Clear();state.store(1);
            if(!channel.Reconnect(0x1212)) {WaitForSingleObject(cancel,500);continue;}
        }
        Client client(channel);Packet status{};
        if(!client.Status(status) || status[3] || status[16]) {
            // Never hijack an existing session. Its owner/watchdog must release it.
            Clear();state.store(1);WaitForSingleObject(cancel,500);continue;
        }
        present.store(true);connected.store(true);Bind();state.store(2);
        bool healthy=true;
        auto nextProfile=uint64_t{0};
        while(!stopping.load() && healthy) {
            const auto now=GetTickCount64();
            const bool enabled=admitted.load() && Settings_GetVirtualGamepadsEnabled();
            if(!enabled && client.Active()) {
                state.store(2);ClearPad();healthy=client.Close();
            }
            if(enabled && now>=nextProfile) {
                hjo_profile profile{};
                const auto captured=CaptureProfile(profile);
                nextProfile=now+100;
                if(captured==ProfileResult::Ready) {
                    state.store(client.Active()?4:3);
                    healthy=client.Active()?client.Update(profile):client.Open(profile);
                    if(healthy) state.store(4);
                } else if(captured!=ProfileResult::Busy) {
                    if(client.Active()) {healthy=client.Close();ClearPad();}
                    state.store(10+static_cast<unsigned>(captured));
                }
            }
            if(!healthy) break;
            healthy=client.KeepAlive();
            if(healthy && now<monitorUntil.load()) {
                std::array<uint16_t,HJO_SLOTS> depth{};
                healthy=client.Depth(depth);
                if(healthy) {
                    const auto stamp=GetTickCount64();
                    for(unsigned i=0;i<HJO_SLOTS;++i) if(kSlotHid[i])
                        values.Publish(static_cast<uint8_t>(i+1),static_cast<uint16_t>((uint32_t(depth[i])*1000+32767)/65535),stamp);
                    lastFrame.store(stamp);updates.fetch_add(1);RealtimeLoop_NotifyInputChanged();
                }
            }
            // UI reads may run as quickly as transport permits. When hidden,
            // only settings/lease work remains; no full matrix polling.
            if(healthy && GetTickCount64()>=monitorUntil.load()) WaitForSingleObject(cancel,20);
        }
        Clear();
        if(!healthy) {failures.fetch_add(1);state.store(5);}
        // Cancel only the in-flight operation, then permit bounded orderly STOP.
        // stopping is a separate gate and prevents another activation.
        if(stopping.load()) ResetEvent(cancel);
        (void)client.Close();
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
    out->mappedKeys=100;out->nominalRawLevels=241;out->inputReportBytes=33;out->outputReportBytes=33;
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
void KeychronOnboard_SetAdmission(bool on) noexcept {admitted.store(on);}
void KeychronOnboard_MonitorVisible(bool visible) noexcept {if(visible) monitorUntil.store(GetTickCount64()+250);}
const NativeAnalogBackendDescriptor& KeychronOnboard_GetNativeBackendDescriptor() {
    static const NativeAnalogBackendDescriptor d{kNativeAnalogBackendAbiVersion,sizeof(NativeAnalogBackendDescriptor),
        "keychron-k4-onboard-hjo1",L"Keychron K4 HE onboard",NativeAnalogProtocol::KeychronOnboard,
        NativeAnalogStartPhase::BeforeUap,NativeAnalogBackendFlag_PolledTransport|NativeAnalogBackendFlag_ReadOnlyProbe,
        Prepare,Start,Stop,Notify,Present,Connected,Owns,Get,Telemetry};
    return d;
}

bool KeychronOnboard_WorkerHealthy() noexcept {return state.load()!=6;}

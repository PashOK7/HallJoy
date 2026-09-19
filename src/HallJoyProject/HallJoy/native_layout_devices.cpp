#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <setupapi.h>
#include <hidsdi.h>
#include <devpropdef.h>
#include "native_layout_devices.h"
#include "keyboard_support_status.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <cstring>
#include <cwchar>
#include <mutex>
#include <vector>
namespace {
std::atomic<std::uint64_t> topology{0};
// Windows SDK DEVPKEY_Device_ContainerId; never serialized or logged.
constexpr DEVPROPKEY containerKey{{0x8c7ed206,0x3f8a,0x4827,{0xb3,0xab,0xae,0x9e,0x1f,0xae,0xfc,0x6c}},2};
int Enumerate(const std::vector<std::uint32_t>& identities) {
    if (identities.empty()) return 0;
    GUID hid{};HidD_GetHidGuid(&hid);
    const auto handle=SetupDiGetClassDevsW(&hid,nullptr,nullptr,DIGCF_PRESENT|DIGCF_DEVICEINTERFACE);
    if(handle==INVALID_HANDLE_VALUE) return -2;
    struct Guard { HDEVINFO handle; ~Guard(){SetupDiDestroyDeviceInfoList(handle);} } guard{handle};
    halljoy::layout_devices::Containers containers;
    bool complete=false;
    for(DWORD i=0;i<512;++i) {
        SP_DEVINFO_DATA device{};device.cbSize=sizeof(device);
        if(!SetupDiEnumDeviceInfo(handle,i,&device)) {
            complete=GetLastError()==ERROR_NO_MORE_ITEMS;break;
        }
        wchar_t ids[2048]{};DWORD type=0;
        if(!SetupDiGetDeviceRegistryPropertyW(handle,&device,SPDRP_HARDWAREID,&type,
            reinterpret_cast<BYTE*>(ids),sizeof(ids)-sizeof(wchar_t),nullptr)) {containers.Missing();continue;}
        _wcsupr_s(ids);
        const auto vid=wcsstr(ids,L"VID_"),pid=wcsstr(ids,L"PID_");
        if(!vid || !pid) continue;
        const auto identity=std::uint32_t((wcstoul(vid+4,nullptr,16)&0xffff)<<16)|(wcstoul(pid+4,nullptr,16)&0xffff);
        if(std::find(identities.begin(),identities.end(),identity)==identities.end()) continue;
        GUID container{};DEVPROPTYPE propertyType=0;
        if(!SetupDiGetDevicePropertyW(handle,&device,&containerKey,&propertyType,
            reinterpret_cast<BYTE*>(&container),sizeof(container),nullptr,0) || propertyType!=DEVPROP_TYPE_GUID) {
            containers.Missing();continue;
        }
        std::array<unsigned char,16> bytes{};std::memcpy(bytes.data(),&container,16);
        containers.Add(bytes);
        // Two matching containers already prove ambiguity, even if others fail.
        if(containers.Result(false)>1) return 2;
    }
    return containers.Result(complete);
}
struct State {
    std::mutex mutex;
    std::vector<std::uint32_t> identities;
    std::uint64_t epoch=0,generation=0,nextScan=0;
    bool running=false;
    int result=-1;
    PTP_WORK work=CreateThreadpoolWork(&Run,this,nullptr);
    ~State(){if(work){WaitForThreadpoolWorkCallbacks(work,FALSE);CloseThreadpoolWork(work);}}
    static void CALLBACK Run(PTP_CALLBACK_INSTANCE,void* context,PTP_WORK) noexcept {
        auto& state=*static_cast<State*>(context);
        for(;;) {
            std::vector<std::uint32_t> ids;std::uint64_t version=0;
            int value=-2;
            try {
                {std::lock_guard<std::mutex> lock(state.mutex);version=state.generation;ids=state.identities;}
                value=Enumerate(ids);
            } catch(...) { value=-2; }
            std::lock_guard<std::mutex> lock(state.mutex);
            if(version!=state.generation) continue;
            state.result=value;state.running=false;return;
        }
    }
};
}
void NativeLayoutDevices_Invalidate() noexcept {topology.fetch_add(1,std::memory_order_release);}
int NativeLayoutDevices_Query(const std::uint32_t* identities,std::size_t count) {
    static State state;
    if(count>16 || (count && !identities)) return -2;
    std::vector<std::uint32_t> ids;
    if(count) ids.assign(identities,identities+count);
    std::sort(ids.begin(),ids.end());ids.erase(std::unique(ids.begin(),ids.end()),ids.end());
    std::lock_guard<std::mutex> lock(state.mutex);
    const auto epoch=topology.load(std::memory_order_acquire),now=GetTickCount64();
    if(ids!=state.identities || epoch!=state.epoch) {
        state.identities=std::move(ids);state.epoch=epoch;++state.generation;state.result=-1;state.nextScan=0;
    }
    if(state.identities.empty()) return 0;
    if(!state.work) return -2;
    if(!state.running && now>=state.nextScan) {
        state.running=true;state.nextScan=now+1000;SubmitThreadpoolWork(state.work);
    }
    return state.result;
}

namespace {
unsigned EnumerateFrozen() {
    // Read Windows USB metadata only; no device handles or protocol requests.
    constexpr DEVPROPKEY busDescription{{0x540b947e,0x8b40,0x45bc,{0xa8,0xa2,0x6a,0x0b,0x89,0x4c,0xbd,0xa2}},4};
    const auto handle=SetupDiGetClassDevsW(nullptr,L"USB",nullptr,DIGCF_PRESENT|DIGCF_ALLCLASSES);
    if(handle==INVALID_HANDLE_VALUE) return 0;
    struct Guard {HDEVINFO h;~Guard(){SetupDiDestroyDeviceInfoList(h);}} guard{handle};
    unsigned mask=0;
    for(DWORD i=0;i<512;++i) {
        SP_DEVINFO_DATA device{};device.cbSize=sizeof(device);
        if(!SetupDiEnumDeviceInfo(handle,i,&device)) break;
        wchar_t ids[2048]{};DWORD type=0;
        if(!SetupDiGetDeviceRegistryPropertyW(handle,&device,SPDRP_HARDWAREID,&type,
            reinterpret_cast<BYTE*>(ids),sizeof(ids)-sizeof(wchar_t),nullptr)) continue;
        _wcsupr_s(ids);
        const auto vid=wcsstr(ids,L"VID_"),pid=wcsstr(ids,L"PID_");
        if(!vid || !pid) continue;
        wchar_t name[256]{};DEVPROPTYPE propertyType=0;
        SetupDiGetDevicePropertyW(handle,&device,&busDescription,&propertyType,
            reinterpret_cast<BYTE*>(name),sizeof(name)-sizeof(wchar_t),nullptr,0);
        if(propertyType!=DEVPROP_TYPE_STRING) name[0]=0;
        _wcsupr_s(name);
        mask |= halljoy::keyboard_support::ClassifyFrozen(
            wcstoul(vid+4,nullptr,16)&0xffff,wcstoul(pid+4,nullptr,16)&0xffff,name);
    }
    return mask;
}
struct FrozenInventory {
    std::mutex mutex;
    std::uint64_t epoch=UINT64_MAX;
    unsigned mask=0;
    bool running=false;
    PTP_WORK work=CreateThreadpoolWork(&Run,this,nullptr);
    ~FrozenInventory(){if(work){WaitForThreadpoolWorkCallbacks(work,FALSE);CloseThreadpoolWork(work);}}
    static void CALLBACK Run(PTP_CALLBACK_INSTANCE,void* context,PTP_WORK) noexcept {
        auto& state=*static_cast<FrozenInventory*>(context);
        for(;;) {
            const auto version=topology.load(std::memory_order_acquire);
            const auto result=EnumerateFrozen();
            std::lock_guard<std::mutex> lock(state.mutex);
            if(version!=topology.load(std::memory_order_acquire)) continue;
            state.mask=result;state.epoch=version;state.running=false;return;
        }
    }
};
}
unsigned NativeLayoutDevices_QueryFrozen() {
    static FrozenInventory state;
    std::lock_guard<std::mutex> lock(state.mutex);
    if(state.epoch!=topology.load(std::memory_order_acquire) && !state.running && state.work) {
        state.running=true;SubmitThreadpoolWork(state.work);
    }
    return state.mask;
}

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <setupapi.h>
#include <hidsdi.h>
#include <hidpi.h>
#include <initguid.h>
#include <devpkey.h>
#include "keychron_onboard_channel.h"
#include "hid_io_operation.h"
#include <array>
#include <cwchar>
#include <cwctype>
#include <utility>
namespace halljoy::k4_onboard {
namespace {
struct Handle {
    HANDLE value=INVALID_HANDLE_VALUE;
    ~Handle() { if (value!=INVALID_HANDLE_VALUE && value) CloseHandle(value); }
};
}
std::vector<Device> EnumerateDevices() {
    GUID guid{}; HidD_GetHidGuid(&guid);
    const auto set=SetupDiGetClassDevsW(&guid,nullptr,nullptr,DIGCF_PRESENT|DIGCF_DEVICEINTERFACE);
    if (set==INVALID_HANDLE_VALUE) return {};
    struct Guard { HDEVINFO set; ~Guard(){SetupDiDestroyDeviceInfoList(set);} } guard{set};
    std::vector<Device> result;
    for (DWORD i=0;i<512;++i) {
        SP_DEVICE_INTERFACE_DATA item{}; item.cbSize=sizeof(item);
        if (!SetupDiEnumDeviceInterfaces(set,nullptr,&guid,i,&item)) {
            if (GetLastError()==ERROR_NO_MORE_ITEMS) break;
            continue;
        }
        DWORD needed=0;
        SetupDiGetDeviceInterfaceDetailW(set,&item,nullptr,0,&needed,nullptr);
        if (needed<sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W) || needed>65536) continue;
        std::vector<unsigned char> storage(needed);
        auto* detail=reinterpret_cast<SP_DEVICE_INTERFACE_DETAIL_DATA_W*>(storage.data());
        detail->cbSize=sizeof(*detail);
        if (!SetupDiGetDeviceInterfaceDetailW(set,&item,detail,needed,nullptr,nullptr)) continue;
        // Avoid opening unrelated devices, even just for metadata.
        std::wstring path=detail->DevicePath;
        for (auto& c:path) c=static_cast<wchar_t>(towlower(c));
        if (path.find(L"vid_3434&pid_0e40")==std::wstring::npos) continue;
        Handle h{CreateFileW(detail->DevicePath,0,FILE_SHARE_READ|FILE_SHARE_WRITE,
            nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr)};
        if (h.value==INVALID_HANDLE_VALUE) continue;
        HIDD_ATTRIBUTES attributes{}; attributes.Size=sizeof(attributes);
        if (!HidD_GetAttributes(h.value,&attributes) || attributes.VendorID!=0x3434 ||
            attributes.ProductID!=0x0e40 || (attributes.VersionNumber!=0x1212 && attributes.VersionNumber!=0x1213)) continue;
        PHIDP_PREPARSED_DATA prepared=nullptr; HIDP_CAPS caps{};
        if (!HidD_GetPreparsedData(h.value,&prepared)) continue;
        const auto status=HidP_GetCaps(prepared,&caps); HidD_FreePreparsedData(prepared);
        if (status!=HIDP_STATUS_SUCCESS || caps.UsagePage!=0xff60 || caps.Usage!=0x61 ||
            caps.InputReportByteLength!=33 || caps.OutputReportByteLength!=33) continue;
        wchar_t serial[128]{};
        if (!HidD_GetSerialNumberString(h.value,serial,sizeof(serial))) continue;
        serial[127]=0; const auto length=wcslen(serial);
        if (length<5 || wcscmp(serial+length-4,L"HJO1")) continue;
        result.push_back({detail->DevicePath,serial,attributes.VersionNumber});
    }
    return result;
}
bool K4UsbDevicePresent() {
    const auto set=SetupDiGetClassDevsW(nullptr,L"USB",nullptr,DIGCF_PRESENT|DIGCF_ALLCLASSES);
    if (set==INVALID_HANDLE_VALUE) return false;
    struct Guard { HDEVINFO set; ~Guard(){SetupDiDestroyDeviceInfoList(set);} } guard{set};
    for (DWORD i=0;i<1024;++i) {
        SP_DEVINFO_DATA info{}; info.cbSize=sizeof(info);
        if (!SetupDiEnumDeviceInfo(set,i,&info)) {
            if (GetLastError()==ERROR_NO_MORE_ITEMS) break;
            continue;
        }
        wchar_t id[512]{};
        if (!SetupDiGetDeviceInstanceIdW(set,&info,id,512,nullptr)) continue;
        for (auto& c:id) { if (!c) break; c=static_cast<wchar_t>(towupper(c)); }
        if (std::wcsstr(id,L"VID_3434&PID_0E40")) return true;
    }
    return false;
}
namespace {
bool HjoHardwareIds(HDEVINFO set, SP_DEVINFO_DATA& info, const wchar_t* collection) {
    std::array<wchar_t, 4096> ids{};
    DWORD type = 0;
    if (!SetupDiGetDeviceRegistryPropertyW(set, &info, SPDRP_HARDWAREID, &type,
            reinterpret_cast<PBYTE>(ids.data()), static_cast<DWORD>((ids.size() - 2) * sizeof(wchar_t)), nullptr) ||
        type != REG_MULTI_SZ) return false;
    bool device = false, firmware = false, usage = collection == nullptr;
    for (const wchar_t* id = ids.data(); *id; id += std::wcslen(id) + 1) {
        std::wstring upper(id);
        for (auto& c : upper) c = static_cast<wchar_t>(towupper(c));
        device |= upper.find(L"VID_3434&PID_0E40") != std::wstring::npos;
        firmware |= upper.find(L"REV_1212") != std::wstring::npos || upper.find(L"REV_1213") != std::wstring::npos;
        if (collection) usage |= upper.find(collection) != std::wstring::npos;
    }
    return device && firmware && usage;
}
}
std::vector<std::wstring> K4HjoVendorInterfaces() {
    std::vector<std::wstring> result;
    GUID hid{}; HidD_GetHidGuid(&hid);
    const auto set = SetupDiGetClassDevsW(&hid, nullptr, nullptr, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    if (set == INVALID_HANDLE_VALUE) return result;
    struct Guard { HDEVINFO set; ~Guard() { SetupDiDestroyDeviceInfoList(set); } } guard{ set };
    for (DWORD i = 0; i < 256; ++i) {
        SP_DEVICE_INTERFACE_DATA iface{}; iface.cbSize = sizeof(iface);
        if (!SetupDiEnumDeviceInterfaces(set, nullptr, &hid, i, &iface)) break;
        DWORD needed = 0;
        SetupDiGetDeviceInterfaceDetailW(set, &iface, nullptr, 0, &needed, nullptr);
        if (!needed || needed > 4096) continue;
        std::vector<std::uint8_t> buffer(needed);
        auto* detail = reinterpret_cast<SP_DEVICE_INTERFACE_DETAIL_DATA_W*>(buffer.data());
        detail->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W);
        SP_DEVINFO_DATA info{}; info.cbSize = sizeof(info);
        if (!SetupDiGetDeviceInterfaceDetailW(set, &iface, detail, needed, nullptr, &info)) continue;
        if (HjoHardwareIds(set, info, L"HID_DEVICE_UP:FF60_U:0061")) result.emplace_back(detail->DevicePath);
    }
    return result;
}
bool K4HjoRecentlyRemoved(unsigned withinMs) {
    // Not-present nodes keep their hardware IDs and last removal time.
    const auto set = SetupDiGetClassDevsW(nullptr, L"USB", nullptr, DIGCF_ALLCLASSES);
    if (set == INVALID_HANDLE_VALUE) return false;
    struct Guard { HDEVINFO set; ~Guard() { SetupDiDestroyDeviceInfoList(set); } } guard{ set };
    FILETIME nowFt{}; GetSystemTimeAsFileTime(&nowFt);
    const ULONGLONG now = (ULONGLONG(nowFt.dwHighDateTime) << 32) | nowFt.dwLowDateTime;
    for (DWORD i = 0; i < 4096; ++i) {
        SP_DEVINFO_DATA info{}; info.cbSize = sizeof(info);
        if (!SetupDiEnumDeviceInfo(set, i, &info)) { if (GetLastError() == ERROR_NO_MORE_ITEMS) break; continue; }
        if (!HjoHardwareIds(set, info, nullptr)) continue;
        FILETIME removed{}; DEVPROPTYPE type = 0;
        if (!SetupDiGetDevicePropertyW(set, &info, &DEVPKEY_Device_LastRemovalDate, &type,
                reinterpret_cast<PBYTE>(&removed), sizeof(removed), nullptr, 0) || type != DEVPROP_TYPE_FILETIME) continue;
        const ULONGLONG at = (ULONGLONG(removed.dwHighDateTime) << 32) | removed.dwLowDateTime;
        if (at <= now && now - at <= ULONGLONG(withinMs) * 10000ULL) return true;
    }
    return false;
}
WindowsChannel::~WindowsChannel() { if(handle_!=INVALID_HANDLE_VALUE) CloseHandle(handle_); }
bool WindowsChannel::Connect() {
    if (handle_!=INVALID_HANDLE_VALUE) { CloseHandle(handle_); handle_=INVALID_HANDLE_VALUE; }
    if (Cancelled()) return false;
    // Exclusive read/write ownership prevents two hosts interleaving commands.
    handle_=CreateFileW(device_.path.c_str(),GENERIC_READ|GENERIC_WRITE,0,nullptr,
        OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL|FILE_FLAG_OVERLAPPED,nullptr);
    return handle_!=INVALID_HANDLE_VALUE;
}
bool WindowsChannel::Transfer(bool write,void* data,DWORD bytes) {
    if (handle_==INVALID_HANDLE_VALUE || Cancelled()) return false;
    HidIoOperation io(handle_); DWORD error=0,transferred=0;
    const auto start=write?io.StartWrite(data,bytes,&error):io.StartRead(data,bytes,&error);
    if (start==HidIoOperation::StartResult::Failed) return false;
    if (start==HidIoOperation::StartResult::Pending) {
        HANDLE waits[]={io.Event(),cancel_};
        const DWORD waited=WaitForMultipleObjects(cancel_?2:1,waits,FALSE,120);
        if (waited!=WAIT_OBJECT_0) { io.CancelAndDrain(&transferred,&error); return false; }
    }
    return io.Finish(&transferred,&error,false) && transferred==bytes;
}
bool WindowsChannel::Exchange(const Packet& request,Packet& response) {
    std::array<unsigned char,33> output{},input{};
    std::memcpy(output.data()+1,request.data(),request.size());
    if (!Transfer(true,output.data(),static_cast<DWORD>(output.size())) ||
        !Transfer(false,input.data(),static_cast<DWORD>(input.size())) || input[0]) return false;
    std::memcpy(response.data(),input.data()+1,response.size()); return true;
}
bool WindowsChannel::Read(Packet& response) {
    std::array<unsigned char,33> input{};
    if(!Transfer(false,input.data(),static_cast<DWORD>(input.size())) || input[0])return false;
    std::memcpy(response.data(),input.data()+1,response.size());return true;
}
bool WindowsChannel::Reconnect(std::uint16_t revision) {
    if (handle_!=INVALID_HANDLE_VALUE) { CloseHandle(handle_); handle_=INVALID_HANDLE_VALUE; }
    const auto deadline=NowMs()+3500;
    do {
        const auto devices=EnumerateDevices();
        const Device* found=nullptr;
        for (const auto& d:devices) if (d.serial==device_.serial && d.revision==revision) {
            if (found) return false; found=&d;
        }
        if (found) { device_=*found; if (Connect()) return true; }
        if (cancel_) { if(WaitForSingleObject(cancel_,50)==WAIT_OBJECT_0) return false; }
        else Sleep(50);
    } while (!Cancelled() && NowMs()<deadline);
    return false;
}
}

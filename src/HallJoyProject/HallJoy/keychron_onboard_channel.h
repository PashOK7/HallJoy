#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include "keychron_onboard_client.h"
#include <atomic>
#include <string>
#include <vector>
namespace halljoy::k4_onboard {
struct Device { std::wstring path,serial; std::uint16_t revision=0; };
std::vector<Device> EnumerateDevices();
// True while any USB device node of the K4 HE (3434:0E40) is present, including
// while its HID interfaces are still being (re)created after a mode change.
// Metadata only: no device handle is opened.
bool K4UsbDevicePresent();
// HallJoy onboard firmware (HJO1) is identified from Windows metadata alone:
// it reports USB release 0x1212 (keyboard mode) or 0x1213 (native pad mode);
// stock firmware reports its own version. Interface paths of the HJO vendor
// collection (FF60:0061) of every present K4 running it. No device is opened.
std::vector<std::wstring> K4HjoVendorInterfaces();
// True when a K4 running HJO1 was removed within `withinMs`: it is most
// likely re-enumerating after a mode change (for example a previous HallJoy
// exiting) and will return shortly.
bool K4HjoRecentlyRemoved(unsigned withinMs);
class WindowsChannel final : public Channel {
    Device device_;
    HANDLE handle_=INVALID_HANDLE_VALUE;
    HANDLE cancel_=nullptr;
    bool Transfer(bool write,void* data,DWORD bytes);
public:
    explicit WindowsChannel(Device device,HANDLE cancel=nullptr):device_(std::move(device)),cancel_(cancel) {}
    ~WindowsChannel() override;
    WindowsChannel(const WindowsChannel&)=delete;
    WindowsChannel& operator=(const WindowsChannel&)=delete;
    bool Connect();
    bool Exchange(const Packet&,Packet&) override;
    bool Read(Packet&) override;
    bool Reconnect(std::uint16_t revision) override;
    std::uint64_t NowMs() const override { return GetTickCount64(); }
    bool Cancelled() const override { return cancel_ && WaitForSingleObject(cancel_,0)==WAIT_OBJECT_0; }
    const Device& Identity() const { return device_; }
};
}

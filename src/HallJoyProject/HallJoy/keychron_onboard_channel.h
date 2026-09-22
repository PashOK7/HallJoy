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

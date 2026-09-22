#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Gaming.Input.h>
#include "keychron_onboard_gamepad.h"
#include "keychron_hj_protocol.h"
#include <algorithm>
#include <cmath>
#pragma comment(lib,"windowsapp.lib")
namespace {
struct Apartment {
    Apartment(){winrt::init_apartment(winrt::apartment_type::multi_threaded);}
    ~Apartment(){winrt::uninit_apartment();}
};
}
bool KeychronOnboard_ReadOsGamepad(std::array<uint8_t,20>& report) noexcept {
    report={};
    try {
        thread_local Apartment apartment;
        using namespace winrt::Windows::Gaming::Input;
        Gamepad selected{nullptr};
        for(const auto& pad:Gamepad::Gamepads()) {
            const auto raw=RawGameController::FromGameController(pad);
            if(raw.HardwareVendorId()!=0x3434 || raw.HardwareProductId()!=0x0e40) continue;
            if(selected) return false; // ambiguous identical devices: never guess
            selected=pad;
        }
        if(!selected) return false;
        const auto reading=selected.GetCurrentReading();
        uint16_t buttons=0;
        const auto add=[&](GamepadButtons button,uint16_t bit){
            if((reading.Buttons & button)!=GamepadButtons::None) buttons|=bit;
        };
        add(GamepadButtons::A,0x1000);add(GamepadButtons::B,0x2000);
        add(GamepadButtons::X,0x4000);add(GamepadButtons::Y,0x8000);
        add(GamepadButtons::LeftShoulder,0x100);add(GamepadButtons::RightShoulder,0x200);
        add(GamepadButtons::View,0x20);add(GamepadButtons::Menu,0x10);
        add(GamepadButtons::LeftThumbstick,0x40);add(GamepadButtons::RightThumbstick,0x80);
        add(GamepadButtons::DPadUp,1);add(GamepadButtons::DPadDown,2);
        add(GamepadButtons::DPadLeft,4);add(GamepadButtons::DPadRight,8);
        report[1]=20;hjk4_put16(report.data()+2,buttons);
        report[4]=static_cast<uint8_t>(std::lround(std::clamp(reading.LeftTrigger,0.,1.)*255.));
        report[5]=static_cast<uint8_t>(std::lround(std::clamp(reading.RightTrigger,0.,1.)*255.));
        const double axes[]={reading.LeftThumbstickX,reading.LeftThumbstickY,reading.RightThumbstickX,reading.RightThumbstickY};
        for(unsigned i=0;i<4;++i) {
            const double value=std::clamp(axes[i],-1.,1.);
            const auto axis=static_cast<int16_t>(std::lround(value*(value<0?32768.:32767.)));
            hjk4_put16(report.data()+6+2*i,static_cast<uint16_t>(axis));
        }
        return true;
    } catch(...) {report={};return false;}
}

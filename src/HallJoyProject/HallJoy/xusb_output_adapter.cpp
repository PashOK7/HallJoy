#include "xusb_output_adapter.h"

#include <array>

namespace halljoy::xusb_output
{
namespace
{
struct ButtonTranslation final
{
    controller::ButtonV1 source;
    std::uint16_t target;
};

constexpr std::array<ButtonTranslation, 15> kButtons{
    ButtonTranslation{ controller::ButtonV1::South, 0x1000u },
    ButtonTranslation{ controller::ButtonV1::East, 0x2000u },
    ButtonTranslation{ controller::ButtonV1::West, 0x4000u },
    ButtonTranslation{ controller::ButtonV1::North, 0x8000u },
    ButtonTranslation{ controller::ButtonV1::LeftShoulder, 0x0100u },
    ButtonTranslation{ controller::ButtonV1::RightShoulder, 0x0200u },
    ButtonTranslation{ controller::ButtonV1::Select, 0x0020u },
    ButtonTranslation{ controller::ButtonV1::Start, 0x0010u },
    ButtonTranslation{ controller::ButtonV1::Home, 0x0400u },
    ButtonTranslation{ controller::ButtonV1::LeftStick, 0x0040u },
    ButtonTranslation{ controller::ButtonV1::RightStick, 0x0080u },
    ButtonTranslation{ controller::ButtonV1::DpadUp, 0x0001u },
    ButtonTranslation{ controller::ButtonV1::DpadDown, 0x0002u },
    ButtonTranslation{ controller::ButtonV1::DpadLeft, 0x0004u },
    ButtonTranslation{ controller::ButtonV1::DpadRight, 0x0008u },
};
}

vigem_output::XusbReportV1 ToReport(
    const controller::VirtualControllerFrameV1& frame) noexcept
{
    vigem_output::XusbReportV1 report{};
    for (const ButtonTranslation& button : kButtons)
    {
        if ((frame.buttons & controller::ButtonMask(button.source)) != 0)
            report.buttons |= button.target;
    }
    report.leftTrigger = frame.leftTrigger;
    report.rightTrigger = frame.rightTrigger;
    report.thumbLX = frame.leftStickX;
    report.thumbLY = frame.leftStickY;
    report.thumbRX = frame.rightStickX;
    report.thumbRY = frame.rightStickY;
    return report;
}

bool ReportsEqual(const vigem_output::XusbReportV1& left,
    const vigem_output::XusbReportV1& right) noexcept
{
    return left.buttons == right.buttons &&
        left.leftTrigger == right.leftTrigger &&
        left.rightTrigger == right.rightTrigger &&
        left.thumbLX == right.thumbLX && left.thumbLY == right.thumbLY &&
        left.thumbRX == right.thumbRX && left.thumbRY == right.thumbRY;
}
}

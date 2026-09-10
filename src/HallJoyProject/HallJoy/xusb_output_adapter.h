#pragma once

#include "vigem_output_shared.h"
#include "virtual_controller_frame.h"

namespace halljoy::xusb_output
{
vigem_output::XusbReportV1 ToReport(
    const controller::VirtualControllerFrameV1& frame) noexcept;

bool ReportsEqual(const vigem_output::XusbReportV1& left,
    const vigem_output::XusbReportV1& right) noexcept;
}

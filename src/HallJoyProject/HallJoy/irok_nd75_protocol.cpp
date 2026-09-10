#include "irok_nd75_protocol.h"

#include <algorithm>
#include <cstring>

namespace irok_nd75
{
namespace
{
Report Base(std::uint8_t command) noexcept
{
    Report report{};
    report[0] = kReportId;
    report[1] = command;
    return report;
}

bool IsResponse(const std::uint8_t* report, std::size_t bytes,
    std::uint8_t command) noexcept
{
    return report && bytes >= kReportBytes && report[0] == kReportId &&
        report[1] == command;
}

template <std::size_t N>
bool StoreField(std::array<char, N>* destination, const std::uint8_t* begin,
    const std::uint8_t* end) noexcept
{
    if (!destination || !begin || !end || end < begin ||
        static_cast<std::size_t>(end - begin) + 1u > destination->size())
        return false;
    std::size_t used = 0;
    for (auto current = begin; current != end; ++current)
    {
        if (*current < 0x20u || *current > 0x7eu) return false;
        (*destination)[used++] = static_cast<char>(*current);
    }
    return used != 0;
}
}

Report BuildIdentityRequest() noexcept
{
    return Base(kIdentityCommand);
}

Report BuildCapabilityRequest() noexcept
{
    auto report = Base(kHostAnalogCommand);
    report[5] = kAnalogChannel;
    report[6] = 0x04;
    return report;
}

Report BuildSubscriptionRequest(
    const std::array<std::uint8_t, kColumns>& mask) noexcept
{
    auto report = Base(kHostAnalogCommand);
    report[5] = kAnalogChannel;
    report[6] = 0x02;
    std::copy(mask.begin(), mask.end(), report.begin() + 7);
    return report;
}

Report BuildUnsubscribeRequest() noexcept
{
    auto report = Base(kHostAnalogCommand);
    report[5] = kAnalogChannel;
    report[6] = 0x03;
    return report;
}

PositionToHid FactoryMap() noexcept
{
    // Official KeyInfo_X86HERGB.config, EveryKeyInfo_Row01..Row06. The
    // encoder is not a keyboard usage; the internal Fn position uses HallJoy's
    // established 0xFA pseudo-usage and is intentionally not bindable as HID.
    return {
        0x29, 0x00, 0x3a, 0x3b, 0x3c, 0x3d, 0x00, 0x3e, 0x3f, 0x40, 0x41,
        0x42, 0x43, 0x44, 0x45, 0x49, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x35, 0x1e, 0x1f, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27,
        0x2d, 0x2e, 0x00, 0x2a, 0x4c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x2b, 0x14, 0x1a, 0x08, 0x15, 0x17, 0x1c, 0x18, 0x0c, 0x12, 0x13,
        0x2f, 0x30, 0x00, 0x31, 0x4b, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x39, 0x00, 0x04, 0x16, 0x07, 0x09, 0x0a, 0x0b, 0x0d, 0x0e, 0x0f,
        0x33, 0x34, 0x00, 0x28, 0x4e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0xe1, 0x00, 0x1d, 0x1b, 0x06, 0x19, 0x05, 0x11, 0x10, 0x36, 0x37,
        0x38, 0xe5, 0x00, 0x52, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0xe0, 0xe3, 0xe2, 0x00, 0x00, 0x00, 0x2c, 0x00, 0x00, 0xe6, 0xfa,
        0x00, 0xe4, 0x50, 0x51, 0x4f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    };
}

std::array<std::uint8_t, kColumns> SubscriptionMask(
    const PositionToHid& map) noexcept
{
    std::array<std::uint8_t, kColumns> mask{};
    for (std::size_t position = 0; position < map.size(); ++position)
        if (map[position] != 0)
            mask[position % kColumns] |= static_cast<std::uint8_t>(
                1u << (position / kColumns));
    return mask;
}

bool DecodeDeviceInfo(const std::uint8_t* r, std::size_t bytes,
    DeviceInfo* out) noexcept
{
    if (out) *out = {};
    if (!out || !IsResponse(r, bytes, kIdentityCommand) || r[2] != 0 ||
        r[4] != 0 || r[5] <= 5u || r[5] >= kReportBytes)
        return false;

    const std::size_t begin = 6u;
    const std::size_t end = static_cast<std::size_t>(r[5]) + 1u;
    if (end > bytes || end <= begin) return false;

    std::array<std::pair<const std::uint8_t*, const std::uint8_t*>, 6> fields{};
    std::size_t field = 0;
    const std::uint8_t* fieldBegin = r + begin;
    for (std::size_t index = begin; index <= end; ++index)
    {
        const bool terminal = index == end || r[index] == 0;
        if (!terminal && r[index] != ',') continue;
        if (field >= fields.size()) return false;
        fields[field++] = { fieldBegin, r + index };
        fieldBegin = r + index + 1;
        if (terminal) break;
    }
    if (field < 5u) return false;
    if (!StoreField(&out->controller, fields[0].first, fields[0].second) ||
        !StoreField(&out->product, fields[4].first, fields[4].second))
        return false;
    if (field >= 6u && fields[5].first != fields[5].second &&
        !StoreField(&out->firmware, fields[5].first, fields[5].second))
        return false;
    return true;
}

bool IsExpectedDevice(const DeviceInfo& info) noexcept
{
    return std::strcmp(info.controller.data(), "M484") == 0 &&
        std::strcmp(info.product.data(), "X86HERGB") == 0;
}

bool IsFirmwareWithinKnownFamily(const DeviceInfo& info) noexcept
{
    return std::strncmp(info.firmware.data(), "V1.", 3) == 0;
}

bool DecodeCapabilityInfo(const std::uint8_t* r, std::size_t bytes,
    CapabilityInfo* out) noexcept
{
    if (out) *out = {};
    if (!out || !IsResponse(r, bytes, kDeviceAnalogCommand) ||
        r[5] < 2u || r[6] != 0x04)
        return false;
    out->sensitivity = r[7];
    return out->sensitivity <= kNominalTravelMaximum;
}

bool DecodeLiveEvent(const std::uint8_t* r, std::size_t bytes,
    LiveEvent* out) noexcept
{
    if (out) *out = {};
    if (!out || !IsResponse(r, bytes, kDeviceAnalogCommand) ||
        r[5] < 3u || r[6] != 0x01)
        return false;
    LiveEvent event{};
    event.row = r[7];
    event.column = r[8];
    event.travel = r[9];
    if (event.row >= kRows || event.column >= kColumns) return false;
    *out = event;
    return true;
}

ReadFailureAction ClassifyReadFailure(bool stopping, bool timedOut,
    bool deviceLost, std::uint32_t consecutiveErrors) noexcept
{
    if (stopping || deviceLost || (!timedOut && consecutiveErrors >= 3u))
        return ReadFailureAction::EndSession;
    return ReadFailureAction::Continue;
}

std::uint16_t ToMilli(std::uint8_t travel) noexcept
{
    return static_cast<std::uint16_t>(std::min<std::uint32_t>(1000u,
        (std::uint32_t(travel) * 1000u + kNominalTravelMaximum / 2u) /
            kNominalTravelMaximum));
}

std::size_t MappedKeyCount(const PositionToHid& map) noexcept
{
    return static_cast<std::size_t>(std::count_if(map.begin(), map.end(),
        [](std::uint8_t hid) { return hid != 0; }));
}
}

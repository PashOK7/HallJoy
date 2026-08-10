#include "aula_w669_protocol.h"

#include <algorithm>
#include <cctype>
#include <cstring>

namespace aula_w669
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
}

Report BuildDeviceInfoRequest() noexcept
{
    return Base(kDeviceInfoCommand);
}

Report BuildTravelInfoRequest() noexcept
{
    auto report = Base(kAnalogCommand);
    report[6] = 0x04;
    return report;
}

Report BuildKeyMapRequest() noexcept
{
    auto report = Base(kKeyMapCommand);
    report[2] = 0x80;
    return report;
}

Report BuildSubscriptionRequest(const std::array<std::uint8_t, kColumns>& mask) noexcept
{
    auto report = Base(kAnalogCommand);
    report[5] = 0x18;
    report[6] = 0x02;
    std::copy(mask.begin(), mask.end(), report.begin() + 7);
    return report;
}

Report BuildSnapshotRequest() noexcept
{
    auto report = Base(kAnalogCommand);
    report[6] = 0x0e;
    return report;
}

Report BuildPollRateQuery() noexcept
{
    auto report = Base(kAnalogCommand);
    report[6] = 0x0a;
    return report;
}

Report BuildUnsubscribeRequest() noexcept
{
    auto report = Base(kAnalogCommand);
    report[6] = 0x03;
    return report;
}

PositionToHid Win60FactoryMap() noexcept
{
    // Official SI2825KZHEARGB layout indices.  An all-zero 0x18/0x80 record
    // means "inherit the factory assignment"; it does not mean that the
    // matrix position is absent.  Keep this baseline separate from explicit
    // remap records so a future device profile can provide a different map.
    return {
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x29, 0x1e, 0x1f, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27,
        0x2d, 0x2e, 0x00, 0x2a, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x2b, 0x14, 0x1a, 0x08, 0x15, 0x17, 0x1c, 0x18, 0x0c, 0x12, 0x13,
        0x2f, 0x30, 0x00, 0x31, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x39, 0x00, 0x04, 0x16, 0x07, 0x09, 0x0a, 0x0b, 0x0d, 0x0e, 0x0f,
        0x33, 0x34, 0x00, 0x28, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0xe1, 0x00, 0x1d, 0x1b, 0x06, 0x19, 0x05, 0x11, 0x10, 0x36, 0x37,
        0x38, 0xe5, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0xe0, 0xe3, 0xe2, 0x00, 0x00, 0x00, 0x2c, 0x00, 0x00, 0xe6, 0x65,
        0xe4, 0xfa, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    };
}

PositionToHid Win68FactoryMap() noexcept
{
    // Official SI2828HEARGB / SI2828KZHEARGB 68-key layout.  Indices are the
    // firmware's row-major 6x22 sensor positions, not visual key order.
    auto map = Win60FactoryMap();
    map[38] = 0x49;  // Insert
    map[60] = 0x4c;  // Delete
    map[82] = 0x4b;  // Page Up
    map[100] = 0x00;
    map[101] = 0xe5; // Right Shift
    map[103] = 0x52; // Up
    map[104] = 0x4e; // Page Down
    map[119] = 0xe6;
    map[120] = 0x00;
    map[121] = 0xfa; // Fn
    map[122] = 0xe4; // Right Control
    map[124] = 0x50; // Left
    map[125] = 0x51; // Down
    map[126] = 0x4f; // Right
    return map;
}

PositionToHid KpTe153UkFactoryMap() noexcept
{
    // Official SI2851UKKZHEARGB 69-key ISO-UK/JIS-derived layout.
    auto map = Win68FactoryMap();
    map[38] = 0x4a;  // Home
    map[58] = 0x28;  // ISO Enter
    map[79] = 0x32;  // Non-US #/~
    map[80] = 0x00;
    map[89] = 0x64;  // Non-US backslash
    map[99] = 0x87;  // International Ro
    return map;
}

PositionToHid K673BrFactoryMap() noexcept
{
    // Official iLLumiPC 7272BRHEXYXK673JCARGB profile. Matrix indices are
    // firmware sensor positions; the rotary encoder and internal Fn control
    // deliberately remain unpublished because they have no keyboard usage.
    return {
        0x29, 0x00, 0x3a, 0x3b, 0x3c, 0x3d, 0x00, 0x3e, 0x3f, 0x40, 0x41,
        0x42, 0x43, 0x44, 0x45, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x35, 0x1e, 0x1f, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27,
        0x2d, 0x2e, 0x00, 0x2a, 0x49, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x2b, 0x14, 0x1a, 0x08, 0x15, 0x17, 0x1c, 0x18, 0x0c, 0x12, 0x13,
        0x2f, 0x30, 0x00, 0x00, 0x4c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x39, 0x00, 0x04, 0x16, 0x07, 0x09, 0x0a, 0x0b, 0x0d, 0x0e, 0x0f,
        0x33, 0x34, 0x32, 0x28, 0x4b, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0xe1, 0x64, 0x1d, 0x1b, 0x06, 0x19, 0x05, 0x11, 0x10, 0x36, 0x37,
        0x38, 0x87, 0x00, 0x52, 0x4e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0xe0, 0xe3, 0xe2, 0x00, 0x00, 0x00, 0x2c, 0x00, 0x00, 0x00, 0xe6,
        0x00, 0xe4, 0x50, 0x51, 0x4f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    };
}

PositionToHid K673UkFactoryMap() noexcept
{
    auto map = K673BrFactoryMap();
    map[79] = 0x31;  // Backslash beside ISO Enter
    map[100] = 0x00;
    map[101] = 0xe5; // Right Shift
    return map;
}

PositionToHid K673UsFactoryMap() noexcept
{
    auto map = K673UkFactoryMap();
    map[58] = 0x31;  // ANSI backslash
    map[79] = 0x00;
    map[89] = 0x00;  // No ISO key
    return map;
}

FactoryLayoutProfile FactoryProfileForProduct(const char* product) noexcept
{
    if (!product) return FactoryLayoutProfile::Unknown;
    char normalized[kDeviceProductBytes]{};
    std::size_t used = 0;
    while (*product && used + 1u < sizeof(normalized))
    {
        const auto ch = static_cast<unsigned char>(*product++);
        if (!std::isspace(ch))
            normalized[used++] = static_cast<char>(std::toupper(ch));
    }
    const auto is = [&](const char* expected) noexcept {
        return std::strcmp(normalized, expected) == 0;
    };
    if (is("SI2825HEARGB") || is("SI2825KRT12HEARGB") ||
        is("SI2825KR-AHEARGB") || is("SI2825KZHEARGB"))
        return FactoryLayoutProfile::Si2825Win60;
    if (is("SI2828HEARGB") || is("SI2828KZHEARGB"))
        return FactoryLayoutProfile::Si2828Win68;
    if (is("SI2851UKKZHEARGB"))
        return FactoryLayoutProfile::Si2851KpTe153Uk;
    if (is("7272BRHEXYXK673JCARGB"))
        return FactoryLayoutProfile::K673Br;
    if (is("7272UKHEXYXBJCARGB"))
        return FactoryLayoutProfile::K673Uk;
    if (is("7272USHEXYXK673JCARGB"))
        return FactoryLayoutProfile::K673Us;
    return FactoryLayoutProfile::Unknown;
}

PositionToHid FactoryMap(FactoryLayoutProfile profile) noexcept
{
    switch (profile)
    {
    case FactoryLayoutProfile::Si2825Win60: return Win60FactoryMap();
    case FactoryLayoutProfile::Si2828Win68: return Win68FactoryMap();
    case FactoryLayoutProfile::Si2851KpTe153Uk: return KpTe153UkFactoryMap();
    case FactoryLayoutProfile::K673Br: return K673BrFactoryMap();
    case FactoryLayoutProfile::K673Uk: return K673UkFactoryMap();
    case FactoryLayoutProfile::K673Us: return K673UsFactoryMap();
    default: return {};
    }
}

bool DecodeDeviceInfo(const std::uint8_t* r, std::size_t bytes,
    DeviceInfo* out) noexcept
{
    if (out) *out = DeviceInfo{};
    // The official WebHID client reads data[0]=0x0d, data[1]=success,
    // data[3]=0 and treats data[4] as the exclusive end of a CSV string that
    // begins at data[5].  Windows prepends report ID 1 to those indices.
    if (!out || !IsResponse(r, bytes, kDeviceInfoCommand) || r[2] != 0 ||
        r[4] != 0 || r[5] <= 5u || r[5] >= kReportBytes)
        return false;

    const std::size_t textBegin = 6u;
    const std::size_t textEnd = static_cast<std::size_t>(r[5]) + 1u;
    if (textEnd > bytes || textEnd <= textBegin) return false;

    std::size_t field = 0, productBytes = 0;
    for (std::size_t index = textBegin; index < textEnd; ++index)
    {
        const std::uint8_t value = r[index];
        if (value == 0) break;
        if (value == ',')
        {
            ++field;
            if (field > 4u) break;
            continue;
        }
        if (value < 0x20u || value > 0x7eu) return false;
        if (field == 4u)
        {
            if (productBytes + 1u >= out->product.size()) return false;
            out->product[productBytes++] = static_cast<char>(value);
        }
    }
    return field >= 4u && productBytes != 0u;
}

bool DecodeTravelInfo(const std::uint8_t* r, std::size_t bytes, TravelInfo* out) noexcept
{
    if (!out || !IsResponse(r, bytes, kAnalogCommand) || r[6] != 0x04 ||
        r[5] < 6 || r[7] == 0 || r[10] == 0)
        return false;
    TravelInfo info{};
    info.maximum = static_cast<std::uint16_t>(r[7] | (std::uint16_t(r[10]) << 8));
    info.unitCode = r[8];
    info.formatCode = r[11];
    if (info.maximum < 32 || info.maximum > 10000) return false;
    *out = info;
    return true;
}

bool DecodeKeyMapFragment(const std::uint8_t* r, std::size_t bytes,
    PositionToHid* map, std::array<bool, 10>* received) noexcept
{
    if (!map || !received || !IsResponse(r, bytes, kKeyMapCommand) || r[2] != 0x80)
        return false;
    const std::size_t fragment = (std::size_t(r[3]) << 8) | r[4];
    if (fragment >= received->size()) return false;
    const std::size_t expected = fragment < 9 ? 56u : 24u;
    if (r[5] != expected || 6u + expected > bytes) return false;
    const std::size_t records = expected / 4u;
    for (std::size_t i = 0; i < records; ++i)
    {
        const std::size_t position = fragment * 14u + i;
        const std::uint8_t code1 = r[6 + i * 4u];
        const std::uint8_t hid = r[7 + i * 4u];
        // A zero record inherits the factory layer.  Non-zero records are
        // explicit assignments; ordinary ones expose their USB HID usage in
        // byte 1, while advanced/macro records are deliberately unpublished.
        if (code1 != 0 && code1 != 0xff)
            (*map)[position] = (hid != 0 && hid != 0xff) ? hid : 0;
    }
    (*received)[fragment] = true;
    return true;
}

bool DecodeLiveEvent(const std::uint8_t* r, std::size_t bytes, LiveEvent* out) noexcept
{
    if (!out || !IsResponse(r, bytes, kAnalogCommand) || r[6] != 0x01 ||
        r[5] < 3 || bytes < 11)
        return false;
    LiveEvent event{};
    event.declaredLength = r[5];
    event.row = r[7];
    event.column = r[8];
    event.travel = static_cast<std::uint16_t>(r[9] | (std::uint16_t(r[10]) << 8));
    if (event.row >= kRows || event.column >= kColumns) return false;
    *out = event;
    return true;
}

bool DecodeSnapshotPacket(const std::uint8_t* r, std::size_t bytes,
    std::array<std::uint16_t, kColumns>* rowValues) noexcept
{
    if (!rowValues || !IsResponse(r, bytes, kAnalogCommand) || r[6] != 0x0e ||
        r[5] != 0x30 || bytes < 51)
        return false;
    for (std::size_t column = 0; column < kColumns; ++column)
        (*rowValues)[column] = static_cast<std::uint16_t>(
            (std::uint16_t(r[7 + column * 2u]) << 8) | r[8 + column * 2u]);
    return true;
}

bool DecodePollRate(const std::uint8_t* r, std::size_t bytes,
    std::uint8_t* code, std::uint16_t* nominalHz) noexcept
{
    if (!code || !nominalHz || !IsResponse(r, bytes, kAnalogCommand) ||
        r[6] != 0x09 || r[5] < 2)
        return false;
    const std::uint8_t value = r[7];
    std::uint16_t hz = 0;
    switch (value)
    {
    // Zero is the firmware's untouched/default setting.  It is a valid query
    // response, but the firmware does not state a nominal rate for it.
    case 0: hz = 0; break;
    case 1: hz = 1000; break;
    case 2: hz = 2000; break;
    case 4: hz = 4000; break;
    case 8: hz = 8000; break;
    default: return false;
    }
    *code = value; *nominalHz = hz; return true;
}

std::uint16_t ToMilli(std::uint16_t travel, std::uint16_t maximum) noexcept
{
    if (maximum == 0) return 0;
    return static_cast<std::uint16_t>(std::min<std::uint32_t>(1000u,
        (std::uint32_t(travel) * 1000u + maximum / 2u) / maximum));
}

std::size_t MappedKeyCount(const PositionToHid& map) noexcept
{
    return static_cast<std::size_t>(std::count_if(map.begin(), map.end(),
        [](std::uint8_t hid) { return hid != 0; }));
}
}

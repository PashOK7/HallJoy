#include "../HallJoy/drunkdeer_protocol.h"
#include "../HallJoy/analog_key_codes.h"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <iterator>
#include <utility>

int main()
{
    using namespace drunkdeer;

    const auto start = BuildTrackingRequest(true);
    const auto stop = BuildTrackingRequest(false);
    assert(start.size() == 64);
    assert(start[0] == 0x04 && start[1] == 0xb6 &&
        start[2] == 0x03 && start[3] == 0x01);
    assert(stop[0] == 0x04 && stop[1] == 0xb6 &&
        stop[2] == 0x03 && stop[3] == 0x00);
    assert(BuildMatrixRequest() == start);
    assert(std::count(start.begin() + 4, start.end(), 0) == 60);

    Reports reports{};
    for (std::size_t report = 0; report < reports.size(); ++report)
    {
        reports[report][0] = kReportId;
        reports[report][1] = kTrackingResponseCommand;
        reports[report][2] = static_cast<std::uint8_t>(0x20 + report);
        reports[report][3] = static_cast<std::uint8_t>(0x30 + report);
        reports[report][4] = static_cast<std::uint8_t>(report);
        for (std::size_t byte = kHeaderBytes; byte < kReportBytes; ++byte)
            reports[report][byte] = static_cast<std::uint8_t>(
                report * (kReportBytes - kHeaderBytes) +
                byte - kHeaderBytes);
    }
    // A frame decoder must use chunk numbers, not arrival order.
    std::swap(reports[0], reports[2]);
    TrackingFrame frame{};
    assert(DecodeTrackingFrame(reports, &frame));
    assert(frame.reports[0][4] == 0 && frame.reports[1][4] == 1 &&
        frame.reports[2][4] == 2);
    assert(frame.payload.size() == 177);
    assert(frame.payload[0] == 0);
    assert(frame.payload[58] == 58);
    assert(frame.payload[59] == 59);
    assert(frame.payload[117] == 117);
    assert(frame.payload[118] == 118);
    assert(frame.payload[125] == 125);
    assert(frame.payload[176] == 176);

    TrackingFrameAssembler assembler;
    Report unrelated{};
    unrelated[0] = 3;
    assert(assembler.Consume(unrelated) == TrackingConsumeResult::Ignored);
    assert(assembler.Consume(frame.reports[2]) ==
        TrackingConsumeResult::OutOfOrder);
    assert(assembler.Consume(frame.reports[0]) ==
        TrackingConsumeResult::Accepted);
    assert(assembler.Consume(frame.reports[2]) ==
        TrackingConsumeResult::OutOfOrder);
    assert(assembler.Consume(frame.reports[0]) ==
        TrackingConsumeResult::Restarted);
    assert(assembler.Consume(frame.reports[1]) ==
        TrackingConsumeResult::Accepted);
    assert(assembler.Consume(frame.reports[2]) ==
        TrackingConsumeResult::Complete);
    assert(assembler.OrderedReports() == frame.reports);
    assembler.Reset();
    assert(assembler.Consume(frame.reports[1]) ==
        TrackingConsumeResult::OutOfOrder);

    Matrix matrix{};
    assert(DecodeMatrix(reports, &matrix));
    assert(matrix[0] == 0);
    assert(matrix[58] == 58);
    assert(matrix[59] == 59);
    assert(matrix[117] == 117);
    assert(matrix[118] == 118);
    assert(matrix[125] == 125);
    reports[1][1] = 3;
    assert(!DecodeMatrix(reports, &matrix));
    assert(std::all_of(matrix.begin(), matrix.end(),
        [](std::uint8_t value) { return value == 0; }));
    assert(!DecodeMatrix(reports, nullptr));

    reports[1][1] = kTrackingResponseCommand;
    reports[1][4] = reports[0][4];
    assert(!DecodeTrackingFrame(reports, &frame)); // duplicate chunk
    reports[1][4] = 3;
    assert(!DecodeTrackingFrame(reports, &frame)); // impossible chunk

    const auto map = GenericUapMap();
    assert(map.size() == 126);
    assert(MappedKeyCount(map) == 82);
    assert(map[0 * kColumns + 0] == 0x29); // Escape
    assert(map[3 * kColumns + 1] == 0x04); // A
    assert(map[5 * kColumns + 6] == 0x2c); // Space
    assert(map[5 * kColumns + 11] == 0x409); // Soup/UAP Fn
    assert(map[5 * kColumns + 12] == 0x403); // Soup/UAP OEM_1
    assert(map[4 * kColumns + 14] == 0x52); // Up
    assert(map[5 * kColumns + 13] == 0);    // unmapped gap
    assert(map[5 * kColumns + 14] == 0x50); // Left
    assert(map[5 * kColumns + 15] == 0x51); // Down
    assert(map[5 * kColumns + 16] == 0x4f); // Right

    const auto g65 = G65AnsiMap();
    assert(g65.size() == 126);
    assert(MappedKeyCount(g65) == 68);
    assert(MapForProduct(0x2382) == g65);
    assert(MapForProduct(0x2383) == map);

    // Every high-confidence raw cell captured by physical firmware 0012 log
    // HallJoyStabilityTrace (3).log must agree with the compiled PID map.
    constexpr std::pair<std::size_t, std::uint16_t> observed[] = {
        {21, 0x29}, {22, 0x1e}, {23, 0x1f}, {24, 0x20},
        {25, 0x21}, {26, 0x22}, {27, 0x23}, {28, 0x24},
        {29, 0x25}, {30, 0x26}, {31, 0x27}, {32, 0x2d},
        {33, 0x2e}, {34, 0x2a},
        {42, 0x2b}, {43, 0x14}, {44, 0x1a}, {45, 0x08},
        {47, 0x17}, {48, 0x1c}, {49, 0x18}, {50, 0x0c},
        {52, 0x13}, {53, 0x2f}, {54, 0x30},
        {63, 0x39}, {64, 0x04}, {69, 0x0b}, {70, 0x0d},
        {71, 0x0e},
        {84, 0xe1}, {86, 0x1d}, {87, 0x1b}, {91, 0x11},
        {92, 0x10}, {94, 0x37}, {95, 0x38}, {96, 0xe5},
        {97, 0x52},
        {105, 0xe0}, {107, 0xe2}, {111, 0x2c}, {114, 0xe6},
        {118, 0x51}, {119, 0x4f},
    };
    static_assert(std::size(observed) == 45);
    for (const auto& [offset, hid] : observed)
        assert(g65[offset] == hid);

    // Official ANSI G65 right column and compressed navigation cluster.
    assert(g65[1 * kColumns + 14] == 0x4c); // Delete: Antler offset 35
    assert(g65[2 * kColumns + 14] == 0x4d); // End: Antler offset 56
    assert(g65[3 * kColumns + 14] == 0x4b); // Page Up: Antler offset 77
    assert(g65[4 * kColumns + 14] == 0x4e); // Page Down: Antler offset 98
    for (std::size_t row = 1; row <= 4; ++row)
        assert(g65[row * kColumns + 15] == 0); // old guessed cells are empty
    assert(g65[4 * kColumns + 12] == 0xe5); // Right Shift
    assert(g65[4 * kColumns + 13] == 0x52); // Up
    assert(g65[5 * kColumns + 9] == 0xe6);  // Right Alt
    assert(g65[5 * kColumns + 10] == halljoy::keycode::kFn);
    assert(g65[5 * kColumns + 11] == halljoy::keycode::kOem1);
    assert(g65[5 * kColumns + 12] == 0x50); // Left
    assert(g65[5 * kColumns + 13] == 0x51); // Down
    assert(g65[5 * kColumns + 14] == 0x4f); // Right

    // All ordinary and extended codes are one-to-one; no raw cell aliases a
    // second physical key in the complete static G65 table.
    for (const auto code : g65)
        if (code)
            assert(std::count(g65.begin(), g65.end(), code) == 1);

    assert(ToMilli(0) == 0);
    assert(ToMilli(20) == 500);
    assert(ToMilli(40) == 1000);
    assert(ToMilli(255) == 1000);
    return 0;
}

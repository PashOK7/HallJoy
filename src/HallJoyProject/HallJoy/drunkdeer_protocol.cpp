#include "drunkdeer_protocol.h"

#include "analog_key_codes.h"

#include <algorithm>

namespace drunkdeer
{
Report BuildTrackingRequest(bool enabled) noexcept
{
    Report report{};
    report[0] = kReportId;
    report[1] = kTrackingRequestCommand;
    report[2] = 0x03;
    report[3] = enabled ? 0x01 : 0x00;
    return report;
}

Report BuildMatrixRequest() noexcept
{
    return BuildTrackingRequest(true);
}

bool TrackingChunkIndex(const Report& report, std::size_t* out) noexcept
{
    if (out) *out = 0;
    if (!out || report[0] != kReportId ||
        report[1] != kTrackingResponseCommand ||
        report[4] >= kReportCount)
        return false;
    *out = report[4];
    return true;
}

TrackingConsumeResult TrackingFrameAssembler::Consume(
    const Report& report) noexcept
{
    std::size_t chunk = 0;
    if (!TrackingChunkIndex(report, &chunk))
        return TrackingConsumeResult::Ignored;
    if (chunk == 0)
    {
        const bool restarted = expectedChunk_ != 0;
        reports_ = {};
        reports_[0] = report;
        expectedChunk_ = 1;
        return restarted ? TrackingConsumeResult::Restarted :
            TrackingConsumeResult::Accepted;
    }
    if (chunk != expectedChunk_)
        return TrackingConsumeResult::OutOfOrder;
    reports_[chunk] = report;
    ++expectedChunk_;
    return expectedChunk_ == kReportCount ?
        TrackingConsumeResult::Complete : TrackingConsumeResult::Accepted;
}

void TrackingFrameAssembler::Reset() noexcept
{
    reports_ = {};
    expectedChunk_ = 0;
}

bool DecodeTrackingFrame(const Reports& arrivals, TrackingFrame* out) noexcept
{
    if (out) *out = {};
    if (!out) return false;

    std::array<bool, kReportCount> seen{};
    for (const auto& report : arrivals)
    {
        std::size_t chunk = 0;
        if (!TrackingChunkIndex(report, &chunk) || seen[chunk])
            return false;
        seen[chunk] = true;
        out->reports[chunk] = report;
    }
    if (!std::all_of(seen.begin(), seen.end(), [](bool value) {
            return value;
        }))
        return false;

    for (std::size_t chunk = 0; chunk < kReportCount; ++chunk)
    {
        std::copy(out->reports[chunk].begin() + kHeaderBytes,
            out->reports[chunk].end(),
            out->payload.begin() + chunk * kPayloadBytesPerReport);
    }
    std::copy_n(out->payload.begin(), out->matrix.size(),
        out->matrix.begin());
    return true;
}

bool DecodeMatrix(const Reports& reports, Matrix* out) noexcept
{
    if (out) out->fill(0);
    if (!out) return false;
    TrackingFrame frame{};
    if (!DecodeTrackingFrame(reports, &frame)) return false;
    *out = frame.matrix;
    return true;
}

PositionToHid GenericUapMap() noexcept
{
    PositionToHid map{};
    const auto put = [&map](std::size_t row, std::size_t column,
        std::uint16_t code) noexcept {
        map[row * kColumns + column] = code;
    };

    put(0, 0, 0x29); // Escape
    put(0, 2, 0x3a); put(0, 3, 0x3b); put(0, 4, 0x3c);
    put(0, 5, 0x3d); put(0, 6, 0x3e); put(0, 7, 0x3f);
    put(0, 8, 0x40); put(0, 9, 0x41); put(0, 10, 0x42);
    put(0, 11, 0x43); put(0, 12, 0x44); put(0, 13, 0x45);
    put(0, 14, 0x4c); // Delete

    put(1, 0, 0x35); put(1, 1, 0x1e); put(1, 2, 0x1f);
    put(1, 3, 0x20); put(1, 4, 0x21); put(1, 5, 0x22);
    put(1, 6, 0x23); put(1, 7, 0x24); put(1, 8, 0x25);
    put(1, 9, 0x26); put(1, 10, 0x27); put(1, 11, 0x2d);
    put(1, 12, 0x2e); put(1, 13, 0x2a); put(1, 15, 0x4a);

    put(2, 0, 0x2b); put(2, 1, 0x14); put(2, 2, 0x1a);
    put(2, 3, 0x08); put(2, 4, 0x15); put(2, 5, 0x17);
    put(2, 6, 0x1c); put(2, 7, 0x18); put(2, 8, 0x0c);
    put(2, 9, 0x12); put(2, 10, 0x13); put(2, 11, 0x2f);
    put(2, 12, 0x30); put(2, 13, 0x31); put(2, 15, 0x4b);

    put(3, 0, 0x39); put(3, 1, 0x04); put(3, 2, 0x16);
    put(3, 3, 0x07); put(3, 4, 0x09); put(3, 5, 0x0a);
    put(3, 6, 0x0b); put(3, 7, 0x0d); put(3, 8, 0x0e);
    put(3, 9, 0x0f); put(3, 10, 0x33); put(3, 11, 0x34);
    put(3, 13, 0x28); put(3, 15, 0x4e);

    put(4, 0, 0xe1); put(4, 2, 0x1d); put(4, 3, 0x1b);
    put(4, 4, 0x06); put(4, 5, 0x19); put(4, 6, 0x05);
    put(4, 7, 0x11); put(4, 8, 0x10); put(4, 9, 0x36);
    put(4, 10, 0x37); put(4, 11, 0x38); put(4, 13, 0xe5);
    put(4, 14, 0x52); put(4, 15, 0x4d);

    put(5, 0, 0xe0); put(5, 1, 0xe3); put(5, 2, 0xe2);
    put(5, 6, 0x2c); put(5, 10, 0xe6); put(5, 11, 0x409);
    put(5, 12, 0x403); // Soup/UAP OEM_1 code, not USB HID Application.
    put(5, 14, 0x50); put(5, 15, 0x51); put(5, 16, 0x4f);
    return map;
}

PositionToHid G65AnsiMap() noexcept
{
    PositionToHid map{};
    const auto put = [&map](std::size_t row, std::size_t column,
        std::uint16_t code) noexcept {
        map[row * kColumns + column] = code;
    };

    // Physical G65 row 1: Esc replaces the generic full-size Backquote row.
    put(1, 0, 0x29); put(1, 1, 0x1e); put(1, 2, 0x1f);
    put(1, 3, 0x20); put(1, 4, 0x21); put(1, 5, 0x22);
    put(1, 6, 0x23); put(1, 7, 0x24); put(1, 8, 0x25);
    put(1, 9, 0x26); put(1, 10, 0x27); put(1, 11, 0x2d);
    // Antler getG65() and factory layer agree on navigation column 14.
    // These four cells were not exercised by the historical firmware 0012 log.
    // Owner selected the official map on 2026-09-09; measured cells stay intact.
    put(1, 12, 0x2e); put(1, 13, 0x2a); put(1, 14, 0x4c);

    put(2, 0, 0x2b); put(2, 1, 0x14); put(2, 2, 0x1a);
    put(2, 3, 0x08); put(2, 4, 0x15); put(2, 5, 0x17);
    put(2, 6, 0x1c); put(2, 7, 0x18); put(2, 8, 0x0c);
    put(2, 9, 0x12); put(2, 10, 0x13); put(2, 11, 0x2f);
    put(2, 12, 0x30); put(2, 13, 0x31); put(2, 14, 0x4d);

    put(3, 0, 0x39); put(3, 1, 0x04); put(3, 2, 0x16);
    put(3, 3, 0x07); put(3, 4, 0x09); put(3, 5, 0x0a);
    put(3, 6, 0x0b); put(3, 7, 0x0d); put(3, 8, 0x0e);
    put(3, 9, 0x0f); put(3, 10, 0x33); put(3, 11, 0x34);
    put(3, 13, 0x28); put(3, 14, 0x4b);

    put(4, 0, 0xe1); put(4, 2, 0x1d); put(4, 3, 0x1b);
    put(4, 4, 0x06); put(4, 5, 0x19); put(4, 6, 0x05);
    put(4, 7, 0x11); put(4, 8, 0x10); put(4, 9, 0x36);
    put(4, 10, 0x37); put(4, 11, 0x38); put(4, 12, 0xe5);
    put(4, 13, 0x52); put(4, 14, 0x4e);

    put(5, 0, 0xe0); put(5, 1, 0xe3); put(5, 2, 0xe2);
    put(5, 6, 0x2c); put(5, 9, 0xe6);
    put(5, 10, halljoy::keycode::kFn);
    put(5, 11, halljoy::keycode::kOem1); // physical Menu/Fn2
    put(5, 12, 0x50); put(5, 13, 0x51); put(5, 14, 0x4f);
    return map;
}

PositionToHid MapForProduct(std::uint16_t productId) noexcept
{
    return productId == 0x2382 ? G65AnsiMap() : GenericUapMap();
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
        [](std::uint16_t code) { return code != 0; }));
}
}

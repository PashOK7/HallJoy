#pragma once

#include <array>
#include <algorithm>
#include <cstddef>
#include <cstdint>

#include "analog_key_codes.h"

// Red Square Alumix 68 (RSQ-20058, 0C45:80A2) analog depth, decoded offline from
// the official RSQ-20058 v1.30 firmware image
// (SHA256 020778ba842603c49fe21365ad10380dd877f9be29ecaeefb9760eeab69461c1) with
// tools/review_redsquare_alumix68_firmware.py and ..._paths.py. See
// docs/current/ALUMIX68_RSQ20058_2026-10-06.md.
//
// The best reviewed analog path reads the current per-slot key-depth table from
// RAM with the ordinary vendor read command, without enabling calibration or the
// selected-key simulation stream: independent multi-key depth, no held-key loss.
// Because that table's RAM offset is specific to v1.30 internals, the backend
// must verify an exact firmware fingerprint over HID before reading it and must
// refuse to connect on any other firmware (owner decision 2026-10-06).
namespace halljoy::redsquare_alumix68 {

inline constexpr std::uint16_t kVendorId = 0x0c45;
inline constexpr std::uint16_t kProductId = 0x80a2;

// Windows HID interrupt reports carry a leading report-id byte; the device frame
// (the emulator's view) is 64 bytes. The sibling Alumix 104 exposes the vendor
// transport on collection FF68:61 with 65/65-byte reports; the same holds here.
inline constexpr std::size_t kReportBytes = 65;    // Windows buffer: report id + 64
inline constexpr std::size_t kPayloadBytes = 64;   // device-side frame

// Vendor transport: 0xAA host request -> 0x55 device reply (SONiX family, as 104).
inline constexpr std::uint8_t kRequestPrefix = 0xaa;
inline constexpr std::uint8_t kReplyPrefix = 0x55;
inline constexpr std::uint8_t kCmdReadRam = 0x16;    // reads RAM 0x20002A9C + offset
inline constexpr std::uint8_t kCmdReadFlash = 0x12;  // reads flash 0x9600 + offset

// Depth table: 128 LE16 scan-slot strokes at RAM 0x20002C9C (0x20002A9C + 0x200).
inline constexpr std::uint16_t kDepthOffset = 0x200;
inline constexpr std::size_t kSlots = 128;
inline constexpr std::size_t kDepthBytes = kSlots * 2;  // 256 bytes
// A 64-byte frame carries 8 header bytes, so at most 56 payload bytes per read.
inline constexpr std::size_t kMaxChunk = 56;

// Full travel 3.3 mm. The official configurator shows keyStroke/100 mm, so one
// stroke unit is 0.01 mm and full scale is 330. There is no per-key maximum in
// this table; HallJoy's curve and deadzones handle the rest.
inline constexpr std::uint16_t kFullScaleStroke = 330;

// scan slot -> HID usage, from the firmware scan map (flash 0x159D2) joined with
// the official RSQ-20058 keyList value->keyCode. 68 of the 128 slots are mapped;
// the sentinel 125 and unused slots are 0. The configurator's Fn key (keyList
// keyCode 175, key "-1") has no USB usage of its own and maps to HallJoy's kFn.
inline constexpr std::array<std::uint16_t, kSlots> kSlotHid{
    0, 41, 43, 26, 225, 224, 0, 0, 0, 30, 20,
    4, 29, 227, 0, 0, 0, 31, 57, 22, 27, 226,
    0, 0, 0, 32, 8, 7, 6, 0, 0, 0, 0,
    33, 21, 9, 25, 0, 0, 0, 0, 34, 23, 10,
    5, 44, 0, 0, 0, 35, 28, 11, 17, 0, 0,
    0, 0, 36, 24, 13, 16, 0, 0, 0, 0, 37,
    12, 14, 54, 0, 0, 0, 0, 38, 18, 15, 55,
    230, 0, 0, 0, 39, 19, 51, 56, halljoy::keycode::kFn, 0, 0,
    0, 45, 47, 52, 0, 228, 0, 0, 0, 46, 48,
    0, 229, 80, 0, 0, 0, 42, 49, 40, 82, 81,
    0, 0, 0, 73, 76, 75, 78, 79, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0,
};
inline constexpr std::size_t kMappedSlots = 68;

// Firmware v1.30 fingerprint, read over HID with command 0x12 before any depth
// read. The scan map (immutable layout table) alone does not prove the version,
// so the command dispatcher window is checked too: a firmware rebuild that could
// move the depth table changes this code and the gate fails closed.
inline constexpr std::uint16_t kScanMapOffset = 0xc3d2;   // flash 0x159D2
inline constexpr std::array<std::uint8_t, kSlots> kScanMap{
    125, 0, 32, 34, 64, 80, 125, 125, 125, 17, 33, 49, 65, 81, 125, 125,
    125, 18, 48, 50, 66, 82, 125, 125, 125, 19, 35, 51, 67, 125, 125, 125,
    125, 20, 36, 52, 68, 125, 125, 125, 125, 21, 37, 53, 69, 83, 125, 125,
    125, 22, 38, 54, 70, 125, 125, 125, 125, 23, 39, 55, 71, 125, 125, 125,
    125, 24, 40, 56, 72, 125, 125, 125, 125, 25, 41, 57, 73, 84, 125, 125,
    125, 26, 42, 58, 74, 85, 125, 125, 125, 27, 43, 59, 125, 87, 125, 125,
    125, 28, 44, 125, 75, 88, 125, 125, 125, 92, 60, 76, 90, 89, 125, 125,
    125, 103, 106, 105, 108, 91, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125,
};
inline constexpr std::uint16_t kCodeFpOffset = 0x4e00;    // flash 0xE400
inline constexpr std::array<std::uint8_t, 56> kCodeFp{
    0xf0, 0xfb, 0x01, 0x40, 0x05, 0x00, 0x00, 0x20, 0x64, 0x00, 0x00, 0x20, 0x2d, 0xe9,
    0xf0, 0x5f, 0xfd, 0x4c, 0x20, 0x79, 0x00, 0x28, 0x71, 0xd0, 0x4f, 0xf0, 0x00, 0x0b,
    0x84, 0xf8, 0x04, 0xb0, 0xfa, 0x4d, 0x28, 0x78, 0xaa, 0x28, 0x6a, 0xd1, 0xf9, 0x48,
    0x01, 0x78, 0x01, 0x29, 0x08, 0xd1, 0x69, 0x78, 0x32, 0x29, 0x05, 0xd0, 0x33, 0x29,
};

inline bool ExactCollection(unsigned page, unsigned usage,
                            unsigned inputBytes, unsigned outputBytes) noexcept {
    return page == 0xff68 && usage == 0x61 &&
           inputBytes == kReportBytes && outputBytes == kReportBytes;
}

inline std::uint16_t ToMilli(std::uint16_t stroke) noexcept {
    const std::uint32_t scaled = std::uint32_t(stroke) * 1000u / kFullScaleStroke;
    return static_cast<std::uint16_t>(scaled > 1000u ? 1000u : scaled);
}

// Build a Windows output report requesting `length` bytes at `offset`.
inline bool MakeReadRequest(std::uint8_t command, std::uint8_t length,
                            std::uint16_t offset,
                            std::array<std::uint8_t, kReportBytes>& out) noexcept {
    if (!length || length > kMaxChunk) return false;
    std::array<std::uint8_t, kReportBytes> frame{};
    frame[0] = 0;  // Windows report id
    frame[1] = kRequestPrefix;
    frame[2] = command;
    frame[3] = length;
    frame[4] = static_cast<std::uint8_t>(offset);
    frame[5] = static_cast<std::uint8_t>(offset >> 8);
    frame[7] = 1;  // the vendor one-packet query marks its final packet
    out = frame;
    return true;
}

// Validate a Windows input report against the request and return the payload.
// The payload sits at report[9] (device byte 8) and spans `length` bytes.
inline const std::uint8_t* ParseReadReply(std::uint8_t command, std::uint8_t length,
                                          std::uint16_t offset,
                                          const std::uint8_t* report,
                                          std::size_t bytes) noexcept {
    if (!report || !length || length > kMaxChunk) return nullptr;
    if (bytes < 9u + length) return nullptr;
    if (report[0] != 0 || report[1] != kReplyPrefix || report[2] != command ||
        report[3] != length || report[4] != static_cast<std::uint8_t>(offset) ||
        report[5] != static_cast<std::uint8_t>(offset >> 8)) return nullptr;
    return report + 9;
}

// Read-chunk plan over the 256-byte depth table (5 reads: 56*4 + 32).
struct Chunk { std::uint16_t offset; std::uint8_t length; std::uint8_t firstSlot; };
inline constexpr std::array<Chunk, 5> kDepthChunks{{
    {kDepthOffset + 0, 56, 0},
    {kDepthOffset + 56, 56, 28},
    {kDepthOffset + 112, 56, 56},
    {kDepthOffset + 168, 56, 84},
    {kDepthOffset + 224, 32, 112},
}};

inline std::uint16_t SlotStroke(const std::uint8_t* payload, std::size_t index) noexcept {
    return static_cast<std::uint16_t>(payload[index * 2] | (payload[index * 2 + 1] << 8));
}

inline bool SelfTest() noexcept {
    // 68 mapped slots, all supported, no duplicates, Fn uses the extended code.
    unsigned mapped = 0;
    for (std::size_t slot = 0; slot < kSlotHid.size(); ++slot) {
        const auto hid = kSlotHid[slot];
        if (!hid) continue;
        ++mapped;
        if (!halljoy::keycode::IsSupported(hid)) return false;
    }
    if (mapped != kMappedSlots) return false;
    if (kSlotHid[85] != halljoy::keycode::kFn) return false;
    if (kSlotHid[1] != 41 || kSlotHid[117] != 79) return false;

    // The baked slot->HID must agree with the scan map joined to the keyList:
    // every mapped slot has a non-sentinel scan value, every sentinel is unmapped.
    for (std::size_t slot = 0; slot < kSlots; ++slot) {
        const bool sentinel = kScanMap[slot] == 125;
        if (sentinel && kSlotHid[slot]) return false;
        if (!sentinel && !kSlotHid[slot]) return false;
    }
    unsigned scanMapped = 0;
    for (auto value : kScanMap) if (value != 125) ++scanMapped;
    if (scanMapped != kMappedSlots) return false;

    if (!ExactCollection(0xff68, 0x61, 65, 65) ||
        ExactCollection(0xff68, 0x61, 33, 33) ||
        ExactCollection(0xff67, 0x61, 65, 65)) return false;

    if (ToMilli(0) != 0 || ToMilli(330) != 1000 || ToMilli(165) != 500 ||
        ToMilli(33) != 100 || ToMilli(340) != 1000 || ToMilli(1000) != 1000)
        return false;

    // Request/reply round trip on a 65-byte Windows buffer.
    std::array<std::uint8_t, kReportBytes> request{};
    if (MakeReadRequest(kCmdReadRam, 0, kDepthOffset, request)) return false;
    if (MakeReadRequest(kCmdReadRam, 57, kDepthOffset, request)) return false;
    if (!MakeReadRequest(kCmdReadRam, 56, kDepthOffset, request)) return false;
    if (request[1] != kRequestPrefix || request[2] != kCmdReadRam ||
        request[3] != 56 || request[4] != 0x00 || request[5] != 0x02 ||
        request[7] != 1) return false;

    std::array<std::uint8_t, kReportBytes> reply{};
    reply[0] = 0; reply[1] = kReplyPrefix; reply[2] = kCmdReadRam;
    reply[3] = 56; reply[4] = 0x00; reply[5] = 0x02;
    // slot 0 = 93, slot 1 = 170, slot 2 = 0 (release) in the payload at reply[9].
    reply[9] = 93; reply[11] = 170;
    const auto* payload = ParseReadReply(kCmdReadRam, 56, kDepthOffset,
                                         reply.data(), reply.size());
    if (!payload) return false;
    if (SlotStroke(payload, 0) != 93 || SlotStroke(payload, 1) != 170 ||
        SlotStroke(payload, 2) != 0) return false;
    // A wrong offset, command or truncated frame must be rejected.
    if (ParseReadReply(kCmdReadRam, 56, 0x0238, reply.data(), reply.size())) return false;
    if (ParseReadReply(kCmdReadFlash, 56, kDepthOffset, reply.data(), reply.size())) return false;
    if (ParseReadReply(kCmdReadRam, 56, kDepthOffset, reply.data(), 40)) return false;

    // The chunk plan covers all 128 slots contiguously.
    std::size_t covered = 0;
    for (const auto& chunk : kDepthChunks) {
        if (chunk.firstSlot * 2u + kDepthOffset != chunk.offset) return false;
        covered += chunk.length / 2u;
    }
    return covered == kSlots;
}

}  // namespace halljoy::redsquare_alumix68

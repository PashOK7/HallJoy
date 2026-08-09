#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace mad68pr
{
constexpr std::uint16_t kVid = 0x373B;
constexpr std::uint16_t kPid = 0x1109;
constexpr std::uint16_t kAuditedBcdDevice = 0x0102;
constexpr std::size_t kPayloadBytes = 64;
constexpr std::size_t kPhysicalKeyCount = 68;
constexpr std::size_t kPublishedKeyCount = 67;
constexpr std::uint16_t kAnalogFullScale = 1600;
// A post-sweep threshold crossing is strong evidence that the firmware has
// entered its change-driven steady branch. Keep the minimum delta above idle
// Hall noise observed in hardware logs.
constexpr std::uint16_t kSteadyProofMinDelta = 64;
constexpr std::uint8_t kNormalRequestHeader = 0x55;
constexpr std::uint8_t kRawRequestHeader = 0x5F;
constexpr std::uint8_t kNormalResponseHeader = 0xAA;
constexpr std::uint8_t kChecksumErrorHeader = 0xAB;
constexpr std::uint8_t kStreamHeader = 0xA0;
constexpr std::uint8_t kArmOpcode = 0xA8;
constexpr std::uint8_t kRestoreInputOpcode = 0xA9;

struct KeyDescriptor
{
    std::uint8_t scannerSlot = 0;
    std::uint8_t internalId = 0;
    std::uint16_t hid = 0;
    const char* name = "?";
    std::array<std::uint8_t, 3> bytes{};
};

constexpr std::array<KeyDescriptor, kPhysicalKeyCount> kKeyDescriptors = {{
    {  0, 16, 0x14, "Q", { 0x10, 0x00, 0x14 } },
    {  1, 18, 0x08, "E", { 0x10, 0x00, 0x08 } },
    {  2, 21, 0x1C, "Y", { 0x10, 0x00, 0x1C } },
    {  3, 11, 0x2D, "-", { 0x10, 0x00, 0x2D } },
    {  4, 29, 0x4C, "Delete", { 0x10, 0x00, 0x4C } },
    {  5, 48, 0x19, "V", { 0x10, 0x00, 0x19 } },
    {  6, 53, 0x37, "Period", { 0x10, 0x00, 0x37 } },
    {  7, 63, 0x00, "Fn", { 0xF0, 0xFF, 0x01 } },
    {  9, 31, 0x04, "A", { 0x10, 0x00, 0x04 } },
    { 10, 33, 0x07, "D", { 0x10, 0x00, 0x07 } },
    { 11, 35, 0x0A, "G", { 0x10, 0x00, 0x0A } },
    { 12, 24, 0x12, "O", { 0x10, 0x00, 0x12 } },
    { 13, 28, 0x31, "Backslash", { 0x10, 0x00, 0x31 } },
    { 14, 47, 0x06, "C", { 0x10, 0x00, 0x06 } },
    { 15, 52, 0x36, "Comma", { 0x10, 0x00, 0x36 } },
    { 17, 67, 0x4F, "Right", { 0x10, 0x00, 0x4F } },
    { 18, 45, 0x1D, "Z", { 0x10, 0x00, 0x1D } },
    { 19, 19, 0x15, "R", { 0x10, 0x00, 0x15 } },
    { 20, 36, 0x0B, "H", { 0x10, 0x00, 0x0B } },
    { 21, 25, 0x13, "P", { 0x10, 0x00, 0x13 } },
    { 22, 43, 0x4B, "PageUp", { 0x10, 0x00, 0x4B } },
    { 23, 46, 0x1B, "X", { 0x10, 0x00, 0x1B } },
    { 24, 38, 0x0E, "K", { 0x10, 0x00, 0x0E } },
    { 25, 55, 0xE5, "RightShift", { 0x10, 0x20, 0x00 } },
    { 26, 57, 0x4E, "PageDown", { 0x10, 0x00, 0x4E } },
    { 27,  1, 0x1E, "1", { 0x10, 0x00, 0x1E } },
    { 28,  4, 0x21, "4", { 0x10, 0x00, 0x21 } },
    { 29,  7, 0x24, "7", { 0x10, 0x00, 0x24 } },
    { 30, 10, 0x27, "0", { 0x10, 0x00, 0x27 } },
    { 31, 14, 0x49, "Insert", { 0x10, 0x00, 0x49 } },
    { 32, 49, 0x05, "B", { 0x10, 0x00, 0x05 } },
    { 33, 62, 0xE6, "RightAlt", { 0x10, 0x40, 0x00 } },
    { 34, 64, 0xE4, "RightCtrl", { 0x10, 0x10, 0x00 } },
    { 36, 44, 0xE1, "LeftShift", { 0x10, 0x02, 0x00 } },
    { 37,  3, 0x20, "3", { 0x10, 0x00, 0x20 } },
    { 38, 20, 0x17, "T", { 0x10, 0x00, 0x17 } },
    { 39, 23, 0x0C, "I", { 0x10, 0x00, 0x0C } },
    { 40, 13, 0x2A, "Backspace", { 0x10, 0x00, 0x2A } },
    { 41, 58, 0xE0, "LeftCtrl", { 0x10, 0x01, 0x00 } },
    { 42, 37, 0x0D, "J", { 0x10, 0x00, 0x0D } },
    { 43, 39, 0x0F, "L", { 0x10, 0x00, 0x0F } },
    { 44, 42, 0x28, "Enter", { 0x10, 0x00, 0x28 } },
    { 45,  0, 0x29, "Esc", { 0x10, 0x00, 0x29 } },
    { 46,  2, 0x1F, "2", { 0x10, 0x00, 0x1F } },
    { 47,  5, 0x22, "5", { 0x10, 0x00, 0x22 } },
    { 48,  8, 0x25, "8", { 0x10, 0x00, 0x25 } },
    { 49, 12, 0x2E, "=", { 0x10, 0x00, 0x2E } },
    { 50, 60, 0xE2, "LeftAlt", { 0x10, 0x04, 0x00 } },
    { 51, 51, 0x10, "M", { 0x10, 0x00, 0x10 } },
    { 52, 54, 0x38, "Slash", { 0x10, 0x00, 0x38 } },
    { 53, 65, 0x50, "Left", { 0x10, 0x00, 0x50 } },
    { 54, 30, 0x39, "CapsLock", { 0x10, 0x00, 0x39 } },
    { 55, 32, 0x16, "S", { 0x10, 0x00, 0x16 } },
    { 56, 34, 0x09, "F", { 0x10, 0x00, 0x09 } },
    { 57, 22, 0x18, "U", { 0x10, 0x00, 0x18 } },
    { 58, 26, 0x2F, "[", { 0x10, 0x00, 0x2F } },
    { 60, 50, 0x11, "N", { 0x10, 0x00, 0x11 } },
    { 61, 40, 0x33, "Semicolon", { 0x10, 0x00, 0x33 } },
    { 62, 56, 0x52, "Up", { 0x10, 0x00, 0x52 } },
    { 63, 15, 0x2B, "Tab", { 0x10, 0x00, 0x2B } },
    { 64, 17, 0x1A, "W", { 0x10, 0x00, 0x1A } },
    { 65,  6, 0x23, "6", { 0x10, 0x00, 0x23 } },
    { 66,  9, 0x26, "9", { 0x10, 0x00, 0x26 } },
    { 67, 27, 0x30, "]", { 0x10, 0x00, 0x30 } },
    { 68, 59, 0xE3, "LeftGUI", { 0x10, 0x08, 0x00 } },
    { 69, 61, 0x2C, "Space", { 0x10, 0x00, 0x2C } },
    { 70, 41, 0x34, "Apostrophe", { 0x10, 0x00, 0x34 } },
    { 71, 66, 0x51, "Down", { 0x10, 0x00, 0x51 } },
}};

struct KeySample
{
    std::size_t keyIndex = 0;
    std::uint8_t scannerSlot = 0;
    std::uint8_t internalId = 0;
    std::uint16_t hid = 0;
    std::uint16_t raw = 0;
    std::uint16_t milli = 0;
    std::uint16_t threshold = 0;
    std::uint16_t baseline = 0;
    std::uint8_t state = 0;
};

enum class ControlResponseKind
{
    NotControl,
    Valid,
    ChecksumError,
    Invalid,
};

struct ControlResponse
{
    ControlResponseKind kind = ControlResponseKind::NotControl;
    std::uint8_t header = 0;
    std::uint8_t opcode = 0;
    std::uint8_t xorKey = 0;
    std::uint8_t length = 0;
    std::uint8_t checksum = 0;
    std::uint8_t expectedChecksum = 0;
};

std::array<std::uint8_t, kPayloadBytes> MakeZeroPayloadRequest(
    std::uint8_t opcode,
    std::uint8_t framingHeader = kNormalRequestHeader,
    std::uint8_t xorKey = 0) noexcept;

bool DecodeKeySample(const std::uint8_t* payload, std::size_t bytes, KeySample& out) noexcept;
ControlResponse DecodeControlResponse(
    const std::uint8_t* payload,
    std::size_t bytes,
    std::uint8_t expectedRequestHeader,
    std::uint8_t expectedOpcode) noexcept;

int KeyIndexFromHid(std::uint16_t hid) noexcept;
int KeyIndexFromDescriptor(const std::uint8_t* descriptor3) noexcept;
const char* KeyName(std::size_t index) noexcept;
bool IsPublishedHid(std::uint16_t hid) noexcept;
bool IsWasdHid(std::uint16_t hid) noexcept;
bool AnalogTransitionMatchesDigital(
    bool expectedDown,
    std::uint16_t threshold,
    std::uint16_t rawAtEvent,
    std::uint16_t currentRaw) noexcept;
// Raw Input is useful for per-key freshness arbitration, but some Windows/HID
// stacks do not deliver target keyboard edges for this device. This portable
// predicate recognises only a meaningful actuation-threshold crossing, so the
// native backend can still prove post-sweep steady state from A0 itself.
bool IsPostSweepAnalogProof(
    std::uint16_t previousRaw,
    std::uint16_t currentRaw,
    std::uint16_t threshold) noexcept;
} // namespace mad68pr

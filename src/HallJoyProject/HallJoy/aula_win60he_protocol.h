#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace aula_win60he
{
// Exact USB/HID identity embedded in Aula WIN 60 HE MAX
// App V1.1.6, build date "Feb  4 2026".
constexpr std::uint16_t kAulaVendorId = 0x1CA2;
constexpr std::uint16_t kAulaProductId = 0x1902;
constexpr std::uint16_t kAulaUsagePage = 0xFFA0;
constexpr std::uint16_t kAulaUsage = 0x0001;

// Exact USB/board identities whose SparkPlayJoy 6x21 implementation is backed
// by physical evidence (Aula) or static firmware proof (GravaStar). This table
// is only a bounded admission gate. Every candidate must still pass the full
// read-only sync/scale/default-map/active-map/travel proof before its exact HID
// path is claimed or any analogue value is published.
struct KnownUsbIdentity
{
    std::uint16_t vendorId = 0;
    std::uint16_t productId = 0;
    std::uint32_t boardId = 0;
};

constexpr std::array<KnownUsbIdentity, 4> kKnownUsbIdentities{{
    { 0x1CA2u, 0x1902u, 0x0A021902u }, // Aula WIN 60 HE MAX
    { 0x1CA5u, 0x2201u, 0x16052201u }, // GravaStar Mercury V75
    { 0x1CA5u, 0x2202u, 0x16052202u }, // GravaStar Mercury V75 Pro
    { 0x1CA2u, 0x2201u, 0x2E022201u }, // GravaStar Mercury V75 Lite
}};

constexpr const KnownUsbIdentity* FindKnownUsbIdentity(
    std::uint16_t vendorId,
    std::uint16_t productId) noexcept
{
    for (const auto& identity : kKnownUsbIdentities)
        if (identity.vendorId == vendorId && identity.productId == productId)
            return &identity;
    return nullptr;
}

constexpr bool IsKnownUsbIdentity(
    std::uint16_t vendorId,
    std::uint16_t productId) noexcept
{
    return FindKnownUsbIdentity(vendorId, productId) != nullptr;
}

constexpr bool IsKnownUsbIdentityBoardCompatible(
    std::uint16_t vendorId,
    std::uint16_t productId,
    std::uint32_t boardId) noexcept
{
    const auto* identity = FindKnownUsbIdentity(vendorId, productId);
    return !identity || identity->boardId == boardId;
}

constexpr wchar_t FoldUsbPathAscii(wchar_t value) noexcept
{
    return value >= L'A' && value <= L'Z'
        ? static_cast<wchar_t>(value - L'A' + L'a')
        : value;
}

constexpr bool UsbPathTokenAt(
    std::wstring_view path,
    std::size_t offset,
    std::wstring_view token) noexcept
{
    if (offset > path.size() || token.size() > path.size() - offset)
        return false;
    for (std::size_t index = 0; index < token.size(); ++index)
        if (FoldUsbPathAscii(path[offset + index]) != token[index])
            return false;
    return true;
}

constexpr int UsbPathHexNibble(wchar_t value) noexcept
{
    value = FoldUsbPathAscii(value);
    if (value >= L'0' && value <= L'9') return value - L'0';
    if (value >= L'a' && value <= L'f') return value - L'a' + 10;
    return -1;
}

constexpr bool ReadUsbPathHex4(
    std::wstring_view path,
    std::size_t offset,
    std::uint16_t* out) noexcept
{
    if (out) *out = 0;
    if (!out || offset > path.size() || 4u > path.size() - offset)
        return false;
    std::uint16_t value = 0;
    for (std::size_t index = 0; index < 4u; ++index)
    {
        const int nibble = UsbPathHexNibble(path[offset + index]);
        if (nibble < 0)
            return false;
        value = static_cast<std::uint16_t>((value << 4u) |
            static_cast<std::uint16_t>(nibble));
    }
    *out = value;
    return true;
}

constexpr bool TryReadUsbIdentityFromPath(
    std::wstring_view path,
    std::uint16_t* outVendorId,
    std::uint16_t* outProductId) noexcept
{
    if (outVendorId) *outVendorId = 0;
    if (outProductId) *outProductId = 0;
    if (!outVendorId || !outProductId)
        return false;
    constexpr std::size_t kVidPidTokenBytes = 17u;
    if (path.size() < kVidPidTokenBytes)
        return false;
    for (std::size_t offset = 0;
         offset <= path.size() - kVidPidTokenBytes;
         ++offset)
    {
        if (!UsbPathTokenAt(path, offset, L"vid_") ||
            !UsbPathTokenAt(path, offset + 8u, L"&pid_"))
            continue;
        std::uint16_t vendorId = 0;
        std::uint16_t productId = 0;
        if (!ReadUsbPathHex4(path, offset + 4u, &vendorId) ||
            !ReadUsbPathHex4(path, offset + 13u, &productId))
            continue;
        *outVendorId = vendorId;
        *outProductId = productId;
        return true;
    }
    return false;
}

constexpr bool PathContainsKnownUsbIdentity(std::wstring_view path) noexcept
{
    std::uint16_t vendorId = 0;
    std::uint16_t productId = 0;
    return TryReadUsbIdentityFromPath(
        path, &vendorId, &productId) &&
        IsKnownUsbIdentity(vendorId, productId);
}

constexpr std::uint8_t kFrameHead = 0x5C;
constexpr std::size_t kWireReportBytes = 64;
constexpr std::size_t kWindowsHidReportBytes = kWireReportBytes + 1u;
constexpr std::size_t kMaximumResponseReports = 3;
constexpr std::size_t kMaximumResponseBytes =
    kWireReportBytes * kMaximumResponseReports;

constexpr std::size_t kRows = 6;
constexpr std::size_t kColumns = 21;
constexpr std::size_t kMatrixPositions = kRows * kColumns;
constexpr std::size_t kRowsPerTravelHalf = 3;
constexpr std::size_t kTravelValuesPerHalf =
    kRowsPerTravelHalf * kColumns;
constexpr std::size_t kTravelPayloadBytes =
    2u + kTravelValuesPerHalf * 2u;
constexpr std::size_t kStatusPayloadBytes =
    2u + kRows * kColumns + 2u;
// Exact physical response contract, repeated across three complete exclusive
// proofs in HallJoy (3).log. The former 54-byte oracle shape was inferred
// without hardware and is deliberately no longer accepted.
constexpr std::size_t kSyncPayloadBytes = 60u;
constexpr std::size_t kSyncDescriptorBytes = 16u;
constexpr std::size_t kSyncSerialLengthOffset = 8u;
constexpr std::size_t kSyncSerialOffset = 9u;
constexpr std::size_t kSyncSerialBytes = kSyncDescriptorBytes;
constexpr std::size_t kSyncAppLengthOffset = 25u;
constexpr std::size_t kSyncAppVersionOffset = 26u;
constexpr std::size_t kSyncAppVersionBytes = 10u;
constexpr std::size_t kSyncAppDescriptorOffset = 26u;
constexpr std::size_t kSyncBuildLengthOffset = 42u;
constexpr std::size_t kSyncBuildDescriptorOffset = 43u;
constexpr std::size_t kSyncBuildLabelBytes = 7u;
constexpr std::size_t kSyncTrailerOffset = 59u;
constexpr std::size_t kDefaultKeyPayloadBytes = 45u;
static_assert(kSyncSerialOffset + kSyncDescriptorBytes ==
    kSyncAppLengthOffset);
static_assert(kSyncAppDescriptorOffset + kSyncDescriptorBytes ==
    kSyncBuildLengthOffset);
static_assert(kSyncBuildDescriptorOffset + kSyncDescriptorBytes ==
    kSyncTrailerOffset);
static_assert(kSyncTrailerOffset + 1u == kSyncPayloadBytes);
static_assert(kTravelPayloadBytes == 128u);
static_assert(kStatusPayloadBytes == 130u);

constexpr std::uint8_t kCommandApi = 0x00;
constexpr std::uint8_t kCommandSync = 0x01;
constexpr std::uint8_t kCommandMatrix6x21 = 0x12;
constexpr std::uint8_t kCommandKeyFunctions = 0x23;
constexpr std::uint8_t kCommandDefaultKeys = 0x2B;
constexpr std::uint8_t kReadOperation = 0x00;
constexpr std::uint8_t kOrderPrecisionStroke = 0x25;
constexpr std::uint8_t kSelectorTravel = 0x02;
constexpr std::uint8_t kSelectorStatus = 0x03;
constexpr std::uint8_t kKeyLayoutFn0 = 0x00;

// HallJoy deliberately sends the maximum fourteen records on every read. This
// yields one exact request/response shape and lets the final short batch be
// padded with a repeated, correlation-checked factory key.
constexpr std::size_t kKeyFunctionRecordsPerFrame = 14u;
constexpr std::size_t kKeyFunctionPayloadBytes =
    1u + kKeyFunctionRecordsPerFrame * 4u;
static_assert(kKeyFunctionPayloadBytes == 57u);

constexpr std::uint16_t kExpectedPrecisionUm = 10u;
constexpr std::uint16_t kExpectedMinimumTravelUm = 10u;
constexpr std::uint16_t kExpectedMaximumTravelUm = 3400u;
constexpr std::size_t kExpectedPhysicalKeyPositions = 61u;
constexpr std::size_t kExpectedPublishableDefaultKeys = 60u;
constexpr std::size_t kExactKeyFunctionBatchCount =
    (kExpectedPhysicalKeyPositions + kKeyFunctionRecordsPerFrame - 1u) /
    kKeyFunctionRecordsPerFrame;
constexpr std::size_t kMaximumKeyFunctionBatchCount =
    (kMatrixPositions + kKeyFunctionRecordsPerFrame - 1u) /
    kKeyFunctionRecordsPerFrame;
static_assert(kExactKeyFunctionBatchCount == 5u);
static_assert(kMaximumKeyFunctionBatchCount == 9u);

enum class CompatibilityProfile : std::uint8_t
{
    ExactWin60HeMax = 0,
    ExactKnownBoard6x21Family,
    Compatible6x21Family,
};

// USB discovery owns the model table; the protocol client owns the wire proof.
// Passing the expected board explicitly keeps those trust domains joined for a
// known model without teaching the client about Windows HID paths or brands.
struct CompatibilityPolicy
{
    CompatibilityProfile profile = CompatibilityProfile::ExactWin60HeMax;
    std::uint32_t expectedBoardId = 0;
};

using Report = std::array<std::uint8_t, kWireReportBytes>;
using HidWireReport = std::array<std::uint8_t, kWindowsHidReportBytes>;
using ResponseStream = std::array<std::uint8_t, kMaximumResponseBytes>;
using KeyMap = std::array<std::array<std::uint8_t, kColumns>, kRows>;
using ActiveKeyMap =
    std::array<std::array<std::uint16_t, kColumns>, kRows>;
using KeyFunctionMap =
    std::array<std::array<std::uint16_t, kColumns>, kRows>;
using TravelHalf =
    std::array<std::array<std::uint16_t, kColumns>, kRowsPerTravelHalf>;
using TravelMatrix =
    std::array<std::array<std::uint16_t, kColumns>, kRows>;
using StatusMatrix =
    std::array<std::array<std::uint8_t, kColumns>, kRows>;
using KeyQuery =
    std::array<std::uint8_t, kKeyFunctionRecordsPerFrame>;

struct FrameView
{
    std::uint8_t command = 0;
    const std::uint8_t* payload = nullptr;
    std::size_t payloadBytes = 0;
};

struct SyncInfo
{
    std::uint32_t boardId = 0;
    std::array<std::uint8_t, kSyncPayloadBytes> rawPayload{};
    std::array<char, kSyncSerialBytes + 1u> serial{};
    std::array<char, kSyncAppVersionBytes + 1u> appVersion{};
    std::array<char, kSyncBuildLabelBytes + 1u> buildLabel{};
};

struct PrecisionStroke
{
    std::uint16_t precisionUm = 0;
    std::uint16_t minimumTravelUm = 0;
    std::uint16_t maximumTravelUm = 0;
};

struct KeyFunctionRecord
{
    std::uint8_t key = 0;
    std::uint8_t layout = 0;
    std::uint16_t value = 0;
};
using KeyFunctionBatch =
    std::array<KeyFunctionRecord, kKeyFunctionRecordsPerFrame>;

constexpr std::uint8_t ResponseCommand(
    std::uint8_t requestCommand) noexcept
{
    return static_cast<std::uint8_t>(requestCommand | 0x80u);
}

std::uint8_t ComputeChecksum(
    std::uint8_t payloadLength,
    std::uint8_t command,
    const std::uint8_t* payload) noexcept;
Report BuildFrame(
    std::uint8_t command,
    const std::uint8_t* payload,
    std::size_t payloadLength) noexcept;

Report BuildSyncRequest() noexcept;
Report BuildPrecisionStrokeRequest() noexcept;
Report BuildDefaultKeyRequest(
    std::uint8_t firstRow,
    std::uint8_t secondRow) noexcept;
Report BuildKeyFunctionReadRequest(
    const KeyQuery& factoryKeys,
    std::uint8_t layout) noexcept;
Report BuildTravelRequest(std::uint8_t half) noexcept;
Report BuildStatusRequest() noexcept;

// Windows HIDP_CAPS lengths include a leading Report-ID byte. This firmware
// declares no Report ID, so Win32 byte zero must be 0 and 64 protocol bytes
// follow. Only the exact 65-byte Win32 envelope is accepted.
bool EncodeHidReport(
    const Report& protocol,
    std::size_t reportBytes,
    HidWireReport* out,
    std::size_t* outBytes) noexcept;
bool DecodeHidReport(
    const std::uint8_t* wire,
    std::size_t reportBytes,
    Report* out) noexcept;
bool FlattenHidReports(
    const std::uint8_t* reports,
    std::size_t reportCount,
    std::size_t reportBytes,
    ResponseStream* out,
    std::size_t* outBytes) noexcept;

bool ParseResponseFrame(
    const std::uint8_t* stream,
    std::size_t streamBytes,
    std::uint8_t requestCommand,
    FrameView* out) noexcept;
std::size_t ResponseReportCount(std::size_t payloadBytes) noexcept;

bool DecodeSyncInfo(const FrameView& frame, SyncInfo* out) noexcept;
bool DecodePrecisionStroke(
    const FrameView& frame,
    PrecisionStroke* out) noexcept;
bool DecodeDefaultKeyRows(
    const FrameView& frame,
    std::uint8_t expectedFirstRow,
    std::uint8_t expectedSecondRow,
    KeyMap* inOutMap) noexcept;
bool DecodeKeyFunctionReadResponse(
    const FrameView& frame,
    const KeyQuery& expectedFactoryKeys,
    std::uint8_t expectedLayout,
    KeyFunctionBatch* out) noexcept;
bool DecodeTravelHalf(const FrameView& frame, TravelHalf* out) noexcept;
bool DecodeStatusMatrix(const FrameView& frame, StatusMatrix* out) noexcept;
void MergeTravelHalves(
    const TravelHalf& first,
    const TravelHalf& second,
    TravelMatrix* out) noexcept;

const KeyMap& ExpectedAulaWin60HeMaxDefaultMap() noexcept;
bool IsExpectedAulaWin60HeMaxDefaultMap(const KeyMap& map) noexcept;
bool IsExpectedAulaWin60HeMaxPrecision(
    const PrecisionStroke& precision) noexcept;
bool IsExpectedAulaWin60HeMaxFirmware(const SyncInfo& sync) noexcept;
bool IsExactKnownBoard6x21Firmware(
    const SyncInfo& sync,
    std::uint32_t expectedBoardId) noexcept;
bool IsAula6x21FamilyFirmware(const SyncInfo& sync) noexcept;
bool IsAula6x21FamilyPrecision(const PrecisionStroke& precision) noexcept;
bool IsAula6x21FamilyDefaultMap(const KeyMap& map) noexcept;
bool IsPublishableKeyboardUsage(std::uint8_t hidUsage) noexcept;
bool IsPublishableKeyFunction(std::uint16_t function) noexcept;
std::uint16_t PublishedKeyCodeForFunction(std::uint16_t function) noexcept;
std::size_t CountMappedHids(const KeyMap& map) noexcept;
std::size_t CountMappedKeyCodes(const ActiveKeyMap& map) noexcept;
std::size_t CountPhysicalKeyPositions(const KeyMap& map) noexcept;
bool ApplyKeyFunctionBatch(
    const KeyMap& defaultMap,
    const KeyFunctionBatch& batch,
    KeyFunctionMap* inOutFunctions) noexcept;
void BuildPublishableActiveKeyMap(
    const KeyMap& defaultMap,
    const KeyFunctionMap& functions,
    ActiveKeyMap* out) noexcept;
std::uint16_t NormalizeTravelToMilli(
    std::uint16_t travelUm,
    std::uint16_t maximumTravelUm) noexcept;
}

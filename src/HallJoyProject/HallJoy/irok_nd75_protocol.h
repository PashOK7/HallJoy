#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace irok_nd75
{
constexpr std::size_t kReportBytes = 64;
constexpr std::size_t kRows = 6;
constexpr std::size_t kColumns = 22;
constexpr std::size_t kMatrixSlots = kRows * kColumns;
constexpr std::uint8_t kReportId = 1;
constexpr std::uint8_t kIdentityCommand = 0x0d;
constexpr std::uint8_t kHostAnalogCommand = 0x29;
constexpr std::uint8_t kDeviceAnalogCommand = 0x21;
constexpr std::uint8_t kAnalogChannel = 0x18;
constexpr std::uint8_t kNominalTravelMaximum = 40;

using Report = std::array<std::uint8_t, kReportBytes>;
using PositionToHid = std::array<std::uint8_t, kMatrixSlots>;

struct DeviceInfo
{
    std::array<char, 16> controller{};
    std::array<char, 32> product{};
    std::array<char, 24> firmware{};
};

struct CapabilityInfo
{
    std::uint8_t sensitivity = 0;
};

struct LiveEvent
{
    std::uint8_t row = 0;
    std::uint8_t column = 0;
    std::uint8_t travel = 0;
};

enum class ReadFailureAction : std::uint8_t
{
    Continue,
    EndSession,
};

Report BuildIdentityRequest() noexcept;
Report BuildCapabilityRequest() noexcept;
Report BuildSubscriptionRequest(
    const std::array<std::uint8_t, kColumns>& mask) noexcept;
Report BuildUnsubscribeRequest() noexcept;
PositionToHid FactoryMap() noexcept;
std::array<std::uint8_t, kColumns> SubscriptionMask(
    const PositionToHid& map) noexcept;

bool DecodeDeviceInfo(const std::uint8_t* report, std::size_t bytes,
    DeviceInfo* out) noexcept;
bool IsExpectedDevice(const DeviceInfo& info) noexcept;
bool IsFirmwareWithinKnownFamily(const DeviceInfo& info) noexcept;
bool DecodeCapabilityInfo(const std::uint8_t* report, std::size_t bytes,
    CapabilityInfo* out) noexcept;
bool DecodeLiveEvent(const std::uint8_t* report, std::size_t bytes,
    LiveEvent* out) noexcept;
ReadFailureAction ClassifyReadFailure(bool stopping, bool timedOut,
    bool deviceLost, std::uint32_t consecutiveErrors) noexcept;
std::uint16_t ToMilli(std::uint8_t travel) noexcept;
std::size_t MappedKeyCount(const PositionToHid& map) noexcept;
}

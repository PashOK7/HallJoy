#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace aula_w669
{
constexpr std::size_t kReportBytes = 64;
constexpr std::size_t kRows = 6;
constexpr std::size_t kColumns = 22;
constexpr std::size_t kMatrixSlots = kRows * kColumns;
constexpr std::uint8_t kReportId = 1;
constexpr std::uint8_t kDeviceInfoCommand = 0x0d;
constexpr std::uint8_t kAnalogCommand = 0x21;
constexpr std::uint8_t kKeyMapCommand = 0x18;

using Report = std::array<std::uint8_t, kReportBytes>;
using PositionToHid = std::array<std::uint8_t, kMatrixSlots>;

struct TravelInfo
{
    std::uint16_t maximum = 0;
    std::uint8_t unitCode = 0;
    std::uint8_t formatCode = 0;
};

struct LiveEvent
{
    std::uint8_t row = 0;
    std::uint8_t column = 0;
    std::uint16_t travel = 0;
    std::uint8_t declaredLength = 0;
};

constexpr std::size_t kDeviceProductBytes = 32;
struct DeviceInfo
{
    std::array<char, kDeviceProductBytes> product{};
};

enum class FactoryLayoutProfile : std::uint8_t
{
    Unknown = 0,
    Si2825Win60,
    Si2828Win68,
    Si2851KpTe153Uk,
};

Report BuildDeviceInfoRequest() noexcept;
Report BuildTravelInfoRequest() noexcept;
Report BuildKeyMapRequest() noexcept;
Report BuildSubscriptionRequest(const std::array<std::uint8_t, kColumns>& mask) noexcept;
Report BuildSnapshotRequest() noexcept;
Report BuildPollRateQuery() noexcept;
Report BuildUnsubscribeRequest() noexcept;
PositionToHid Win60FactoryMap() noexcept;
PositionToHid Win68FactoryMap() noexcept;
PositionToHid KpTe153UkFactoryMap() noexcept;
PositionToHid FactoryMap(FactoryLayoutProfile profile) noexcept;
FactoryLayoutProfile FactoryProfileForProduct(const char* product) noexcept;

bool DecodeDeviceInfo(const std::uint8_t* report, std::size_t bytes,
    DeviceInfo* out) noexcept;
bool DecodeTravelInfo(const std::uint8_t* report, std::size_t bytes, TravelInfo* out) noexcept;
bool DecodeKeyMapFragment(const std::uint8_t* report, std::size_t bytes,
    PositionToHid* map, std::array<bool, 10>* received) noexcept;
bool DecodeLiveEvent(const std::uint8_t* report, std::size_t bytes, LiveEvent* out) noexcept;
bool DecodeSnapshotPacket(const std::uint8_t* report, std::size_t bytes,
    std::array<std::uint16_t, kColumns>* rowValues) noexcept;
bool DecodePollRate(const std::uint8_t* report, std::size_t bytes,
    std::uint8_t* code, std::uint16_t* nominalHz) noexcept;
std::uint16_t ToMilli(std::uint16_t travel, std::uint16_t maximum) noexcept;
std::size_t MappedKeyCount(const PositionToHid& map) noexcept;
}

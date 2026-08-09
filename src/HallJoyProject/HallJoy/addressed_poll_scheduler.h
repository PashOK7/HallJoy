#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace addressed
{
constexpr std::size_t kMaxKeysPerPacket = 9;
constexpr std::size_t kMaxPhysicalKeys = 255;

struct PollKeyConfig
{
    std::uint8_t keyId = 0;
    std::uint16_t hidUsage = 0;
};

enum class PollClass : std::uint8_t
{
    Bound,
    Moving,
    Active,
    Recent,
    Background,
};

struct PollPlan
{
    std::array<std::uint8_t, kMaxKeysPerPacket> keyIds{};
    std::array<PollClass, kMaxKeysPerPacket> classes{};
    std::size_t count = 0;
};

struct PollSchedulerStats
{
    std::uint32_t boundCount = 0;
    std::uint32_t movingCount = 0;
    std::uint32_t activeCount = 0;
    std::uint32_t recentCount = 0;
    std::uint32_t backgroundCount = 0;
    std::uint64_t maxBoundAgeUs = 0;
    std::uint64_t maxActiveAgeUs = 0;
    std::uint64_t maxBackgroundAgeUs = 0;
    std::uint64_t deadlineMisses = 0;
};

// Pure scheduling policy. It has no Windows, HID, logging or HallJoy dependencies.
// The transport feeds completed samples through OnSample() and asks for the next
// packet through BuildPlan(). This makes the priority policy independently testable.
class PollScheduler
{
public:
    PollScheduler(const PollKeyConfig* keys, std::size_t count);

    void Reset(std::uint64_t nowUs);
    void SetBound(std::uint16_t hidUsage, bool bound);
    void OnSample(std::uint8_t keyId, std::uint16_t raw, std::uint16_t milli, std::uint64_t nowUs);
    PollPlan BuildPlan(std::uint64_t nowUs);
    PollSchedulerStats GetStats(std::uint64_t nowUs) const;

    std::uint16_t HidForKeyId(std::uint8_t keyId) const;
    bool IsKnownHid(std::uint16_t hidUsage) const;

private:
    struct KeyState
    {
        PollKeyConfig config{};
        std::uint16_t raw = 0;
        std::uint16_t milli = 0;
        std::uint64_t lastPollUs = 0;
        std::uint64_t lastChangeUs = 0;
        std::uint64_t recentUntilUs = 0;
        bool initialised = false;
        bool bound = false;
        bool selected = false;
    };

    std::array<KeyState, kMaxPhysicalKeys> keys_{};
    std::size_t count_ = 0;
    std::uint64_t deadlineMisses_ = 0;

    PollClass Classify(const KeyState& key, std::uint64_t nowUs) const;
    static std::uint64_t TargetIntervalUs(PollClass cls);
    static std::uint64_t MaxAgeUs(PollClass cls);
    static double ClassWeight(PollClass cls);

    int FindBestClass(PollClass wanted, std::uint64_t nowUs) const;
    int FindBestLive(std::uint64_t nowUs) const;
    int FindBestPriority(std::uint64_t nowUs) const;
    bool Add(PollPlan& plan, int index, std::uint64_t nowUs);
    std::size_t CountLive(std::uint64_t nowUs) const;
};
} // namespace addressed

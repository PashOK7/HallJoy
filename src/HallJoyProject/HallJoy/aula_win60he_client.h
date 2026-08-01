#pragma once

#include "aula_win60he_protocol.h"

#include <array>
#include <cstddef>
#include <cstdint>

namespace aula_win60he
{
// The official transport allows up to one second for a command. These are
// deadlines, not sleeps: a successful HID read completes immediately.
constexpr std::uint32_t kSingleResponseTimeoutMs = 1000u;
constexpr std::uint32_t kMatrixResponseTimeoutMs = 1000u;
// sync + precision + 3 default-map reads + 2 complete active-map generations
// (5 reads each) + 2 travel halves.  The active map spans five commands and
// carries no generation counter, so one generation alone is not a coherent
// snapshot proof.
constexpr std::size_t kCapabilityTransactions = 17u;

class ReportTransport
{
public:
    virtual ~ReportTransport() = default;
    virtual bool FlushInput() = 0;
    virtual bool WriteReport(const Report& report) = 0;
    virtual bool ReadReport(std::uint32_t timeoutMs, Report* out) = 0;
    virtual std::uint64_t NowMilliseconds() const noexcept = 0;
};

enum class FailureStage : std::uint8_t
{
    None = 0,
    SessionPoisoned,
    FlushInput,
    Write,
    ReadFirst,
    ReadContinuation,
    ParseFrame,
    DecodeSync,
    UnexpectedFirmware,
    DecodePrecision,
    UnexpectedPrecision,
    DecodeDefaultMap,
    UnexpectedDefaultMap,
    DecodeActiveMap,
    UnstableActiveMap,
    DecodeTravel,
    DecodeStatus,
    ImplausibleTravel,
};

struct Failure
{
    FailureStage stage = FailureStage::None;
    std::uint8_t command = 0;
    std::uint8_t selector = 0;
    std::uint8_t index = 0;
};

const char* FailureStageName(FailureStage stage) noexcept;

enum class TraceKind : std::uint8_t
{
    Transmit = 0,
    Receive,
    Error,
};
using TraceCallback = void(*)(
    void* context,
    TraceKind kind,
    const char* label,
    const Report* report) noexcept;
struct TraceSink
{
    TraceCallback callback = nullptr;
    void* context = nullptr;
};

struct ActiveMapSnapshot
{
    KeyFunctionMap functions{};
    KeyMap keyMap{};
    std::size_t mappedKeys = 0;
};

struct CapabilityProof
{
    SyncInfo sync{};
    PrecisionStroke precision{};
    KeyMap defaultKeyMap{};
    KeyFunctionMap activeFunctions{};
    KeyMap keyMap{};
    std::size_t physicalKeyPositions = 0;
    std::size_t defaultMappedKeys = 0;
    std::size_t mappedKeys = 0;
};

using HidMilliSnapshot = std::array<std::uint16_t, 256>;
struct SnapshotResult
{
    HidMilliSnapshot milli{};
    std::uint32_t activeKeys = 0;
};

class Client
{
public:
    explicit Client(ReportTransport& transport, TraceSink trace = {}) noexcept;

    bool Probe(CapabilityProof* out, Failure* failure = nullptr);
    bool ReadActiveMap(
        const KeyMap& defaultKeyMap,
        ActiveMapSnapshot* out,
        Failure* failure = nullptr);
    bool ReadTravelMatrix(
        const CapabilityProof& proof,
        TravelMatrix* out,
        Failure* failure = nullptr);
    bool QueryStatus(StatusMatrix* out, Failure* failure = nullptr);

    bool IsUsable() const noexcept { return !poisoned_; }

private:
    bool ReadActiveMapGeneration(
        const KeyMap& defaultKeyMap,
        ActiveMapSnapshot* out,
        Failure* failure);
    bool ReadResponse(
        const char* label,
        std::uint8_t requestCommand,
        std::size_t expectedReports,
        std::uint32_t timeoutMs,
        ResponseStream* outStream,
        std::size_t* outBytes,
        Failure* failure,
        std::uint8_t selector,
        std::uint8_t index);
    bool Transact(
        const char* label,
        const Report& request,
        std::uint8_t requestCommand,
        std::size_t expectedReports,
        std::uint32_t timeoutMs,
        ResponseStream* outStream,
        std::size_t* outBytes,
        Failure* failure,
        std::uint8_t selector,
        std::uint8_t index);

    void Trace(
        TraceKind kind,
        const char* label,
        const Report* report) const noexcept;
    void Fail(
        Failure* failure,
        FailureStage stage,
        std::uint8_t command,
        std::uint8_t selector,
        std::uint8_t index,
        const char* traceLabel) noexcept;

    ReportTransport& transport_;
    TraceSink trace_{};
    bool poisoned_ = false;
};

bool TravelValuesPlausible(
    const TravelHalf& values,
    const KeyMap& physicalMap,
    std::uint8_t firstMatrixRow,
    const PrecisionStroke& precision) noexcept;
void BuildHidMilliSnapshot(
    const KeyMap& activeMap,
    const TravelMatrix& travel,
    std::uint16_t maximumTravelUm,
    SnapshotResult* out) noexcept;
}

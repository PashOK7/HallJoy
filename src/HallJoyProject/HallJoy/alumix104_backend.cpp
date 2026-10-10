#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <setupapi.h>
#include <hidsdi.h>
#include <hidpi.h>

#include "alumix104_backend.h"
#include "alumix104_protocol.h"
#include "addressed_poll_scheduler.h"
#include "backend.h"
#include "bindings.h"
#include "physical_analog_state.h"
#include "redsquare_code_probe.h"
#include "support_log.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>
#include <mutex>
#include <memory>
#include <string>
#include <vector>

#pragma comment(lib, "setupapi.lib")
#pragma comment(lib, "hid.lib")

namespace {
namespace probe = halljoy::redsquare_probe;
namespace protocol = halljoy::alumix104;
bool Emit(const std::string& line);
unsigned BoundPadMask(std::uint16_t hid);
unsigned RawHeldLetters(std::uint32_t mask) noexcept;

// The selected-key 0x55FB stream is retained for comparison only. Its missing
// per-key updates cannot safely drive the gamepad (see physical log 43).
struct BatchProbe {
    struct KeyEvidence {
        unsigned slot = 0;
        std::uint64_t positive = 0, zero = 0, changed = 0, releases = 0;
        std::uint64_t lastMs = 0, rawHeldPositive = 0, rawOffPositive = 0;
    };
    std::array<addressed::PollKeyConfig, protocol::kFactoryHid.size()> keys{};
    std::unique_ptr<addressed::PollScheduler> scheduler;
    protocol::BatchFrame request{};
    addressed::PollPlan pendingPlan{};
    std::array<bool, protocol::kFactoryHid.size()> visited{};
    std::array<std::uint16_t, protocol::kFactoryHid.size()> lastStroke{};
    std::array<KeyEvidence, protocol::kFactoryHid.size()> evidence{};
    std::uint64_t sent = 0, echo = 0, candidate = 0, malformed = 0, headerVariant = 0;
    std::uint64_t timeout = 0, writeFail = 0, positive = 0, zero = 0, changed = 0, releases = 0;
    unsigned maxPositivePerReply = 0, maxFreshPositive = 0, nextSlot = 1;
    std::uint64_t lastSendMs = 0, summaryMs = 0, lastBindingRefreshMs = 0;
    unsigned unique = 0, consecutiveTimeouts = 0;
    bool awaiting = false, stopped = false, sweepReported = false, candidateShown = false;

    BatchProbe() {
        for (unsigned i = 0; i < keys.size(); ++i)
            keys[i] = {static_cast<std::uint8_t>(i), protocol::kFactoryHid[i]};
        scheduler = std::make_unique<addressed::PollScheduler>(keys.data(), keys.size());
        scheduler->Reset(GetTickCount64() * 1000);
    }
    void RefreshBindings() {
        for (const auto& key : keys)
            scheduler->SetPhysicalBound(key.keyId,
                key.hidUsage && Bindings_IsHidBound(key.hidUsage));
    }
    bool Write(HANDLE device) {
        HidIoOperation op(device);
        DWORD got = 0, error = 0;
        const auto start = op.StartWrite(request.data(),
            static_cast<DWORD>(request.size()), &error);
        bool ok = false;
        if (start != HidIoOperation::StartResult::Failed) {
            if (start == HidIoOperation::StartResult::Pending &&
                op.Wait(400) != WAIT_OBJECT_0)
                op.CancelAndDrain(&got, &error);
            else ok = op.Finish(&got, &error, false);
        }
        if (ok && got == request.size()) return true;
        ++writeFail;
        SupportLog_Event("alumix104.batch68_write_failed", sent,
                         SupportLog_Win32(error));
        return false;
    }
    void Observe(const std::vector<std::uint8_t>& frame, DWORD got,
                 std::uint64_t nowMs) {
        if (!awaiting || got < 3 || frame[0] != 0 || frame[1] != 0x55 ||
            frame[2] != 0x68) return;
        std::array<protocol::BatchReading, protocol::kBatchKeys> readings{};
        const auto kind = protocol::ParseBatchReply(frame.data(), got, request,
            pendingPlan.keyIds.data(), pendingPlan.count, readings);
        if (kind == protocol::BatchReplyKind::Malformed) {
            ++malformed;
            return; // A delayed/wrong-offset reply does not complete this query.
        }
        if (kind == protocol::BatchReplyKind::Unrelated) return;
        if (frame[3] != request[3]) {
            if (!headerVariant)
                Emit("batch68_header_variant expected=" + std::to_string(request[3]) +
                     " actual=" + std::to_string(frame[3]));
            ++headerVariant;
        }
        awaiting = false;
        consecutiveTimeouts = 0;
        if (kind == protocol::BatchReplyKind::Echo) ++echo;
        else {
            ++candidate;
            if (!candidateShown) {
                probe::Show(probe::Status::BatchCandidate);
                candidateShown = true;
                Emit("batch68_candidate response=non_echo requires_motion_release_validation=1 gamepad_source=disabled");
            }
        }
        unsigned positiveInReply = 0;
        for (std::size_t i = 0; i < pendingPlan.count; ++i) {
            const auto index = pendingPlan.keyIds[i];
            if (!visited[index]) { visited[index] = true; ++unique; }
            // Echo advances matrix fairness without overwriting measured depth.
            const std::uint16_t stroke = kind == protocol::BatchReplyKind::Candidate
                ? readings[i].stroke : std::uint16_t(0);
            if (kind == protocol::BatchReplyKind::Echo)
                scheduler->OnQueried(index, nowMs * 1000);
            else
                scheduler->OnSample(index, stroke,
                    stroke ? std::uint16_t(500) : std::uint16_t(0), nowMs * 1000);
            if (kind == protocol::BatchReplyKind::Candidate) {
                auto& key = evidence[index];
                if (stroke && !key.slot) key.slot = nextSlot++;
                if (stroke) { ++positive; ++positiveInReply; } else ++zero;
                if (lastStroke[index] != stroke) ++changed;
                if (lastStroke[index] && !stroke) ++releases;
                if (stroke) {
                    ++key.positive;
                    for (unsigned letter = 0; letter < probe::letterSensorIndex.size(); ++letter)
                        if (probe::letterSensorIndex[letter] == index) {
                            const auto mask = probe::rawHeldState.load(std::memory_order_acquire);
                            if (mask & (std::uint64_t(1) << letter)) ++key.rawHeldPositive;
                            else ++key.rawOffPositive;
                            break;
                        }
                } else ++key.zero;
                if (lastStroke[index] != stroke) ++key.changed;
                if (lastStroke[index] && !stroke) ++key.releases;
                key.lastMs = nowMs;
                lastStroke[index] = stroke;
            }
        }
        maxPositivePerReply = (std::max)(maxPositivePerReply, positiveInReply);
        unsigned freshPositive = 0;
        for (std::size_t index = 0; index < evidence.size(); ++index)
            if (lastStroke[index] && evidence[index].lastMs &&
                nowMs >= evidence[index].lastMs && nowMs - evidence[index].lastMs <= 50)
                ++freshPositive;
        maxFreshPositive = (std::max)(maxFreshPositive, freshPositive);
        if (unique == keys.size() && !sweepReported) {
            sweepReported = true;
            Emit("batch68_full_sweep positions=" + std::to_string(unique) +
                 " replies=" + std::to_string(echo + candidate) +
                 " echo=" + std::to_string(echo) +
                 " candidate=" + std::to_string(candidate));
            if (!candidate) {
                probe::Show(probe::Status::BatchEcho);
            }
        }
    }
    void Step(HANDLE device, std::uint64_t nowMs) {
        if (stopped) return;
        if (awaiting && nowMs >= lastSendMs && nowMs - lastSendMs >= 1000) {
            awaiting = false;
            ++timeout; ++consecutiveTimeouts;
            if (consecutiveTimeouts >= 3) {
                stopped = true;
                Emit("batch68_stopped reason=no_reply timeouts=" +
                     std::to_string(timeout) + " queried_positions=" +
                     std::to_string(unique));
                probe::Show(probe::Status::Fault);
                return;
            }
        }
        if (awaiting || (lastSendMs && nowMs - lastSendMs < 5)) return;
        if (!lastBindingRefreshMs || nowMs - lastBindingRefreshMs >= 250) {
            RefreshBindings();
            lastBindingRefreshMs = nowMs;
        }
        pendingPlan = scheduler->BuildPlan(nowMs * 1000, protocol::kBatchKeys);
        if (!pendingPlan.count || !protocol::MakeBatchRequest(
                pendingPlan.keyIds.data(), pendingPlan.count,
                static_cast<std::uint16_t>((sent % 18) * 56), request)) {
            stopped = true;
            Emit("batch68_stopped reason=invalid_plan");
            return;
        }
        if (!Write(device)) {
            stopped = true;
            Emit("batch68_stopped reason=write_failed");
            return;
        }
        ++sent;
        lastSendMs = nowMs;
        awaiting = true;
        if (!summaryMs || nowMs - summaryMs >= 10000) {
            summaryMs = nowMs;
            Emit("batch68_progress sent=" + std::to_string(sent) +
                 " echo=" + std::to_string(echo) +
                 " candidate=" + std::to_string(candidate) +
                 " positions=" + std::to_string(unique) +
                 " positive=" + std::to_string(positive) +
                 " zero=" + std::to_string(zero) +
                 " changed=" + std::to_string(changed) +
                 " releases=" + std::to_string(releases) +
                 " max_positive_batch=" + std::to_string(maxPositivePerReply) +
                 " max_fresh_positive=" + std::to_string(maxFreshPositive) +
                 " timeout=" + std::to_string(timeout) +
                 " malformed=" + std::to_string(malformed));
            LogSlots(nowMs);
        }
    }
    void LogSlots(std::uint64_t nowMs) const {
        for (std::size_t index = 0; index < evidence.size(); ++index) {
            const auto& key = evidence[index];
            if (!key.slot) continue;
            Emit("batch68_slot slot=" + std::to_string(key.slot) +
                 " positive=" + std::to_string(key.positive) +
                 " zero=" + std::to_string(key.zero) +
                 " changed=" + std::to_string(key.changed) +
                 " releases=" + std::to_string(key.releases) +
                 " age_ms=" + std::to_string(key.lastMs && nowMs >= key.lastMs ? nowMs - key.lastMs : 0) +
                 " last_positive=" + std::to_string(lastStroke[index] != 0) +
                 " bound=" + std::to_string(BoundPadMask(keys[index].hidUsage)) +
                 " raw_held_positive=" + std::to_string(key.rawHeldPositive) +
                 " raw_off_positive=" + std::to_string(key.rawOffPositive));
        }
    }
    void Final() const {
        Emit("batch68_header_variant count=" + std::to_string(headerVariant));
        Emit("batch68_final sent=" + std::to_string(sent) +
             " echo=" + std::to_string(echo) +
             " candidate=" + std::to_string(candidate) +
             " positions=" + std::to_string(unique) +
             " positive=" + std::to_string(positive) +
             " zero=" + std::to_string(zero) +
             " changed=" + std::to_string(changed) +
             " releases=" + std::to_string(releases) +
             " max_positive_batch=" + std::to_string(maxPositivePerReply) +
             " max_fresh_positive=" + std::to_string(maxFreshPositive) +
             " timeout=" + std::to_string(timeout) +
             " malformed=" + std::to_string(malformed) +
             " write_failed=" + std::to_string(writeFail) +
             " pending=" + std::to_string(awaiting));
        LogSlots(GetTickCount64());
    }
};

struct Candidate {
    std::wstring path;
    unsigned inputBytes = 0;
    unsigned outputBytes = 0;
};
struct Discovery {
    unsigned vidpid = 0, rejectedProduct = 0, rejectedCollection = 0;
    std::vector<Candidate> accepted;
    std::vector<std::string> collectionEvidence;
};

std::mutex g_service;
HANDLE g_worker = nullptr, g_wake = nullptr;
std::atomic<bool> g_stop{false}, g_present{false}, g_connected{false};
std::atomic<std::uint64_t> g_published{0}, g_failed{0}, g_consumed{0},
    g_consumedPositive{0}, g_lastSample{0}, g_logDrops{0};
std::atomic<std::uint64_t> g_outputPublished{0}, g_outputNonneutral{0},
    g_outputRejected{0};
std::atomic<std::uint64_t> g_outputCandidates{0}, g_outputCandidateActive{0},
    g_outputCandidateChanges{0}, g_outputCandidateDue{0};
std::atomic<unsigned> g_outputCandidatePadMask{0};
std::atomic<std::uint32_t> g_inputBytes{0}, g_outputBytes{0}, g_lastMaximum{0};
halljoy::physical_analog::Publication g_depth;
std::array<std::atomic<std::uint64_t>, halljoy::physical_analog::kHidCount>
    g_consumerByHid{};
std::array<std::atomic<std::uint16_t>, halljoy::physical_analog::kHidCount>
    g_consumerMilliByHid{};

bool Emit(const std::string& line) {
    if (SupportLog_RedSquareResearch(line.c_str())) return true;
    ++g_logDrops;
    return false;
}
// Source-thread-only trace. Selected-key samples remain diagnostic references.
// The exact-device research trial records changed unknown vendor reports below.
struct SensorTrace {
    unsigned slot = 0;
    std::uint64_t reports = 0, positive = 0, zero = 0, samePositive = 0;
    std::uint64_t changedPositive = 0, explicitRelease = 0;
    std::uint64_t lastMs = 0, maxGapAfterPositiveMs = 0;
    std::uint64_t digitalHeldPositive = 0, digitalOffPositive = 0;
    std::uint64_t consumerBaseline = 0;
    std::uint16_t lastStroke = 0, lastMaximum = 0;
    unsigned windowReports = 0, windowSame = 0, windowZero = 0;
    bool lastPositive = false;
};
struct AnalogTrace {
    static constexpr unsigned kRawCaptureLimit = 512;
    struct UnknownClass {
        unsigned slot = 0;
        std::uint64_t reports = 0, changed = 0, stableChanged = 0;
        std::uint64_t held2 = 0, held2Changed = 0, zeroBody = 0;
        std::uint64_t changedOffsets = 0, lastRevision = 0;
        unsigned maxNonzero = 0, lastSize = 0;
        std::array<std::uint8_t, 65> last{};
    };
    std::array<SensorTrace, 121> sensors{};
    std::array<UnknownClass, 256> unknown{};
    unsigned nextSlot = 1;
    unsigned nextUnknownSlot = 1;
    std::uint64_t windowStartMs = 0, windows = 0;
    std::uint64_t lastConsumerPositive = 0, lastOutput = 0;
    std::uint64_t lastOutputActive = 0, lastCandidates = 0;
    std::uint64_t lastCandidateActive = 0;
    std::uint64_t lastRawDown = 0, lastRawUp = 0;
    std::uint64_t otherPrefix = 0, aaResponses = 0, other55 = 0;
    std::uint64_t unknownShortOrReportId = 0;
    unsigned unknownWindow = 0, unknownChangedWindow = 0;
    unsigned unknownStableWindow = 0, unknownHeld2Window = 0;
    std::array<std::uint64_t, 256> aaOpcodeCounts{}, other55OpcodeCounts{};
    unsigned windowReports = 0, windowPositive = 0, windowZero = 0;
    unsigned windowSame = 0;
    unsigned rawEligible = 0, rawCaptured = 0, rawCapSkipped = 0;
    unsigned rawEmitFailed = 0;
    std::uint64_t lastSelectedMs = 0;
    std::uint16_t lastSelectedStroke = 0, lastSelectedMaximum = 0;
    unsigned lastSelectedIndex = 0;

    void Reset(std::uint64_t now) noexcept {
        *this = {};
        windowStartMs = now;
    }
    void Observe(const protocol::TravelSample& sample, std::uint64_t now,
                 std::uint32_t digitalMask) noexcept {
        auto& s = sensors[sample.index];
        if (!s.slot) s.slot = nextSlot++;
        if (s.lastMs && s.lastPositive && now >= s.lastMs)
            s.maxGapAfterPositiveMs = (std::max)(s.maxGapAfterPositiveMs,
                                                now - s.lastMs);
        const bool positive = sample.stroke != 0;
        const bool same = positive && s.lastPositive &&
            sample.stroke == s.lastStroke && sample.maximum == s.lastMaximum;
        ++s.reports; ++s.windowReports; ++windowReports;
        if (positive) {
            ++s.positive; ++windowPositive;
            if (same) { ++s.samePositive; ++s.windowSame; ++windowSame; }
            else if (s.lastPositive) ++s.changedPositive;
            for (unsigned letter = 0; letter < probe::letterSensorIndex.size(); ++letter)
                if (probe::letterSensorIndex[letter] == sample.index) {
                    if (digitalMask & (1u << letter)) ++s.digitalHeldPositive;
                    else ++s.digitalOffPositive;
                    break;
                }
        } else {
            ++s.zero; ++s.windowZero; ++windowZero;
            if (s.lastPositive) ++s.explicitRelease;
        }
        s.lastStroke = sample.stroke;
        s.lastMaximum = sample.maximum;
        s.lastPositive = positive;
        s.lastMs = now;
        lastSelectedMs = now;
        lastSelectedIndex = sample.index;
        lastSelectedStroke = sample.stroke;
        lastSelectedMaximum = sample.maximum;
    }
    bool ObserveOther(const std::vector<std::uint8_t>& report,
                      DWORD got, std::uint64_t rawHeld) noexcept {
        if (got < 3 || report[0]) {
            ++otherPrefix; ++unknownShortOrReportId; return false;
        }
        if (report[1] == 0xaa) {
            ++aaResponses; ++aaOpcodeCounts[report[2]];
            return false;
        } else if (report[1] == 0x55) {
            ++other55; ++other55OpcodeCounts[report[2]];
            return false;
        } else {
            ++otherPrefix; ++unknownWindow;
            auto& item = unknown[report[1]];
            if (!item.slot) item.slot = nextUnknownSlot++;
            const auto size = (std::min)(std::size_t(got), report.size());
            const auto revision = rawHeld >> 32;
            const auto held = static_cast<std::uint32_t>(rawHeld) & ((1u << 26) - 1u);
            const bool held2 = (held & (held - 1u)) != 0;
            const bool first = item.lastSize == 0;
            bool changed = !first && item.lastSize != size;
            unsigned nonzero = 0;
            for (std::size_t i = 2; i < size; ++i) {
                if (report[i]) ++nonzero;
                if (item.lastSize && report[i] != item.last[i]) {
                    changed = true;
                    item.changedOffsets |= std::uint64_t(1) << (i - 2);
                }
            }
            ++item.reports;
            if (held2) { ++item.held2; ++unknownHeld2Window; }
            if (!nonzero) ++item.zeroBody;
            item.maxNonzero = (std::max)(item.maxNonzero, nonzero);
            if (changed) {
                ++item.changed; ++unknownChangedWindow;
                if (held2) ++item.held2Changed;
                if (revision == item.lastRevision) {
                    ++item.stableChanged; ++unknownStableWindow;
                }
            }
            std::copy_n(report.begin(), size, item.last.begin());
            item.lastSize = static_cast<unsigned>(size);
            item.lastRevision = revision;
            return first || changed;
        }
    }
};
AnalogTrace g_trace;

void LogChangedUnknown(const std::vector<std::uint8_t>& report, DWORD got,
                       std::uint64_t rawHeld, std::uint64_t now) {
    ++g_trace.rawEligible;
    if (g_trace.rawCaptured >= AnalogTrace::kRawCaptureLimit) {
        ++g_trace.rawCapSkipped;
        if (g_trace.rawCapSkipped == 1)
            probe::Show(probe::Status::RawCaptureFull);
        return;
    }
    const auto size = (std::min)(std::size_t(got), report.size());
    const auto id = g_trace.rawEligible;
    const auto& item = g_trace.unknown[report[1]];
    const auto held = static_cast<std::uint32_t>(rawHeld) & ((1u << 26) - 1u);
    std::string context = "unknown_packet id=" + std::to_string(id) +
        " slot=" + std::to_string(item.slot) +
        " report_n=" + std::to_string(item.reports) +
        " bytes=" + std::to_string(size) +
        " held_letters=" + std::to_string(RawHeldLetters(held)) +
        " digital_revision=" + std::to_string(rawHeld >> 32);
    if (g_trace.lastSelectedMs && now >= g_trace.lastSelectedMs)
        context += " ref_index=" + std::to_string(g_trace.lastSelectedIndex) +
            " ref_stroke=" + std::to_string(g_trace.lastSelectedStroke) +
            " ref_max=" + std::to_string(g_trace.lastSelectedMaximum) +
            " ref_age_ms=" + std::to_string(now - g_trace.lastSelectedMs);
    else context += " ref=none";
    constexpr char digits[] = "0123456789ABCDEF";
    std::string payload = "unknown_bytes id=" + std::to_string(id) + " hex=";
    payload.reserve(payload.size() + size * 2);
    for (std::size_t i = 0; i < size; ++i) {
        payload += digits[report[i] >> 4];
        payload += digits[report[i] & 0x0f];
    }
    // Separate lines keep both the 65-byte packet and its reference within
    // the existing support writer's 240-byte record/256-byte stored-line bounds.
    const bool contextOk = Emit(context);
    const bool payloadOk = Emit(payload);
    if (contextOk && payloadOk) ++g_trace.rawCaptured;
    else ++g_trace.rawEmitFailed;
}

unsigned BoundPadMask(std::uint16_t hid) {
    unsigned mask = 0;
    for (int pad = 0; pad < BINDINGS_MAX_GAMEPADS; ++pad)
        if (hid && Bindings_IsHidBoundForPad(pad, hid)) mask |= 1u << pad;
    return mask;
}
unsigned BindingActionMask(std::uint16_t hid, int pad) {
    if (!hid) return 0;
    unsigned mask = 0;
    for (unsigned axis = 0; axis < 4; ++axis) {
        const auto binding = Bindings_GetAxisForPad(pad, static_cast<Axis>(axis));
        if (std::find(binding.minusHids.begin(), binding.minusHids.end(), hid) != binding.minusHids.end())
            mask |= 1u << axis;
        if (std::find(binding.plusHids.begin(), binding.plusHids.end(), hid) != binding.plusHids.end())
            mask |= 1u << (axis + 4);
    }
    for (unsigned trigger = 0; trigger < 2; ++trigger)
        if (Bindings_GetTriggerForPad(pad, static_cast<Trigger>(trigger)) == hid)
            mask |= 1u << (trigger + 8);
    for (unsigned button = 0; button < 15; ++button)
        if (Bindings_ButtonHasHidForPad(pad, static_cast<GameButton>(button), hid))
            mask |= 1u << (button + 10);
    return mask;
}
unsigned RawHeldLetters(std::uint32_t mask) noexcept {
    unsigned count = 0;
    for (; mask; mask &= mask - 1) ++count;
    return count;
}
unsigned MilliBucket(std::uint16_t milli) noexcept {
    return milli ? (std::min)(4u, 1u + unsigned(milli - 1) / 250u) : 0u;
}
void TraceWindow(std::uint64_t now, bool final = false) {
    if (!g_trace.windowStartMs || now < g_trace.windowStartMs) return;
    bool recentPositive = false;
    for (const auto& s : g_trace.sensors)
        if (s.lastPositive && s.lastMs && now - s.lastMs <= 2000)
            recentPositive = true;
    const auto duration = now - g_trace.windowStartMs;
    if (!final && duration < (recentPositive ? 250u : 1000u)) return;
    if (!duration && !g_trace.windowReports) return;
    unsigned unique = 0, carry = 0, fresh50 = 0, stale250 = 0;
    std::vector<unsigned> slots;
    for (unsigned index = 0; index < g_trace.sensors.size(); ++index) {
        const auto& s = g_trace.sensors[index];
        if (s.windowReports) ++unique;
        if (s.lastPositive && s.lastMs) {
            ++carry;
            const auto age = now - s.lastMs;
            if (age <= 50) ++fresh50;
            if (age > 250) ++stale250;
        }
        if (s.slot && (s.windowReports ||
            (s.lastPositive && s.lastMs && now - s.lastMs <= 2000)))
            slots.push_back(index);
    }
    (std::sort)(slots.begin(), slots.end(), [](unsigned a, unsigned b) {
        return g_trace.sensors[a].slot < g_trace.sensors[b].slot;
    });
    const auto consumerPositive = g_consumedPositive.load();
    const auto output = g_outputPublished.load();
    const auto outputActive = g_outputNonneutral.load();
    const auto candidates = g_outputCandidates.load();
    const auto candidateActive = g_outputCandidateActive.load();
    const auto rawMask = static_cast<std::uint32_t>(
        probe::rawHeldState.load(std::memory_order_acquire)) & ((1u << 26) - 1u);
    const auto rawDown = probe::letterDown.load();
    const auto rawUp = probe::letterUp.load();
    Emit("trace_source ms=" + std::to_string(duration) +
         " r=" + std::to_string(g_trace.windowReports) +
         " p=" + std::to_string(g_trace.windowPositive) +
         " z=" + std::to_string(g_trace.windowZero) +
         " same=" + std::to_string(g_trace.windowSame) +
         " unique=" + std::to_string(unique) +
         " carry=" + std::to_string(carry) +
         " fresh50=" + std::to_string(fresh50) +
         " stale250=" + std::to_string(stale250) +
         " raw_letters=" + std::to_string(RawHeldLetters(rawMask)) +
         " raw_down=" + std::to_string(rawDown - g_trace.lastRawDown) +
         " raw_up=" + std::to_string(rawUp - g_trace.lastRawUp));
    Emit("trace_pipeline consume_pos=" +
         std::to_string(consumerPositive - g_trace.lastConsumerPositive) +
         " candidate=" + std::to_string(candidates - g_trace.lastCandidates) +
         " candidate_active=" + std::to_string(candidateActive - g_trace.lastCandidateActive) +
         " pub=" + std::to_string(output - g_trace.lastOutput) +
         " pub_active=" + std::to_string(outputActive - g_trace.lastOutputActive) +
         " pad_mask=" + std::to_string(g_outputCandidatePadMask.load()));
    if (g_trace.unknownWindow)
        Emit("trace_unknown_window n=" +
             std::to_string(g_trace.unknownWindow) +
             " changed=" + std::to_string(g_trace.unknownChangedWindow) +
             " stable_changed=" + std::to_string(g_trace.unknownStableWindow) +
             " held2=" + std::to_string(g_trace.unknownHeld2Window) +
             " raw_letters=" + std::to_string(RawHeldLetters(rawMask)));
    // Each opaque slot reports source cadence and the consumer's latest depth
    // bucket. Digital state is diagnostic context only, never an input value.
    for (std::size_t offset = 0; offset < slots.size(); offset += 4) {
        std::string line = "trace_slots g=" + std::to_string(offset / 4) + " data=";
        for (std::size_t j = offset; j < (std::min)(offset + 4, slots.size()); ++j) {
            const auto index = slots[j];
            auto& s = g_trace.sensors[index];
            const auto hid = protocol::kFactoryHid[index];
            const auto reads = hid ? g_consumerByHid[hid].load() : 0;
            unsigned digital = 2; // 2 = no alphabetic Raw Input reference.
            for (unsigned letter = 0; letter < probe::letterSensorIndex.size(); ++letter)
                if (probe::letterSensorIndex[letter] == index) {
                    digital = (rawMask & (1u << letter)) ? 1u : 0u;
                    break;
                }
            if (j != offset) line += ";";
            line += std::to_string(s.slot) + "," +
                std::to_string(s.windowReports) + "," +
                std::to_string(s.windowSame) + "," +
                std::to_string(s.windowZero) + "," +
                std::to_string(s.lastMs ? now - s.lastMs : 0) + "," +
                std::to_string(s.lastPositive) + "," +
                std::to_string(reads - s.consumerBaseline) + "," +
                std::to_string(hid ? MilliBucket(g_consumerMilliByHid[hid].load()) : 0) + "," +
                std::to_string(BoundPadMask(hid)) + "," +
                std::to_string(digital);
            s.consumerBaseline = reads;
        }
        Emit(line);
    }
    for (auto& s : g_trace.sensors)
        s.windowReports = s.windowSame = s.windowZero = 0;
    g_trace.windowReports = g_trace.windowPositive = 0;
    g_trace.windowZero = g_trace.windowSame = 0;
    g_trace.unknownWindow = g_trace.unknownChangedWindow = 0;
    g_trace.unknownStableWindow = g_trace.unknownHeld2Window = 0;
    g_trace.windowStartMs = now;
    g_trace.lastConsumerPositive = consumerPositive;
    g_trace.lastOutput = output;
    g_trace.lastOutputActive = outputActive;
    g_trace.lastCandidates = candidates;
    g_trace.lastCandidateActive = candidateActive;
    g_trace.lastRawDown = rawDown;
    g_trace.lastRawUp = rawUp;
    ++g_trace.windows;
}
void TraceFinal(std::uint64_t now) {
    TraceWindow(now, true);
    Emit("unknown_packet_summary eligible=" + std::to_string(g_trace.rawEligible) +
         " captured=" + std::to_string(g_trace.rawCaptured) +
         " cap_skipped=" + std::to_string(g_trace.rawCapSkipped) +
         " emit_failed=" + std::to_string(g_trace.rawEmitFailed) +
         " limit=" + std::to_string(AnalogTrace::kRawCaptureLimit));
    Emit("trace_final windows=" + std::to_string(g_trace.windows) +
         " slots=" + std::to_string(g_trace.nextSlot - 1) +
         " aa=" + std::to_string(g_trace.aaResponses) +
         " other55=" + std::to_string(g_trace.other55) +
         " other_prefix=" + std::to_string(g_trace.otherPrefix));
    unsigned opcodeRows = 0;
    for (unsigned opcode = 0; opcode < 256; ++opcode) {
        const auto aa = g_trace.aaOpcodeCounts[opcode];
        const auto other55 = g_trace.other55OpcodeCounts[opcode];
        if (aa || other55) {
            if (opcodeRows++ < 32)
                Emit("trace_header opcode=" + std::to_string(opcode) +
                     " aa=" + std::to_string(aa) +
                     " stream55=" + std::to_string(other55));
        }
    }
    if (opcodeRows > 32)
        Emit("trace_header_omitted=" + std::to_string(opcodeRows - 32));
    Emit("trace_unknown_classes count=" +
         std::to_string(g_trace.nextUnknownSlot - 1) +
         " short_or_report_id=" +
         std::to_string(g_trace.unknownShortOrReportId));
    for (const auto& item : g_trace.unknown) {
        if (!item.slot) continue;
        Emit("trace_unknown slot=" + std::to_string(item.slot) +
             " reports=" + std::to_string(item.reports) +
             " changed=" + std::to_string(item.changed) +
             " stable_changed=" + std::to_string(item.stableChanged) +
             " held2=" + std::to_string(item.held2) +
             " held2_changed=" + std::to_string(item.held2Changed) +
             " zero_body=" + std::to_string(item.zeroBody) +
             " max_nonzero=" + std::to_string(item.maxNonzero) +
             " changed_offsets=" + std::to_string(item.changedOffsets));
    }
    for (unsigned index = 0; index < g_trace.sensors.size(); ++index) {
        const auto& s = g_trace.sensors[index];
        if (!s.slot) continue;
        const auto hid = protocol::kFactoryHid[index];
        Emit("trace_sensor slot=" + std::to_string(s.slot) +
             " r=" + std::to_string(s.reports) +
             " p=" + std::to_string(s.positive) +
             " z=" + std::to_string(s.zero) +
             " same=" + std::to_string(s.samePositive) +
             " changed=" + std::to_string(s.changedPositive) +
             " release=" + std::to_string(s.explicitRelease) +
             " max_gap_pos_ms=" + std::to_string(s.maxGapAfterPositiveMs) +
             " last_pos=" + std::to_string(s.lastPositive) +
             " last_age_ms=" + std::to_string(s.lastMs ? now - s.lastMs : 0) +
             " raw_on_pos=" + std::to_string(s.digitalHeldPositive) +
             " raw_off_pos=" + std::to_string(s.digitalOffPositive) +
             " bound=" + std::to_string(BoundPadMask(hid)) +
             " consumer_reads=" + std::to_string(hid ? g_consumerByHid[hid].load() : 0));
        Emit("trace_mapping slot=" + std::to_string(s.slot) +
             " pad0=" + std::to_string(BindingActionMask(hid, 0)) +
             " pad1=" + std::to_string(BindingActionMask(hid, 1)) +
             " pad2=" + std::to_string(BindingActionMask(hid, 2)) +
             " pad3=" + std::to_string(BindingActionMask(hid, 3)));
    }
}
void ClearValues() {
    g_connected.store(false, std::memory_order_release);
    g_depth.Clear();
    g_lastSample.store(0);
    g_lastMaximum.store(0);
}
void BindFactoryMap() {
    g_depth.Clear();
    for (unsigned index = 0; index < protocol::kFactoryHid.size(); ++index)
        if (const auto hid = protocol::kFactoryHid[index])
            g_depth.Bind(static_cast<std::uint8_t>(index + 1), hid);
}

Discovery FindExactDevice() {
    Discovery found{};
    GUID guid{};
    HidD_GetHidGuid(&guid);
    const auto set = SetupDiGetClassDevsW(&guid, nullptr, nullptr,
                                           DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    if (set == INVALID_HANDLE_VALUE) {
        SupportLog_Event("alumix104.enumeration_failed", 0,
                         SupportLog_Win32(GetLastError()));
        return found;
    }
    struct Guard { HDEVINFO h; ~Guard() { SetupDiDestroyDeviceInfoList(h); } } guard{set};
    for (DWORD i = 0; i < 256 && !g_stop.load(); ++i) {
        SP_DEVICE_INTERFACE_DATA iface{}; iface.cbSize = sizeof(iface);
        if (!SetupDiEnumDeviceInterfaces(set, nullptr, &guid, i, &iface)) break;
        DWORD bytes = 0;
        SetupDiGetDeviceInterfaceDetailW(set, &iface, nullptr, 0, &bytes, nullptr);
        if (bytes < sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W) || bytes > 65536) continue;
        std::vector<std::uint8_t> storage(bytes);
        auto* detail = reinterpret_cast<SP_DEVICE_INTERFACE_DETAIL_DATA_W*>(storage.data());
        detail->cbSize = sizeof(*detail);
        if (!SetupDiGetDeviceInterfaceDetailW(set, &iface, detail, bytes,
                                              nullptr, nullptr)) continue;
        probe::Handle meta{CreateFileW(detail->DevicePath, 0,
                           FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                           OPEN_EXISTING, 0, nullptr)};
        if (meta.h == INVALID_HANDLE_VALUE) continue;
        HIDD_ATTRIBUTES attr{}; attr.Size = sizeof(attr);
        if (!HidD_GetAttributes(meta.h, &attr) || attr.VendorID != 0x0c45 ||
            attr.ProductID != 0x80ac) continue;
        ++found.vidpid;
        wchar_t product[256]{};
        if (!HidD_GetProductString(meta.h, product, sizeof(product)) ||
            !protocol::ExactProduct(product)) {
            ++found.rejectedProduct;
            found.collectionEvidence.push_back("analog_collection slot=" +
                std::to_string(found.vidpid) + " exact_product=0");
            continue;
        }
        PHIDP_PREPARSED_DATA prepared = nullptr;
        if (!HidD_GetPreparsedData(meta.h, &prepared)) {
            ++found.rejectedCollection;
            found.collectionEvidence.push_back("analog_collection slot=" +
                std::to_string(found.vidpid) + " descriptor=unavailable");
            continue;
        }
        HIDP_CAPS caps{};
        const auto status = HidP_GetCaps(prepared, &caps);
        HidD_FreePreparsedData(prepared);
        if (status != HIDP_STATUS_SUCCESS) {
            ++found.rejectedCollection;
            found.collectionEvidence.push_back("analog_collection slot=" +
                std::to_string(found.vidpid) + " caps=unavailable");
            continue;
        }
        const bool exact = protocol::ExactCollection(caps.UsagePage, caps.Usage,
                                       caps.InputReportByteLength,
                                       caps.OutputReportByteLength);
        found.collectionEvidence.push_back("analog_collection slot=" + std::to_string(found.vidpid) +
             " page=" + std::to_string(caps.UsagePage) +
             " usage=" + std::to_string(caps.Usage) +
             " input_bytes=" + std::to_string(caps.InputReportByteLength) +
             " output_bytes=" + std::to_string(caps.OutputReportByteLength) +
             " feature_bytes=" + std::to_string(caps.FeatureReportByteLength) +
             " input_values=" + std::to_string(caps.NumberInputValueCaps) +
             " input_buttons=" + std::to_string(caps.NumberInputButtonCaps) +
             " accepted=" + std::to_string(exact));
        if (!exact) {
            ++found.rejectedCollection; continue;
        }
        found.accepted.push_back({detail->DevicePath, 65, 65});
    }
    return found;
}

void RecordSummary(const probe::StreamStats& s, bool firstOff, bool on,
                   bool finalOff, bool finalAck, unsigned endReason,
                   std::uint64_t maxGapMs) {
    Emit("analog_end reason=" + std::to_string(endReason) +
         " first_off=" + std::to_string(firstOff) +
         " on_write=" + std::to_string(on) +
         " on_ack=" + std::to_string(s.ack_on) +
         " final_off=" + std::to_string(finalOff) +
         " final_off_ack=" + std::to_string(finalAck));
    Emit("analog_samples reports=" + std::to_string(s.reports) +
         " valid=" + std::to_string(s.samples) +
         " unique=" + std::to_string(s.unique) +
         " positive_indices=" + std::to_string(s.unique_positive) +
         " depth_changes=" + std::to_string(s.changed) +
         " releases=" + std::to_string(s.releases));
    Emit("analog_timing paired_hold=" + std::to_string(s.pair_witnesses) +
         " max_report_gap_ms=" + std::to_string(maxGapMs) +
         " idle_reads=" + std::to_string(s.idle_reads));
    Emit("analog_quality max_zero=" + std::to_string(s.max_zero) +
         " max_zero_positive=" + std::to_string(s.max_zero_positive) +
         " above_full=" + std::to_string(s.over_full) +
         " above_125=" + std::to_string(s.over_125) +
         " above_200=" + std::to_string(s.over_200) +
         " adc_outside=" + std::to_string(s.adc_outside_bounds));
    Emit("analog_integrity malformed=" + std::to_string(s.malformed) +
         " out_of_catalog=" + std::to_string(s.out_of_catalog) +
         " unrelated=" + std::to_string(s.unrelated) +
         " bad_prefix=" + std::to_string(s.bad_prefix));
    Emit("analog_pipeline published=" + std::to_string(g_published.load()) +
         " consumer_reads=" + std::to_string(g_consumed.load()) +
         " consumer_positive=" + std::to_string(g_consumedPositive.load()) +
         " failed=" + std::to_string(g_failed.load()) +
         " log_drops=" + std::to_string(g_logDrops.load()));
    const auto padStatus = Backend_GetStatus();
    Emit("analog_gamepad pub_while_connected=" +
         std::to_string(g_outputPublished.load()) +
         " active_while_connected=" +
         std::to_string(g_outputNonneutral.load()) +
         " rejected=" +
         std::to_string(g_outputRejected.load()) +
         " enabled=" + std::to_string(Backend_GetVirtualGamepadsEnabled()) +
         " vigem_ready=" + std::to_string(padStatus.vigemOk) +
         " vigem_error=" + std::to_string(static_cast<unsigned>(padStatus.lastVigemError)) +
         " requires_binding=1");
    Emit("trace_output candidates=" + std::to_string(g_outputCandidates.load()) +
         " active=" + std::to_string(g_outputCandidateActive.load()) +
         " changed=" + std::to_string(g_outputCandidateChanges.load()) +
         " due=" + std::to_string(g_outputCandidateDue.load()) +
         " published=" + std::to_string(g_outputPublished.load()) +
         " rejected=" + std::to_string(g_outputRejected.load()) +
         " candidate_pad_mask=" + std::to_string(g_outputCandidatePadMask.load()));
    Emit("analog_keyboard raw_registered=" +
         std::to_string(probe::rawInputAvailable.load()) +
         " matched=" + std::to_string(probe::rawMatched.load()) +
         " letter_down=" + std::to_string(probe::letterDown.load()) +
         " letter_up=" + std::to_string(probe::letterUp.load()) +
         " orphan_up=" + std::to_string(probe::letterOrphan.load()) +
         " peak_held=" + std::to_string(probe::peakLettersHeld.load()));
}

void RunSession(const Candidate& candidate) {
    ClearValues();
    BindFactoryMap();
    g_trace.Reset(GetTickCount64());
    for (auto& count : g_consumerByHid) count.store(0);
    for (auto& value : g_consumerMilliByHid) value.store(0);
    g_trace.lastConsumerPositive = g_consumedPositive.load();
    g_trace.lastOutput = g_outputPublished.load();
    g_trace.lastOutputActive = g_outputNonneutral.load();
    g_trace.lastCandidates = g_outputCandidates.load();
    g_trace.lastCandidateActive = g_outputCandidateActive.load();
    g_trace.lastRawDown = probe::letterDown.load();
    g_trace.lastRawUp = probe::letterUp.load();
    g_outputCandidatePadMask.store(0);
    probe::Show(probe::Status::Opening);
    probe::Handle device{CreateFileW(candidate.path.c_str(), GENERIC_READ | GENERIC_WRITE,
                        FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING,
                        FILE_FLAG_OVERLAPPED, nullptr)};
    if (device.h == INVALID_HANDLE_VALUE) {
        ++g_failed;
        Emit("analog_open_failed win32=" + std::to_string(GetLastError()));
        probe::Show(probe::Status::Fault);
        return;
    }
    const bool buffered = HidD_SetNumInputBuffers(device.h, 256) != FALSE;
    Emit("analog_open report_bytes=65 buffers_256=" + std::to_string(buffered) +
         " factory_mapped=103 calculator_unmapped=1");
    probe::digitalEnabled.store(true, std::memory_order_release);
    probe::StreamStats stats{};
    bool firstOff = false, on = false, finalOff = false, finalAck = false;
    unsigned endReason = 0; // 0=Pause/exit, 1=ON write, 2=read, 3=exception
    std::uint64_t lastPacket = 0, maxGapMs = 0, idleStreak = 0;
    try {
        firstOff = probe::WriteMode(device.h, candidate.outputBytes, 0x67);
        Emit("batch68_disabled reason=physical_log44_echo_only calibration=off");
        on = !g_stop.load() && endReason != 2 &&
             probe::WriteMode(device.h, candidate.outputBytes, 0x66);
        Emit("analog_mode initial_off=" + std::to_string(firstOff) +
             " on_write=" + std::to_string(on));
        if (!on && endReason == 0) endReason = 1;
        std::vector<std::uint8_t> report(candidate.inputBytes);
        while (on && !g_stop.load()) {
            DWORD got = 0;
            const auto io = probe::ReadMaybe(device.h, report, got, 250);
            if (io == 2) { ++g_failed; endReason = 2; break; }
            if (io == 1) {
                ++stats.idle_reads;
                if (probe::Milestone(++idleStreak))
                    Emit("analog_idle reads=" + std::to_string(idleStreak) +
                         " samples=" + std::to_string(stats.samples) +
                         " on_ack=" + std::to_string(stats.ack_on));
                TraceWindow(GetTickCount64());
                continue;
            }
            idleStreak = 0;
            const auto now = GetTickCount64();
            if (lastPacket && now >= lastPacket)
                maxGapMs = (std::max)(maxGapMs, now - lastPacket);
            lastPacket = now;
            const auto rawHeld = probe::rawHeldState.load(std::memory_order_acquire);
            const auto kind = stats.Observe(report, got, true, rawHeld);
            if (kind == probe::FrameKind::AckOn && stats.ack_on == 1)
            {
                Emit("analog_on_ack=1");
                Emit("unknown_trace begin=1 selected_stream=reference_only gamepad_source=disabled raw_digital=reference_only");
                probe::Show(probe::Status::UnknownTrace);
            }
            if (kind == probe::FrameKind::Sample) {
                protocol::TravelSample sample{};
                if (!protocol::ParseTravel(report.data(), got, sample)) {
                    ++g_failed; continue;
                }
                g_trace.Observe(sample, now,
                    static_cast<std::uint32_t>(rawHeld) & ((1u << 26) - 1u));
                if (sample.zeroMaximumPositive) {
                    ++g_failed; // Invalid denominator: never fabricate depth.
                } else if (sample.mapped) {
                    // Keep the selected-key stream as evidence. It omits held
                    // keys and cannot supply a reliable independent gamepad.
                    if (sample.maximum) g_lastMaximum.store(sample.maximum);
                }
                if (probe::Milestone(stats.samples))
                    Emit("analog_checkpoint samples=" + std::to_string(stats.samples) +
                         " indices=" + std::to_string(stats.unique) +
                         " positive=" + std::to_string(stats.unique_positive) +
                         " published=" + std::to_string(g_published.load()) +
                         " consumer_positive=" +
                         std::to_string(g_consumedPositive.load()) +
                         " gamepad_active=" +
                         std::to_string(g_outputNonneutral.load()));
            } else if (kind == probe::FrameKind::Invalid) {
                ++g_failed;
                if (probe::Milestone(stats.malformed + stats.out_of_catalog))
                    Emit("analog_invalid malformed=" + std::to_string(stats.malformed) +
                         " out_of_catalog=" + std::to_string(stats.out_of_catalog));
            } else if (kind == probe::FrameKind::Other) {
                if (g_trace.ObserveOther(report, got, rawHeld))
                    LogChangedUnknown(report, got, rawHeld, now);
                if (probe::Milestone(stats.unrelated))
                    Emit("analog_unrelated reports=" + std::to_string(stats.unrelated) +
                         " bad_prefix=" + std::to_string(stats.bad_prefix));
            }
            TraceWindow(now);
        }
    } catch (...) {
        ++g_failed; endReason = 3;
        SupportLog_Event("alumix104.session_exception", 1);
    }
    TraceFinal(GetTickCount64());
    Emit("unknown_trace end=1");
    ClearValues(); // Release all gamepad keys before the OFF transaction.
    probe::digitalEnabled.store(false, std::memory_order_release);
    Emit("analog_cleanup begin=1 reason=" + std::to_string(endReason));
    try {
        finalOff = probe::WriteMode(device.h, candidate.outputBytes, 0x67);
        if (!finalOff) finalOff = probe::WriteMode(device.h, candidate.outputBytes, 0x67);
        if (finalOff) {
            const auto before = stats.ack_off;
            std::vector<std::uint8_t> report(candidate.inputBytes);
            for (unsigned i = 0; i < 8 && stats.ack_off == before; ++i) {
                DWORD got = 0;
                const auto io = probe::ReadMaybe(device.h, report, got, 100);
                if (io == 2) { ++g_failed; break; }
                if (io == 0) stats.Observe(report, got, false);
            }
            finalAck = stats.ack_off > before;
        }
    } catch (...) {
        ++g_failed; SupportLog_Event("alumix104.cleanup_exception", 1);
    }
    if (!finalOff || !finalAck) ++g_failed;
    RecordSummary(stats, firstOff, on, finalOff, finalAck, endReason, maxGapMs);
    SupportLog_RequestSnapshot();
    if (!g_stop.load()) probe::Show(probe::Status::Fault);
}

DWORD WINAPI Worker(void*) noexcept {
    try {
        while (!g_stop.load()) {
            const auto found = FindExactDevice();
            g_present.store(!found.accepted.empty());
            if (found.vidpid) {
                Emit("HallJoy RedSquare unknown packet trace v2; exact Alumix104 only");
                Emit("trace_schema=2 slots=opaque windows=readiness_driven raw_digital=reference_only depth_from_sensor_only=1 action_bits=axis_minus0-3,axis_plus4-7,triggers8-9,buttons10-24");
                Emit("trace_slot_fields=slot,source_reports,exact_repeats,zero_reports,age_ms,last_positive,consumer_reads,milli_bucket,bound_pad_mask,digital_reference");
                Emit("unknown_packet_schema=2 changed_packets_full_hex=1 limit=512 exact_ff68_only=1 selected_reference=1 may_encode_key_state=1 no_transcribed_text=1");
                for (const auto& line : found.collectionEvidence) Emit(line);
                Emit("analog_admission vidpid=" + std::to_string(found.vidpid) +
                     " product_rejected=" + std::to_string(found.rejectedProduct) +
                     " collection_rejected=" +
                     std::to_string(found.rejectedCollection) +
                     " matched=" + std::to_string(found.accepted.size()));
            }
            if (found.accepted.size() == 1) {
                g_inputBytes.store(found.accepted[0].inputBytes);
                g_outputBytes.store(found.accepted[0].outputBytes);
                RunSession(found.accepted[0]);
            } else if (found.accepted.size() > 1) {
                ++g_failed;
                Emit("analog_admission ambiguous=1");
                probe::Show(probe::Status::Fault);
            } else if (found.vidpid) {
                Emit("analog_admission exact_collection_missing=1");
                probe::Show(probe::Status::NotFound);
            }
            if (g_stop.load()) break;
            WaitForSingleObject(g_wake, found.accepted.empty() ? INFINITE : 1000);
        }
    } catch (...) {
        ++g_failed;
        SupportLog_Event("alumix104.worker_exception", 1);
        Emit("analog_worker_exception=1");
        probe::Show(probe::Status::Fault);
    }
    ClearValues();
    return 0;
}
bool Start() {
    std::lock_guard<std::mutex> lock(g_service);
    if (g_worker) return true;
    g_stop.store(false);
    g_present.store(false);
    g_published.store(0); g_failed.store(0); g_consumed.store(0);
    g_consumedPositive.store(0); g_logDrops.store(0);
    for (auto& count : g_consumerByHid) count.store(0);
    for (auto& value : g_consumerMilliByHid) value.store(0);
    g_outputCandidates.store(0); g_outputCandidateActive.store(0);
    g_outputCandidateChanges.store(0); g_outputCandidateDue.store(0);
    g_outputCandidatePadMask.store(0);
    g_outputPublished.store(0); g_outputNonneutral.store(0);
    g_outputRejected.store(0);
    probe::rawMatched.store(0); probe::letterDown.store(0);
    probe::letterUp.store(0); probe::letterOrphan.store(0);
    probe::peakLettersHeld.store(0); probe::rawHeldState.store(0);
    probe::rawHeldRevision.store(0); probe::rawSessionGeneration.fetch_add(1);
    BindFactoryMap();
    g_wake = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (!g_wake) return false;
    g_worker = CreateThread(nullptr, 0, Worker, nullptr, 0, nullptr);
    if (!g_worker) {
        CloseHandle(g_wake); g_wake = nullptr;
        return false;
    }
    return true;
}
halljoy::lifecycle::StopResult Stop(halljoy::lifecycle::GenerationId generation) {
    std::lock_guard<std::mutex> lock(g_service);
    g_stop.store(true);
    if (!g_worker) return NativeAnalogBackendStopJoined(generation);
    if (g_wake) SetEvent(g_wake);
    const auto wait = WaitForSingleObject(g_worker, 4000);
    if (wait != WAIT_OBJECT_0)
        return NativeAnalogBackendStopFailed(generation,
            halljoy::lifecycle::LifecycleErrorCode::StopTimedOut,
            wait == WAIT_TIMEOUT ? WAIT_TIMEOUT : GetLastError());
    CloseHandle(g_worker); g_worker = nullptr;
    CloseHandle(g_wake); g_wake = nullptr;
    ClearValues();
    return NativeAnalogBackendStopJoined(generation);
}
void Notify() {
    std::lock_guard<std::mutex> lock(g_service);
    if (g_wake) SetEvent(g_wake);
}
bool Present() { return g_present.load(); }
bool Connected() { return g_connected.load(); }
bool Owns(std::uint16_t hid) { return Connected() && g_depth.Owns(hid); }
std::uint16_t Get(std::uint16_t hid) {
    if (!Owns(hid)) return 0;
    ++g_consumed;
    const auto sample = g_depth.Read(hid, GetTickCount64(), 1000);
    if (sample.milli) ++g_consumedPositive;
    if (hid < g_consumerByHid.size()) {
        ++g_consumerByHid[hid];
        g_consumerMilliByHid[hid].store(sample.milli, std::memory_order_relaxed);
    }
    return sample.milli;
}
void Telemetry(NativeAnalogBackendTelemetry* out) {
    if (!out) return;
    *out = {};
    out->present = Present(); out->connected = Connected();
    out->vendorId = 0x0c45; out->productId = 0x80ac;
    out->usagePage = 0xff68; out->usage = 0x61;
    out->mappedKeys = Connected() ? 103 : 0;
    out->activeKeys = Connected() ? g_depth.Active(GetTickCount64()) : 0;
    out->inputReportBytes = g_inputBytes.load();
    out->outputReportBytes = g_outputBytes.load();
    out->nominalRawLevels = g_lastMaximum.load() * 10u + 1u;
    out->successfulUpdates = g_published.load();
    out->failedUpdates = g_failed.load();
    const auto last = g_lastSample.load(), now = GetTickCount64();
    out->lastUpdateAgeMs = last && now >= last
        ? static_cast<std::uint32_t>((std::min)(std::uint64_t(0xffffffff), now - last)) : 0;
    wcscpy_s(out->status, L"Alumix 104: read-only batch probe; gamepad source unconfirmed");
    wcscpy_s(out->deviceName, L"Alumix 104 Yotei Magnetic");
}
}

const NativeAnalogBackendDescriptor& Alumix104_GetNativeBackendDescriptor() {
    static const NativeAnalogBackendDescriptor descriptor{
        kNativeAnalogBackendAbiVersion, sizeof(NativeAnalogBackendDescriptor),
        "alumix104-yotei", L"Alumix 104 Yotei analog gamepad trial",
        NativeAnalogProtocol::Alumix104Yotei,
        NativeAnalogStartPhase::AfterRawInput,
        NativeAnalogBackendFlag_StreamTransport |
            NativeAnalogBackendFlag_ReversibleControlProbe |
            NativeAnalogBackendFlag_RequiresRawInput,
        nullptr, &Start, &Stop, &Notify, &Present, &Connected, &Owns, &Get,
        &Telemetry};
    return descriptor;
}

bool Alumix104_TrialConnected() noexcept { return g_connected.load(); }
void Alumix104_ObserveGamepadCandidate(unsigned nonneutralPadMask, bool changed,
                                       bool publishDue) noexcept {
    if (!g_connected.load(std::memory_order_acquire)) return;
    ++g_outputCandidates;
    if (nonneutralPadMask) ++g_outputCandidateActive;
    if (changed) ++g_outputCandidateChanges;
    if (publishDue) ++g_outputCandidateDue;
    g_outputCandidatePadMask.store(nonneutralPadMask, std::memory_order_relaxed);
}
void Alumix104_ObserveGamepadPublish(bool published, bool nonneutral) noexcept {
    if (!g_connected.load(std::memory_order_acquire)) return;
    if (published) {
        ++g_outputPublished;
        if (nonneutral) ++g_outputNonneutral;
    } else ++g_outputRejected;
}

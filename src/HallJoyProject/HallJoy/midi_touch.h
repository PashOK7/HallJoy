#pragma once
#include "midi_profile.h"
#include <algorithm>
#include <array>
#include <cstdint>

namespace halljoy::midi {
enum class TouchQuality : unsigned { None, CapturedMotion, SnapshotMotion, SparseInterval, UnresolvedTiming };
struct TouchReport {
    std::uint64_t attacks = 0, sparse = 0;
    unsigned speedMilli = 0, travelMilli = 0;
    std::uint64_t intervalUs = 0;
    TouchQuality quality = TouchQuality::None;
    unsigned velocity = 0;
};
struct MotionPoint {
    std::uint64_t timeUs = 0;
    std::uint16_t depth = 0;
    bool snapshot = false, timestampKnown = false;
};
struct MotionTrace {
    // Preserve the onset and the latest points of long strokes.
    static constexpr unsigned kHead = 4, kPoints = 16;
    std::array<MotionPoint, kPoints> points{};
    unsigned count = 0, observations = 0;
    int priorDepth = 0, anchorDepth = 0, startDepth = 0, triggerDepth = 0;
    std::uint64_t priorUs = 0, anchorUs = 0, startUs = 0, triggerUs = 0;
    bool measured = false, priorTimed = false, gap = false, reversed = false;
    int onsetDepth = 0, endDepth = 0;
    std::uint64_t onsetUs = 0, endUs = 0;
    bool onsetKnown = false, onsetSelected = false;
    void Add(int depth, std::uint64_t timeUs, bool snapshot, bool timestampKnown) noexcept {
        if(count) {
            const unsigned last = count<=kPoints ? count-1 : kHead+(count-1-kHead)%(kPoints-kHead);
            if(points[last].depth==depth && points[last].timestampKnown==timestampKnown) return;
        }
        const unsigned index = count<kPoints ? count : kHead+(count-kHead)%(kPoints-kHead);
        points[index] = {timeUs,static_cast<std::uint16_t>(depth),snapshot,timestampKnown};
        ++count;
    }
    MotionPoint Point(unsigned position) const noexcept {
        const unsigned skipped = count>kPoints ? count-kPoints : 0;
        const unsigned ordinal = position<kHead ? position : position+skipped;
        const unsigned index = ordinal<kPoints ? ordinal : kHead+(ordinal-kHead)%(kPoints-kHead);
        return points[index];
    }
};
struct AttackState {
    bool blocked = true, down = false, timed = false, moving = false;
    bool previousObserved = false, onsetKnown = false;
    int previous = 0, anchor = 0, onsetDepth = 0;
    std::uint64_t observedUs = 0, anchorUs = 0, onsetUs = 0;
    unsigned observations = 0;
    std::uint8_t velocity = 0;
    TouchReport last{};
};
struct Input {
    int milli = 0;
    bool available = false;
    bool snapshot = false;
    bool timestampKnown = true;
    bool acquiredUnchanged = false; // Real unchanged publication, never a consumer wake.
};
struct AttackContext {
    std::uint64_t source = 0, connection = 0, sequence = 0;
    std::uint16_t physical = 0, hid = 0, code = 0;
};
struct AttackSink {
    void* context = nullptr;
    void (*put)(void*,const AttackState&,const MotionTrace&,const Profile&,const AttackContext&) = nullptr;
    void operator()(const AttackState& key,const MotionTrace& trace,const Profile& config,const AttackContext& where) const noexcept {
        if(put) put(context,key,trace,config,where);
    }
};
inline std::uint8_t VelocityForSpeed(unsigned speedMilli, const Profile& config) noexcept {
    if(config.touchVersion==1)
        return static_cast<std::uint8_t>(1+(LegacyCurveMilli(speedMilli,config)*126+500)/1000);
    if(speedMilli<config.velocitySpeed.front())return 0;
    if(speedMilli>=config.velocitySpeed.back())
        return static_cast<std::uint8_t>((config.velocityCurve.back()*127+500)/1000);
    unsigned high=1;
    while(speedMilli>config.velocitySpeed[high])++high;
    const auto low=high-1,width=config.velocitySpeed[high]-config.velocitySpeed[low];
    const auto offset=speedMilli-config.velocitySpeed[low];
    const auto rise=static_cast<unsigned>(config.velocityCurve[high]-config.velocityCurve[low]);
    const auto level=static_cast<unsigned>(config.velocityCurve[low])+
        static_cast<unsigned>((std::uint64_t(rise)*offset+width/2)/width);
    return static_cast<std::uint8_t>((level*127+500)/1000);
}
inline void SeedReleased(AttackState& key, const Profile& config, int depth) noexcept {
    if (depth <= config.releaseMilli) {
        key.blocked = false;
        key.previous = key.anchor = depth;
    }
}
// Version 1 uses observed motion timestamps. Nothing here sleeps or reads a clock.
inline bool ObserveAttack(AttackState& key, const Profile& config, std::uint64_t nowUs,
                          Input input, std::uint64_t& sparseAttacks, MotionTrace* trace = nullptr) noexcept {
    if (!input.available || !ValidTouch(config)) { key = {}; if(trace)*trace={}; return false; }
    const int depth = std::clamp(input.milli, 0, 1000);
    if (key.timed && input.timestampKnown && nowUs < key.observedUs) {
        key = {};
        if(trace)*trace={};
        SeedReleased(key, config, depth);
        return false;
    }
    if (key.blocked) {
        if(trace)*trace={};
        if (depth > config.releaseMilli) return false;
        key.blocked = false;
        key.previous = key.anchor = depth;
    }
    if (key.down) {
        if (depth <= config.releaseMilli) {
            if(trace){
                *trace={};
                trace->Add(depth,nowUs,input.snapshot,input.timestampKnown);
            }
            key.down = false;
            key.moving = false;
            key.observations = 0;
            key.onsetKnown = false;
            key.previousObserved = input.acquiredUnchanged && input.timestampKnown;
            key.previous = key.anchor = depth;
            key.observedUs = key.anchorUs = nowUs;
            key.timed = input.timestampKnown;
        }
        return true;
    }
    if(trace)trace->Add(depth,nowUs,input.snapshot,input.timestampKnown);
    const bool rising = depth > key.previous;
    const bool significant = depth - key.previous >= std::max(1, config.noiseMilli);
    const bool reversed = key.previous - depth > config.noiseMilli;
    // 50 ms classifies an interrupted stroke after arrival, never defers an edge.
    const bool gap = key.timed && nowUs > key.observedUs && nowUs - key.observedUs > 50000;
    if (reversed || gap) {
        key.moving = false;
        key.observations = 0;
        key.onsetKnown = false;
    }
    const int priorDepth = key.previous;
    const auto priorUs = key.observedUs;
    const bool priorTimed = key.timed;
    if (significant) {
        if (!key.moving) {
            key.onsetDepth = priorDepth;
            key.onsetUs = priorUs;
            key.onsetKnown = key.previousObserved && priorTimed && !gap &&
                input.timestampKnown && nowUs > priorUs;
            key.anchor = depth;
            key.anchorUs = nowUs;
            key.moving = true;
            key.observations = 1;
        } else {
            ++key.observations;
        }
    }
    if (rising && depth >= config.attackMilli) {
        const bool measured = key.observations >= 2 && nowUs > key.anchorUs && input.timestampKnown;
        int start = measured ? key.anchor : priorDepth;
        auto startUs = measured ? key.anchorUs : priorUs;
        int end = depth;
        auto endUs = nowUs;
        // A first sample near the bottom can hide most of a fast stroke.
        // Use the wider observed interval to avoid estimating speed from only
        // a small, clipped terminal distance. The onset needs a genuine prior
        // acquisition timestamp, never a stale release or an assumed poll rate.
        bool onsetSelected = false;
        if(measured && key.onsetKnown) {
            const auto onsetTravel=static_cast<unsigned>(key.anchor-key.onsetDepth);
            const auto tailTravel=static_cast<unsigned>(depth-key.anchor);
            const auto onsetSpeed=std::uint64_t(onsetTravel)*1000000/(key.anchorUs-key.onsetUs);
            const auto tailSpeed=std::uint64_t(tailTravel)*1000000/(nowUs-key.anchorUs);
            onsetSelected=onsetTravel>tailTravel && onsetSpeed>tailSpeed;
        }
        if(onsetSelected) {
            start=key.onsetDepth;startUs=key.onsetUs;
            end=key.anchor;endUs=key.anchorUs;
        }
        const bool intervalKnown = input.timestampKnown && (measured || priorTimed) && endUs > startUs;
        const auto elapsed = intervalKnown ? endUs - startUs : std::uint64_t{1};
        const unsigned distance = static_cast<unsigned>(std::max(0, end - start));
        const auto speed = (std::uint64_t(distance) * 1000000 + elapsed / 2) / elapsed;
        if(trace){
            trace->priorDepth = priorDepth;
            trace->priorUs = priorUs;
            trace->priorTimed = priorTimed;
            trace->anchorDepth = key.anchor;
            trace->anchorUs = key.anchorUs;
            trace->startDepth = start;
            trace->startUs = intervalKnown ? startUs : 0;
            trace->triggerDepth = depth;
            trace->triggerUs = nowUs;
            trace->observations = key.observations;
            trace->measured = measured;
            trace->gap = gap;
            trace->reversed = reversed;
            trace->onsetDepth = key.onsetDepth;
            trace->onsetUs = key.onsetUs;
            trace->onsetKnown = key.onsetKnown;
            trace->onsetSelected = onsetSelected;
            trace->endDepth = end;
            trace->endUs = endUs;
        }
        key.last = {};
        key.last.speedMilli = static_cast<unsigned>(std::min<std::uint64_t>(speed, 1000000000));
        key.last.travelMilli = distance;
        key.last.intervalUs = intervalKnown ? elapsed : 0;
        key.last.quality = !intervalKnown ? TouchQuality::UnresolvedTiming :
            !measured || gap ? TouchQuality::SparseInterval :
            input.snapshot ? TouchQuality::SnapshotMotion : TouchQuality::CapturedMotion;
        if (key.last.quality >= TouchQuality::SparseInterval) ++sparseAttacks;
        key.velocity = VelocityForSpeed(key.last.speedMilli, config);
        key.last.velocity = key.velocity;
        key.down = true;
    }
    // Stationary consumer wakes never become motion points or reset an anchor.
    if (significant || reversed || !key.timed || depth <= config.releaseMilli) {
        key.previous = depth;
        key.observedUs = nowUs;
        key.timed = input.timestampKnown;
        key.previousObserved = input.acquiredUnchanged && input.timestampKnown;
    }
    return true;
}
inline void RecordTouch(TouchReport& total, const AttackState& key) noexcept {
    const auto attacks = total.attacks + 1;
    const auto sparse = total.sparse + (key.last.quality >= TouchQuality::SparseInterval);
    total = key.last;
    total.attacks = attacks;
    total.sparse = sparse;
}
}

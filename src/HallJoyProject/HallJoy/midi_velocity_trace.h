#pragma once
#include "midi_touch.h"
#include <atomic>
#include <cstdio>
#include <mutex>

namespace halljoy::midi {
enum class VelocityTraceKind : unsigned { Attack, Queued, Sent };
struct VelocityTraceRecord {
    std::uint64_t sequence = 0, epoch = 0, consumerUs = 0;
    VelocityTraceKind kind = VelocityTraceKind::Attack;
    AttackContext context{};
    MotionTrace motion{};
    TouchReport touch{};
    unsigned velocity = 0, result = 0, reference = 0, status = 0;
    int attack = 0, release = 0, noise = 0;
    std::array<int,5> curve{};
    std::array<unsigned,5> curveX{};
    unsigned curveVersion = 1;
};
// Both owners use a bounded try-lock. Formatting and log I/O run on the log worker.
class VelocityTrace {
public:
    static constexpr unsigned kCapacity = 256;
    struct Batch {
        std::array<VelocityTraceRecord,kCapacity> records{};
        unsigned count = 0;
        std::uint64_t overwritten = 0, contention = 0;
    };
private:
    std::mutex mutex_;
    std::array<VelocityTraceRecord,kCapacity> records_{};
    std::uint64_t written_ = 0, drained_ = 0;
    std::atomic<std::uint64_t> pending_{0}, contention_{0};
    std::uint64_t reportedContention_ = 0;
public:
    void Record(VelocityTraceRecord record) noexcept {
        std::unique_lock lock(mutex_,std::try_to_lock);
        if(!lock.owns_lock()) {contention_.fetch_add(1,std::memory_order_relaxed);return;}
        record.sequence=++written_;
        records_[(written_-1)%kCapacity]=record;
        pending_.store(written_,std::memory_order_release);
    }
    bool Pending() const noexcept {
        return pending_.load(std::memory_order_acquire)!=drained_ ||
            contention_.load(std::memory_order_relaxed)!=reportedContention_;
    }
    void Drain(Batch& batch) {
        std::lock_guard lock(mutex_);
        batch={};
        const auto first=written_>kCapacity ? written_-kCapacity : 0;
        batch.overwritten=first>drained_ ? first-drained_ : 0;
        for(auto n=std::max(first,drained_);n<written_;++n)
            batch.records[batch.count++]=records_[n%kCapacity];
        batch.contention=contention_.load(std::memory_order_relaxed);
        reportedContention_=batch.contention;
        drained_=written_;
    }
};
template<class Write> void WriteVelocityTrace(const VelocityTraceRecord& r,Write&& write) {
    char line[352]{};
    if(r.kind!=VelocityTraceKind::Attack) {
        std::snprintf(line,sizeof line,
            "evidence.midi.output id=%llu epoch=%llu stage=%s note=%u channel=%u status=%u velocity=%u result=%u observed_us=%llu",
            static_cast<unsigned long long>(r.sequence),static_cast<unsigned long long>(r.epoch),
            r.kind==VelocityTraceKind::Queued?"queued":"driver",
            (r.context.code-1)%128,(r.context.code-1)/128+1,r.status,r.velocity,r.result,
            static_cast<unsigned long long>(r.consumerUs));
        write(line);return;
    }
    const auto id=static_cast<unsigned long long>(r.sequence);
    std::snprintf(line,sizeof line,
        "evidence.midi.attack id=%llu epoch=%llu source=%llu connection=%llu input_seq=%llu physical=%u hid=%u note=%u channel=%u consumer_us=%llu trigger_us=%llu",
        id,static_cast<unsigned long long>(r.epoch),static_cast<unsigned long long>(r.context.source),
        static_cast<unsigned long long>(r.context.connection),static_cast<unsigned long long>(r.context.sequence),
        r.context.physical,r.context.hid,(r.context.code-1)%128,(r.context.code-1)/128+1,
        static_cast<unsigned long long>(r.consumerUs),static_cast<unsigned long long>(r.motion.triggerUs));
    write(line);
    std::snprintf(line,sizeof line,
        "evidence.midi.velocity id=%llu velocity=%u speed=%u travel=%u interval_us=%llu quality=%u samples=%u rising=%u full_speed=%u attack=%d release=%d noise=%d curve=%d,%d,%d,%d,%d",
        id,r.velocity,r.touch.speedMilli,r.touch.travelMilli,static_cast<unsigned long long>(r.touch.intervalUs),
        static_cast<unsigned>(r.touch.quality),r.motion.count,r.motion.observations,r.reference,r.attack,r.release,r.noise,
        r.curve[0],r.curve[1],r.curve[2],r.curve[3],r.curve[4]);
    write(line);
    std::snprintf(line,sizeof line,
        "evidence.midi.interval id=%llu start_depth=%d start_us=%llu prior_depth=%d prior_us=%llu prior_timed=%u anchor_depth=%d anchor_us=%llu trigger_depth=%d measured=%u gap=%u reversed=%u",
        id,r.motion.startDepth,static_cast<unsigned long long>(r.motion.startUs),r.motion.priorDepth,
        static_cast<unsigned long long>(r.motion.priorUs),r.motion.priorTimed?1u:0u,r.motion.anchorDepth,
        static_cast<unsigned long long>(r.motion.anchorUs),r.motion.triggerDepth,r.motion.measured?1u:0u,
        r.motion.gap?1u:0u,r.motion.reversed?1u:0u);
    write(line);
    std::snprintf(line,sizeof line,
        "evidence.midi.selection id=%llu version=2 onset_selected=%u onset_known=%u onset_depth=%d onset_us=%llu selected_end_depth=%d selected_end_us=%llu",
        id,r.motion.onsetSelected?1u:0u,r.motion.onsetKnown?1u:0u,r.motion.onsetDepth,
        static_cast<unsigned long long>(r.motion.onsetUs),r.motion.endDepth,
        static_cast<unsigned long long>(r.motion.endUs));
    write(line);
    if(r.curveVersion==2) {
        std::snprintf(line,sizeof line,"evidence.midi.curve id=%llu version=2 x=%u,%u,%u,%u,%u",
            id,r.curveX[0],r.curveX[1],r.curveX[2],r.curveX[3],r.curveX[4]);
        write(line);
    }
    const unsigned count=std::min(r.motion.count,MotionTrace::kPoints);
    for(unsigned offset=0;offset<count;offset+=4) {
        unsigned used=static_cast<unsigned>(std::snprintf(line,sizeof line,"evidence.midi.motion id=%llu first=%u ago_us_depth_flags=",id,offset));
        for(unsigned i=offset;i<std::min(offset+4,count);++i) {
            const auto point=r.motion.Point(i);
            const auto age=r.motion.triggerUs>=point.timeUs?r.motion.triggerUs-point.timeUs:0;
            const unsigned flags=(point.snapshot?1u:0u)|(point.timestampKnown?2u:0u);
            used+=static_cast<unsigned>(std::snprintf(line+used,sizeof line-used,"%s%llu/%u/%u",
                i==offset?"":",",static_cast<unsigned long long>(age),point.depth,flags));
        }
        write(line);
    }
}
}

#pragma once
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <mutex>

namespace halljoy::observation {
enum class Kind : std::uint8_t { Sample, Disconnect, Baseline, RetainSnapshot };
enum class Timing : std::uint8_t { HostPublication, HostSnapshot };
enum class KeySpace : std::uint8_t { Position, BindableCode };
struct Event {
    std::uint64_t source=0, connection=0, sequence=0, timeUs=0;
    std::uint16_t physical=0, hid=0, milli=0, previous=0;
    Kind kind=Kind::Sample;
    Timing timing=Timing::HostPublication;
    KeySpace keySpace=KeySpace::BindableCode;
    bool previousValid=false, available=true;
};
struct Sink {
    void* context=nullptr;
    void (*put)(void*,const Event&)=nullptr;
    void operator()(const Event& e) const { if(put) put(context,e); }
};
inline std::uint64_t NowUs() noexcept {
    return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count());
}
// Optional bounded multi-producer history. Acquisition and consumption never
// wait on one another. Full/bounded-contention failures are explicit loss.
class Journal {
    static constexpr std::size_t kCapacity=32768;
    struct Cell {std::atomic<std::uint64_t> sequence{0};Event event{};};
    std::array<Cell,kCapacity> events_{};
    alignas(64) std::atomic<std::uint64_t> head_{0};
    alignas(64) std::uint64_t tail_=0; // One consumer, never read by a producer.
    std::uint64_t discardBefore_=0;
public:
    std::atomic<std::uint64_t> lost{0}, malformed{0};
    Journal() {for(std::size_t i=0;i<kCapacity;++i)events_[i].sequence.store(i);}
    bool Push(Event event) noexcept {
        auto pos=head_.load(std::memory_order_relaxed);
        for(unsigned attempt=0;attempt<32;++attempt) {
            auto& cell=events_[pos%kCapacity];
            const auto sequence=cell.sequence.load(std::memory_order_acquire);
            if(sequence<pos) {++lost;return false;} // Full; unread data is never overwritten.
            if(sequence==pos && head_.compare_exchange_weak(pos,pos+1,std::memory_order_relaxed)) {
                event.sequence=pos+1;cell.event=event;
                cell.sequence.store(pos+1,std::memory_order_release);return true;
            }
            pos=head_.load(std::memory_order_relaxed);
        }
        ++lost;return false; // Bounded contention, never wait in keyboard acquisition.
    }
    std::size_t Drain(Event* output,std::size_t capacity) noexcept {
        std::size_t count=0;
        while(count<capacity) {
            auto& cell=events_[tail_%kCapacity];
            if(cell.sequence.load(std::memory_order_acquire)!=tail_+1) break;
            if(tail_+1>discardBefore_) output[count++]=cell.event;
            cell.sequence.store(tail_+kCapacity,std::memory_order_release);++tail_;
        }
        return count;
    }
    // Also fences a producer that reserved an old cell but has not committed it.
    void Discard() noexcept {discardBefore_=head_.load(std::memory_order_acquire);}
};
inline std::atomic<bool> enabled{false};
inline std::atomic<std::shared_ptr<Journal>> current;
inline std::atomic<void (*)()> wake{nullptr};
inline std::atomic<void (*)(bool)> captureChanged{nullptr};
inline void SetCapture(bool value) noexcept {
    enabled.store(value,std::memory_order_release);
    if(auto notify=captureChanged.load(std::memory_order_acquire)) notify(value);
}
inline std::atomic<std::uint64_t> nextSource{1};
inline void Record(Event event) noexcept {
    if(!enabled.load(std::memory_order_relaxed)) return;
    const auto journal=current.load(std::memory_order_acquire);
    if(!journal) return;
    if(event.kind==Kind::Sample && (event.milli>1000 || !event.hid || !event.physical)) {
        ++journal->malformed;return;
    }
    if(!event.timeUs) event.timeUs=NowUs();
    journal->Push(event);
    if(auto notify=wake.load(std::memory_order_acquire)) notify();
}
class Source {
    const std::uint64_t id_=nextSource.fetch_add(1,std::memory_order_relaxed);
    std::atomic<std::uint64_t> connection_{1};
    const KeySpace keySpace_;
public:
    Source() noexcept : Source(KeySpace::BindableCode) {}
    explicit Source(KeySpace keySpace) noexcept : keySpace_(keySpace) {}
    std::uint64_t Id() const noexcept {return id_;}
    std::uint64_t Connection() const noexcept {return connection_.load(std::memory_order_acquire);}
    void Clear() noexcept {
        const auto old=connection_.fetch_add(1,std::memory_order_acq_rel);
        if(!enabled.load(std::memory_order_relaxed)) return;
        Event e{};e.source=id_;e.connection=old;e.kind=Kind::Disconnect;Record(e);
    }
    void Publish(std::uint16_t physical,std::uint16_t hid,unsigned milli,unsigned previous,
                 bool previousValid=true,std::uint64_t timeUs=0,Timing timing=Timing::HostPublication,bool available=true,
                 std::uint64_t previousObservedUs=0) const noexcept {
        if(!enabled.load(std::memory_order_relaxed) || (milli==previous && previousValid==available)) return;
        Event e{};e.source=id_;e.connection=Connection();e.physical=physical;e.hid=hid;
        if(milli>1000 || previous>1000) {e.milli=1001;} else {e.milli=static_cast<std::uint16_t>(milli);e.previous=static_cast<std::uint16_t>(previous);}
        e.previousValid=previousValid;e.available=available;e.timeUs=timeUs;e.timing=timing;e.keySpace=keySpace_;
        // A polling source may have received unchanged values since its last edge.
        // Replay only the caller's genuinely acquired preceding sample, with its
        // original timestamp. No extra journal traffic occurs for unchanged frames.
        if(previousValid && available && previous<=1000 && previousObservedUs && previousObservedUs<timeUs) {
            auto prior=e;prior.milli=static_cast<std::uint16_t>(previous);
            prior.timeUs=previousObservedUs;Record(prior);
        }
        Record(e);
    }
    template<class T,class V> void Store(std::atomic<T>& cell,std::uint16_t hid,V value,
                                 std::memory_order order=std::memory_order_seq_cst) const noexcept {
        const bool capture=enabled.load(std::memory_order_relaxed);
        const auto before=capture ? cell.load(std::memory_order_relaxed) : T{};
        cell.store(static_cast<T>(value),order);
        if(capture) Publish(hid,hid,value,before);
    }
    template<class Owns,class Get> void Visit(Sink sink,Owns owns,Get get,unsigned count=0x410) const {
        for(unsigned hid=1;hid<count;++hid) if(owns(static_cast<std::uint16_t>(hid)))
            Baseline(sink,static_cast<std::uint16_t>(hid),static_cast<std::uint16_t>(hid),get(static_cast<std::uint16_t>(hid)));
    }
    void Baseline(Sink sink,std::uint16_t physical,std::uint16_t hid,unsigned milli,bool available=true,
                  Timing timing=Timing::HostPublication) const {
        Event e{};e.source=id_;e.connection=Connection();e.physical=physical;e.hid=hid;
        e.milli=static_cast<std::uint16_t>(milli);e.available=available;e.kind=Kind::Baseline;e.timing=timing;e.keySpace=keySpace_;sink(e);
    }
};
}

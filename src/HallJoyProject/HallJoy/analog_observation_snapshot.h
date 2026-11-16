#pragma once
#include "analog_observation.h"
#include <algorithm>
#include <bitset>
#include "analog_key_codes.h"
namespace halljoy::observation {
using SnapshotValues=std::array<std::uint16_t,halljoy::keycode::kCount>;
using SnapshotValidity=std::bitset<halljoy::keycode::kCount>;
// A source-qualified adapter for APIs that expose snapshots rather than sensor
// history. Gaps stay visible; snapshot timestamps never claim sensor precision.
class SnapshotTracker {
    static constexpr unsigned kDevices=16,kKeys=halljoy::keycode::kCount;
    struct Device {
        Source source;
        std::uint64_t identity=0,wireConnection=0;
        unsigned instance=0;
        bool online=false,seen=false;
        std::array<std::uint16_t,kKeys> values{};
        SnapshotValidity valid;
    };
    std::array<Device,kDevices> devices_{};
    std::mutex mutex_;
    std::uint64_t previousGeneration_=0,connection_=0;
public:
    std::atomic<std::uint64_t> gaps{0};
    std::atomic<unsigned> streamMode{0}; // 0 idle, 1 ordered IPC, 2 snapshot fallback, 3 starting.
    template<class Visit> void Frame(std::uint64_t connection,std::uint64_t generation,Visit visit) noexcept {
        std::unique_lock lock(mutex_,std::try_to_lock);
        if(!lock.owns_lock()) {++gaps;return;}
        if(connection_==connection && previousGeneration_ && generation>previousGeneration_+1) ++gaps;
        // A snapshot-only API can coalesce unchanged frames. Expose the quality
        // limit; do not turn every skipped snapshot into a fictitious disconnect.
        if(connection_!=connection || generation<previousGeneration_) {
            for(auto& device:devices_) {device.source.Clear();device.online=false;}
        }
        connection_=connection;previousGeneration_=generation;streamMode.store(2,std::memory_order_relaxed);
        for(auto& device:devices_) device.seen=false;
        visit([&](std::uint64_t identity,const std::array<std::uint16_t,kKeys>& values,std::uint64_t timeUs,unsigned instance=0,const SnapshotValidity* validity=nullptr) {
            if(!identity) return;
            Device* slot=nullptr;
            for(auto& device:devices_) if(device.identity==identity && device.instance==instance) {slot=&device;break;}
            if(!slot) for(auto& device:devices_) if(!device.online) {slot=&device;break;}
            if(!slot) {++gaps;return;}
            if(slot->identity!=identity || slot->instance!=instance) {slot->source.Clear();slot->identity=identity;slot->instance=instance;slot->online=false;}
            const bool prior=slot->online;
            for(unsigned hid=1;hid<kKeys;++hid) {
                const bool valid=validity ? validity->test(hid) : true;
                slot->source.Publish(static_cast<std::uint16_t>(hid),static_cast<std::uint16_t>(hid),values[hid],
                    prior?slot->values[hid]:values[hid],prior && slot->valid.test(hid),timeUs,Timing::HostSnapshot,valid);
            }
            slot->values=values;
            if(validity) slot->valid=*validity; else slot->valid.set();
            slot->online=true;slot->seen=true;
        });
        for(auto& device:devices_) if(device.online && !device.seen) {device.source.Clear();device.online=false;}
    }
    // SPSC IPC is drained only after acquiring the tracker. A busy baseline
    // reader leaves unread wire records intact; it never turns contention into
    // an unreported dropped observation.
    template<class Drain> bool ImportStream(std::uint64_t connection,Drain drain) {
        std::unique_lock lock(mutex_,std::try_to_lock);
        if(!lock.owns_lock()) return false;
        if(connection_!=connection) {
            for(auto& d:devices_) {d.source.Clear();d.online=false;d.identity=0;}
            connection_=connection;previousGeneration_=0;
        }
        streamMode.store(1,std::memory_order_relaxed);
        drain([&](Event e) {
            if(!e.source || !e.connection || (e.kind!=Kind::Disconnect &&
               (e.kind!=Kind::Sample || !e.hid || e.hid>=kKeys || e.physical!=e.hid || e.milli>1000))) {
                ++gaps;if(auto j=current.load()) ++j->malformed;return;
            }
            Device* slot=nullptr;
            for(auto& d:devices_) if(d.identity==e.source && d.wireConnection==e.connection) {slot=&d;break;}
            if(e.kind==Kind::Disconnect) {
                if(slot && slot->online) {slot->source.Clear();slot->online=false;}
                return;
            }
            if(!slot) for(auto& d:devices_) if(!d.online) {slot=&d;break;}
            if(!slot) {++gaps;if(auto j=current.load()) ++j->lost;return;}
            if(!slot->online) {
                slot->source.Clear();slot->identity=e.source;slot->wireConnection=e.connection;
                slot->instance=0;slot->values.fill(0);slot->valid.reset();slot->online=true;
            }
            slot->values[e.hid]=e.milli;slot->valid.set(e.hid,e.available);
            e.source=slot->source.Id();e.connection=slot->source.Connection();Record(e);
        });
        return true;
    }
    bool ResetStream() {
        std::unique_lock lock(mutex_,std::try_to_lock);
        if(!lock.owns_lock()) return false;
        for(auto& d:devices_) {d.source.Clear();d.online=false;d.identity=0;}
        connection_=0;previousGeneration_=0;streamMode.store(0,std::memory_order_relaxed);return true;
    }
    bool Visit(Sink sink) {
        std::unique_lock lock(mutex_,std::try_to_lock);
        if(!lock.owns_lock()) {
            Event retain{};retain.kind=Kind::RetainSnapshot;retain.timing=Timing::HostSnapshot;sink(retain);
            return false;
        }
        for(const auto& device:devices_) if(device.online)
            for(unsigned hid=1;hid<kKeys;++hid) device.source.Baseline(sink,static_cast<std::uint16_t>(hid),
                static_cast<std::uint16_t>(hid),device.values[hid],device.valid.test(hid),Timing::HostSnapshot);
        return true;
    }
};
inline std::atomic<std::shared_ptr<SnapshotTracker>> uap;
}

#pragma once
#include <cstdint>
#include <mutex>
namespace halljoy::telemetry {
struct Owner {
    std::uint64_t session=0;
    std::uint32_t process=0,restart=0;
    bool operator==(const Owner&) const = default;
};
// UI metadata only. Never used to retain live analog samples or output state.
// A failed read is unavailable evidence, not an observed empty inventory.
template<class Snapshot> class CoherentCache {
    std::mutex mutex_;
    Owner owner_{};
    Snapshot value_{};
    std::uint64_t acceptedMs_=0;
    bool valid_=false;
public:
    bool Resolve(Owner owner,bool healthy,bool captured,const Snapshot& candidate,
        std::uint64_t now,Snapshot& out,bool& reused) {
        std::lock_guard<std::mutex> lock(mutex_);
        reused=false;
        if(!healthy || owner!=owner_) {valid_=false;owner_=owner;}
        if(!healthy) {out={};return false;}
        const bool accept=captured && (!valid_ || candidate.generation>=value_.generation);
        if(accept) {value_=candidate;acceptedMs_=now;valid_=true;}
        if(!valid_ || now<acceptedMs_ || now-acceptedMs_>1000) {out={};return false;}
        out=value_;reused=!accept;return true;
    }
};
}

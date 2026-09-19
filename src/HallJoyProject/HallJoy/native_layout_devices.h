#pragma once
#include <cstddef>
#include <cstdint>
#include <array>
#include <algorithm>

namespace halljoy::layout_devices {
class Containers {
    std::array<std::array<unsigned char,16>,2> ids_{};
    int count_=0;
    bool missing_=false;
public:
    void Missing() noexcept {missing_=true;}
    void Add(const std::array<unsigned char,16>& id) noexcept {
        if(std::all_of(id.begin(),id.end(),[](unsigned char b){return !b;})) {Missing();return;}
        for(int i=0;i<count_;++i) if(ids_[i]==id) return;
        if(count_<2) ids_[count_++]=id;
    }
    int Result(bool complete) const noexcept {
        return count_>1 ? 2 : complete && !missing_ ? count_ : -2;
    }
};
}

// Metadata-only physical-device count for active native VID/PID identities.
// Query queues bounded background work and returns cached results:
// -1 pending, -2 incomplete metadata, >=0 distinct physical containers.
int NativeLayoutDevices_Query(const std::uint32_t* identities, std::size_t count);
void NativeLayoutDevices_Invalidate() noexcept;

// Cached USB product metadata, refreshed only after device-topology changes.
unsigned NativeLayoutDevices_QueryFrozen();

#pragma once
#include "irok_nd75_protocol.h"
#include <algorithm>
#include <array>
#include <cstring>

namespace irok_na87 {
using irok_nd75::Report;
using irok_nd75::PositionToHid;
inline bool IsExpectedDevice(const irok_nd75::DeviceInfo& info) noexcept {
    return std::strcmp(info.controller.data(), "M484") == 0 &&
        std::strcmp(info.product.data(), "GK8260HERGB") == 0;
}
inline Report MapRequest() noexcept { Report r{}; r[0]=1; r[1]=0x10; return r; }

// The final row uses the terminal marker FF:FF, not a seventh row.
class MapReader {
    PositionToHid map_{};
    unsigned rows_ = 0;
    bool conflict_ = false;
public:
    bool Feed(const Report& r) noexcept {
        if (r[0]!=1 || r[1]!=0x10 || r[2]!=0 || r[5]!=22) return false;
        unsigned row=6;
        if (r[3]==0 && r[4]>=1 && r[4]<=5) row=r[4]-1;
        else if (r[3]==255 && r[4]==255) row=5;
        if (row>=6) return false;
        auto begin=map_.begin()+row*22;
        if ((rows_ & (1u<<row)) && !std::equal(begin,begin+22,r.begin()+6))
            conflict_=true;
        std::copy_n(r.begin()+6,22,begin); rows_|=1u<<row;
        return true;
    }
    bool Complete() const noexcept {
        if (rows_!=63 || conflict_) return false;
        std::array<bool,256> seen{}; unsigned count=0;
        for (auto hid:map_) {
            if (!hid) continue;
            if (hid<4 || (hid>0xe7 && hid!=0xfa) || seen[hid]) return false;
            seen[hid]=true; ++count;
        }
        return count>=60 && count<=110;
    }
    const PositionToHid& Map() const noexcept { return map_; }
};

// Preserve other keys when an event updates one matrix position. Idle time is
// not a release: firmware may remain silent while a key is held stationary.
inline bool DecodeSample(const Report& r, const PositionToHid& map,
    std::uint8_t& hid, std::uint16_t& milli) noexcept {
    if (r[2] || r[3] || r[4] || r[5]!=3) return false;
    irok_nd75::LiveEvent e{};
    if (!irok_nd75::DecodeLiveEvent(r.data(),r.size(),&e) ||
        !irok_nd75::TryTravelToMilli(e.travel,&milli)) return false;
    hid=map[std::size_t(e.row)*22+e.column];
    return hid!=0;
}
}

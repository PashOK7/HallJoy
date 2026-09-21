#pragma once
#include <array>
#include <algorithm>
#include <cstdint>

namespace halljoy::ajazz {
// Observed SG8994HERGB V1.13.17 physical sensors, not the larger firmware map.
constexpr std::array<unsigned,6> sensorMask{{0xffbd,0xdfff,0xdfff,0xdffd,0xeffd,0xfc47}};
inline bool Physical(unsigned p) {return p<132 && (sensorMask[p/22]&(1u<<(p%22)));}
inline unsigned HostCode(unsigned code) {return code==250?1033:code;}

// Preliminary linear sensor-range normalization, explicitly NOT calibrated mm.
// Start released; each key independently expands its observed limits.
class DynamicLimits {
    struct Key {
        std::array<unsigned,3> recent{};
        unsigned samples=0,next=0,rest=0,full=0,expansions=0;
    };
    std::array<Key,132> keys_{};
    static unsigned InitialFull(unsigned p) {
        // Log25 minima for keys that reported depth40; other keys use their
        // median1353 as an explicitly provisional seed, never a factory value.
        switch(p) {
        case 46: return 1353;
        case 68: return 1353;
        case 69: return 1355;
        case 70: return 1366;
        case 71: return 1431;
        case 72: return 1341;
        case 90: return 1345;
        case 91: return 1317;
        case 92: return 1293;
        case 93: return 1382;
        case 94: return 1301;
        case 121: return 1289;
        default:return 1353;
        }
    }
public:
    void Event(unsigned,unsigned,std::uint64_t) {} // Event stream is diagnostic only.
    bool Raw(unsigned p,unsigned raw,std::uint64_t,unsigned& milli) {
        milli=0;
        if(!Physical(p) || !raw || raw>65535) return false;
        auto& k=keys_[p];
        if(!k.samples) {
            k.recent.fill(raw);k.rest=raw;k.full=InitialFull(p);
        }
        k.recent[k.next]=raw;k.next=(k.next+1)%3;++k.samples;
        auto a=k.recent;std::sort(a.begin(),a.end());const auto v=a[1];
        // A three-sample median excludes isolated USB/sensor spikes. At the
        // observed1kHz row rate this adds about one sample of transition delay.
        if(v>k.rest) {k.rest=v;++k.expansions;}
        if(v<k.full) {k.full=v;++k.expansions;}
        // Startup with a held key must not amplify a tiny noise-only span.
        if(k.rest<=k.full+32) return false;
        milli=v>=k.rest?0:v<=k.full?1000:
            (k.rest-v)*1000/(k.rest-k.full);
        return true;
    }
    struct Status {bool ready;unsigned samples,expansions,rest,full;};
    Status Inspect(unsigned p) const {
        const auto& k=keys_[p];return {k.samples && k.rest>k.full+32,k.samples,k.expansions,k.rest,k.full};
    }
    unsigned Ready() const {
        unsigned n=0;for(unsigned p=0;p<132;++p) n+=Inspect(p).ready;return n;
    }
};
}

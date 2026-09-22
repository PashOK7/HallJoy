#pragma once
#include <array>
#include <algorithm>
#include <cstdint>
#include <span>
namespace halljoy::mini60diag {
// Only commands justified by the archived SDK and the inspected 80A2/80A1 images.
inline std::array<std::uint8_t,65> Request(unsigned command,unsigned length=0,unsigned offset=0) {
    std::array<std::uint8_t,65> r{};
    if (!((command==0x10 && length==56 && offset==0) ||
          (command==0x12 && length>0 && length<=56 && offset+length<=512) ||
          ((command==0x66 || command==0x67) && length==0 && offset==0))) return r;
    r[1]=0xaa;r[2]=static_cast<std::uint8_t>(command);r[3]=static_cast<std::uint8_t>(length);
    r[4]=static_cast<std::uint8_t>(offset);r[5]=static_cast<std::uint8_t>(offset>>8);
    r[7]=(command==0x12 && offset+length<512)?0:1;return r;
}
inline unsigned U16(const std::uint8_t* p){return p[0] | (unsigned(p[1])<<8);}
inline std::span<const std::uint8_t> Payload(std::span<const std::uint8_t> r){
    if(r.size()==65 && r[0]==0)r=r.subspan(1);
    if(r.size()!=64 || r[0]!=0x55)return {};
    return r;
}
struct Sample {unsigned key=0,status=0,high=0,low=0,adc=0,travel=0,stroke=0;};
inline bool Decode(std::span<const std::uint8_t> r,Sample& s){
    r=Payload(r);if(r.empty() || r[1]!=0xfb || r[2]>=126 || r[3]>1)return false;
    s={r[2],r[3],U16(r.data()+4),U16(r.data()+6)&0x7fff,U16(r.data()+8),U16(r.data()+10),U16(r.data()+12)};
    return true; // Preserve unexpected ranges as evidence, without normalising them.
}
inline bool Reply(std::span<const std::uint8_t> p,unsigned c,unsigned n,unsigned offset){
    return p.size()==64 && p[0]==0x55 && p[1]==c && p[2]==n && U16(p.data()+3)==offset;
}
struct Key {
    std::uint64_t count=0,zero=0,increase=0,decrease=0,equal=0,gap50=0,gap250=0,last=0,maxGap=0;
    unsigned minTravel=65535,maxTravel=0,minAdc=65535,maxAdc=0,previous=0;
    unsigned minHigh=65535,maxHigh=0,minLow=65535,maxLow=0,minStroke=65535,maxStroke=0,statusMask=0;
    void Add(const Sample& s,std::uint64_t now){
        if(count){auto gap=now-last;maxGap=std::max(maxGap,gap);gap50+=gap>50;gap250+=gap>250;
            increase+=s.travel>previous;decrease+=s.travel<previous;equal+=s.travel==previous;}
        ++count;zero+=s.travel==0;previous=s.travel;last=now;
        minTravel=std::min(minTravel,s.travel);maxTravel=std::max(maxTravel,s.travel);
        minAdc=std::min(minAdc,s.adc);maxAdc=std::max(maxAdc,s.adc);
        minHigh=std::min(minHigh,s.high);maxHigh=std::max(maxHigh,s.high);
        minLow=std::min(minLow,s.low);maxLow=std::max(maxLow,s.low);
        minStroke=std::min(minStroke,s.stroke);maxStroke=std::max(maxStroke,s.stroke);statusMask|=1u<<s.status;
    }
};
struct Metrics {
    std::array<Key,126> keys{};
    std::array<std::uint64_t,4> phases{};
    std::uint64_t packets=0,unknown=0,malformed=0;unsigned recentKeysMax=0;
    bool Add(std::span<const std::uint8_t> r,std::uint64_t now,unsigned phase){
        Sample s{};if(!Decode(r,s)){auto p=Payload(r);if(p.empty() || p[1]==0xfb)++malformed;else ++unknown;return false;}
        keys[s.key].Add(s,now);++packets;++phases[std::min(phase,3u)];unsigned recent=0;
        for(const auto& k:keys)if(k.count && now-k.last<=50)++recent;
        recentKeysMax=std::max(recentKeysMax,recent);return true;
    }
    unsigned Seen()const{unsigned n=0;for(const auto& k:keys)n+=k.count!=0;return n;}
    unsigned Varying()const{unsigned n=0;for(const auto& k:keys)n+=k.count && k.maxTravel>k.minTravel;return n;}
};
struct Coverage {
    unsigned phase=0;
    bool complete=false;
    std::uint64_t allReleasedAt=0;
    void Update(unsigned keys,unsigned varying,unsigned pressed,unsigned released,unsigned held,
                unsigned stableHeldMs,unsigned chordPackets,unsigned chordKeys,unsigned chordVarying,std::uint64_t now){
        if(phase==0 && keys>=8 && varying>=4 && pressed>=8 && released>=8)phase=1;
        if(phase==1 && held>=2 && stableHeldMs>=600 && chordPackets>=10 && chordKeys>=2 && chordVarying>=1)phase=2;
        if(phase==2){
            if(held==0){if(!allReleasedAt)allReleasedAt=now;phase=3;}
        }
        if(phase==3){
            if(held){phase=2;allReleasedAt=0;}
            else if(now-allReleasedAt>=1000)complete=true;
        }
    }
};
inline bool Enough(unsigned keys,unsigned varying,unsigned presses,unsigned releases,unsigned held,bool clean){
    return clean && keys>=8 && varying>=4 && presses>=8 && releases>=8 && held>=2;
}
}

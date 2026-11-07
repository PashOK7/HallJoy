#pragma once
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstddef>
#include <vector>
#include "generated/corsair_vanguard_data.h"

namespace halljoy::corsair_vanguard {
constexpr std::uint16_t kVid=0x1b1c, kPid=0x2b0e;
constexpr std::size_t kPayloadBytes=1024, kReportBytes=1025;
using Frame=std::array<std::uint8_t,kPayloadBytes>;
enum class ReplyKind { Normal, Calibration };
enum class Stage : unsigned { Idle, Open, Session, Identity, Firmware, Layout,
    Directory, Calibration, SwitchSettings, Sampling, Cleanup };
enum class Reject { None, Transport, ReplyPrefix, SessionEcho, Vendor, Product,
    Firmware, Bragi, Layout, FileSize, DirectoryFormat, DirectoryChanged,
    CalibrationTail, CalibrationRange, CalibrationChanged, CalibrationEndpoints,
    CalibrationSpan, Curve, SwitchFormat, SwitchRecord, SnapshotTail,
    SnapshotZero, SnapshotSentinel };
inline const char* Reason(Reject r) noexcept {
    switch(r) {
#define CV_REASON(name,text) case Reject::name:return text;
    CV_REASON(None,"none") CV_REASON(Transport,"transport") CV_REASON(ReplyPrefix,"reply_prefix")
    CV_REASON(SessionEcho,"session_echo") CV_REASON(Vendor,"vendor_mismatch") CV_REASON(Product,"product_mismatch")
    CV_REASON(Firmware,"firmware_not_reviewed") CV_REASON(Bragi,"bragi_version") CV_REASON(Layout,"layout_not_mapped")
    CV_REASON(FileSize,"file_size") CV_REASON(DirectoryFormat,"directory_format") CV_REASON(DirectoryChanged,"directory_changed")
    CV_REASON(CalibrationTail,"calibration_tail") CV_REASON(CalibrationRange,"calibration_word_range")
    CV_REASON(CalibrationChanged,"calibration_changed") CV_REASON(CalibrationEndpoints,"calibration_endpoints")
    CV_REASON(CalibrationSpan,"calibration_span") CV_REASON(Curve,"curve_index")
    CV_REASON(SwitchFormat,"switch_file_format") CV_REASON(SwitchRecord,"switch_record")
    CV_REASON(SnapshotTail,"snapshot_tail") CV_REASON(SnapshotZero,"snapshot_zero_sensor")
    CV_REASON(SnapshotSentinel,"snapshot_sensor_out_of_range")
#undef CV_REASON
    }
    return "unknown";
}
inline const char* StageName(Stage s) noexcept {
    switch(s) {
#define CV_STAGE(name,text) case Stage::name:return text;
    CV_STAGE(Idle,"idle") CV_STAGE(Open,"open") CV_STAGE(Session,"session")
    CV_STAGE(Identity,"identity") CV_STAGE(Firmware,"firmware") CV_STAGE(Layout,"layout")
    CV_STAGE(Directory,"directory") CV_STAGE(Calibration,"calibration")
    CV_STAGE(SwitchSettings,"switch_settings") CV_STAGE(Sampling,"sampling") CV_STAGE(Cleanup,"cleanup")
#undef CV_STAGE
    }
    return "unknown";
}
inline bool Rejected(Reject* out,Reject reason) noexcept {if(out)*out=reason;return false;}
inline std::uint16_t U16(const std::uint8_t* p) noexcept {return p[0]|std::uint16_t(p[1])<<8;}
inline std::uint32_t U32(const std::uint8_t* p) noexcept {return U16(p)|std::uint32_t(U16(p+2))<<16;}
inline void Put32(std::uint8_t* p,std::uint32_t n) noexcept {
    for(unsigned i=0;i<4;++i)p[i]=static_cast<std::uint8_t>(n>>(i*8));
}
inline Frame Request(std::uint8_t session,std::uint8_t command) noexcept {
    Frame f{};f[1]=1;f[2]=session;f[3]=command;return f;
}
inline bool Prefix(const Frame& request,const Frame& reply,ReplyKind kind) noexcept {
    if(reply[0]!=0 || reply[1]!=0 || reply[2]!=request[2])return false;
    return kind==ReplyKind::Calibration || (reply[3]==request[3] && reply[4]==0);
}
inline bool ZeroTail(const Frame& f,std::size_t begin) noexcept {
    return begin<=f.size() && std::all_of(f.begin()+begin,f.end(),[](auto v){return v==0;});
}
struct Calibration {std::uint16_t rest=0,bottom=0;std::uint8_t scale=0,curve=0;};
struct Profile {
    const Layout* layout=nullptr;
    std::array<Calibration,100> calibration{};
    unsigned mapped=0;
};
inline bool Directory(const std::vector<std::uint8_t>& data,bool& saved,bool& switches) {
    saved=switches=false;
    if(data.size()<4 || U16(data.data())!=0xab5b)return false;
    const unsigned count=U16(data.data()+2);
    if(count>4000 || data.size()!=4+count*2)return false;
    std::vector<std::uint16_t> ids;
    for(unsigned i=0;i<count;++i) {
        auto id=U16(data.data()+4+i*2);
        if(!id || id==0xffff || std::find(ids.begin(),ids.end(),id)!=ids.end())return false;
        ids.push_back(id);saved|=id==0xca;switches|=id==0x4b;
    }
    return true;
}
inline bool CalibrationWords(const Frame& reply,unsigned count,std::array<std::uint16_t,100>& words,Reject* why=nullptr) noexcept {
    if(count>words.size() || !ZeroTail(reply,3+count*2))return Rejected(why,Reject::CalibrationTail);
    for(unsigned i=0;i<count;++i) {
        words[i]=U16(reply.data()+3+2*i);
        if(words[i]>0x3fff)return Rejected(why,Reject::CalibrationRange);
    }
    return true;
}
inline bool BuildProfile(const Layout& layout,bool saved,
    const std::array<std::uint16_t,100>& rest,const std::array<std::uint16_t,100>& bottom,
    const std::vector<std::uint8_t>* switches,Profile& out,Reject* why=nullptr) noexcept {
    Profile candidate{};candidate.layout=&layout;
    for(unsigned i=0;i<layout.count;++i) {
        auto& c=candidate.calibration[i];c.rest=rest[i];c.bottom=bottom[i];c.curve=layout.factoryCurve[i];
        if(!saved && i==6) {
            if(c.bottom<200)return Rejected(why,Reject::CalibrationEndpoints);
            c.bottom-=200;
        }
    }
    if(switches) {
        const auto& file=*switches;
        if(file.size()<3 || U16(file.data())!=0x67 || file[2]>139 || file.size()!=3+std::size_t(file[2])*4)return Rejected(why,Reject::SwitchFormat);
        for(unsigned i=0;i<file[2];++i) {
            const auto* record=file.data()+3+i*4;
            const unsigned code=record[0],type=record[1];
            if(code<4 || code>=139 || type>=6 || record[2]>40)return Rejected(why,Reject::SwitchRecord);
            const auto sensor=layout.sensor[code];
            // Process even absent-key aliases of sensor0, in firmware order.
            // Out-of-report slots never contribute input.
            if(sensor<layout.count)candidate.calibration[sensor].curve=layout.overrideCurve[type][code-4];
        }
    }
    for(unsigned i=0;i<layout.count;++i)if(layout.hid[i]) {
        auto& c=candidate.calibration[i];
        if(!c.rest || c.rest>=0x3fff || !c.bottom || c.bottom>=c.rest)
            return Rejected(why,Reject::CalibrationEndpoints);
        if(c.rest-c.bottom<100)return Rejected(why,Reject::CalibrationSpan);
        if(c.curve>=18)return Rejected(why,Reject::Curve);
        c.scale=static_cast<std::uint8_t>((c.rest-c.bottom)/100);++candidate.mapped;
    }
    if(!candidate.mapped)return Rejected(why,Reject::Layout);
    out=candidate;return true;
}
inline std::uint16_t Normalize(std::uint16_t raw,const Calibration& c) noexcept {
    if(raw>=c.rest || !c.scale || c.curve>=18)return 0;
    const unsigned difference=c.rest-raw;
    for(unsigned n=40;n;--n)
        if(difference>unsigned(kCurves[c.curve][n])*c.scale/10)return static_cast<std::uint16_t>(n*25);
    return 0;
}
inline bool Snapshot(const Frame& reply,const Profile& profile,std::array<std::uint16_t,100>& values,Reject* why=nullptr) noexcept {
    if(!profile.layout || !ZeroTail(reply,5+profile.layout->count*2))return Rejected(why,Reject::SnapshotTail);
    std::array<std::uint16_t,100> candidate{};
    for(unsigned i=0;i<profile.layout->count;++i)if(profile.layout->hid[i]) {
        const auto raw=U16(reply.data()+5+2*i);
        // The reviewed sensor producer rejects 0 and 3FFF. An uninitialized
        // array is not a live all-released keyboard snapshot.
        if(!raw)return Rejected(why,Reject::SnapshotZero);
        if(raw>=0x3fff)return Rejected(why,Reject::SnapshotSentinel);
        candidate[i]=Normalize(raw,profile.calibration[i]);
    }
    values=candidate;return true;
}
}

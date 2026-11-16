#pragma once
#include "mchose_jet75_protocol.h"

namespace halljoy::ace68gt {
using jet75::Report;
using jet75::Settings;
using Base = std::array<std::uint8_t,8>;
inline bool IsControl(unsigned interfaceNumber,unsigned page,unsigned usage,unsigned input,unsigned output) noexcept {
    return interfaceNumber==1 && page==1 && usage==0 && input==65 && output==65;
}
inline Report Info() noexcept {
    Report request{};
    request[0]=0x55;request[1]=jet75::InfoOp;
    return request;
}
inline bool InfoReply(const Report& reply) noexcept {
    return reply[0]==0xaa && reply[1]==jet75::InfoOp && reply[2]==0 &&
        reply[4]>=2 && reply[4]<=56 && reply[5]==0 && reply[6]==0 && reply[7]==0 &&
        reply[3]==jet75::Checksum(reply);
}
enum class Writer : unsigned { Unavailable, Reviewed };
inline Writer WriterPolicy(const Report& reply) noexcept {
    if(!InfoReply(reply))return Writer::Unavailable;
    const auto version=jet75::Version(reply);
    // Verified from the official 1.16 image and on the exact GT collection in log 49.
    if(version==0x0116)return Writer::Reviewed;
    const bool knownHeader=reply[4]>=8 && reply[10]==0xff && reply[11]==0xff &&
        reply[12]==0xff && reply[13]==0xff;
    if(knownHeader && ((version==0x0113 && reply[15]=='F') ||
                       (version==0x117a && reply[15]=='J')))return Writer::Reviewed;
    return Writer::Unavailable;
}
inline const char* WriterName(Writer writer) noexcept {
    constexpr const char* names[]={"unavailable","reviewed"};
    return names[static_cast<unsigned>(writer)];
}
inline bool Profile(const Base& base,unsigned& profile) noexcept {
    if(base[6]!=0xaa || base[7]!=0xbb)return false;
    const auto current=base[0]<=3?base[0]:0;
    const auto selected=base[2+current];
    profile=selected<=3?selected:0;
    return true;
}
enum class Mode : unsigned {
    Verified, AlreadySet, ProbeFailed, UnreviewedWriter, ProfileInvalid,
    RebootArmed, Stale, Uncertain, Cancelled, AttemptSpent
};
inline const char* ModeName(Mode mode) noexcept {
    constexpr const char* names[]={"verified","already_set","probe_failed","unreviewed_writer",
        "profile_invalid","reboot_armed","profile_or_flag_changed","write_unverified","cancelled","attempt_spent"};
    return names[static_cast<unsigned>(mode)];
}
// The RAM projection at byte 0x20 of each 64-byte record is not the saved flag.
inline void RemoveProjection(Settings& settings) noexcept {
    for(unsigned i=0x20;i<settings.size();i+=64)settings[i]=0;
}
template<class ReadBase,class ReadSettings,class Write>
Mode SetFlag(const Base& consent,bool enable,ReadBase readBase,ReadSettings readSettings,Write write) {
    Base base{},check{};Settings before{},after{};
    unsigned profile=0;
    if(!readBase(base) || !readSettings(before) || !readBase(check))return Mode::ProbeFailed;
    if(base!=consent || check!=base)return Mode::Stale;
    if(!Profile(base,profile) || !jet75::ProfileValid(before,profile))return Mode::ProfileInvalid;
    const auto offset=profile*64+7;
    if(bool(before[offset]&8)==enable)return Mode::AlreadySet;
    // The GT writer may reset when this factory/recovery bit is armed.
    if(jet75::WriteReboots(before))return Mode::RebootArmed;
    const auto request=jet75::FlagWrite(profile,before[offset],enable);
    const bool ack=write(request);
    if(!readSettings(after) || !readBase(check))return Mode::Uncertain;
    before[offset]=request[8];
    RemoveProjection(before);RemoveProjection(after);
    // An ACK can be lost after a write. Full readback remains the stronger proof.
    if(before==after && base==check)return Mode::Verified;
    return ack?Mode::Uncertain:Mode::Stale;
}
template<class ReadBase,class ReadSettings,class Write>
Mode Activate(bool writerProven,bool canWrite,Base& base,bool& wrote,ReadBase readBase,ReadSettings readSettings,Write write) {
    Base check{};Settings before{};unsigned profile=0;
    if(!readBase(base) || !readSettings(before) || !readBase(check))return Mode::ProbeFailed;
    if(base!=check)return Mode::Stale;
    if(!Profile(base,profile) || !jet75::ProfileValid(before,profile))return Mode::ProfileInvalid;
    if(before[profile*64+7]&8)return Mode::AlreadySet;
    if(!writerProven)return Mode::UnreviewedWriter;
    if(!canWrite)return Mode::AttemptSpent;
    return SetFlag(base,true,readBase,readSettings,[&](const Report& request){
        wrote=true;
        return write(request);
    });
}
struct Travel {
    unsigned packets=0,rejected=0,distinct=0,active=0,peak=0,releases=0;
    std::array<std::uint16_t,256> values{};
    std::array<bool,256> seen{};
    bool Consume(const Report& report,const std::array<bool,256>& allowed,jet75::Sample& sample) noexcept {
        if(report[0]!=0xa0)return false;
        if(!jet75::Decode(report,allowed,sample)){++rejected;return false;}
        ++packets;
        const auto hid=sample.hid;
        if(!seen[hid]){seen[hid]=true;++distinct;}
        if(values[hid] && !sample.milli){--active;++releases;}
        else if(!values[hid] && sample.milli){++active;peak=std::max(peak,active);}
        values[hid]=sample.milli;
        return true;
    }
};
}

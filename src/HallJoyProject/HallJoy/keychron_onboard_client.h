#pragma once
#include "keychron_onboard_profile.h"
#include "keychron_onboard_compact.h"
#include <array>
#include <cstdint>
#include <cstring>

namespace halljoy::k4_onboard {
using Packet = std::array<std::uint8_t,32>;
// One worker owns this channel and every transaction. Transport must bound I/O
// and reconnect waits and keep the same hardware serial across enumeration.
struct Channel {
    virtual ~Channel() = default;
    virtual bool Exchange(const Packet&, Packet&) = 0;
    virtual bool Read(Packet&) { return false; }
    virtual bool Reconnect(std::uint16_t revision) = 0;
    virtual std::uint64_t NowMs() const = 0;
    virtual bool Cancelled() const = 0;
};
class Client {
    Channel& channel_;
    std::uint32_t token_=0, sequence_=0, crc_=0;
    std::uint64_t heartbeatAt_=0;
    bool active_=false, burst_=false;
    hjk4_receiver receiver_{};
    bool Query(Packet request, Packet& reply, bool tokenRequired=true) {
        if (channel_.Cancelled()) return false;
        request[0]=0xa9;
        hjk4_put32(request.data()+4,token_);
        if (!channel_.Exchange(request,reply) || reply[0]!=0xa9 ||
            reply[1]!=request[1] || reply[2] ||
            (tokenRequired && hjk4_u32(reply.data()+4)!=token_)) return false;
        if (request[1]!=0x78 && request[1]!=0x7B && std::memcmp(reply.data()+8,"HJO1",4)) return false;
        return true;
    }
    bool Upload(const std::uint8_t* wire) {
        Packet request{},reply{}; request[1]=0x72;
        if (!KeepAlive() || !Query(request,reply)) return false;
        for (unsigned offset=0;offset<HJO_PROFILE_BYTES;offset+=21) {
            if (!KeepAlive()) return false;
            request={}; request[1]=0x73;
            const unsigned count=HJO_PROFILE_BYTES-offset<21?HJO_PROFILE_BYTES-offset:21;
            hjk4_put16(request.data()+8,static_cast<uint16_t>(offset));
            request[10]=static_cast<uint8_t>(count);
            std::memcpy(request.data()+11,wire+offset,count);
            if (!Query(request,reply)) return false;
        }
        request={}; request[1]=0x74;
        const auto nextCrc=hjk4_u32(wire+HJO_PROFILE_BYTES-4);
        if (!KeepAlive() || !Query(request,reply) || !(reply[17]&1) ||
            hjk4_u32(reply.data()+12)!=nextCrc) return false;
        crc_=nextCrc;
        return true;
    }
public:
    explicit Client(Channel& channel):channel_(channel) {}
    bool Active() const noexcept { return active_; }
    std::uint32_t Token() const noexcept { return token_; }
    bool Status(Packet& reply) {
        Packet request{}; request[1]=0x70;
        if(!Query(request,reply,false) || hjk4_u16(reply.data()+18)!=HJO_PROFILE_BYTES) return false;
        burst_=(reply[17]&4)!=0;return true;
    }
    bool Open(const hjo_profile& profile) {
        if (token_) return false;
        std::array<std::uint8_t,HJO_PROFILE_BYTES> wire{};
        if (!hjo_profile_encode(wire.data(),wire.size(),&profile)) return false;
        Packet request{},reply{};
        if (!Status(reply) || reply[3] || reply[16]) return false;
        request[1]=0x71; std::memcpy(request.data()+8,"HJO1",4);
        if (!Query(request,reply,false) || reply[3]!=1) return false;
        token_=hjk4_u32(reply.data()+4);
        if (!token_ || !channel_.Reconnect(0x1213) || !Status(reply) ||
            hjk4_u32(reply.data()+4)!=token_ || reply[3]!=1 || !reply[16] ||
            !Upload(wire.data())) return false;
        request={}; request[1]=0x75; sequence_=1;
        hjk4_put32(request.data()+8,sequence_);
        if (!Query(request,reply) || reply[3]!=2) return false;
        active_=true; heartbeatAt_=channel_.NowMs(); receiver_={token_,0,0,0};
        return true;
    }
    bool KeepAlive() {
        if (channel_.Cancelled()) return false;
        if (!active_ || channel_.NowMs()-heartbeatAt_<80) return true;
        if (sequence_==UINT32_MAX) return false; // never wrap/reuse lease sequence
        Packet request{},reply{}; request[1]=0x76;
        hjk4_put32(request.data()+8,++sequence_);
        if (!Query(request,reply) || reply[3]!=2) return false;
        heartbeatAt_=channel_.NowMs(); return true;
    }
    bool Update(const hjo_profile& profile) {
        if (!active_) return false;
        std::array<std::uint8_t,HJO_PROFILE_BYTES> wire{};
        if (!hjo_profile_encode(wire.data(),wire.size(),&profile)) return false;
        return hjk4_u32(wire.data()+HJO_PROFILE_BYTES-4)==crc_ ? KeepAlive() : Upload(wire.data());
    }
    bool Depth(std::array<std::uint16_t,HJO_SLOTS>& values) {
        if(burst_) {
            if(!KeepAlive())return false;
            std::array<uint8_t,HJK4_COMPACT_BYTES> frame{};
            Packet request{},reply{};request[1]=0x7B;
            const auto started=channel_.NowMs();
            if(!Query(request,reply,active_))return false;
            const auto generation=hjk4_u32(reply.data()+4);
            for(unsigned page=0;page<HJK4_COMPACT_PAGES;++page) {
                if(page && (!channel_.Read(reply) || channel_.Cancelled()))return false;
                const unsigned offset=page*22,count=HJK4_COMPACT_BYTES-offset<22?HJK4_COMPACT_BYTES-offset:22;
                if(channel_.NowMs()-started>200 || reply[0]!=0xa9 || reply[1]!=0x7B || reply[2] ||
                    hjk4_u32(reply.data()+4)!=generation || reply[8]!=page || reply[9]!=count)return false;
                std::memcpy(frame.data()+offset,reply.data()+10,count);
            }
            const auto session=hjk4_u32(frame.data()+4);
            if(!active_ && receiver_.session!=session)receiver_={session,0,0,0};
            return hjk4_compact_accept(&receiver_,frame.data(),frame.size(),values.data())!=0;
        }

        std::array<std::uint8_t,HJK4_FRAME_BYTES> frame{};
        Packet reply{};
        for (unsigned page=0;page<12;++page) {
            if (!KeepAlive()) return false;
            Packet request{}; request[1]=0x78; request[2]=static_cast<uint8_t>(page);
            if (!Query(request,reply,active_)) return false;
            const unsigned offset=page*22, count=HJK4_FRAME_BYTES-offset<22?HJK4_FRAME_BYTES-offset:22;
            if (reply[8]!=page || reply[9]!=count) return false;
            std::memcpy(frame.data()+offset,reply.data()+10,count);
        }
        const auto session=hjk4_u32(frame.data()+8);
        if (!active_ && receiver_.session!=session) receiver_={session,0,0,0};
        return hjk4_accept(&receiver_,frame.data(),frame.size(),values.data())!=0;
    }
    bool Pad(std::array<std::uint8_t,20>& report) {
        if (!KeepAlive()) return false;
        Packet request{},reply{};request[1]=0x7A;
        if (!Query(request,reply,active_)) return false;
        std::memcpy(report.data(),reply.data()+12,report.size());return true;
    }
    bool Close() {
        if (!token_) { active_=false; return true; }
        Packet request{},reply{}; request[1]=0x77;
        const bool acknowledged=Query(request,reply) && reply[3]==0;
        active_=false;
        // Failed STOP still falls back to the firmware lease; never claim a
        // confirmed close until the ordinary descriptor is observed.
        if (!channel_.Reconnect(0x1212) || !Status(reply) || reply[3] || reply[16]) return false;
        token_=sequence_=crc_=0; receiver_={};
        return acknowledged;
    }
};
}

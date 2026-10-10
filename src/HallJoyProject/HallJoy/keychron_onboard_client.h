#pragma once
#include "keychron_onboard_profile.h"
#include "keychron_onboard_session.h"
#include "keychron_onboard_compact.h"
#include "keychron_onboard_sparse.h"
#include <array>
#include <atomic>
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
// Last profile wire committed to the firmware staging buffer. Kept outside a
// single Client so a session reopened after pause can send only the delta.
struct CommittedProfile {
    bool valid=false;
    std::uint32_t crc=0;
    std::uint32_t size=0; // HJO_PROFILE_BYTES or HJO_PROFILE_BYTES_V2
    std::array<std::uint8_t,HJO_PROFILE_BYTES_MAX> wire{};
};
// Last firmware answer about HJP2 (several keys per stick direction): -1 unknown,
// 0 not supported (r8 and older), 1 supported (r9+). Read by the profile notice.
inline std::atomic<int>& K4FirmwareMultiKey() noexcept {
    static std::atomic<int> state{-1};
    return state;
}
class Client {
    Channel& channel_;
    CommittedProfile own_;
    CommittedProfile* committed_;
    std::uint32_t token_=0, sequence_=0, crc_=0;
    std::uint64_t heartbeatAt_=0;
    bool active_=false, burst_=false, sparse_=false, keepAltTab_=false, delta_=false, park_=false, multiKey_=false;
    std::uint8_t lastStatus_=0xff; // status byte of the last reply, 0xff when none arrived
    Packet lastReply_{};
    std::uint32_t strayReports_=0;
    // More unsolicited reports than this before the reply fails the query.
    static constexpr unsigned kMaxStrayReports=8;
    hjk4_receiver receiver_{};
    bool Query(Packet request, Packet& reply, bool tokenRequired=true) {
        if (channel_.Cancelled()) return false;
        request[0]=0xa9;
        hjk4_put32(request.data()+4,token_);
        if (!channel_.Exchange(request,reply)) { lastStatus_=0xff; return false; }
        // QMK notifications (layer change on Fn, report rate) share the RAW endpoint
        // and can precede this command's reply. Skip them, bounded.
        for (unsigned stray=0; reply[0]!=0xa9 || reply[1]!=request[1]; ++stray) {
            if (stray==kMaxStrayReports || !channel_.Read(reply)) { lastStatus_=0xff; return false; }
            ++strayReports_;
        }
        lastStatus_=reply[2];
        lastReply_=reply;
        if (reply[2] || (tokenRequired && hjk4_u32(reply.data()+4)!=token_)) return false;
        if (request[1]!=0x78 && request[1]!=0x7B && std::memcmp(reply.data()+8,"HJO1",4)) return false;
        return true;
    }
    // Skips reports that are not this command's pages (QMK notifications, replies
    // to another program sharing the RAW endpoint), bounded per multi-page read.
    bool SkipForeign(Packet& reply, std::uint8_t command, unsigned& skipped) {
        while (reply[0]!=0xa9 || reply[1]!=command) {
            if (skipped==kMaxStrayReports || channel_.Cancelled() || !channel_.Read(reply)) return false;
            ++skipped; ++strayReports_;
        }
        return true;
    }
    // Delta (r8): the firmware keeps the committed wire in staging, so only
    // changed chunks are sent (a Block toggle is one chunk instead of ~240).
    // It refuses a delta whose base CRC is not its committed staging.
    bool Send(const std::uint8_t* wire, std::uint32_t size, bool delta) {
        const auto base=*committed_;
        committed_->valid=false;
        Packet request{},reply{}; request[1]=0x72;
        if (delta) { request[8]=1; hjk4_put32(request.data()+12,base.crc); }
        if (!KeepAlive() || !Query(request,reply)) return false;
        for (unsigned offset=0;offset<size;offset+=21) {
            const unsigned count=size-offset<21?size-offset:21;
            if (delta && !std::memcmp(base.wire.data()+offset,wire+offset,count)) continue;
            if (!KeepAlive()) return false;
            request={}; request[1]=0x73;
            hjk4_put16(request.data()+8,static_cast<uint16_t>(offset));
            request[10]=static_cast<uint8_t>(count);
            std::memcpy(request.data()+11,wire+offset,count);
            if (!Query(request,reply)) return false;
        }
        request={}; request[1]=0x74;
        const auto nextCrc=hjk4_u32(wire+size-4);
        if (!KeepAlive() || !Query(request,reply) || !(reply[17]&1) ||
            hjk4_u32(reply.data()+12)!=nextCrc) return false;
        crc_=nextCrc;
        std::memcpy(committed_->wire.data(),wire,size);
        committed_->size=size; committed_->crc=nextCrc; committed_->valid=true;
        return true;
    }
    // A delta needs the same profile size as the committed staging: bytes past
    // the old size are not part of its base and would be stale otherwise.
    bool Upload(const std::uint8_t* wire, std::uint32_t size) {
        if (delta_ && committed_->valid && committed_->size==size && Send(wire,size,true)) return true;
        return Send(wire,size,false);
    }
public:
    explicit Client(Channel& channel, CommittedProfile* committed=nullptr)
        :channel_(channel), committed_(committed?committed:&own_) {}
    bool Active() const noexcept { return active_; }
    bool PreciseTelemetry() const noexcept { return sparse_; }
    bool SupportsKeepAltTab() const noexcept { return keepAltTab_; }
    bool SupportsPark() const noexcept { return park_; }
    bool SupportsMultiKey() const noexcept { return multiKey_; }
    // Status byte of the last firmware reply (0xff: no reply). Diagnostics only.
    std::uint8_t LastStatus() const noexcept { return lastStatus_; }
    // Commit rejection with status 8: the non-physical key slot and its kind
    // (0 stick, 1 trigger, 2 extra stick key, 3 button). Diagnostics only.
    std::uint8_t LastDetailSlot() const noexcept { return lastReply_[12]; }
    std::uint8_t LastDetailKind() const noexcept { return lastReply_[13]; }
    // Unsolicited RAW reports skipped by Query since the client was created.
    std::uint32_t StrayReports() const noexcept { return strayReports_; }
    std::uint32_t Token() const noexcept { return token_; }
    bool Status(Packet& reply) {
        Packet request{}; request[1]=0x70;
        if(!Query(request,reply,false)) return false;
        // The announced profile size is the capability: r9 reports the HJP2 size.
        const auto announced=hjk4_u16(reply.data()+18);
        if (announced!=HJO_PROFILE_BYTES && announced!=HJO_PROFILE_BYTES_V2) return false;
        multiKey_=announced==HJO_PROFILE_BYTES_V2;
        K4FirmwareMultiKey().store(multiKey_ ? 1 : 0);
        burst_=(reply[17]&4)!=0;sparse_=(reply[17]&16)!=0;
        keepAltTab_=(reply[17]&HJO_CAP_KEEP_ALT_TAB)!=0;
        delta_=(reply[17]&HJO_CAP_DELTA_UPLOAD)!=0;park_=(reply[17]&HJO_CAP_PARK)!=0;return true;
    }
    bool Open(const hjo_profile& profile) {
        if (token_) return false;
        std::array<std::uint8_t,HJO_PROFILE_BYTES_MAX> wire{};
        const unsigned size=hjo_profile_encode(wire.data(),wire.size(),&profile);
        if (!size) return false;
        Packet request{},reply{};
        if (!Status(reply) || (size==HJO_PROFILE_BYTES_V2 && !multiKey_)) return false;
        // A parked keyboard already exposes the native interface: reopening it
        // needs no USB re-enumeration.
        const bool parked=reply[3]==HJO_PARKED && reply[16];
        if (!parked && (reply[3] || reply[16])) return false;
        request[1]=0x71; std::memcpy(request.data()+8,"HJO1",4);
        if (!Query(request,reply,false) || reply[3]!=1) return false;
        token_=hjk4_u32(reply.data()+4);
        if (!token_ || (!parked && !channel_.Reconnect(0x1213)) || !Status(reply) ||
            hjk4_u32(reply.data()+4)!=token_ || reply[3]!=1 || !reply[16] ||
            !Upload(wire.data(),size)) return false;
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
        std::array<std::uint8_t,HJO_PROFILE_BYTES_MAX> wire{};
        const unsigned size=hjo_profile_encode(wire.data(),wire.size(),&profile);
        if (!size || (size==HJO_PROFILE_BYTES_V2 && !multiKey_)) return false;
        return hjk4_u32(wire.data()+size-4)==crc_ ? KeepAlive() : Upload(wire.data(),size);
    }
    bool Depth(std::array<std::uint16_t,HJO_SLOTS>& values) {
        if(sparse_) {
            if(!KeepAlive())return false;
            hjk4_sparse_assembly frame{};
            Packet request{},reply{};request[0]=0xa9;request[1]=0x7e;
            hjk4_put32(request.data()+4,token_);
            const auto started=channel_.NowMs();
            if(!channel_.Exchange(request,reply))return false;
            unsigned skipped=0;
            for(unsigned page=0;page<HJK4_SPARSE_PAGES;++page) {
                if(page && !channel_.Read(reply))return false;
                if(!SkipForeign(reply,0x7e,skipped))return false;
                if(channel_.Cancelled() || channel_.NowMs()-started>200)return false;
                const int result=hjk4_sparse_append(&frame,reply.data());
                if(result<0 || (active_ && frame.session!=token_))return false;
                if(!result)continue;
                auto next=receiver_;
                if(!active_ && next.session!=frame.session)next={frame.session,0,0,0};
                const uint32_t delta=frame.sequence-next.sequence;
                if(next.have_sequence && (!delta || delta>=0x80000000u))return false;
                std::memcpy(values.data(),frame.depth,sizeof(frame.depth));
                next.sequence=frame.sequence;next.have_sequence=1;receiver_=next;
                return true;
            }
            return false;
        }
        if(burst_) {
            if(!KeepAlive())return false;
            std::array<uint8_t,HJK4_COMPACT_BYTES> frame{};
            Packet request{},reply{};request[1]=0x7B;
            const auto started=channel_.NowMs();
            if(!Query(request,reply,active_))return false;
            const auto generation=hjk4_u32(reply.data()+4);
            unsigned skipped=0;
            for(unsigned page=0;page<HJK4_COMPACT_PAGES;++page) {
                if(page && (!channel_.Read(reply) || channel_.Cancelled()))return false;
                if(!SkipForeign(reply,0x7B,skipped))return false;
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
    // Pause (r8): release the lease but keep the native controller interface,
    // so the next Open needs no re-enumeration of the whole keyboard. The
    // firmware sends a neutral report and stops suppressing keys.
    bool Park() {
        if (!park_ || !token_) return Close();
        // Two tries: a reply left over from a cancelled request is skipped.
        Packet request{},reply{}; request[1]=0x77; request[8]=1;
        bool parked=false;
        for (unsigned attempt=0;attempt<2 && !parked;++attempt)
            parked=Query(request,reply) && reply[3]==HJO_PARKED;
        if (!parked) return Close();
        active_=false; token_=sequence_=crc_=0; receiver_={};
        return true;
    }
    // Process exit while parked: take the ownerless parked session and stop
    // it so the keyboard returns to ordinary mode. No wait for re-enumeration.
    bool ReleaseParked() {
        Packet request{},reply{};
        if (token_ || !Status(reply)) return false;
        if (reply[3]!=HJO_PARKED) return true;
        request[1]=0x71; std::memcpy(request.data()+8,"HJO1",4);
        if (!Query(request,reply,false) || reply[3]!=1) return false;
        token_=hjk4_u32(reply.data()+4);
        request={}; request[1]=0x77;
        const bool stopped=token_ && Query(request,reply) && reply[3]==0;
        token_=sequence_=crc_=0;
        return stopped;
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

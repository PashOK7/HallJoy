#pragma once
#include "aula_f75_protocol.h"
#include "boom68_client.h"
#include <cstring>
namespace halljoy::f75 {
enum class MonitorFrame { Ack, Depth, InvalidSlot, InvalidRange, OtherOpcode, OtherHeader, Framing, Disconnect, Count };
// IO: Open/Close, Send/Receive, Stopping, Now, Error. Output: cached publication
// and bounded structural evidence only. The identical session runs in image tests.
template<class IO,class Output> class Client {
public:
    Client(IO& io,Output& output):io_(io),out_(output) {}
    Result Run(const Candidate& c) {
        if(Boom(c))return halljoy::boom68::Client<IO,Output>(io_,out_).Run(c);
        candidate_=c;out_.Reset();
        if(!IdentityCandidate(c)) {Event("identity","foreign_descriptor",c.vid,c.pid);return Result::Foreign;}
        if(!Collection(c)) {Event("collection","wrong_framing",c.page,c.usage,c.inputBytes,c.outputBytes);return Result::Foreign;}
        if(io_.Stopping()) {Event("request","cancelled");return Result::Stopped;}
        if(!io_.Open(c)) {Event("open","failed",io_.Error());return Result::Retry;}
        opened_=true;Event("open","success",c.vid,c.pid,c.inputBytes,c.outputBytes);
        std::array<std::uint8_t,56> rawInfo{};
        const bool infoAvailable=Blob(0x10,rawInfo.data(),unsigned(rawInfo.size()));
        if(io_.Stopping())return Finish(Result::Stopped);
        const auto info=DecodeInfo(rawInfo);
        if(!infoAvailable)Event("identity","info_unavailable_exact_descriptor_continue");
        Event("identity","device_info",info.vid,info.pid,info.version,info.manufacturer);
        Event("identity_metadata","precision_frame",info.precision,info.frame);
        if(infoAvailable && (info.vid || info.pid) && !CompatibleInfo(c,info)) {Event("identity","foreign_remote",info.vid,info.pid);return Finish(Result::Foreign);}
        out_.Identity(c,info);identity_=true;
        // These sampled ROM reads are optional evidence, never an admission gate.
        // Use only the proven remote F75 flash tuple, not a receiver-local ID.
        if(infoAvailable && Wired(info.vid,info.pid)) Fingerprints();
        else Event("fingerprint","remote_rom_not_established");
        Assignments assigned=Factory;
        std::array<std::uint8_t,512> rawMap{};
        const bool available=Blob(0x12,rawMap.data(),unsigned(rawMap.size()));
        if(available) for(unsigned i=0;i<kSlots;++i) if(Factory[i])assigned[i]=Assigned(rawMap.data()+4*i,i);
        Event("map","physical_channels_continue",kPhysical,available?1:0);
        out_.Map(assigned,available);
        const auto positions=Positions();
        const auto batch=std::min((c.outputBytes-9)/8,(c.inputBytes-9)/8);
        std::vector<unsigned> proof(positions.begin(),positions.begin()+std::min<std::size_t>(batch,positions.size()));
        Event("status68","attempt",unsigned(proof.size()),Radio(c)?1:0);
        if(Status(proof,true)) {
            statusProven_=true;Event("status68","independent_reply",unsigned(proof.size()));
            Poll(positions,batch);
        } else if(!io_.Stopping()) {
            Event("status68","unproven_try_monitor66");
            stream_=true;monitorAttempted_=true;
            const auto request=Request(0x66,0);Report reply{};
            Event("monitor66","attempt");
            const bool acknowledged=Exchange(request,reply,0,true);
            Event("monitor66",acknowledged?"ack_or_live_sample":"ack_unavailable_keep_listening");
            Event("stream","awaiting_live_depth_no_timer_gate");
            while(!io_.Stopping()) {
                Report incoming{};unsigned bytes=0;
                if(!io_.Receive(incoming,bytes,0xffffffffu)) {
                    if(!io_.Stopping())Event("stream","transport_failed",io_.Error());
                    break;
                }
                if(!Incoming(incoming,bytes,true))break;
            }
        }
        return Finish(io_.Stopping()?Result::Stopped:Result::Retry);
    }
    ~Client() {if(opened_) Finish(Result::Stopped);}
private:
    IO& io_;Output& out_;Candidate candidate_;
    bool opened_=false,identity_=false,stream_=false,monitorAttempted_=false,statusProven_=false;
    bool analogObserved_=false;
    std::array<bool,256> announced_{},receivedShape_{};
    unsigned unknown_=0,depthMetadataCount_=0;
    std::array<bool,256> depthStates_{};
    void Event(const char* stage,const char* reason,unsigned a=0,unsigned b=0,unsigned c=0,unsigned d=0) {
        out_.Event(stage,reason,a,b,c,d);
    }
    Result Finish(Result result) {
        if(!opened_)return result;
        out_.Neutral();
        if(monitorAttempted_) {
            const auto off=Request(0x67,0);
            // Cleanup is intentionally sent even after cancellation, without an
            // ACK wait; this is the manufacturer's reversible monitor shutdown.
            out_.Requested(0x67);
            const bool sent=io_.Send(off,candidate_.outputBytes,200);
            Event("cleanup",sent?"monitor_off_sent":"monitor_off_failed",sent?0:io_.Error());
        }
        io_.Close();opened_=false;out_.Closed();
        Event("cleanup","handle_closed_input_cleared",identity_?1:0,analogObserved_?1:0);
        return result;
    }
    bool Incoming(const Report& r,unsigned bytes,bool monitorRead=false) {
        out_.Received(bytes>2?r[2]:0,bytes);
        if(bytes!=candidate_.inputBytes || r[0]!=0) {
            if(stream_)out_.MonitorReceived(MonitorFrame::Framing);
            if(!monitorRead || unknown_++<16)
                Event("framing","partial_or_report_id",bytes,r[0],candidate_.inputBytes);
            // Once listening, discarding one malformed report preserves the
            // chance to read a later valid sample or release in this session.
            return monitorRead;
        }
        if(!stream_ && r[1]==0x55 && !receivedShape_[r[2]]) {
            receivedShape_[r[2]]=true;Event("reply_shape","first_opcode",r[2],r[3],Word(r.data()+4),bytes);
        }
        if(r[1]==0x55 && r[2]==0xfc && r[3]==4) {
            if(stream_)out_.MonitorReceived(MonitorFrame::Disconnect);
            Event("receiver","remote_disconnect");out_.Neutral();return false;
        }
        if(r[1]==0x55 && r[2]==0xfb && stream_ && identity_) {
            const unsigned slot=r[3],raw=Word(r.data()+11),stroke=Word(r.data()+13),adc=Word(r.data()+9);
            if(slot>=kSlots || !Factory[slot]) {
                out_.MonitorReceived(MonitorFrame::InvalidSlot);
                out_.IgnoredSlot();return true;
            }
            if(raw>1000 || stroke<10 || stroke>50) {
                out_.MonitorReceived(MonitorFrame::InvalidRange);
                if(unknown_++<16)Event("depth_frame","unsupported_range",raw,stroke);return true;
            }
            if(!depthStates_[r[4]] && depthMetadataCount_<16) {
                depthStates_[r[4]]=true;++depthMetadataCount_;
                Event("depth_metadata","first_calibration_status",r[4],Word(r.data()+5),Word(r.data()+7)&0x7fff,stroke);
            }
            out_.MonitorReceived(MonitorFrame::Depth);
            out_.Sample(slot,raw,adc,stroke,2);analogObserved_=true;out_.Batch(1,false);
        } else if(stream_) {
            if(Header(r,bytes,0x66,0,0))out_.MonitorReceived(MonitorFrame::Ack);
            else if(r[1]==0x55) {
                out_.MonitorReceived(MonitorFrame::OtherOpcode);
                if(!receivedShape_[r[2]] && unknown_++<16) {
                    receivedShape_[r[2]]=true;
                    Event("stream","other_vendor_opcode",r[2],bytes);
                }
            } else {
                out_.MonitorReceived(MonitorFrame::OtherHeader);
                if(unknown_++<16)Event("stream","unrecognized_header",bytes);
            }
        } else if(r[1]!=0x55 && unknown_++<16) {
            Event("frame","unrecognized_header",r[1],r[2],r[3],bytes);
        }
        return true;
    }
    bool Exchange(const Report& request,Report& reply,unsigned expected,bool allowStream=false) {
        if(io_.Stopping()) {Event("request","cancelled",request[2]);return false;}
        const auto cmd=request[2];
        if(!announced_[cmd]) {announced_[cmd]=true;Event("request","first_attempt",cmd,expected);}
        out_.Requested(cmd);
        if(!io_.Send(request,candidate_.outputBytes,500)) {
            Event("send","failed",cmd,io_.Error());return false;
        }
        const auto end=io_.Now()+500;
        bool shapeReported=false;
        while(!io_.Stopping()) {
            const auto now=io_.Now();
            if(now>=end)break;
            Report incoming{};unsigned bytes=0;
            if(!io_.Receive(incoming,bytes,unsigned(end-now))) {
                Event("reply","unavailable",cmd,io_.Error());return false;
            }
            if(!Incoming(incoming,bytes))return false;
            if(allowStream && analogObserved_) {reply=incoming;return true;}
            if(Header(incoming,bytes,cmd,expected,Word(request.data()+4))) {
                reply=incoming;return true;
            }
            if(!shapeReported && incoming[1]==0x55 && incoming[2]==cmd) {
                shapeReported=true;Event("reply","envelope_mismatch",cmd,incoming[3],Word(incoming.data()+4),bytes);
            }
        }
        Event("reply",io_.Stopping()?"cancelled":"deadline",cmd);return false;
    }
    bool Blob(unsigned cmd,std::uint8_t* output,unsigned count,unsigned start=0) {
        const auto chunk=std::min(candidate_.inputBytes,candidate_.outputBytes)-9;
        for(unsigned offset=0;offset<count;) {
            const auto bytes=std::min(chunk,count-offset);
            auto request=Request(cmd,bytes,start+offset,offset+bytes==count);Report reply{};
            if(!Exchange(request,reply,bytes))return false;
            std::copy_n(reply.begin()+9,bytes,output+offset);offset+=bytes;
        }
        return true;
    }
    void Fingerprints() {
        for(unsigned i=0;i<2 && !io_.Stopping();++i) {
            const unsigned address=i?0x13138:0x13446;
            std::array<std::uint8_t,24> data{};
            if(!Blob(0x10,data.data(),24,address-0x9000)) {
                Event("fingerprint","optional_read_unavailable",i);continue;
            }
            std::uint64_t h=1469598103934665603ull;
            for(unsigned n=0;n<24;++n)if(n<14 || n>=19){h^=data[n];h*=1099511628211ull;}
            out_.Fingerprint(i,h);
        }
    }
    bool Status(const std::vector<unsigned>& keys,bool proof=false) {
        if(keys.empty())return true;
        const auto count=unsigned(keys.size());
        auto request=Request(0x68,count*8);Report reply{};
        for(unsigned i=0;i<count;++i)request[9+8*i]=std::uint8_t(keys[i]);
        if(!Exchange(request,reply,count*8))return false;
        // Real reviewed old firmware turns AA into55 and copies the whole body.
        // Such an echo has zero stroke fields but is not independent live data.
        const bool echo=std::equal(request.begin()+9,request.begin()+9+count*8,reply.begin()+9);
        if(proof && echo) {Event("status68","request_echo_not_analog",count);return false;}
        if(proof)Event("status68","first_independent_fields",reply[9],Word(reply.data()+10),Word(reply.data()+12),count);
        for(unsigned i=0;i<count;++i) {
            const auto raw=Word(reply.data()+9+8*i+3);
            if(raw>1000) {Event("status68","out_of_scale",raw,i);return false;}
        }
        for(unsigned i=0;i<count;++i) {
            const auto* record=reply.data()+9+8*i;
            out_.Sample(keys[i],Word(record+3),Word(record+1),34,1);
        }
        analogObserved_=true;out_.Batch(count,proof);return true;
    }
    void Poll(const std::vector<unsigned>& positions,unsigned batch) {
        unsigned background=0;
        while(!io_.Stopping()) {
            std::vector<unsigned> hot;
            for(auto slot:positions)if(out_.Bound(slot))hot.push_back(slot);
            for(unsigned begin=0;begin<hot.size() && !io_.Stopping();begin+=batch) {
                std::vector<unsigned> keys(hot.begin()+begin,hot.begin()+std::min<std::size_t>(hot.size(),begin+batch));
                if(!Status(keys))return;
            }
            if(io_.Stopping())break;
            // One background packet between complete bound-key rounds. No delay,
            // key suppression or per-key timer is used to manufacture releases.
            std::vector<unsigned> keys;
            for(unsigned visits=0;visits<positions.size() && keys.size()<batch;++visits) {
                const auto slot=positions[background++%positions.size()];
                if(std::find(hot.begin(),hot.end(),slot)==hot.end())keys.push_back(slot);
            }
            if(!keys.empty() && !Status(keys))return;
        }
    }
};
} // namespace halljoy::f75

#pragma once
#include "mchose_gt75_protocol.h"
namespace halljoy::gt75 {
enum class Result {Retry,Foreign,Stopped};
// Shared by real HID and exact-image fixtures. Control failure never vetoes
// passive A0 input from the exact collection. No calibration or rate writes.
template<class IO,class Output> class Client {
    IO& io_;Output& out_;Candidate candidate_;
    bool opened_=false,cleanup_=false,wrote_=false,allowed_=true;
    unsigned ownedProfile_=0,unknown_=0;bool first_=true;
    std::array<bool,256> announced_{};
    void Event(const char* s,const char* r,unsigned a=0,unsigned b=0,unsigned c=0,unsigned d=0){out_.Event(s,r,a,b,c,d);}
    bool Stopping()const{return !cleanup_ && io_.Stopping();}
    bool Incoming(const Report& r,unsigned bytes){
        if(Stopping()){Event("receive","cancelled_before_publication");return false;}
        out_.Received(bytes>2?r[2]:0,bytes);
        if(bytes!=65 || r[0]){Event("framing","partial_or_report_id",bytes,r[0]);return false;}
        if(r[1]>=0xa1 && r[1]<=0xa3){Event("profile","device_changed_reopen",r[1]);return false;}
        if(r[1]==0xa0){
            Sample sample{};
            if(Decode(r,bytes,sample)){
                if(!cleanup_){out_.Sample(sample.slot,sample.raw,0,sample.maximum,1);out_.Batch(1,first_);first_=false;}
            }else if(unknown_++<16)Event("depth","unrecognized_descriptor_or_range",r[2],0,0,(unsigned(r[15])<<8)|r[16]);
        }else if(r[1]!=0xaa && unknown_++<16)Event("frame","unknown_header",r[1],r[2],bytes);
        return true;
    }
    bool Exchange(const Report& q,Report& answer,bool info=false){
        if(Stopping())return false;
        if(!announced_[q[2]]){announced_[q[2]]=true;Event("request","first_attempt",q[2],q[5],Offset(q));}
        out_.Requested(q[2]);
        if(!io_.Send(q,65,500)){Event("send","failed",q[2],io_.Error());return false;}
        const auto end=io_.Now()+500;bool shape=false;
        while(!Stopping()){
            const auto now=io_.Now();if(now>=end)break;
            Report r{};unsigned bytes=0;
            if(!io_.Receive(r,bytes,unsigned(end-now))){Event("reply","unavailable",q[2],io_.Error());return false;}
            if(!Incoming(r,bytes))return false;
            if(Reply(q,r,bytes,info)){answer=r;return true;}
            if(r[1]==0xaa && r[2]==q[2] && !shape){shape=true;Event("reply","envelope_mismatch",q[2],r[5],Offset(r),r[3]);}
        }
        Event("reply","deadline_or_cancelled",q[2]);return false;
    }
    bool Blob(unsigned op,std::uint8_t* dst,unsigned count){
        for(unsigned offset=0;offset<count;){const auto n=std::min(56u,count-offset);Report r{};
            if(!Exchange(Request(op,n,offset),r))return false;
            std::copy_n(r.begin()+9,n,dst+offset);offset+=n;
        }return true;
    }
    bool State(Base& b,Settings& s){Base check{};
        if(!Blob(4,b.data(),8) || !Blob(5,s.data(),256) || !Blob(4,check.data(),8)){Event("control","state_unavailable_passive_continues");return false;}
        if(Profile(b)!=Profile(check)){Event("control","effective_profile_changed_passive_continues");return false;}
        return true;
    }
    bool Change(Base b,Settings s,unsigned target,bool on){
        if(!ValidProfile(s,Profile(b)) || !ValidProfile(s,target)){Event("control","stored_profile_invalid_no_write",target);return false;}
        if(s[0]&1){Event("control","reset_armed_no_write",target);return false;}
        const auto offset=target*64+7;
        if(bool(s[offset]&8)==on){Event("control",on?"already_enabled_preserved":"already_restored",target);return true;}
        if(on && !allowed_){Event("control","attempt_spent_passive_continues");return false;}
        const auto q=Flag(target,s[offset],on);Report r{};
        // Own the attempt even if an ACK is lost after the flash write.
        if(on){wrote_=true;ownedProfile_=target;}
        Event("control",on?"enable_one_bit_flash_rmw":"restore_owned_flag",target,s[offset],q[9]);
        const bool ack=Exchange(q,r);Settings after{};Base check{};
        if(!Blob(5,after.data(),256) || !Blob(4,check.data(),8)){Event("control","write_unverified",ack?1:0,target);return false;}
        s[offset]=q[9];RemoveProjection(s);RemoveProjection(after);
        const bool verified=s==after;
        Event("control",verified?"saved_readback_verified":"readback_mismatch",target,ack?1:0,Profile(b),Profile(check));
        return verified && Profile(b)==Profile(check);
    }
    Result Finish(Result result){
        if(!opened_)return result;out_.Neutral();
        if(wrote_){
            cleanup_=true;io_.CleanupMode(true);Base b{};Settings s{};
            if(State(b,s))Change(b,s,ownedProfile_,false);
            else Event("cleanup","state_unavailable_owned_flag_may_remain",ownedProfile_);
            io_.CleanupMode(false);cleanup_=false;
        }else Event("cleanup","no_owned_flag_preserve_original");
        io_.Close();opened_=false;out_.Closed();Event("cleanup","handle_closed_input_cleared");return result;
    }
public:
    Client(IO& io,Output& out,bool canWrite=true):io_(io),out_(out),allowed_(canWrite){}
    bool WasWritten()const{return wrote_;}
    ~Client(){if(opened_)Finish(Result::Stopped);}
    Result Run(const Candidate& c){
        candidate_=c;out_.Reset();
        if(!IdentityCandidate(c)){Event("identity","foreign_descriptor",c.vid,c.pid);return Result::Foreign;}
        if(!Collection(c)){Event("collection","wrong_framing",c.page,c.usage,c.inputBytes,c.outputBytes);return Result::Foreign;}
        if(Stopping()){Event("open","cancelled");return Result::Stopped;}
        if(!io_.Open(c)){Event("open","failed",io_.Error());return Result::Retry;}
        opened_=true;Event("open","exact_gt75_success",c.vid,c.pid);
        out_.Identity(c,{});out_.Map(Factory,false);
        Report info{};
        if(Exchange(Request(3,56),info,true)){
            const unsigned version=info[9]|(unsigned(info[10])<<8);
            out_.Identity(c,{version});Event("identity","firmware_metadata",version,info[5]);
        }else Event("identity","info_missing_passive_and_safe_state_continue");
        Base b{};Settings s{};
        if(!Stopping() && State(b,s)){
            Event("control","effective_profile_and_saved_flag",Profile(b),s[Profile(b)*64+7],s[0],b[1]);
            Change(b,s,Profile(b),true);
        }
        Event("stream","awaiting_live_a0_no_timer_gate");
        while(!Stopping()){
            Report r{};unsigned bytes=0;
            if(!io_.Receive(r,bytes,0xffffffffu)){if(!Stopping())Event("stream","transport_failed",io_.Error());break;}
            if(!Incoming(r,bytes))break;
        }
        return Finish(io_.Stopping()?Result::Stopped:Result::Retry);
    }
};
} // namespace halljoy::gt75

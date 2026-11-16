#pragma once
#include "aula_f75_protocol.h"
#include <cstring>
namespace halljoy::boom68 {
// This immutable upper-code fingerprint binds a reviewed RAM layout, not a
// version allowlist. It is not a hash of the entire device firmware. A changed
// implementation receives the documented independent status trial instead.
struct Stock110 {
    static constexpr unsigned CodeBegin=0xc000,CodeEnd=0x1358c;
    static constexpr std::uint64_t CodeHash=0x1ea95d27cac2a12eull;
};
inline std::uint64_t Hash(std::uint64_t h,const std::uint8_t* bytes,unsigned count) {
    for(unsigned i=0;i<count;++i){h^=bytes[i];h*=1099511628211ull;}return h;
}
inline unsigned Stroke(unsigned axis) {return axis==4 || axis==5 || axis==7?34:33;}
// The normal dispatcher GET16 base is20002442. The normal scanner maintains
// calibrated depth at20002A42, independently of simulation/calibration reports.
constexpr unsigned DepthOffset=0x600, AxisOffset=0x1180;
template<class IO,class Output,class Firmware=Stock110> class Client {
public:
    Client(IO& io,Output& out):io_(io),out_(out){}
    f75::Result Run(const f75::Candidate& c) {
        c_=c;out_.Reset();
        if(!f75::Boom(c)){Event("identity","foreign_descriptor",c.vid,c.pid);return f75::Result::Foreign;}
        // Identity is an official exact USB tuple, before any vendor command.
        if(!f75::Collection(c)){Event("collection","wrong_framing",c.page,c.usage,c.inputBytes,c.outputBytes);return f75::Result::Foreign;}
        if(io_.Stopping()){Event("request","cancelled");return f75::Result::Stopped;}
        if(!out_.MayOpen(c)){Event("routing","ownership_collision_before_open");return f75::Result::Foreign;}
        if(!io_.Open(c)){Event("open","failed",io_.Error());return f75::Result::Retry;}
        opened_=true;out_.Identity(c,{c.vid,c.pid,c.version,0,0,0});
        Event("open","success",c.vid,c.pid,c.inputBytes,c.outputBytes);
        Event("identity","descriptor_version_no_get10",c.version);
        chunk_=std::min(c.inputBytes,c.outputBytes)-9;
        std::array<std::uint8_t,512> metadata{};f75::Assignments assigned=Factory;
        const bool map=Blob(0x12,0,metadata.data(),unsigned(metadata.size()));
        if(map)for(unsigned i=0;i<128;++i)if(Factory[i])assigned[i]=f75::Assigned(metadata.data()+4*i,i,Factory);
        Event("map","physical_channels_continue",Physical,map?1:0);out_.Map(assigned,map);
        if(io_.Stopping())return Finish(f75::Result::Stopped);
        const bool known=CodeProof();
        if(io_.Stopping())return Finish(f75::Result::Stopped);
        if(known && Channels()) {
            Event("matrix16","read_attempt",Physical,DepthOffset,AxisOffset);
            if(ReadAxes())PollRam();
            else if(!io_.Stopping())Trial68();
        } else if(!io_.Stopping()) {
            Event("matrix16","layout_unproven_try_status68");
            Trial68();
        }
        return Finish(io_.Stopping()?f75::Result::Stopped:unusable_?f75::Result::Foreign:f75::Result::Retry);
    }
    ~Client(){if(opened_)Finish(f75::Result::Stopped);}
private:
    IO& io_;Output& out_;f75::Candidate c_;
    bool opened_=false,unusable_=false,live_=false;
    unsigned chunk_=0;
    std::array<std::uint8_t,128> channel_{},axis_{};
    std::array<bool,256> seen_{};
    unsigned unknown_=0;
    void Event(const char* a,const char* b,unsigned x=0,unsigned y=0,unsigned z=0,unsigned w=0){out_.Event(a,b,x,y,z,w);}
    f75::Result Finish(f75::Result r){
        if(opened_){out_.Neutral();io_.Close();opened_=false;Event("cleanup","readonly_handle_closed_input_cleared",live_?1:0);out_.Closed();}return r;
    }
    bool Exchange(unsigned cmd,unsigned offset,unsigned count,f75::Report& reply,const std::uint8_t* data=nullptr) {
        if(io_.Stopping()){Event("request","cancelled",cmd);return false;}
        auto request=f75::Request(cmd,count,offset);
        std::fill(request.begin()+9,request.end(),std::uint8_t{0xa5});
        if(data)std::copy_n(data,count,request.begin()+9);
        out_.Requested(cmd);
        if(!io_.Send(request,c_.outputBytes,500)){Event("send","failed",cmd,io_.Error());return false;}
        const auto deadline=io_.Now()+500;
        while(!io_.Stopping()){
            const auto now=io_.Now();if(now>=deadline)break;
            unsigned bytes=0;f75::Report r{};
            if(!io_.Receive(r,bytes,unsigned(deadline-now))){Event("reply","unavailable",cmd,io_.Error());return false;}
            out_.Received(bytes>2?r[2]:0,bytes);
            if(io_.Stopping()){Event("reply","cancel_after_read",cmd);return false;}
            if(bytes!=c_.inputBytes || r[0]){Event("framing","partial_or_report_id",bytes,r[0],c_.inputBytes);return false;}
            if(!seen_[r[2]]){seen_[r[2]]=true;Event("reply_shape","first_opcode",r[1],r[2],r[3],f75::Word(r.data()+4));}
            if(f75::Header(r,bytes,cmd,count,offset)){reply=r;return true;}
            if(unknown_++<16)Event("reply","envelope_mismatch",cmd,r[2],r[3],f75::Word(r.data()+4));
        }
        Event("reply",io_.Stopping()?"cancelled":"deadline",cmd);return false;
    }
    bool Blob(unsigned cmd,unsigned offset,std::uint8_t* bytes,unsigned count){
        for(unsigned n=0;n<count;){
            const unsigned take=std::min(chunk_,count-n);f75::Report r{};
            if(!Exchange(cmd,offset+n,take,r))return false;
            std::copy_n(r.begin()+9,take,bytes+n);n+=take;
        }return true;
    }
    bool CodeProof(){
        std::uint64_t h=1469598103934665603ull;
        Event("code","readonly_fingerprint_attempt",Firmware::CodeBegin,Firmware::CodeEnd);
        for(unsigned a=Firmware::CodeBegin;a<Firmware::CodeEnd;){
            const unsigned count=std::min(chunk_,Firmware::CodeEnd-a);f75::Report r{};
            if(!Exchange(0x12,a-0x9600,count,r)){Event("code","fingerprint_unavailable");return false;}
            h=Hash(h,r.data()+9,count);a+=count;
        }
        out_.Fingerprint(0,h);const bool match=h==Firmware::CodeHash;
        Event("code",match?"reviewed_ram_layout":"different_code_status_trial",unsigned(h),unsigned(h>>32));return match;
    }
    bool Channels(){
        std::array<std::uint8_t,128> map{};
        if(!Blob(0x12,0x1358c-0x9600,map.data(),128)){Event("matrix16","channel_map_unavailable");return false;}
        unsigned extra=0;
        for(unsigned slot=0;slot<128;++slot)if(Factory[slot]){
            unsigned count=0;
            for(unsigned c=0;c<128;++c)if(map[c]==slot){channel_[slot]=std::uint8_t(c);++count;}
            if(count!=1){Event("matrix16","physical_channel_mapping_ambiguous",count);return false;}
        }
        for(auto slot:map)extra+=slot<128 && !Factory[slot] && slot!=125;
        out_.Fingerprint(1,Hash(1469598103934665603ull,map.data(),128));
        Event("matrix16","channel_map_ready",Physical,extra);return true;
    }
    bool ReadAxes(){
        const bool okay=Blob(0x16,AxisOffset,axis_.data(),128);
        if(!okay)Event("matrix16","axis_read_unavailable");return okay;
    }
    bool Page(unsigned start,unsigned channels){
        f75::Report r{};
        if(!Exchange(0x16,DepthOffset+start*2,channels*2,r))return false;
        for(unsigned slot=0;slot<128;++slot)if(Factory[slot] && channel_[slot]>=start && channel_[slot]<start+channels){
            const unsigned raw=f75::Word(r.data()+9+2*(channel_[slot]-start));
            if(raw>1000){Event("matrix16","depth_outside_reviewed_units",raw);return false;}
        }
        if(io_.Stopping())return false;
        unsigned samples=0;
        for(unsigned slot=0;slot<128;++slot)if(Factory[slot] && channel_[slot]>=start && channel_[slot]<start+channels){
            const unsigned raw=f75::Word(r.data()+9+2*(channel_[slot]-start));
            out_.Sample(slot,raw,0,Stroke(axis_[slot]),3);++samples;
        }
        if(samples){live_=true;out_.Batch(samples,false);}return true;
    }
    bool BoundRound(){
        std::vector<unsigned> hot;
        for(unsigned i=0;i<128;++i)if(Factory[i] && out_.Bound(i))hot.push_back(channel_[i]);
        std::sort(hot.begin(),hot.end());
        for(unsigned at=0;at<hot.size();){
            unsigned end=at+1;while(end<hot.size() && hot[end]-hot[at]+1<=chunk_/2)++end;
            if(!Page(hot[at],hot[end-1]-hot[at]+1))return false;at=end;
        }return true;
    }
    void PollRam(){
        unsigned background=0,axisPage=0;
        while(!io_.Stopping()){
            if(!BoundRound())return;
            if(axisPage){
                const unsigned take=std::min(chunk_,128-(axisPage-1));f75::Report r{};
                if(!Exchange(0x16,AxisOffset+axisPage-1,take,r))return;
                std::copy_n(r.begin()+9,take,axis_.begin()+axisPage-1);axisPage+=take;
                if(axisPage>128)axisPage=0;
            }else{
                const unsigned count=std::min(chunk_/2,128-background);
                if(!Page(background,count))return;background+=count;
                if(background==128){background=0;axisPage=1;}
            }
        }
    }
    bool Status(const std::vector<unsigned>& slots,bool proof){
        std::array<std::uint8_t,56> query{};
        for(unsigned i=0;i<slots.size();++i)query[i*8]=std::uint8_t(slots[i]);
        f75::Report r{};const unsigned count=unsigned(slots.size())*8;
        if(!Exchange(0x68,0,count,r,query.data()))return false;
        if(std::equal(query.begin(),query.begin()+count,r.begin()+9) &&
            std::any_of(slots.begin(),slots.end(),[](unsigned slot){return slot!=0;})){
            Event("status68","request_echo_not_analog");return false;
        }
        for(unsigned i=0;i<slots.size();++i)if(f75::Word(r.data()+12+i*8)>1000){Event("status68","unsupported_depth_units");return false;}
        if(io_.Stopping())return false;
        for(unsigned i=0;i<slots.size();++i)out_.Sample(slots[i],f75::Word(r.data()+12+i*8),f75::Word(r.data()+10+i*8),33,1);
        live_=true;out_.Batch(unsigned(slots.size()),proof);return true;
    }
    void Trial68(){
        std::vector<unsigned> positions;for(unsigned i=1;i<128;++i)if(Factory[i])positions.push_back(i);if(Factory[0])positions.push_back(0);
        const unsigned batch=chunk_/8;
        std::vector<unsigned> first(positions.begin(),positions.begin()+std::min<unsigned>(batch,unsigned(positions.size())));
        Event("status68","independent_getter_attempt",batch);
        if(!Status(first,true)){Event("analog","no_proven_full_matrix_getter");unusable_=!io_.Stopping();return;}
        Event("status68","independent_reply_scale_hardware_unverified");
        unsigned background=0;
        while(!io_.Stopping()){
            std::vector<unsigned> hot;for(auto slot:positions)if(out_.Bound(slot))hot.push_back(slot);
            for(unsigned at=0;at<hot.size();at+=batch){
                std::vector<unsigned> group(hot.begin()+at,hot.begin()+std::min<unsigned>(unsigned(hot.size()),at+batch));
                if(!Status(group,false))return;
            }
            std::vector<unsigned> group;
            for(unsigned n=0;n<positions.size() && group.size()<batch;++n){const auto slot=positions[background++%positions.size()];if(std::find(hot.begin(),hot.end(),slot)==hot.end())group.push_back(slot);}
            if(!group.empty() && !Status(group,false))return;
        }
    }
};
} // namespace halljoy::boom68

#pragma once
#include "boom68_client.h"
namespace halljoy::boom68::fixture {
// Copyright-free reconstructed ROM data tests the actual proof algorithm with
// an explicit model policy. The exact vendor bytes are executed separately by
// review_boom68_firmware.py, never bundled in HallJoy or its test fixtures.
struct Rom:Stock110 {
    static constexpr std::uint8_t Byte(unsigned a){return std::uint8_t(a*37+13);}
    static constexpr std::uint64_t Expected(){
        std::uint64_t h=1469598103934665603ull;
        for(unsigned a=CodeBegin;a<CodeEnd;++a){h^=Byte(a);h*=1099511628211ull;}return h;
    }
    static const std::uint64_t CodeHash;
};
inline const std::uint64_t Rom::CodeHash=Rom::Expected();
class Io {
    unsigned variant_,phase_=0;bool stopped_=false,queued_=false;
    std::uint64_t now_=0;f75::Report reply_{};f75::Candidate c_;
    std::array<unsigned,128> slots_{};
    std::array<bool,128> covered_{};
    bool advance_=false;
    unsigned error_=0;
public:
    unsigned opens=0,closes=0,commands=0,ram=0,status=0;bool forbidden=false;
    explicit Io(unsigned v):variant_(v){
        slots_.fill(125);unsigned channel=0;
        for(unsigned slot=0;slot<128;++slot)if(Factory[slot])slots_[channel++]=slot;
        if(v==15)std::reverse(slots_.begin(),slots_.end());
        if(v==16)slots_[120]=120;
        if(v==108)slots_[0]=125;
    }
    bool Open(const f75::Candidate& c){c_=c;++opens;if(variant_==102){error_=5;return false;}return true;}
    void Close(){++closes;}
    bool Stopping()const{return stopped_ || variant_==103;}
    std::uint64_t Now(){return now_;}
    unsigned Error()const{return error_;}
    bool Send(const f75::Report& r,unsigned bytes,unsigned){
        if(phase_==4){stopped_=true;error_=995;return false;}
        ++commands;++now_;
        if(bytes!=c_.outputBytes || r[0] || r[1]!=0xaa || (r[2]!=0x12 && r[2]!=0x16 && r[2]!=0x68)){forbidden=true;return false;}
        const unsigned offset=f75::Word(r.data()+4),count=r[3];reply_=r;reply_[1]=0x55;queued_=true;advance_=false;
        if(r[2]==0x12){
            if(offset<512){
                if(variant_==3){queued_=false;error_=121;return false;}
                for(unsigned i=0;i<count;++i){
                    const unsigned index=offset+i,pos=index/4,field=index%4;
                    const bool alias=variant_==1 || (variant_==6 && pos==17) || (variant_==10 && pos==17);
                    reply_[9+i]=std::uint8_t(variant_==2 || variant_==5?0xcc:alias?(field==0?2:field==2?26:0):0);
                }
            }else if(offset>=Rom::CodeBegin-0x9600 && offset<Rom::CodeEnd-0x9600){
                if(variant_==14){queued_=false;error_=121;return false;}
                for(unsigned i=0;i<count;++i)reply_[9+i]=Rom::Byte(0x9600+offset+i);
                if(variant_==13 || variant_==107 || variant_==109)reply_[9]^=1;
            }else if(offset>=0x1358c-0x9600 && offset<0x1360c-0x9600){
                for(unsigned i=0;i<count;++i)reply_[9+i]=std::uint8_t(slots_[offset+i-(0x1358c-0x9600)]);
            }else{forbidden=true;return false;}
            if(variant_==113)reply_=r,reply_[1]=0x55;
        }else if(r[2]==0x16){
            if(offset>=AxisOffset && offset<AxisOffset+128){
                for(unsigned i=0;i<count;++i)reply_[9+i]=std::uint8_t(variant_==17?0:variant_==18?5:1);
            }else if(offset>=DepthOffset && offset<DepthOffset+256){
                ++ram;
                if(variant_==111){queued_=false;error_=121;return false;}
                for(unsigned i=0;i<count;i+=2){
                    const unsigned channel=(offset-DepthOffset+i)/2,slot=slots_[channel];
                    const unsigned raw=Value(slot);
                    reply_[9+i]=std::uint8_t(raw);reply_[10+i]=std::uint8_t(raw>>8);
                    if(slot<128 && Factory[slot])covered_[slot]=true;
                }
                advance_=Complete();
            }else{forbidden=true;return false;}
        }else{
            ++status;
            if(variant_!=13 && variant_!=14 && variant_!=109)return true; // exact old-style echo
            for(unsigned i=0;i<count;i+=8){
                const unsigned slot=r[9+i],raw=Value(slot);std::fill_n(reply_.begin()+9+i,8,0);
                reply_[10+i]=0x34;reply_[11+i]=0x08;reply_[12+i]=std::uint8_t(raw);reply_[13+i]=std::uint8_t(raw>>8);
                covered_[slot]=true;
            }
            advance_=Complete();
        }
        return true;
    }
    bool Receive(f75::Report& r,unsigned& bytes,unsigned){
        ++now_;if(!queued_){error_=121;return false;}queued_=false;r=reply_;bytes=c_.inputBytes;
        if(variant_==104)--bytes;
        if(variant_==110 && ram)stopped_=true;
        if(advance_){covered_.fill(false);++phase_;}
        return true;
    }
private:
    unsigned Value(unsigned slot)const{
        if(variant_==106 || variant_==109)return 65535;
        return slot>=128 || !Factory[slot] || phase_==0 || phase_==3 || (phase_==2 && slot==17)?0:33*(slot%4+1)*2;
    }
    bool Complete()const{for(unsigned s=0;s<128;++s)if(Factory[s] && !covered_[s])return false;return true;}
};
} // namespace halljoy::boom68::fixture

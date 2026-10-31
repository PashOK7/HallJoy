#include "../HallJoy/mchose_mix87_protocol.h"
#include <cassert>
#include <cstring>
#include <cstdio>
using namespace halljoy::mix87;
int main(){
    const auto read=Read(0x2c007,1);assert(read[0]==0x55 && read[1]==0xe0 && read[5]==7 && read[6]==0xc0 && read[7]==2);
    Report reply=read;reply[0]=0xaa;reply[8]=0xaf;reply[3]=Checksum(reply);assert(Reply(read,reply));
    reply[6]^=1;reply[3]=Checksum(reply);assert(!Reply(read,reply));
    reply=read;reply[0]=0xaa;reply[3]^=1;assert(!Reply(read,reply));
    reply=read;reply[0]=0xaa;reply[4]=255;assert(!Reply(read,reply));
    assert(Read(0x80000,1)[0]==0 && Read(0,57)[0]==0);
    std::array<bool,256> allowed{};allowed[26]=true;Sample sample{};
    Report a{};a[0]=0xa0;a[1]=16;a[3]=26;a[14]=1;a[15]=0x55;
    a[6]=1;a[7]=0x55;assert(Decode(a,allowed,sample) && sample.milli==1000);
    a[6]=0;a[7]=10;assert(Decode(a,allowed,sample) && sample.milli==29);
    a[7]=0;assert(Decode(a,allowed,sample) && sample.milli==0);
    a[15]=0;assert(!Decode(a,allowed,sample));
    // Non-fingerprinted ARM models accept the plausible range; Mix87 III stays exact.
    a[14]=1;a[15]=0x5f;a[6]=0;a[7]=0;assert(!Decode(a,allowed,sample) && Decode(a,allowed,sample,false));
    a[14]=0;a[15]=150;assert(!Decode(a,allowed,sample,false));a[14]=3;a[15]=0xe8;assert(!Decode(a,allowed,sample,false));
    a[14]=1;a[15]=0x55;
    assert(FindModel(Vid,Pid)==&Models[0] && Models[0].fingerprinted && Models[0].keyCount==86 && std::size(Models)==7);
    for(std::size_t i=0;i<std::size(Models);++i){
        assert(Models[i].fingerprinted==(i==0) && FindModel(Models[i].vid,Models[i].pid)==&Models[i]);
        std::array<bool,256> m{};for(std::size_t k=0;k<Models[i].keyCount;++k){assert(!m[Models[i].keys[k]]);m[Models[i].keys[k]]=true;}
        for(std::size_t j=0;j<i;++j)assert(Models[i].vid!=Models[j].vid || Models[i].pid!=Models[j].pid);
    }
    assert(FindModel(0x3837,0x303c)->keyCount==80 && FindModel(0x3837,0x3003)->keyCount==67);
    assert(!FindModel(0x41e4,0x2101) && !FindModel(0x3837,0x3007) && !FindModel(0x3837,0x3026) && !FindModel(0x41e4,0x211a));
    assert(Hid(16,128,0)==0xe7 && Hid(16,3,0)==0 && Hid(0xf0,255,1)==0);
    for(unsigned profile=0;profile<4;++profile)for(unsigned byte=0;byte<256;++byte)for(bool enable:{false,true}){
        SettingsPage config{};config.fill(0xff);for(unsigned i=0;i<256;++i)config[i]=static_cast<unsigned char>(i*17+5);
        const unsigned offset=profile*64+7;config[offset]=static_cast<unsigned char>(byte);const auto original=config;
        Base base{0,1,static_cast<unsigned char>(profile),0,0,0,0,0};unsigned writes=0;
        auto reader=[&](std::uint32_t address,std::uint8_t* p,std::size_t n){
            if(address==BaseAddress)std::memcpy(p,base.data(),n);else {assert(address==SettingsAddress);std::memcpy(p,config.data(),n);}return true;
        };
        auto exchange=[&](const Report& q,Report& r){++writes;assert(q[1]==6 && q[4]==1 && q[5]==offset && q[3]==Checksum(q));config[q[5]]=q[8];r=q;r[0]=0xaa;return Reply(q,r);};
        const auto result=ChangeFlag(base,enable,reader,exchange);
        assert(result==(bool(byte&8)==enable?ChangeResult::Unchanged:ChangeResult::Verified));
        assert(writes==(bool(byte&8)==enable?0u:1u));
        for(unsigned i=0;i<256;++i)assert(i==offset || config[i]==original[i]);
        assert(config[offset]==(enable?(byte|8):(byte&~8u)));
    }
    for(unsigned scenario=0;scenario<5;++scenario){
        Base consent{0,1,2,0,0,0,0,0};SettingsPage settings{};settings.fill(0xff);settings[135]=0;unsigned reads=0,writes=0;
        auto reader=[&](std::uint32_t address,std::uint8_t* p,std::size_t n){
            ++reads;if(scenario==0)return false;
            if(address==BaseAddress){auto base=consent;if(scenario==1)base[2]=1;std::memcpy(p,base.data(),n);}
            else std::memcpy(p,settings.data(),n);
            if(scenario==4 && writes)return false;
            return true;
        };
        auto exchange=[&](const Report& q,Report&){++writes;if(scenario!=3)settings[q[5]]=q[8];return scenario!=2;};
        const auto result=ChangeFlag(consent,true,reader,exchange);
        assert(result==(scenario==0?ChangeResult::ReadFailed:scenario==1?ChangeResult::StaleProfile:ChangeResult::Uncertain));
        assert(writes==(scenario<2?0u:1u));
    }
    {
        Base base{0,1,0,0,0,0,0,0};SettingsPage page{};page.fill(0xff);page[7]=0;page[4096]=0x42;
        unsigned writes=0;
        auto read=[&](std::uint32_t address,std::uint8_t* out,std::size_t size){
            if(address==BaseAddress)std::memcpy(out,base.data(),size);else std::memcpy(out,page.data(),size);return true;};
        auto write=[&](const Report&,Report&){++writes;return true;};
        assert(ChangeFlag(base,true,read,write)==ChangeResult::ReservedData && writes==0);
    }
    {
        Base base{0,1,0,0,0,0,0,0};SettingsPage page{};
        auto read=[&](std::uint32_t address,std::uint8_t* out,std::size_t size){
            if(address==BaseAddress)std::memcpy(out,base.data(),size);else std::memcpy(out,page.data(),size);return true;};
        auto write=[&](const Report& q,Report&){page[q[5]]=q[8];std::fill(page.begin()+256,page.end(),0xff);return true;};
        assert(ChangeFlag(base,true,read,write)==ChangeResult::Verified);
    }
    puts("Mix87 protocol PASS (model table): 2048 preserving opt-in transactions, no-op, stale profile, lost ACK, ignored write, failed readback, framing/depth/modifiers");
}

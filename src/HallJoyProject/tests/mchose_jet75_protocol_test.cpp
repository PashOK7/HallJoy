#include "../HallJoy/mchose_jet75_protocol.h"
#include <cassert>
#include <cstring>
#include <cstdio>
using namespace halljoy::jet75;
static Settings Stored(unsigned profile,std::uint8_t flagByte){
    Settings s{};for(unsigned i=0;i<256;++i)s[i]=static_cast<std::uint8_t>(i*17+5);
    for(unsigned p=0;p<4;++p){s[p*64+2]=0xaa;s[p*64+3]=0xbb;}
    s[0]&=0xfe;s[profile*64+7]=flagByte;return s;
}
int main(){
    const auto read=Read(ReadSettingsOp,200,56);
    assert(read[0]==0x55 && read[1]==5 && read[4]==56 && read[5]==200 && read[6]==0 && read[3]==Checksum(read));
    assert(Read(ReadSettingsOp,201,56)[0]==0 && Read(ReadSettingsOp,0,0)[0]==0 && Read(ReadSettingsOp,0,57)[0]==0 && Read(6,0,1)[0]==0);
    assert(Read(ReadBaseOp,0,6)[1]==4);
    Report reply=read;reply[0]=0xaa;reply[8]=0x42;reply[3]=Checksum(reply);assert(Reply(read,reply));
    reply[5]^=1;reply[3]=Checksum(reply);assert(!Reply(read,reply));
    auto info=Info();assert(info[1]==3 && info[4]==22 && info[3]==Checksum(info));
    Report version=info;version[0]=0xaa;version[8]=0x16;version[9]=1;std::memcpy(version.data()+10,"Aug 28 2025,10:45:06",20);
    version[3]=Checksum(version);assert(Reply(info,version) && Version(version)==ReviewedVersion);
    version[8]=0x17;assert(Version(version)==0x0117);
    const auto allowed=Allowed();unsigned count=0;for(bool a:allowed)count+=a;
    assert(count==79 && allowed[0x29] && allowed[0xe5] && !allowed[0x48] && !allowed[0xe7]);
    {   // Model table: exact VID:PID per reviewed image, no duplicates.
        assert(FindModel(0x41e4,0x211a)==&Models[0] && FindModel(0x41e4,0x2116)==&Models[1]);
        assert(std::size(Models)==11);
        for(std::size_t i=0;i<std::size(Models);++i){
            assert(FindModel(Models[i].vid,Models[i].pid)==&Models[i] && Models[i].keyCount>=60);
            const auto m=Allowed(Models[i]);unsigned c=0;for(bool k:m)c+=k;assert(c==Models[i].keyCount);
            for(std::size_t j=0;j<i;++j)assert(Models[i].vid!=Models[j].vid || Models[i].pid!=Models[j].pid);
        }
        // Not in the table: ARM / other-core revisions and the Ace 60 (ARM) board.
        for(unsigned pid:{0x2101u,0x2132u,0x3003u,0x300au,0x3024u,0x3028u,0x303cu,0x3007u,0x3026u,0x300du})
            assert(!FindModel(0x41e4,static_cast<std::uint16_t>(pid)) && !FindModel(0x3837,static_cast<std::uint16_t>(pid)));
        assert(!FindModel(0x3837,0x2116));
        const auto nordic=Allowed(*FindModel(0x3837,0x3002));assert(nordic[0x64] && nordic[0x65] && !nordic[0x49]);
        const auto mix=Allowed(*FindModel(0x41e4,0x2122));assert(mix[0x46] && mix[0x48] && mix[0x4d] && mix[0x35]);
        assert(Models[0].freshFlashReboot && Models[1].freshFlashReboot && FindModel(0x41e4,0x211c)->freshFlashReboot);
        assert(!FindModel(0x41e4,0x2118)->freshFlashReboot && !FindModel(0x41e4,0x2120)->freshFlashReboot);
        const auto ace=Allowed(Models[1]);unsigned n=0;for(bool k:ace)n+=k;
        assert(n==67 && Models[1].keyCount==67 && Models[1].reviewed==0x0121);
        assert(ace[0x49] && ace[0xe6] && ace[0x29] && !ace[0x35] && !ace[0x4a] && !ace[0x3a] && !ace[0x4d]);
        assert(!allowed[0xe6] && !allowed[0x49] && Models[0].keyCount==std::size(Keys));
        for(unsigned i=0;i<Models[1].keyCount;++i)for(unsigned j=0;j<i;++j)assert(Models[1].keys[i]!=Models[1].keys[j]);
        Sample s{};Report r{};r[0]=0xa0;r[1]=16;r[3]=0x49;r[14]=1;r[15]=0x5f;r[6]=0;r[7]=0xb0; // Insert, max 351
        assert(Decode(r,ace,s) && s.hid==0x49 && s.milli==(176*1000+175)/351 && !Decode(r,allowed,s));
        r[3]=0;r[2]=0x40;assert(Decode(r,ace,s) && s.hid==0xe6);
    }
    Sample sample{};Report a{};a[0]=0xa0;a[1]=16;a[3]=26;
    for(unsigned maximum:{341u,351u,331u}){
        a[14]=static_cast<std::uint8_t>(maximum>>8);a[15]=static_cast<std::uint8_t>(maximum);
        a[6]=a[14];a[7]=a[15];assert(Decode(a,allowed,sample) && sample.hid==26 && sample.milli==1000);
        a[6]=0;a[7]=10;assert(Decode(a,allowed,sample) && sample.milli==(10000+maximum/2)/maximum);
        a[6]=a[14];a[7]=static_cast<std::uint8_t>(a[15]+1);assert(!Decode(a,allowed,sample));
    }
    a[14]=1;a[15]=0x68;a[6]=0;a[7]=0;assert(Decode(a,allowed,sample));   // new switch type 360: accepted
    a[14]=0;a[15]=0;assert(!Decode(a,allowed,sample));                    // no maximum
    a[14]=3;a[15]=0xe8;assert(!Decode(a,allowed,sample));                 // implausible 1000
    a[14]=1;a[15]=0x55;a[3]=0x48;assert(!Decode(a,allowed,sample));      // Pause: not on this keyboard
    a[1]=16;a[2]=0x20;a[3]=0;assert(Decode(a,allowed,sample) && sample.hid==0xe5);
    a[2]=0;a[0]=0xa1;a[3]=26;assert(!Decode(a,allowed,sample));
    unsigned cases=0;
    for(unsigned profile=0;profile<4;++profile)for(unsigned byte=0;byte<256;++byte)for(bool enable:{false,true}){
        auto settings=Stored(profile,static_cast<std::uint8_t>(byte));const auto original=settings;
        Base base{0,1,static_cast<std::uint8_t>(profile),0,0,0};unsigned writes=0;
        auto rb=[&](Base& b){b=base;return true;};
        auto rs=[&](Settings& s){s=settings;return true;};
        auto exchange=[&](const Report& q,Report& r){
            ++writes;assert(q[1]==6 && q[4]==1 && q[5]==profile*64+7 && q[6]==0 && q[3]==Checksum(q));
            settings[q[5]]=q[8];r=q;r[0]=0xaa;return Reply(q,r);};
        const auto result=ChangeFlag(base,enable,rb,rs,exchange);
        assert(result==(bool(byte&8)==enable?ChangeResult::Unchanged:ChangeResult::Verified));
        assert(writes==(bool(byte&8)==enable?0u:1u));
        for(unsigned i=0;i<256;++i)assert(i==profile*64+7 || settings[i]==original[i]);
        assert(settings[profile*64+7]==(enable?(byte|8):(byte&~8u)));
        ++cases;
    }
    {   // Base selects the profile through its index: current=1 of 3 -> table[1].
        auto settings=Stored(3,0);Base base{1,3,0,3,1,0};unsigned profile=9;assert(Profile(base,profile) && profile==3);
        unsigned writes=0;
        auto result=ChangeFlag(base,true,[&](Base& b){b=base;return true;},[&](Settings& s){s=settings;return true;},
            [&](const Report& q,Report& r){++writes;assert(q[5]==3*64+7);settings[q[5]]=q[8];r=q;r[0]=0xaa;return true;});
        assert(result==ChangeResult::Verified && writes==1 && (settings[199]&8));
        // Firmware boot rules (2026-10-24): the base the keyboard itself accepts resolves to its profile.
        const auto resolve=[](Base b,BaseRule rule){unsigned p=9;return Profile(b,p,rule)?p:99u;};
        // Counted (Jet 75 II, Zero75X): current<count ? current : 0; entry>3 -> 0; current>=4 unknowable.
        assert(resolve({0,0,2,0,0,0},BaseRule::Counted)==2 && resolve({2,2,1,3,0,0},BaseRule::Counted)==1);
        assert(resolve({0,5,0,0,0,0},BaseRule::Counted)==0 && resolve({0,1,4,0,0,0},BaseRule::Counted)==0);
        assert(resolve({1,3,0,3,1,0},BaseRule::Counted)==3 && resolve({5,6,0,0,0,0},BaseRule::Counted)==99);
        assert(resolve({0xff,0xff,0xff,0xff,0xff,0xff},BaseRule::Counted)==0);
        // Bounded (Ace 60 Pro family): count ignored; current>3 -> 0; entry>3 -> 0.
        assert(resolve({2,0,0,1,3,2},BaseRule::Bounded)==3 && resolve({2,2,0,1,3,2},BaseRule::Bounded)==3);
        assert(resolve({7,0,1,2,3,0},BaseRule::Bounded)==1 && resolve({0,1,9,0,0,0},BaseRule::Bounded)==0);
        assert(resolve({0xff,0xff,0xff,0xff,0xff,0xff},BaseRule::Bounded)==0);
        // BoundedUnchecked (older writers): entry>3 would load outside the settings area.
        assert(resolve({1,0,0,2,0,0},BaseRule::BoundedUnchecked)==2 && resolve({9,0,3,0,0,0},BaseRule::BoundedUnchecked)==3);
        assert(resolve({0,1,4,0,0,0},BaseRule::BoundedUnchecked)==99 && resolve({0xff,0xff,0xff,0xff,0xff,0xff},BaseRule::BoundedUnchecked)==99);
        // Every valid base of the old strict check resolves identically under all three rules.
        for(unsigned count=1;count<=4;++count)for(unsigned current=0;current<count;++current)for(unsigned entry=0;entry<4;++entry){
            Base b{static_cast<std::uint8_t>(current),static_cast<std::uint8_t>(count),0,0,0,0};b[2+current]=static_cast<std::uint8_t>(entry);
            for(auto rule:{BaseRule::Counted,BaseRule::Bounded,BaseRule::BoundedUnchecked})assert(resolve(b,rule)==entry);
        }
        assert(Pack(Base{1,2,3,4,5,6})==0x060504030201ull);
        // Ace 60 Pro: flag transaction on a base with count 0 goes to the firmware's profile.
        auto ace=Stored(2,0);Base odd{1,0,0,2,0,0};unsigned aceWrites=0;
        auto aceResult=ChangeFlag(odd,true,[&](Base& b){b=odd;return true;},[&](Settings& s){s=ace;return true;},
            [&](const Report& q,Report& r){++aceWrites;assert(q[5]==2*64+7);ace[q[5]]=q[8];r=q;r[0]=0xaa;return true;},false,BaseRule::Bounded);
        assert(aceResult==ChangeResult::Verified && aceWrites==1 && (ace[2*64+7]&8));
        assert(FindModel(0x41e4,0x2103)->baseRule==BaseRule::Bounded && Models[0].baseRule==BaseRule::Counted);
        assert(FindModel(0x41e4,0x2118)->baseRule==BaseRule::BoundedUnchecked && FindModel(0x41e4,0x211c)->baseRule==BaseRule::Counted);
    }
    {   // Reboot-on-write firmware state: exactly one write, no readback, Rebooting.
        auto settings=Stored(0,0);settings[0]|=1;Base base{0,1,0,0,0,0};unsigned writes=0,reads=0;
        auto result=ChangeFlag(base,true,[&](Base& b){++reads;b=base;return true;},[&](Settings& s){++reads;s=settings;return true;},
            [&](const Report& q,Report&){++writes;settings[q[5]]=q[8];return false;});
        assert(result==ChangeResult::Rebooting && writes==1 && reads==3 && (settings[7]&8));
    }
    {   // Older writers do not reboot on the fresh-flash bit: normal readback path.
        auto settings=Stored(0,0);settings[0]|=1;Base base{0,1,0,0,0,0};unsigned writes=0;
        auto result=ChangeFlag(base,true,[&](Base& b){b=base;return true;},[&](Settings& s){s=settings;return true;},
            [&](const Report& q,Report& r){++writes;settings[q[5]]=q[8];r=q;r[0]=0xaa;return true;},false);
        assert(result==ChangeResult::Verified && writes==1 && (settings[7]&8));
    }
    for(unsigned scenario=0;scenario<6;++scenario){
        auto settings=Stored(2,0);Base consent{0,1,2,0,0,0};unsigned writes=0;
        if(scenario==5)settings[2*64+2]=0xff;
        auto rb=[&](Base& b){if(scenario==0)return false;b=consent;if(scenario==1)b[2]=1;return true;};
        auto rs=[&](Settings& s){if(scenario==4 && writes)return false;s=settings;return true;};
        auto exchange=[&](const Report& q,Report&){++writes;if(scenario!=3)settings[q[5]]=q[8];return scenario!=2;};
        const auto result=ChangeFlag(consent,true,rb,rs,exchange);
        assert(result==(scenario==0?ChangeResult::ReadFailed:scenario==1?ChangeResult::StaleProfile:
                        scenario==5?ChangeResult::ReservedData:ChangeResult::Uncertain));
        assert(writes==(scenario<2 || scenario==5?0u:1u));
    }
    printf("Jet75 protocol PASS: %u preserving flag transactions, base index, reboot-on-write, no-op, stale profile, lost ACK, ignored write, failed readback, foreign format, framing/version/depth range/modifiers, 11-model table, per-model reboot rule, firmware base rules\n",cases);
}

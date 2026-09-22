#include "keychron_onboard_client.h"
#include "keychron_onboard_session.h"
#include <cassert>
#include <iostream>
using namespace halljoy::k4_onboard;
struct Fake : Channel {
    uint64_t now=10; bool cancel=false,wrongToken=false,corruptPage=false;
    uint8_t failCommand=0;
    unsigned nextPage=0;
    bool burstSupported=true;
    std::array<uint8_t,HJK4_COMPACT_BYTES> compact{};
    hjo_session session{};
    unsigned heartbeats=0,commits=0,maxHeartbeatGap=0,lastHeartbeat=0;
    bool native=false,ready=false;
    uint32_t crc=0,scan=0;
    std::array<uint8_t,HJO_PROFILE_BYTES> upload{};
    std::array<uint8_t,HJK4_FRAME_BYTES> frame{};
    bool Exchange(const Packet& q,Packet& r) override {
        now+=5; hjo_tick(&session,static_cast<uint32_t>(now));
        if(q[1]==failCommand) return false;
        const auto token=hjk4_u32(q.data()+4);
        uint8_t status=0;
        switch(q[1]) {
        case 0x70: case 0x7A: break;
        case 0x71: if(!hjo_open(&session,static_cast<uint32_t>(now))) status=1; else native=true; break;
        case 0x72: upload.fill(0); break;
        case 0x73: {
            auto offset=hjk4_u16(q.data()+8); assert(offset+q[10]<=upload.size());
            std::memcpy(upload.data()+offset,q.data()+11,q[10]); break;
        }
        case 0x74: {
            hjo_profile p{}; if(!hjo_profile_decode(&p,upload.data(),upload.size())) status=3;
            else {ready=true; crc=hjk4_u32(upload.data()+5040); ++commits;} break;
        }
        case 0x75:
            if(!hjo_start(&session,token,hjk4_u32(q.data()+8),static_cast<uint32_t>(now))) status=4;
            lastHeartbeat=static_cast<unsigned>(now); break;
        case 0x76:
            if(!hjo_heartbeat(&session,token,hjk4_u32(q.data()+8),static_cast<uint32_t>(now))) status=4;
            maxHeartbeatGap=std::max(maxHeartbeatGap,static_cast<unsigned>(now)-lastHeartbeat);
            lastHeartbeat=static_cast<unsigned>(now); ++heartbeats; break;
        case 0x77: if(!hjo_host_stop(&session,token)) status=4; else native=false; break;
        case 0x7B: {
            uint8_t travel[114]{};travel[40]=120;
            hjk4_compact_encode(compact.data(),session.generation?session.generation:1,++scan,2345,HJK4_CALIBRATED,travel);
            nextPage=0;break;
        }
        case 0x78:
            if(!q[2]) { uint16_t depth[114]{}; depth[40]=32767;
                assert(hjk4_encode(frame.data(),frame.size(),1,session.generation?session.generation:1,
                    ++scan,static_cast<uint32_t>(now*1000),2345,HJK4_CALIBRATED,depth)); }
            break;
        default: assert(false);
        }
        r={}; r[0]=0xa9; r[1]=q[1]; r[2]=status; r[3]=static_cast<uint8_t>(session.phase);
        hjk4_put32(r.data()+4,session.generation+(wrongToken?1:0));
        if(q[1]==0x7B) { FillBurst(r); } else if(q[1]==0x78) {
            unsigned offset=q[2]*22, count=std::min(22u,256-offset);
            r[8]=q[2]; r[9]=static_cast<uint8_t>(count);
            std::memcpy(r.data()+10,frame.data()+offset,count);
            if(corruptPage) r[10]^=1;
        } else {
            std::memcpy(r.data()+8,"HJO1",4); hjk4_put32(r.data()+12,crc);
            r[16]=native; r[17]=static_cast<uint8_t>(ready)|(burstSupported?4:0); hjk4_put16(r.data()+18,HJO_PROFILE_BYTES);
        }
        return true;
    }
    void FillBurst(Packet& r) {
        r={};r[0]=0xa9;r[1]=0x7B;r[3]=static_cast<uint8_t>(session.phase);
        hjk4_put32(r.data()+4,session.generation+(wrongToken?1:0));
        const unsigned offset=nextPage*22,count=std::min(22u,HJK4_COMPACT_BYTES-offset);
        r[8]=static_cast<uint8_t>(nextPage++);r[9]=static_cast<uint8_t>(count);
        std::memcpy(r.data()+10,compact.data()+offset,count);
        if(corruptPage)r[10]^=1;
    }
    bool Read(Packet& r) override {now+=3;if(nextPage>=6)return false;FillBurst(r);return true;}
    bool Reconnect(uint16_t revision) override {now+=1000; hjo_tick(&session,static_cast<uint32_t>(now)); return native==(revision==0x1213);}
    uint64_t NowMs() const override {return now;}
    bool Cancelled() const override {return cancel;}
};
hjo_profile Profile() {
    hjo_profile p{}; std::memset(p.mapping.axes,255,8); std::memset(p.mapping.triggers,255,2);
    p.mapping.sensitivity=.02f;
    for(auto& c:p.curves) {c={{0,.3f,.7f,1},{0,.3f,.7f,1},{1,1},1,0};}
    return p;
}
int main() {
    auto p=Profile(); Fake f; Client c(f);
    Packet capability{};assert(c.Status(capability));
    std::array<uint16_t,114> depth{};
    assert(c.Depth(depth) && depth[40]==32767); // idle monitor needs no gamepad
    assert(c.Open(p) && c.Active());
    assert(f.commits==1);
    p.mapping.flags=HJO_SNAP;
    assert(c.Update(p)); // >1 second upload must not starve 500ms lease
    assert(f.heartbeats>10 && f.maxHeartbeatGap<=90 && f.session.phase==HJO_ACTIVE);
    assert(c.Update(p) && f.commits==2); // unchanged profile isn't uploaded
    assert(c.Depth(depth));
    std::array<uint8_t,20> pad{};assert(c.Pad(pad));
    f.corruptPage=true; const auto previous=depth;
    assert(!c.Depth(depth) && depth==previous); f.corruptPage=false;
    f.wrongToken=true; f.now+=100; assert(!c.KeepAlive()); f.wrongToken=false;
    assert(c.Close() && !c.Active() && !f.native);
    for(uint8_t command:{0x71,0x72,0x73,0x74,0x75}) {
        Fake broken; broken.failCommand=command; Client other(broken);
        assert(!other.Open(Profile()) && !other.Active());
        broken.now+=6000; hjo_tick(&broken.session,static_cast<uint32_t>(broken.now));
        assert(broken.session.phase==HJO_OFF);
    }
    Fake legacy;legacy.burstSupported=false;Client old(legacy);Packet info{};assert(old.Status(info) && old.Depth(depth));
    Fake stopped; stopped.cancel=true; Client other(stopped); assert(!other.Open(Profile()));
    std::cout<<"Client lifecycle, upload heartbeat priority, integrity and failure paths PASS\n";
}

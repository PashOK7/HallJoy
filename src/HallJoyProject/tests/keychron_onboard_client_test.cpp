#include "keychron_onboard_client.h"
#include "keychron_onboard_session.h"
#include <cassert>
#include <iostream>
using namespace halljoy::k4_onboard;
struct Fake : Channel {
    uint64_t now=10; bool cancel=false,wrongToken=false,corruptPage=false;
    uint8_t failCommand=0;
    unsigned nextPage=0;
    bool burstSupported=true,sparseSupported=false,repeatScan=false;
    bool reorder=false,mixScan=false;
    unsigned sparseCursor=0,readDelay=3;
    std::array<uint16_t,HJO_SLOTS> precise{};
    std::array<uint8_t,HJK4_COMPACT_BYTES> compact{};
    hjo_session session{};
    unsigned heartbeats=0,commits=0,maxHeartbeatGap=0,lastHeartbeat=0;
    bool native=false,ready=false,r8=true,stagingCommitted=false;
    unsigned chunks=0,reconnects=0,deltaRefusals=0;
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
        case 0x7E: if(!repeatScan)++scan;sparseCursor=0;nextPage=0;FillSparse(r);return true;
        case 0x71: if(!hjo_open(&session,static_cast<uint32_t>(now))) status=1; else {native=true;ready=false;} break;
        case 0x72:
            if(q[8]==1) {
                if(!r8 || !stagingCommitted || hjk4_u32(q.data()+12)!=crc) {status=2;++deltaRefusals;}
                else stagingCommitted=false;
            } else {upload.fill(0);stagingCommitted=false;}
            break;
        case 0x73: {
            auto offset=hjk4_u16(q.data()+8); assert(offset+q[10]<=upload.size());
            std::memcpy(upload.data()+offset,q.data()+11,q[10]); ++chunks; break;
        }
        case 0x74: {
            hjo_profile p{}; if(!hjo_profile_decode(&p,upload.data(),upload.size())) status=3;
            else {ready=true; crc=hjk4_u32(upload.data()+5040); ++commits; stagingCommitted=true;} break;
        }
        case 0x75:
            if(!hjo_start(&session,token,hjk4_u32(q.data()+8),static_cast<uint32_t>(now))) status=4;
            lastHeartbeat=static_cast<unsigned>(now); break;
        case 0x76:
            if(!hjo_heartbeat(&session,token,hjk4_u32(q.data()+8),static_cast<uint32_t>(now))) status=4;
            maxHeartbeatGap=std::max(maxHeartbeatGap,static_cast<unsigned>(now)-lastHeartbeat);
            lastHeartbeat=static_cast<unsigned>(now); ++heartbeats; break;
        case 0x77:
            if(q[8]==1 && r8) {if(!hjo_host_park(&session,token,static_cast<uint32_t>(now))) status=4; else hjo_neutral_delivered(&session);}
            else if(!hjo_host_stop(&session,token)) status=4; else native=false;
            break;
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
            r[16]=native; r[17]=static_cast<uint8_t>(static_cast<uint8_t>(ready)|(burstSupported?4:0)|(sparseSupported?16:0)|
                (r8?HJO_CAP_DELTA_UPLOAD|HJO_CAP_PARK:0)); hjk4_put16(r.data()+18,HJO_PROFILE_BYTES);
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
    void FillSparse(Packet& r) {
        hjk4_sparse_encode(r.data(),precise.data(),&sparseCursor,static_cast<uint8_t>(nextPage++),
            (session.generation?session.generation:1)+(wrongToken?1:0),scan,1);
        if(reorder)r[12]++;
        if(mixScan && nextPage>1)hjk4_put32(r.data()+8,scan+1);
        hjk4_put16(r.data()+30,hjk4_sparse_crc(r.data()));
        if(corruptPage)r[14]^=1;
    }
    // Reports another program on the shared RAW endpoint provokes (a VIA reply).
    unsigned foreign=0;
    bool Read(Packet& r) override {
        if(foreign){--foreign;r={};r[0]=0x01;r[1]=0x0c;return true;}
        now+=readDelay;
        if(sparseSupported){if(sparseCursor>=HJO_SLOTS)return false;FillSparse(r);return true;}
        if(nextPage>=6)return false;FillBurst(r);return true;
    }
    bool Reconnect(uint16_t revision) override {++reconnects;now+=1000; hjo_tick(&session,static_cast<uint32_t>(now)); return native==(revision==0x1213);}
    uint64_t NowMs() const override {return now;}
    bool Cancelled() const override {return cancel;}
};
hjo_profile Profile() {
    hjo_profile p{}; std::memset(p.mapping.axes,255,sizeof(p.mapping.axes)); std::memset(p.mapping.triggers,255,2);
    p.mapping.sensitivity=.02f;
    for(auto& c:p.curves) {c={{0,.3f,.7f,1},{0,.3f,.7f,1},{1,1},1,0};}
    return p;
}
int main() {
    {
        // r8: toggling one mapping flag sends one chunk; pause parks without
        // re-enumeration; resume reopens and sends only what changed.
        auto p=Profile(); Fake f; CommittedProfile shared; Client c(f,&shared);
        assert(c.Open(p) && f.chunks==241 && f.reconnects==1);
        f.chunks=0; p.mapping.flags=HJO_SUPPRESS;
        assert(c.Update(p) && f.chunks==2 && f.commits==2 && f.deltaRefusals==0); // flag chunk + CRC chunk
        assert(c.Park() && !c.Active() && f.session.phase==HJO_PARKED && f.native && f.reconnects==1);
        Client resumed(f,&shared); f.chunks=0;
        assert(resumed.Open(p) && resumed.Active() && f.reconnects==1 && f.chunks==0 && f.commits==3);
        p.mapping.flags=0; f.chunks=0;
        assert(resumed.Update(p) && f.chunks==2);
        assert(resumed.Park());
        Client exiting(f); assert(exiting.ReleaseParked() && f.session.phase==HJO_OFF);
        Client idle(f); assert(idle.ReleaseParked()); // nothing parked: no-op
        // A delta against a base the firmware does not hold falls back to full.
        Fake g; CommittedProfile stale; Client d(g,&stale);
        assert(d.Open(p)); stale.crc^=1; f.chunks=0; g.chunks=0; p.mapping.flags=HJO_SNAP;
        assert(d.Update(p) && g.deltaRefusals==1 && g.chunks==241);
        // Firmware without r8 capabilities: full uploads, Park is an orderly STOP.
        Fake old; old.r8=false; Client o(old);
        assert(o.Open(p) && o.Update(Profile()) && old.chunks==482 && old.deltaRefusals==0);
        assert(o.Park() && old.session.phase==HJO_OFF && !old.native);
    }
    auto p=Profile(); Fake f; f.r8=false; Client c(f); // full uploads: long-transfer lease case
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
    Fake fast;fast.sparseSupported=true;Client fresh(fast);
    assert(fresh.Status(info));
    fast.precise[0]=1;fast.precise[40]=12345;fast.precise[113]=65535;
    assert(fresh.Depth(depth) && depth==fast.precise && fast.nextPage==1);
    fast.precise.fill(0);assert(fresh.Depth(depth) && depth==fast.precise && fast.nextPage==1);
    for(unsigned i=0;i<HJO_SLOTS;++i)fast.precise[i]=static_cast<uint16_t>(i+1);
    assert(fresh.Depth(depth) && depth==fast.precise && fast.nextPage==23);
    const auto intact=depth;
    fast.corruptPage=true;assert(!fresh.Depth(depth) && depth==intact);fast.corruptPage=false;
    fast.reorder=true;assert(!fresh.Depth(depth) && depth==intact);fast.reorder=false;
    fast.mixScan=true;assert(!fresh.Depth(depth) && depth==intact);fast.mixScan=false;
    fast.readDelay=20;assert(!fresh.Depth(depth) && depth==intact);fast.readDelay=3;
    assert(fresh.Depth(depth));fast.repeatScan=true;assert(!fresh.Depth(depth));fast.repeatScan=false;
    assert(fresh.Open(Profile()));fast.wrongToken=true;
    assert(!fresh.Depth(depth) && depth==intact);fast.wrongToken=false;
    assert(fresh.Depth(depth));
    // The vendor interface is opened shared: foreign reports between pages are
    // skipped (bounded), more than the bound fail the read without touching depth.
    fast.foreign=8;assert(fresh.Depth(depth) && depth==intact && fast.foreign==0);
    fast.foreign=9;assert(!fresh.Depth(depth) && depth==intact);fast.foreign=0;
    assert(fresh.Close());
    {   Fake burst;Client b(burst);std::array<uint16_t,114> values{};
        assert(b.Status(info) && b.Depth(values));
        burst.foreign=8;assert(b.Depth(values) && burst.foreign==0);
        burst.foreign=9;assert(!b.Depth(values));
    }
    // Reject malformed records even when packet CRC is recomputed correctly.
    for(unsigned mutation=0;mutation<7;++mutation) {
        Packet packet{};unsigned cursor=0;hjk4_sparse_assembly assembly{};
        hjk4_sparse_encode(packet.data(),fast.precise.data(),&cursor,0,1,1,1);
        switch(mutation) {
        case 0:packet[17]=packet[14];break; // duplicate slot
        case 1:packet[14]=114;break;
        case 2:hjk4_put16(packet.data()+15,0);break;
        case 3:packet[3]=0;break; // uncalibrated
        case 4:packet[29]=1;break;
        case 5:packet[13]=4;break; // short nonfinal page
        case 6:packet[2]=2;break;
        }
        hjk4_put16(packet.data()+30,hjk4_sparse_crc(packet.data()));
        assert(hjk4_sparse_append(&assembly,packet.data())==-1);
    }
    // Every chord size, page boundary and wire bit; no partial publication.
    for(unsigned keys=0;keys<=HJO_SLOTS;++keys) {
        std::array<uint16_t,HJO_SLOTS> input{};
        for(unsigned i=0;i<keys;++i)input[i]=static_cast<uint16_t>(65535-i*17);
        unsigned cursor=0;hjk4_sparse_assembly assembly{};
        for(unsigned page=0;page<HJK4_SPARSE_PAGES;++page) {
            Packet packet{};hjk4_sparse_encode(packet.data(),input.data(),&cursor,
                static_cast<uint8_t>(page),9,123,1);
            const auto before=assembly;
            for(unsigned bit=0;bit<256;++bit) {
                auto bad=packet;bad[bit/8]^=static_cast<uint8_t>(1u<<(bit%8));auto trial=before;
                assert(hjk4_sparse_append(&trial,bad.data())==-1);
            }
            const int result=hjk4_sparse_append(&assembly,packet.data());assert(result>=0);
            if(result) {assert(std::equal(input.begin(),input.end(),assembly.depth));break;}
        }
        assert(assembly.complete);
    }
    Fake stopped; stopped.cancel=true; Client other(stopped); assert(!other.Open(Profile()));
    std::cout<<"Client lifecycle, upload heartbeat priority, integrity and failure paths PASS\n";
}

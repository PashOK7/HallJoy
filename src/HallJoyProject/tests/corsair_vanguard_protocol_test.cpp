#include "corsair_vanguard_client.h"
#include <cassert>
#include <iostream>
#include <string>
using namespace halljoy::corsair_vanguard;

struct FakeTransport {
    unsigned layout=1,session=0,nextSession=1,handleFile[2]{},directoryReads=0;
    unsigned calibrationReads=0,switchOpens=0,closes=0,reads=0;
    std::size_t offset[2]{};
    bool saved=true,switches=true,wrongFirmware=false,wrongVendor=false,badCalibration=false,
        badCalibrationPrefix=false,badSnapshot=false,badSnapshotOpcode=false,
        directoryRace=false,failIo=false,failCleanup=false,badSwitch=false,largeDirectory=false,
        badCalTail=false,badCalRange=false,calibrationRace=false,badCalSpan=false,
        badSnapshotTail=false,sentinelSnapshot=false,badBragi=false;
    unsigned snapshotPhase=0;
    std::vector<std::uint8_t> file;
    bool Exchange(const Frame& q,Frame& r,ReplyKind kind,bool cleanup) {
        assert(q[0]==0 && q[1]==1);
        assert(q[3]==2 || q[3]==5 || q[3]==8 || q[3]==9 || q[3]==13 || q[3]==27);
        if(failIo && !cleanup)return false;
        if(failCleanup && cleanup)return false;
        r={};r[2]=q[2];r[3]=q[3];
        if(q[3]==27) {
            if(q[4]==1) {assert(!q[2] && !session);session=nextSession++;}
            else {assert(q[4]==2 && q[9]==session && q[2]==session);++closes;}
            std::copy(q.begin()+5,q.begin()+9,r.begin()+5);r[9]=static_cast<std::uint8_t>(session);
            if(q[4]==2)session=0;
            return true;
        }
        assert(session && q[2]==session);
        if(q[3]==13) {
            auto id=U16(q.data()+5);assert(id==0x11 || (id==0x4b && switches));
            assert(q[4]<2);handleFile[q[4]]=id;offset[q[4]]=0;
            if(id==0x4b)++switchOpens;
            return true;
        }
        if(q[3]==5) {assert(q[4]==1 && q[5]==0 && handleFile[0]==0x11);handleFile[0]=0;return true;}
        if(q[3]==9 || q[3]==8) {
            const auto id=handleFile[q[4]];assert(id==0x11 || id==0x4b);
            if(id==0x11) {
                file={0x5b,0xab,0,0};std::vector<unsigned> ids;
                if(saved)ids.push_back(0xca);if(switches)ids.push_back(0x4b);
                if(largeDirectory)for(unsigned i=0;i<600;++i)ids.push_back(0x7000+i);
                if(directoryRace && directoryReads)ids.push_back(0x6d91);
                file[2]=static_cast<std::uint8_t>(ids.size());file[3]=static_cast<std::uint8_t>(ids.size()>>8);
                for(auto value:ids){file.push_back(static_cast<std::uint8_t>(value));file.push_back(static_cast<std::uint8_t>(value>>8));}
            } else file={0x67,0,1,4,static_cast<std::uint8_t>(badSwitch?6:5),0,0};
            if(q[3]==9)Put32(r.data()+7,static_cast<std::uint32_t>(file.size()));
            else {
                auto& position=offset[q[4]];
                const auto count=std::min<std::size_t>(1019,file.size()-position);
                std::copy(file.begin()+position,file.begin()+position+count,r.begin()+5);
                if(id==0x11 && position==0)++directoryReads;
                position+=count;
            }
            return true;
        }
        const auto prop=U16(q.data()+4);
        if(prop==0x11){r[5]=wrongVendor?0:0x1c;r[6]=0x1b;}
        else if(prop==0x12){r[5]=0x0e;r[6]=0x2b;}
        else if(prop==0x13){r[5]=2;r[6]=8;r[7]=wrongFirmware?170:171;}
        else if(prop==0x105){r[5]=1;r[6]=badBragi?6:5;}
        else if(prop==0x41)r[5]=static_cast<std::uint8_t>(layout);
        else if(prop==0xc6) {
            assert(kind==ReplyKind::Calibration);++calibrationReads;
            assert(q[6]==(saved?6:3) || q[6]==(saved?5:2));
            const unsigned count=FindLayout(layout)->count;
            for(unsigned i=0;i<count;++i) {
                unsigned value=q[6]==6 || q[6]==3?6200+i:2100+i;
                if(badCalibration)value=0;
                if(badCalRange)value=0x4000;
                if(badCalSpan && (q[6]==5 || q[6]==2))value=6190+i;
                if(calibrationRace && calibrationReads==3)++value;
                r[3+2*i]=static_cast<std::uint8_t>(value);r[4+2*i]=static_cast<std::uint8_t>(value>>8);
            }
            if(badCalibrationPrefix)r[2]^=1;
            if(badCalTail)r[3+count*2]=1;
        } else if(prop==0xc7) {
            assert(kind==ReplyKind::Normal);++reads;
            const auto* l=FindLayout(layout);assert(l);
            for(unsigned i=0;i<l->count;++i) {
                unsigned raw=6200+i;
                if((snapshotPhase==1 || snapshotPhase==2) && (l->hid[i]==4 || l->hid[i]==26))raw-=snapshotPhase==2?1:600;
                if(badSnapshot && l->hid[i])raw=0;
                if(sentinelSnapshot && l->hid[i])raw=0x3fff;
                r[5+i*2]=static_cast<std::uint8_t>(raw);r[6+i*2]=static_cast<std::uint8_t>(raw>>8);
            }
            if(badSnapshotOpcode)r[3]=8;
            if(badSnapshotTail)r[5+l->count*2]=1;
        } else assert(false);
        return true;
    }
};
int main() {
    for(unsigned id:{1,2,4})for(bool saved:{false,true})for(bool switches:{false,true}) {
        FakeTransport io;io.layout=id;io.saved=saved;io.switches=switches;
        {
            Client<FakeTransport> client(io);assert(client.Initialize(0x87654321));
            assert(client.CurrentStage()==Stage::Sampling && io.closes==1);
            assert(client.profile.mapped==(id==1?96u:id==2?97u:98u));
            assert(io.switchOpens==(switches?1u:0u));
            assert(client.profile.calibration[6].bottom==(saved?2106:1906));
            std::array<std::uint16_t,100> values{};assert(client.Read(values));
            for(auto v:values)assert(v==0);
            io.snapshotPhase=2;assert(client.Read(values) && client.rawChanged==2);
            for(auto v:values)assert(!v);
            io.snapshotPhase=1;assert(client.Read(values));
            unsigned active=0;for(auto v:values)active+=v!=0;assert(active==2);
            io.snapshotPhase=0;assert(client.Read(values));for(auto v:values)assert(v==0);
            // Sensor0 is physical N, not a missing-key marker.
            assert(client.profile.layout->hid[0]==17);
            io.badSnapshot=true;assert(!client.Read(values) && client.reject==Reject::SnapshotZero);
            io.badSnapshot=false;io.badSnapshotOpcode=true;assert(!client.Read(values) && client.reject==Reject::ReplyPrefix);
            io.badSnapshotOpcode=false;io.badSnapshotTail=true;
            assert(!client.Read(values) && client.reject==Reject::SnapshotTail);
            io.badSnapshotTail=false;io.sentinelSnapshot=true;
            assert(!client.Read(values) && client.reject==Reject::SnapshotSentinel);
        }
        assert(!io.session && io.closes==2);
    }
    {FakeTransport io;io.largeDirectory=true;Client<FakeTransport> client(io);
     assert(client.Initialize(17));assert(io.directoryReads==2);}
    for(unsigned failure=0;failure<8;++failure) {
        FakeTransport io;
        if(failure==0)io.wrongVendor=true;
        if(failure==1)io.wrongFirmware=true;
        if(failure==2)io.layout=3;
        if(failure==3)io.badCalibration=true;
        if(failure==4)io.badCalibrationPrefix=true;
        if(failure==5)io.directoryRace=true;
        if(failure==6)io.badSwitch=true;
        if(failure==7)io.failIo=true;
        {Client<FakeTransport> client(io);assert(!client.Initialize(123));
         const Reject expected[]={Reject::Vendor,Reject::Firmware,Reject::Layout,Reject::CalibrationEndpoints,
             Reject::ReplyPrefix,Reject::DirectoryChanged,Reject::SwitchRecord,Reject::Transport};
         assert(client.reject==expected[failure]);
         if(failure<3)assert(io.calibrationReads==0 && io.switchOpens==0);}
        assert(!io.session);
    }
    for(unsigned failure=0;failure<5;++failure) {
        FakeTransport io;
        if(failure==0)io.badCalTail=true;
        if(failure==1)io.badCalRange=true;
        if(failure==2)io.calibrationRace=true;
        if(failure==3)io.badCalSpan=true;
        if(failure==4)io.badBragi=true;
        Client<FakeTransport> client(io);assert(!client.Initialize(9));
        const Reject expected[]={Reject::CalibrationTail,Reject::CalibrationRange,Reject::CalibrationChanged,Reject::CalibrationSpan,Reject::Bragi};
        assert(client.reject==expected[failure]);
    }
    // The non-monotonic reviewed curve must retain the firmware's descending
    // lookup. A binary search over its knots would lose this 1.8mm result.
    Calibration c{6200,2100,41,3};assert(Normalize(6200-620,c)==450);
    assert(!FindLayout(0) && !FindLayout(3) && !FindLayout(5));
    std::cout<<"CORSAIR_CLIENT=PASS (12 complete sessions, releases, admission and malformed-data cases)\n";
    return 0;
}

#pragma once
// Isolated queued-control fixtures for the production receiver Client. No HID I/O.
#include "rongyuan_receiver_client.h"
#include "physical_analog_state.h"
#include "keydous_rf_release_trace.h"
#include <cstring>
#include <vector>
namespace halljoy::receiver_sync_cases {
using ry_receiver::Report;
struct Queued {
    Report visible{},cached{},scheduled{};
    std::array<std::uint16_t,128> depth{};
    unsigned delayMarker=0,delayStatus=0,delayFetch=0,left=0,kind=0;
    unsigned operations=0,checks=0,failAt=0,cancelAt=0,version=0x0801;
    unsigned enabled=0,disabled=0,drainedPage=4,analog=0;
    bool pending=false,selected=false,good=true,noMarker=false,wrongEcho=false,echoMarker=false,invalid=false,offline=false,holdRemote=false,cancelOnDispatch=false;
    std::uint64_t clock=0;
    std::vector<unsigned> writes;
    bool Check(){return ++checks!=cancelAt;}
    std::uint64_t ClockUs()const{return clock;}
    void YieldBusy(){} // No elapsed time except modeled I/O.
    void WaitDispatch(unsigned us){clock+=us;if(cancelOnDispatch){cancelOnDispatch=false;cancelAt=checks+1;}}
    void Status(bool){}
    bool Step(){clock+=1000;return ++operations!=failAt;}
    void DrainedPage(unsigned page,const Report&){drainedPage=page;}
    Report StatusReply()const {
        Report r{};r[1]=pending && !holdRemote?1:0;r[2]=1;r[4]=offline?1:0;r[5]=1;r[6]=1;r[7]=1;return r;
    }
    Queued(){visible=StatusReply();}
    bool Write(const Report& r){
        if(!Step())return false;
        writes.push_back(r[1]);
        // A status poll can supersede another status poll, but never a pending
        // marker or FC. Overwriting a queued FC would lose response ownership.
        if(kind && kind!=0xf7){good=false;return false;}
        kind=0;
        if(r[1]==0xf0){
            unsigned sum=0;for(unsigned i=1;i<=8;++i)sum+=r[i];
            if((sum&255)!=255 || r[8]<128){good=false;return false;}
            if(noMarker)return true;
            scheduled={};scheduled[1]=0xf0;scheduled[8]=std::uint8_t(r[8]+(wrongEcho?1:0));
            for(unsigned i=0;i<4;++i)scheduled[2+i]=std::uint8_t(version>>(8*i));
            if(echoMarker)scheduled=r;
            kind=0xf0;left=delayMarker;
        }else if(r[1]==0xf7){scheduled=StatusReply();kind=0xf7;left=delayStatus;}
        else if(r[1]==0xf6){selected=true;}
        else if(r[1]==0xfc){scheduled=cached;kind=0xfc;left=delayFetch;}
        else if(r[1]==0x1b){if(r[2])++enabled;else ++disabled;}
        else if(r[1]==0x8f){
            cached={};cached[1]=0x8f;cached[2]=std::uint8_t(3793);cached[3]=std::uint8_t(3793>>8);pending=true;
        }else if(r[1]==0xe6){pending=false;} // Optional metadata with no reply.
        else if(r[1]==0xe5){
            if(!selected || r[4]>=4){good=false;return false;}
            cached={};pending=true;++analog;
            for(unsigned i=0;i<32;++i){unsigned v=invalid?65535:depth[32*r[4]+i];cached[1+2*i]=std::uint8_t(v);cached[2+2*i]=std::uint8_t(v>>8);}
        }else {good=false;return false;}
        return true;
    }
    bool Read(Report& r){
        if(!Step())return false;
        if(kind){
            if(left)--left;
            else {visible=scheduled;if(kind==0xfc)pending=false;kind=0;}
        }
        r=visible;return true;
    }
};
// SET_REPORT stages a SINGLE MCU mailbox. Another SET before the worker runs
// replaces it. GET_REPORT does not acknowledge remote-command forwarding.
// The worker delay is synthetic2ms, within the vendor10ms handoff interval.
struct Mailbox : Queued {
    bool delayed=false,outstanding=false;
    unsigned overwritten=0;
    std::uint64_t due=0;
    Report outbound{};
    void Service(){
        if(outstanding && clock>=due){
            outstanding=false;
            if(!Queued::Write(outbound))good=false;
        }
    }
    void WaitDispatch(unsigned us){clock+=us;Service();}
    bool Write(const Report& r){
        Service();
        if(outstanding){outstanding=false;++overwritten;}
        if(delayed && (r[1]==0xe5 || r[1]==0x1b)){
            if(!Step())return false;
            outbound=r;outstanding=true;due=clock+2000;return true;
        }
        return Queued::Write(r);
    }
    bool Read(Report& r){Service();return Queued::Read(r);}
};
inline bool Proof(Queued& io,ry_receiver::Client<Queued>& c){
    Report r{};
    return c.Query(ry_stream::Request(0x8f),r,[](const Report& x){return ry_receiver::Model(ry_stream::Board(x))!=nullptr;}) && io.good;
}
inline bool Positive(unsigned which){
    Queued io;ry_receiver::Client c(io);if(!Proof(io,c))return false;
    c.RequireResponseSync();
    io.delayMarker=which==3?5:0;io.delayStatus=which==1?7:0;io.delayFetch=which==2?11:0;
    if(which==4)io.version=0xfedcba98; // Version is evidence, never an admission allowlist.
    const unsigned units=(which==5 || which==8)?1000:100;
    if(which==12){io.visible={};io.visible[1]=0xf0;io.visible[8]=129;}
    if(which==10 || which==13 || which==15){
        io.noMarker=which==10;io.wrongEcho=which==13;io.echoMarker=which==15;rongyuan::Values p{};p.fill(77);
        if(c.Page(0,100,p) || c.stage!=ry_receiver::Stage::MarkerReply)return false;
        for(auto v:p)if(v!=77)return false;
        if(which==15 && c.sync.lastKind!=4)return false;
        if(!c.ResponseSyncUnconfirmed() || !c.SendKeyboard(ry_stream::Request(0x1b,1)) || io.enabled!=1)return false;
        const auto operations=io.operations;
        if(c.Page(1,100,p) || io.operations!=operations || !c.MaintainLink())return false;
        unsigned probes=0;for(auto op:io.writes)probes+=op==0xf0;
        if(probes!=1)return false;
        unsigned char data[32]={5,0x1b,0,0,14};ry_stream::Sample sample{};
        if(!ry_stream::Parse(data,32,sample) || sample.raw)return false;
        if(!c.SendKeyboard(ry_stream::Request(0x1b,0)))return false;
        return io.good && io.disabled==1 && !io.analog; // Real stream attempted; unsafe table never issued.
    }
    if(which==14){
        if(!c.BeginResponseSync())return false;
        Report optional{};
        if(c.Query(ry_stream::Request(0xe6),optional,[](const Report& r){return r[1]==0xe6;}))return false;
        if(!c.HasPendingReply())return false;
        c.StreamOnlyForPendingMetadata();rongyuan::Values p{};
        if(!c.ResponseSyncUnconfirmed() || c.HasPendingReply() || c.sync.reason!=2 || c.sync.abandonedCommand!=0xe6 || c.Page(0,100,p))return false;
        return c.SendKeyboard(ry_stream::Request(0x1b,1)) && c.SendKeyboard(ry_stream::Request(0x1b,0)) && io.enabled==1 && io.disabled==1 && io.good;
    }
    if(which==11){
        io.depth[0]=11;io.depth[32]=22;io.delayFetch=250;rongyuan::Values p{};p.fill(77);
        if(c.Page(0,100,p) || c.stage!=ry_receiver::Stage::FetchPending)return false;
        for(auto v:p)if(v!=77)return false;
        io.delayFetch=0;
        return c.Page(1,100,p) && p[0]==22 && io.drainedPage==0 && io.good;
    }
    std::array<std::uint16_t,128> initial{};
    if(!c.Frame(units,initial) || !c.SendKeyboard(ry_stream::Request(0x1b,1)))return false;
    // Every valid source page is admitted, including byte-for-byte F7/F0 shapes.
    if(which==7){io.depth[0]=257;io.depth[2]=257;io.depth[3]=1;}
    else if(which==8){io.depth[0]=496;io.depth[3]=3840;}
    else if(which!=9){io.depth[14]=units*2;io.depth[9]=units;}
    if(which==6)for(unsigned page=0;page<4;++page)io.depth[32*page]=37;
    std::array<std::uint16_t,128> frame{};
    if(!c.Frame(units,frame) || frame!=io.depth)return false;
    if(!c.Frame(units,frame) || frame!=io.depth)return false; // Identical consecutive pages are valid.
    physical_analog::Publication published;
    if(!published.Bind(15,26) || !published.Bind(10,4))return false;
    published.Publish(15,ry_stream::Normalize(frame[14],units,4000),100,true);
    published.Publish(10,ry_stream::Normalize(frame[9],units,4000),100,true);
    if(which<6 && (published.Read(26,100).milli!=500 || published.Read(4,100).milli!=250))return false;
    io.depth[14]=0;
    if(!c.Frame(units,frame) || frame!=io.depth)return false;
    io.depth.fill(0);
    if(!c.Frame(units,frame) || frame!=io.depth || !c.SendKeyboard(ry_stream::Request(0x1b,0)))return false;
    published.Clear();
    return io.good && c.sync.acks>0 && io.enabled==1 && io.disabled==1 && !published.Read(26,101).milli;
}
inline bool Negative(unsigned which){
    Queued io;ry_receiver::Client c(io);if(!Proof(io,c))return false;c.RequireResponseSync();
    if(which==0){io.invalid=true;rongyuan::Values p{};p.fill(77);if(c.Page(0,100,p) || c.snapshotProblem!=3)return false;for(auto v:p)if(v!=77)return false;return io.good;}
    if(which==1){io.delayFetch=100000;c.timeoutUs=9000;rongyuan::Values p{};p.fill(77);if(c.Page(0,100,p) || c.stage!=ry_receiver::Stage::FetchPending)return false;for(auto v:p)if(v!=77)return false;return io.good && c.HasPendingReply();}
    // Exhaust every I/O and cancellation boundary in one fresh page transaction.
    Queued baseline;ry_receiver::Client base(baseline);if(!Proof(baseline,base))return false;base.RequireResponseSync();
    const auto start=which==2?baseline.operations:baseline.checks;rongyuan::Values page{};
    if(!base.Page(0,100,page))return false;
    const auto end=which==2?baseline.operations:baseline.checks;
    for(unsigned point=start+1;point<=end;++point){
        Queued fault;ry_receiver::Client test(fault);if(!Proof(fault,test))return false;test.RequireResponseSync();
        if(which==2)fault.failAt=point;else fault.cancelAt=point;
        page.fill(77);if(test.Page(0,100,page))return false;for(auto v:page)if(v!=77)return false;
    }
    return true;
}
inline bool RecoveryPositive(unsigned which){
    if(which==4){
        for(unsigned gap:{0u,ry_receiver::Client<Mailbox>::kRemoteDispatchUs}){
            Mailbox io;ry_receiver::Client c(io,false,gap);Report identity{};
            if(!c.Query(ry_stream::Request(0x8f),identity,[](const Report& r){return ry_stream::Board(r)==3793;}))return false;
            c.RequireResponseSync();io.delayed=true;io.depth[64]=123;
            rongyuan::Values values{};const bool okay=c.Page(2,100,values);
            if(!gap){if(okay || !io.overwritten || c.stage!=ry_receiver::Stage::ReadReady)return false;}
            else {
                if(!okay || io.overwritten || values[0]!=123 || !c.dispatchWaits)return false;
                if(!c.SendKeyboard(ry_stream::Request(0x1b,1)) || io.enabled!=1)return false;
                if(!c.SendKeyboard(ry_stream::Request(0x1b,0)) || io.disabled!=1)return false;
            }
            if(!io.good)return false;
        }
        return true;
    }
    if(which==5){
        keydous_rf_trace::Control state{};state.resumedPages=7;state.dispatchWaits=13;
        state.dispatchUs=10000;state.pendingPage=2;state.pendingAgeUs=41063000;
        char line[256]{};keydous_rf_trace::FormatProgress(line,sizeof line,2,state);
        return std::strstr(line,"resumed_pages=7") && std::strstr(line,"dispatch_gap_us=10000") &&
            std::strstr(line,"pending_page=2") && std::strstr(line,"pending_age_us=41063000");
    }
    Queued io;ry_receiver::Client c(io);if(!Proof(io,c))return false;c.RequireResponseSync();
    const auto* model=ry_receiver::Model(3793);physical_analog::Publication publication;ry_receiver::StreamOrder order;
    unsigned channels=0;
    for(unsigned slot=0;slot<128;++slot){
        const auto hid=ry_stream::Decode(model->matrix.data()+4*slot);if(!hid)continue;
        if(!publication.Bind(std::uint8_t(slot+1),hid))return false;
        publication.Publish(std::uint8_t(slot+1),500,1,true);order.Event(slot,200);++channels;
    }
    if(channels!=97)return false;
    for(unsigned page=0;page<(which==3?4u:1u);++page){
        ry_receiver::StreamOrder::Ticket ticket{};unsigned sends=0;
        io.holdRemote=which!=1;io.delayFetch=which==1?1000:0;c.timeoutUs=9000;
        rongyuan::Values values{};values.fill(77);
        if(c.Page(page,100,values,[&]{ticket=order.Before(page);++sends;}))return false;
        if(which==1 && c.stage!=ry_receiver::Stage::FetchPending)return false;
        if(which!=1 && c.stage!=ry_receiver::Stage::ReadReady)return false;
        for(auto value:values)if(value!=77)return false;
        if(which==2){order.Event(9,300);publication.Publish(10,750,2,true);}
        const auto before=io.analog;
        io.holdRemote=false;io.left=0;
        if(!c.Page(page,100,values,[&]{ticket=order.Before(page);++sends;}))return false;
        if(sends!=1 || io.analog!=before || c.drains || c.HasPendingReply())return false;
        for(unsigned i=0;i<32;++i){
            const auto slot=page*32+i;
            if(ry_stream::Decode(model->matrix.data()+4*slot) && order.Accept(page,i,ticket)){
                publication.Publish(std::uint8_t(slot+1),ry_stream::Normalize(values[i],100,4000),3,true);
                order.Table(slot,values[i]);
            }
        }
    }
    if(which==2){if(publication.Read(4,3,1500,true).milli!=750)return false;}
    else if(publication.Read(4,3,1500,true).milli)return false;
    if(which==3)for(unsigned slot=0;slot<128;++slot)if(publication.PhysicalMilli(std::uint8_t(slot+1)))return false;
    publication.Clear();
    return c.resumedPages==(which==3?4u:1u) && io.good && c.SendKeyboard(ry_stream::Request(0x1b,0));
}
inline bool RecoveryNegative(unsigned which){
    Queued io;ry_receiver::Client c(io,false,which==1?10000:0);if(!Proof(io,c))return false;c.RequireResponseSync();
    rongyuan::Values values{};values.fill(77);
    if(which==1){
        io.cancelOnDispatch=true;
        if(c.SendKeyboard(ry_stream::Request(0x1b,1)) || c.stage!=ry_receiver::Stage::Cancelled || io.enabled!=1)return false;
        io.cancelAt=0;
        return c.SendKeyboard(ry_stream::Request(0x1b,0)) && io.disabled==1;
    }
    io.holdRemote=true;io.invalid=which==3;c.timeoutUs=9000;
    if(c.Page(0,100,values))return false;
    const auto sent=io.analog;io.holdRemote=false;
    if(which==2)io.cancelAt=io.checks+1;
    if(c.Page(0,which==0?200:100,values))return false;
    for(auto value:values)if(value!=77)return false;
    return io.analog==sent && (which==2?c.stage==ry_receiver::Stage::Cancelled:c.stage==ry_receiver::Stage::ResponseShape);
}
struct Result {unsigned positive=0,negative=0;};
inline Result Run(){
    Result r;for(unsigned i=0;i<16;++i)if(Positive(i))++r.positive;
    for(unsigned i=0;i<4;++i)if(Negative(i))++r.negative;
    for(unsigned i=0;i<6;++i)if(RecoveryPositive(i))++r.positive;
    for(unsigned i=0;i<4;++i)if(RecoveryNegative(i))++r.negative;
    return r;
}
}

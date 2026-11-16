#pragma once
#include "rongyuan_receiver_client.h"
#include "rongyuan_receiver_start.h"
#include "keyboard_support_status.h"
#include "support_notice_catalog.h"
#include "rongyuan_receiver_sync_cases.h"
#include "keydous_rf_release_trace.h"
#include <cstring>
#include "mchose_ace68gt_protocol.h"
#include "mchose_ace60_protocol.h"
#include "physical_analog_state.h"
#include "generated/layout_pipeline/identities.h"
#include <cwchar>

namespace halljoy::tester_cases {
struct HatorRadio {
    rongyuan::Report request{},command{},cached{};
    std::array<std::uint16_t,128> depth{};
    unsigned board=3628,enabled=0,disabled=0,analogReads=0;
    bool offline=false,boot=false,badStatus=false,badDepth=false,pending=false,cancel=false;
    bool shapeOk=true,hator=true,readNever=false,markerEcho=true;
    unsigned drainedPage=4;
    rongyuan::Report drainedReply{};
    std::uint64_t now=0;
    bool Write(const rongyuan::Report& report) {
        ++now;
        command=report;
        if(report[1]==0xf7 || report[1]==0xfc || report[1]==0xf6)return true;
        const auto checksum=static_cast<std::uint8_t>(255u-report[1]-report[2]-report[3]-report[4]);
        const unsigned tail=!hator && (report[1]==0xe5 || report[1]==0xf0)?255:0;
        if(report[8]!=checksum || report[9]!=tail){shapeOk=false;return false;}
        if(report[1]==0xf0)return true;
        if(report[1]==0x1b){
            if(report[2])++enabled;else ++disabled;
            return true;
        }
        pending=true;
        request=report;
        return true;
    }
    bool Read(rongyuan::Report& report) {
        ++now;
        report={};
        if(command[1]==0x1b){report=cached;return true;} // Fire-and-forget does not replace the USB reply.
        if(command[1]==0xf0){report[1]=0xf0;report[2]=1;report[3]=8;report[8]=markerEcho?command[8]:0;cached=report;return true;}
        if(command[1]==0xf7){
            report[1]=pending && !readNever?1:0;report[4]=offline?1:0;report[6]=1;
            report[7]=badStatus?9:1;report[9]=boot?1:0;
            cached=report;return true;
        }
        if(command[1]!=0xfc)return false;
        if(!pending){report=cached;return true;}
        pending=false;
        if(request[1]==0x8f){
            report[1]=0x8f;report[2]=static_cast<std::uint8_t>(board);
            report[3]=static_cast<std::uint8_t>(board>>8);
            cached=report;return true;
        }
        if(request[1]!=0xe5 || request[2]!=0xfe || request[3]!=1 || request[4]>=4)return false;
        ++analogReads;
        for(unsigned i=0;i<32;++i){
            const unsigned value=badDepth?0xffff:depth[request[4]*32+i];
            report[1+2*i]=static_cast<std::uint8_t>(value);
            report[2+2*i]=static_cast<std::uint8_t>(value>>8);
        }
        cached=report;return true;
    }
    void DrainedPage(unsigned page,const rongyuan::Report& report){drainedPage=page;drainedReply=report;}
    bool Check() const {return !cancel;}
    std::uint64_t ClockUs() const {return now;}
    void WaitDispatch(unsigned us){now+=us;}
    void YieldBusy(){now+=1000;}
    void Status(bool){}
};

inline bool HatorPositive(unsigned board) {
    HatorRadio io;
    io.board=board;
    ry_receiver::Client radio(io,true);
    rongyuan::Report identity{};
    const ry_stream::Model* model=nullptr;
    if(!radio.Query(ry_stream::HatorRequest(0x8f),identity,[&](const auto& r){
        model=ry_receiver::Model(ry_stream::Board(r),0x379a,0x1804);
        return model!=nullptr;
    }))return false;
    const auto token=layout_identity::Token("rongyuan-stream",model->product);
    const bool iso=board==2858 || board==3629;
    if(token!=layout_identity::Token("rongyuan-stream",iso?"RY1B-2858":"RY1B-2857") && board!=2800)return false;
    if(!token)return false;
    physical_analog::Publication publication;
    unsigned a=128,w=128;
    for(unsigned slot=0;slot<128;++slot){
        const auto hid=ry_stream::Decode(model->matrix.data()+4*slot);
        if(hid && !publication.Bind(static_cast<std::uint8_t>(slot+1),hid))return false;
        if(hid==4)a=slot;
        if(hid==26)w=slot;
    }
    if(a==128 || w==128)return false;
    std::array<std::uint16_t,128> frame{};
    if(!radio.Frame(100,frame) || !radio.SendKeyboard(ry_stream::HatorRequest(0x1b,1)))return false;
    io.depth[a]=70;io.depth[w]=175;
    if(!radio.Frame(100,frame))return false;
    for(unsigned slot=0;slot<128;++slot)
        publication.Publish(static_cast<std::uint8_t>(slot+1),ry_stream::Normalize(frame[slot],100,model->rangeUm),100);
    if(publication.Read(4,100).milli!=200 || publication.Read(26,100).milli!=500)return false;
    // Use the production interrupt decoder and order gate. A delta that
    // arrives during E5 must beat the older in-flight table result.
    ry_receiver::StreamOrder order;
    const auto ticket=order.Before(a/32);
    unsigned char stream[32]={5,0x1b,0,0,static_cast<unsigned char>(a)};
    ry_stream::Sample delta{};
    if(!ry_stream::Parse(stream,sizeof stream,delta))return false;
    order.Event(delta.slot,delta.raw);
    if(order.Accept(a/32,a%32,ticket))return false;
    publication.Publish(static_cast<std::uint8_t>(delta.slot+1),0,101);
    if(publication.Read(4,101).milli || publication.Read(26,101).milli!=500)return false;
    if(!radio.SendKeyboard(ry_stream::HatorRequest(0x1b,0)))return false;
    publication.Clear();
    return !publication.Read(26,102).milli && io.shapeOk && io.enabled==1 && io.disabled==1;
}

inline bool HatorNegative(unsigned scenario) {
    HatorRadio io;
    ry_receiver::Client radio(io,true);
    const bool collection=ry_receiver::Identity(0x379a,0x1804,0xffff,2,65,scenario==0?1:2);
    if(scenario==0)return !collection && io.now==0;
    if(!collection)return false;
    io.board=scenario==1?2856:3628;
    io.offline=scenario==2;io.boot=scenario==3;io.badStatus=scenario==4;
    io.cancel=scenario==5;io.badDepth=scenario==6;
    rongyuan::Report reply{};
    const bool proved=radio.Query(ry_stream::HatorRequest(0x8f),reply,[](const auto& r){
        return ry_receiver::Model(ry_stream::Board(r),0x379a,0x1804)!=nullptr;
    });
    constexpr ry_receiver::Stage expected[]={ry_receiver::Stage::None,ry_receiver::Stage::ResponseShape,
        ry_receiver::Stage::Offline,ry_receiver::Stage::Bootloader,ry_receiver::Stage::StatusShape,
        ry_receiver::Stage::Cancelled,ry_receiver::Stage::ResponseShape};
    if(scenario==6){
        rongyuan::Values page{};
        if(!proved || radio.Page(0,100,page) || radio.snapshotProblem!=3)return false;
    }else if(proved || io.analogReads)return false;
    return radio.stage==expected[scenario] && !io.enabled && !io.disabled;
}

inline int Hator() {
    unsigned passed=0,negatives=0;
    for(const unsigned board:{2800u,2857u,2858u,3627u,3628u,3629u})if(HatorPositive(board))++passed;
    for(unsigned scenario=0;scenario<7;++scenario)if(HatorNegative(scenario))++negatives;
    const bool ok=passed==6 && negatives==7;
    wprintf(L"HATOR_ULTIMA_SELF_TEST=%ls scenarios=%u negatives=%u proof_to_subscribe=1 first_sample=1 multikey=1 release=1 cleanup=1 hardware_opened=0\n",
        ok?L"PASS":L"FAIL",passed,negatives);
    return ok?0:94;
}

inline bool KeydousRfScenario(unsigned scenario) {
    namespace rf=keydous_rf_trace;
    HatorRadio io;io.hator=false;io.board=scenario==4?2857:3793;
    ry_receiver::Client radio(io);
    rongyuan::Report reply{};
    const ry_stream::Model* model=nullptr;
    const bool proved=radio.Query(ry_stream::Request(0x8f),reply,[&](const auto& report){
        const auto board=ry_stream::Board(report);
        model=ry_receiver::Model(board);
        return model && rf::Matches(0x3151,0x5038,board);
    });
    if(scenario==4)return !proved && io.analogReads==0 && io.enabled==0;
    if(!proved || !layout_identity::Token("rongyuan-stream",model->product))return false;

    radio.RequireResponseSync();
    std::array<std::uint16_t,128> initial{};
    if(!radio.Frame(100,initial) || !radio.SendKeyboard(ry_stream::Request(0x1b,1)))return false;
    rf::State state;state.session=1;state.units=100;
    ry_receiver::StreamOrder order;
    physical_analog::Publication publication;
    const unsigned slot=14,other=9,page=slot/32;
    if(!publication.Bind(slot+1,26) || !publication.Bind(other+1,4))return false;
    const auto delta=[&](unsigned index,unsigned raw,std::uint64_t time){
        unsigned char input[32]={5,0x1b,static_cast<unsigned char>(raw),static_cast<unsigned char>(raw>>8),
                                static_cast<unsigned char>(index)};
        ry_stream::Sample sample{};
        if(!ry_stream::Parse(input,sizeof input,sample))return false;
        state.slots[index].Stream(sample.raw,time);
        publication.Publish(static_cast<std::uint8_t>(index+1),ry_stream::Normalize(sample.raw,100,4000),time,true);
        order.Event(index,sample.raw);
        state.slots[index].Applied(rf::Source::Stream,sample.raw,time);
        return true;
    };
    if(!delta(slot,54,20) || !delta(other,148,21) ||
       publication.Read(26,21).milli!=135 || publication.Read(4,21).milli!=370)return false;

    auto& status=state.pages[page];status.Begin(25);
    io.depth[slot]=scenario==2?54:0;
    io.badDepth=scenario==6;
    io.readNever=scenario==5;
    ry_receiver::StreamOrder::Ticket ticket{};
    rongyuan::Values values{};
    const bool ok=radio.Page(page,100,values,[&]{
        status.Sent(26);
        ticket=order.Before(page);
        if(scenario==3)delta(slot,55,27);
    });
    status.Finish(ok,static_cast<unsigned>(radio.stage),0,radio.snapshotProblem,
                  radio.drains,1,radio.HasPendingReply(),30);
    auto& trace=state.slots[slot];
    if(scenario>=5){
        char line[640]{};rf::FormatPage(line,sizeof line,state.session,page,status);
        if(ok || status.completed || !status.failed || !std::strstr(line,"failed=1"))return false;
        if(scenario==5 && (!radio.HasPendingReply() || radio.stage!=ry_receiver::Stage::ReadReady ||
                           !std::strstr(line,"pending=1")))return false;
        if(scenario==6 && (radio.HasPendingReply() || radio.snapshotProblem!=3 ||
                           !std::strstr(line,"snapshot_reason=3")))return false;
        io.readNever=false;
    }else{
        if(!ok || !status.completed)return false;
        if(scenario==0 && !delta(slot,0,29))return false;
        const bool accepted=order.Accept(page,slot%32,ticket);
        const unsigned raw=values[slot%32];
        trace.Table(raw,accepted,status.attempts,30);
        if(accepted){
            publication.Publish(slot+1,ry_stream::Normalize(raw,100,4000),30,true);
            order.Table(slot,raw);trace.Applied(rf::Source::Table,raw,30);
        }
        char line[640]{};
        rf::FormatSlot(line,sizeof line,state.session,slot,26,trace,publication.PhysicalMilli(slot+1),publication.Read(26,30).milli);
        if(scenario==0 && (trace.streamZeroMs!=29 || trace.streamZeroAppliedMs!=29 || trace.appliedRaw || !std::strstr(line,"applied_source=1")))return false;
        if(scenario==1 && (trace.streamZeroMs || trace.tableZeroMs!=30 || trace.tableZeroAppliedMs!=30 || trace.appliedRaw ||
                           !std::strstr(line,"applied_source=2")))return false;
        if(scenario==2 && (!accepted || trace.tableRaw!=54 || publication.Read(26,30000,1500,true).milli!=135))return false;
        if(scenario==3 && (accepted || trace.skippedZeroMs!=30 || trace.appliedRaw!=55 ||
                           !std::strstr(line,"table_accepted=0")))return false;
    }
    if(!radio.SendKeyboard(ry_stream::Request(0x1b,0)) || io.disabled!=1)return false;
    if(scenario==5) {
        rongyuan::Values late{};
        if(io.drainedPage!=page || ry_receiver::ParsePage(io.drainedReply,page,100,late) ||
           late[slot%32] || publication.PhysicalMilli(slot+1)!=135)return false;
        trace.Drained(late[slot%32],50);
        char line[640]{};
        rf::FormatSlot(line,sizeof line,state.session,slot,26,trace,publication.PhysicalMilli(slot+1),135);
        if(!std::strstr(line,"drained_zero_ms=50"))return false;
    }
    publication.Clear();
    return io.shapeOk && publication.PhysicalMilli(slot+1)==0 && !publication.Read(4,31).milli;
}
inline int KeydousRf() {
    unsigned positives=0,negatives=0;
    for(unsigned scenario=0;scenario<7;++scenario)if(KeydousRfScenario(scenario)){
        if(scenario<4)++positives;else ++negatives;
    }
    const auto sync=receiver_sync_cases::Run();
    positives+=sync.positive;negatives+=sync.negative;
    const bool ok=positives==26 && negatives==11;
    wprintf(L"KEYDOUS_RF_TRACE_SELF_TEST=%ls scenarios=%u negatives=%u proof_to_subscribe=1 first_sample=1 multikey=1 release=1 cleanup=1 hardware_opened=0 sync_scenarios=%u sync_negatives=%u late_page_publication=1 mailbox_handoff=1\n",
        ok?L"PASS":L"FAIL",positives,negatives,sync.positive,sync.negative);
    return ok?0:96;
}

inline bool He108RfCase(unsigned scenario) {
    constexpr unsigned vid=0x3151,pid=0x5038,board=3365;
    HatorRadio io;
    io.hator=false;
    io.board=scenario==3?3496:board;
    io.markerEcho=scenario!=1;
    io.offline=scenario==4;
    io.badDepth=scenario==2;
    ry_receiver::Client radio(io,false,ry_receiver::Client<HatorRadio>::kRemoteDispatchUs);
    const bool eligible=ry_receiver::Identity(vid,pid,0xffff,2,65,scenario==5?1:2);
    if(scenario==5)return !eligible && io.now==0;
    if(!eligible)return false;
    rongyuan::Report reply{};
    const ry_stream::Model* model=nullptr;
    const bool proof=radio.Query(ry_stream::Request(0x8f),reply,[&](const auto& r){
        model=ry_receiver::Model(ry_stream::Board(r),vid,pid);
        return model && ry_receiver::He108Trial(vid,pid,model->board);
    });
    if(scenario==3 || scenario==4){
        const auto expected=scenario==3?ry_receiver::Stage::ResponseShape:ry_receiver::Stage::Offline;
        return !proof && radio.stage==expected && !io.analogReads && !io.enabled;
    }
    const auto token=layout_identity::Token("rongyuan-stream",model?model->product:"");
    const auto* preset=layout_identity::Match(token);
    if(!proof || !preset || std::wcscmp(preset,L"EPOMAKER HE108 ANSI") ||
       model->rangeUm!=3300)return false;
    const bool sync=radio.BeginResponseSync();
    if(sync!=io.markerEcho)return false;
    if(scenario==8){
        io.readNever=true;radio.timeoutUs=9000;rongyuan::Report optional{};
        if(radio.Query(ry_stream::Request(0xe6),optional,[](const auto&r){return r[1]==0xe6;}))return false;
        radio.StreamOnlyForPendingMetadata();
        if(!radio.ResponseSyncUnconfirmed() || radio.HasPendingReply())return false;
    }
    struct Pump {
        HatorRadio& io;ry_receiver::Client<HatorRadio>& radio;
        bool reader=false,stopped=false,enableAttempt=false;
        bool Start(){reader=true;enableAttempt=true;return !io.analogReads && radio.SendKeyboard(ry_stream::Request(0x1b,1));}
        void Stop(){reader=false;stopped=true;}
    } pump{io,radio};
    bool attempted=false,active=false;
    if(scenario==10)io.cancel=true;
    const bool eligibleInput=scenario!=7;
    if(!ry_receiver::StartBeforeTable(vid,pid,board,eligibleInput,attempted,active,pump,[&]{
        io.cancel=false;if(pump.enableAttempt)radio.EndStream(ry_stream::Request(0x1b,0));
    }))return false;
    if(scenario==10)return attempted && !active && pump.stopped && !io.analogReads && !io.enabled && io.disabled==1;
    if(!attempted || active!=eligibleInput || (eligibleInput && (!pump.reader || io.enabled!=1)))return false;
    if(scenario==6){io.readNever=true;radio.timeoutUs=9000;}
    std::array<std::uint16_t,128> depths{};
    if(scenario<3 && (radio.Frame(200,depths)!=(scenario==0) || radio.TrialNeedsStreamFallback()!=(scenario!=0)))return false;
    if(scenario==1 && (!radio.ResponseSyncUnconfirmed() || io.analogReads))return false;
    if(scenario==2 && (!sync || radio.snapshotProblem!=3 || io.analogReads!=1))return false;
    if(scenario==6 && (radio.Frame(200,depths) || radio.stage!=ry_receiver::Stage::ReadReady || !radio.HasPendingReply() || io.enabled!=1))return false;
    if(scenario==8 && (radio.Frame(200,depths) || io.analogReads || io.enabled!=1))return false;

    physical_analog::Publication output;
    unsigned a=128,w=128,mapped=0;
    for(unsigned slot=0;slot<128;++slot){
        const auto hid=ry_stream::Decode(model->matrix.data()+4*slot);
        if(!hid)continue;
        if(!output.Bind(static_cast<std::uint8_t>(slot+1),hid))return false;
        ++mapped;
        if(hid==4)a=slot;
        if(hid==26)w=slot;
    }
    if(a==128 || w==128 || mapped!=104)return false;
    const auto notice=keyboard_support::ResearchNotice(22,vid,pid,true,false,false,L"EPOMAKER HE108");
    if(notice || keyboard_support::ResearchNotice(22,vid,pid,true,true,false,L"EPOMAKER HE108") ||
       keyboard_support::NativeNotice(22,layout_identity::Token("rongyuan-stream","RY1B-3365"),true,pid))return false;
    if(scenario==7){
        io.depth[a]=132;io.depth[w]=330;
        if(!radio.Frame(200,depths) || io.enabled)return false;
        for(unsigned slot=0;slot<128;++slot)output.Publish(static_cast<std::uint8_t>(slot+1),ry_stream::Normalize(depths[slot],200,3300),100);
        if(output.Read(4,100).milli!=200 || output.Read(26,100).milli!=500)return false;
        io.depth.fill(0);if(!radio.Frame(200,depths))return false;
        output.Clear();return !io.enabled && !io.disabled && pump.stopped;
    }
    const auto delta=[&](unsigned slot,unsigned raw){
        const unsigned char packet[32]={5,0x1b,static_cast<unsigned char>(raw),
            static_cast<unsigned char>(raw>>8),static_cast<unsigned char>(slot)};
        ry_stream::Sample sample{};
        if(!ry_stream::Parse(packet,sizeof packet,sample))return false;
        output.Publish(static_cast<std::uint8_t>(sample.slot+1),
                       ry_stream::Normalize(sample.raw,200,model->rangeUm),100);
        return true;
    };
    if(!delta(a,0) || output.Read(4,100).milli || !delta(a,132) || !delta(w,330) ||
       output.Read(4,100).milli!=200 || output.Read(26,100).milli!=500 ||
       !delta(w,0) || output.Read(26,100).milli || output.Read(4,100).milli!=200 ||
       !delta(a,0) || output.Read(4,100).milli)return false;
    if(scenario==9){
        struct Caps{unsigned char ReportID;};Caps split[]={{5},{5},{6}};
        if(!ry_receiver::StreamCollection(0xffff,1,32,ry_receiver::DeclaresReport5(split,3)) ||
           ry_receiver::StreamCollection(0xffff,1,31,true) || ry_receiver::StreamCollection(1,6,32,true))return false;
        for(unsigned slot=0;slot<128;++slot)if(ry_stream::Decode(model->matrix.data()+4*slot) && !delta(slot,330))return false;
        for(unsigned slot=0;slot<128;++slot)if(ry_stream::Decode(model->matrix.data()+4*slot) && output.PhysicalMilli(static_cast<std::uint8_t>(slot+1))!=500)return false;
        for(unsigned slot=0;slot<128;++slot)if(ry_stream::Decode(model->matrix.data()+4*slot) && !delta(slot,0))return false;
    }
    pump.Stop();
    if(!radio.EndStream(ry_stream::Request(0x1b,0)))return false;
    rongyuan::Values stoppedPage{};
    if(scenario==6 && (radio.Page(1,200,stoppedPage) || radio.stage!=ry_receiver::Stage::Cancelled))return false;
    output.Clear();
    return io.shapeOk && io.enabled==1 && io.disabled==1 && !output.Read(26,101).milli;
}

inline int He108Rf() {
    unsigned positives=0,negatives=0;
    for(unsigned scenario=0;scenario<11;++scenario)if(He108RfCase(scenario)){
        if(scenario<3 || (scenario>=6 && scenario<=9))++positives;else ++negatives;
    }
    const bool ok=positives==7 && negatives==4;
    wprintf(L"HE108_RF_SELF_TEST=%ls scenarios=%u negatives=%u proof_to_subscribe=1 first_sample=1 multikey=1 release=1 cleanup=1 layout=1 hardware_opened=0 stream_before_table=1 pending_cleanup=1 supported_no_notice=1 physical_channels=104\n",
        ok?L"PASS":L"FAIL",positives,negatives);
    return ok?0:99;
}

struct GtProfile {
    ace68gt::Base base{0,3,0,1,2,3,0xaa,0xbb};
    ace68gt::Settings settings{};
    unsigned reads=0,writes=0;
    bool stale=false,badRead=false,unexpectedWrite=false;
    GtProfile(){for(unsigned p=0;p<4;++p){settings[p*64+2]=0xaa;settings[p*64+3]=0xbb;}}
    bool ReadBase(ace68gt::Base& out) {
        out=base;
        if(stale && ++reads==2)out[0]=1;
        return !badRead;
    }
    bool ReadSettings(ace68gt::Settings& out){out=settings;return !badRead;}
    bool Write(const ace68gt::Report& request) {
        const unsigned offset=request[5];
        if(request[0]!=0x55 || request[1]!=6 || request[4]!=1 || request[6]!=0 ||
           offset%64!=7 || request[3]!=jet75::Checksum(request) || (settings[offset]^request[8])!=8)return false;
        ++writes;
        settings[offset]=request[8];
        // Dynamic byte 0x20 can change across the write/readback.
        ++settings[(offset/64)*64+0x20];
        if(unexpectedWrite)settings[(offset/64)*64+20]^=1;
        return true;
    }
};

inline ace68gt::Report GtVersion(unsigned scenario) {
    const unsigned version=scenario<4?0x0113:scenario==5?0x117a:scenario==8?0x0114:0x0116;
    ace68gt::Report reply{};
    reply[0]=0xaa;reply[1]=3;
    reply[4]=static_cast<std::uint8_t>(version==0x0116?27:20+scenario%3);
    reply[8]=static_cast<std::uint8_t>(version);reply[9]=static_cast<std::uint8_t>(version>>8);
    // Log 48 supplies the length and version. Unobserved build metadata is synthetic.
    if(version!=0x0116)std::fill_n(reply.begin()+10,4,std::uint8_t{0xff});
    reply[15]=version==0x117a?'J':version==0x0116?'X':'F';
    reply[3]=jet75::Checksum(reply);
    if(scenario==12)reply[3]^=1;
    return reply;
}
inline bool GtScenario(unsigned scenario) {
    using ace68gt::Mode;
    GtProfile io;
    auto readBase=[&](auto& value){return io.ReadBase(value);};
    auto readSettings=[&](auto& value){return io.ReadSettings(value);};
    auto write=[&](const auto& q){return io.Write(q);};
    if(scenario==1)io.base={1,0,0,2,0,0,0xaa,0xbb};
    if(scenario==2)io.settings[7]=8; // Leave an already enabled profile alone.
    if(scenario==3){io.base[0]=255;io.base[2]=255;} // Same fallback as the firmware.
    if(scenario==6)io.settings[0]=1;
    if(scenario==7)io.base[6]=0;
    if(scenario==9)io.stale=true;
    if(scenario==10)io.badRead=true;
    if(scenario==11)return !ace68gt::IsControl(2,1,0,65,65) && !io.writes;
    if(scenario==14)io.unexpectedWrite=true;
    const auto info=GtVersion(scenario);
    const bool infoOk=ace68gt::InfoReply(info);
    if(infoOk!=(scenario!=12) || !ace68gt::IsControl(1,1,0,65,65))return false;
    const auto writer=ace68gt::WriterPolicy(info);
    if(scenario<6 && writer!=ace68gt::Writer::Reviewed)return false;
    if(!infoOk && writer!=ace68gt::Writer::Unavailable)return false;
    ace68gt::Base base{};
    bool wrote=false;
    const auto mode=infoOk?ace68gt::Activate(writer!=ace68gt::Writer::Unavailable,scenario!=13,base,wrote,
        readBase,readSettings,write):Mode::ProbeFailed;
    constexpr Mode expected[]={Mode::Verified,Mode::Verified,Mode::AlreadySet,Mode::Verified,Mode::Verified,
        Mode::Verified,Mode::RebootArmed,Mode::ProfileInvalid,Mode::UnreviewedWriter,Mode::Stale,Mode::ProbeFailed,
        Mode::ProbeFailed,Mode::ProbeFailed,Mode::AttemptSpent,Mode::Uncertain};
    if(mode!=expected[scenario])return false;
    if(scenario==14)return wrote && io.writes==1;
    if(scenario>=6)return !wrote && !io.writes;
    const auto* model=jet75::FindModel(0x3837,0x3007);
    if(!model || model->keyCount!=67 ||
       !layout_identity::Token(model->layoutProtocol,model->layoutProduct))return false;
    ace68gt::Travel travel;
    const auto allowed=jet75::Allowed(*model);
    ace68gt::Report report{};
    report[0]=0xa0;report[1]=0x10;report[3]=4;report[7]=100;report[14]=1;report[15]=0x55;
    jet75::Sample sample{};
    if(!travel.Consume(report,allowed,sample) || sample.hid!=4 || sample.milli!=293)return false;
    report[3]=26;report[7]=200;
    if(!travel.Consume(report,allowed,sample) || travel.active!=2 || travel.peak!=2)return false;
    report[3]=4;report[7]=0;
    if(!travel.Consume(report,allowed,sample) || travel.active!=1 || travel.releases!=1 || !travel.values[26])return false;
    if(scenario==2)return !wrote && !io.writes && (io.settings[7]&8);
    if(!wrote || io.writes!=1 || ace68gt::SetFlag(base,false,readBase,readSettings,write)!=Mode::Verified)return false;
    const unsigned offset=scenario==1?2*64+7:7;
    return io.writes==2 && !(io.settings[offset]&8);
}
inline int Ace68Gt() {
    unsigned passed=0,negatives=0;
    for(unsigned scenario=0;scenario<15;++scenario)if(GtScenario(scenario)){
        if(scenario<6)++passed;else ++negatives;
    }
    const bool ok=passed==6 && negatives==9;
    wprintf(L"ACE68GT_SELF_TEST=%ls scenarios=%u negatives=%u physical_channels=67 proof_to_subscribe=1 first_sample=1 multikey=1 release=1 cleanup=1 hardware_opened=0\n",
        ok?L"PASS":L"FAIL",passed,negatives);
    return ok?0:95;
}

struct Ace60Fixture {
    ace60::Base base{0,4,0,1,2,3,0,0};
    ace60::Settings settings{};
    unsigned scenario=0, reads=0, writes=0;
    bool Cancelled() const {return scenario==12;}
    bool ReadBase(ace60::Base& result) {
        result=base;
        if(scenario==9 && reads++)result[0]=1;
        return scenario!=10;
    }
    bool ReadSettings(ace60::Settings& result) {result=settings;return true;}
    void Write(const ace60::Report& request) {
        ++writes;
        if(scenario==13)return;
        settings[request[5]]=request[8];
    }
};
inline bool Ace60Scenario(unsigned n) {
    using namespace ace60;
    Ace60Fixture io;io.scenario=n;
    for(unsigned i=0;i<4;++i){io.settings[i*64+2]=0xaa;io.settings[i*64+3]=0xbb;}
    io.settings[8]=0x56;
    if(n==1)io.base={1,1,2,3,2,0,0x23,0x67};
    if(n==2)io.settings[7]=8;
    if(n==6)io.settings[0]=1;
    if(n==7)io.settings[2]=0;
    if(n==8)io.base={4,6,0,1,2,3,0,0};
    const auto model=jet75::FindModel(Vid,Pid);
    if(!model || !model->Ace60() || model->keyCount!=60 ||
        layout_identity::Token(model->layoutProtocol,model->layoutProduct)!=
        layout_identity::Token("mchose-ace60","ACE60PRO-41E4-2103"))return false;
    if(!IsControl(Vid,Pid,1,0,65,65))return false;
    if(n==14)return !IsControl(Vid,0x2103,1,0,65,65) && !IsControl(Vid,0x2102,1,0,65,65);
    if(n==15)return !IsControl(Vid,Pid,1,6,65,65) && !IsControl(Vid,Pid,1,0,64,64);
    Report info{};info[0]=0xaa;info[1]=3;info[4]=22;info[8]=n==3?0x32:0x20;info[9]=1;
    constexpr char build[]="Jan 10 2026,14:58:22";
    std::copy_n(build,sizeof(build)-1,info.begin()+10);
    if(n==4)info[10]='X';
    info[3]=jet75::Checksum(info);
    if(n==5)++info[3];
    ModeLease lease;bool attempted=n==11;
    const auto mode=lease.Activate(io,info,attempted);
    constexpr Mode expected[]={Mode::Verified,Mode::Verified,Mode::AlreadyEnabled,Mode::UnreviewedWriter,
        Mode::UnreviewedWriter,Mode::UnreviewedWriter,Mode::ResetArmed,Mode::InvalidProfile,Mode::InvalidProfile,
        Mode::ProfileRace,Mode::ProbeFailed,Mode::AttemptSpent,Mode::Cancelled,Mode::WriteUnverified};
    if(mode!=expected[n])return false;
    if(n>=6)return io.writes==(n==13?1u:0u);
    const auto allowed=jet75::Allowed(*model);
    Travel travel;
    Report packet{};packet[0]=0xa0;packet[1]=0x10;packet[3]=26;packet[15]=170;
    jet75::Sample sample{};
    if(!travel.Consume(packet,allowed,sample) || sample.hid!=26 || sample.milli!=0)return false;
    packet[7]=85;
    if(!travel.Consume(packet,allowed,sample) || sample.milli!=500)return false;
    packet[1]=0xf0;packet[2]=0xff;packet[3]=1;packet[7]=170;
    if(!travel.Consume(packet,allowed,sample) || sample.hid!=Fn || travel.peak!=2)return false;
    packet[1]=0x10;packet[2]=0;packet[3]=26;packet[7]=0;
    if(!travel.Consume(packet,allowed,sample) || travel.active!=1 || travel.releases!=1)return false;
    packet[15]=0;
    if(travel.Consume(packet,allowed,sample) || travel.rejected!=1 || travel.active!=1)return false;
    if(n==1){io.base={0,4,0,1,2,3,0,0};io.settings[72]=0x99;}
    const auto restore=lease.Restore(io);
    if(n>=2)return restore==Mode::NoOwnedWrite && !io.writes && bool(io.settings[7]&8)==(n==2);
    return restore==Mode::Verified && io.writes==2 && !(io.settings[(n==1?2:0)*64+7]&8) &&
        io.settings[8]==0x56 && (n!=1 || io.settings[72]==0x99);
}
inline int Ace60() {
    unsigned passed=0,negatives=0;
    for(unsigned scenario=0;scenario<16;++scenario)if(Ace60Scenario(scenario)){
        if(scenario<6)++passed;else ++negatives;
    }else wprintf(L"Ace60 scenario %u failed\n",scenario);
    const bool ok=passed==6 && negatives==10;
    wprintf(L"ACE60_SELF_TEST=%ls scenarios=%u negatives=%u physical_channels=61 proof_to_subscribe=1 first_sample=1 multikey=1 release=1 cleanup=1 hardware_opened=0\n",
        ok?L"PASS":L"FAIL",passed,negatives);
    return ok?0:96;
}
}

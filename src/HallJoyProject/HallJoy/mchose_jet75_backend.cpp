#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <setupapi.h>
#include <hidsdi.h>
#include <hidpi.h>
#include <process.h>
#include <algorithm>
#include <atomic>
#include <mutex>
#include <vector>
#include <string>
#include <cwctype>
#include "mchose_jet75_backend.h"
#include "mchose_jet75_protocol.h"
#include "mchose_ace68gt_protocol.h"
#include "mchose_ace60_protocol.h"
#include "hid_io_operation.h"
#include "support_log.h"
#include "keyboard_support_status.h"
#include "generated/layout_pipeline/identities.h"
#pragma comment(lib,"setupapi.lib")
#pragma comment(lib,"hid.lib")

// MCHOSE Jet 75 II and Ace68-II (tp::Models): same lifecycle policy as Mix87 III
// (owner decision 2026-09-27): enable the persistent debug flag at start/resume,
// disable it on pause/exit, at most one automatic enable per worker generation.
namespace {
namespace tp=halljoy::jet75;
using tp::Report;
namespace gt=halljoy::ace68gt;
namespace ace=halljoy::ace60;
struct Handle {
    HANDLE v=INVALID_HANDLE_VALUE;
    explicit Handle(HANDLE h=INVALID_HANDLE_VALUE):v(h){}
    ~Handle(){if(v && v!=INVALID_HANDLE_VALUE)CloseHandle(v);}
    Handle(const Handle&)=delete;Handle& operator=(const Handle&)=delete;
    explicit operator bool()const{return v && v!=INVALID_HANDLE_VALUE;}
};
struct Candidate {std::wstring path;const tp::Model* model=&tp::Models[0];};
std::atomic<bool> g_stop{false},g_running{false},g_present{false},g_connected{false};
std::atomic<std::uint64_t> g_good{0},g_bad{0},g_last{0};
halljoy::observation::Source g_observation;
std::array<std::atomic<std::uint16_t>,256> g_values{};
std::array<std::atomic<bool>,256> g_owned{};
std::atomic<std::uint16_t> g_fnValue{0};
std::mutex g_service;
// Model of the last enumerated keyboard (telemetry name, layout, keys).
std::atomic<const tp::Model*> g_model{&tp::Models[0]};
HANDLE g_thread=nullptr,g_wake=nullptr;
// At most one automatic enable attempt per worker generation. Recovery reads
// may continue, but repeated faults must never become a flash-write loop.
bool g_autoAttempted=false;
bool g_aceAttempted=false;
void Clear(){
    g_observation.Clear();g_connected.store(false);g_fnValue.store(0);for(auto& v:g_values)v.store(0);}
void Failure(const char* stage,SupportLogDetail detail){++g_bad;SupportLog_Event(stage,0,detail);halljoy::keyboard_support::ReportCommunicationAnomaly(29);}
std::vector<Candidate> Enumerate(){
    GUID guid{};HidD_GetHidGuid(&guid);
    HDEVINFO set=SetupDiGetClassDevsW(&guid,nullptr,nullptr,DIGCF_PRESENT|DIGCF_DEVICEINTERFACE);
    if(set==INVALID_HANDLE_VALUE)return {};
    struct FreeSet{HDEVINFO s;~FreeSet(){SetupDiDestroyDeviceInfoList(s);}} freeSet{set};
    std::vector<Candidate> out;std::vector<std::uint64_t> seen;
    unsigned aceCollections=0,aceAccepted=0;
    // Collections of admitted models, logged when the set changes (not every pass).
    static std::vector<std::uint64_t> loggedCaps;
    static unsigned previousAceCollections=0,previousAceAccepted=0;
    for(DWORD i=0;;++i){
        SP_DEVICE_INTERFACE_DATA d{};d.cbSize=sizeof(d);
        if(!SetupDiEnumDeviceInterfaces(set,nullptr,&guid,i,&d))break;
        DWORD needed=0;SetupDiGetDeviceInterfaceDetailW(set,&d,nullptr,0,&needed,nullptr);
        if(needed<sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W))continue;
        std::vector<unsigned char> bytes(needed);
        auto* detail=reinterpret_cast<SP_DEVICE_INTERFACE_DETAIL_DATA_W*>(bytes.data());detail->cbSize=sizeof(*detail);
        if(!SetupDiGetDeviceInterfaceDetailW(set,&d,detail,needed,nullptr,nullptr))continue;
        Handle h(CreateFileW(detail->DevicePath,0,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr));
        if(!h)continue;
        HIDD_ATTRIBUTES a{};a.Size=sizeof(a);
        if(!HidD_GetAttributes(h.v,&a))continue;
        const auto* model=tp::FindModel(a.VendorID,a.ProductID);if(!model)continue;
        PHIDP_PREPARSED_DATA data=nullptr;if(!HidD_GetPreparsedData(h.v,&data))continue;
        HIDP_CAPS caps{};const auto status=HidP_GetCaps(data,&caps);HidD_FreePreparsedData(data);
        if(status!=HIDP_STATUS_SUCCESS)continue;
        // Stock descriptor: unnumbered 64-byte IN/OUT => Windows includes ID0.
        seen.push_back((static_cast<std::uint64_t>(a.ProductID)<<56)|(static_cast<std::uint64_t>(caps.UsagePage)<<40)|(static_cast<std::uint64_t>(caps.Usage)<<24)|(caps.InputReportByteLength<<12)|caps.OutputReportByteLength);
        if(model->Ace68Gt()){
            std::wstring path=detail->DevicePath;
            std::transform(path.begin(),path.end(),path.begin(),[](wchar_t ch){return static_cast<wchar_t>(towlower(ch));});
            const unsigned interfaceNumber=path.find(L"&mi_01")!=std::wstring::npos?1u:~0u;
            const bool matches=gt::IsControl(interfaceNumber,caps.UsagePage,caps.Usage,caps.InputReportByteLength,caps.OutputReportByteLength);
            if(matches)out.push_back({detail->DevicePath,model});
        }else if(model->Ace60()){
            ++aceCollections;
            if(ace::IsControl(a.VendorID,a.ProductID,caps.UsagePage,caps.Usage,caps.InputReportByteLength,caps.OutputReportByteLength)){
                ++aceAccepted;
                out.push_back({detail->DevicePath,model});
            }
        }else if(caps.UsagePage==1 && caps.Usage==0 && caps.InputReportByteLength==65 && caps.OutputReportByteLength==65){
            out.push_back({detail->DevicePath,model});
        }
    }
    std::sort(seen.begin(),seen.end());
    if(seen!=loggedCaps){
        // value: pid low byte<<56 | usage page<<40 | usage<<24 | input bytes<<12 | output bytes
        for(const auto v:seen)SupportLog_Event("jet75.hid_caps",v);
        SupportLog_Event("jet75.control_collections",out.size(),SupportLog_Data(seen.size()));
        loggedCaps=seen;
    }
    if(aceCollections!=previousAceCollections || aceAccepted!=previousAceAccepted){
        char detail[160]{};
        _snprintf_s(detail,_TRUNCATE,"evidence.ace60 discovery collections=%u accepted=%u required_page=0001 required_usage=0000 required_in_out=65",
            aceCollections,aceAccepted);
        SupportLog_Evidence(detail,true);
        previousAceCollections=aceCollections;previousAceAccepted=aceAccepted;
    }
    return out;
}
bool Io(HANDLE handle,bool write,void* bytes,DWORD count,DWORD timeout){
    HidIoOperation op(handle);DWORD error=0,done=0;
    const auto start=write?op.StartWrite(bytes,count,&error):op.StartRead(bytes,count,&error);
    if(start==HidIoOperation::StartResult::Failed){SetLastError(error);return false;}
    if(start==HidIoOperation::StartResult::Pending && op.Wait(timeout)!=WAIT_OBJECT_0){
        op.CancelAndDrain(&done,&error);
        // Keep a report that completed between the timeout and CancelIoEx.
        if(error!=ERROR_SUCCESS || done==0){SetLastError(WAIT_TIMEOUT);return false;}
        if(done!=count){SetLastError(ERROR_BAD_LENGTH);return false;}
        return true;
    }
    if(!op.Finish(&done,&error,false)){SetLastError(error);return false;}
    if(done!=count){SetLastError(ERROR_BAD_LENGTH);return false;}return true;
}
class Session {
    Handle handle;
public:
    void* interleavedContext=nullptr;
    void (*interleavedReport)(void*, const Report&)=nullptr;
    explicit Session(const Candidate& c):handle(CreateFileW(c.path.c_str(),GENERIC_READ|GENERIC_WRITE,
        (c.model->Ace68Gt() || c.model->Ace60())?0:FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_EXISTING,FILE_FLAG_OVERLAPPED,nullptr)){}
    bool Valid()const{return bool(handle);}
    bool Receive(Report& report,DWORD timeout){
        std::array<std::uint8_t,65> wire{};
        if(!Io(handle.v,false,wire.data(),65,timeout))return false;
        if(wire[0]){SetLastError(ERROR_INVALID_DATA);return false;}
        std::copy(wire.begin()+1,wire.end(),report.begin());return true;
    }
    bool Exchange(const Report& request,Report& reply,bool gtInfo=false){
        std::array<std::uint8_t,65> wire{};std::copy(request.begin(),request.end(),wire.begin()+1);
        if(!Io(handle.v,true,wire.data(),65,300)){Failure("jet75.send_failed",SupportLog_Win32(GetLastError()));return false;}
        const auto end=GetTickCount64()+800;
        while(GetTickCount64()<end){
            if(!Receive(reply,50)){
                if(GetLastError()==WAIT_TIMEOUT)continue;
                Failure("jet75.reply_failed",SupportLog_Win32(GetLastError()));return false;
            }
            if(gtInfo?gt::InfoReply(reply):tp::Reply(request,reply))return true;
            if(reply[0]==0xab || reply[0]==0xaa){
                if(gtInfo){
                    char evidence[192]{};
                    _snprintf_s(evidence,_TRUNCATE,"evidence.ace68gt info_refused marker=%02x opcode=%02x length=%u checksum_ok=%u",
                                reply[0],reply[1],reply[4],reply[4]<=56 && reply[3]==tp::Checksum(reply)?1:0);
                    SupportLog_Evidence(evidence,true);
                }
                Failure("jet75.reply_mismatch",SupportLog_Protocol(request[1]));return false;
            }
            if(interleavedReport)interleavedReport(interleavedContext,reply);
            // Other families intentionally ignore analog during their configuration exchange.
        }
        Failure("jet75.ack_timeout",SupportLog_Protocol(request[1]));return false;
    }
    bool ReadBytes(std::uint8_t op,std::uint8_t* target,std::size_t size,bool completeWrite=false,ULONGLONG deadline=0){
        for(std::size_t pos=0;pos<size;){
            if(deadline && GetTickCount64()>=deadline){SetLastError(WAIT_TIMEOUT);return false;}
            if(g_stop.load() && !completeWrite)return false;
            const auto count=static_cast<std::uint8_t>(std::min<std::size_t>(56,size-pos));
            Report reply{};
            if(!Exchange(tp::Read(op,static_cast<std::uint8_t>(pos),count),reply))return false;
            std::copy_n(reply.data()+8,count,target+pos);pos+=count;
        }return true;
    }
    bool ReadBase(tp::Base& b,bool completeWrite=false,ULONGLONG deadline=0){return ReadBytes(tp::ReadBaseOp,b.data(),b.size(),completeWrite,deadline);}
    bool ReadSettings(tp::Settings& s,bool completeWrite=false,ULONGLONG deadline=0){return ReadBytes(tp::ReadSettingsOp,s.data(),s.size(),completeWrite,deadline);}
};
// Any firmware version is admitted: the M HUB protocol answer is required,
// and the version is only recorded. Safety comes from the data checks
// (settings layout, readback, A0 validation), not from a version list.
bool Verify(Session& session,const tp::Model& model){
    Report reply{};
    if(!session.Exchange(tp::Info(),reply))return false;
    const auto version=tp::Version(reply);
    SupportLog_Event("jet75.model",model.pid);
    SupportLog_Event("jet75.firmware_version",version);
    if(version!=model.reviewed)SupportLog_Event("jet75.firmware_unreviewed",version,SupportLog_Data(model.reviewed));
    return true;
}
bool LoadMode(Session& session,const tp::Model& model,tp::Base& base,bool& enabled,unsigned& profile){
    tp::Settings settings{};tp::Base check{};
    // The keyboard may rewrite the base while it is read (a profile switch saved
    // late): read until two reads around the settings read agree.
    bool stable=false;
    for(unsigned attempt=0;attempt<3 && !stable;++attempt){
        if(!session.ReadBase(base) || !session.ReadSettings(settings) || !session.ReadBase(check)){
            if(!g_stop.load())SupportLog_Event("jet75.load_failed",attempt,SupportLog_Win32(GetLastError()));
            return false;
        }
        stable=check==base;
        if(!stable)SupportLog_Event("jet75.profile_base_changed",tp::Pack(base),SupportLog_Data(tp::Pack(check)));
    }
    if(!stable)return false;
    // value: the six base bytes {current, count, table[4]}, little-endian;
    // detail: resolved profile, or 0xff when the firmware rule leaves it unknown.
    const bool resolved=tp::Profile(base,profile,model.baseRule);
    SupportLog_Event("jet75.profile_base",tp::Pack(base),SupportLog_Data(resolved?profile:0xffu));
    if(!resolved){SupportLog_Event("jet75.profile_base_unusable",tp::Pack(base),SupportLog_Data(static_cast<unsigned>(model.baseRule)));return false;}
    if(!tp::ProfileValid(settings,profile)){
        // detail: the profile's first eight settings bytes (magic AA BB at 2..3).
        std::uint64_t head=0;for(unsigned i=8;i-->0;)head=(head<<8)|settings[profile*64+i];
        SupportLog_Event("jet75.profile_format",profile,SupportLog_Data(head));return false;
    }
    if(model.freshFlashReboot && tp::WriteReboots(settings))SupportLog_Event("jet75.write_reboots",1);
    enabled=(settings[profile*64+7]&8)!=0;return true;
}
tp::ChangeResult ChangeMode(Session& session,const tp::Model& model,const tp::Base& base,bool enabled,bool cleanup){
    const auto deadline=GetTickCount64()+10000;
    return tp::ChangeFlag(base,enabled,
        [&](tp::Base& b){return GetTickCount64()<deadline && session.ReadBase(b,true,deadline);},
        [&](tp::Settings& s){return GetTickCount64()<deadline && session.ReadSettings(s,true,deadline);},
        [&](const Report& q,Report& r){if(!cleanup && g_stop.load())return false;return session.Exchange(q,r);},
        model.freshFlashReboot,model.baseRule);
}
bool Settled(tp::ChangeResult r){
    return r==tp::ChangeResult::Verified || r==tp::ChangeResult::Unchanged || r==tp::ChangeResult::Rebooting;
}
struct ModeLease {
    Session& session; const tp::Model& model; tp::Base base; bool armed=false;
    ~ModeLease(){
        if(!armed)return;
        Clear(); // No gamepad input while the keyboard changes mode.
        const auto result=ChangeMode(session,model,base,false,true);
        SupportLog_Event("jet75.cleanup_mode_result",static_cast<unsigned>(result));
        if(!Settled(result))Failure("jet75.mode_may_remain_enabled",SupportLog_Protocol(static_cast<unsigned>(result)));
    }
};
void RunGt(const Candidate& c,Session& session) {
    const auto& model=*c.model;
    Report info{};
    const bool infoOk=session.Exchange(gt::Info(),info,true);
    const auto writer=infoOk?gt::WriterPolicy(info):gt::Writer::Unavailable;
    char identity[224]{};
    _snprintf_s(identity,_TRUNCATE,"evidence.ace68gt probe revision=3 vid=3837 pid=3007 info_ok=%u length=%u firmware=%04x writer_reviewed=%u writer_policy=%s",
                infoOk?1:0,info[4],infoOk?tp::Version(info):0,writer==gt::Writer::Reviewed?1:0,gt::WriterName(writer));
    SupportLog_Evidence(identity,true);
    if(!NativeAnalogRouting_Claim(model.vid,model.pid,c.path.c_str(),NativeAnalogProtocol::MchoseJet75) &&
       !NativeAnalogRouting_IsClaimedBy(c.path.c_str(),NativeAnalogProtocol::MchoseJet75)){
        SupportLog_Evidence("evidence.ace68gt refused stage=claim reason=transport_owned",true);return;
    }
    auto deadline=GetTickCount64()+8000;
    auto readBase=[&](gt::Base& base){
        return session.ReadBytes(tp::ReadBaseOp,base.data(),base.size(),true,deadline);
    };
    auto readSettings=[&](gt::Settings& settings){return session.ReadSettings(settings,true,deadline);};
    gt::Base base{};
    bool wrote=false,issued=false,acked=false;
    const auto mode=infoOk?gt::Activate(writer!=gt::Writer::Unavailable,!g_autoAttempted,base,wrote,readBase,readSettings,
        [&](const Report& request){
            if(g_stop.load())return false;
            issued=true;
            Report reply{};
            acked=session.Exchange(request,reply);
            return acked;
        }):gt::Mode::ProbeFailed;
    if(issued)g_autoAttempted=true;
    char activation[192]{};
    _snprintf_s(activation,_TRUNCATE,"evidence.ace68gt subscribe result=%s write_attempted=%u write_ack=%u read_a0=1",
                gt::ModeName(wrote && !issued?gt::Mode::Cancelled:mode),issued?1:0,acked?1:0);
    SupportLog_Evidence(activation,true);
    const auto allowed=tp::Allowed(model);
    for(unsigned i=0;i<allowed.size();++i)g_owned[i].store(allowed[i]);
    gt::Travel travel;
    auto nextSummary=GetTickCount64()+3000;
    unsigned lastReported=0,receiveError=0,lastOther=0,other=0;
    auto evidence=[&](const char* stage){
        char line[248]{};
        _snprintf_s(line,_TRUNCATE,"evidence.ace68gt %s packets=%u rejected=%u channels=%u simultaneous=%u releases=%u other_reports=%u last_other=%02x io_error=%u",
                    stage,travel.packets,travel.rejected,travel.distinct,travel.peak,travel.releases,other,lastOther,receiveError);
        SupportLog_Evidence(line,true);
        lastReported=travel.packets;
    };
    while(!g_stop.load()){
        Report report{};
        const bool received=session.Receive(report,50);
        if(!received && GetLastError()!=WAIT_TIMEOUT){receiveError=GetLastError();break;}
        const auto now=GetTickCount64();
        if(received){
            if(report[0]!=0xa0){
                lastOther=report[0];
                if(++other==1)evidence("first_other_report");
            }
            // Firmware/profile notifications invalidate the previous analog state.
            if(report[0]==0xa1 || report[0]==0xa2 || report[0]==0xa3)break;
            tp::Sample sample{};
            const unsigned priorPeak=travel.peak,priorReleases=travel.releases,priorRejected=travel.rejected;
            if(travel.Consume(report,allowed,sample)){
                g_observation.Store(g_values[sample.hid],static_cast<std::uint16_t>(sample.hid),sample.milli);
                g_last.store(now);++g_good;g_connected.store(true);
                if(travel.packets==1)evidence("first_sample");
                if(priorPeak<2 && travel.peak>=2)evidence("first_multikey");
                if(!priorReleases && travel.releases)evidence("first_release");
            }else if(!priorRejected && travel.rejected){
                evidence("first_rejected_sample");
            }
        }
        if(now>=nextSummary){
            if(lastReported!=travel.packets || !travel.packets)evidence(travel.packets?"travel":"no_sample");
            nextSummary=now+30000;
        }
    }
    Clear();
    evidence("end");
    if(issued){
        deadline=GetTickCount64()+8000;
        const auto restored=gt::SetFlag(base,false,readBase,readSettings,[&](const Report& request){
            Report reply{};return session.Exchange(request,reply);
        });
        char line[168]{};
        _snprintf_s(line,_TRUNCATE,"evidence.ace68gt cleanup result=%s",gt::ModeName(restored));
        SupportLog_Evidence(line,true);
    }else{
        SupportLog_Evidence("evidence.ace68gt cleanup result=no_owned_write",true);
    }
}
struct AceControl {
    Session& session;
    ULONGLONG deadline;
    bool Cancelled() const {return g_stop.load();}
    bool ReadBase(ace::Base& base) {
        return session.ReadBytes(tp::ReadBaseOp,base.data(),base.size(),true,deadline);
    }
    bool ReadSettings(tp::Settings& settings) {return session.ReadSettings(settings,true,deadline);}
    void Write(const Report& request) {
        Report reply{};
        const bool ack=session.Exchange(request,reply);
        SupportLog_Event("ace60.write_ack",ack?1:0);
    }
};
void RunAce60(const Candidate& candidate,Session& session) {
    const auto claim=[&]{
        return NativeAnalogRouting_IsClaimedBy(candidate.path.c_str(),NativeAnalogProtocol::MchoseJet75) ||
            NativeAnalogRouting_Claim(ace::Vid,ace::Pid,candidate.path.c_str(),NativeAnalogProtocol::MchoseJet75);
    };
    if(NativeAnalogRouting_IsClaimed(candidate.path.c_str()) &&
       !NativeAnalogRouting_IsClaimedBy(candidate.path.c_str(),NativeAnalogProtocol::MchoseJet75)) {
        SupportLog_Evidence("evidence.ace60 refused stage=claim reason=transport_owned",true);
        return;
    }
    const auto allowed=tp::Allowed(*candidate.model);
    for(unsigned k=0;k<allowed.size();++k)g_owned[k].store(allowed[k]);
    ace::Travel travel;
    bool publish=false;
    const auto evidence=[&](const char* stage,unsigned error=0){
        char line[224]{};
        _snprintf_s(line,_TRUNCATE,"evidence.ace60 %s samples=%u rejected=%u channels=%u max_simultaneous=%u releases=%u error=%u",
            stage,travel.samples,travel.rejected,travel.distinct,travel.peak,travel.releases,error);
        SupportLog_Evidence(line,true);
    };
    auto process=[&](const Report& packet){
        const auto oldPeak=travel.peak,oldReleases=travel.releases,oldRejected=travel.rejected;
        tp::Sample sample{};
        if(!travel.Consume(packet,allowed,sample)){
            if(!oldRejected && travel.rejected){
                char line[164]{};
                const unsigned maximum=(packet[14]<<8)|packet[15],depth=(packet[6]<<8)|packet[7];
                _snprintf_s(line,_TRUNCATE,"evidence.ace60 first_rejected_a0 descriptor_type=%02x maximum=%u depth_valid=%u",
                    packet[1],maximum,depth<=maximum?1u:0u);
                SupportLog_Evidence(line,true);
            }
            return;
        }
        auto& cell=sample.hid==ace::Fn?g_fnValue:g_values[sample.hid];
        if(publish){
            g_observation.Store(cell,static_cast<std::uint16_t>(sample.hid),sample.milli);
            g_connected.store(true);++g_good;g_last.store(GetTickCount64());
        }else cell.store(sample.milli);
        if(travel.samples==1)evidence("first_sample");
        if(oldPeak<2 && travel.peak>=2)evidence("first_multikey");
        if(!oldReleases && travel.releases)evidence("first_release");
    };
    struct Tap {
        Session& session;
        ~Tap(){session.interleavedReport=nullptr;session.interleavedContext=nullptr;}
    } tap{session};
    session.interleavedContext=&process;
    session.interleavedReport=+[](void* context,const Report& packet){(*static_cast<decltype(process)*>(context))(packet);};
    Report info{};
    const bool infoOk=session.Exchange(tp::Info(),info,true) && ace::InfoReply(info);
    const bool writer=infoOk && ace::ReviewedWriter(info);
    char detail[224]{};
    _snprintf_s(detail,_TRUNCATE,"evidence.ace60 probe vid=41e4 pid=2101 info_valid=%u firmware=%04x reply_length=%u reviewed_writer=%u",
        infoOk?1u:0u,infoOk?unsigned(tp::Version(info)):0u,unsigned(info[4]),writer?1u:0u);
    SupportLog_Evidence(detail,true);
    if((writer || travel.samples) && !claim()){
        evidence("refused stage=claim reason=transport_owned");return;
    }
    ace::ModeLease lease;
    AceControl control{session,GetTickCount64()+8000};
    const auto mode=lease.Activate(control,info,g_aceAttempted);
    _snprintf_s(detail,_TRUNCATE,"evidence.ace60 subscribe result=%s reader=A0 owned_write=%u",ace::Name(mode),lease.ownedProfile<4?1u:0u);
    SupportLog_Evidence(detail,true);
    unsigned error=0,reported=0;
    bool waitingLogged=false;
    auto nextSummary=GetTickCount64()+1000;
    if(travel.samples && !g_stop.load() && claim()){
        publish=true;g_connected.store(true);g_good.fetch_add(travel.samples);g_last.store(GetTickCount64());
    }
    while(!g_stop.load()){
        Report packet{};
        if(session.Receive(packet,50)){
            if(packet[0]==0xa1 || packet[0]==0xa2 || packet[0]==0xa3){
                evidence("profile_notification");break;
            }
            const unsigned before=travel.samples;
            process(packet);
            if(!publish && travel.samples!=before){
                if(!claim()){evidence("refused stage=claim reason=transport_owned");break;}
                publish=true;g_connected.store(true);g_good.fetch_add(travel.samples);g_last.store(GetTickCount64());
            }
        }else if(GetLastError()!=WAIT_TIMEOUT){error=GetLastError();break;}
        const auto now=GetTickCount64();
        if(now>=nextSummary){
            if(reported!=travel.samples || (!travel.samples && !waitingLogged)){
                evidence(travel.samples?"travel":"awaiting_analog");reported=travel.samples;waitingLogged=true;
            }
            nextSummary=now+30000;
        }
    }
    publish=false;session.interleavedReport=nullptr;
    Clear();evidence("session_end",error);
    control.deadline=GetTickCount64()+8000;
    const auto restored=lease.Restore(control);
    _snprintf_s(detail,_TRUNCATE,"evidence.ace60 cleanup result=%s",ace::Name(restored));
    SupportLog_Evidence(detail,true);
}
void Run(const Candidate& c){
    Clear();for(auto& owned:g_owned)owned.store(false);
    struct End{~End(){Clear();}} end;
    const auto& model=*c.model;g_model.store(c.model);
    Session session(c);
    if(!session.Valid()){
        const auto error=GetLastError();
        if(model.Ace60()){
            char line[176]{};
            _snprintf_s(line,_TRUNCATE,"evidence.ace60 refused stage=open required=exclusive native_error=%lu",error);
            SupportLog_Evidence(line,true);
        }
        if(model.Ace68Gt()){
            char line[176]{};
            _snprintf_s(line,_TRUNCATE,"evidence.ace68gt refused stage=open required=exclusive native_error=%lu",error);
            SupportLog_Evidence(line,true);
        }
        Failure("jet75.open_failed",SupportLog_Win32(error));return;
    }
    if(model.Ace60()){RunAce60(c,session);return;}
    if(model.Ace68Gt()){RunGt(c,session);return;}
    if(!Verify(session,model)){SupportLog_Event("jet75.admission_failed",1);return;}
    if(!NativeAnalogRouting_Claim(model.vid,model.pid,c.path.c_str(),NativeAnalogProtocol::MchoseJet75) &&
       !NativeAnalogRouting_IsClaimedBy(c.path.c_str(),NativeAnalogProtocol::MchoseJet75)){
        SupportLog_Event("jet75.claim_refused",model.pid,SupportLog_Data(NativeAnalogRouting_IsClaimed(c.path.c_str())));return;}
    bool enabled=false;unsigned profile=0;tp::Base base{};
    if(!LoadMode(session,model,base,enabled,profile))return;
    ModeLease lease{session,model,base,enabled};
    if(!enabled){
        if(g_stop.load())return;
        if(g_autoAttempted){SupportLog_Event("jet75.auto_enable_retry_blocked",profile);return;}
        g_autoAttempted=true;lease.armed=true;
        const auto result=ChangeMode(session,model,base,true,false);
        SupportLog_Event("jet75.auto_enable_result",static_cast<unsigned>(result));
        if(result==tp::ChangeResult::Rebooting){
            // The flag is saved; the keyboard reconnects and the next session
            // finds it enabled (and owns its cleanup).
            lease.armed=false;return;
        }
        if(!Settled(result))return;
        enabled=true;
    }
    // If a pre-enabled session later fails, do not repeatedly cycle flash during recovery.
    g_autoAttempted=true;
    const auto allowed=tp::Allowed(model);
    for(unsigned i=0;i<256;++i)g_owned[i].store(allowed[i]);
    SupportLog_Event("jet75.mode_enabled",enabled);
    SupportLog_Event("jet75.profile",profile);
    // The stream is event-only: an untouched keyboard sends nothing. An
    // admitted session with the mode on is a live source with all keys at 0.
    g_connected.store(true);
    std::array<bool,256> unknownLogged{};
    // Device-change notifications are system-wide (another USB device, our own
    // virtual gamepad appearing) and do not end a healthy session: removal of
    // this keyboard fails the next read (stream_failed) instead.
    while(!g_stop.load()){
        Report report{};
        if(!session.Receive(report,50)){
            if(GetLastError()==WAIT_TIMEOUT)continue; // idle is valid, never expire held keys
            Failure("jet75.stream_failed",SupportLog_Win32(GetLastError()));return;
        }
        if(report[0]==0xa1 || report[0]==0xa2 || report[0]==0xa3 || report[0]==0xaa || report[0]==0xab){
            // A profile switch (A1) or configurator activity can stop debug
            // output without a USB disconnect. Neutralize and require a new
            // admission (pause/resume).
            Failure("jet75.configuration_changed",SupportLog_Protocol(report[0]));return;
        }
        tp::Sample sample{};
        if(tp::Decode(report,allowed,sample)){
            g_observation.Store(g_values[sample.hid],static_cast<std::uint16_t>(sample.hid),sample.milli);g_last.store(GetTickCount64());++g_good;g_connected.store(true);
        }else if(report[0]==0xa0 && report[1]==0x10){
            const auto hid=tp::Hid(report[1],report[2],report[3]);
            const unsigned depth=(report[6]<<8)|report[7],maximum=(report[14]<<8)|report[15];
            const std::uint64_t data=(static_cast<std::uint64_t>(hid)<<32)|(maximum<<16)|depth;
            if(hid && hid<256 && !allowed[hid] && tp::KnownMaximum(maximum) && depth<=maximum){
                // A key outside this model's reviewed set: ignore it, keep the session.
                if(!unknownLogged[hid]){unknownLogged[hid]=true;SupportLog_Event("jet75.unknown_key",hid,SupportLog_Data(data));}
                continue;
            }
            Failure("jet75.invalid_analog",SupportLog_Data(data));return;
        }
    }
}
unsigned __stdcall Worker(void*) noexcept{
    try{
        while(!g_stop.load()){
            const auto candidates=Enumerate();g_present.store(!candidates.empty());
            static std::size_t loggedCount=1;
            if(candidates.size()!=loggedCount && (candidates.size()>1 || loggedCount>1))SupportLog_Event("jet75.multiple_devices",candidates.size());
            loggedCount=candidates.empty()?1:candidates.size();
            // Each collection is tried in turn; one that fails admission returns at once.
            for(const auto& c:candidates){if(g_stop.load())break;Run(c);}
            if(!g_stop.load())WaitForSingleObject(g_wake,candidates.empty()?INFINITE:5000);
        }
    }catch(...){Failure("jet75.worker_exception",SupportLog_Protocol(1));}
    Clear();g_running.store(false);return 0;
}
bool Start(){
    std::lock_guard<std::mutex> l(g_service);if(g_thread)return g_running.load();
    g_autoAttempted=false;g_aceAttempted=false;g_stop.store(false);g_wake=CreateEventW(nullptr,FALSE,FALSE,nullptr);if(!g_wake)return false;
    g_running.store(true);unsigned id=0;g_thread=reinterpret_cast<HANDLE>(_beginthreadex(nullptr,0,Worker,nullptr,0,&id));
    if(!g_thread){g_running.store(false);CloseHandle(g_wake);g_wake=nullptr;return false;}return true;
}
halljoy::lifecycle::StopResult Stop(halljoy::lifecycle::GenerationId generation){
    std::lock_guard<std::mutex> l(g_service);g_stop.store(true);if(!g_thread)return NativeAnalogBackendStopJoined(generation);
    SetEvent(g_wake);const auto wait=WaitForSingleObject(g_thread,25000);
    if(wait!=WAIT_OBJECT_0)return NativeAnalogBackendStopFailed(generation,halljoy::lifecycle::LifecycleErrorCode::StopTimedOut,WAIT_TIMEOUT);
    CloseHandle(g_thread);g_thread=nullptr;CloseHandle(g_wake);g_wake=nullptr;Clear();return NativeAnalogBackendStopJoined(generation);
}
void Notify(){std::lock_guard<std::mutex> l(g_service);if(g_wake)SetEvent(g_wake);}
bool Present(){return g_present.load();}bool Connected(){return g_connected.load();}
bool Owns(std::uint16_t hid){
    return Connected() && (hid==ace::Fn?g_model.load()->Ace60():hid<256 && g_owned[hid].load());
}
std::uint16_t Get(std::uint16_t hid){return Owns(hid)?(hid==ace::Fn?g_fnValue.load():g_values[hid].load()):0;}
void Telemetry(NativeAnalogBackendTelemetry* out){
    if(!out)return;
    *out={};const auto& model=*g_model.load();
    out->present=Present();out->connected=Connected();out->vendorId=model.vid;out->productId=model.pid;out->usagePage=1;
    out->inputReportBytes=65;out->outputReportBytes=65;out->successfulUpdates=g_good.load();out->failedUpdates=g_bad.load();
    out->nominalRawLevels=model.Ace60()?0:342;
    out->mappedKeys=Connected()?static_cast<unsigned>(model.keyCount)+(model.Ace60()?1u:0u):0;
    for(unsigned i=0;i<256;++i)if(Get(static_cast<std::uint16_t>(i)))++out->activeKeys;
    const auto last=g_last.load();out->lastUpdateAgeMs=last?static_cast<unsigned>(std::min<ULONGLONG>(0xffffffff,GetTickCount64()-last)):0;
    wcscpy_s(out->deviceName,model.name);
    // Published only after firmware admission; selects the official M HUB layout.
    if(out->connected)out->verifiedLayoutToken=halljoy::layout_identity::Token(model.layoutProtocol,model.layoutProduct);
    if(Get(ace::Fn))++out->activeKeys;
    const auto* active=(model.Ace68Gt() || model.Ace60())?L"analog active; profile flag restored on normal exit":
        L"analog active; automatically disabled on pause or exit";
    swprintf_s(out->status,L"%ls: %ls",model.shortName,Connected()?active:
        Present()?L"checking firmware / preparing analog; see log if this persists":L"not connected");
}
}
const NativeAnalogBackendDescriptor& MchoseJet75_GetNativeBackendDescriptor(){
    static const NativeAnalogBackendDescriptor d{kNativeAnalogBackendAbiVersion,sizeof(NativeAnalogBackendDescriptor),
        "mchose_jet75",L"MCHOSE M HUB (Jet 75 II family) analog",NativeAnalogProtocol::MchoseJet75,
        NativeAnalogStartPhase::AfterRealtime,NativeAnalogBackendFlag_StreamTransport|NativeAnalogBackendFlag_ReadOnlyProbe,
        nullptr,&Start,&Stop,&Notify,&Present,&Connected,&Owns,&Get,&Telemetry, nullptr, +[](halljoy::observation::Sink sink) { g_observation.Visit(sink,&Owns,&Get); }};return d;
}

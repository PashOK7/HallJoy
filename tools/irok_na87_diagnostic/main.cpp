#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <setupapi.h>
#include <hidsdi.h>
#include <hidpi.h>
#include <initguid.h>
#include <devpkey.h>
#include <algorithm>
#include <atomic>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <memory>
#include <set>
#include <sstream>
#include <string_view>
#include <span>
#include <thread>
#include <vector>
#include "hid_io_operation.h"
#include "protocol.h"

#if defined(_MSC_VER)
#pragma comment(lib, "setupapi.lib")
#pragma comment(lib, "hid.lib")
#pragma comment(lib, "uuid.lib")
#endif
namespace fs = std::filesystem;
using namespace na87diag;
static std::atomic<bool> stopping{false};
static std::atomic<std::uint64_t> ioDeadline{UINT64_MAX};
struct IoDeadline {
    std::uint64_t previous;
    explicit IoDeadline(unsigned ms=4000):previous(ioDeadline.exchange(GetTickCount64()+ms)){}
    ~IoDeadline(){ioDeadline=previous;}
};
struct Watchdog {
    std::atomic<bool> done{false};
    std::thread thread;
    explicit Watchdog(const fs::path& note):thread([this,note]{
        while(!done){
            if(GetTickCount64()>ioDeadline.load()){
                HANDLE h=CreateFileW(note.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,0,nullptr);
                if(h!=INVALID_HANDLE_VALUE){
                    const char message[]="A bounded device operation timed out. Worker terminated; parent will attempt cleanup.\r\n";
                    DWORD written=0;WriteFile(h,message,sizeof(message)-1,&written,nullptr);CloseHandle(h);
                }
                TerminateProcess(GetCurrentProcess(),102);
            }
            Sleep(50);
        }
    }){}
    ~Watchdog(){done=true;thread.join();}
};
static BOOL WINAPI ConsoleStop(DWORD) { stopping = true; return TRUE; }
static std::uint64_t Us() {
    LARGE_INTEGER q{}, f{}; QueryPerformanceCounter(&q); QueryPerformanceFrequency(&f);
    return (q.QuadPart / f.QuadPart) * 1000000ull +
        ((q.QuadPart % f.QuadPart) * 1000000ull) / f.QuadPart;
}
static std::string Q(std::string_view s) {
    std::string result = "\"";
    for (unsigned char c : s) {
        if (c == '\\' || c == '"') { result += '\\'; result += static_cast<char>(c); }
        else if (c < 32) { char t[7]{}; sprintf_s(t, "\\u%04x", c); result += t; }
        else result += static_cast<char>(c);
    }
    return result + '"';
}
static std::string Utf8(const std::wstring& s) {
    if (s.empty()) return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0, nullptr, nullptr);
    std::string r(n, 0);
    WideCharToMultiByte(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), r.data(), n, nullptr, nullptr);
    return r;
}
static std::string Hex(std::span<const std::uint8_t> r) {
    const char* digits = "0123456789abcdef"; std::string s;
    for (auto b : r) { s += digits[b >> 4]; s += digits[b & 15]; }
    return s;
}
static std::string Hash(const std::wstring& path) {
    std::uint64_t h = 1469598103934665603ull;
    for (wchar_t c : path) { h ^= static_cast<std::uint16_t>(towlower(c)); h *= 1099511628211ull; }
    char out[17]{}; sprintf_s(out, "%016llx", static_cast<unsigned long long>(h)); return out;
}
struct Handle {
    HANDLE h = INVALID_HANDLE_VALUE;
    Handle() = default;
    explicit Handle(HANDLE value):h(value){}
    ~Handle() { if (h != INVALID_HANDLE_VALUE && h) CloseHandle(h); }
    Handle(const Handle&) = delete;
    Handle& operator=(const Handle&) = delete;
    explicit operator bool() const { return h != INVALID_HANDLE_VALUE && h; }
};
struct Log {
    Handle file;
    std::string phase = "start";
    fs::path directory;
    std::uint64_t start = Us(), lines = 0;
    explicit Log(const fs::path& p):file(CreateFileW(p.c_str(), GENERIC_WRITE, FILE_SHARE_READ,
        nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr)), directory(p.parent_path()) {
        if (!file) throw std::runtime_error("cannot create new evidence file");
        SYSTEMTIME t{}; GetSystemTime(&t);
        char utc[40]{}; sprintf_s(utc, "%04u-%02u-%02uT%02u:%02u:%02uZ", t.wYear,t.wMonth,t.wDay,t.wHour,t.wMinute,t.wSecond);
        Line("start", "\"utc\":" + Q(utc) + ",\"schema\":1");
    }
    void Line(const std::string& kind, const std::string& fields = "") {
        std::string s = "{\"us\":" + std::to_string(Us()-start) + ",\"kind\":" + Q(kind) +
            ",\"phase\":" + Q(phase) + (fields.empty() ? "" : "," + fields) + "}\n";
        DWORD written = 0;
        if (!WriteFile(file.h, s.data(), static_cast<DWORD>(s.size()), &written, nullptr) || written != s.size())
            throw std::runtime_error("evidence write failed");
        ++lines;
    }
    void Flush() { FlushFileBuffers(file.h); }
};
struct Candidate {
    std::wstring path, manufacturer, product;
    std::string hash, container;
    HIDD_ATTRIBUTES attributes{};
    HIDP_CAPS caps{};
    std::set<unsigned> outputIds, inputIds, featureIds;
};
static std::wstring HidText(HANDLE h, BOOLEAN (__stdcall *get)(HANDLE,PVOID,ULONG)) {
    wchar_t s[256]{}; return get(h,s,sizeof(s)) ? s : L"";
}
static std::vector<Candidate> Enumerate(Log& log) {
    IoDeadline deadline(8000);
    GUID guid{}; HidD_GetHidGuid(&guid);
    HDEVINFO set = SetupDiGetClassDevsW(&guid,nullptr,nullptr,DIGCF_PRESENT|DIGCF_DEVICEINTERFACE);
    if (set == INVALID_HANDLE_VALUE) { log.Line("enumeration_error", "\"error\":"+std::to_string(GetLastError())); return {}; }
    std::vector<Candidate> found;
    for (DWORD i=0; i<512; ++i) {
        SP_DEVICE_INTERFACE_DATA iface{}; iface.cbSize=sizeof(iface);
        if (!SetupDiEnumDeviceInterfaces(set,nullptr,&guid,i,&iface)) break;
        DWORD needed=0; SetupDiGetDeviceInterfaceDetailW(set,&iface,nullptr,0,&needed,nullptr);
        if (needed<sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W)) continue;
        std::vector<BYTE> buffer(needed);
        auto* detail=reinterpret_cast<SP_DEVICE_INTERFACE_DETAIL_DATA_W*>(buffer.data()); detail->cbSize=sizeof(*detail);
        SP_DEVINFO_DATA info{}; info.cbSize=sizeof(info);
        if (!SetupDiGetDeviceInterfaceDetailW(set,&iface,detail,needed,nullptr,&info)) continue;
        std::wstring path=detail->DevicePath, lower=path;
        std::transform(lower.begin(),lower.end(),lower.begin(),[](wchar_t ch){return static_cast<wchar_t>(towlower(ch));});
        if (lower.find(L"vid_0416&pid_7372")==std::wstring::npos) continue;
        Candidate c; c.path=path; c.hash=Hash(path);
        Handle h(CreateFileW(path.c_str(),0,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_EXISTING,0,nullptr));
        if (!h) { log.Line("metadata_open_error","\"path_hash\":"+Q(c.hash)+",\"error\":"+std::to_string(GetLastError())); continue; }
        c.attributes.Size=sizeof(c.attributes);
        if (!HidD_GetAttributes(h.h,&c.attributes) || c.attributes.VendorID!=0x0416 || c.attributes.ProductID!=0x7372) continue;
        PHIDP_PREPARSED_DATA pp=nullptr;
        if (!HidD_GetPreparsedData(h.h,&pp)) continue;
        const auto capStatus=HidP_GetCaps(pp,&c.caps);
        if(capStatus==HIDP_STATUS_SUCCESS) {
            auto ids=[&](HIDP_REPORT_TYPE reportType, USHORT nValues, USHORT nButtons, std::set<unsigned>& target) {
                std::vector<HIDP_VALUE_CAPS> v(nValues);
                if(nValues && HidP_GetValueCaps(reportType,v.data(),&nValues,pp)==HIDP_STATUS_SUCCESS)
                    for(unsigned j=0;j<nValues;++j){
                        const auto& item=v[j];target.insert(item.ReportID);
                        log.Line("value_cap","\"path_hash\":"+Q(c.hash)+",\"type\":"+Q(reportType==HidP_Input?"input":reportType==HidP_Output?"output":"feature")+
                            ",\"report_id\":"+std::to_string(item.ReportID)+",\"usage_page\":"+std::to_string(item.UsagePage)+
                            ",\"usage_min\":"+std::to_string(item.IsRange?item.Range.UsageMin:item.NotRange.Usage)+
                            ",\"usage_max\":"+std::to_string(item.IsRange?item.Range.UsageMax:item.NotRange.Usage)+
                            ",\"bit_size\":"+std::to_string(item.BitSize)+",\"report_count\":"+std::to_string(item.ReportCount)+
                            ",\"logical_min\":"+std::to_string(item.LogicalMin)+",\"logical_max\":"+std::to_string(item.LogicalMax)+
                            ",\"bit_field_flags\":"+std::to_string(item.BitField)+",\"link_collection\":"+std::to_string(item.LinkCollection));
                    }
                std::vector<HIDP_BUTTON_CAPS> b(nButtons);
                if(nButtons && HidP_GetButtonCaps(reportType,b.data(),&nButtons,pp)==HIDP_STATUS_SUCCESS)
                    for(unsigned j=0;j<nButtons;++j){
                        const auto& item=b[j];target.insert(item.ReportID);
                        log.Line("button_cap","\"path_hash\":"+Q(c.hash)+",\"type\":"+Q(reportType==HidP_Input?"input":reportType==HidP_Output?"output":"feature")+
                            ",\"report_id\":"+std::to_string(item.ReportID)+",\"usage_page\":"+std::to_string(item.UsagePage)+
                            ",\"usage_min\":"+std::to_string(item.IsRange?item.Range.UsageMin:item.NotRange.Usage)+
                            ",\"usage_max\":"+std::to_string(item.IsRange?item.Range.UsageMax:item.NotRange.Usage)+
                            ",\"bit_field_flags\":"+std::to_string(item.BitField)+",\"link_collection\":"+std::to_string(item.LinkCollection));
                    }
            };
            ids(HidP_Output,c.caps.NumberOutputValueCaps,c.caps.NumberOutputButtonCaps,c.outputIds);
            ids(HidP_Input,c.caps.NumberInputValueCaps,c.caps.NumberInputButtonCaps,c.inputIds);
            ids(HidP_Feature,c.caps.NumberFeatureValueCaps,c.caps.NumberFeatureButtonCaps,c.featureIds);
        }
        HidD_FreePreparsedData(pp);
        if (capStatus!=HIDP_STATUS_SUCCESS) continue;
        c.manufacturer=HidText(h.h,HidD_GetManufacturerString); c.product=HidText(h.h,HidD_GetProductString);
        GUID container{}; DEVPROPTYPE type=0;
        if (SetupDiGetDevicePropertyW(set,&info,&DEVPKEY_Device_ContainerId,&type,
            reinterpret_cast<PBYTE>(&container),sizeof(container),nullptr,0) && type==DEVPROP_TYPE_GUID) {
            const auto* bytes=reinterpret_cast<const BYTE*>(&container);
            char tmp[3]{};
            for (unsigned j=0;j<sizeof(container);++j) { sprintf_s(tmp,"%02x",bytes[j]); c.container+=tmp; }
        }
        bool inferredId=c.outputIds.empty();
        if (inferredId) c.outputIds.insert(lower.find(L"col06")!=std::wstring::npos ? 6u : 1u);
        auto jsonIds=[](const std::set<unsigned>& values) {
            std::string text="[";for(auto value:values){if(text.size()>1)text+=',';text+=std::to_string(value);}return text+']';
        };
        std::string ids=jsonIds(c.outputIds);
        log.Line("interface","\"path_hash\":"+Q(c.hash)+",\"container\":"+Q(c.container)+
            ",\"product\":"+Q(Utf8(c.product))+",\"manufacturer\":"+Q(Utf8(c.manufacturer))+
            ",\"usb_bcd_device\":"+std::to_string(c.attributes.VersionNumber)+
            ",\"usage_page\":"+std::to_string(c.caps.UsagePage)+",\"usage\":"+std::to_string(c.caps.Usage)+
            ",\"input_bytes\":"+std::to_string(c.caps.InputReportByteLength)+",\"output_bytes\":"+std::to_string(c.caps.OutputReportByteLength)+
            ",\"feature_bytes\":"+std::to_string(c.caps.FeatureReportByteLength)+",\"output_report_ids\":"+ids+
            ",\"input_report_ids\":"+jsonIds(c.inputIds)+",\"feature_report_ids\":"+jsonIds(c.featureIds)+
            ",\"report_id_inferred_from_sdk_rule\":"+(inferredId?"true":"false"));
        // Keep keyboard/mouse collections in metadata only, never read their input.
        if (c.caps.UsagePage>=0xff00 &&
            ((c.caps.InputReportByteLength>0 && c.caps.InputReportByteLength<=512) ||
             (c.caps.FeatureReportByteLength>0 && c.caps.FeatureReportByteLength<=512)))
            found.push_back(std::move(c));
    }
    SetupDiDestroyDeviceInfoList(set); log.Flush(); return found;
}
struct Profile { std::string hash; unsigned id=1; bool control=false, exclusive=false; };
static DWORD Child(const fs::path&,const fs::path&,const Profile&,const std::string&,
    const std::string&,DWORD,bool,Log&);
static fs::path ThisExe(){wchar_t path[32768]{};GetModuleFileNameW(nullptr,path,32768);return path;}

struct Reader {
    HANDLE handle; bool owns;
    std::string hash;
    std::vector<std::uint8_t> buffer;
    HidIoOperation io;
    bool active=false, dead=false;
    Reader(HANDLE h,bool owner,std::string key,unsigned bytes):handle(h),owns(owner),hash(std::move(key)),buffer(bytes),io(h){}
    ~Reader() { IoDeadline deadline; if(active)io.CancelAndDrain(nullptr,nullptr); if(owns)CloseHandle(handle); }
    void Arm(Log& log) {
        if(active||dead)return;
        IoDeadline deadline;
        DWORD error=0; std::fill(buffer.begin(),buffer.end(),0);
        auto state=io.StartRead(buffer.data(),static_cast<DWORD>(buffer.size()),&error);
        active=state!=HidIoOperation::StartResult::Failed;
        if(!active){dead=true;log.Line("read_error","\"path_hash\":"+Q(hash)+",\"error\":"+std::to_string(error));}
    }
};
static void ConfigureQueue(HANDLE handle,const std::string& hash,Log& log){
    bool set=HidD_SetNumInputBuffers(handle,512)!=FALSE;DWORD setError=set?0:GetLastError();
    ULONG actual=0;bool got=HidD_GetNumInputBuffers(handle,&actual)!=FALSE;
    log.Line("input_queue","\"path_hash\":"+Q(hash)+",\"requested\":512,\"set_ok\":"+(set?"true":"false")+
        ",\"set_error\":"+std::to_string(setError)+",\"queried\":"+(got?"true":"false")+",\"actual\":"+std::to_string(actual));
}
struct Session {
    Log& log; Profile profile; Handle writer;
    std::vector<std::unique_ptr<Reader>> readers;
    bool verified=false, subscribed=false;
    unsigned outputBytes=64;
    Metrics state;
    const std::vector<Candidate>& candidates;
    std::set<std::string> timedOutSnapshots;
    Session(Log& l,const Profile& p,const std::vector<Candidate>& cs):log(l),profile(p),candidates(cs) { Open(); }
    void Open() {
        IoDeadline deadline;
        const auto& cs=candidates;const auto& p=profile;
        auto it=std::find_if(cs.begin(),cs.end(),[&](const auto& c){return c.hash==p.hash;});
        if(it==cs.end() || !it->caps.InputReportByteLength || it->caps.InputReportByteLength>512 ||
            (it->caps.OutputReportByteLength!=64 && it->caps.OutputReportByteLength!=65))return;
        outputBytes=it->caps.OutputReportByteLength;
        writer.h=CreateFileW(it->path.c_str(),GENERIC_READ|GENERIC_WRITE,p.exclusive?0:FILE_SHARE_READ|FILE_SHARE_WRITE,
            nullptr,OPEN_EXISTING,FILE_FLAG_OVERLAPPED,nullptr);
        if(!writer){log.Line("open_error","\"error\":"+std::to_string(GetLastError()));return;}
        ConfigureQueue(writer.h,it->hash,log);
        readers.push_back(std::make_unique<Reader>(writer.h,false,it->hash,it->caps.InputReportByteLength));
        for(const auto& c:cs) if(c.hash!=it->hash && !c.container.empty() && c.container==it->container &&
            c.caps.InputReportByteLength>0 && c.caps.InputReportByteLength<=512 && readers.size()<16) {
            HANDLE h=CreateFileW(c.path.c_str(),GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_EXISTING,FILE_FLAG_OVERLAPPED,nullptr);
            if(h!=INVALID_HANDLE_VALUE){ConfigureQueue(h,c.hash,log);readers.push_back(std::make_unique<Reader>(h,true,c.hash,c.caps.InputReportByteLength));}
        }
        for(auto& r:readers)r->Arm(log);
    }
    ~Session() {
        IoDeadline deadline;
        if(subscribed && writer) { try { Send(Command::Unsubscribe); } catch(...){} }
        readers.clear();
        if(writer){CloseHandle(writer.h);writer.h=INVALID_HANDLE_VALUE;}
    }
    void CloseForSnapshot() {
        if(subscribed && writer)Send(Command::Unsubscribe);
        readers.clear();
        if(writer){CloseHandle(writer.h);writer.h=INVALID_HANDLE_VALUE;}
        verified=false;
        log.Line("session_closed_for_standard_snapshot");
    }
    bool Send(Command c,const Mask& m={},unsigned row=0,unsigned col=0) {
        IoDeadline deadline;
        if(!writer || (c!=Command::Identity && !verified))return false;
        auto report=Request(c,static_cast<std::uint8_t>(profile.id),m,row,col);
        if(!Allowed(report))throw std::runtime_error("TX outside diagnostic command set");
        std::vector<std::uint8_t> packet(outputBytes,0);std::copy(report.begin(),report.end(),packet.begin());
        log.Line("tx","\"path_hash\":"+Q(profile.hash)+",\"data\":"+Q(Hex(packet))+
            ",\"control\":"+(profile.control?"true":"false"));
        // Mark before the potentially blocking call so normal exception cleanup attempts unsubscribe.
        if(c==Command::Subscribe)subscribed=true;
        DWORD error=0,bytes=0; bool ok=false;
        if(profile.control) {ok=HidD_SetOutputReport(writer.h,packet.data(),static_cast<ULONG>(packet.size()))!=FALSE;if(!ok)error=GetLastError();}
        else {
            HidIoOperation io(writer.h);
            auto start=io.StartWrite(packet.data(),static_cast<DWORD>(packet.size()),&error);
            if(start!=HidIoOperation::StartResult::Failed) {
                if(start==HidIoOperation::StartResult::Pending && io.Wait(300)!=WAIT_OBJECT_0) {
                    io.CancelAndDrain(nullptr,nullptr);error=WAIT_TIMEOUT;
                } else ok=io.Finish(&bytes,&error,false)&&bytes==packet.size();
            }
        }
        log.Line("tx_result","\"ok\":"+std::string(ok?"true":"false")+",\"error\":"+std::to_string(error));
        if(c==Command::Unsubscribe && ok)subscribed=false;
        return ok;
    }
    bool Poll(DWORD timeout,const std::function<void(const Report&)>& receive={}) {
        std::vector<HANDLE> events;std::vector<Reader*> owners;
        for(auto& r:readers){r->Arm(log);if(r->active){events.push_back(r->io.Event());owners.push_back(r.get());}}
        if(events.empty()){Sleep(std::min<DWORD>(timeout,50));return false;}
        DWORD wait=WaitForMultipleObjects(static_cast<DWORD>(events.size()),events.data(),FALSE,timeout);
        if(wait>=WAIT_OBJECT_0+events.size())return false;
        Reader& r=*owners[wait-WAIT_OBJECT_0];DWORD bytes=0,error=0;
        bool ok=r.io.Finish(&bytes,&error,false);r.active=false;
        if(ok && bytes==r.buffer.size()) {
            log.Line("rx","\"path_hash\":"+Q(r.hash)+",\"data\":"+Q(Hex(r.buffer)));
            if(r.buffer.size()>=64){
                Report canonical{};std::copy_n(r.buffer.begin(),64,canonical.begin());
                state.Feed(canonical);if(receive)receive(canonical);
            }
        } else {r.dead=true;log.Line("read_error","\"path_hash\":"+Q(r.hash)+",\"bytes\":"+std::to_string(bytes)+",\"error\":"+std::to_string(error));}
        r.Arm(log);
        // Fairness: a busy first collection must not starve a reply on another one.
        auto it=std::find_if(readers.begin(),readers.end(),[&](const auto& item){return item.get()==&r;});
        if(it!=readers.end())std::rotate(it,it+1,readers.end());
        return ok;
    }
    void Drain(unsigned ms) {auto end=GetTickCount64()+ms;while(!stopping&&GetTickCount64()<end)Poll(20);}
    bool Query(Command command,unsigned ms,const std::function<bool(const Report&)>& match,unsigned row=0,unsigned col=0) {
        if(!Send(command,{},row,col))return false;
        bool found=false;auto end=GetTickCount64()+ms;
        while(!stopping&&!found&&GetTickCount64()<end)Poll(20,[&](const Report& r){found=match(r);});
        return found;
    }
    int Identify() {
        irok_nd75::DeviceInfo info{};
        bool identity=Query(Command::Identity,1100,[&](const Report& raw){auto r=Normalized(raw);return irok_nd75::DecodeDeviceInfo(r.data(),64,&info);});
        log.Line("identity","\"decoded\":"+std::string(identity?"true":"false")+",\"controller\":"+Q(info.controller.data())+
            ",\"product\":"+Q(info.product.data())+",\"firmware\":"+Q(info.firmware.data()));
        verified=identity&&IsNa87(info);
        if(!verified)return 0;
        irok_nd75::CapabilityInfo cap{};
        bool capability=Query(Command::Capability,900,[&](const Report& raw){auto r=Normalized(raw);return irok_nd75::DecodeCapabilityInfo(r.data(),64,&cap);});
        log.Line("capability","\"decoded_with_nd75_reference\":"+std::string(capability?"true":"false")+",\"value\":"+std::to_string(cap.sensitivity));
        return capability?3:2;
    }
};
static int ReadControl(Log& log,const Profile& p,const std::vector<Candidate>& candidates,bool feature) {
    auto it=std::find_if(candidates.begin(),candidates.end(),[&](const auto& c){return c.hash==p.hash;});
    if(it==candidates.end())return 6;
    unsigned length=feature?it->caps.FeatureReportByteLength:it->caps.InputReportByteLength;
    const auto& ids=feature?it->featureIds:it->inputIds;
    if(!length || length>512 || (!ids.empty()&&!ids.contains(p.id)) || (ids.empty()&&p.id!=0&&!it->outputIds.contains(p.id)))return 6;
    IoDeadline deadline(1000);
    DWORD access=GENERIC_READ;
    Handle handle(CreateFileW(it->path.c_str(),access,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_EXISTING,0,nullptr));
    if(!handle && GetLastError()==ERROR_ACCESS_DENIED){
        access=0;handle.h=CreateFileW(it->path.c_str(),0,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_EXISTING,0,nullptr);
    }
    if(!handle){log.Line("snapshot_open_error","\"error\":"+std::to_string(GetLastError()));return 6;}
    log.Line("snapshot_scope","\"path_hash\":"+Q(p.hash)+",\"bytes\":"+std::to_string(length)+",\"desired_access\":"+std::to_string(access)+
        ",\"id_inferred_from_output_or_unnumbered\":"+(ids.empty()?"true":"false"));
    unsigned successes=0;
    for(unsigned sample=0;sample<4&&!stopping;++sample) {
        std::vector<std::uint8_t> data(length,0);data[0]=static_cast<std::uint8_t>(p.id);
        BOOL ok=feature?HidD_GetFeature(handle.h,data.data(),length):HidD_GetInputReport(handle.h,data.data(),length);
        DWORD error=ok?0:GetLastError();successes+=ok!=FALSE;
        log.Line("control_snapshot","\"method\":"+Q(feature?"GetFeature":"GetInputReport")+
            ",\"report_id\":"+std::to_string(p.id)+",\"sample\":"+std::to_string(sample)+
            ",\"ok\":"+(ok?"true":"false")+",\"error\":"+std::to_string(error)+
            ",\"data\":"+Q(Hex(data)));
        if(!ok)break;
        Sleep(40);
    }
    log.Flush();return successes?0:6;
}
static void Snapshots(Log& log,const std::vector<Candidate>& candidates,const std::string& container,
    const std::string& fallbackHash,const std::string& phase,std::set<std::string>& timedOut) {
    log.phase=phase;unsigned count=0;auto deadline=GetTickCount64()+15000;
    for(const auto& c:candidates) {
        if(!fallbackHash.empty() && c.hash!=fallbackHash && (container.empty()||c.container!=container))continue;
        for(bool feature:{false,true}) {
            unsigned bytes=feature?c.caps.FeatureReportByteLength:c.caps.InputReportByteLength;
            if(!bytes || bytes>512)continue;
            auto ids=feature?c.featureIds:c.inputIds;if(ids.empty()){ids=c.outputIds;ids.insert(0);}
            for(unsigned id:ids) {
                if(stopping || count>=16 || GetTickCount64()>deadline){
                    log.Line("snapshot_budget_end","\"attempts\":"+std::to_string(count));log.Flush();return;
                }
                std::string route=c.hash+(feature?"-feature-":"-input-")+std::to_string(id);
                if(timedOut.contains(phase+"-"+route))continue;
                Profile p{c.hash,id,false,false};
                auto prefix=phase+"-"+std::to_string(count++);
                auto result=Child(ThisExe(),log.directory,p,feature?"feature_snapshot":"input_snapshot",prefix,1600,true,log);
                log.Line("snapshot_result","\"prefix\":"+Q(prefix)+",\"route\":"+Q(route)+",\"exit_code\":"+std::to_string(result));
                if(result==101 || result==102)timedOut.insert(phase+"-"+route);
            }
        }
    }
    log.Flush();
}
static void Snapshots(Session& s,const std::string& phase) {
    auto it=std::find_if(s.candidates.begin(),s.candidates.end(),[&](const auto& c){return c.hash==s.profile.hash;});
    s.CloseForSnapshot();
    Snapshots(s.log,s.candidates,it==s.candidates.end()?"":it->container,s.profile.hash,phase,s.timedOutSnapshots);
    if(!stopping){s.Open();if(!s.Identify())throw std::runtime_error("identity could not be restored after standard snapshots");}
}
static void Check(bool yes,const char* what){if(!yes)throw std::runtime_error(what);}
static bool Prompt(const std::string& text) {
    if(stopping)return false;
    std::cout<<"\n"<<text<<"\nНажмите Enter, чтобы начать. Ctrl+C — остановить тест.\n"<<std::flush;
    HANDLE input=GetStdHandle(STD_INPUT_HANDLE); FlushConsoleInputBuffer(input);
    while(!stopping) {
        if(WaitForSingleObject(input,100)!=WAIT_OBJECT_0)continue;
        INPUT_RECORD record{};DWORD count=0;
        if(!ReadConsoleInputW(input,&record,1,&count))return false;
        if(count && record.EventType==KEY_EVENT && record.Event.KeyEvent.bKeyDown && record.Event.KeyEvent.wVirtualKeyCode==VK_RETURN)return true;
    }
    return false;
}
static std::string Last(const Metrics& m) {
    std::string s="[";for(auto value:m.last){if(s.size()>1)s+=',';s+=std::to_string(value);}return s+']';
}
static void Summary(Session& s,const Metrics& m) {
    unsigned positive=0,zero=0,nonzero=0;
    for(unsigned i=0;i<Slots;++i){positive+=m.positive[i]!=0;zero+=m.zero[i]!=0;nonzero+=s.state.last[i]>0;}
    s.log.Line("phase_summary","\"events\":"+std::to_string(m.events)+",\"above_nd75_reference_range\":"+std::to_string(m.invalid)+
        ",\"positions_positive\":"+std::to_string(positive)+",\"positions_zero\":"+std::to_string(zero)+
        ",\"last_reported_nonzero_positions\":"+std::to_string(nonzero)+",\"last_reported_depths\":"+Last(s.state));
    s.log.Flush();
}
static Metrics Capture(Session& s,const std::string& name,unsigned seconds,int rotation=0) {
    s.log.phase=name;s.log.Line("phase_start","\"duration_seconds\":"+std::to_string(seconds));
    Metrics metrics;auto end=GetTickCount64()+seconds*1000ull;auto next=GetTickCount64();unsigned step=0;
    while(!stopping&&GetTickCount64()<end) {
        if(rotation && GetTickCount64()>=next) {
            Mask mask{};
            if(rotation==1)mask[step%22]=63;
            if(rotation==2)for(unsigned c=0;c<22;++c)mask[c]=static_cast<std::uint8_t>((step%2)?0x38:0x07);
            s.Send(Command::Subscribe,mask);next=GetTickCount64()+100;++step;
        }
        s.Poll(20,[&](const Report& r){metrics.Feed(r);});
    }
    Summary(s,metrics);return metrics;
}
static void MapRead(Session& s,const std::string& phase) {
    s.log.phase=phase;s.log.Line("map_start");
    if(s.Send(Command::Map))s.Drain(700);
    s.log.Line("map_end");s.log.Flush();
}
static void Sweep(Session& s,const std::string& phase) {
    s.log.phase=phase;unsigned answers=0;auto end=GetTickCount64()+15000;
    for(unsigned row=0;row<6&&!stopping&&GetTickCount64()<end;++row)for(unsigned col=0;col<22&&!stopping&&GetTickCount64()<end;++col) {
        s.log.Line("config_query","\"row\":"+std::to_string(row)+",\"column\":"+std::to_string(col));
        bool ok=s.Query(Command::Configuration,80,[](const Report& r){return r[1]==0x21&&r[6]==5;},row,col);
        answers+=ok;s.log.Line("config_result","\"matched_response\":"+std::string(ok?"true":"false"));
    }
    s.log.Line("sweep_end","\"matched_responses\":"+std::to_string(answers)+",\"correlation\":\"sequential requests; protocol has no sequence id\"");s.log.Flush();
}
static int Exercise(Session& s,bool reconnect) {
    if(!s.Identify())return 5;
    if(!Prompt(reconnect ? "Повторное подключение: в течение 12 секунд нажимайте и отпускайте WASD." :
        "Начало полного теста NA87. Отпустите все клавиши. Первые чтения займут до 40 секунд."))return 4;
    s.Send(Command::Unsubscribe);s.Drain(250);
    if(reconnect){s.Send(Command::Subscribe,All());Capture(s,"reconnect_wasd",12);s.Send(Command::Unsubscribe);return 0;}
    MapRead(s,"map_rest");Sweep(s,"configuration_rest");Snapshots(s,"hid_snapshot_rest");
    if(!Prompt("Общая подписка: 75 секунд по очереди нажимайте и отпускайте все клавиши.\nНажимайте модификаторы отдельно, без сочетаний. Затем подождите следующую инструкцию."))return 4;
    s.Send(Command::Subscribe,All());s.Drain(200);Capture(s,"all_keys_coverage",75);
    std::array<std::optional<unsigned>,4> learned;
    const char* labels[]={"W","A","S","D"};
    for(unsigned key=0;key<4&&!stopping;++key) {
        for(unsigned attempt=0;attempt<2&&!learned[key]&&!stopping;++attempt) {
            if(!Prompt(std::string("Обучение ")+labels[key]+": в течение 8 секунд несколько раз медленно нажмите и отпустите только эту клавишу."))return 4;
            s.Send(Command::Subscribe,All());s.Drain(200);
            auto m=Capture(s,std::string("learn_")+labels[key]+"_"+std::to_string(attempt),8);
            learned[key]=m.Learn();s.log.Line("learn_result","\"key\":"+Q(labels[key])+",\"slot\":"+(learned[key]?std::to_string(*learned[key]):"null"));
        }
    }
    auto multi=[&](const char* name,const Mask& mask,const std::string& instruction,int rotation=0) {
        if(!Prompt(instruction+"\nПосле звукового сигнала отпустите всё и подождите 5 секунд."))return;
        s.Send(Command::Subscribe,mask);s.Drain(200);Capture(s,name,15,rotation);
        Beep(900,150);std::cout<<"Отпустите все клавиши.\n"<<std::flush;
        Capture(s,std::string(name)+"_released",5,rotation);
    };
    multi("all_wasd",All(),"Общая подписка: удерживайте WASD вместе, меняйте глубину W/A, затем отпускайте W при удержании остальных.");
    Mask subset{};unsigned known=0;
    for(auto slot:learned)if(slot){auto m=Only(*slot);for(unsigned c=0;c<22;++c)subset[c]|=m[c];++known;}
    if(known)multi("learned_subset",subset,"Выборочная подписка: повторите движения WASD и отпускания по одной клавише.");
    if(learned[0])multi("only_w",Only(*learned[0]),"Подписка только на W: двигайте W, одновременно удерживая и двигая A/S/D.");
    multi("rotating_columns",All(),"Сменяющиеся колонки: медленно двигайте WASD и отпускайте клавиши по одной.",1);
    multi("alternating_rows",All(),"Две группы строк: повторите медленные движения WASD и отдельные отпускания.",2);
    s.Send(Command::Unsubscribe);s.Drain(250);
    if(!Prompt("Чтения без подписки: после Enter зажмите WASD и держите до следующего сигнала (до 40 секунд)."))return 4;
    Sleep(1500);MapRead(s,"map_held");Sweep(s,"configuration_held");Snapshots(s,"hid_snapshot_held");Beep(900,150);
    std::cout<<"Отпустите всё.\n"<<std::flush;Sleep(1500);MapRead(s,"map_released");Sweep(s,"configuration_released");Snapshots(s,"hid_snapshot_released");
    if(!Prompt("Повторная подписка: после Enter нажмите WASD и держите неподвижно до сигнала (около 10 секунд)."))return 4;
    s.Send(Command::Subscribe,All());Capture(s,"before_resubscribe",4);
    s.Send(Command::Unsubscribe);s.Drain(400);s.Send(Command::Subscribe,All());Capture(s,"stationary_resubscribe",5);
    Beep(900,150);std::cout<<"Отпустите всё.\n"<<std::flush;Capture(s,"final_released",6);
    s.Send(Command::Unsubscribe);s.Drain(250);s.log.Line("exercise_complete");s.log.Flush();
    return stopping?4:0;
}
static std::wstring QuoteArg(const std::wstring& s) {
    // These arguments are internal paths/decimal/hex tokens, not a shell command.
    std::wstring out=L"\"";unsigned slashes=0;
    for(wchar_t c:s){if(c==L'\\'){++slashes;continue;}if(c==L'"'){out.append(slashes*2+1,L'\\');out+=c;}else{out.append(slashes,L'\\');out+=c;}slashes=0;}
    out.append(slashes*2,L'\\');return out+L'"';
}
static DWORD Child(const fs::path& exe,const fs::path& out,const Profile& p,const std::string& mode,
    const std::string& prefix,DWORD timeout,bool hidden,Log& log) {
    auto wide=[](const std::string& s){return std::wstring(s.begin(),s.end());};
    std::wstring cmd=QuoteArg(exe.wstring())+L" --worker "+wide(mode)+L" "+QuoteArg(out.wstring())+L" "+wide(prefix)+L" "+
        wide(p.hash)+L" "+std::to_wstring(p.id)+L" "+(p.control?L"1":L"0")+L" "+(p.exclusive?L"1":L"0");
    STARTUPINFOW startup{};startup.cb=sizeof(startup);PROCESS_INFORMATION process{};
    DWORD flags=hidden?CREATE_NO_WINDOW:CREATE_NEW_PROCESS_GROUP;
    if(!CreateProcessW(exe.c_str(),cmd.data(),nullptr,nullptr,FALSE,flags,nullptr,nullptr,&startup,&process)) {
        log.Line("child_start_error","\"error\":"+std::to_string(GetLastError()));return 100;
    }
    Handle ph(process.hProcess),th(process.hThread);auto deadline=GetTickCount64()+timeout;
    while(WaitForSingleObject(ph.h,100)==WAIT_TIMEOUT) {
        if((stopping && mode!="cleanup") || GetTickCount64()>deadline) {
            log.Line("worker_timeout_or_cancel","\"prefix\":"+Q(prefix));
            if(!hidden)GenerateConsoleCtrlEvent(CTRL_BREAK_EVENT,process.dwProcessId);
            if(WaitForSingleObject(ph.h,1500)==WAIT_TIMEOUT)TerminateProcess(ph.h,101);
            WaitForSingleObject(ph.h,2000);return 101;
        }
    }
    DWORD code=100;GetExitCodeProcess(ph.h,&code);return code;
}
static void SelfTest() {
    Check(Q("a\n\"\\")=="\"a\\u000a\\\"\\\\\"","JSON escaping");
    Check(Request(Command::Capability,1)==Report{1,0x21,0,0,0,0x18,4},"capability golden");
    Check(Request(Command::Unsubscribe,6)==Report{6,0x21,0,0,0,0x18,3},"report6 golden");
    for(unsigned row=0;row<6;++row)for(unsigned col=0;col<22;++col)
        Check(Allowed(Request(Command::Configuration,1,{},row,col)),"valid coordinate");
    auto bad=Request(Command::Configuration,1);bad[7]=6;Check(!Allowed(bad),"invalid row");
    bad=Request(Command::Configuration,1);bad[8]=22;Check(!Allowed(bad),"invalid column");
    bad=Request(Command::Subscribe,1,All());bad[7]=64;Check(!Allowed(bad),"invalid mask");
    for(unsigned command=0;command<256;++command) {
        Report arbitrary{};arbitrary[0]=1;arbitrary[1]=static_cast<std::uint8_t>(command);
        Check(Allowed(arbitrary)==(command==0x0d||command==0x10),"unlisted command");
    }
    for(unsigned slot=0;slot<Slots;++slot){auto m=Only(slot);unsigned bits=0;for(auto b:m)for(unsigned k=0;k<8;++k)bits+=(b>>k)&1;Check(bits==1,"single slot mask");}
    std::vector<std::uint8_t> raw65(65,0);raw65[64]=0xab;Check(Hex(raw65).size()==130 && Hex(raw65).ends_with("ab"),"complete 65-byte raw record");
    Metrics m;Report e{1,0x21,0,0,0,3,1,2,2,16};m.Feed(e);e[9]=20;m.Feed(e);
    Check(m.Learn()==46,"coordinate learning");e[9]=0;m.Feed(e);Check(m.last[46]==0&&m.zero[46]==1,"zero release");
    e[9]=255;m.Feed(e);Check(m.invalid==1&&m.last[46]==0,"reference range accounting");
    e[7]=3;e[9]=20;m.Feed(e);Check(!m.Learn(),"ambiguous learning");
    auto count=m.events;e[2]=1;m.Feed(e);Check(m.events==count,"non-reference header retained raw, not counted");
    irok_nd75::DeviceInfo info{};strcpy_s(info.controller.data(),info.controller.size(),"M484");
    strcpy_s(info.product.data(),info.product.size(),"GK8260HERGB");Check(IsNa87(info),"NA87 identity");
    strcpy_s(info.product.data(),info.product.size(),"X86HERGB");Check(!IsNa87(info),"ND75 is not NA87");
    Check(QuoteArg(L"C:\\folder with space\\")==L"\"C:\\folder with space\\\\\"","Windows argument quoting");
    std::cout<<"NA87_DIAGNOSTIC_SELF_TEST=PASS\n";
}
int wmain(int argc,wchar_t** argv) {
    SetConsoleOutputCP(CP_UTF8);SetConsoleCtrlHandler(nullptr,FALSE);SetConsoleCtrlHandler(ConsoleStop,TRUE);
    try {
        if(argc==2&&std::wstring_view(argv[1])==L"--self-test"){SelfTest();return 0;}
        if(argc==3&&std::wstring_view(argv[1])==L"--watchdog-self-test"){
            Watchdog watchdog(argv[2]);IoDeadline deadline(200);Sleep(2000);return 99;
        }
        if(argc==3&&std::wstring_view(argv[1])==L"--parent-timeout-self-test") {
            fs::path out=argv[2];Log log(out/"parent-timeout.jsonl");
            wchar_t module[32768]{};GetModuleFileNameW(nullptr,module,32768);
            Profile p{"self-test",1,false,false};
            Check(Child(module,out,p,"test_hang","hung-worker",100,true,log)==101,"parent timeout");
            std::cout<<"NA87_PARENT_TIMEOUT_SELF_TEST=PASS\n";return 0;
        }
        if(argc==9&&std::wstring_view(argv[1])==L"--worker") {
            const std::string mode=Utf8(argv[2]),prefix=Utf8(argv[4]);fs::path out=argv[3];
            Profile p{Utf8(argv[5]),static_cast<unsigned>(std::stoul(argv[6])),std::wstring_view(argv[7])==L"1",std::wstring_view(argv[8])==L"1"};
            Log log(out/(prefix+".jsonl"));log.phase=prefix;log.Line("profile","\"path_hash\":"+Q(p.hash)+",\"report_id\":"+std::to_string(p.id)+
                ",\"control\":"+(p.control?"true":"false")+",\"exclusive\":"+(p.exclusive?"true":"false"));
            Watchdog watchdog(out/(prefix+"-timeout.txt"));
            if(mode=="test_hang"){Sleep(10000);return 99;}
            auto candidates=Enumerate(log);
            if(mode=="input_snapshot" || mode=="feature_snapshot")return ReadControl(log,p,candidates,mode=="feature_snapshot");
            Session session(log,p,candidates);int code=0;
            if(mode=="probe")code=session.Identify();
            else if(mode=="cleanup"){if(session.Identify()){if(!session.Send(Command::Unsubscribe))code=6;session.Drain(200);}else code=5;}
            else code=Exercise(session,mode=="reconnect");
            log.Line("worker_result","\"exit_code\":"+std::to_string(code));log.Flush();return code;
        }
        if(argc!=3||(std::wstring_view(argv[1])!=L"--output" && std::wstring_view(argv[1])!=L"--inventory")) {
            std::cout<<"Запустите START-NA87-TEST.cmd из распакованного пакета.\n";return 1;
        }
        fs::path out=fs::absolute(argv[2]);if(!fs::is_directory(out))throw std::runtime_error("output directory missing");
        Log log(out/"session.jsonl");Watchdog watchdog(out/"enumeration-timeout.txt");
        if(std::wstring_view(argv[1])==L"--inventory"){auto cs=Enumerate(log);log.Line("inventory_complete");return cs.empty()?2:0;}
        std::cout<<"NA87: расширенная диагностика. Прошивка и настройки не записываются.\n"
            "Закройте HallJoy, официальный драйвер и веб-конфигуратор.\n"
            "Подключите одну обычную NA87 по USB. Тест займёт примерно 6–10 минут.\n"<<std::flush;
        if(!Prompt("Готовность к тесту"))return 4;
        auto candidates=Enumerate(log);
        if(candidates.empty()){log.Line("no_matching_vendor_interface");std::cout<<"Подходящие vendor-интерфейсы не найдены. Сведения сохранены.\n";return 2;}
        wchar_t module[32768]{};GetModuleFileNameW(nullptr,module,32768);fs::path exe=module;
        std::vector<Profile> profiles;
        for(bool exclusive:{false,true})for(bool control:{false,true})for(const auto& c:candidates)for(auto id:c.outputIds)
            if(c.caps.InputReportByteLength>0 && c.caps.InputReportByteLength<=512 &&
                (c.caps.OutputReportByteLength==64 || c.caps.OutputReportByteLength==65) && profiles.size()<64)profiles.push_back({c.hash,id,control,exclusive});
        std::optional<Profile> best;DWORD bestScore=0;unsigned attempt=0;
        for(const auto& p:profiles) {
            if(stopping)break;
            std::cout<<"Проверка интерфейсов: "<<(attempt+1)<<"/"<<profiles.size()<<"\n"<<std::flush;
            auto prefix="probe-"+std::to_string(attempt++);
            DWORD score=Child(exe,out,p,"probe",prefix,6000,true,log);
            log.Line("probe_result","\"prefix\":"+Q(prefix)+",\"exit_code\":"+std::to_string(score));log.Flush();
            if((score==2||score==3)&&score>bestScore){best=p;bestScore=score;}
        }
        if(!best){
            log.Line("no_verified_na87_identity");
            std::cout<<"Точная идентификация NA87 не получена. Продолжим только стандартные чтения HID.\n";
            std::set<std::string> timedOut;
            if(Prompt("Отпустите все клавиши для контрольного снимка."))Snapshots(log,candidates,"","","fallback_rest",timedOut);
            if(Prompt("После Enter нажмите и держите WASD до сигнала (до 20 секунд).")){
                Sleep(1500);Snapshots(log,candidates,"","","fallback_held",timedOut);Beep(900,150);
                std::cout<<"Отпустите всё.\n";Sleep(1500);Snapshots(log,candidates,"","","fallback_released",timedOut);
            }
            log.Line("fallback_complete");return 3;
        }
        if(stopping)return 4;
        log.Line("selected_profile","\"path_hash\":"+Q(best->hash)+",\"report_id\":"+std::to_string(best->id)+
            ",\"control\":"+(best->control?"true":"false")+",\"exclusive\":"+(best->exclusive?"true":"false"));
        DWORD result=Child(exe,out,*best,"exercise","exercise",20*60*1000,false,log);
        // Cleanup runs even after a timed-out exercise. It can never write settings.
        auto cleanupResult=Child(exe,out,*best,"cleanup","cleanup",6000,true,log);
        log.Line("cleanup_result","\"exit_code\":"+std::to_string(cleanupResult));
        if(result==0&&!stopping && Prompt("Последний этап: отключите NA87, подключите снова и подождите 3 секунды.")) {
            // Re-enumerate after reconnect; do not assume the OS retained the old HID path.
            auto again=Enumerate(log);std::optional<Profile> reconnected;
            for(const auto& c:again)for(auto id:c.outputIds) {
                if(reconnected||stopping)break;
                Profile p{c.hash,id,best->control,best->exclusive};auto prefix="reprobe-"+std::to_string(attempt++);
                DWORD score=Child(exe,out,p,"probe",prefix,6000,true,log);
                if(score==2||score==3)reconnected=p;
            }
            if(reconnected){
                auto reconnectResult=Child(exe,out,*reconnected,"reconnect","reconnect",5*60*1000,false,log);
                auto reconnectCleanup=Child(exe,out,*reconnected,"cleanup","cleanup-reconnect",6000,true,log);
                log.Line("reconnect_result","\"exit_code\":"+std::to_string(reconnectResult)+",\"cleanup_exit_code\":"+std::to_string(reconnectCleanup));
            }
            else log.Line("reconnect_identity_failed");
        }
        log.Line("session_complete","\"exercise_exit_code\":"+std::to_string(result));log.Flush();
        std::cout<<"Сбор завершён. Пакет с результатами создаст запускающий скрипт.\n"
            "Если тест был прерван, переподключите клавиатуру перед обычной работой.\n"<<std::flush;
        return static_cast<int>(result);
    } catch(const std::exception& e){std::cerr<<"Diagnostic error: "<<e.what()<<"\n";return 10;}
}

#if defined(HALLJOY_IROK_NA87_DIAGNOSTIC)
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
#include "../../../tools/irok_na87_diagnostic/protocol.h"
#include "irok_na87_diagnostic.h"
#include "debug_log.h"
#include "stability_trace.h"
#include <shellapi.h>
#include <map>
#include <mutex>

#if defined(_MSC_VER)
#pragma comment(lib, "setupapi.lib")
#pragma comment(lib, "hid.lib")
#pragma comment(lib, "uuid.lib")
#endif
namespace {
namespace fs = std::filesystem;
using namespace na87diag;
static std::atomic<bool> stopping{false};
static std::atomic<std::uint64_t> ioDeadline{UINT64_MAX};
struct IoDeadline {
    std::uint64_t previous;
    explicit IoDeadline(unsigned ms=4000):previous(ioDeadline.exchange(GetTickCount64()+ms)){}
    ~IoDeadline(){ioDeadline=previous;}
};
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
    explicit Log(const fs::path& p):directory(p.parent_path()) {
        HANDLE duplicate=nullptr;
        if(DuplicateHandle(GetCurrentProcess(),GetStdHandle(STD_OUTPUT_HANDLE),GetCurrentProcess(),
            &duplicate,0,FALSE,DUPLICATE_SAME_ACCESS))file.h=duplicate;
        if (!file) throw std::runtime_error("cannot create new evidence file");
        SYSTEMTIME t{}; GetSystemTime(&t);
        char utc[40]{}; sprintf_s(utc, "%04u-%02u-%02uT%02u:%02u:%02uZ", t.wYear,t.wMonth,t.wDay,t.wHour,t.wMinute,t.wSecond);
        Line("start", "\"utc\":" + Q(utc) + ",\"schema\":1");
    }
    void Line(const std::string& kind, const std::string& fields = "") {
        std::string s = "{\"qpc_us\":" + std::to_string(Us()) + ",\"us\":" + std::to_string(Us()-start) + ",\"kind\":" + Q(kind) +
            ",\"phase\":" + Q(phase) + (fields.empty() ? "" : "," + fields) + "}\n";
        DWORD written = 0;
        if (!WriteFile(file.h, s.data(), static_cast<DWORD>(s.size()), &written, nullptr) || written != s.size())
            throw std::runtime_error("evidence write failed");
        ++lines;
    }
    void Flush() {} // Pipe is drained continuously by the parent; never wait for a pipe flush.
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
        DWORD error=0; std::fill(buffer.begin(),buffer.end(),std::uint8_t{0});
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
        bool identity=Query(Command::Identity,400,[&](const Report& raw){auto r=Normalized(raw);return irok_nd75::DecodeDeviceInfo(r.data(),64,&info);});
        log.Line("identity","\"decoded\":"+std::string(identity?"true":"false")+",\"controller\":"+Q(info.controller.data())+
            ",\"product\":"+Q(info.product.data())+",\"firmware\":"+Q(info.firmware.data()));
        verified=identity&&IsNa87(info);
        if(!verified)return 0;
        irok_nd75::CapabilityInfo cap{};
        bool capability=Query(Command::Capability,250,[&](const Report& raw){auto r=Normalized(raw);return irok_nd75::DecodeCapabilityInfo(r.data(),64,&cap);});
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

static std::wstring Quote(const std::wstring& s) {
    std::wstring result=L"\"";unsigned slash=0;
    for(wchar_t c:s){if(c==L'\\'){++slash;continue;}result.append(c==L'"'?slash*2+1:slash,L'\\');slash=0;result+=c;}
    result.append(slash*2,L'\\');return result+L'"';
}
static void Require(bool value,const char* label){if(!value)throw std::runtime_error(label);}
static void MetricsLog(Session& s) {
    unsigned presses=0,releases=0;std::string last="[";
    for(unsigned i=0;i<Slots;++i){presses+=s.state.positive[i]!=0;releases+=s.state.zero[i]!=0;
        if(i)last+=',';last+=std::to_string(s.state.last[i]);}
    last+=']';
    s.log.Line("metrics","\"events\":"+std::to_string(s.state.events)+",\"positions_positive\":"+std::to_string(presses)+
        ",\"positions_zero\":"+std::to_string(releases)+",\"invalid_reference_depths\":"+std::to_string(s.state.invalid)+
        ",\"last_reported_not_physical_state\":"+last);
}
static void PlanSnapshots(Log& log,const std::vector<Candidate>& candidates) {
    for(const auto& c:candidates)for(bool feature:{false,true}) {
        unsigned bytes=feature?c.caps.FeatureReportByteLength:c.caps.InputReportByteLength;
        if(!bytes || bytes>512)continue;
        const auto& ids=feature?c.featureIds:c.inputIds;
        // No report-ID sweep: only parsed report IDs, or the descriptor's unnumbered report.
        std::set<unsigned> supported=ids;if(supported.empty())supported.insert(0);
        for(unsigned id:supported)log.Line("snapshot_plan","\"route\":"+Q(c.hash+":"+std::to_string(id)+":"+(feature?"f":"i")));
    }
}
static int Automatic(Log& log,const std::vector<Candidate>& candidates,bool cleanup) {
    const auto begin=GetTickCount64();
    if(candidates.empty()){log.Line("not_found");return 2;}
    if(!cleanup)PlanSnapshots(log,candidates);
    std::unique_ptr<Session> selected;
    // Shared access preserves ordinary application ownership. A second transport is
    // useful only when the first failed to obtain identity, never after success.
    for(bool control:{false,true}) {
        for(const auto& c:candidates)for(auto id:c.outputIds) {
            if(stopping || GetTickCount64()-begin>8000 || selected)break;
            if(!c.caps.InputReportByteLength || (c.caps.OutputReportByteLength!=64 && c.caps.OutputReportByteLength!=65))continue;
            Profile p{c.hash,id,control,false};
            log.Line("profile","\"path_hash\":"+Q(c.hash)+",\"report_id\":"+std::to_string(id)+",\"control\":"+(control?"true":"false"));
            auto trial=std::make_unique<Session>(log,p,candidates);
            if(trial->Identify())selected=std::move(trial);
        }
        if(selected || stopping || GetTickCount64()-begin>8000)break;
    }
    if(!selected){log.Line("identity_unavailable");return 3;}
    Session& s=*selected;
    log.Line("selected","\"path_hash\":"+Q(s.profile.hash)+",\"report_id\":"+std::to_string(s.profile.id));
    if(cleanup){bool ok=s.Send(Command::Unsubscribe);s.Drain(80);return ok?0:6;}
    s.Send(Command::Unsubscribe);s.Drain(80);
    s.Send(Command::Map);s.Drain(250);
    // Ordinary configuration is already known to represent settings. Reading all
    // 132 slots while the user types does not resolve the live-state question.
    log.Line("skipped","\"reason\":\"full_settings_sweep_is_not_a_live_snapshot; exclusive_after_shared_success_is_redundant; no_prompted_learning\"");
    if(!s.Send(Command::Subscribe,All()))return 6;
    const auto streamBegin=GetTickCount64();unsigned phase=99;auto next=streamBegin;
    while(!stopping && GetTickCount64()-begin<44000) {
        const auto elapsed=GetTickCount64()-streamBegin;
        unsigned current=elapsed<18000?0:elapsed<26000?1:elapsed<34000?2:3;
        if(current!=phase){
            phase=current;log.phase=phase==0?"all_keys":phase==1?"observed_subset":phase==2?"row_and_column_groups":"all_keys_recheck";
            log.Line("phase_start");MetricsLog(s);
            if(phase==1){
                Mask subset{};unsigned count=0;
                for(unsigned i=0;i<Slots;++i)if(s.state.positive[i]){subset[i%22]|=static_cast<std::uint8_t>(1u<<(i/22));++count;}
                if(count && count<Slots)s.Send(Command::Subscribe,subset);
                else log.Line("phase_skipped","\"reason\":\"no_observed_subset; preserve_all_keys\"");
            }
            if(phase==3){s.Send(Command::Unsubscribe);s.Drain(80);s.Send(Command::Subscribe,All());}
        }
        if(phase==2 && GetTickCount64()>=next){
            Mask mask{};const auto group=(elapsed-26000)/125;
            if(elapsed<30000)mask.fill(static_cast<std::uint8_t>((group%2)?0x38:0x07));
            else mask[group%22]=63;
            s.Send(Command::Subscribe,mask);next=GetTickCount64()+125;
        }
        s.Poll(10);
    }
    MetricsLog(s);const bool clean=s.Send(Command::Unsubscribe);s.Drain(80);
    log.Line("capture_complete","\"cancelled\":"+std::string(stopping?"true":"false")+",\"unsubscribe_tx_ok\":"+(clean?"true":"false"));
    return stopping?4:clean?0:6;
}
static std::atomic<bool> parentStop{false};
static std::atomic<unsigned> uiState{0}, keyCount{0}, releaseCount{0};
static std::thread supervisor;
static std::vector<std::string> snapshotRoutes;
static std::atomic<bool> collecting{false};
static bool pipeTestSeen=false;
static std::atomic<unsigned> maximumHeld{0};
static bool EnoughInput(unsigned keys,unsigned releases,unsigned held){return keys>=8 && releases>=8 && held>=2;}
static void ReceiveLine(const std::string& line) {
    // Child records and digital input share the system-wide QPC clock.
    if(line.find("\"kind\":\"pipe_test\"")!=std::string::npos && line.find("complete_record")!=std::string::npos)pipeTestSeen=true;
    int n=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,line.data(),static_cast<int>(line.size()),nullptr,0);
    if(n<=0 || n>32768)return;
    std::wstring wide(static_cast<size_t>(n),L'\0');
    MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,line.data(),static_cast<int>(line.size()),wide.data(),n);
    // Do not route full descriptor arrays through DebugLog's smaller formatting buffer.
    StabilityTrace_AppendPlain((L"[na87] "+wide).c_str());
    if(line.find("\"kind\":\"snapshot_plan\"")!=std::string::npos){
        const std::string marker="\"route\":\"";auto at=line.find(marker);
        if(at!=std::string::npos){at+=marker.size();auto end=line.find('"',at);
            if(end!=std::string::npos && snapshotRoutes.size()<32)snapshotRoutes.push_back(line.substr(at,end-at));}
    }
}
static DWORD RunChild(const std::wstring& mode,DWORD budget,bool allowCancelled=false) {
    SECURITY_ATTRIBUTES security{sizeof(security),nullptr,TRUE};
    HANDLE r=nullptr,w=nullptr;
    if(!CreatePipe(&r,&w,&security,65536))return 100;
    Handle read(r),write(w),cancel(CreateEventW(&security,TRUE,FALSE,nullptr));
    if(!cancel || !SetHandleInformation(read.h,HANDLE_FLAG_INHERIT,0))return 100;
    Handle nullInput(CreateFileW(L"NUL",GENERIC_READ|GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,&security,OPEN_EXISTING,0,nullptr));
    if(!nullInput)return 100;
    SIZE_T bytes=0;InitializeProcThreadAttributeList(nullptr,1,0,&bytes);
    std::vector<unsigned char> attributeBytes(bytes);
    auto attributes=reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(attributeBytes.data());
    if(!InitializeProcThreadAttributeList(attributes,1,0,&bytes))return 100;
    HANDLE inherit[]={write.h,cancel.h,nullInput.h};
    bool ready=UpdateProcThreadAttribute(attributes,0,PROC_THREAD_ATTRIBUTE_HANDLE_LIST,inherit,sizeof(inherit),nullptr,nullptr)!=FALSE;
    STARTUPINFOEXW startup{};startup.StartupInfo.cb=sizeof(startup);startup.lpAttributeList=attributes;
    startup.StartupInfo.dwFlags=STARTF_USESTDHANDLES;startup.StartupInfo.hStdOutput=write.h;
    startup.StartupInfo.hStdError=write.h;startup.StartupInfo.hStdInput=nullInput.h;
    auto exe=ThisExe();auto command=Quote(exe.wstring())+L" --halljoy-na87-worker "+Quote(mode)+L" "+
        std::to_wstring(reinterpret_cast<uintptr_t>(cancel.h));
    PROCESS_INFORMATION pi{};
    const bool created=ready && CreateProcessW(exe.c_str(),command.data(),nullptr,nullptr,TRUE,
        CREATE_NO_WINDOW|CREATE_SUSPENDED|EXTENDED_STARTUPINFO_PRESENT,nullptr,nullptr,&startup.StartupInfo,&pi);
    const DWORD creationError=created?0:GetLastError();DeleteProcThreadAttributeList(attributes);
    if(!created){DebugLog_Write(L"[na87] child_start_error=%lu",creationError);return 100;}
    Handle process(pi.hProcess),thread(pi.hThread);
    // Kill-on-close prevents an orphaned probe from retaining a keyboard handle
    // after a parent crash. Assign before sending any runtime commands below.
    Handle job(CreateJobObjectW(nullptr,nullptr));
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limit{};limit.BasicLimitInformation.LimitFlags=JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    if(!job || !SetInformationJobObject(job.h,JobObjectExtendedLimitInformation,&limit,sizeof(limit)) || !AssignProcessToJobObject(job.h,process.h)) {
        TerminateProcess(process.h,100);WaitForSingleObject(process.h,2000);return 100;
    }
    if(ResumeThread(thread.h)==DWORD(-1)){TerminateProcess(process.h,100);WaitForSingleObject(process.h,2000);return 100;}
    CloseHandle(write.h);write.h=INVALID_HANDLE_VALUE;
    std::string pending;auto end=GetTickCount64()+budget;ULONGLONG cancelledAt=0;bool forced=false;
    for(;;){
        DWORD available=0;
        if(PeekNamedPipe(read.h,nullptr,0,nullptr,&available,nullptr) && available){
            char buffer[8192];DWORD got=0;
            if(ReadFile(read.h,buffer,std::min<DWORD>(available,sizeof(buffer)),&got,nullptr))pending.append(buffer,got);
            size_t newline;
            while((newline=pending.find('\n'))!=std::string::npos){ReceiveLine(pending.substr(0,newline));pending.erase(0,newline+1);}
            if(pending.size()>65536){ReceiveLine("{\"kind\":\"oversized_record\"}");pending.clear();}
            // Recheck deadlines even under an uninterrupted input stream.
        }
        else if(WaitForSingleObject(process.h,0)==WAIT_OBJECT_0)break;
        const auto now=GetTickCount64();
        if(!cancelledAt && (now>=end || (parentStop && !allowCancelled))){SetEvent(cancel.h);cancelledAt=now;}
        if(cancelledAt && now-cancelledAt>=1200 && WaitForSingleObject(process.h,0)!=WAIT_OBJECT_0){
            TerminateProcess(process.h,101);forced=true;
            if(WaitForSingleObject(process.h,1000)!=WAIT_OBJECT_0){
                DebugLog_Write(L"[na87] child_unreaped stop_retrying_device=1");return 103;
            }
        }
        Sleep(2);
    }
    if(!pending.empty())ReceiveLine(pending);
    DWORD code=100;GetExitCodeProcess(process.h,&code);
    DebugLog_Write(L"[na87] worker mode=%s exit=%lu forced=%d",mode.c_str(),code,forced?1:0);
    return code;
}
static void Supervise() noexcept {
    try {
        snapshotRoutes.clear();const auto begin=GetTickCount64();
        DWORD capture=RunChild(L"capture",47000);
        bool complete=capture==0;unsigned snapshots=0,failures=0;
        if(capture!=0 && capture!=2 && capture!=3 && capture!=103)RunChild(L"cleanup",2200,true);
        for(const auto& route:snapshotRoutes){
            if(parentStop || capture==103 || GetTickCount64()-begin>=54000)break;
            const DWORD result=RunChild(L"snapshot:"+std::wstring(route.begin(),route.end()),700);
            ++snapshots;failures+=result!=0;
        }
        // An unsuccessful protocol response is still useful evidence, but cannot
        // be labelled as a successful full capture or hardware support.
        while(capture!=2 && !parentStop && GetTickCount64()-begin<60000 &&
            (GetTickCount64()-begin<30000 || !EnoughInput(keyCount,releaseCount,maximumHeld)))Sleep(50);
        collecting=false;
        DebugLog_Write(L"[na87] final capture_exit=%lu snapshot_attempts=%u snapshot_failures=%u snapshot_planned=%zu keys=%u releases=%u cancelled=%d physical_support_proven=0",
            capture,snapshots,failures,snapshotRoutes.size(),keyCount.load(),releaseCount.load(),parentStop?1:0);
        DebugLog_Write(L"[na87] simultaneous_digital_max=%u",maximumHeld.load());
        if(!parentStop)uiState=StabilityTrace_IsEnabled()?(complete && snapshots==snapshotRoutes.size() && EnoughInput(keyCount,releaseCount,maximumHeld)?2:3):4;
    }catch(...){collecting=false;uiState=4;DebugLog_Write(L"[na87] supervisor_exception partial_log_retained=1");}
}
} // namespace
bool Na87Diagnostic_TryRunCommand(int& result) noexcept {
    int argc=0;wchar_t** argv=CommandLineToArgvW(GetCommandLineW(),&argc);
    if(!argv)return false;
    const bool worker=argc==4 && std::wstring_view(argv[1])==L"--halljoy-na87-worker";
    const bool test=argc==2 && std::wstring_view(argv[1])==L"--halljoy-na87-self-test";
    if(!worker && !test){LocalFree(argv);return false;}
    try{
        if(test){
            Require(!EnoughInput(7,20,2) && !EnoughInput(20,7,2) && !EnoughInput(8,8,1) && EnoughInput(8,8,2),"sufficiency threshold");
            Require(Request(Command::Capability,1)==Report{1,0x21,0,0,0,0x18,4},"wire golden");
            auto bad=Request(Command::Configuration,1);bad[7]=6;Require(!Allowed(bad),"configuration bound");
            Metrics m;Report e{1,0x21,0,0,0,3,1,2,2,16};m.Feed(e);e[9]=0;m.Feed(e);
            Require(m.last[46]==0 && m.zero[46]==1,"release accounting");e[9]=255;m.Feed(e);
            Require(m.last[46]==0 && m.invalid==1,"invalid value isolation");
            Require(Quote(L"C:\\folder name\\")==L"\"C:\\folder name\\\\\"","command quoting");
            StabilityTrace_Init();DebugLog_Init();
            parentStop=false;pipeTestSeen=false;Require(RunChild(L"selftest_pipe",1000)==0 && pipeTestSeen,"same-image pipe contents");
            Require(RunChild(L"selftest_hang",100)==101,"bounded hung worker");
            Require(RunChild(L"selftest_cancel",100)==4,"cooperative cancellation");
            const DWORD inventory=RunChild(L"inventory",3000);
            Require(inventory==0 || inventory==2,"metadata enumeration");
            if(inventory==2){
                Na87Diagnostic_Start();const auto deadline=GetTickCount64()+8000;
                while(uiState==1 && GetTickCount64()<deadline)Sleep(10);
                Na87Diagnostic_Stop();Require(uiState==3,"no-device automatic finish");
                DebugLog_Write(L"[na87] no_device_auto_finish=PASS");
            }
            DebugLog_Write(L"[na87] SELF_TEST=PASS pipe_contents=1 hung_child=1 cooperative_cancel=1 wire=1 sufficiency=1");
            Require(DebugLog_Shutdown().RestartSafe(),"log shutdown");StabilityTrace_Shutdown(0);
            result=0;
        }else{
            std::wstring mode=argv[2];wchar_t* end=nullptr;
            auto value=wcstoull(argv[3],&end,10);
            Require(end && !*end && value!=0,"stop handle");
            HANDLE cancel=reinterpret_cast<HANDLE>(static_cast<uintptr_t>(value));DWORD flags=0;
            Require(GetHandleInformation(cancel,&flags)!=FALSE,"valid inherited stop handle");
            std::atomic<bool> done{false};
            std::thread watcher([&]{while(!done){
                if(GetTickCount64()>ioDeadline.load())TerminateProcess(GetCurrentProcess(),102);
                if(WaitForSingleObject(cancel,20)==WAIT_OBJECT_0){stopping=true;break;}
            }});
            try{
                Log log{fs::path()};log.Line("clock","\"origin_qpc_us\":"+std::to_string(Us()));
                if(mode==L"selftest_hang"){Sleep(10000);result=99;}
                else if(mode==L"selftest_pipe"){log.Line("pipe_test","\"payload\":\"complete_record\"");result=0;}
                else if(mode==L"selftest_cancel"){while(!stopping)Sleep(10);log.Line("cancelled_cleanly");result=4;}
                else {
                    auto candidates=Enumerate(log);
                    if(mode==L"inventory")result=candidates.empty()?2:0;
                    else if(mode==L"capture" || mode==L"cleanup")result=Automatic(log,candidates,mode==L"cleanup");
                    else if(mode.starts_with(L"snapshot:")){
                        auto route=Utf8(mode.substr(9));auto first=route.find(':');auto last=route.rfind(':');
                        Require(first!=std::string::npos && first!=last,"snapshot route");
                        Profile p{route.substr(0,first),static_cast<unsigned>(std::stoul(route.substr(first+1,last-first-1))),false,false};
                        result=ReadControl(log,p,candidates,route.substr(last+1)=="f");
                    }else result=7;
                }
            }catch(...){done=true;watcher.join();throw;}
            done=true;watcher.join();
        }
    }catch(...){result=10;}
    LocalFree(argv);return true;
}
void Na87Diagnostic_Start() noexcept {
    if(supervisor.joinable())return;
    if(!StabilityTrace_IsEnabled()){uiState=4;return;}
    parentStop=false;keyCount=0;releaseCount=0;maximumHeld=0;uiState=1;collecting=true;
    DebugLog_Write(L"[na87] automatic_start budget_seconds=60 gamepad_unchanged=1 input_scope=0416:7372 no_text=1 qpc_us=%llu",Us());
    try{supervisor=std::thread(Supervise);}catch(...){collecting=false;uiState=4;}
}
void Na87Diagnostic_Stop() noexcept {
    parentStop=true;collecting=false;
    if(supervisor.joinable())supervisor.join();
}
void Na87Diagnostic_UpdateWindow(HWND window) noexcept {
    static unsigned previous=99;const unsigned state=uiState.load();if(state==previous)return;previous=state;
    const wchar_t* title=state==1?L"HallJoy - NA87: press keys individually and together (up to 60 s)":
        state==2?L"HallJoy - NA87: sufficient data collected. Close and send HallJoy.log":
        state==3?L"HallJoy - NA87: test completed with limitations. Send HallJoy.log":
        state==4?L"HallJoy - NA87: diagnostic error. Check HallJoy.log":L"HallJoy";
    SetWindowTextW(window,title);
}
void Na87Diagnostic_ObserveRawInput(HRAWINPUT input) noexcept {
    if(!collecting)return;
    try{
        RAWINPUT raw{};UINT size=sizeof(raw);
        if(GetRawInputData(input,RID_INPUT,&raw,&size,sizeof(RAWINPUTHEADER))==UINT(-1) ||
            size<sizeof(RAWINPUTHEADER)+sizeof(RAWKEYBOARD) || raw.header.dwType!=RIM_TYPEKEYBOARD)return;
        wchar_t name[1024]{};UINT chars=1024;
        if(GetRawInputDeviceInfoW(raw.header.hDevice,RIDI_DEVICENAME,name,&chars)==UINT(-1))return;
        std::wstring lower=name;std::transform(lower.begin(),lower.end(),lower.begin(),[](wchar_t c){return static_cast<wchar_t>(towlower(c));});
        if(lower.find(L"vid_0416&pid_7372")==std::wstring::npos)return;
        const auto& k=raw.data.keyboard;const unsigned index=(k.MakeCode&255u)|((k.Flags&RI_KEY_E0)?256u:0u)|((k.Flags&RI_KEY_E1)?512u:0u);
        static std::array<bool,1024> seen{},released{},down{};
        const bool release=(k.Flags&RI_KEY_BREAK)!=0;
        if(!release && !seen[index]){seen[index]=true;++keyCount;}
        if(release && down[index] && !released[index]){released[index]=true;++releaseCount;}
        if(down[index]==!release)return; // Typematic repeats carry no new state.
        down[index]=!release;
        const auto held=static_cast<unsigned>(std::count(down.begin(),down.end(),true));
        if(held>maximumHeld)maximumHeld=held;
        DebugLog_Write(L"[na87.digital] qpc_us=%llu scan=%u flags=%u vkey=%u released=%d",Us(),k.MakeCode,k.Flags,k.VKey,release?1:0);
    }catch(...){DebugLog_Write(L"[na87] digital_capture_error");}
}
#endif

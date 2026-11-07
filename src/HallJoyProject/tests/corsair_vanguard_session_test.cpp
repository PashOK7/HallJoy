// Actual Win32 transport with fake I/O; never opens physical devices.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <setupapi.h>
#include <hidsdi.h>
#include <hidpi.h>
#define main CorsairPortableRegression
#include "corsair_vanguard_protocol_test.cpp"
#undef main
#include <cstring>

static FakeTransport device;
static Frame queued{};
static bool isRead=false,timeoutRead=false,cancelled=false,partial=false,wrongReportId=false;
static unsigned readCalls=0,writeCalls=0,cancelCalls=0,drainCalls=0,opens=0,wakes=0;
static bool foreignClaim=false,allowOpen=false,busyOpen=false,runMode=false,badCaps=false;
static unsigned claims=0,lastProperty=0;
static bool failAttributes=false,failPreparsed=false,failCaps=false,failFlush=false;
static bool failDiscovery=false,failDetail=false;
static std::vector<std::wstring> discovered;
static std::vector<std::string> evidence;
static bool HasEvidence(const char* text) {
    for(const auto& line:evidence)if(line.find(text)!=std::string::npos)return true;
    return false;
}
static HDEVINFO FakeClassDevs(const GUID*,PCWSTR,HWND,DWORD) {
    if(failDiscovery){SetLastError(ERROR_ACCESS_DENIED);return INVALID_HANDLE_VALUE;}
    return reinterpret_cast<HDEVINFO>(1);
}
static BOOL FakeEnum(HDEVINFO,PSP_DEVINFO_DATA,const GUID*,DWORD index,PSP_DEVICE_INTERFACE_DATA data) {
    if(index>=discovered.size()){SetLastError(ERROR_NO_MORE_ITEMS);return FALSE;}
    data->Reserved=index;return TRUE;
}
static BOOL FakeDetail(HDEVINFO,PSP_DEVICE_INTERFACE_DATA data,PSP_DEVICE_INTERFACE_DETAIL_DATA_W output,DWORD,PDWORD required,PSP_DEVINFO_DATA) {
    const auto& path=discovered[data->Reserved];
    if(required)*required=static_cast<DWORD>(offsetof(SP_DEVICE_INTERFACE_DETAIL_DATA_W,DevicePath)+(path.size()+1)*sizeof(wchar_t));
    if(!output){SetLastError(ERROR_INSUFFICIENT_BUFFER);return FALSE;}
    if(failDetail){SetLastError(ERROR_ACCESS_DENIED);return FALSE;}
    wcscpy(output->DevicePath,path.c_str());return TRUE;
}
static BOOL FakeDestroy(HDEVINFO){return TRUE;}
static HANDLE FakeCreate(LPCWSTR,DWORD,DWORD,LPSECURITY_ATTRIBUTES,DWORD,DWORD,HANDLE) {
    ++opens;assert(allowOpen && "unexpected discovery/open path");
    if(busyOpen){SetLastError(ERROR_SHARING_VIOLATION);return INVALID_HANDLE_VALUE;}
    return CreateEventW(nullptr,TRUE,FALSE,nullptr);
}
static BOOL FakeWrite(HANDLE,LPCVOID buffer,DWORD bytes,LPDWORD,LPOVERLAPPED operation) {
    ++writeCalls;assert(bytes==kReportBytes);const auto* wire=static_cast<const unsigned char*>(buffer);assert(!wire[0]);
    Frame request{};std::copy(wire+1,wire+bytes,request.begin());
    lastProperty=request[3]==2?U16(request.data()+4):0;
    if(runMode && lastProperty==0xc7)device.snapshotPhase=device.reads==1?1:0;
    const auto kind=request[3]==2 && U16(request.data()+4)==0xc6?ReplyKind::Calibration:ReplyKind::Normal;
    assert(device.Exchange(request,queued,kind,request[3]==27 && request[4]==2));
    isRead=false;operation->InternalHigh=bytes;return TRUE;
}
static void CheckLayoutForTest();
static BOOL FakeRead(HANDLE,LPVOID buffer,DWORD bytes,LPDWORD,LPOVERLAPPED operation) {
    ++readCalls;assert(bytes==kReportBytes);isRead=true;operation->InternalHigh=bytes;
    if(timeoutRead){SetLastError(ERROR_IO_PENDING);return FALSE;}
    auto* wire=static_cast<unsigned char*>(buffer);wire[0]=wrongReportId?1:0;
    std::copy(queued.begin(),queued.end(),wire+1);
    if(runMode && lastProperty==0xc7)CheckLayoutForTest();
    return TRUE;
}
static BOOL FakeResult(HANDLE,LPOVERLAPPED operation,LPDWORD bytes,BOOL wait) {
    if(cancelled && isRead) {assert(wait);++drainCalls;*bytes=0;SetLastError(ERROR_OPERATION_ABORTED);return FALSE;}
    *bytes=static_cast<DWORD>(operation->InternalHigh)-(partial && isRead?1:0);return TRUE;
}
static BOOL FakeCancel(HANDLE,LPOVERLAPPED){cancelled=true;++cancelCalls;return TRUE;}
static DWORD FakeWait(DWORD,const HANDLE*,BOOL,DWORD){assert(timeoutRead);return WAIT_TIMEOUT;}
static BOOLEAN FakeAttributes(HANDLE,HIDD_ATTRIBUTES* a){if(failAttributes){SetLastError(ERROR_ACCESS_DENIED);return FALSE;}a->VendorID=0x1b1c;a->ProductID=0x2b0e;return TRUE;}
static BOOLEAN FakePreparsed(HANDLE,PHIDP_PREPARSED_DATA* p){if(failPreparsed){SetLastError(ERROR_ACCESS_DENIED);return FALSE;}*p=reinterpret_cast<PHIDP_PREPARSED_DATA>(1);return TRUE;}
static BOOLEAN FakeFreePreparsed(PHIDP_PREPARSED_DATA){return TRUE;}
static NTSTATUS FakeCaps(PHIDP_PREPARSED_DATA,HIDP_CAPS* c){if(failCaps)return HIDP_STATUS_INVALID_PREPARSED_DATA;*c={};c->InputReportByteLength=badCaps?64:1025;c->OutputReportByteLength=1025;return HIDP_STATUS_SUCCESS;}
static BOOLEAN FakeFlush(HANDLE){if(failFlush){SetLastError(ERROR_GEN_FAILURE);return FALSE;}return TRUE;}
#define SetupDiGetClassDevsW FakeClassDevs
#define SetupDiEnumDeviceInterfaces FakeEnum
#define SetupDiGetDeviceInterfaceDetailW FakeDetail
#define SetupDiDestroyDeviceInfoList FakeDestroy
#define HidD_GetAttributes FakeAttributes
#define HidD_GetPreparsedData FakePreparsed
#define HidD_FreePreparsedData FakeFreePreparsed
#define HidP_GetCaps FakeCaps
#define HidD_FlushQueue FakeFlush
#define CreateFileW FakeCreate
#define WriteFile FakeWrite
#define ReadFile FakeRead
#define GetOverlappedResult FakeResult
#define CancelIoEx FakeCancel
#define WaitForMultipleObjects FakeWait
#include "../HallJoy/corsair_vanguard_backend.cpp"
#undef SetupDiGetClassDevsW
#undef SetupDiEnumDeviceInterfaces
#undef SetupDiGetDeviceInterfaceDetailW
#undef SetupDiDestroyDeviceInfoList
#undef HidD_GetAttributes
#undef HidD_GetPreparsedData
#undef HidD_FreePreparsedData
#undef HidP_GetCaps
#undef HidD_FlushQueue
#undef CreateFileW
#undef WriteFile
#undef ReadFile
#undef GetOverlappedResult
#undef CancelIoEx
#undef WaitForMultipleObjects

static std::uint64_t observedLayout=0;
static void CheckLayoutForTest() {
    NativeAnalogBackendTelemetry status{};
    Telemetry(&status);
    if(device.reads==1)assert(!status.connected && status.verifiedLayoutToken==0);
    if(device.reads==3) {
        assert(status.connected);
        observedLayout=status.verifiedLayoutToken;
        g_stop.store(true);
    }
}
void SupportLog_Event(const char*,std::uint64_t,SupportLogDetail) noexcept {}
void SupportLog_Evidence(const char* record,bool keep) noexcept {assert(!keep && strlen(record)<330);evidence.emplace_back(record);}
void RealtimeLoop_NotifyInputChanged(){++wakes;}
void RealtimeLoop_NotifyInputChangedAt(LONGLONG){++wakes;}
bool NativeAnalogRouting_Claim(std::uint16_t vid,std::uint16_t pid,const wchar_t* path,NativeAnalogProtocol protocol){
    assert(vid==0x1b1c && pid==0x2b0e && path && protocol==NativeAnalogProtocol::CorsairVanguard);
    assert(device.reads>0);++claims;return !foreignClaim;
}
bool NativeAnalogRouting_IsClaimed(const wchar_t*){return foreignClaim;}
bool NativeAnalogRouting_IsClaimedBy(const wchar_t*,NativeAnalogProtocol){return false;}
bool NativeAnalogRouting_ProtocolHasClaims(NativeAnalogProtocol){return false;}
void NativeAnalogRouting_Reset(){}

int main() {
    g_wake=CreateEventW(nullptr,FALSE,FALSE,nullptr);
    const auto handle=CreateEventW(nullptr,TRUE,FALSE,nullptr);
    {
        Transport io(handle,g_topology.load());Client<Transport> client(io);
        assert(client.Initialize(0x12345678));
        std::array<std::uint16_t,100> values{};assert(client.Read(values));
        device.snapshotPhase=1;assert(client.Read(values));unsigned count=0;for(auto v:values)count+=v>0;assert(count==2);
        device.snapshotPhase=0;assert(client.Read(values));for(auto v:values)assert(!v);
        assert(client.Close() && device.closes==2);
    }
    // Wrong report ID and short reads are rejected before payload parsing.
    for(unsigned scenario=0;scenario<3;++scenario) {
        device={};cancelled=false;partial=scenario==0;wrongReportId=scenario==1;timeoutRead=scenario==2;
        Transport io(handle,g_topology.load());Client<Transport> client(io);
        assert(!client.Initialize(9));
        if(timeoutRead){assert(cancelCalls==1 && drainCalls==1 && io.error==WAIT_TIMEOUT);}
    }
    timeoutRead=partial=wrongReportId=cancelled=false;
    // Discovery uses metadata only, tolerates case and exposes every exclusion.
    discovered={L"hid#VID_1B1C&PID_2B0E&MI_00#private",L"hid#vid_1b1c&pid_2b0e&mi_02#private",L"hid#vid_1b1c&pid_1ba6&mi_02#receiver"};
    Discovery scan{};auto paths=Enumerate(scan);assert(paths.size()==1 && scan.products==2 && scan.controls==1 && opens==0);
    assert(HasEvidence("product_collections=2 control_if2_collections=1"));
    failDiscovery=true;scan={};assert(Enumerate(scan).empty() && scan.errors==1 && scan.firstError==ERROR_ACCESS_DENIED);failDiscovery=false;
    failDetail=true;scan={};assert(Enumerate(scan).empty() && scan.errors==3);failDetail=false;
    // Foreign exact-path claims are checked before any CreateFile/metadata open.
    foreignClaim=true;assert(Run(L"fake Corsair path"));assert(opens==0);foreignClaim=false;
    const auto* layout=FindLayout(1);assert(layout);
    g_layout.store(layout);assert(g_values.Bind(1,4));g_values.Publish(1,500,GetTickCount64());g_connected.store(true);
    assert(Get(4)==500);Clear();assert(!Connected() && Get(4)==0 && wakes>0);
    // Execute the real production session, including admission, ownership,
    // publication and neutralization, with no physical HID calls.
    allowOpen=runMode=true;
    const unsigned regions[]={1,2,4};
    const wchar_t* presets[]={L"Corsair VANGUARD PRO 96 ANSI", L"Corsair VANGUARD PRO 96 ISO",
                              L"Corsair VANGUARD PRO 96 JIS"};
    for(unsigned i=0;i<3;++i) {
        device={};
        device.layout=regions[i];
        claims=0;
        observedLayout=0;
        g_stop.store(false);
        const auto wakeBefore=wakes;
        assert(!Run(L"fake exact interface"));
        assert(claims==1 && device.reads==3 && device.closes==2 && !Connected());
        assert(wakes>wakeBefore && Get(4)==0);
        const auto* preset=halljoy::layout_identity::Match(observedLayout);
        assert(preset && wcscmp(preset,presets[i])==0);
        NativeAnalogBackendTelemetry status{};
        Telemetry(&status);
        assert(status.verifiedLayoutToken==0);
    }
    assert(VanguardLayoutToken(3)==0);
    assert(HasEvidence("frames=3 raw_changed_slots=4 positive_samples=2 value_changes=4 release_transitions=2 max_simultaneous=2"));
    runMode=false;g_stop.store(false);claims=0;device={};device.badSnapshot=true;
    assert(Run(L"fake invalid analog"));assert(claims==0 && !Connected());
    device={};badCaps=true;const auto beforeWrites=writeCalls;
    assert(Run(L"fake wrong caps"));assert(writeCalls==beforeWrites && claims==0);badCaps=false;
    busyOpen=true;assert(!Run(L"fake busy"));assert(g_busy.load() && writeCalls==beforeWrites);busyOpen=false;
    // Metadata/queue errors stay retryable; none may issue a Bragi command.
    for(unsigned scenario=0;scenario<4;++scenario) {
        device={};const auto writes=writeCalls;
        failAttributes=scenario==0;failPreparsed=scenario==1;failCaps=scenario==2;failFlush=scenario==3;
        assert(!Run(L"fake transient metadata") && writeCalls==writes);
        failAttributes=failPreparsed=failCaps=failFlush=false;
    }
    assert(HasEvidence("corsair.preparsed_failed") && HasEvidence("corsair.caps_query_failed"));
    // Inspect the actual production-formatted failure, not merely its counter.
    evidence.clear();device={};device.wrongFirmware=true;
    assert(Run(L"fake firmware refusal"));
    assert(HasEvidence("stage=firmware reason=firmware_not_reviewed command=2 property=19"));
    assert(HasEvidence("expected_firmware=2.8.171"));
    evidence.clear();device={};device.badCalTail=true;
    assert(Run(L"fake strict calibration refusal"));
    assert(HasEvidence("stage=calibration reason=calibration_tail command=2 property=198 subcommand=6"));
    evidence.clear();device={};device.badSnapshot=true;
    assert(Run(L"fake zero snapshot refusal"));
    assert(HasEvidence("stage=sampling reason=snapshot_zero_sensor command=2 property=199"));
    for(unsigned scenario=0;scenario<3;++scenario) {
        evidence.clear();device={};cancelled=false;
        partial=scenario==0;wrongReportId=scenario==1;timeoutRead=scenario==2;
        assert(Run(L"fake transport refusal"));
        assert(HasEvidence("stage=session reason=transport command=27"));
        if(partial)assert(HasEvidence("expected_bytes=1025 actual_bytes=1024"));
        if(wrongReportId)assert(HasEvidence("report_id=1"));
        if(timeoutRead)assert(HasEvidence("win32=258"));
    }
    partial=wrongReportId=timeoutRead=false;
    for(const auto& line:evidence){assert(line.find("private")==std::string::npos && line.size()<768);}
    CloseHandle(handle);CloseHandle(g_wake);g_wake=nullptr;
    std::cout<<"CORSAIR_WINDOWS_TRANSPORT=PASS framing, sessions, cancellation, metadata retry, discovery and precise retained rejection evidence\n";
}

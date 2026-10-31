
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <setupapi.h>
#include <hidsdi.h>
#include <hidpi.h>
#include <algorithm>
#include <array>
#include <string>
#include <vector>
#include <cwctype>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <cstdint>
#include <stdexcept>
#include "attackshark_pro_diagnostic_model.h"
using namespace halljoy::sharkdiag;
namespace {
struct Row { USHORT page,usage,feature,pid=0x502d; bool receiver=false; };
std::vector<Row> rows;
unsigned active=0,passes=0,opens=0,busyFor=0,metadataError=32,listFailures=0;
ULONGLONG topologyAt=0;
ULONGLONG tick=0,lastOutput=0,limit=12000;
bool cancelled=false,watchdogEnabled=false,watchdogFired=false,reboundBusy=false;
std::vector<std::string> records;
struct Shared {volatile LONG topology=0;};Shared storage;Shared* shared=&storage;
LONG Load(volatile LONG& value){return InterlockedCompareExchange(&value,0,0);}
bool Stop(){return cancelled;}
void Line(const std::string& value){records.push_back(value);lastOutput=tick;}
void Event(const char* name,unsigned error){Line(std::string(name)+" error="+std::to_string(error));}
void TestSleep(DWORD ms){tick+=ms;if(topologyAt && tick>=topologyAt){++storage.topology;topologyAt=0;}if(tick>=limit)cancelled=true;if(watchdogEnabled && tick-lastOutput>8000){watchdogFired=true;cancelled=true;}}
ULONGLONG TestTick(){return tick;}
HDEVINFO FakeList(const GUID*,PCWSTR,HWND,DWORD){++passes;if(passes<=listFailures){SetLastError(ERROR_GEN_FAILURE);return INVALID_HANDLE_VALUE;}return reinterpret_cast<HDEVINFO>(1);}
BOOL FakeEnum(HDEVINFO,PSP_DEVINFO_DATA,const GUID*,DWORD index,PSP_DEVICE_INTERFACE_DATA){active=index;if(index>=rows.size()){SetLastError(ERROR_NO_MORE_ITEMS);return FALSE;}return TRUE;}
BOOL FakeDetail(HDEVINFO,PSP_DEVICE_INTERFACE_DATA,PSP_DEVICE_INTERFACE_DETAIL_DATA_W detail,DWORD,PDWORD size,PSP_DEVINFO_DATA){
 if(size)*size=sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W)+512;
 if(!detail){SetLastError(ERROR_INSUFFICIENT_BUFFER);return FALSE;}
 swprintf_s(detail->DevicePath,240,L"hid#vid_3151&pid_%04x&mi_01&col07#PRIVATE_PATH_row%u",rows[active].pid,active);return TRUE;
}
BOOL FakeDestroy(HDEVINFO){return TRUE;}
HANDLE FakeOpen(LPCWSTR,DWORD access,DWORD share,LPSECURITY_ATTRIBUTES,DWORD creation,DWORD,HANDLE){
 ++opens;if(access!=0 || share!=(FILE_SHARE_READ|FILE_SHARE_WRITE) || creation!=OPEN_EXISTING)throw std::runtime_error("unexpected metadata access");
 if(rows[active].feature && (passes<=busyFor || (reboundBusy && passes==3))){SetLastError(metadataError);return INVALID_HANDLE_VALUE;}
 return reinterpret_cast<HANDLE>(static_cast<uintptr_t>(active+16));
}
BOOL FakeClose(HANDLE){return TRUE;}
BOOL FakeAttributes(HANDLE,HIDD_ATTRIBUTES* attributes){attributes->VendorID=0x3151;attributes->ProductID=rows[active].pid;attributes->VersionNumber=0x0505;return TRUE;}
BOOLEAN FakePreparsed(HANDLE,PHIDP_PREPARSED_DATA* data){*data=reinterpret_cast<PHIDP_PREPARSED_DATA>(static_cast<uintptr_t>(active+16));return TRUE;}
NTSTATUS FakeCaps(PHIDP_PREPARSED_DATA,PHIDP_CAPS caps){*caps={};caps->UsagePage=rows[active].page;caps->Usage=rows[active].usage;caps->FeatureReportByteLength=rows[active].feature;return HIDP_STATUS_SUCCESS;}
BOOLEAN FakeFree(PHIDP_PREPARSED_DATA){return TRUE;}
BOOLEAN FakeProduct(HANDLE,PVOID out,ULONG bytes){const wchar_t* name=rows[active].receiver?L"R82HE Receiver":L"R82HE";wcscpy_s(static_cast<wchar_t*>(out),bytes/sizeof(wchar_t),name);return TRUE;}
void FakeGuid(LPGUID guid){*guid={};}
#define SetupDiGetClassDevsW FakeList
#define SetupDiEnumDeviceInterfaces FakeEnum
#define SetupDiGetDeviceInterfaceDetailW FakeDetail
#define SetupDiDestroyDeviceInfoList FakeDestroy
#define CreateFileW FakeOpen
#define CloseHandle FakeClose
#define HidD_GetAttributes FakeAttributes
#define HidD_GetPreparsedData FakePreparsed
#define HidP_GetCaps FakeCaps
#define HidD_FreePreparsedData FakeFree
#define HidD_GetProductString FakeProduct
#define HidD_GetHidGuid FakeGuid
#define Sleep TestSleep
#define GetTickCount64 TestTick
struct Handle {HANDLE h=INVALID_HANDLE_VALUE;explicit Handle(HANDLE x=INVALID_HANDLE_VALUE):h(x){}~Handle(){if(h && h!=INVALID_HANDLE_VALUE)FakeClose(h);}explicit operator bool()const{return h && h!=INVALID_HANDLE_VALUE;}};
bool DiagnosticPid(unsigned pid){return CandidatePid(pid);}
// Include generated excerpts of Find and Capture, not a second implementation.
#include "shark_discovery.inc"
void Check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
bool Has(const char* fragment){for(const auto& line:records)if(line.find(fragment)!=std::string::npos)return true;return false;}
void Reset(){rows={{1,6,0},{0xffff,2,65}};active=passes=opens=busyFor=listFailures=0;metadataError=32;tick=lastOutput=0;limit=12000;topologyAt=0;storage.topology=0;cancelled=watchdogFired=reboundBusy=false;watchdogEnabled=true;records.clear();}
void Done(const char* name,int expected,unsigned expectedPasses){
 const int result=Capture();
 Check(result==expected,"unexpected discovery result");Check(passes==expectedPasses,"unexpected scan count");Check(!watchdogFired,"parent watchdog would kill worker");
 for(const auto& line:records)Check(line.find("PRIVATE_PATH")==std::string::npos,"device path escaped to log");
 std::cout<<name<<" PASS scans="<<passes<<" elapsed_ms="<<tick<<"\n";
}
} // namespace
int main(){try{
 Reset();Done("immediate eligible",0,1);Check(Has("interface=1 collection=7 stage=6"),"missing numeric collection proof");
 for(unsigned error:{32u,5u,2u}){Reset();busyFor=1;metadataError=error;Done("transient metadata failure",0,2);Check(tick==1000,"retry interval");Check(Has("phase=2"),"missing recovery evidence");}
 Reset();busyFor=1;reboundBusy=true;Done("no logging rescan race",0,2);
 Reset();busyFor=100;Done("persistent busy heartbeat",4,12);Check(Has("phase=3 reason=1"),"missing cancellation evidence");Check(records.size()<70,"unchanged inventory log flood");
 Reset();busyFor=12;limit=20000;Done("busy longer than watchdog then recovery",0,13);Check(Has("elapsed_ms=12000"),"missing busy duration");
 Reset();listFailures=1;Done("class-list failure recovery",0,2);
 Reset();rows={{1,6,0}};Done("missing control collection",4,1);Check(Has("phase=0 reason=3"),"missing absent-interface evidence");
 Reset();rows[1].usage=3;Done("wrong usage rejected",4,1);Check(Has("rejection=4"),"missing usage rejection");
 Reset();rows[1].feature=64;Done("wrong feature length rejected",4,1);Check(Has("rejection=5"),"missing feature rejection");
 Reset();rows[1].receiver=true;Done("receiver excluded",4,1);Check(Has("rejection=1"),"missing receiver rejection");
 Reset();rows[1].pid=0x1234;Done("foreign PID excluded",4,1);Check(Has("rejection=2"),"missing PID rejection");
 Reset();rows.push_back({0xffff,2,0});Done("eligible result retained alongside rejected collection",0,1);
 Reset();rows={{1,6,0}};topologyAt=500;Done("topology change wakes empty wait",4,2);
 Reset();busyFor=100;limit=300;Done("busy cancellation bounded",4,1);Check(tick==300,"cancel latency");
 Check(Known(2844,0x502d) && !Known(2844,0x5030) && !Known(9999,0x502d),"identity gate weakened");
 std::cout<<"SHARK_DISCOVERY=PASS actual_production_excerpt=1 hardware_access=0\n";return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}}

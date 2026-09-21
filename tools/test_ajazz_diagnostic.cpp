#include <algorithm>
#include <array>
#include <atomic>
#include <cassert>
#include <cstdarg>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>
#include "../src/HallJoyProject/HallJoy/irok_nd75_protocol.h"
using DWORD=unsigned long;
constexpr DWORD WAIT_TIMEOUT=258, ERROR_TIMEOUT=1460, kProofTimeoutMs=1200;
std::atomic<bool> g_stop{false};
std::atomic<unsigned> g_diagnosticStatus{0};
unsigned tick=0,scenario=0,stage=0,eventIndex=0,reads=0;
DWORD lastError=0;
std::vector<irok_nd75::Report> sent;
std::vector<std::wstring> logEvents;
unsigned long long GetTickCount64(){return tick+=100;}
DWORD GetLastError(){return lastError;}
void StabilityTrace_Write(const wchar_t*,const wchar_t*,const wchar_t* event,const wchar_t*,...){logEvents.emplace_back(event);}
struct Candidate{};
struct Session {
    Session(const Candidate&,bool,bool){}
    bool Open(){return scenario!=1;}
    bool Send(const irok_nd75::Report& r){
        sent.push_back(r);stage=r[1]==0x0d?1:r[6]==2?2:3;
        return !(scenario==2 || (scenario==5 && r[6]==2) || (scenario==14 && r[1]==0x23 && r[6]==1));
    }
    bool Read(irok_nd75::Report* r,unsigned){
        r->fill(0);
        if(scenario==3){lastError=WAIT_TIMEOUT;return false;}
        if(stage==1){
            const char* id=scenario==4?"M484,01,KB,SG,OTHER,V1.0":"M484,01,KB,SG,SG8994HERGB,V1.13.17";
            (*r)[0]=1;(*r)[1]=0x0d;(*r)[5]=static_cast<unsigned char>(strlen(id)+5);
            memcpy(r->data()+6,id,strlen(id));return true;
        }
        if(scenario==6){lastError=1167;return false;}
        if(scenario==8){if(++reads==1000)g_stop=true;lastError=WAIT_TIMEOUT;return false;}
        if((scenario==12 || scenario==13) && eventIndex<12) {
            (*r)[0]=1;(*r)[1]=0x23;(*r)[4]=static_cast<unsigned char>(eventIndex%6+1);(*r)[5]=44;
            for(unsigned c=0;c<22;++c) {(*r)[6+2*c]=12;(*r)[7+2*c]=static_cast<unsigned char>(c+eventIndex);}
            if(scenario==13)(*r)[4]=7;
            ++eventIndex;return true;
        }
        if(eventIndex==(scenario==10?5000u:12u)){g_stop=true;lastError=WAIT_TIMEOUT;return false;}
        (*r)[0]=1;(*r)[1]=0x21;(*r)[5]=3;(*r)[6]=1;
        (*r)[7]=0;(*r)[8]=eventIndex%4;(*r)[9]=eventIndex<8?static_cast<unsigned char>(10+eventIndex):0;
        if(scenario==9 && eventIndex<4)(*r)[7]=6; // Invalid coordinates must be rejected.
        ++eventIndex;return true;
    }
};
bool ReceiveCapability(Session&,irok_nd75::CapabilityInfo* c){c->sensitivity=40;return scenario!=7;}
unsigned publications=0; bool playing=false;
bool BeginAjazzPlay(Session&) {playing=scenario!=11;return playing;}
void PublishAjazzPlay(const irok_nd75::LiveEvent&) {assert(playing);++publications;}
void EndAjazzPlay() {playing=false;}
void ObserveAjazzDepth(const irok_nd75::LiveEvent&) {}
void PublishAjazzRawRow(const irok_nd75::Report&) {}
unsigned AjazzReadyKeys() {return 0;}
void LogAjazzLearning() {}
#include "../src/HallJoyProject/HallJoy/ajazz_diagnostic.inl"
int main(){
    for(scenario=0;scenario<15;++scenario){
        tick=stage=eventIndex=reads=publications=0;g_stop=false;sent.clear();logEvents.clear();
        bool result=RunAjazzDiagnostic(Candidate{});
        assert(result==(scenario==0 || scenario==6 || (scenario>=8 && scenario!=11)));
        for(const auto& r:sent) assert(r==irok_nd75::BuildIdentityRequest() || r[6]==2 || r[6]==3 || r==AjazzRawRequest(true) || r==AjazzRawRequest(false));
        assert(!playing);
        if(result) assert(sent.back()==AjazzRawRequest(false));
        if(result || scenario==5) assert(std::find(sent.begin(),sent.end(),irok_nd75::BuildUnsubscribeRequest())!=sent.end());
        if(scenario==4 || scenario==7 || scenario==11) assert(sent.size()==1);
        if(scenario==0){assert(eventIndex==12);assert(publications==0);assert(std::count(logEvents.begin(),logEvents.end(),L"position.summary")==4);}
        if(scenario==8){assert(tick>60000);assert(std::count(logEvents.begin(),logEvents.end(),L"stream.progress")>10);}
        if(scenario==9)assert(std::count(logEvents.begin(),logEvents.end(),L"depth")==8);
        if(scenario==10){assert(eventIndex==5000);assert(std::count(logEvents.begin(),logEvents.end(),L"depth")==4096);assert(std::count(logEvents.begin(),logEvents.end(),L"position.summary")==4);}
        if(scenario==6) assert(std::count(logEvents.begin(),logEvents.end(),L"read.failed")==3);
    }
    AjazzRawDiagnostic raw;
    std::array<std::uint8_t,132> depths{};std::array<unsigned,132> counts{};
    depths[0]=20;counts[0]=1;
    for(unsigned sweep=0;sweep<2;++sweep) for(unsigned row=1;row<=6;++row) {
        irok_nd75::Report r{};r[0]=1;r[1]=0x23;r[4]=static_cast<unsigned char>(row);r[5]=44;
        for(unsigned c=0;c<22;++c) {r[6+2*c]=0x12;r[7+2*c]=static_cast<unsigned char>(c+sweep);}
        assert(raw.Accept(r,depths,counts));
    }
    assert(raw.mask==63 && raw.packets==12 && raw.Changed()==132);
    assert(raw.last[0]==0x1201 && raw.positions[0].minimum==0x1200);
    assert(raw.depthBins[0][20].count==2 && raw.depthBins[1][0].count==0);
    irok_nd75::Report bad{};bad[0]=1;bad[1]=0x23;bad[4]=7;bad[5]=44;
    assert(!raw.Accept(bad,depths,counts) && raw.invalid==1 && raw.packets==12);
    bad[4]=1;bad[5]=43;assert(!raw.Accept(bad,depths,counts));
    std::cout<<"AJAZZ_DIAGNOSTIC_TEST=PASS scenarios=15 raw_rows=PASS hardware_opened=0\n";
}

#include "keyboard_support_status.h"
#include "support_notice_catalog.h"

#include <cassert>

int main()
{
    using namespace halljoy::keyboard_support;
    // No warning for startup, unsupported devices, idle input or one unplug.
    CommunicationHealth health;
    assert(!health.Observe(100,true,false));
    assert(!health.Observe(10000,true,false));
    assert(!health.Observe(11000,true,true));
    assert(!health.Observe(12000,false,false));
    assert(!health.Observe(14000,false,false));
    assert(!health.Observe(15000,true,true));
    assert(health.Observe(16000,false,false)); // repeated drop in 30 seconds
    assert(health.Observe(17000,true,true));
    assert(health.Observe(31999,true,true));
    assert(!health.Observe(32000,true,true)); // 15 seconds healthy
    health={};health.Observe(100,true,true);
    assert(!health.Observe(200,true,false));
    assert(!health.Observe(1699,true,false));
    assert(health.Observe(1700,true,false)); // still present, stream lost
    health={};assert(health.Observe(100,true,true,true));
    assert(health.Observe(200,true,true));
    assert(!health.Observe(15200,true,true));
    assert(!health.Observe(16000,true,true)); // zero activity isn't a fault
    const auto sequence=CommunicationAnomalySequence(17);
    ReportCommunicationAnomaly(17);
    assert(CommunicationAnomalySequence(17)==sequence+1);
    ReportCommunicationAnomaly(256);assert(CommunicationAnomalySequence(256)==0);
    const auto initial=CommunicationAnomalySequence(22);
    assert(!ReportCommunicationAccessFailure(22,5));
    assert(!ReportCommunicationAccessFailure(22,1167));
    assert(CommunicationAnomalySequence(22)==initial);
    assert(ReportCommunicationAccessFailure(22,32));
    assert(CommunicationAnomalySequence(22)==initial+1);
    health={};assert(health.Observe(100,true,false,true)); // busy before first sample
    assert(health.Observe(20000,true,false));
    assert(health.Observe(20001,true,true));
    assert(!health.Observe(35001,true,true));
    health={};assert(health.Observe(100,true,false,true));
    assert(health.Observe(101,false,false));
    assert(!health.Observe(30102,false,false)); // no forever-latched startup fault
    SetSearchObservation(true,true,AttackShark,true);
    assert(GetStatusSnapshot().communicationWarning && GetStatusSnapshot().frozenModels==AttackShark);
    SetSearchObservation(false,true,AttackShark,true);
    assert(!GetStatusSnapshot().communicationWarning);

    SetSearchObservation(false, false);
    assert(!GetStatusSnapshot().searchCompleted);
    assert(!GetStatusSnapshot().analogSourceConnected);

    // A source observation before startup completion is deliberately not a
    // user-facing connection state.
    SetSearchObservation(false, true);
    assert(!GetStatusSnapshot().searchCompleted);
    assert(!GetStatusSnapshot().analogSourceConnected);

    SetSearchObservation(true, true);
    assert(GetStatusSnapshot().searchCompleted);
    assert(GetStatusSnapshot().analogSourceConnected);

    SetSearchObservation(true, false);
    assert(GetStatusSnapshot().searchCompleted);
    assert(!GetStatusSnapshot().analogSourceConnected);

    SetSearchObservation(false, false);
    assert(!GetStatusSnapshot().searchCompleted);
    SetSearchObservation(true, true, NA87);
    assert(GetStatusSnapshot().frozenModels==NA87);
    assert(GetStatusSnapshot().analogSourceConnected);
    SetSearchObservation(true, false, NA87Pro | ND75);
    assert(GetStatusSnapshot().frozenModels==(NA87Pro | ND75));
    SetSearchObservation(false, true, NA87);
    assert(GetStatusSnapshot().frozenModels==0);
    assert(ClassifyFrozen(0x0416,0x7372,L"GK8260HERGB")==0);
    assert(ClassifyFrozen(0x0416,0x7372,L"MU68")==FamilyCandidate);
    assert(ClassifyFrozen(0x0416,0x7372,L"ND75")==ND75);
    assert(ClassifyFrozen(0x1c4f,0xee88,L"NA87 PRO")==NA87Pro);
    assert(ClassifyFrozen(0x372e,0x103e,L"HERO84 HE")==Hero84);
    assert(ClassifyFrozen(0x372e,0x103e,L"AURORA65")==FamilyCandidate);
    assert(ClassifyFrozen(0x0b05,0x1c10,L"")==Azoth96);
    assert(ClassifyFrozen(0x3151,0x502d,L"X68HE")==FamilyCandidate);
    assert(ClassifyFrozen(0x3151,0x502f,L"X68HE")==FamilyCandidate);
    assert(ClassifyFrozen(0x3434,0x0e40,L"NA87 PRO")==0);
    assert(ClassifyFrozen(0x1ca5,0x0807,L"IROK MG75 PRO")==Mg75Pro);
    assert(ClassifyFrozen(0x1ca5,0x0807,L"IROK MG75")==0);
    SetSearchObservation(true,true);
    assert(GetStatusSnapshot().frozenModels==0);
    for(unsigned mask=0;mask<16384;++mask){
        SetSearchObservation(true,true,mask);assert(GetStatusSnapshot().frozenModels==mask);
        SetSearchObservation(false,true,mask);assert(GetStatusSnapshot().frozenModels==0);
    }
    // MCHOSE Ace 68 Air III (41E4:2132) is Supported; other ARM Ace 68 boards keep the notice.
    assert(NativeNotice(27,0xC19BC9D40FAC0AA4ull,true,0x2132)==0);
    assert(NativeNotice(27,0xC19BC9D40FAC0AA4ull,true,0x300a)==MchoseFamily);
    // WLMOUSE Ying75 (36A7:F887) is Supported: no notice; other JingTai boards keep it.
    assert(NativeNotice(18,0xE6D996D6FD1177ECull,true,0xF887)==0 && NativeNotice(18,0,true,0x0807)==Mg75Pro);
    assert(ClassifyFrozen(0x36a7,0xf887,L"WLKB YING 75")==0);
    assert(NativeNotice(30,0,true,0xC364)==LogitechRapid && NativeNotice(30,0,false,0xC364)==0); // Logitech G RAPID: yellow
    // Red Square Alumix 68 (0C45:80A2): yellow when the verified firmware streams.
    assert(NativeNotice(31,0,true,0x80a2)==RedSquareAlumix68 && NativeNotice(31,0,false,0x80a2)==0);
    assert(NativeNotice(22,0x5F95BAB8BCAB36D1ull,true,0x5030)==0);
    assert(NativeNotice(22,0x5F95BAB8BCAB36D1ull,true,0x5029)!=0);
    assert(NativeNotice(22,0,true,0x5030)!=0);
    assert(NativeNotice(17,0x5348583635414E53ull,true)==0);
    assert(NativeNotice(17,0x5348583638414E53ull,true)==AttackShark);
    assert(NativeNotice(17,0x683D58FC5C0C5E90ull,true,0x5029)==0);
    assert(NativeNotice(17,0x683D58FC5C0C5E90ull,true,0x5030)==AttackShark);
    assert(NativeNotice(17,0x683D58FC5C0C5E90ull,true)==AttackShark);
    assert(NativeNotice(6,0x70060281B23AF667ull,true)==0);
    assert(NativeNotice(6,0x98E6602F43E0E69Cull,true)==GravaStar);
    assert(NativeNotice(3,0x0FEFA7117D763FE5ull,true)==0);
    assert(NativeNotice(3,0xBEE2B020B2BFBA27ull,true)==Ipi);
    assert(NativeNotice(7,0xF3B7ECFB2D628746ull,true)==Redragon);
    // Redragon K686 HE (US, BR, UK firmware products) is experimental.
    for(auto token:{0xBB4DA08AE1DC0AE0ull,0x500507220128AACFull,0x76353BB82B7776D5ull})assert(NativeNotice(7,token,true)==Redragon);
    assert(NativeNotice(7,0x2479523793C06E66ull,true)==0);
    assert(NativeNotice(6,0x57494E363850524Full,false)==0);
    assert(NativeNotice(6,0x57494E363850524Full,true)==AulaRm);
    assert(NativeNotice(12,0x4845110000000003ull,true)==Hero84);
    // AJAZZ AK820 MAX HE 0C45:80B1 is not admitted (red): no notice at all.
    assert(NativeNotice(16,0xE73449D3F54AAB62ull,true)==0);
    assert(NativeNotice(16,0xE73449D3F54AAB62ull,false)==0);
    assert(NativeNotice(16,0x4d363050414e5349ull,true)==0);
    assert(ResearchNotice(28,0x0c45,0x80ac,true,false)==Alumix104Research);
    assert(ResearchNotice(28,0x0c45,0x80ac,true,true)==0);
    assert(ResearchNotice(28,0x0c45,0x80ac,false,false)==0);
    assert(ResearchNotice(28,0x0c45,0x80ab,true,false)==0);
    assert(ResearchNotice(27,0x0c45,0x80ac,true,false)==0);
    // Alumix 68 present but firmware unrecognized: yellow research notice (not connected).
    assert(ResearchNotice(31,0x0c45,0x80a2,true,false)==RedSquareAlumix68);
    assert(ResearchNotice(31,0x0c45,0x80a2,true,true)==0);
    assert(ResearchNotice(31,0x0c45,0x80a2,false,false)==0);
    assert(ResearchNotice(31,0x0c45,0x80a1,true,false)==0);
    assert((ImplementedModels & RedSquareAlumix68)==RedSquareAlumix68);
    SetSearchObservation(true,true,RedSquareAlumix68);
    assert(GetStatusSnapshot().frozenModels==RedSquareAlumix68);
    SetSearchObservation(false,true,RedSquareAlumix68);
    assert(GetStatusSnapshot().frozenModels==0);
    SetSearchObservation(true,false,Alumix104Research);
    assert(GetStatusSnapshot().frozenModels==Alumix104Research);
    assert(!GetStatusSnapshot().analogSourceConnected);
    assert(!ShouldAutoSaveSupportLog(GetStatusSnapshot()));
    SetSearchObservation(true,false,Alumix104Research | NA87);
    assert(ShouldAutoSaveSupportLog(GetStatusSnapshot()));
    SetSearchObservation(true,false,0);
    assert(ShouldAutoSaveSupportLog(GetStatusSnapshot()));
    assert((ImplementedModels & (NA87Pro|ND75|Azoth96|NuPhy|FamilyCandidate))==0);
    return 0;
}

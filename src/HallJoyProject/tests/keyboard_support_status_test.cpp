#include "keyboard_support_status.h"
#include "support_notice_catalog.h"

#include <cassert>

int main()
{
    using namespace halljoy::keyboard_support;
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
    assert(NativeNotice(17,0x5348583635414E53ull,true)==0);
    assert(NativeNotice(17,0x5348583638414E53ull,true)==AttackShark);
    assert(NativeNotice(6,0x70060281B23AF667ull,true)==0);
    assert(NativeNotice(6,0x98E6602F43E0E69Cull,true)==GravaStar);
    assert(NativeNotice(3,0x0FEFA7117D763FE5ull,true)==0);
    assert(NativeNotice(3,0xBEE2B020B2BFBA27ull,true)==Ipi);
    assert(NativeNotice(7,0xF3B7ECFB2D628746ull,true)==Redragon);
    assert(NativeNotice(7,0x2479523793C06E66ull,true)==0);
    assert(NativeNotice(6,0x57494E363850524Full,false)==0);
    assert(NativeNotice(6,0x57494E363850524Full,true)==AulaRm);
    assert(NativeNotice(12,0x4845110000000003ull,true)==Hero84);
    assert((ImplementedModels & (NA87Pro|ND75|Azoth96|NuPhy|FamilyCandidate))==0);
    return 0;
}

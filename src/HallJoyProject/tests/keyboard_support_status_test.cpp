#include "keyboard_support_status.h"

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
    assert(ClassifyFrozen(0x3151,0x502d,L"X68HE")==X68);
    assert(ClassifyFrozen(0x3151,0x502f,L"X68HE")==0);
    assert(ClassifyFrozen(0x3434,0x0e40,L"NA87 PRO")==0);
    SetSearchObservation(true,true);
    assert(GetStatusSnapshot().frozenModels==0);
    return 0;
}

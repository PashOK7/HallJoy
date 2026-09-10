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
    return 0;
}

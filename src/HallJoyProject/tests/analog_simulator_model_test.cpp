#include "analog_simulator_model.h"

#include <cassert>

using namespace halljoy::analog_simulator;

int main()
{
    const Snapshot neutral = Evaluate(0);
    assert(neutral.present && neutral.connected && !neutral.faulted);
    assert(neutral.milli[kHidW] == 0);

    assert(Evaluate(500).phase == Phase::WRamp);
    assert(Evaluate(1000).milli[kHidW] == 500);
    assert(Evaluate(1499).milli[kHidW] == 999);
    assert(Evaluate(1500).milli[kHidW] == 1000);
    assert(Evaluate(2250).milli[kHidW] == 500);

    const Snapshot ws = Evaluate(2500);
    assert(ws.phase == Phase::OpposingWS);
    assert(ws.milli[kHidW] == 750 && ws.milli[kHidS] == 750);

    const Snapshot ad = Evaluate(3000);
    assert(ad.phase == Phase::OpposingAD);
    assert(ad.milli[kHidA] == 650 && ad.milli[kHidD] == 650);

    const Snapshot diagonal = Evaluate(3500);
    assert(diagonal.milli[kHidW] == 800 && diagonal.milli[kHidD] == 600);

    const Snapshot disconnected = Evaluate(4000);
    assert(!disconnected.present && !disconnected.connected);
    assert(disconnected.milli[kHidW] == 0 && disconnected.milli[kHidD] == 0);

    const Snapshot reconnected = Evaluate(4500);
    assert(reconnected.present && reconnected.connected);
    assert(reconnected.milli[kHidW] == 0);
    assert(Evaluate(5000).milli[kHidW] == 900);

    const Snapshot fault = Evaluate(5500);
    assert(fault.present && !fault.connected && fault.faulted);
    assert(fault.milli[kHidW] == 0);

    const Snapshot recovered = Evaluate(6000);
    assert(recovered.present && recovered.connected && !recovered.faulted);
    assert(Evaluate(6500).phase == Phase::Complete);

    const Snapshot first = Evaluate(3375);
    const Snapshot replay = Evaluate(3375);
    assert(first.phase == replay.phase && first.milli == replay.milli);
    return 0;
}

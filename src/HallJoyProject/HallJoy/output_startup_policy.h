#pragma once
namespace halljoy::outputs {
struct StartupResult { bool gamepad = false, midi = false, admitted = false; };
// Input capability proof precedes this policy. Keep a failed optional output local;
// an ordinary gamepad-only startup still uses the established dependency guidance.
template<class Gamepad, class Midi>
StartupResult Start(bool midiRequested, Gamepad&& gamepad, Midi&& midi) {
    StartupResult result;
    result.gamepad = gamepad();
    if (!result.gamepad && !midiRequested) return result;
    result.midi = midi();
    result.admitted = true; // Working input stays available to repair requested outputs.
    return result;
}
}

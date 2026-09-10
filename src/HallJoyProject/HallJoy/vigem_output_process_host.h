#pragma once

// Called at the first point in wWinMain. Returns true only when the exact
// internal output-host command was present and places the child exit code in
// exitCode. The fake transport is compiled only into the opt-in simulator.
bool VigemOutputHost_TryRunCommand(int& exitCode) noexcept;

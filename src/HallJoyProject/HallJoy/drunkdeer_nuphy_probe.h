#pragma once
// Read-only evidence for DrunkDeer keyboards on the NuPhy USB VID (19F5): a
// second shared reader of the A0 travel stream that the UAP plugin decodes.
// Writes "evidence.dd19f5" records; sends nothing to the keyboard.
void DrunkDeerNuphyProbe_Start() noexcept;
void DrunkDeerNuphyProbe_Stop() noexcept;

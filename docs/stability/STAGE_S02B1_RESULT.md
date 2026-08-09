# S02B.1 result: MAD68 and Hex80 exception barriers

- Date: July 30, 2026
- Starting point: `HallJoy_v3_9_0_STABILITY_S02A1_SOUP_PATCH_HOTFIX.zip`
- Status: Local implementation complete; Windows/MSVC x64 Release gate required
  before S02B.2

## Package boundary

S02B.1 covers only the two native backends without nested worker generations:
MAD68 Pro R and Hex80. Addressed, Sayo, SparkLink, UAP/C ABI, shutdown timeouts,
protocols, USB framing, polling cadence, and ViGEm scheduling are unchanged.

## Implementation

For each backend, the normal algorithm moved into a separate `uint32_t` body
function. The `noexcept` entry invokes `RunWorkerEntryBarrier`, converts
`std::exception` or an unknown C++ exception into a fixed record, publishes a
safe state on fault, and always clears `g_running` on completion. Fault details
are sent through allocation-free `OutputDebugStringA`.

Before creating a replacement generation, start joins a completed joinable
`std::thread` and releases its old wake handle. This prevents `std::terminate`
when restarting after an unexpected worker exit.

MAD68 fault clears physical/digital down state, stream ownership, mode,
coverage, analogue values, device identity, and firmware/product publications,
then reports `Stopped`. Hex80 fault clears connected/present state,
detected/active PID and version, neutralizes analogue values, and wakes realtime
when publication actually changed.

## Regression evidence

- MAD68 and Hex80 worker bodies match S02A.1 after excluding the new terminal
  `return 0u`;
- polling loops, timeouts, protocol calls, and reconnect waits are unchanged;
- static audit verifies barrier wiring, safe-state publication, and
  join-before-replace;
- Addressed nested-reader work is explicitly deferred;
- full GCC and Clang common gates: PASS;
- portable ASan and UBSan tests: PASS;
- MSVC project XML parse: PASS;
- MAD68/Hex80 hot-path comparison: PASS;
- clean package manifest created before packaging.

The required Windows gate was a clean x64 Release build, HallJoy/ViGEm smoke,
available-device connect/disconnect, repeated close/restart, and confirmation
that input did not stick or hang. When matching hardware was unavailable, the
minimum build/start/ViGEm gate could pass while hardware validation remained
explicitly deferred.

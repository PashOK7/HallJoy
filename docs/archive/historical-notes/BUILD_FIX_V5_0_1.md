# HallJoy Addressed Analog v5.0.1 build fix

This update fixes two problems in the original v5 Stable Clean package:

1. Shared code in `backend.cpp` used `g_physicalDown`, but its declaration was
   in the removed `backend_aula.inc`. The declaration now lives in the common
   backend beside the `g_hidToScan` and `g_hidToVk` tables.
2. The MSVC C4244 warning in the percentile RTT calculation is fixed. The
   `size_t`-to-`double` conversion is now explicit, and the index remains
   bounded as a `size_t` value.

The addressed polling, scheduler, and protocol logic did not change.

Build command:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\BUILD_ADDRESSED_ANALOG_V5_STABLE.ps1
```

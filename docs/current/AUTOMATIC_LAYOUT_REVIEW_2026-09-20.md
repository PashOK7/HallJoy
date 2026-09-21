# Automatic layout transition review — 2026-09-20

Reviewed keyboard_layout.cpp automatic selection, native_layout_state.h,
KeyboardUI_OnTimerTick status-only notification routing and existing coherent
telemetry regressions. No new production defect was confirmed. ATTACK SHARK
protocol work remains paused.

Expanded KeyboardLayout_TestAutomatic in the isolated simulator to cover:

- Host metadata age1000 ms retains the selected immutable snapshot;1001 ms
  unlocks and restores the manual preset. Fresh evidence selects the device again.
- An unhealthy plugin host cannot retain a locked automatic selection.
- 32 complete coherent unplug/replug cycles restore manual/select automatic,
  while repeated identical inventory retains the same immutable snapshot.
- With Automatic disabled, inventories of zero, one and two devices leave the
  chosen manual preset and snapshot unchanged.

Existing production-linked coverage also passes10,000 inconsistent inventory
observations, all reviewed Keychron identities, multiple-device ambiguity,
manual preference restoration, remap success/failure, O3C three-key behavior,
and saved automatic preference. The actual telemetry reader is exercised with
1000 contended captures in the same suite. Status-only routing is checked by
static audit; this is not visual observation of the owner's preview.

Portable tests pass100,000 cache contention cases plus owner replacement,
expiry, generation rollback and concurrent publication; native layout state
validation/concurrency and physical analog alias tests also pass. Coherence and
UAP identity static audits pass. Entire linked profile/layout suite and18 startup
recovery scenarios pass with zero backend initialization attempts.

Evidence:
- `.local/automatic-layout-review-20260920.txt`
- `.local/automatic-layout-review-portable-20260920.txt`
- `C:/Users/PC/AppData/Local/Temp/HJProfileTest-2aedcee2d9454c8b9db7e9186348d197`
- Backup: `.local/backups/automatic-layout-review-20260920.zip`

Only simulator regression coverage and documentation changed. Ordinary HallJoy.exe
and user settings were untouched; no hardware unplug, visual run or publication.
This verifies the shared selector against supplied telemetry, not every device's
real enumeration timing or transport behavior.

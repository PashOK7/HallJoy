# HERO84 integration and layout/native-map audit

## Behavior

Exact interface and UUID 110000000005 proof followed by all 84 layer-0 assignments now publishes the HERO84 ANSI layout token. Existing multi-device ambiguity handling restores manual choice. A disconnected/incomplete session publishes no token.

Native publication retains separate factory and assigned channels from the same physical sensor samples. Successful automatic layout selection uses the assigned channels; manual selection, failed matching or multiple devices use factory channels. Unsupported macro/compound actions are unassigned in the automatic map, but their physical positions remain available in manual mode. No digital depth synthesis was added.

The previous parser accepted only a low-byte HID assignment, dropping factory Ctrl/Shift/Alt/Win masks and Fn. The new typed decoder handles one modifier bit at bits 16..23 and the exact vendor Fn1 action 0D000000. Other compound actions remain unassigned. Fn uses the ordinary physical sensor read at position 72, with the existing 94/02 command; no calibration mode or additional device-writing command was introduced. Normalization remains based on observed extrema, and the experimental warning remains. Real-device Fn validation is still pending.

The factory header contains all 84 position/HID pairs; the source extractor checks exact pair equality against the pinned manufacturer array, not just a key count. This protects the earlier position-53 correction.

## Review boundaries across other layouts

The complete 12-brand generated catalog (88 source variants) reproduces without drift: Keychron 34, Lemokey 2, DrunkDeer 7, Aula 5, Redragon 2, Razer 7, NuPhy 2, Wooting 18, IROK 2, MADLIONS 4, ATK 1, IPI 4 source variants. Built-in presets outside this generator remain covered by production catalog/profile and native protocol tests.

[Saved per-model audit](../research/layout-analog-map-audit-20260919.json). `tools/audit_layout_analog_maps.py` records per-model evidence and limitations rather than calling every geometry a proven analog implementation. IPI's eight native physical-ID sets match the complete layout HID multisets; HERO84 position/HID pairs and KP-TE153 assignments are source-checked. AULA W669/Redragon adapters check native factory matrices. The separate Hex80 audit checks all native slots and physical keys; DrunkDeer checks all seven maps through the actual patched Soup conversion.

Known retained limitation: MG75 Max/Pro layouts include Fn, while SparkLink's byte map does not publish its extended analog code. This is already recorded in the original layout reports. No unsupported Fn protocol was invented. Other brands with SDK/live-map or manual-only identities retain their documented limits; source geometry checks are not a universal hardware proof.

## Tests and delivery

Production-linked profile tests pass, including HERO84 alias release, stale neutral, Fn/modifiers, macro/manual recovery, mode switching, all 84 positions reaching the actual request planner, automatic remap geometry and multiple-device fallback. 47 layout tests pass. The full native suite exposed an outdated test expecting 17 mask words after Wooting split channels expanded the domain to 19; corrected the expectation and added a real bind/read/clear regression at the highest physical Fn code.

All static/portable tests passed across the completed main run and continuation. The continuation reran the isolated instance-guard test and all remaining tests; previously passed tests were not recompiled. The old instance-guard test had attempted to acquire the running app's real mutex. It now uses a run-specific suffix under HALLJOY_INSTANCE_GUARD_TEST, defined only for that test binary. Parent/child conflict and release are still tested through the real guard implementation; ordinary runtime naming is unchanged. No interactive HallJoy closure was required.

Ordinary Release build and four linked-image checks PASS. tools/build_release.ps1 atomically installed build/bin/Release/x64/HallJoy.exe and restored the previously running window. Existing compiler/PDB warnings remain; no build errors. Evidence: .local/hero84-integration-release.txt. SHA-256: 2c4307f953202675b42074c628eb8682f6ea679014a7feff067d97a35b968133. Evidence: .local/hero84-integration-profiles-final.txt, .local/hero84-integration-layouts.txt, .local/hero84-integration-native-final.txt, .local/hero84-integration-native-continuation.txt, .local/hero84-integration-map-audit.json. Backups: .local/backups/hero84-integration/ and the layout pipeline integration backup. No physical hardware or visual UI test was performed.

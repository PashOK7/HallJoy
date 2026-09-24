# Last automatic layout at startup — 2026-09-24

Owner reports K4 HE preview starts as a smaller layout, then switches to K4.
Confirmed code cause: SaveToIni preserved manual PresetName and Automatic,
but no last automatic selection; LoadFromIni activated manual geometry until
live discovery. This was deliberate transient selection behavior, not keyboard
firmware lag. New owner request supersedes that startup presentation policy.

Added LastAutomaticPreset in KeyboardLayout INI section. Load resolves it
against the existing catalog/aliases and uses factory geometry as an unlocked
startup preview when Automatic is enabled. Manual PresetName remains separate.
Live discovery still determines actual source identity and remaps. Cached name
never sets activeToken, claims a connected device, or restores old live remaps.
Pending/empty startup telemetry preserves the preview snapshot; a validated
match replaces it normally. Disabling auto or explicitly selecting a manual
preset leaves preview mode. Subsequent established-device disconnect behavior
is unchanged. Invalid/deleted cache entries fall back to manual geometry.

No per-frame settings writes: existing settings save/shutdown records the last
confirmed selection. Old settings lack the new key, so the first updated run
must detect the keyboard and save/close normally before later launches can
restore it. Do not silently seed the owner's settings with a guessed layout.

Extended production-linked automatic layout test: actual INI save/load,
cached geometry before discovery, stable snapshot during empty/pending search,
manual-name preservation when saved before detection, disable-auto restoration,
and absence of old live remaps before revalidation. Existing freshness,
reconnect, ambiguous-device and profile tests remain relevant.

No support-status changes or Sheet changes. No agent GUI run or GitHub upload.
Validation and final ordinary EXE hash recorded after checks below.

## Validation and delivery

Full isolated production-linked profile/layout suite PASS, including new actual
INI cache roundtrip and old manual/freshness/remap regressions. Catalog audit,
recovery file preservation and18 startup recovery scenarios + repeat PASS.
Backend init attempts0. Evidence .local/layout-cache-tests.log and
C:/Users/PC/AppData/Local/Temp/HJProfileTest-332a67bedfd640229d25e2c5b60796b7.
Ordinary1.6.2 Release and all6 linked gates PASS, K4/R85/AJAZZ retained.
Build .local/layout-cache-release-build.log. Delivered normal path
build/bin/Release/x64/HallJoy.exe SHA256 5e0b2097ef340b49fb415df76056a163cfc19a09d05f9100a180cecde816fcb6;
candidate/delivery equal. Replaces earlier1.6.2 candidate, no forced logging.
Owner visual verification remains separate; no agent manual GUI run.


## Follow-up: retain geometry across USB reenumeration

Owner still observed a jump after the first fix. Confirmed remaining path:
once the automatic layout was locked, Searching/Missing restored the manual
preset. K4 onboard gamepad activation intentionally reenumerates USB, so the
startup-only preview did not cover this lifecycle. No evidence of interrupted
firmware initialization is required to explain this host presentation defect.

Searching/Missing now retain the last geometry as an unlocked preview, clearing
active remap ownership immediately. Applied live remaps revert to factory keys
within the SAME geometry. Reconfirmation of an unchanged factory preview keeps
the same immutable render snapshot. Multiple sources or invalid remaps still
restore manual fallback; explicit manual selection/disable-auto remains usable.
No added delay, timer, polling or firmware changes. Previous disconnect policy
in the September20 review and the initial section above is superseded.

Linked tests now cover32 native K4 admission/disconnect/identity-pending/reconnect
cycles with identical render snapshot,32 UAP reconnect cycles, stale host data,
remapped device disconnect with factory-key restoration/reconfirmation, manual
fallback isolation, ambiguity and cache roundtrip. Validation pending below.
No support-status change, user-settings seeding, GUI run or GitHub publication.


Follow-up validation: full linked profile/layout/remap suite PASS, catalog audit
PASS,18 startup recovery scenarios + repeat PASS; backend init attempts0.
Evidence: .local/layout-reconnect-tests.log;
C:/Users/PC/AppData/Local/Temp/HJProfileTest-2aa6773f3998426595e6f5e43f632505.
An initial run failed in the unrelated isolated-desktop overlay editing helper
before reaching automatic-layout tests; an unchanged retry passed, and the final
run after adding remap reconnect coverage passed too. No production overlay
change was made; exact cause of the first helper failure was not established.
Backup: .local/backups/layout-reconnect-20260924-093739/keyboard_layout.cpp.

Ordinary1.6.2 rebuild and all6 linked release gates PASS; delivered
build/bin/Release/x64/HallJoy.exe SHA256
ceb47c21bfb7ff12efccd6e15853c8fddc6a2793e8dfc95f39bc121dbe4a0183.
Candidate and delivered hashes match. Build: .local/layout-reconnect-release-build.log.
No forced logging, firmware change or GitHub upload. Owner visual check pending.

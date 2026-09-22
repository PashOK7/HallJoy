> Follow-up: the owner corrected the support criterion; the four yellow statuses below are historical and have been promoted. See [current protocol review](BRAND_PROTOCOL_REVIEW_2026-09-21.md).

# Existing-protocol additions — 2026-09-21

Owner scope: finish models using a known protocol; yellow is acceptable without
hardware certainty. Do not reverse engineer different protocols or work on closed
Keychron firmware. No hardware, flashing, or visual run performed.

## Implemented locally

- Keychron K6 HE ANSI, Q2 HE ANSI, Q4 HE ANSI: exact official 2025q3 source
  matrices and Launcher geometry, 68 / 66 / 61 analog keys. Q2 knob excluded.
  Firmware source commit 9ada9b7baecb9591c469b9b068146ac5891a480a.
  New rows are appended to preserve the existing Keychron preset ordering.
- Independent Fn at matrix 4,10 and Fn2 at 4,11 use existing HallJoy codes
  0x409 / 0x408. These are host physical identities assigned to matrix positions,
  not firmware action codes. Transport maps and layouts are generated together.
  This removes the old second-Fn blocker for these three ANSI variants only.
- Exact new PIDs are admitted on FF60/61, with FAR version marker 0x45 required.
  No stock polling fallback for the new models. The owner's custom-firmware
  preparation premise remains in force; no prebuilt/flashed firmware is claimed.
- FAR receiver now uses floor(matrix slots / 30) + 1 exact 32-byte reports,
  matching the firmware's unconditional final send. The old hardcoded four
  packets would wait for a nonexistent fourth report on 70/75-slot matrices.
  Existing 90/96/114-slot models still read four. Malformed/truncated replies
  fail without publishing partial key state. The known untagged ABI is unchanged.
- Wooting 80HE+: manufacturer PID 0x1410, ordinary ANSI/ISO and Split ANSI/ISO
  layouts (84 / 85 / 86 / 87 keys). Generic Wooting discovery/decoder already
  admits this family. No new protocol or firmware command added.
  Wootility Rg declares cT=Dg+Sg and family selection B2 uses Hu. Existing pinned
  bundle SHA256 987ba72a09a78b1fc508c8739b1c0a25a3434d7ef614fba56f1822e5acdbc40a.
  Independent split positions 5,4 / 5,8 / 5,6 / 5,12 use the existing four aliases;
  60HE v2 retains 5,13 for its right Fn. No inference for other PIDs or JIS.
  Layout selection remains manual because regional/split identity is not reported.

## Evidence and excluded work

Primary source snapshots and immutable URLs: ../research/known-protocols-20260921/sources.json.
AnalogSense FAR source e21916b695ec3b31fbbbb325e32849be5cdb9e46 proves row-major
travel bytes and final-report semantics. The official Wooting SDK admits the
vendor plus FF53/FF54 interfaces without a model PID allowlist. This establishes
an existing implementation route, not a physical 80HE+ test.

Razer: OpenRazer identifies Huntsman V3 Pro 8KHz as 1532:02CF; Tartarus Pro is
1532:0244. Its RGB/device support does not prove the analog report payload used
by HallJoy. No analog decoder/admission is added for the seven extra Razer rows.
References: https://github.com/openrazer/openrazer/issues/2633 and
https://github.com/openrazer/openrazer/issues/2793 and https://openrazer.github.io/.

The checked official Keychron 2025q3 / hall_effect_playground source listings
contain K6/Q2/Q4 definitions, not the extra Q0/HE8K/C/J/V HE definitions needed
for this task. Source absence in those branches is not a claim that source can
never exist elsewhere. No binary reverse engineering or closed-firmware work.
K6 ISO remains excluded: no exact source set. Q2 HE 8K is a distinct model.
Previously supported models and their recorded owner decisions are unchanged.

## Sheet update

Workbook 1ueQ4labXpuBOmGjCkUcJllNJ68Jzx4py2MuQm-p-R7c, Main:
- C294 K6 HE, C300 Q2 HE, C304 Q4 HE changed from Not investigated to
  Implemented; custom firmware; awaiting testing.
- C542 Wooting 80HE+ changed from Not investigated to
  Implemented; awaiting hardware testing.
- Added the custom-firmware status to these three dropdowns and the existing
  yellow conditional rule. Other statuses, cells, validation and styles retained.
- Readback confirms all four B:C pairs have RGB 1 / 0.9019608 / 0.6392157.
- No new green Supported claim, source notes, or user support checklist added.

## Validation

Production FAR loop executed against mocked firmware packetization: 70, 75, 90,
96, 114, 120, 126, 132 slots; failed sends and every truncated-fragment position.
Keychron catalog/source reproducibility and Fn uniqueness: six tests PASS.
Wooting physical-channel test: both exact PIDs, separate depths, binding,
controller output, release and disconnect PASS. All 20 layout pipeline tests
and native static audit suite PASS. Wootility extraction --check PASS for all
nine source reports; prior five reports unchanged. Native UAP ABI0/ABI1 build
and ordinary Release x64 build PASS (existing ViGEm debug-symbol warning only).
No keyboard-specific live diagnostics or forced logging enabled.

Backups: .local/backups/known-protocols-20260921 and layout-integrate-sv0_clng.
GitHub release v1.6.0 and its EXE are unchanged by this work.

Local candidate: `build/obj/ReleaseCandidate/x64/HallJoy.exe`. SHA256 `3eb5fe64b96abe66bec56ca9622a1e91d5ef3e24718582d841981b5c626e4342`.
The installed delivery EXE and GitHub release have not been replaced.

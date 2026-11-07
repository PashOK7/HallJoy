# AJAZZ AK680 MAX HE — Supported, 2026-11-05

Owner explicitly requests Supported after providing a tester screenshot: AJAZZ
AK680 Max works normally, no complaints; asks whether the testing banner can be
removed. Accepted as model-level user confirmation, not an agent hardware test.
The screenshot does not identify a board, firmware, regional variant or transport;
no such new hardware details are invented. Private capture SHA256:
ee5b7d613e339babf9107eabf56beda870210793f7d771b12891353d4a55581b.
Local-only evidence: .local/research/ajazz-ak680max-supported-20261105/tester-feedback.png.

## Retained implementation and exact scope

Existing native RongYuan stream22 support covers boards2255/3151:502D and
2336,2343,2371/3151:502F. No admission, firmware/scale or input path is added.
These exact profiles were source-reviewed in RONGYUAN_BATCH_2_2026-09-24.md;
owner permits model-level promotion without testing each known hardware revision.
This confirmation is not transferred to AK680 V2/V3/MC, ALUX68 or a receiver.

The already-built preset is "AJAZZ AK680 MAX HE + ALUX68 AIR + ALUX68 PRO ANSI",
68 keys, ry_layout_2255, tokenE8EB300245326B1C. All four AK680 MAX products have
automatic selection. Preset/token is also shared by ALUX68; its5030 PID must
continue to show the testing notice. confirmed_devices therefore records the
four AK680 MAX boards with their exact PID, not a whole-token exclusion.
supported_layouts.json now records AK680 MAX HE and its existing exact preset.

The generated runtime notice and telemetry text use the same support catalog.
README moves this model to Supported; hardware evidence and NEXT notes agree.
No forced logging or global banner suppression is introduced.

## Validation and delivery

The existing compiled catalog contains the68-key preset. The intended Supported
state passed both compiled-layout and Sheet-model checks BEFORE the live write.
Reference snapshots were refreshed once because the notice catalog changed; all
original private audits passed. Cached native suite then passed EXIT0, including
exact AK680 MAX notice exclusion and ALUX68/unknown-PID negative cases.

Live Main!C11 changed Implemented; awaiting hardware testing -> Supported.
Fresh readback verifies AJAZZ / AK680 MAX HE, unchanged strict dropdown, green
BASE and effective B11:C11, and unchanged neighboring rows. Full bounded before/
after data differs for exactly this model. No rows were inserted or reordered;
no notes/comments were added. All283 yellow notices and87 Supported layout records
reconcile. Evidence: sheet-before.json, sheet-intended.json, sheet-after.json,
sheet-proof.json; logs: reference-record.log and native-checks.log in the private
task directory.

Ordinary tools/build_release.ps1 EXIT0: complete native catalog, embedded resources,
production profiles/automatic layout transitions, catalog coverage and recovery
(18 scenarios plus repeated startup) PASS. Fresh compiled catalog contains68
rows for the AK680 MAX combined preset; Supported gate passes with all87 models.
The new ordinary EXE was installed atomically, with identical candidate/delivery
bytes: version1.6.14.0,11330048 bytes, SHA256
`142fe70791789632ef46464e01c3e3b46d9ad4a0b756d4e8cfbd7e6615bf3b51`.
It supersedes6cb724b0 locally and retains the Keydous USB/receiver test path.
The AK680 MAX testing banner is suppressed by the generated runtime conditions;
ordinary communication/problem warnings are still active. No visual run or agent
hardware test was performed. Build log: build.log in the private task directory.
The ordinary EXE must retain the previous Keydous USB/receiver test path and all
approved native functions. No GitHub publication is requested by this change.

Verified backup: .local/backups/ajazz-ak680max-supported-20261105.zip.

# MADLIONS layouts

2026-09-14. Owner requested all four previously listed supported MADLIONS models.
This explicitly resumes layout work after the earlier release pause.

| Preset | Keys | Exact scope and geometry |
|---|---:|---|
| MADLIONS MAD60HE ANSI | 61 | Original UAP-supported HE revisions; official DuckBread Lt |
| MADLIONS MAD68HE ANSI | 68 | Original UAP-supported HE revisions; official gt inherits yt |
| MADLIONS MAD68R ANSI | 68 | UAP-supported 373B:10A7, official MAD68 R (LL); wt inherits yt |
| MADLIONS MAD 68 Pro R ANSI | 68 | Official MAD 68 Pro, including native-tested 373B:1109; Sn |

Select Layout / Brand MADLIONS. Four named presets remain separate; MAD68HE and
MAD68R share official geometry. No new automatic identity matching is added.
Other MAD68R revisions 106E/10A8 use the Fire-family wn geometry and are not
represented by the 10A7 preset. Titan68 Turbo, MAD60/68 V2, LIGHT, FIRE and other
models are outside this requested four-model batch. Their analogue support is
not inferred from a brand name or visual resemblance.

Sources: https://hub.fgg.com.cn/ official driver. Exact bundle names, SHA256 and
URLs are locked in tools/prepare_madlions_layouts.py and each reviewed report.
Source files: docs/research/madlions-layout-sources/. Main device catalog binds
10A7 to DuckBread/MAD68 R (LL), while 1109 binds to Fire/MAD 68 Pro. DuckBread
geometry is in ConfigPage-jy-tY0c4.js; Fire geometry and the shared renderer are
in vue-draggable-plus-cdvt9v0M.js. ConfigPage-8vLbbb0u.js proves the Fire layout
selection through the device name. Initial source acquisition files are retained.

The extractor statically resolves string tables and parses literal objects.
It does not execute downloaded JS, access HID or write keyboard settings.
Absolute l/t coordinates and 50 px base-size multipliers follow the official
renderer, normalized to 42 px standard keys. English labels use the Windows
base layer. Matrix positions and key assignments are not guessed from key count.

All 61 MAD60HE and 68 MAD68HE physical positions were compared against the
existing UAP tables. MAD68R uses the same UAP table as MAD68HE. MAD60HE does not
render the UAP table's unused RGUI entry at matrix 4,9, which has no physical key
in the official layout. All 67 non-Fn Pro R usages match kKeyDescriptors in the
native backend. Fn is displayed as 0x409; native Pro R currently uses HID 0 for
its Fn descriptor and does not publish its separate analogue value. UAP HE/R
layouts retain their existing Fn mapping. Analogue backends are unchanged.

Commands:

    python tools/prepare_madlions_layouts.py
    python tools/layout_pipeline.py check MADLIONS
    python tools/run_native_backend_checks.py --static-only

All passed, including source/report locks, four model bindings, key counts,
unique HIDs and contour overlap. Final MSVC Release/x64 build passed; only the
existing ViGEm LNK4099 missing-PDB warning. Existing generated geometry/presets
are byte-preserved as prefixes; generated identities.h is byte-identical.
All four names were verified in the final EXE. No agent visual or hardware run;
owner evaluates appearance. Normal gamepad and NA87 native support retained.

EXE: build/bin/IrokNa87Diagnostic/Release/x64/HallJoy.exe
SHA256: 817959975abc83372042cd5885926bd38a56b9a6af764233388b1dcc62fa9762
Evidence: build/evidence/madlions-layouts-20260914/verification.json
Backup: .local/backups/madlions-layouts-20260914/ plus pipeline backups
layout-integrate-bhqcd5o_ and layout-integrate-g4ymgns7. The first generated
MAD68R draft was corrected to exact supported 10A7 before final delivery.
To roll back this batch, restore backed-up catalog, runner and generated headers;
restore the backed-up EXE if reverting the binary. User settings were not edited.

## Local startup observation after installation — 2026-09-14

Owner reported slow startup. Read-only review of the 00:09:12 session found a
5.289-second wall-clock gap between spark.settings/poll_mode at 00:09:13.008
and overlay/start.ok at 00:09:18.297. Backend init begins only at 00:09:18.344
and succeeds at 00:09:18.521 (177 ms), so keyboard discovery does not explain
the preceding long main-thread gap.

Local layout file metadata confirms new MADLIONS presets were being created
at 00:09:15--18, inside that gap. SettingsIni_Load calls KeyboardLayout_LoadFromIni;
EnsureInit loads existing presets then synchronously calls EnsurePresetFilesExist
before the main window is displayed. Each missing preset uses SavePresetFile,
which writes per-key INI fields using repeated WritePrivateProfileStringW calls
and validates the resulting temporary file. New preset materialization is thus
a directly evidenced contributor. The entire 5.289 seconds is not separately
profiled; existing preset loading and UI initialization also occur in this interval.
Subsequent launches should avoid these four writes, but have not been measured.
No runtime fixes or user settings changes were made during this read-only diagnosis.

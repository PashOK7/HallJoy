# IPI physical layouts — 2026-09-14

> 2026-09-14 implementation update: [IPI native support](IPI_NATIVE_SUPPORT_2026-09-14.md).
> Exact UUID profiles, complete live maps, device calibration, Fn and alias
> publication now replace the historical IPI fallback. Eight models/four layouts.
> Earlier layout-only/no-EXE statements below describe the preceding step.
> Physical USB tests and AURORA65W receiver forwarding remain unverified.

> 2026-09-14: [IPI firmware reverse](IPI_FIRMWARE_REVERSE_2026-09-14.md) now covers18 images/eight UUIDs.
> Addressed analog handlers and exact physical-ID sets confirmed in firmware;
> 216 synthetic handler/helper cases PASS. Empty-map request and calibration
> policy gaps remain in HallJoy. No hardware test or EXE change in this step.

Four manual presets cover eight exact public catalog models:

| Preset family | Keys |
|---|---:|
| QBZ75 / Aurora 75 | 82 |
| QBZ65 / AURORA65 / AURORA65W / RAIN65 | 67 |
| Aurora75 PRO | 82 |
| flash68 | 68 |

Same-geometry models are merged only after exact comparison of all physical IDs
and rectangles. Plus revisions are excluded. These are ANSI physical layouts,
not new analog backend support or proof of all transports working. No automatic
regional/device selection was introduced. User configuration remains untouched.

## Sources and resolved discrepancy

The public catalog at api.hubx.pro/v1/device/devices identifies model UUIDs for
IPI tenant 6. The official qbz.ipigame.cn/keyboard/ application loads a separate
geometry module per UUID. Those modules provide physical IDs, not USB usages.

Further review located vendor-bytech-Dmk7DNgV.js export J (array pa), with 145
explicit ID/keycode pairs. demo-device-session-CMlZtzIh.js imports that exact
export and joins it to the selected layout by physical ID. This supplies the
official demo defaults, independently of a captured user's remapped settings.
Modifier bit masks decode to standard USB modifier usages; the vendor FN action
is represented as display Fn 0x409. Native publication of Fn is not added.

This resolves the missing-default-label gate recorded in REMAINING_LAYOUTS_2026-09-14.md.
It does not resolve the separate runtime fallback discrepancy: Addressed's
canonical table has unassigned keys and assigns the physical Left Alt ID69 to
Right Alt. The vendor default is Left Alt, while ID71 is Right Alt. Existing
live mapping takes precedence; backend behavior is unchanged in this layout task.
The issue is recorded next to kCanonicalKeys in addressed_analog_backend.cpp.

Coordinates retain manufacturer unit positions and sizes, normalized to the
existing HallJoy 46px pitch / 4px gap. No guessed keys or row reflow. All four
reports pass unique-HID and contour validation. Source hashes, UUID module paths,
and report hashes are locked; downloaded JavaScript is never executed.

## Reproduction and validation

python tools/prepare_ipi_layouts.py
python tools/layout_pipeline.py check IPI
python tools/run_native_backend_checks.py --static-only

Backup: .local/backups/layout-followup-20260914 and pipeline integration backup.
Validation and delivered executable hash are appended after the build.

## Completed validation

Source regeneration, all static checks, full production-linked profile/control
regressions and MSVC Release/x64 build PASS. Backend initialization attempts: 0.
Existing generated geometry and registration remain unchanged byte prefixes.
No visual or hardware run. Existing optional ViGEm PDB warning only.

Delivered: build/bin/IrokNa87Diagnostic/Release/x64/HallJoy.exe

SHA256: 9cf7a1b29a0d85801ab56a65ddee0c9888a9f3fa9a1e53691ea781beb6ad2f52

Evidence: build/evidence/ipi-layouts-20260914/verification.json and
profiles/profile-test-result.txt. Logs: .local/ipi-layouts-{static,simulator,release}.log.
No additional delivery folder; the main EXE was replaced.

## Support qualification after owner question

The cached official catalog assigns all eight models supplier BY, protocol
MagneticSwitch v1, usage FF60:0061 and report ID09. Wired entries share
372E:105C; AURORA65W wireless uses 372E:106C. This proves common declared
software protocol family, not identical firmware, hardware or complete analog
compatibility. Model-specific UUIDs remain different.

Existing HallJoy physical evidence covers the QBZ75-compatible addressed route.
No individual hardware confirmation for all eight models was established by
this layout work. Other models are compatibility candidates on the same declared
family, not eight newly confirmed supported keyboards. Admission additionally
requires a valid correlated 09/94/02 response; a shared VID/PID is insufficient.

A further review concern is BuildProfile's verifiedCalibrationSeed assignment
in addressed_analog_backend.cpp: it selects captured raw endpoint seeds solely
by 372E:105C. The catalog shows that identifier is shared across these models.
Therefore its name/condition cannot establish model-specific calibration
validity. Alongside the incomplete fallback map, this prevents a blanket claim
that all eight will work correctly. No runtime behavior was changed here.

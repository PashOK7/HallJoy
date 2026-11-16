# Everglide SU75 Pro V1 / V2: exact firmware evidence (2026-11-14)

> Runtime continuation: [2026-11-15 automatic-layout/input implementation](EVERGLIDE_SU75_IMPLEMENTATION_2026-11-15.md). The manual-only/no-new-EXE statements below describe the research baseline.

This supersedes the unresolved revision/PID conclusions in the October 12 report.
Research of the existing support path; no runtime/admission change or new tester
EXE. No keyboard was flashed, opened or physically tested by the agent.

## Result and scope

| Retail / PCB identity | Producer firmware identity | Current HallJoy behavior |
| --- | --- | --- |
| SU75 Pro V1; seller PCB marking `SU75 Pro_XS117_V1.2` | `1CA6:3002`, board `00300002`, C048P002, XS117; API name `Su75PRO` | Existing exact Supported identity; October 2 tester success belongs to this USB identity. |
| SU75 Pro V2 pictured by the seller; PCB marking `SU75 Ultra_SZX_V1.1 20260401` | `1CA6:3007`, board `00300007`, C048P007, XS125; API name `SU75 Ultra` | Existing official-list SparkLink v2 admission, enabled since 1.6.10; generic yellow notice after reply/live-depth proof. |

The V2-to-Ultra association is supported by the actual silkscreen in the seller's
[V2 PCB photograph](https://keebforce.com/wp-content/uploads/2026/08/SU75-PRO-V2-full-pcb-board.webp),
not merely by the shared hub URL. The [comparison](https://keebforce.com/su75-pro-v1-vs-v2-pcb/)
identifies that picture as V2. Do not generalize the mapping to an unpictured future
retail revision, or confuse the PCB revision with the protocol version `v2`.
The October tester did not explicitly report V1/V2; the new producer association
identifies the platform behind the tested `3002` identity, not an additional test.

Both reviewed identities use `FFB0:0001`, report ID0, SparkLink v2:
`01 02` device info, `03 01 00 row` assignments, `04 03 01 row` live travel.
A different MCU architecture does not require a different host decoder here.
`3007` is already in `sparklink_official_identities.h`; no ID patch is needed.
This is an existing protocol-match path, not a new named Supported promotion.

## Acquisition and provenance

Private evidence directory:
`.local/research/everglide-su75-revisions-20261114/`.
Vendor binaries, configurator bundles and PCB pictures remain local; public access
is not redistribution permission. Acquisition used normal read-only public-client
requests; no firmware update command or authenticated user data was involved.

- Hub `https://hub.xsyd.top/`, version1.5.4; keyboard V2 SDK
  `/keyboard/js/esm-BcEkXhfe.js`, SHA256
  `4d6d717e19a54f57f98cb4d9d5a76992513f06e328a2c53b6dbffa6c2c443acc`.
- Fresh `https://api.sparklinkplayjoy.com/api/v1/getAllDevices`:547 catalog keys.
  `7334-12290` is public/v2; `7334-12295` is shuangzixin/v2. The catalog has no
  product names, but the firmware endpoint supplies them.
- The actual hub requests `/api/v1/getKeyboardFirmware` using hex VID, PID and
  the device's eight-digit `board_id`. Named producer replies are retained as
  `device-3002.json` / `device-3007.json`. They connect names, board IDs, chips,
  product codes and exact download URLs. Board IDs used for acquisition were
  derived from C048/P002 or P007, then checked against the returned profile,
  binary header and V2 info-handler output; no connected device was queried.
- SU75 Pro latest listed: `XS117_C048P002_Su75Pro_App_v0.2.0.0_20260930a.bin`,
 119552bytes, SHA256
  `b4e4dc21f3e879e69e349d4c1f5a8f7d6160f1b07b6fd9373ba0436821a0538e`.
 Six XS117 images were acquired; only header/identity/catalog were checked for
 this V1 association. No V1 firmware emulation is claimed.
- SU75 Ultra latest listed: `XS125_C048P007_SU75Ultra_App_v0.0.6.0_20261015a.bin`,
 163664bytes, SHA256
  `aac64f14be4ad68128a17db728fdabc4b006660042759a04263f046479cee2df`.
 Two older XS125 images were acquired; execution below is specifically0.0.6.0.

`evidence-manifest.json` pins all acquired/research bytes and source URLs where
known. `audit_su75_v2.py` and `branch-replay-r2.json` retain the reproducible replay.

## Exact XS125 branch execution

4096-byte update header; executable payload mapped read/execute at08009000.
The repeated default-handler vector0800937B points to the actual self-loop at
file137A, and reset08009361 to file1360. This establishes the image offset rather
than guessing an address from a plausible disassembly. RAM is synthetic and mapped
explicitly; no automatic MMIO, USB, ADC or second/third-MCU emulation.

Dispatch descriptors at0802F940 select the Device, LayoutAndKey and Performance
function tables. Concrete reviewed entries:

- Device-info writer080174F8: executes until08017528, immediately before the UID
  helper. It emits keyboard type1, board00300007, app0.0.6.0, PCB0-1-1-0.
  Serial/UID bytes are neither modeled nor retained.
- Layout getter0801A880: `03 01 layer row`, six rows,21 LE16 assignments copied
  from RAM733C; row stride3C6 and per-column stride2E. Remaps, unbound assignments
  and duplicates are returned unchanged. Host uses layer0.
- Travel getter08018F38: `04 03 01 row`, six rows,21 LE16 readings copied directly
  from RAM4FCC + row*5E8 + col*48 +1A. No changed-key queue or missing-release
  reconstruction. Zero is a normal value; all21 slots are copied every read.
- Factory assignments at0802F418 and packed geometry at0802E7F2:81 nonzero
  physical assignments, including Fn F101 -> HallJoy409. Every row's nonzero
  HID sequence equals the existing SU75 Pro ANSI source. Result retained in
  `factory-map-comparison.json`.

22 replay scenarios PASS: six full rows with independent supplied depths0..4000,
six same-session all-channel releases, six assignment rows with aliases/unbound/Fn,
and four invalid route/layer/row cases. Travel writes are limited to the reply
and stack in this modeled call. This proves getter behavior against supplied RAM,
not the physical ADC's scan freshness, measured resolution, USB rate or packet loss.

## Behavior review and present limitations

The chosen host path remains the already implemented read-only full-row travel
getter. No calibration/settings writes, persistent debug mode or event-only
fallback was added. The reviewed travel branch has no typing-diversion write.
Full ordinary-HID coexistence under actual scheduling was not executed. All-row
replay establishes simultaneous supplied values and release copying, not a
physical multikey measurement. Calibration, sensor curve, quantization and
transport contention remain hardware-dependent; a full sensor-to-gamepad
production-session replay was not performed in this research.

The existing 81-key preset `Everglide SU75 Pro ANSI` matches V2 factory key order
and physical arrangement. Both currently require manual preset selection;
SparkLink does not publish an automatic-layout token. This report is not a new
tester delivery: before a targeted new EXE, implement/verify autoselection and
complete the mandatory production-session delivery cases. No unverified guarantee
of perfect analog or a blanket V1/V2 Supported claim is made.

## Documentation and Sheet reconciliation

README retains Supported SU75 Pro scoped to1CA6:3002 and links this revision check.
SUPPORTED_HARDWARE retains the same Supported row and explains the separate existing
generic3007 path. No named runtime notice, new support row or Supported promotion
was introduced. Generic protocol-match devices are outside the named Sheet notice
catalog, as documented in SPARKLINK_GENERIC_ADMISSION_2026-10-12.md.

Live read2026-11-14: Main has1306 rows; the sole Everglide row is Main!A266:C266,
Everglide / SU75 Pro / Supported. Status dropdown retained; B:C base and effective
Supported green agree (.65882355,.8666667,.70980394), dark green text retained.
No Sheet cells were written. Neighbors265/267 are separators. No pending sync.

Validation: exact firmware22-case branch replay PASS; factory-map81-key comparison
PASS; `layout_pipeline.py summary Everglide` PASS; `project_context.py --check` PASS.
No application compilation, visual run, release or hardware test was needed for
these evidence/documentation changes. Ordinary installed EXE remains unchanged.

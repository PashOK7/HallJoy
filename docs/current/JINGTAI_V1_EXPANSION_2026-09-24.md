# JingTai V1 expansion — local, 2026-09-24

## Scope and implementation

Five newly enabled experimental wired USB models:

| Public catalog | Exact USB scope | Physical channels including Fn |
|---|---|---|
|IROK NA87 Pro|1C4F:EE88 or1CA2:0401, exact official product names|87|
|IROK ND63|1C4F:EE88 or1CA2:0406, exact official product names|64|
|IYX MU68 Pro|1C4F:EE88 or1CA2:0402, exact official product names|68|
|IROK Mercury68|1CA5:0806, CAROTMAS MER68|68|
|IROK Mercury68 Pro|1CA5:0806, CAROTMAS MER68 PRO|68|

The vendor catalog explicitly supplies old/new USB identities and Cyan/TTC/Universal product aliases. They are28 exact VID/PID/product tuples, not28 models. No generic product-name or shared-PID admission. The generated header lists the complete exact strings. MU68 Pro Cyan and ND63 Cyan/TTC are aliases covered by the same reviewed factory tables; neither MU68 Ultra nor ND63 Ultra is inferred from those names.

Existing MG75 Pro backend now accepts per-model factory actions and publishes each model's assigned actions and analog samples through the ordinary HallJoy engine/gamepad path. Its existing MG75 Pro firmware-derived factory proof and automatic layout token remain intact. New models use manual visual presets where no exact geometry identity exists; verified source factory maps still provide all keys independently.

Only read-only0x12 travel and0x23 base-layer assignment requests are used for the newly admitted models. The existing MG75 Pro retains0x2B factory reads; no assumption that older V1 devices implement that command. Admission requires exact HID identity, unnumbered65-byte Windows input/output reports at FFA0:1, both valid travel halves and valid assignment replies before claiming the interface. Failed/late exchanges poison the session rather than guessing which half arrived. Exclusive vendor-interface ownership, release/stale cleanup and lifecycle behavior remain in the existing implementation.

Normalization is4mm for ordinary NA87 Pro/MU68 Pro/ND63 and3.6mm for explicitly identified Cyan/TTC variants, backed by saved firmware conversion tables. Mercury68/Pro retain provisional3.5mm. Actual switch endpoints and firmware behavior have not been tested on these five physical devices. This is usable experimental support, not a promise of measured accuracy. No calibration/configuration writes, forced diagnostic logging, firmware flashing or GitHub publication.

## Primary-source evidence

Official client: https://hid.irok.cn/assets/index-D22onkw6.js

Pinned at `docs/research/jingtai-v1/index-D22onkw6.js`, SHA256
`e9aebf1b6c1c452d1be8d9481e2838fcc26c0820ba648dd2fff3f617839e37aa`.
Vendor JavaScript is parsed as data, never executed.

- `cc` catalog contains exact product names and old/new USB IDs.
- `ul` driver routes the reviewed V1 groups to `wIt`; `wIt.startTravels` polls selector2 halves1/2 with `[92,4,18,checksum,2,half,255,255]`. Calibration is a distinct command and is not required for this read loop.
- `FMt` decodes little-endian16-bit travel. `zc` selects xMt/za/_l/pIt maps, and `gs` establishes the actual physical key set. The generator requires complete, unique coverage of that set, rather than copying unused inherited positions.
- Each half contains132 logical bytes in three64-byte reports. Vendor UI removes the six-byte header but retains trailing packet padding, so its second-half indices start at93. HallJoy's parser strips padding; index93 maps to compact slot63 (subtract30), not93. The generated factory maps explicitly perform that conversion.
- `cmdKey(35, makeCMDSelectArg(keys,0))` semantics use base-layer assignment reads. Existing native0x23 parser validates echoed selectors and response framing.

The saved September13 Pro firmware corpus was rechecked, without changing its frozen binaries or scripts. All10 USB descriptor/physical-map checks and all10 lookup-to-depth range checks PASS. Current generated compact actions match every firmware map, including all empty positions. Source URLs, image hashes and ranges are retained in `docs/research/jingtai-v1/firmware-evidence.json`. This adds firmware evidence for NA87 Pro/MU68 Pro/ND63; it does not imply firmware emulation of Mercury68. The owner's later request to integrate known protocols with experimental notices supersedes the earlier tester-dependent research hold for this scope.

Audit/generation: `python tools/check_jingtai_v1_profiles.py` (`--write` regenerates with a current-content guard). The native check runner invokes this audit. Full source hash pinning makes an upstream change require renewed review.

## Deferred models

The official MU68 Ultra and Polar75/MG75 tables inherit stale positions: after restricting to actual keys, duplicate selectors remain. Do not silently choose one entry, combine unrelated sensor channels, or extend the five-model declaration to these variants. Existing MG75 Pro support remains valid because its map has independent firmware evidence. The other variants need a firmware matrix or a proven read-only dynamic factory-map response. This is a concrete mapping gap, not a requirement to find a tester.

## Sheet and documentation

README, hardware documentation, next patch notes and runtime notices updated. The obsolete blanket NA87 Pro unsupported claim was removed for this exact scope; ND75 and unrelated frozen models remain unchanged.

[Live Sheet](https://docs.google.com/spreadsheets/d/1ueQ4labXpuBOmGjCkUcJllNJ68Jzx4py2MuQm-p-R7c/edit): Main, sheetId0. Fresh native snapshot plus unchanged-snapshot comparison preceded one18-request batch. Four existing IROK statuses changed: Mercury68/Mercury68 Pro/ND63 from Not investigated, NA87 Pro from Research frozen; tester needed. IYX/MU68 Pro added in a new brand block. All now use the existing `Implemented; awaiting hardware testing` dropdown value.

Independent readback:646 models,251 yellow,63 green,328 gray,4 red;148 brand blocks. All251 yellow rows match the runtime catalog. Native structure plus base/effective colors PASS.1269 previous rows exactly preserved, four affected rows differ only in authorized status/color fields; all prior validations, row heights, widths and rule semantics preserved. No cell comments/notes. API verification, not a claim of owner visual acceptance.

Artifacts: `.local/jingtai-{before,after}-20260924.packed.json`, `.local/jingtai-sheet-plan-20260924.json`, `.local/jingtai-statuses-20260924.json`. Before-code backup `.local/backups/before-jingtai-v1-20260924.zip`.

## Validation status

Ordinary Release and six linked-image gates PASS; installed EXE SHA256
`58e7156d31b2da5219ae1d10372437e4131fd412a90ee8b13c90416285a6f763`.
Build log `.local/jingtai-release-final-20260924.log`.
Full native regression suite PASS (`.local/jingtai-native-20260924.log`). After the range refinement, affected protocol/alias/publication/support-status regressions rerun and PASS (`.local/jingtai-targeted-final-20260924.log`). Production-linked profile/layout tests and18 recovery scenarios plus repeated startup PASS on the final source (`.local/jingtai-profile-final-20260924.log`). No GUI visual run or physical device test. A preliminary ad-hoc command named a nonexistent standalone Slice75 test; the actual existing three_keyboard_protocol regression was then located and passed. This was a harness invocation error, not an application failure.

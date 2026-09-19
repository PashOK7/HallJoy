# ATTACK SHARK X65 / X68 / X82 Pro HE — renewed investigation

Owner clarified on 2026-09-19 that all three interested users have **Pro** models.
Non-Pro X65/X68 must not be substituted. There are now potential external testers;
the September12 statement that no tester is available is superseded.

## New exact firmware

The official firmware API now supplies **X82 Pro HE dev_id2935 v503**:
POST `https://api2.rongyuan.tech:3816/api/v2/get_fw_version`, JSON dev_id2935.
GET `https://api2.rongyuan.tech:3816/download/fw_upgrade_file/2935_v503`.

- Compressed raw-DEFLATE: 61501 bytes, SHA256
  `4cc3aac8692e67efec257d83e04c6532d8ffad700fb6b7bf1df770231894a992`.
- Inflated firmware: 121480 bytes, SHA256
  `0ce46d01a2e8d40b2728e7203ed7152cc64c577d309abd573ab93589d2990395`.
- Embedded strings include AT32F405 8KMKB and X82PROHE-1.
- Metadata queries for2308,2938,2370,2901,2356 still return HTTP500 Record not found.
  Control non-Pro2268 still supplies v309. This is not proof no other image exists.
- Current qmk.top changed to index.9Prprr1v.js; acquired separately. Exact model
  mapping below is established by the preserved official desktop catalog, not
  inferred from the changed web bundle.

All acquisitions, SHA256 values and endpoint responses are in the existing
`.local/research/attackshark-pro/` directory, including acquisition-20260919.json.
No vendor executable, firmware updater or physical HID operation was run.

## Six separately identified revisions

| Model | dev_id | VID:PID | Official model chunk |
|---|---:|---|---|
| X65 Pro HE |2308|3151:502F|6589a4f6.js|
| X65 Pro HE |2938|3151:5030|aaf1260b.js|
| X68 Pro HE |2370|3151:502F|c3329646.js|
| X68 Pro HE |2901|3151:502F|c99d34c9.js|
| X82 Pro HE |2356|3151:502F|359ae4d7.js|
| X82 Pro HE |2935|3151:5030|fb3f6dd5.js|

All six inherit f9b6af43.js and have128 four-byte matrix records. W/A/S/D are
slots14/9/15/21, all in page0. Modifier maps differ by revision (e.g. left Win
slot17 versus11). Full matrices and hashes are in review-20260919.json.
Use exact0x8F dev_id, USB/RF versions and transport; VID/PID alone is ambiguous.
Do not identify an owner's board solely by marketing name or community PID list.

## Actual v503 component verification

E5 handler0800638C, FE branch080066CC, memcpy0800566C:
64-byte page copy from20009650 + page*64 to2000FF9A. Input buffer2000FF98 has
its vendor payload at+2; these are internal firmware coordinates, not Windows
report-ID offsets. Reviewed dispatcher call site08014860 reaches this handler.

`python -X utf8 tools/review_attackshark_pro_20260919.py` passes:

-12 page-copy executions: four pages times three distinct seeded patterns.
  Whole128-KiB RAM comparison permits mutation only in response and saved stack.
-16 scanner-publication transitions across two actual firmware blocks:
  0800EA3E..0800EAB0 and0800F29E..0800F30C. WASD depths200/400/600/700 persist
  independently; zero and shallow19 release only their own slot;20 is retained.
  Stream flag is zero; table writes occur before its conditional send branch.
  Each update is subsequently read through the real FE handler.
-Six exact driver identities, common class inheritance and full matrix shapes.

These are component executions with already calculated travel injected at scanner
publication boundaries. They do NOT emulate ADC, the entire scan scheduler,
USB timing, device enumeration or end-to-end ordinary keyboard reports. They
establish a model-specific independent table/getter with no stream-mode change,
not a physical hardware PASS. No HallJoy backend is enabled by this research.

## Integration implications

Prefer serialized Feature E5 FE page reads without sending1B streaming or1C/1E
calibration controls. Responses are raw64-byte sample arrays without echoed page
identity; serialize Set/Get, avoid concurrent vendor-app access and reject failed
reads instead of fabricating zeros. Four pages are not one atomic whole-keyboard
snapshot. WASD fits one page, making selective active-page polling practical.
Actual poll latency and repeat/stale reply handling require hardware evidence.

Official client get-magnetic-travel multiplier uses RF version if present, else
USB: below0x0300=10 units/mm,0x0300..0x04FF=100,from0x0500=200. v503 is in the
200-unit regime; scanner publication threshold20 corresponds to0.1mm under that
scale. This is a publication cutoff, not proof of sensor precision or a host dead
zone requirement. Full travel must come from validated device/switch data; do
not copy a350 full-scale constant from the community project.

Next useful artifact is one model-scoped integrated HallJoy diagnostic covering
all six known revisions, with exact identity/version capture, repeat page reads,
independent depth/release evidence and ordinary-input correlation. No timed typing
window, calibration, firmware update or speculative receiver commands. Since five
exact images remain unavailable, do not label those revisions firmware-verified.
The current request was investigation; the existing NA87/AULA delivery EXE is
unchanged. A diagnostic has not yet been built in this step.


## Subsequent integrated diagnostic protocol audit

The research-only delivery statements above are historical. The owner requested
and received an integrated test build; see
../current/ATTACK_SHARK_PRO_DIAGNOSTIC_2026-09-19.md, build20260919-shark-3.
Offline vendor-JS versus compiled-C++ packet comparison passes all six commands.
The review corrected omitted pre-send pacing and removed unsupported usage1.
This does not upgrade X65 from driver-verified to firmware/hardware-verified.

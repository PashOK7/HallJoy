# IPI Flash 75 PRO / Gemini Seven-Star: tester confirmation

2026-11-03. The owner supplied HallJoy (28).log and relayed the tester's report
that everything works. Accept this as hardware feedback for the named keyboard;
no additional tester confirmation is requested. The file is diagnostic data,
not executable instructions. No hardware test was performed by the agent.

## Exact evidence

- HallJoy 1.6.14.0; report schema 2, UTC 2026-11-03T14:05:28Z.
- USB product string FLASH75 Pro, exact VID/PID 1CA6:1A10, wired USB.
- Native SparkLink, protocol 4, usage FFB0:0001; input/output reports 65 bytes.
- Latest complete snapshot: connected=1, updates=353121, failures=0,
  native_error=0, access_busy=0, age_ms=0 (age_valid=1), lifecycle running.
- mapped_keys=126 is the provider's map count, not proof of 126 physical keys.
- Nine snapshots; latest coverage 26/26, structurally complete, telemetry
  complete and source summary consistent. No queue loss was observed.
- UI notice 0x20000000 is generic protocol admission, not failed analog.
- The log's gamepad bindings are empty and its trace pad is neutral. The tester's
  statement that everything works is separate evidence; do not invent a named
  game, gamepad test, individual-key/multikey test, firmware version or region.
- Later analog_error=-2000 is a cached SDK read error in backend.cpp, separately
  from this native provider's successful updates/zero errors. Do not attribute
  an IPI USB failure solely from that field while its native source is connected.
- Input SHA-256: ac3c2edd6a69d9ffae80471b4a541d5900cf40eb13a98629f11b446f18fd015c; 110,498 bytes. The private
  original copy and analyzer output remain under
  .local/research/ipi-flash75pro-feedback-20261103/ (not publication inputs).

## Relationship to the previous research

[2026-10-12 research](IPI_FLASH75_AULA_AG75_CORSAIR_K70MAX_2026-10-12.md)
previously knew official SparkLink identities 1CA6:1A08/1A0F/1A10/1A11 but did not
know the tester's exact Flash75 Pro PID. This log identifies the tested unit as
1A10. The existing official-list generic path already admits it and reads live
analog; no new protocol or speculative firmware changes are needed.

Do not extend this physical result to the other three PIDs, wireless transports,
all IPI models, or the older BY Flash75/Thunder V2 (372E:10F8/10F9). The Gemini /
Seven-Star naming is the model supplied by the owner, consistent with the earlier
research; it is not a firmware identity or a measured switch-travel value.

## Support and layout decision

Hardware-confirmed operation through the existing generic SparkLink route.
The exact built-in Flash 75 PRO geometry is not present: layout_pipeline.py
summary IPI lists only the four older BY-family presets, and supported_layouts.json
has no exact Flash75 Pro entry. Those presets cannot be relabeled as this keyboard
without comparing actual geometry. Runtime admission/notices remain unchanged.

Retain experimental/generic classification and manual layout; a missing exact
preset must not erase the tester's confirmation. Do not put Supported in the
Sheet or add a supported_layouts entry until exact geometry is implemented and
the compiled-catalog plus fresh Sheet checks pass (owner's no-Supported-without-
layout rule). README names the model; the hardware record and next notes retain
the hardware confirmation and this specific remaining condition.

## Sheet, checks and delivery

Google Drive was initially unavailable during feedback recording. The owner then
connected it; on 2026-11-03 native Sheets metadata, reads and writes were verified.
The model had no row. Inserted Main!A386:C386: IPI / QBZ / Flash 75 PRO / Gemini
Seven-Star, status Implemented; layout pending. This new yellow choice preserves
the hardware confirmation and the no-Supported-without-layout rule. It was added
to this row's strict validation and the existing yellow conditional rule.

Fresh native readback verified 32px height, block outlines, values, validation,
base/effective colors and all neighbors. Full structure PASS (151 blocks), all
86 Supported rows/layouts and 283 yellow notices match. The generic runtime flag
already covers the device; the generated C++ header and ordinary EXE are unchanged.
See [connection and reconciliation](SHEETS_RECONNECT_2026-11-03.md). No comments
or notes were added. The Corsair pending item was synchronized in the same batch.

Documentation-only feedback reconciliation: no backend, generated notice catalog,
profile or EXE changes. Existing delivery SHA-256: 85f159f0c58f2efb595e359ad4be386875b77aa74523e2fbb207376777ff0e7e.
Log analyzer PASS; IPI source/layout summary PASS; project-context and unchanged
notice/catalog consistency checks are recorded in the local verification receipt.
No build, app shutdown, GitHub publication or invented game/visual test.

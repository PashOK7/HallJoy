> Owner decision, 2026-09-20: Further ATTACK SHARK investigation/development is paused until a tester is available. Existing implementation and EXE are retained. Remaining investigations below are deferred, not an active work plan.

# ATTACK SHARK switch selection and RAM lookup initialization — 2026-09-20

## Result

This follow-up supersedes the unresolved X82 startup and X68 selector statements
in ATTACK_SHARK_DEPTH_RANGE_2026-09-20.md. It does not establish physical bottom-out
travel, validate other firmware revisions, or change the delivered EXE. Custom
remap readback remains deferred.

X82 Pro dev2935 v503 initializes seven identical travel tables with raw ceiling
730. X68 MAX dev2755 v504 has a real switch-type-to-bank conversion: some types
select 350-ceiling tables instead of the default 700-ceiling table. The examined
publication blocks and E5 FE handlers preserve those values without rescaling.
These are component results, not a fully emulated ADC-to-host execution.

## X82 Pro: application startup, not bootloader startup

The image contains two startup contexts. The initial reset vector belongs to the
bootloader. Its jump at08001358 targets application vectors at08005200 (literal
08001390); the application reset vector is08005501. Application startup calls its
own scatter loader at080053E8. The compressed-data descriptor at0801CEDC expands
0801CEFC into20000000 for0x7458 bytes using0800541C; the following descriptor clears
RAM starting20007458. Stopping at080054AC avoids hardware initialization.

This populates seven4096-byte tables beginning2000000C. All seven have identical
SHA256 `53fdf0f79a6c12fe5a91bf96a18bca537670e15c58a5c2c8827672d8eed95bfe`,
index bound2040 and saturation730. The actual lookup block0800E2A0–0800E2DA was
executed for indices0–2049 in every bank. Both scanner publication branches pass
730 unchanged into the table returned by E5 FE, with streaming disabled.

At the vendor version-dependent200 units/mm,730 means3.65 mm numerically. This is
an algorithmic ceiling, not proof that the installed switches physically reach
3.65 mm. Current HallJoy reaches100% at700/3.5 mm and clamps larger values. Raising
that endpoint blindly to730 would make a keyboard reaching700 stop at95.9%.
Therefore this audit does not change normalization to the lookup ceiling.

## X68 MAX: exact switch selector

The initialization loop0800F4E2–0800F56C reads126 switch bytes at
20001180+0x41DF and writes126 active bank bytes at20001180+0x425D.
E5 FC reads the same source switch bytes, in two64-byte pages. It does not
return the bank number directly. Executing the selector for all256 byte values
at every126 position produces:

| Configured switch-type byte | Selected bank | Lookup ceiling |
|---|---:|---:|
|0,7 and values12–255 |0 |700 |
|1,2,3,9 |1 |350 |
|5,10 |2 |350 |
|6 |3 |350 |
|8 |5 |0; zero-filled region |
|4,11 |6 |781 in a nonmonotonic region with index bound1; not a valid travel table |

Bank4 contains a350-ceiling table but this selector never chooses it. The pinned
vendor client exports these switch-type codes through enum d6/ax and marks
model2755 as switch-replaceable, without a per-model supportedSwitchTypes list.
That is not proof that every global enum choice is physically usable on X68 MAX.

Both publication branches preserve350 and700 through the actual FE read handler;
no multiplication occurs in those blocks. Combined with the earlier real lookup
checks, this strengthens the conditional risk: a350-bank path can reach only50%
under current700 normalization. It does not prove which configuration ships on a
real device, nor rule out other writes/branches in the full scanner lifecycle.
The anomalous banks must not be treated as valid endpoints or silently repaired
by guessing from their final words.

## Verification and implementation boundary

Run `python tools/review_attackshark_bank_init_20260920.py`:

- 32,256 switch selection cases (256 values x126 positions).
- 2 FC pages with RAM side-effect checks.
- 14,350 actual X82 lookup cases after executing application decompression.
- 30 X68 MAX and72 X82 publication/read transitions across page boundaries,
 including releases, with streaming disabled and getter RAM side-effect checks.

All pass. Firmware SHA256s are asserted. Inputs and results are recorded in
[bank initialization evidence](../research/attackshark-bank-init-20260920.json).
No hardware access, calibration commands or device-setting writes are performed.
The test injects normalized sensor indices and calculated values at component
boundaries; it must not be described as complete ADC/USB scheduling emulation.

Before a production per-key scale exception, establish the exact X68 firmware's
complete bank lifecycle and the intended physical travel of the affected switch
configuration. A focused hardware log should contain exact identity, USB/RF
versions, FC switch bytes and independent observed depths; maxima alone must not
be used as automatic calibration. Keep any exception restricted to proven exact
identity/version/configuration. No blanket change for all37 family profiles.

Ordinary EXE unchanged, SHA256:
`26c4ff4733fc258ad721abfde99d6709f49d54fd1f5f1a9f04e8e4025c6a5cd4`.

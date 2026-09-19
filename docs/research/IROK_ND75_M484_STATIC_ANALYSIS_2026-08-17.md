# IROK ND75 M484 static protocol analysis

> Исправление 2026-09-13: 0x29 — opcode входа Witmod SDK; фактический HID
> opcode — 0x21. Полный scanner и все serializer entry points теперь проверены
> offline. См. [IROK_WITMOD_OFFLINE_CLOSURE_2026-09-13.md](IROK_WITMOD_OFFLINE_CLOSURE_2026-09-13.md).


Date: 2026-08-17  
Status: historical experimental analysis; 2026-09-13 event-loss finding below
blocks treating the subscription stream as reliable multi-key input.

## Sources and provenance

The implementation was derived without executing vendor firmware tools and without
connecting an ND75. The primary sources were the official, Authenticode-signed IROK
desktop package and firmware updater:

- `ND75_Installer_V2.03.07.exe`;
- `ND75_Firmware_Upgrade_V12.exe`;
- extracted `IROK Keyboad Engine.exe` and `witmodSdk.dll`;
- raw updater image `ND75_V12_flash_512K.bin`, SHA-256
  `A5168399CACA4BBD2C04A1E988F478265364577A46994199A73848ABF0AB735F`.

The signed updater publisher is Beijing Juewei, the company behind IROK. The
desktop package contains a Qt v1 resource bundle. Its official
`KeyInfo_X86HERGB.config` has SHA-256
`2DD9CB7950CE9F9489383794C89973F6A6F9EB0AB024B9DA416A27249779A7CA`.
Only the derived matrix-to-HID table is included in HallJoy; no vendor executable,
firmware, image or configuration file is redistributed.

## Device identity

The runtime configuration endpoint has this exact fingerprint:

- USB VID/PID `0416:7372`;
- HID usage page/usage `FF1B:0091`;
- report ID `01`;
- 64-byte Windows input and output reports;
- configuration interface `MI_02`;
- firmware identity `M484,01,KB,ABT,X86HERGB,V1.00.09` in the analyzed image.

The firmware's HID descriptor declares 63-byte input/output payloads after the
report ID. The SDK catalog independently identifies controller `M484`, HID name
`X86HERGB`, marketing name `ND75`, and 82 controls.

## Host and device messages

All offsets below include the Windows report-ID byte.

| Purpose | Direction | Required bytes |
|---|---|---|
| identity query | host to device | `[0]=01 [1]=0D` |
| capability/sensitivity query | host to device | `[0]=01 [1]=29 [5]=18 [6]=04` |
| subscribe | host to device | `[0]=01 [1]=29 [5]=18 [6]=02 [7..28]=column masks` |
| unsubscribe | host to device | `[0]=01 [1]=29 [5]=18 [6]=03` |
| capability response | device to host | `[0]=01 [1]=21 [6]=04 [7]=value` |
| live travel event | device to host | `[0]=01 [1]=21 [6]=01 [7]=row [8]=column [9]=travel` |

The important family distinction is asymmetric opcode use: the M484 host sends
analogue control requests under `29`, while the device responds under `21`.
HallJoy's W669 backend sends `21` and reads a 16-bit live value; therefore ND75 is
not admitted by adding a PID or product alias to W669.

The subscription mask is 22 bytes, one per matrix column; bits zero through five
select matrix rows. Only `0D`, read-only `29/18/04`, RAM subscription `29/18/02`,
and matching unsubscribe `29/18/03` are allowed by the experimental backend.
No configuration, calibration, key-map write or firmware command is present.

## Matrix and scale

`EveryKeyInfo_Row01` through `EveryKeyInfo_Row06` in the official profile provide
the exact `6x22` row-major sensor map. It contains 81 publishable positions,
including the internal Fn position represented by HallJoy pseudo-usage `FA`.
The advertised 82-control count includes the rotary encoder, which is not a
keyboard HID usage and is not published as analogue input.

The derived map has FNV-1a-64 `4BEF9FCF48E36C37`. Its subscription mask is:

```text
3F 26 3F 1F 1F 1F 3E 1F 1F 3F 3F 1F 3F 21 3F 2F 00 00 00 00 00 00
```

Static artifacts establish a one-byte live value and a 4.0 mm physical travel.
The diagnostic normalization therefore uses the explicit, still-unverified
hypothesis that the byte is tenths of a millimetre, nominally `0..40`. It clamps
only at the public `0..1000` boundary and counts every raw value above 40 as
`out_of_range`; this makes a wrong scale immediately visible in the owner's log
instead of silently treating the hypothesis as verified.

## Admission and remaining boundary

An interface is claimed only after all of these checks pass:

1. exact `0416:7372` VID/PID;
2. exact `FF1B:0091`, 64-byte report shape;
3. valid `0D` CSV with controller `M484` and product `X86HERGB`;
4. valid asymmetric `21/04` response to read-only `29/18/04`;
5. hash-pinned 81-position official map.

Portable parser tests and static lifecycle audits can verify those invariants
without hardware. They cannot establish physical event polarity, real maximum,
release-to-zero behavior, multi-key delivery, reconnect behavior, or the exact
identity returned by the owner's firmware revision. Those are the explicit goals
of the isolated owner build and must pass before production support is enabled.

## 2026-09-13: producer/scheduler tracing and actual-code emulation

The exact same image was revisited. Important new restriction: this stream is
not a queued sequence of every subscribed key change.

- Scanner at 0x3190 traverses physical scan positions, mapping to logical row
  and column. At 0x34d2..0x34f8 depth conversion is capped at **40**. This
  strengthens the earlier 0..40 range hypothesis for this image; physical
  calibration accuracy remains outside static verification.
- At 0x3588..0x35ae the scanner compares and updates the per-key depth cache
  at `0x20003fec + 0x124 + row*22 + column`, with direction/deadband gates.
- 0x35b2..0x35c6 checks the column mask at base+0x10e.
- 0x35c8..0x35e0 sets one pending bit (22) in RAM 0x20004a20 and writes
  **one shared row/column pair**, base+0x10c/base+0x10d. No event is queued here.
- 0xf9d6 reads that shared pair and its cached depth into report bytes 7/8/9.
- Scheduler 0xfafc selects handler through table 0x136f0; entry 22 is
  0xf9d7 (Thumb). After the handler returns 1 it clears bit 22. Busy report
  state at 0x200041a0+0xb8 can defer the scheduler.

` .local/research/irok-na87/nd75_event_emulation.py` executes the actual
producer fragment 0x35ae..0x35e4 and complete scheduler 0xfafc with Unicorn
2.1.4, pinned image hash, synthetic RAM and no peripherals. Test results:

1. Accepted changes (row1,col2)=17 then (row3,col4)=29 before service.
2. Both cache entries updated, but only report
   `01 21 00 00 00 03 01 03 04 1D` emitted; pending bit cleared.
3. A zero release for the first key followed by another key's change likewise
   emitted only the second key. **PASS: loss reproduced in actual machine code.**

This is a component-level emulation, not a full firmware/peripheral timing test.
It proves overwrite for this scheduling sequence, not how often hardware meets
the sequence. Nevertheless there is no basis to promise lossless multi-key or
release delivery. The unchanged-depth gate means the lost value is not
automatically resent simply because it remains held.

Subscription at 0xf648 copies 22 masks; unsubscribe at 0xf674 clears them.
Neither path invalidates the depth cache or explicitly requests its current
value, so rotating a one-key subscription is **not** an established snapshot
polling workaround. Calibration/configuration commands must not be substituted.

The scanner continues into ordinary key-processing branches after setting the
analog event (0x35e4 onward); no unconditional "analog subscription disables
letters" branch was found at this point. This does not establish end-to-end
keyboard report scheduling or absence of stalls.

Do not enable the ND75 backend in production on the strength of the old report.
Next useful work is a separate read/snapshot path or a different firmware;
ordinary NA87 compatibility cannot be inferred from this ND75 event stream.


## Read-path follow-up after handoff

`IROK_ND75_READ_PATHS_2026-09-13.md` records actual-code tests of addressed
29/18/05 (configuration only) and internal 0x10 (six rows of key-record bytes,
not cached depth). Both are excluded as live-depth recovery paths for this image.
The wider command-table and task-scheduling audit is still incomplete.

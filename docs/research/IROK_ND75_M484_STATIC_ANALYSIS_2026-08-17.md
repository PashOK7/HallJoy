# IROK ND75 M484 static protocol analysis

Date: 2026-08-17  
Status: sufficient for an isolated experimental owner build; physical validation pending

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

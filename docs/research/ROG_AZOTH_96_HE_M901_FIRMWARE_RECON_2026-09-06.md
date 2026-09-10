# ROG Azoth 96 HE / M901 firmware reconnaissance

Date: 2026-09-06. This is a research record, not a production-support claim.

## Acquisition

The firmware was downloaded from ASUS Gear Link's official device endpoint:

`https://gearlink.asus.com/api/v1/firmware-update-tools/7184/download?os=Windows`

Gear Link identifies PID `7184` (`0x1C10`) as `ROG AZOTH 96 HE / LITE` / product
`m901`. The downloaded archive is retained outside source control under
`.cache/azoth96_m901/`.

| Artifact | Size | SHA-256 |
|---|---:|---|
| Gear Link ZIP | 989,261 bytes | `E1715C37E92C17C73AF24DBEBFD726F5F85CEE08D213C0BAA194218D1ADEC91D` |
| `M901_DEVICE_V07_00_30.bin` | 643,072 bytes | `3B62EEE83DF9ABA7D8FB5C74B5FD5A830DDDE14C832FC4C2A92E5F61B4C47C27` |
| `OMNI_RECEIVER_NRF54H_V09_00_07.bin` | 643,072 bytes | `37EAFB9A90C5BC638776830FF349957191AAA4E5DD5AB2D08732EDDF52441401` |

The package also includes `peripheral_fwu_pro.exe`, `HidInterruptHandle.dll`,
and the ASUS update GUI. Do not run the updater against an emulated or physical
keyboard in this project; acquisition and static analysis are sufficient.

## Confirmed device/update identity

`config.ini` and the Gear Link M901 device definition agree on the following:

| Field | Value |
|---|---|
| USB VID | `0B05` (ASUSTeK) |
| Wired application PID | `1C10` |
| Wired bootloader PID | `1C11` |
| RF keyboard PID | `1C12` |
| BLE PNPID | `1C13` |
| Receiver PID / bootloader PID | `1AD0` / `1AD1` |
| Keyboard vendor collection | usage page `FF00`, report ID `00`, 64-byte packets |
| Receiver vendor collection | usage page `FF02` |
| Firmware version acquired | `7.00.30` |
| Application image geometry | 628 KiB, 4 KiB pages, image starts at flash address zero |

Gear Link exposes the device as a magnetic keyboard: range 0.10--3.50 mm,
0.01 mm units, global/per-key actuation and dead-zone settings, and rapid
trigger sensitivity down to 0.01 mm. The unpacked firmware identifies Nordic
nRF54H20 application and radio cores and Zephyr/nRF Connect SDK components.

## Confirmed normal-mode analogue telemetry path

Static analysis of the acquired M901 image together with the matching official
Gear Link client establishes a distinct, non-calibration path for per-key
travel. This is the path relevant to HallJoy.

| Purpose | HID collection / report | Format | Evidence |
|---|---|---|---|
| Normal keyboard output | Generic Desktop keyboard descriptor, including NKRO | standard HID keyboard reports | M901 firmware report descriptors |
| Control | vendor usage page `FF00`, no report ID, 64-byte IN/OUT | commands and replies | M901 firmware descriptor and Gear Link device definition |
| Event telemetry | vendor usage page `FFC0`, report ID `03`, 20-byte input report | asynchronous keyboard events | M901 firmware descriptor and Gear Link M901 registration |

The firmware embeds the `FFC0` descriptor as `Report ID 03`, `Usage 02`,
`Report Size 8`, `Report Count 20`, Input. It coexists in the same normal
application descriptor set with both the standard keyboard report and `FF00`.
It is therefore not a replacement USB personality for the keyboard.

Gear Link's keyboard protocol implementation has the following exact command
and event decoder:

```text
Enable travel notifications for all keys:
  FF00 OUT, report ID 0, 64 bytes
  51 61 00 00 00 00 ... 00

Enable travel notifications for one key:
  FF00 OUT, report ID 0, 64 bytes
  51 61 00 00 <firmware-key low> <firmware-key high> 00 ... 00

Travel event:
  FFC0 IN, report ID 3, up to 20 bytes
  7E <firmware-key low> <firmware-key high> <travel low> <travel high> ...
```

`travel` is decoded little-endian by the ASUS client as an unsigned 16-bit
value. The client labels event `7E` `NotifyKeyTravel` / `onKeyTravelNotify`;
it maps the preceding 16-bit firmware key code to a keyboard key and forwards
the value unchanged. `FF00: 51 61` is independently named
`setKeyTravelNotify`.

This is explicitly separate from switch calibration: the same client invokes
calibration as `FF00: 80 26 00 00 <firmware-key low> <firmware-key high> ...`
and waits for a calibration reply. HallJoy must not send `80 26` and does not
need it to receive `7E` travel events.

The evidence supports simultaneous normal letters and analogue telemetry:
the ordinary keyboard collection stays enumerated while an independent `FFC0`
input endpoint emits travel notifications. It does not yet prove the actual
event rate, whether values are emitted for every sensor sample or only upon
change, the value-to-millimetre scale, or the complete M901 firmware-key map.
Those require a passive physical-device trace. The compact, event-only
five-byte payload is nevertheless a promising high-performance route: it does
not require polling every key or suppressing normal keyboard HID output.

## Comparison with HallJoy routes

No HallJoy native route or embedded UAP family matches the M901 wire identity:

| Candidate | Similarity | Why it is not reusable as an M901 reader |
|---|---|---|
| Razer UAP | Both are configurable analogue keyboards | Razer VID/PID/report admission and vendor protocol are unrelated; HallJoy's Razer path is also not a generic Hall HID reader. |
| Keychron/Lemokey UAP | Per-key configurable Hall behaviour | Their custom/official transaction formats and layouts are unrelated to ASUS `FF00` 64-byte HID. |
| MADLIONS/ATK native | Vendor HID and some Hall-capable keyboards | Their A0 / `96` protocols, identities and maps are distinct. |
| Aula, SparkLink, Sayo, Addressed native | Existing safe HID discovery/lifecycle patterns | They are independent protocol families with different requests, data framing and proof rules. |
| ASUS ROG Strix Scope II 96 Wireless (X901) | Gear Link records the same `peripheral_fwu_pro` firmware-update family | This proves only shared firmware-update transport. It does **not** prove that X901 and M901 publish analogue key depth by the same application protocol. |

Thus no existing backend can be enabled by adding `0B05:1C10`. Doing so would
violate the project's exact-interface capability-proof rule.

## Feasibility verdict

Adding production support is **not** a low-effort protocol alias. The available
image and official update tooling do establish a strong, safe starting point:
exact identity, vendor HID collection, application/bootloader separation,
packet size and MCU/SDK family are known. They now establish a normal-mode
runtime command and event prefix, but not its event rate, key-index map, unit
conversion, release semantics, stale-data policy or reconnect behaviour.

The viable next package is a read-only M901 protocol reconnaissance harness:

The dedicated diagnostic scaffold and its tester procedure are recorded in
[ROG_AZOTH_96_HE_M901_TEST_DIAGNOSTIC_PLAN_2026-09-06.md](ROG_AZOTH_96_HE_M901_TEST_DIAGNOSTIC_PLAN_2026-09-06.md).
It is intentionally uncompiled and undistributed until the tester is ready.

1. add an isolated, test-only M901 transport fixture that models `FF00` command
   `51 61` and `FFC0` report ID `03`; do not register a production device yet;
2. capture the physical device non-destructively in normal typing mode while
   `51 61` is active, proving letters and `7E` travel happen concurrently;
3. validate graded press, release, rollover, idle, disconnect and reconnect,
   then derive key mapping, scale and stale-value behavior;
4. measure wired normal and Game Mode traffic, including CPU cost and report
   cadence, before selecting HallJoy coalescing/backpressure rules;
5. only then design a new isolated native provider with capability proof and
   negative tests.

Neither the firmware updater's `peripheral_fwu_pro` command family nor the
ordinary keyboard HID interface may be used to infer analogue values.

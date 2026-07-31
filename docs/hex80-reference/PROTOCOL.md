# Hex80 Protocol

## 1. Device Identity

The Hex80 exposes a vendor-defined HID interface used for analog travel data.

Observed identifiers:

- Vendor ID: `0x373B`
- Product IDs: `0x1176`, `0x1177`, `0x1250`
- Usage page: `0xFF60`
- Usage: `0x61`

This is the interface to use for direct analog reads.

## 2. Transport Format

The protocol is packet-based.

For hidapi-style writes:

```text
00 [128-byte payload]
```

Where:

- byte `0` is the report ID
- the actual command begins at byte `1`

The payload itself is built in a 128-byte buffer, with unused bytes left as zero.

## 3. Command Prefix

The protocol uses two high-level operations:

```text
0x02 = GET keyboard value
0x03 = SET keyboard value
```

The vendor custom command group is:

```text
0x96
```

So the first three payload bytes usually look like:

```text
[get_or_set] [0x96] [subcommand]
```

## 4. Important Subcommands

```text
0x18 = calibration start
0x19 = calibration finish
0x1C = ADC / travel / status buffer
0x24 = travel info
```

## 5. Safe Startup Sequence

Recommended normal-mode startup:

1. Send calibration finish once:

```text
03 96 19 00 00 00 00 00 ...
```

2. Read travel max:

```text
02 96 24 00 00 00 00 00 ...
```

3. Start polling the travel buffer:

```text
02 96 1C 00 00 [offset_hi] [offset_lo] [size] ...
```

Why this sequence matters:

- `03 96 18` puts the keyboard into calibration mode.
- calibration mode is useful for reverse engineering, but not needed for normal analog polling
- `03 96 19` is a safe recovery packet that leaves calibration mode if the board is already stuck there

## 6. Reading travel_max

Request:

```text
02 96 24 00 00 00 00 00 ...
```

Expected response prefix:

```text
02 96 24 [travel_max_hi] [travel_max_lo] ...
```

Observed value:

```text
travel_max = 3300
```

If the read fails, `3300` is a reasonable fallback default.

## 7. Polling the Travel Buffer

The matrix is read in chunks.

Request format:

```text
02 96 1C 00 00 [offset_hi] [offset_lo] [size] ...
```

Recommended polling pattern:

```text
offset = 0, 4, 8, 12, ... 100
size   = up to 4
```

There are `104` total slots, so the last request usually covers the tail end of the matrix.

## 8. Travel Buffer Response Layout

Expected response prefix:

```text
02 96 1C 00 00 [offset_hi] [offset_lo] [size] [entries...]
```

Important header fields:

- bytes `0..2` identify the command family
- bytes `5..6` are the returned offset
- byte `7` is the returned entry count
- entry data begins at byte `8`

Each entry is `5` bytes:

```text
adc_hi adc_lo travel_hi travel_lo status
```

So for entry `n`:

```text
adc    = u16_be(entry[0:2])
travel = u16_be(entry[2:4])
status = entry[4]
```

The useful analog value is `travel`.

## 9. Depth Normalization

The recommended normalization is:

```text
depth = clamp((travel - deadzone) / (travel_max - deadzone), 0.0, 1.0)
```

Using:

```text
deadzone = 8
travel_max = 3300
```

Equivalent behavior:

- if `travel <= 8`, treat the key as released
- if `travel >= travel_max`, treat the key as fully pressed
- otherwise scale linearly

Optional practical threshold:

```text
if depth <= 0.002:
    treat as released
```

## 10. Converting Slots to Keys

The buffer returns matrix slots, not key names directly.

The current working map assumes:

```text
row = index // 17
col = index % 17
```

That covers the visible `6 x 17` keyboard grid:

```text
102 visible grid slots
```

And two trailing extra slots:

```text
102, 103
```

Those final two slots should currently be treated as unused.

The full mapping is listed in `MATRIX.md`.

## 11. Minimal Polling Algorithm

```text
open Hex80 analog HID interface
send 03 96 19 once
send 02 96 24 once and parse travel_max
repeat:
    for offset in range(0, 104, 4):
        send 02 96 1C with that offset and size
        parse each 5-byte entry
        index = returned_offset + slot
        travel = u16_be(entry[2:4])
        depth = normalized travel
        map index to key name
```

## 12. Behavior Notes

- The keyboard can answer the travel-buffer poll while remaining in normal typing mode.
- That is the preferred mode for normal use.
- The calibration commands are still useful for research, but they are not required for live analog reads.
- The `status` and `adc` fields are available for further reverse engineering if deeper semantics are ever needed.

# MCHOSE Ace 68 versus every active analogue route

Assessed 2026-08-28 from the current HallJoy source, its embedded Universal
Analog Plugin (UAP), MCHOSE M HUB web assets, and the verified Ace 68 I image.
This is a read-only analysis; no command was sent to the keyboard.

## MCHOSE baseline

- identity: `VID 41E4 / PID 2114`, model key `Ace 68 I`;
- firmware USB topology: boot keyboard, NKRO keyboard and a separate 64-byte
  M HUB configuration IN/OUT interface;
- transport: firmware-native `55`/`5F` packets with opcode, optional XOR,
  length and additive checksum. This is distinct from the public V3 framing;
- analogue candidate: boot-enabled asynchronous 64-byte `A0` events on
  `EP82`; `BE16[4..5]` is a Hall-derived coordinate clamped to `0..1600`;
- unresolved: `A0[1..3]` key identity mapping, remaining fields, and optional
  command handlers whose jump table is not present in the update image.

The MCHOSE frame is not wire-compatible with any route below. Its actual HID
descriptor is Generic Desktop/undefined; `FF00:0001` must not be used as a
descriptor-level compatibility claim.

## HallJoy native backends

| Route | How it obtains travel | Why Ace 68 is not this route |
|---|---|---|
| MAD68 Pro R | Reversible `A9 -> A8 -> A9` activation, then asynchronous 64-byte `A0` stream; `VID 373B` | Closest architecture: MCHOSE also has `A0`, 72 slots and 0..1600 normalization. But VID, descriptor map, handler table, activation and packet fields are unproven and cannot be reused. |
| ATK Hex80 | `FF60:0061` polling with `02 96 24` and `02 96 1C` proof/reads | Different descriptor, request family and `02 96` protocol. |
| Addressed Analog / QBZ | `FF60:0061`, map `09 83 00`, per-key travel `09 94 02` | Different usage page, request family, key addressing and checksum contract. |
| Aula WIN 60 HE MAX / compatible 6x21 | 64-byte `5C` frames: exact `01`, `12`, `23`, `2B` proofs then travel-matrix polling | Different vendor page and frame header. |
| Aula W669 and Redragon family | `FF1B:0091`; read-only `0D` identity/key map and `21` analogue subscription/event stream | Different page, report ID and command family. |
| Irok ND75 experimental | `0D` identity plus `29` host-analogue subscription / `21` device analogue events | Different identity and protocol family. |
| Sayo O3C | Audited `VID 8089` depth command `22` | Different VID and depth protocol. |
| SparkLink / XD | Its broad prefilter may see some generic HID interfaces, but it must prove 64-byte `01 02`, `03 01`, `04 03 01` responses before claim | MCHOSE's Generic-Desktop/undefined descriptor and native `55`/`5F` parser are different, so the proof must fail closed. |
| DrunkDeer diagnostic | 64-byte `04 B6` request followed by three `04 B7` matrix chunks | Different VID and fixed three-chunk matrix contract. |

## Embedded Universal Analog Plugin

UAP dispatches only the following vendor families: Wooting (`31E3`/`03EB`),
Razer (`1532`), DrunkDeer (`352D`), Keychron (`3434`), Lemokey (`362D`), NuPhy
(`19F5`), and Madlions (`373B`). It has no `41E4` branch, so it cannot even
instantiate an Ace 68 reader.

| UAP family | Activation/read model | Comparison with Ace 68 |
|---|---|---|
| Wooting | Dedicated analogue HID report pages (`FF54`/`FF53`) | MCHOSE's actual descriptor is Generic Desktop/undefined and uses a different report contract. |
| Razer | Synapse enables analogue reports, then input reports carry key/value records | No Razer report IDs or vendor ID. |
| DrunkDeer | Active poll `04 B6 03 01`, three matrix responses | No shared VID or packet family. |
| Keychron / Lemokey | Active polling: `A9 01` version; `A9 31` whole realtime buffer when supported, else `A9 30` per key | This is the nearest *functional* analogue polling design, but not a byte-compatible MCHOSE protocol. |
| NuPhy | Asynchronous `A0` key reports, values normalized from 800/1600 | MCHOSE now has a demonstrated static `A0` builder, but a shared marker/range is not enough to share a backend or a key map. |
| Madlions | Active 33-byte `02 96 1C` four-key transaction chunks | Different VID, usage page, report length and command family. |

## Result and correct next stage

There is no existing HallJoy or UAP protocol to reuse byte-for-byte. The
appropriate new route is a MCHOSE-specific passive `EP82` reader, but only
after a physical capture validates boot-time `A0` traffic and maps bytes
`1..3` to keys. Do not send MAD68 `A8/A9` or command families from other
brands to this keyboard.

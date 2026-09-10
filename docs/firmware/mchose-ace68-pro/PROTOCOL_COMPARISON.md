# MCHOSE Ace 68 Pro: protocol comparison boundary

This comparison is intentionally about evidence, not similarity.  The target
is the verified `41E4:2116` Ace 68 Pro application image, not Ace 68 I
(`41E4:2114`) and not a device whose web UI happens to display the same short
model name.

## Native HallJoy backends

| Backend / family | Mechanism proved for that family | Relation to Pro |
| --- | --- | --- |
| MAD68 / Ace 68 I | 64-byte `55` / `5F` request framing; `A0` stream and reversible `A8` / `A9` control are defined for the audited `373B:1109` MAD68 target and separately recovered for Ace 68 I. | Closest historical framing match only. M HUB does prove Pro `A8` / `A9` / `B1`, but solely as calibration controls; it does not prove the MAD68/Ace I `A0` live-stream meaning for Pro. |
| ATK x QK Hex80 | 128-byte custom `96` polling protocol. | Different report length, identity and framing. No transfer value. |
| Addressed `09/94/02` | Addressed request/response polling, with capability probing before ownership. | No matching ID or packet parser in Pro. |
| AULA WIN60 HE | 64-byte report-ID-1 protocol; `0D` identity and `21` analogue subscription/events. | Different vendor usage page (`FFA0`) and command family. The fact that it has a reversible subscribe command is a testing pattern, not a Pro opcode candidate. |
| AULA W669 | Read-only identity/capability/travel polling. | Different vendor and framing. |
| iRok ND75 | Host `29`, device `21`, and channel `18` analogue commands. | Different identity and command family. |
| DrunkDeer | Report-ID-4 `B6` tracking request / `B7` response. | Different VID and report layout. |
| SparkLink | Its own row/matrix transport and polling control. | No common VID, report descriptor, framing, or function signature with Pro. It is not evidence for a MCHOSE command. |
| Sayo O3C | Sayo depth protocol. | No common transport or identity. |

The backend catalog also contains an Ace 68 diagnostic build. Its PID allow-list
is deliberately `2114`, so the tester's `2116` log performed no USB writes.
That is a model mismatch, not a negative analogue result for Pro.

## Universal Analog Plugin

The bundled UAP identifies Razer, NuPhy, DrunkDeer, Keychron/Lemokey,
Madlions, and optionally Wooting devices. It has no `41E4` MCHOSE identity and
no Ace 68 Pro handler. Therefore UAP supplies neither a Pro command nor a
claim that Pro lacks analogue capability; it is simply out of scope for this
device.

## Calibration is not live analogue

The independently reverse-engineered IO Type 84 magnetic firmware is a useful
counterexample: `AA 66` / `AA 67` enable/disable a simulation-test state, and
its raw `55 FB` coordinate packets are gated by that state. That proves why a
packet containing Hall values is insufficient evidence of a usable runtime
stream. No corresponding state gate, `FB` producer, `66`, or `67` discriminator
has been recovered from the Pro image.

Conversely, the AULA WIN60 HE proof chain contains a reversible subscription,
a scanner RAM mask, and a normal-mode event producer. That is the standard a
Pro path must meet before HallJoy treats it as gameplay analogue input.

## What remains a candidate and what does not

- Candidate for investigation: a Pro-specific `55`/`5F` command discovered
  from its runtime dispatch table or captured from M HUB, followed by a
  normal-mode response correlated with key movement.
- Not a candidate merely from resemblance: MAD68/Ace I live-stream `A0`; IO
  `66`, `67`, `FB`; AULA `21`; iRok `29`; DrunkDeer `B6`; Hex80 `96`; or any
  SparkLink/Sayo packet. Pro `A8`, `A9`, and `B1` are known calibration
  controls, not analogue candidates.

The missing evidence is exact host-to-device and device-to-host traffic for
this PID, or a complete flash/RAM image containing the resolved opcode table.
Until then it is technically incorrect to identify any command as one that
can enable normal-mode analogue on Ace 68 Pro.

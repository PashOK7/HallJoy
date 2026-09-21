# Layout/support inventory correction - 2026-09-19

Scope: local ordinary-build backend registry and build definitions, UAP device
recognition, exact native profiles, README and the compiled-layout inventory.
This is not a survey of every keyboard on the market or hardware validation.

## Superseded by integration

The three GravaStar gaps below are resolved by [the layout integration](GRAVASTAR_LAYOUTS_2026-09-19.md). Historical audit findings follow.

## Confirmed omissions

| Model | Native route | Missing integration |
| --- | --- | --- |
| GravaStar Mercury V75 | SparkPlayJoy 6x21, 1CA5:2201 / 16052201 | Visual preset, automatic layout identity, README entry |
| GravaStar Mercury V75 Pro | SparkPlayJoy 6x21, 1CA5:2202 / 16052202 | Visual preset, automatic layout identity, README entry |
| GravaStar Mercury V75 Lite | SparkPlayJoy 6x21, 1CA2:2201 / 2E022201 | Visual preset, automatic layout identity, README entry |

All three identities are admitted in the ordinary native route. The backend
publishes a verified layout token only for WIN60 HE MAX, not these identities.
Do not assume their geometry is identical before comparing exact sources.

V75 has a physical trace proving identity, map, scale and non-zero travel.
The corrected publication gate, sustained operation and reconnect still await
final runtime validation. Pro/Lite evidence is firmware-only. See
[the detailed evidence](../research/GRAVASTAR_MERCURY_V75_FIRMWARE_STATIC_ANALYSIS_2026-08-22.md).
The protocol overview previously omitted the first physical V75 trace; corrected.

## Do not count research as ordinary gameplay support

- MCHOSE Ace 68: diagnostic-only route; Ace 68 Pro: separate firmware research.
- MADLIONS Titan68 Turbo: isolated diagnostic/experimental build gates.
- IROK NA87 Pro / ND75, ROG Azoth 96 HE, ATTACK SHARK X68 HE non-Pro:
  retained research/frozen branches, not ordinary enabled support.
- IO Type 84 Magnetic and Pwnage Zenblade 65 V2: research, not implemented
  ordinary multi-key analog support.
- MG75 V2: intentionally deferred by owner; keep absent from public README.

The reviewed named ordinary profiles outside GravaStar have model presets in
`docs/KEYBOARD_LAYOUTS.md`. Generic family admission is not an exhaustive model
list. Unverified regional variants are not established omissions.

The earlier statement that only O3C remained was incorrect: the previous
catalog review did not reconcile every native device profile with presets.
Future completion claims must compare native/UAP recognition, visual presets,
automatic identity and public support documentation separately.

No runtime source or executable changed in this audit; no hardware tests run.

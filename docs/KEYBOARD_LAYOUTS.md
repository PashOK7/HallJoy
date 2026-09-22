> Local additions, 2026-09-21: Keychron K6 HE ANSI (68), Q2 HE ANSI (66), Q4 HE ANSI (61); Wooting 80HE+ ANSI (84), ISO (85), Split ANSI (86), Split ISO (87). These seven variants are not in the published 1.6.0 EXE; they await hardware testing. Keychron requires custom firmware. See [record](current/KNOWN_PROTOCOL_ADDITIONS_2026-09-21.md).

# Built-in keyboard layouts

Catalog for the current 1.6.0 development build, audited against the actual
compiled catalog on 2026-09-20. This is not the published 1.5.3 layout inventory.
Layout availability describes geometry, not proof of analogue firmware support
or real-device testing. See the [README](../README.md#compatible-keyboards) for
support status. A shared model row merges only layouts within the same brand;
device protocols and detection identities remain independent.

Automatic layout selection requires an exact supported device/variant identity.
If that evidence is unavailable, select the layout manually. Merged aliases keep
old profile names valid; edited legacy layouts remain independent user presets.
Counts in parentheses are displayed keys, not simultaneous analogue capacity.

| Brand | Model shown in the layout picker | Variants (keys) |
|---|---|---|
| DrunkDeer | A75 Pro | Default (82) |
| DrunkDeer | G65 | ANSI (68) |
| Other | WASD Only | Default (4) |
| Other | Generic 100% | ANSI (103) |
| Keychron | K4 HE | ANSI (100), ISO (101), JIS (103) |
| Keychron | Q1 HE / Q1 HE 8K | ANSI (81), ISO (82) |
| Keychron | K2 HE / K3 HE | ANSI (84), ISO (85) |
| Keychron | Q3 HE / Q3 HE 8K | ANSI (87), ISO (88), JIS (91) |
| Keychron | Q5 HE / Q5 HE 8K | ANSI (101) |
| Keychron | K2 HE | JIS (87) |
| Keychron | K8 HE | ANSI (87), ISO (88), JIS (91) |
| Keychron | K10 HE | ANSI (104), ISO (105) |
| Keychron | Q1 HE | JIS (85) |
| Keychron | Q5 HE | ISO (102), JIS (105) |
| Keychron | Q6 HE / Q6 HE 8K | ANSI (108) |
| Keychron | Q6 HE | ISO (109), JIS (112) |
| Keychron | Q12 HE | ANSI (102), ISO (103) |
| Keychron | Q1 HE 8K | JIS (85) |
| Keychron | K3 HE | JIS (87) |
| Lemokey | P1 HE | ANSI (81), ISO (82) |
| DrunkDeer | A75 | ANSI (82), ISO (83) |
| DrunkDeer | G60 | ANSI (61) |
| DrunkDeer | G75 | ANSI (84), JIS (86) |
| Aula | WIN 60 HE Standard | ANSI (61) |
| Aula | WIN 68 HE Standard | ANSI (68) |
| Aula | WIN 60 HE MAX | ANSI (61) |
| Aula | HERO84 HE | ANSI (84) |
| Aula | KP-TE153 | ISO / vendor UK (69) |
| Redragon | K673WB-RGB-M | ANSI (80) |
| Redragon | K673RGB-M | ISO (81), ABNT2 (81) |
| Razer | Huntsman V3 Pro Mini | ANSI (61), ISO (62), JIS (65) |
| Razer | Huntsman Mini Analog | ANSI (61) |
| Razer | Huntsman V2 Analog / Huntsman V3 Pro | ANSI (104), JIS (108) |
| Razer | Huntsman V3 Pro | ISO (105) |
| Razer | Huntsman V3 Pro Tenkeyless | ANSI (84), JIS (88) |
| NuPhy | Air60 HE | ANSI (61) |
| NuPhy | Air75 HE | ANSI (83) |
| Wooting | 60HE / 60HE+ / 60HE v2 | ANSI (61), ISO (62) |
| Wooting | 80HE | ANSI (84), ISO (85), JIS (88) |
| Wooting | One | ANSI (87), ISO (88) |
| Wooting | Two / Two HE | ANSI (108), ISO (109) |
| Wooting | 60HE v2 Split | ANSI (63), ISO (64) |
| Wooting | UwU / UwU RGB | ANSI (3) |
| IROK | MG75 Max | ANSI (81) |
| IROK | MG75 Pro | ANSI (81) |
| MADLIONS | MAD60HE | ANSI (61) |
| MADLIONS | MAD68HE / MAD68R | ANSI (68) |
| MADLIONS | MAD 68 Pro R | ANSI (68) |
| ATK | Hex80 | ANSI (87) |
| IPI | QBZ75 / Aurora 75 / Aurora75 PRO | ANSI (82) |
| IPI | flash68 | ANSI (68) |
| IPI | QBZ65 / AURORA65 / AURORA65W / RAIN65 | ANSI (67) |
| ATTACK SHARK | R98 GT / R98 HE / R98 Pro / R98 Ultra | ANSI (98) |
| ATTACK SHARK | X65 HE | ANSI (67) |
| ATTACK SHARK | R68 HE / X68 HE / X68 Pro HE | ANSI (66) |
| ATTACK SHARK | Beat75 | ANSI (81) |
| ATTACK SHARK | X87 Ultra | ANSI (84) |
| ATTACK SHARK | X82 HE / X82 Pro HE | ANSI (83) |
| ATTACK SHARK | X68 MAX / X68 Ultra | ANSI (68) |
| ATTACK SHARK | X85 Ultra | ANSI (81) |
| ATTACK SHARK | R86 Pro HE | ANSI (87) |
| ATTACK SHARK | R82 HE / R82 Pro HE | ANSI (80) |
| ATTACK SHARK | K85 / K85 Pro HE | ANSI (82) |
| ATTACK SHARK | X60 HE | ANSI (61) |
| ATTACK SHARK | X96 HE / X98 HE | ANSI (102) |
| ATTACK SHARK | R85 HE / R85 Ultra | ANSI (79) |
| ATTACK SHARK | X820 Pro | ANSI (80) |
| ATTACK SHARK | X65 Pro HE | ANSI (66) |
| Aula | MINI60 HE Pro | ANSI (61) |
| IROK | NA87 Mag | ANSI (87) |
| SayoDevice | O3C | Three magnetic keys (3) |
| GravaStar | Mercury V75 / V75 Pro / V75 Lite | ANSI (79) |

O3C always has three physical keys in automatic mode: known bindings label them,
otherwise Key 1 / Key 2 / Key 3. Manual mode uses Z/X/C. See [O3C details](current/SAYO_O3C_CONFIG_2026-09-19.md).
HERO84 HE selects its layout after exact identity and map proof; its experimental warning remains.
KP-TE153 follows the vendor UK profile, including its IntlRo key.
K673RGB-M ABNT2 has a wide Intl / key instead of right Shift; Fn/encoder have no published analog channel.
UwU lists only its three analogue keys; the auxiliary buttons are digital-only.
Wooting 60HE v2 Split physical channels still need real-device validation.

Source inventory: 121 built-in variants, consolidated to 98 visible variants.
The generic and WASD-only presets are included. User-created layouts are excluded.

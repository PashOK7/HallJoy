# Owner-suggested brand expansion — 2026-09-20

## Result

Main A1:C432 now contains 377 model/configuration records across 55 brands, with 54 single-row separators.
Added 21 records, all `Not investigated`: CIDOO 2, Chilkey 4, CyberLynx 1, DarkBeacon 5, Dark Project 4, FuryCube 3, Red Square 2.
The per-model [source manifest](../research/keyboard-sheet-ozon-brands-20260920.json) records evidence and limitations. Catalog presence is not HallJoy compatibility.

## Corrections and scope

The previous pass left CIDOO C75/C80 unresolved. C75 is now directly confirmed by the manufacturer's PDF, linked from official downloads. C80 is present in official driver support and a hands-on video demonstrates magnetic switches and depth input. Do not repeat the claim that CIDOO only has C60 HE.
Red Square is not exhausted by IO Type 68/84. The owner's Ozon screenshot (item 3618392118) establishes Alumix 68 Yotei with Magnetite Ice, independently corroborated by a hands-on DNS review. Alumix 104 Yotei is also marketed, with an independent review confirming the model.
Keep IO and Red Square as distinct brand blocks; existing IO research statuses remain unchanged.
Sapphire variants and mousepad bundles are not additional magnetic models.
DarkBeacon Devil Breaker HE and TMR are separate sensing versions; TMR theme editions are consolidated. Anarchy/Print Codex are advertised as preorders, while the family already has other editions. Flux colors and Assassin Green/Healer White options are consolidated.
Chilkey ordinary ND65, ND75, ND104 and non-HE builds are excluded.
Dark Project KD83A only qualifies in its Magnetite version; Moonstone/Sapphire/Zircon models are not included.

## Evidence gaps retained

- CyberLynx M68HE: marketed as magnetic with product-specific owner reviews, but absent from the currently published manufacturer product/driver catalog. Retail evidence is weaker than a manufacturer specification; no relation to FuryCube M68HE is established.
- CyberLynx RX75 Pro: conflicting seller magnetic description, fixed actuation specifications and ordinary mechanical branding. Excluded pending specific primary confirmation.
- Red Square Alumix 104 Yotei: retail specifications and separate hands-on model review; no manufacturer product page located.
- VALOR and BIGATECH: no specific magnetic model established. Owner explicitly accepts leaving these unconfirmed without supplying links. This is not proof that none exist.
- Other Red Square models appearing in regulatory lists are not automatically confirmed magnetic or released products.

## Verification

Fresh read immediately before mutation matched the captured baseline; destination rows had no content.
Backup: `.local/backups/google-sheet-before-ozon-brands-20260920.json`.
Native API readback confirmed all 356 prior triples preserved, 21 intended additions, exact per-row validation preserved, gray new statuses and medium block borders for all 55 brands.
Pwnage V2 remains red `Research blocked: V2 firmware unavailable`, now C344.
Header, column widths and conditional-format rules were retained; one spacer per brand boundary.
No runtime or build changes. No claim of worldwide or per-brand completeness.

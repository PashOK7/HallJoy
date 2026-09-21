# ATTACK SHARK public sheet catalog — 2026-09-20

Owner requests all magnetic keyboard models, Not investigated for unresearched support, and no source notes in sheet cells. Sources belong in project documentation. All 23 previous ATK source notes were removed; 22 status values, the status dropdown and gray conditional rule were renamed.

Added three official-product models, each Not investigated:

- AK029: ATTACK SHARK x AJAZZ co-branded 29-key magnetic device. https://attackshark.com/products/attack-shark-x-ajazz-ak029-wired-one-handed-gaming-keyboard
- M36 HE: one-handed magnetic keyboard. https://attackshark.com/products/attack-shark-m36he-one-handed-hall-effect-magnetic-switch-keyboard-5-layer-noise-reducing-8khz-polling-rate-for-pc-mac
- R86 HE: official pre-order listing, distinct from the existing R86 Pro HE driver profile; retail-to-profile equivalence is not established. https://attackshark.com/products/attack-shark-r86-he

Coverage checked against official wired/wireless HE collections, keyboard support, the public products.json catalog (limit 250, second page no additional matching keyboards), and existing manufacturer-client profile evidence in ../research/ATTACK_SHARK_FAMILY_2026-09-20.md. Cable bundles, colors and mechanical-only products are not separate magnetic models.

- https://attackshark.com/collections/wired-he-keyboards
- https://attackshark.com/collections/wireless-he-keyboards
- https://attackshark.com/collections/magnetic-switch-keyboard
- https://attackshark.com/collections/keyboard-support
- https://attackshark.com/products.json?limit=250

## Source discrepancies

X820 Ultra manual headings say magnetic, but its official product specification identifies conventional Shark/Gift switches and 3/5-pin sockets. Do not add this retail model as magnetic based on the manual heading. https://attackshark.com/products/attack-shark-x820ultra-tri-mode-gasket-mechanical-keyboard

The public X87 Ultra listing also describes a mechanical keyboard, while preserved manufacturer-client records include X87Ultra magnetic profiles. Retain existing exact-profile support status; do not generalize it to every retail keyboard with that name. Retail/driver names alone do not establish revision identity. https://attackshark.com/products/attack-shark-x87ultra-wireless-rgb-mechanical-keyboard-with-shark-wireless-receiver

The existing 28 entries include manufacturer-client model names, not proof that all are current retail SKUs. No claim of exhaustive historical/regional availability is made. No firmware investigation or runtime support expansion occurred.

## Result and verification

Canonical spreadsheet: https://docs.google.com/spreadsheets/d/1ueQ4labXpuBOmGjCkUcJllNJ68Jzx4py2MuQm-p-R7c/edit

Main A1:C132: 117 model records, 15 brands, 14 spacer rows. ATTACK SHARK A26:C56 contains 31 entries sorted by model. Existing records and statuses preserved except the requested Not investigated rename. Native API readback checked record preservation, no cell notes, expected count, block side borders, repeated-brand hiding and spacer/next-brand placement. Existing three-column layout and colors retained. Backup: .local/backups/google-sheet-before-shark-catalog-20260920.json.

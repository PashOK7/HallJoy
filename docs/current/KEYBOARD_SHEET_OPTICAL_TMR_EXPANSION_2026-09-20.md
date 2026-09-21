# Public analog catalog: optical and TMR expansion — 2026-09-20

Main now A1:C528: 452 records, 76 brands, 75 blank separators.
Added 12 records: Razer 7, VARO 3, Sony 1, Lofree 1. All Not investigated.

## Sources
- **Razer Huntsman V3 Pro 8KHz**: Official product page explicitly confirms analog optical switches; distinct revision from existing supported models. [Source](https://www.razer.com/gaming-keyboards/razer-huntsman-v3-pro-8khz).
- **Razer Huntsman V3 Pro Tenkeyless 8KHz**: Official product page explicitly confirms analog optical switches; distinct revision from existing supported models. [Source](https://www.razer.com/gaming-keyboards/razer-huntsman-v3-pro-tenkeyless-8khz).
- **Razer Huntsman V3 Pro Low-profile Tenkeyless 8KHz**: Official product page explicitly confirms analog optical switches; distinct revision from existing supported models. [Source](https://www.razer.com/gaming-keyboards/razer-huntsman-v3-pro-low-profile-tenkeyless-8khz).
- **Razer Huntsman V3 HE Magnetic Tenkeyless 8KHz**: Official July 30 2026 announcement names both Hall Effect form factors. [Source](https://www.razer.com/newsroom/product-news/huntsman-v3-he-magnetic-8khz-line).
- **Razer Huntsman V3 HE Magnetic Mini 65% 8KHz**: Official July 30 2026 announcement names both Hall Effect form factors. [Source](https://www.razer.com/newsroom/product-news/huntsman-v3-he-magnetic-8khz-line).
- **Razer Tartarus Pro**: Manufacturer confirms original Tartarus Pro uses analog optical switches. [Source](https://press.razer.com/product-news/razer-analog-optical-switches-gen-2/).
- **Razer Tartarus V2 Pro**: Analog optical Gen-2 switches; distinct from conventional Tartarus V2. [Source](https://www.razer.com/eu-en/gaming-keypads/razer-tartarus-v2-pro).
- **Sony INZONE KBD-H75**: Official Sony product page confirms magnetic Hall Effect switches; KBD-G900 is the hardware model alias. [Source](https://electronics.sony.com/audio/gaming-audio/all-inzone-headsets/p/kbdg900b).
- **VARO VM75 HE**: Official magnetic-switch keyboard product page. [Source](https://www.varomall.com/shop_view/?idx=31).
- **VARO VM87HE-J**: Manufacturer explicitly describes magnetic Hall Effect Rapid Trigger; ordinary VM87-J excluded. [Source](https://varomall.com/VM87HE-J).
- **VARO VM65 HE-S**: Official support names VM65 HE-S and firmware; hands-on DPQP review confirms magnetic sensing on VM65HE. Use official HE-S name, no duplicate unsuffixed row. [Source](https://varomall.com/VM65HE-Sdriver).
- **Lofree Hyzen (announced)**: Manufacturer describes TMR sensing, adjustable actuation, and magnetic/mechanical switch compatibility. Campaign page still says upcoming; general shipment not verified. [Source](https://www.lofree.co/pages/hyzen).

## Decisions
- Existing Razer support statuses remain unchanged. New 8KHz, low-profile, magnetic and Tartarus models are not assumed compatible with the existing backend.
- Tartarus Pro and V2 Pro have analog key switches, unlike ordinary Tartarus V2. Thumbstick analog alone would not qualify a keypad.
- Azeron analog thumbsticks and ordinary SKYLOONG optical switches do not establish analog key-depth sensing; no rows added.
- VARO VM65 HE-S uses the manufacturer's exact support name. Supporting hands-on review: https://dpqp.jp/varo-vm65he . Do not add another unsuffixed VM65HE without evidence of a distinct model.
- Hyzen page remains a crowdfunding/upcoming page, despite an available configuration tool. Label announced rather than assert retail shipment. Wired/tri-mode options consolidated.
- Sony INZONE KBD-H75 and KBD-G900 identify one model, not two.
- No firmware reverse engineering or new HallJoy support was performed.

## Verification
Fresh range matched baseline before mutation. Backup: .local/backups/google-sheet-before-optical-tmr-expansion-20260920.json.
Verified all values and per-record validations, all previous 440 records, 76 outer brand borders and 12 gray status fills.
Pwnage V2 special status/validation preserved at C404. Existing conditional rules and header untouched.
No visual application run; owner evaluates visuals. Expansion remains ongoing.

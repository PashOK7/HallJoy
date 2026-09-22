> 2026-09-22: MAX wired integration is now implemented, built and synchronized as Supported. Earlier integration-required conclusion below is historical. See [current implementation](AULA_MINI60_MAX_SUPPORT_2026-09-22.md).

# AULA catalog gaps — 2026-09-21

Read-only live Google Sheet audit requested by owner; no support decision or Sheet edit.

Sheet: https://docs.google.com/spreadsheets/d/1ueQ4labXpuBOmGjCkUcJllNJ68Jzx4py2MuQm-p-R7c/edit
Verified tab Main, sheetId 0; bounded read A95:C145. AULA occupies rows 106–119 (14 entries): AG60, AG75, F75 HE, HERO 68 HE, HERO 68 HE Ultra, HERO 75 HE, HERO 99 HE, HERO84 HE, KP-TE153, MINI 60 HE, MINI 60 HE Pro, WIN 60 HE, WIN 60 HE MAX, WIN 68 HE.

## Confirmed missing names

Official AULA HUB https://aulastar.com/aula-hub/ lists eleven absent names:

- MINI 60 HE MAX
- AG63
- HERO 68 HE PRO
- HERO 68 MINI
- HERO 68 Air
- HERO 68 XS
- HERO 68 MINI Air
- WIN 60 HE PRO
- WIN 68 HE PRO
- WIN 68 HE MAX
- WIN 68 HE Ultra

Manufacturer driver pages use HE in additional HERO variant names; these are aliases, not extra rows: https://www.aulastar.com/drive/list_28_8/ . Normalize official MINI 6O typo to MINI 60.

One additional confirmed magnetic model absent from the Sheet: BOX63. Official announcement explicitly describes magnetic switches and rapid trigger: https://www.aulastar.com/announcement/850.html ; official web-driver list also includes BOX63: https://www.aulastar.com/web-drive/ . Product availability is not established by this audit; catalog scope permits announced models.

Total: 12 missing model/variant names, including the owner's MINI 60 HE MAX. Current 14 plus these 12 would make 26 AULA entries. This is the confirmed result of this audit, not a guarantee of exhaustive worldwide coverage.

## Why the gap occurred / future checks

Previous KEYBOARD_SHEET_EXPANSION_2026-09-20.md recorded the generic magnetic-product catalog as the AULA source and explicitly did not claim exhaustive coverage. That catalog groups families and does not enumerate every HUB/driver variant. The prior cross-check was incomplete; there is no evidence that MINI MAX was deliberately excluded on compatibility grounds.

For future catalog expansion, reconcile product-family pages with manufacturer HUB/driver model lists and official announcements. Deduplicate regional HE naming aliases; do not merge distinct PRO/MAX/Ultra/Air/XS versions automatically. HUB presence and magnetic switches alone do not prove HallJoy compatibility. No unverified variant is promoted to Supported by this audit.

Sheet additions remain pending; user asked why/what is missing, and this turn performed the audit only.

## Authorized corrections and firmware review

Owner then requested all missing rows be added and MINI MAX firmware checked. Later requested IO Type 68 Magnetic Pro Wireless catalog-only, and explicitly prohibited cell comments/notes. All 13 notes temporarily added by this agent were removed; sources stay here. Mandatory rules: ../development/KEYBOARD_SHEET_RULES.md.

Inserted 12 AULA rows; alphabetically sorted A106:C131 with 26 entries, preserving the original 14 statuses, dropdown validation, brand boundary borders and conditional colors. Readback verified neighbors and all rows. MINI MAX is B123/C123, yellow `Known protocol; integration required`; only this cell's allowed status list was extended and a matching yellow conditional rule added. Other eleven additions are Not investigated. IO insertion A256:C256: Type 68 Magnetic Pro Wireless, Not investigated; adjacent IO entries remain intact. No IO firmware work.

### MINI 60 HE MAX V1.52

Official archived AULA HUB JavaScript maps wired 0C45:80A1 / FF68:0061 to MINI60HE MAX and selects the HFD module. Version page explicitly queries MINI 60 HE MAX via https://hubapi.aulacn.com/user/EXE/getFile/MINI%2060%20HE%20MAX . Retrieved version V1.52 and downloaded https://app.aulacn.com/commonAssets/MINI%2060%20HE%20MAX_V1.52.exe . Updater was never executed; no device I/O or flash.

EXE SHA256: 958a39951d2f908a7fe925e0fa598c321924cb235239bf37484ae7c8213512d6.
PE resource 10/4000/0, 516096 bytes, SHA256: 06a1a47432a6ca7ec060103378e2d984a46f9afa98f19fccd99219eaea4a88e0.
Local artifacts: .local/MINI 60 HE MAX_V1.52.exe; .local/aula-mini60-max-152-resource-10-4000-0.bin; .local/aula-mini60-max-firmware-api-20260921.json.

Actual ARM firmware disassembly and Unicorn execution establish:
- Dispatcher 0xef14 recognizes 0x66/0x67 and sets/clears simulation byte 0x2000049d; calibration byte 0x2000049c remains zero in these cases.
- Report builder 0x14cc creates 64-byte 55 FB packets with key index, status, LE16 high/low/ADC/travel/stroke fields, matching HallJoy mini60diag::Decode.
- Reporting tail 0x3750 calls builder and USB submission 0x11ca8 for qualifying depth. Constant stroke 34 is loaded at 0x3742. Sample tail execution emits travel 170, 93, 1; raw ADC above high-12 emits nothing, as in the PRO review. This is not proof of a continuous zero-release report; existing host stale-release policy remains relevant.
- .local/aula-mini60-max-research-20260921.py reproduces dispatcher and reporting-tail checks; PASS. Synthetic already-computed depth, calibration endpoints and registers; USB is stubbed. Not full scanner, physical USB, radio or timing validation.

Current production admission is explicitly 0C45:80A2 only in aula_mini60_diagnostic.cpp, and device-info also requires manufacturer 0x0166 / product 0x110c. MAX is therefore NOT supported by current HallJoy. Existing analog protocol can be reused, but exact MAX identity, key assignment/geometry, transport/session behavior and normal-input coexistence must be integrated/checked before promotion. No runtime code, EXE, README supported list or SUPPORTED_HARDWARE supported list changed because no new support was implemented.

Final connector readback: Main!A106:C133 and A255:C258 contain zero cell notes; C123 exact new status with strict dropdown and yellow effective RGB (1, 0.9019608, 0.6392157); IO C256 Not investigated with strict dropdown and existing gray color. No agent GUI visual run, per owner policy.

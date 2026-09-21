# Magnetic-keyboard catalog verification — 2026-09-20

## Scope

Rechecked the 149 additions from [the expansion](KEYBOARD_SHEET_EXPANSION_2026-09-20.md) against manufacturer/storefront product material, explicit model-category lists, and exact local firmware evidence for previously researched devices. All 149 belong to magnetic-sensing keyboard/keypad families. No conventional-only keyboard was established among these additions. This does NOT assert that every key is analog, that every switch configuration is magnetic, or that independent depth can be read through USB.

The older 117 records were not a new hardware-category audit in this pass. The complete sheet remains 266 records / 32 brands / A1:C298. Existing optical analog entries remain intentional.

[Per-model verification manifest](../research/keyboard-sheet-magnetic-audit-20260920.json) records all 149 checks and source URLs. These sources stay outside sheet cells, per the owner.

## Important limits found

- Corsair K70 PRO TKL: manufacturer's manual specifies 61 ANSI / 62 ISO / 65 JP MGX magnetic keys, with conventional switches elsewhere. [Manual](https://www.corsair.com/us/en/explorer/gamer/keyboards/k70-pro-tkl/).
- SteelSeries Apex Pro TKL (2023), wired and wireless: manufacturer's support article specifies 64 face keys with Hall sensors; other keys are red switches. [Article](https://support.steelseries.com/hc/en-us/articles/9644740151181-Does-the-Apex-Pro-TKL-2023-Wired-Wireless-feature-OmniPoint-Switches). Do not extrapolate all-key sensing to other Apex revisions without checking.
- Keychron V6 Ultra Hybrid 8K, MonsGeek M1 HE / M1 V5 TMR / FUN60 Ultra TMR, EPOMAKER HE75 V2 TMR and GamaKay TK75 TMR accept conventional switches as well. Magnetic-capable boards are correctly included, but mechanical switch positions do not acquire analog depth merely by being installed on them.
- Wooting 80HE+ is an announced preorder model, explicitly Hall Effect; listing is not a hardware-test or shipping claim.
- MelGeek historical Pro / Ultra pages now sometimes display Pro+ / Ultra+ in their body while retaining original names in titles/URLs. Magnetic technology is confirmed; exact generation aliases are not automatically merged or marked compatible.
- Some manufacturers use “mechanical” as a broad product category even when their switch specification explicitly says Hall Effect. Conversely, a magnetic palm rest, SOCD, or mere presence in a configuration tool is insufficient evidence. Exact switch/model evidence took precedence.
- MADLIONS checks use the named brand storefront plus existing exact firmware evidence for TITAN68. They do not establish legal ownership of the storefront.

## Owner clarification and sheet changes

The owner confirms that Ace 68 and TITAN 68 Turbo already had tester attempts without obtaining usable analog. Both now use red **No usable analog found**. This is the project outcome, not an assertion that sensors are digital or that every possible firmware path is mathematically absent.

The historical Ace 68 log assessment must remain intact: the 2026-08-28 capture had PID 2116 while that diagnostic admitted PID 2114, so that particular capture alone proved neither presence nor absence of analog. The owner's later outcome is recorded separately, not substituted into historical evidence. Exact firmware reviews still contain candidate transport code. No new reverse engineering or device testing was performed.

Every current Research frozen entry is now **Research frozen; tester needed**: ASUS ROG Azoth 96 HE, IROK MG75 V2, NA87 Pro and ND75. MG75 V2 changed to Research frozen in the live sheet during this task; the fresh read detected and preserved that edit before adding the requested suffix.

## Verification and preservation

Native Google Sheets API only. Updated six status cells, status validation and red/gray conditional rules. No model removed or renamed. All other values and base formats were checked unchanged against the immediate pre-write read; zero cell notes. Effective red/gray fills verified. Backup: .local/backups/google-sheet-before-magnetic-audit-20260920.json.

No runtime, firmware, EXE, README compatibility table or support implementation changed.


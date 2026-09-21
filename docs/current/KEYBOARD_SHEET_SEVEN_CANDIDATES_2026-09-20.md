# Seven-candidate catalog expansion — 2026-09-20

Main now occupies A1:C405: 356 model/configuration records, 49 brands, 48 spacer rows.
Added 26 records: AJAZZ 13, VGN 8, Fantech 2, CIDOO 1, Shortcut Studio 1, RAKKA 1.
Every addition has status `Not investigated`. Magnetic sensing is not evidence of HallJoy protocol compatibility.

## Evidence and naming

Per-record primary sources and evidence: [manifest](../research/keyboard-sheet-seven-candidates-20260920.json).
AJAZZ names shared with mechanical products explicitly identify the magnetic version. AK029 also occurs as an ATTACK SHARK collaboration; brand entries do not establish distinct firmware or hardware.
VGN Neon translates 霓虹, and Super Competitive translates 超竞版. Manufacturer configurations are separate rows; colors are not.
VGN Lightning 68 is confirmed by the manufacturer's launch video and product homepage.
Shortcut Studio's official download page provides the Bridge75 HE PCB configuration instructions and firmware.

## Exclusions and unresolved candidates

- ACGAM: no manufacturer-backed magnetic model confirmed. Generic marketplace search results do not establish one.
- VXE: V75 X is a conventional mechanical keyboard; a magnetic charging dock does not make its keyboard switches magnetic. VGN is kept separate from VXE and ATK.
- CIDOO C75 / C80: manufacturer download links exist, but this pass did not establish magnetic sensing from those downloads. Excluded pending product evidence.
- RAKKA ATLAS60MOD / earlier RAKKA 60 ATLAS: manufacturer describes a Hall-effect module kit without case/keycaps. Excluded from this complete-keyboard batch; the two names must not be counted as two complete boards.
- AJAZZ Blue67, AK029 ULTRA, AK980 V2 PRO 8K, MK87, AK820 V2 PRO, Pixel68 and AK870 HE: not included without sufficiently specific primary product evidence.

## Preservation and verification

Before writing, fresh cell data matched the previous read, and the destination tail was empty.
Backup: `.local/backups/google-sheet-before-seven-candidates-20260920.json`.
Native Sheets API readback verified all 330 previous brand/model/status triples unchanged, all row-specific validations preserved, all 356 intended records, and medium outer borders for all 49 brand blocks.
Blank separators remain one row. Existing conditional formatting remains in place: additions are gray; Pwnage Zenblade 65 V2 remains red `Research blocked: V2 firmware unavailable`, now at C320.
No formulas, notes or rich cell chips were overwritten. Sources remain local instead of cluttering public cells.
This is an ongoing catalog, not a claim of worldwide completeness. No runtime, build or firmware changes.

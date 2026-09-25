# RongYuan fifth support batch — 2026-09-24

## Result and scope

Seven models / 13 exact revisions / six brands added locally. Detection,
independent analog input, bindings and virtual gamepad output are enabled over
wired USB. Yellow notices mean remaining switch-range/firmware uncertainty and
no physical-device verification. Manual layouts remain available. No firmware
emulation, flashing, forced logging or GitHub publication. K4 onboard unchanged.

| Brand | Model | Board | VID:PID | Source | Parent | Normalization, um |
|---|---|---|---|---|---|---|
| BOYI | H60 Pro | 2764 | 3151:5029 | 5d0ca086.js | 7a5b12c9.js | 3300 |
| BOYI | H60 Pro | 2875 | 3151:5029 | 91d000f6.js | 7a5b12c9.js | 3300 |
| MageGee | AIR68 | 3146 | 3151:5030 | 1db44737.js | 7a5b12c9.js | 4000 |
| MageGee | Captain87 JIS | 2789 | 3151:5030 | 000f00fb.js | 7a5b12c9.js | 3300 |
| Sunsonny | N-J100 | 2780 | 3151:502D | e608756b.js | 7a5b12c9.js | 4000 |
| Sunsonny | N-J100 | 3294 | 3151:502D | cfaaca03.js | 7a5b12c9.js | 4000 |
| Royal Kludge | A72HE | 3332 | 3151:5030 | 68053ed8.js | 4796d290.js | 4000 |
| Royal Kludge | A72HE | 3333 | 3151:5030 | 3e0869f4.js | 7a5b12c9.js | 4000 |
| XINMENG | Zero 68 | 3611 | 3151:5029 | d9ca6083.js | 7a5b12c9.js | 4000 |
| XINMENG | Zero 68 | 3587 | 3151:5029 | 57a18a71.js | 7a5b12c9.js | 4000 |
| XINMENG | Zero 68 | 3598 | 3151:5029 | 412b3f20.js | 7a5b12c9.js | 4000 |
| XINMENG | Zero 68 | 3609 | 3151:5030 | 20bcdaa4.js | 7a5b12c9.js | 4000 |
| COLORFUL | QY98 Ultra | 3257 | 3151:5030 | 89073ff1.js | 7a5b12c9.js | 4000 |

## Primary protocol evidence

Pinned Womier3.2.15 OEM records, exact USB identifiers, magnetic flag, class
loaders and512-byte factory matrices are retained in admission_sources.json,
profiles.json and source-lock.json. Every compiled byte is audited. All new
model classes contain factory-map fields only; no model-specific behavior.
The shared report5 / command1B backend already implements stream activation,
negotiated units, independent physical positions, delta holds, alias merge,
release and disconnect. No settings/calibration writes added.

Royal Kludge3332 inherits4796d290.js, now pinned and admitted explicitly.
Its entire261-byte subclass adds only getWangBaReset: a one-byte
FEA_CMD_GET_USB_VERSION request with Bit7, returning response byte10.
It inherits all analog behavior from7a5b12c9.js. HallJoy does not call this
auxiliary method. SHA256d4be57083367870df1522bfde18de1661b58579c8c13c18e1f0c4fa1325969b3.
Board3333 inherits7a5b12c9 directly. They have different maps, so each gets its
own profile despite shared VID/PID. Launcher and volume controls are special
non-key records and are not published as analog keyboard positions.

BOYI2764 is ANSI,2875 UK. Its OEM travel setting max3.3mm selects3300um.
Captain872789 is the91-position Japanese factory map and explicitly JIS-only;
do not extend this admission to ANSI Captain87. The Japanese manufacturer page
states adjustable actuation0.1–3.3mm and RY5088. AIR68 remains4000um provisional.
Sunsonny3294 is an internal N-J100 V1 revision, grouped under N-J100 rather than
inventing an extra retail model. XINMENG Zero68 revisions include three wired
and one tri-mode identity, all used over USB. OEM company XinMengK65Keyboard
and exact Zero68 records establish scope; do not promote Beat65/Beat68/X87/X98
siblings by company similarity. COLORFUL QY98 Ultra is the magnetic OEM record;
mechanical QY98 and QY98 Pro are excluded. Other4000um values are provisional
normalization, not measured physical stroke or effective precision.

## Retail corroboration and sources

Primary protocol evidence above is decisive. Retail naming corroboration is
separate and does not constitute hardware testing.

- BOYI H60 Pro official guide and driver listing:
  https://www.boyikeyboard.com/download/UserManual/BOYI-H60Pro-Quick-User-Manual.pdf
  and https://www.boyikeyboard.com/Software-Driver.html
- MageGee official magnetic catalog lists AIR68 and Captain87:
  https://www.magegee.com/magnetic_switch_keyboards.html
- Exact Captain87 JIS manufacturer specifications:
  https://magegee.jp/products/rapid-trigger-captain87-jis
- Sunsonny official download listing supplies N-J100 driver:
  https://www.sunsonny.com/Down.html
- Royal Kludge official news names the A72HE tri-mode magnetic keyboard:
  https://www.rkgaming.com/news/
- XINMENG Zero68 and COLORFUL QY98 Ultra names/magnetic status are grounded
  directly in pinned OEM records. Retail search corroborates these names, but
  no independently mapped current manufacturer firmware was downloaded for them.
  Do not characterize this as a firmware-image analysis.

## Candidates not promoted

VGN2905 Neon75 has only setSideLightSetting/getSideLightSetting overrides on the
known parent. Reviewed functions encode/decode side lighting, not analog.
However the internal name says gk8640w_ulter and the Sheet separately lists
Neon HE/HE+/Pro/Super Competitive/Ultra. Exact retail revision mapping remains
unresolved. Neither added a duplicate Neon75 row nor promoted any existing row.
VGN8670 variants and Neon75HE Air are not automatically covered.

KEYCOOL public download API at kc-keycool.com/api/drivers lists K19/K21/Y17 and
Y84/GZ68, not the inspected KC68/RT68/Yumi68/YZ75 PRO-CZ records. Retail names
can overlap mechanical products, so those candidates remain research only.
Legion K98 retail mapping also unresolved. DELUX RTS1 V22563 remains hybrid:
membrane KeyM and volume functions need an explicit analog-position exclusion
review; no blanket all-key support claim. EPOMAKER HE65 V2 board3417 cannot
silently become HE65 V2 TMR by a name match. Earlier deferred candidates remain.

## Verification

Source audit PASS117 RongYuan models /183 revisions. Native regression PASS:
all factory key positions, exact identities, independent values, aliases,
stationary holds, releases, disconnect, precision and malformed packets plus
4898 existing captured frames. These are not captures from the new keyboards.
Ordinary Release and all six linked-image gates PASS.
Build log: .local/rongyuan-batch5-build.log.
Installed EXE: build/bin/Release/x64/HallJoy.exe.
SHA256: c06153d5cf85d7cce19916043aad5322bb13a542db7755b5fcf9aec893ae8b12

README, detailed hardware inventory, generated runtime notices and next patch
notes synchronized. Live Google Sheet Main: one atomic30-request batch after
fresh unchanged-snapshot check. Seven model rows + five separator rows added.
Targets at checkpoint:154,168,449–450,584,627,698. All yellow values use the
existing strict dropdown rule. Explicit32px model/16px separator heights and
complete affected-block borders. No cell notes/comments.

Readback preserved all695 prior returned rows' values/validation and formatting
except authorized border changes; effective backgrounds and column widths
unchanged. SHEET_STRUCTURE=PASS123 blocks,0 issues. All177 yellow rows match
runtime catalog. API metadata verification only; owner handles visuals.
Totals584 models:63 green,177 yellow,340 gray/research,4 blocked/no-usable-analog.
Grid1188 rows. No publication.

Backups: .local/backups/before-rongyuan-batch5-20260924.zip and
.local/backups/sheet-before-rongyuan-batch5.packed.json.
Readbacks: .local/rongyuan-batch5-sheet-native.packed.json and
.local/rongyuan-batch5-sheet-yellow.json.

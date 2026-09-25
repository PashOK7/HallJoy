# RongYuan and SparkLink catalog evidence — 2026-09-24

These are research inventories, not additional supported-keyboard lists.
The owner wants real enabled analog integrations before yellow status.

## Coverage

- `rongyuan-oem-records.json`:479 manufacturer magnetism-flag records from pinned
  Womier3.2.15, with4 explicit nonmagnetic exclusions:475 candidates. These include
  board revisions, regional variants, internal names and unreleased placeholders.
  A shared parent class narrows research; it does not alone resolve a retail name,
  USB identity, analog scale or complete key map. This is not475 unique keyboards.
- `irok-protocol-groups.json`:48 group memberships from the current official
  IROK web client. Receivers recur in multiple groups; internal secondary IDs can
  distinguish variants sharing VID/PID. This is not a global SparkLink client list.
- No public source here enumerates every OEM customer. Other brand configurators
  and future firmware revisions may contain additional models.

## IROK classification

Official source: https://hid.irok.cn/assets/index-D22onkw6.js and
https://hid.irok.cn/assets/sdk-keyboard-naj1DnVU.js, downloaded2026-09-24.
The catalog pins the main client's SHA256; local full sources retained in
.local/research/protocol-catalogs-20260924/.

- JingTai V2 (the existing SparkLink row backend): MG75 Max (already confirmed),
  MG68 Plus, ND68 Pro, ND63 Ultra, RA68; Carotmas Mars75/Mars75 Pro,
  Mer68 Max/Mer68 SE JT. Distinct RA68 HuoChaiRen revision is not this protocol.
- JingTai V1: MG75/MG75 Pro, Mer68/Mer68 Pro, NA87 Pro, ND63, IYX MU68
  Pro/Pro Cyan/Ultra and Polar75. V1 is NOT automatically interchangeable with V2;
  existing MG75 Pro uses a separate implementation. MG75 V2 is yet another family.
- RongYuan groups: MG68/SE/Pro/Lite/Max/ACE/ACE W, Mars68 family, Mer68 SE/Lite,
  ND68 and ND63 Max, plus receiver variants. They are not SparkLink merely because
  the same IROK configurator lists them. Exact board matching remains required.
- NA87/ND75/IYX MU68 belong to Witmod; they are not inferred from SparkLink.

Four newly enabled V2 models: MG68 Plus1CA6:052A, ND68 Pro1CA6:052C,
ND63 Ultra1CA6:0531, RA681CA6:0528. All official filters use FFB0:1.
SDK Performance=4/AxisData=3/Route=1 sends04 03 01 row; returned depths areLE16.
LayoutAndKey=3/GetKeyLayout=1 sends03 01 layer row; native HallJoy queries layer0.
The official V2 UI polls six rows and assigns all these models the same3500-unit
travel scale. Existing HallJoy queries live rows, decodes standard keyboard keys
and Fn F101->0409, merges aliases, neutralizes stale rows and sends existing
bindings to the virtual gamepad. No flash, calibration or mode write is introduced.
Native semantic device admission is retained; the new table only adds exact
experimental identification and notices. Manual layouts; physical test pending.

## RongYuan additions and remaining leads

EPOMAKER HE68 Lite boards2761/2762/2883/3664 inherit the already reviewed1B
stream base unchanged. All factory entries are byte-checked against vendor sources;
only matrices differ. Enabled USB/manual-layout support. Default3.4mm scale uses
https://epomaker.com/products/epomaker-he68-lite (Clear Mag total travel3.4mm),
not a host-learned maximum. Switch variants remain an experimental caveat.

Further useful records: EPOMAKER HE60 Wired/Wireless, HE68 Mag, HE75 Mag,
HE75 V2; GamaKay NS75; MonsGeek FUN75; Keydous NJ68 Pro-CP;
Skyloong GK61/GK68/GK75; MSI STRIKE700 HE. These are catalog leads only.
HE75 V2 must NOT silently stand in for HE75 V2 TMR. Retail identity/revision
matching and per-key admission still need review before any further promotion.

Ace60 is separately blocked by flash-writing debug activation, documented in the
current expansion checkpoint. Do not count its parser prototype as support.


2026-09-24 follow-ups implemented: HE75 Mag/NS75, then HE68 Mag/FUN75/MSI STRIKE700 HE
and CAROTMAS Mars75/Mars75 Pro/Mer68 Max (IROK catalog rows). See current expansion
checkpoint for exact revisions and remaining range/identity caveats. Inventories
above remain research leads, not bulk support claims.

2026-09-24 further additions: HE60 Wired/Wireless and HE75 V2 exact OEM profiles,
plus Mer68 SE JingTai V2 as a separately scoped yellow entry. General Mercury68
SE and HE75 V2 TMR remain unpromoted. See current checkpoint for verified hashes,
nominal ranges, tests and live Sheet readback.

2026-09-24: NJ68 Pro-CP retail magnetic identity resolved from official Keydous
product pages; enabled together with exact wired Skyloong GK61 HE/GK68 HE/GK75 HE
profiles. Mechanical/optical namesakes and MIX variants are excluded. Full source
locks, tests, range caveats and live Sheet evidence are in the current checkpoint.


2026-09-24 EWEADN E HUB3.3.2:17 additional experimental labels /21 USB identities
reviewed against the official xingshan/SparkLink V2 SDK and connected to the
existing live layout/travel path. See [pinned catalog and wire review](../eweadn-sparklink/README.md)
and [batch checkpoint](../../current/EWEADN_SPARKLINK_BATCH_2026-09-24.md).
Alpha87, X75/Gamma75 PID1C2D and shared ES68 boot identity remain held; other
SDK families and wireless transports are not inferred from the HUB catalog.

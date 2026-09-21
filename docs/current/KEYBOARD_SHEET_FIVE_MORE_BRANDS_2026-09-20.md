# Public analog keyboard catalog: five more brands — 2026-09-20

## Outcome
Main now uses A1:C489: 419 model/configuration records, 70 brands, 69 blank spacer rows.
Added 18 records, all Not investigated: Darmoshark 3, EWEADN 11, Machenike 2, ROCCAT 1, WOBKEY 1.
This establishes catalog eligibility, not USB analog protocol availability or HallJoy support.

## Evidence and scope
- **Darmoshark K7 MAX**: Manufacturer specifies RAESHA magnetic switches. [Source](https://darmoshark.com/products/darmoshark-k7-max-black-wired-magnetic-switch-keyboard).
- **Darmoshark KT68 MAX**: RAESHA magnetic switches; adjustable trigger travel. [Source](https://darmoshark.com/products/darmoshark-kt68-max-wired-raesha-magnetic-switch-cnc-keyboard).
- **Darmoshark TOP75 (magnetic version)**: Kailh magnetic switches; linked manufacturer manual top75.pdf confirms Magnetic Shaft and Rapid Trigger. Ordinary TOP75 excluded. [Source](https://darmoshark.com/products/darmoshark-top75-trio-mode-keyboard).
- **Machenike K500-M61**: Manufacturer product description specifies magnetic switches. [Source](https://global.machenike.com/products/k500-m61).
- **Machenike K500-M81**: Manufacturer driver catalog explicitly labels K500-M81 magnetic-switch driver (磁轴驱动). [Source](https://www.machenike.com/offline/driverunit).
- **ROCCAT Isku+ Force FX (6 pressure-sensitive keys)**: Official ROCCAT tutorial and manufacturer manual describe analog pressure-sensitive QWEASD; remaining keys are not analog. [Source](https://www.youtube.com/watch?v=aeULFHbKIDQ).
- **WOBKEY Rainy 75 RT**: Official Hall Effect product; page says description based on RT Pro. Pro not split into a separate row pending clear SKU mapping. [Source](https://www.wobkey.com/products/wobkey-rainy-75-rt-keyboard).
- **EWEADN DEEP68 HE**: Magnetic actuation and depth-dependent DKS. [Source](https://www.eweadn.com/products/eweadn-deep68-he-magnetic-switch-keyboard).
- **EWEADN DEEP80 HE (magnetic version)**: Base magnetic SKU; mechanical SKU explicitly excluded. [Source](https://www.eweadn.com/products/eweadn-deep80-he-magnetic-switch-keyboard).
- **EWEADN DEEP80 Pro HE (magnetic version)**: Pro magnetic SKU; mechanical SKU explicitly excluded. [Source](https://www.eweadn.com/products/eweadn-deep80-he-magnetic-switch-keyboard).
- **EWEADN DEEP80 Max HE (magnetic version)**: Max magnetic SKU; mechanical SKU explicitly excluded. [Source](https://www.eweadn.com/products/eweadn-deep80-he-magnetic-switch-keyboard).
- **EWEADN DK63 HE**: Magnetic switches and adjustable Rapid Trigger. [Source](https://www.eweadn.com/products/eweadn-dk63-he-magnetic-switch-keyboard).
- **EWEADN DK68 HE**: Magnetic switches and adjustable Rapid Trigger. [Source](https://www.eweadn.com/products/eweadn-dk68-he-magnetic-switch-keyboard).
- **EWEADN Gamma75 HE (EXX collaboration)**: Manufacturer co-branded Hall Effect product. [Source](https://www.eweadn.com/products/eweadn-x-exx-gamma75-magnetic-switch-keyboard).
- **EWEADN X87HE**: Hall Effect magnetic switches; tactile and linear switch options consolidated. [Source](https://www.eweadn.com/products/eweadn-x87he-magnetic-switch-keyboard).
- **EWEADN ZAP68 HE**: Base magnetic SKU; wired/wireless consolidated. [Source](https://www.eweadn.com/products/eweadn-zap68-he-magnetic-switch-keyboard).
- **EWEADN ZAP68 Pro HE**: Pro magnetic SKU; wired/wireless consolidated. [Source](https://www.eweadn.com/products/eweadn-zap68-he-magnetic-switch-keyboard).
- **EWEADN ZAP68 Ultra HE**: Ultra magnetic SKU; Black Mixable also accepts mechanical switches; no assumption that all installed switches are analog. [Source](https://www.eweadn.com/products/eweadn-zap68-he-magnetic-switch-keyboard).

TOP75 manufacturer manual additionally confirms the magnetic version and Rapid Trigger:
https://cdn.shopify.com/s/files/1/0647/6319/9593/files/top75.pdf?v=1761358396

ROCCAT manufacturer quick guide mirror explicitly describes analog QWEASD:
https://fc.darty.com/notices/DOCUMENTATION/2017/13/4298985_NOTCOMP.pdf

EWEADN product variants were read from its public Shopify products.json endpoint.
DEEP80, Pro and Max magnetic SKUs are distinct from their ordinary mechanical options.
ZAP68 wired/wireless and cosmetic switch editions are consolidated by base/Pro/Ultra.
The Ultra Black Mixable configuration can also use mechanical switches; do not infer that all fitted keys are analog.
Machenike M81 is evidenced by the official magnetic-switch driver catalog, not an independently tested board.
WOBKEY RT page references RT Pro but lacks clear named Pro SKUs; a separate Pro row was deferred.

## Exclusions and pending candidates
- Ordinary Darmoshark TOP75, K7 Pro, K8 and Machenike K500-B61 / KT68 Pro are not inferred analog from neighboring product listings.
- Ordinary EWEADN DEEP80 mechanical SKUs are excluded.
- FEKER Fighting 68 remains unconfirmed from manufacturer material; no row added.
- YUNZII x MADLIONS MAD68 HE is not duplicated; MADLIONS MAD68HE already exists in the catalog.
- Everglide.com resolved to unrelated cookware and was not used as keyboard evidence.
- No claim of global or per-brand completeness.

## Mutation and verification
Native Google Sheets API only. Fresh pre-write values, formats and validation matched the baseline.
Backup: .local/backups/google-sheet-before-five-more-brands-20260920.json.
All previous 401 brand/model/status triples and their per-row validation were preserved.
Readback checked every written value and validation, all 70 outer brand borders and all new gray status fills.
Pwnage Zenblade 65 V2 retains its red status and special validation at C381.
Existing six conditional rules, header, widths and 1039-row grid were left unchanged.
No runtime, build, firmware or application support changes.

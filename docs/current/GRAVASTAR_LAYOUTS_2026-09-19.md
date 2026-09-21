> 2026-09-20 owner update: GravaStar support has a user report of working operation. The owner requests inclusion in the main README compatibility table, not a separate experimental category. This supersedes the earlier blanket pending-runtime description below; the exact tested revision was not restated and this is not a new agent hardware test.

# GravaStar Mercury V75 family layouts - 2026-09-19

Three exact legacy/v1 profiles now have ANSI presets and automatic identities:
V75 1CA5:2201 / 16052201; Pro 1CA5:2202 / 16052202;
Lite 1CA2:2201 / 2E022201. No additional v2 USB revisions are admitted.

## Manufacturer geometry

Official support.gravastar.com links to
https://hub.gravastar1.com/gravastar/connect for this family. Pinned assets in
`docs/research/gravastar-layout-sources` contain the manufacturer's shared
79-key Windows preset VL[0], key component specialStyle and scoped row CSS.
The exact VID/PID model selector changes case artwork, not key geometry.
Rows contain 13/15/15/14/13/9 keys; Fn action F001 is preserved as HallJoy Fn.
The reports lock source SHA256 values and geometry evidence. No runtime website
or downloaded asset is needed by the EXE.

All three source names merge within GravaStar into one visible
Mercury V75 / V75 Pro / V75 Lite ANSI preset, preserving distinct exact tokens.
Catalog: 106 source variants / 85 visible variants.

## Runtime integration

Only a complete existing native capability proof can publish a layout token.
USB identity and board ID must agree and compatibilityMismatchMask must be zero.
The proved default matrix supplies factory positions; the current base map
supplies remaps. Automatic mode uses these assignments. Manual mode uses the
factory matrix, including physical Fn, with the same real travel samples.

A previous defect reset the WIN60 MAX token on active-map refresh without
restoring it. Token publication now belongs to PublishProof for both the AULA
and GravaStar profiles. Same-device refresh preserves the selected token;
disconnect clears its session map. No extra polling or persistent device writes.

## Evidence limits and checks

V75 physical identity/map/range/travel evidence predates this change; corrected
sustained gameplay/reconnect validation is still pending. Pro/Lite retain
firmware-only evidence. This change is not a claim of new hardware testing.

Production-linked profile/catalog/automatic selection and recovery checks PASS.
Portable tests cover all four exact family identities, contradictory boards,
invalid proofs, remaps, duplicate assignments, Fn, refresh and disconnect.
Full native static/portable suite PASS. The initial run was interrupted during
compilation of an unrelated transport test; the continuation reran static audits
and completed the remaining executables. Evidence: .local/gravastar-native-checks.txt
and .local/gravastar-native-resume.txt. Ordinary build checks also PASS.

Backup: .local/backups/gravastar-layouts-before.zip; generated outputs also
backed up by layout_pipeline.py integrate. No visual tests or GitHub publication.

## Delivery (2026-09-20 continuation)

Ordinary Release x64 build and linked Shark/Mini60/NA87/ViGEm artifact checks PASS.
Installed build/bin/Release/x64/HallJoy.exe SHA256:
65608d0e85cfdda55642b3b301b1b1e2c59c1259f76c4ea9256899320c29118e.
Layout pipeline unit tests: 20 PASS; catalog audit unit tests: 4 PASS.
Evidence: .local/gravastar-release-build.txt and .local/gravastar-profile-checks.txt.

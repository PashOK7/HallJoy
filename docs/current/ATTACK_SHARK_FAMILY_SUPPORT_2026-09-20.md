> Superseded layout limitation: [family layouts](ATTACK_SHARK_FAMILY_LAYOUTS_2026-09-20.md) now provide exact automatic geometry for all37 revisions. Remap readback remains deferred by owner.

# ATTACK SHARK RY5088 family integration — 2026-09-20

## Delivered behavior

The ordinary Release build now admits 37 exact manufacturer-listed RY5088
magnetic revisions (31 newly enabled). The USB VID is 3151; eligible PIDs are
5029, 502D, 502F and 5030. HID usage FFFF:0002 and a 65-byte feature report are
required. Two matching 8F replies must identify an exact dev_id/PID pair before
any E5 FE depth request. Unknown revisions, other controller families and named
wireless receivers remain excluded. This is wired-USB support.

Each profile has its own pinned factory and Fn matrix. Only keyboard usages and
the physical Fn action are published; consumer, macro, lighting and encoder
records are not treated as keyboard depth. The previous six base maps are
unchanged. Their full vendor Fn maps additionally restore previously omitted
keyboard actions (including navigation and Print Screen/Scroll Lock/Pause where
present). Custom remap readback is not implemented. The native source remains
real independent depth; digital events do not generate depth or identify slots.

All 128 table positions now pass through the native publisher. Previously only
slots 0–95 were published. Profiles with keyboard actions in page3 poll that
page every four-request sweep, rather than once per 128 requests. Compact
profiles retain the existing Fn/WASD schedule. Reply discard on page changes,
exclusive sessions, command allowlist, release handling and process containment
remain in place. Pages are independently sampled, not an atomic snapshot.

The normal stale deadline remains 150 ms. Documented slow-transport retries
(5/10 ms send/read delays) use bounded 200/300 ms deadlines so a four-page sweep
does not regularly expire its own keys. Missing updates still release values;
there is no adaptive analog endpoint or digital fallback. Units follow vendor
USB/RF version rules (10/100/200 units per mm); full travel remains the existing
provisional 3.5 mm endpoint, which needs physical verification per revision.

All active ATTACK SHARK profiles show the existing orange testing banner with
Discord/QR invitation. Shared USB IDs without successful exact identification
only receive an unverified-family notice. X68 HE is no longer described as
frozen/unavailable. The warning state previously masked off flags above bit6,
which also lost MG75 Pro warnings; the mask now preserves all nine defined bits.

Existing X65/X68/X82 Pro geometry and automatic tokens are unchanged. Newly
admitted profiles have no invented geometry/token: manual layout selection
remains available. Matching a native protocol is not evidence of matching key
geometry. A complete physical layout batch is separate from this analog expansion.

## Evidence and reproducibility

See [family research](../research/ATTACK_SHARK_FAMILY_2026-09-20.md) for official
client provenance, the 37-entry registry, all firmware API results and the three
available firmware images. Independent depths are supported by component
emulation for X65 HE v309, X68 MAX v504 and X82 Pro HE v503. New family integration
has no physical-device validation; existing X65 Pro tester evidence does not
validate the other models or the still-open input-blocking report.

Raw factory/Fn records and exact chunk hashes are pinned in
[the profile manifest](../research/attackshark-family-profiles-20260920.json).
Run `python tools/generate_attackshark_family.py --check` to validate generated
C++ identities/maps. Model names below are manufacturer-client labels, not proof
of a particular retail revision's availability.

## Enabled exact profiles

| dev_id | Client model label | PID |
|---:|---|---|
| 3754 | R98PRO | 5029 |
| 3748 | R98GT | 5030 |
| 3737 | R98ULTRA | 5030 |
| 3743 | R98HE | 5029 |
| 2268 | X65HE | 502D |
| 2270 | X68HE | 502D |
| 2308 | X65PRO | 502F |
| 2370 | X68PRO HE | 502F |
| 2472 | X68HE | 502D |
| 2633 | Beat75 | 5030 |
| 2660 | X87Ultra | 502D |
| 2356 | X82PRO HE | 502F |
| 2755 | X68MAX | 502D |
| 2769 | X85Ultra | 5030 |
| 2793 | R86PROHE | 5030 |
| 2650 | X68Ultra | 5030 |
| 2798 | R82PROHE | 502F |
| 2833 | X68Ultra | 5030 |
| 2844 | R82HE | 502D |
| 2552 | K85 | 502D |
| 2901 | X68PRO HE | 502F |
| 2902 | X68HE | 502D |
| 2938 | X65PRO | 5030 |
| 2942 | X65 | 5029 |
| 2929 | X60 HE | 5029 |
| 2978 | K85PROHE | 5030 |
| 2964 | X98HE | 5030 |
| 2792 | X96HE | 5030 |
| 2968 | R85Ultra | 5030 |
| 2982 | R86PROHE | 5029 |
| 2852 | X87Ultra | 5030 |
| 3086 | X82HE | 5030 |
| 3123 | R85HE | 5029 |
| 2935 | X82PRO HE | 5030 |
| 3221 | X820pro | 5030 |
| 3334 | K85 | 502D |
| 3650 | R68HE | 502D |

## Validation and delivery

- Generated-profile reproducibility: PASS, 37 exact pairs and factory/Fn maps.
- Portable C++ protocol/model tests: PASS, exact PID mismatch rejection,
  all-profile page coverage, Fn routing/release order, scale, bounded freshness,
  malformed responses and independent depths.
- Existing Pro layout identity/geometry tests: PASS.
- Warning-state tests: PASS, all 512 masks, startup gating and metadata classification.
- Vendor JS/C++ wire comparison: PASS, six exact command frames, decoder, version
  offsets and all four USB collection filters; no hardware access.
- Full native static audit suite: PASS (`.local/attackshark-family-static.txt`).
- Ordinary Release build and linked self-tests: PASS
  (`.local/attackshark-family-build.txt`). The Shark self-test injects every
  factory-mapped position individually for all 37 profiles and checks native
  publication and release, including page3. Existing gamepad routing, Fn,
  stale/torn-page and child-process failure/timeout tests remain enabled.
- Build script also passed MINI60, NA87 and embedded ViGEm checks before replacement.
- Owner evaluates visuals; no visual run, device I/O test or GitHub publication.
- Existing ViGEm PDB linker warning remains; it does not prevent linking.

Artifact: `build/bin/Release/x64/HallJoy.exe`.
SHA256: `84f66bdf6f1b494448a65493f943598ec235446f20338bcd1140d00479c4fdfc`.

Pre-change backup: `.local/backups/attackshark-family-before-20260920.zip`.

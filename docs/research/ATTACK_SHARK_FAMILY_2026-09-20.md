# ATTACK SHARK RY5088 family evidence - 2026-09-20

The owner asked whether non-Pro support can be developed as one family rather
than requiring a separate firmware download for every model.

## Conclusion

Yes: the preserved manufacturer desktop/web-client catalog identifies 37
ATTACK SHARK magnetic keyboard revisions with the same RY5088 command class.
All import C from f9b6af43.js, declare magnetism: true in their registry entries,
and have 512-byte factory matrices (128 four-byte records). None overrides the
reviewed multi-depth request/decoder methods. Pro/non-Pro is not the wire-family
boundary. However, registry names are not proof every SKU is sold, and other
controller branches (including some K85 revisions) are not included by analogy.

The current qmk.top web bundle lacks these exact records; this evidence comes
from its preserved manufacturer desktop client, not a claimed current web listing.

Scope of acquisition: every one of these 37 Attack Shark revisions was queried
through the client's public firmware API. Three images are available; 34 return
Record not found. No ID enumeration outside manufacturer-listed devices.

| Image | dev_id | Version | Inflated bytes | SHA256 |
|---|---:|---|---:|---|
| X65HE | 2268 | v309 | 117852 | `503940d85d865339bf6250a3c1ad464303a760f3b6da6a6f43dd02fab833a2fa` |
| X68MAX | 2755 | v504 | 129312 | `d9044605e2b5b9447a2b250a5a307b16ca81b6d901f309e4f945eea3f95f3c78` |
| X82PRO HE | 2935 | v503 | 121480 | `0ce46d01a2e8d40b2728e7203ed7152cc64c577d309abd573ab93589d2990395` |

## New X68 MAX v504 component proof

Dispatcher 08012A26 calls E5 handler 08006310. FE branch 08006650 copies
64 bytes from 2000223C + page*64 to response 20008A26, using memcpy 08005664.
The scanner publication blocks 0800E462..0800E4D0 and
0800E986..0800EA0A write the per-slot table before consulting the 1B stream flag.
The publication cutoff is 20 raw units; the manufacturer's v5 scale is 200/mm.

`python tools/review_attackshark_x68max_20260920.py` PASS: 12 four-page pattern
reads and 16 simultaneous WASD hold/release transitions through actual firmware
instructions, with stream flag zero. RAM side-effect checks on the getter allow
only reply and saved-register stack. This is component execution with injected
already calculated travel, not ADC, full USB operation or physical typing tests.

Together with X65 HE v309 and X82 Pro HE v503, this supports reusing one reader
across exact known family profiles. Missing target firmware is a confidence gap,
not an absolute blocker to explicitly experimental support.

## Integration boundaries

- Identify exact dev_id with 8F and constrain USB transport; never admit all VID3151.
- Keep each revision's factory matrix and version-dependent travel scale.
- Prefer E5/FE page reads; do not enable 1B streaming or 1C/1E calibration.
- Validate model/version/map/range and repeated responses; reject invalid reads.
- Mark untested profiles experimental with the existing amber Discord notice.
- Shared protocol does not itself supply physical visual geometry or prove radio
  support, ordinary typing, Fn behavior and simultaneous gameplay on every model.
- Exact X68 HE firmware still missing. Existing Pro implementation also includes
  firmware-unavailable revisions, so demanding every non-Pro image would be an
  inconsistent integration criterion. Do not call those models firmware-verified.

No production backend, enabled model list or EXE changed in this research step.

## Exact reviewed profiles

| dev_id | Client model name | VID:PID | Model chunk | Firmware API |
|---:|---|---|---|---|
| 3754 | R98PRO | 3151:5029 | f16fb9db.js | Record not found |
| 3748 | R98GT | 3151:5030 | 8a56134a.js | Record not found |
| 3737 | R98ULTRA | 3151:5030 | 82cc4689.js | Record not found |
| 3743 | R98HE | 3151:5029 | c1907000.js | Record not found |
| 2268 | X65HE | 3151:502D | f159ac6b.js | v309 |
| 2270 | X68HE | 3151:502D | 0531c77c.js | Record not found |
| 2308 | X65PRO | 3151:502F | 6589a4f6.js | Record not found |
| 2370 | X68PRO HE | 3151:502F | c3329646.js | Record not found |
| 2472 | X68HE | 3151:502D | 88cf928a.js | Record not found |
| 2633 | Beat75 | 3151:5030 | 890cb764.js | Record not found |
| 2660 | X87Ultra | 3151:502D | dbe6f8ee.js | Record not found |
| 2356 | X82PRO HE | 3151:502F | 359ae4d7.js | Record not found |
| 2755 | X68MAX | 3151:502D | 5655bc90.js | v504 |
| 2769 | X85Ultra | 3151:5030 | 7ac6c491.js | Record not found |
| 2793 | R86PROHE | 3151:5030 | 3f42e682.js | Record not found |
| 2650 | X68Ultra | 3151:5030 | f05db737.js | Record not found |
| 2798 | R82PROHE | 3151:502F | 9cc64b86.js | Record not found |
| 2833 | X68Ultra | 3151:5030 | 192675ac.js | Record not found |
| 2844 | R82HE | 3151:502D | d7f79ed6.js | Record not found |
| 2552 | K85 | 3151:502D | 1609cc20.js | Record not found |
| 2901 | X68PRO HE | 3151:502F | c99d34c9.js | Record not found |
| 2902 | X68HE | 3151:502D | c99d34c9.js | Record not found |
| 2938 | X65PRO | 3151:5030 | aaf1260b.js | Record not found |
| 2942 | X65 | 3151:5029 | a983afeb.js | Record not found |
| 2929 | X60 HE | 3151:5029 | 12dbd197.js | Record not found |
| 2978 | K85PROHE | 3151:5030 | ae6f282f.js | Record not found |
| 2964 | X98HE | 3151:5030 | 553c2847.js | Record not found |
| 2792 | X96HE | 3151:5030 | 3cc92aa3.js | Record not found |
| 2968 | R85Ultra | 3151:5030 | 29072f29.js | Record not found |
| 2982 | R86PROHE | 3151:5029 | 84d66c66.js | Record not found |
| 2852 | X87Ultra | 3151:5030 | a35b3414.js | Record not found |
| 3086 | X82HE | 3151:5030 | 7ccd0aac.js | Record not found |
| 3123 | R85HE | 3151:5029 | f46676c2.js | Record not found |
| 2935 | X82PRO HE | 3151:5030 | fb3f6dd5.js | v503 |
| 3221 | X820pro | 3151:5030 | f81df2ca.js | Record not found |
| 3334 | K85 | 3151:502D | e9ff0e59.js | Record not found |
| 3650 | R68HE | 3151:502D | 427b7c3f.js | Record not found |

## Pinned client sources

- `index.2e5bd916.js`: `000d6f2f25cc31836e4a68cd1610b5e8c047a0d63216f623513062c26963ba07`.
- `0e38223b.js`: `1cc7d62aa1cfd29d10b2653652990cfef380b21fac2f401108c429dd6d71ff47`.
- `f9b6af43.js`: `1811416dc2213bfc0041bff89cc8e254e542f322762146223d1872dcee8c3870`.

Acquisition/class/matrix manifests and images are preserved under
.local/research/attackshark-pro (family-*-20260920.json).

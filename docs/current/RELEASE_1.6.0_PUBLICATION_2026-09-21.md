# HallJoy 1.6.0 publication - 2026-09-21

Published stable and latest: https://github.com/PashOK7/HallJoy/releases/tag/v1.6.0
Tag: 1c244bdb4a2d23314273793e116597f3bb9065d7.
Tested code parent: eda5f3e5f2cf57929bb5c27924a09cf606bc8a1a; tag adds only the
verified artifact documentation. Linux and Windows CI PASS:
https://github.com/PashOK7/HallJoy/actions/runs/35615925037.

EXE SHA256: 9d42bc7cc087404d29fcc8a7459b3c923ad16ba431e11764563be3b966e15358.
Assets: HallJoy.exe, LICENSE, THIRD_PARTY_NOTICES.md, SHA256SUMS.txt.
The EXE was downloaded from the draft and compared byte-for-byte with local
delivery. Public release metadata/digest, latest designation and tag were checked.
GitHub returned HTTP 500 to the publication request but readback confirmed the
operation completed; no duplicate release or repeated publication was attempted.

First CI passed functional checks but rejected C4267 in a bounded O3C key-code
conversion. An explicit uint16_t cast removed the warning without changing the
three key codes. Final EXE was rebuilt, resource-checked and uploaded before the
successful full CI/publication. Source docs audits were corrected for the current
output paths and hardware evidence policy. No warning gate was weakened.
NA87 and MINI60 support was already public in 1.5.3; earlier local-only statements
were corrected in 1.6.0 notes. X65 Pro ordinary support is new in this release.
No forced logging, visual tests or new hardware-validation claim.

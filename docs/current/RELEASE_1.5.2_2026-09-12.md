# Release 1.5.2

Owner confirmed the Block Bound Keys fixes work and authorized publication with
short English notes, explicitly no README changes. Runtime changes are documented
in BLOCK_BOUND_KEYS_CONTROLS_2026-09-09.md. Release rebuild changes version identity
only after that user test; EXE file/product version is 1.5.2.0.

Source commit: c2e5d287f439311b655afc577642b93e41192661.
All tracked src/tools/third_party files match the local build inputs byte-for-byte.
README is unchanged, SHA256:
EBA70A7C443BCEBF0B6E8DF91FD884C0EA4DC0535271CC8947556F37342D6E95.

Production build PASS; embedded installer and telemetry isolation gates PASS.
Only the established ViGEm LNK4099 missing-PDB warning occurred.
Local build log: .local/release-1.5.2-build.log.
EXE SHA256: 72107017E37CE3FFB30D09390003526BA08F8AD4B6496EC5AAC1703CC7CD9A0C.
The downloaded draft asset has the same hash. Assets: HallJoy.exe, LICENSE,
THIRD_PARTY_NOTICES.md; no application ZIP. Local 1.5.2 restarted.

CI run: https://github.com/PashOK7/HallJoy/actions/runs/34705114756.
Linux and full Windows CI PASS. Published as latest stable release:
https://github.com/PashOK7/HallJoy/releases/tag/v1.5.2.
Readback verifies the tag points at the CI-passing source commit above, the
release is public/non-prerelease, and the EXE asset digest matches the local hash.
Short public notes are RELEASE_NOTES_v1.5.2.md. README was not modified.

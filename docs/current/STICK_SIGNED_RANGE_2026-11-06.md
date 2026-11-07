# Full signed stick range — 2026-11-06

Owner relays a tester report that HallJoy outputs-32767..32767 and a browser
shows approximately-0.99997 instead of-1 at some negative endpoints.

## Finding

Microsoft documents every XINPUT_GAMEPAD stick axis as-32768..32767 with0 neutral:
https://learn.microsoft.com/en-us/windows/win32/api/xinput/ns-xinput-xinput_gamepad.
The production configured_xusb_builder.cpp Stick function clamped to[-1,1] but
multiplied both signs by32767. This made-32768 unreachable in the host-generated
virtual pad. The normalized difference is1/32768 (about0.00305%). Actual browser
normalization is implementation-dependent; this task establishes the signed
report endpoints, not a browser-specific benchmark or hardware measurement.

## Correction

Use32768 for negative float output and32767 for nonnegative output, then round
as before. All four stick axes and mouse-to-stick use that conversion. Curves,
deadzones, trigger scaling, bindings and SOCD/last-key priority stay unchanged.
Finite checks and clamping retain neutral nonfinite handling and valid limits.
XUSB adaptation already copies int16 values exactly and needs no format change.

Mouse/keyboard strength comparison now uses a common normalized scale; equal
full negative and positive endpoints remain equal-strength, preserving the mouse
winning a tie. Its32-bit intermediate products are at most1,073,741,824 and do
not overflow. The UI axis bar decodes each sign using its matching full scale.
Three production-linked backend self-tests now require-32768 at full negative
input instead of accepting the old-32767 endpoint; no tolerance was loosened.

The allocation-free shared C onboard mapper is corrected in the same way so
its existing exact differential tests retain parity with the host builder.
Its license/attribution is preserved. This is a source update for future custom
firmware builds, NOT a firmware flash or a replacement of already-built firmware.
Existing K4 onboard firmware directly owns hardware gamepad output and bypasses
the host-generated virtual pad. Updating HallJoy.exe alone does not change that
already-flashed mapper; an onboard firmware rebuild/update is separate work.
KeychronOnboard_ReadOsGamepad already used the proper signed scale and is unchanged.

## Validation

New configured builder regression exercises all65,536 signed axis codes through
the actual production builder AND XUSB adapter on all four axes. It also checks
overshoot/clamping and endpoints under plain, Snappy, last-key-priority and
combined modes; both mouse targets/polarities and equal-strength opposite
endpoints; existing neutral, nonfinite, triggers, buttons and state cases remain.
The existing40,000 onboard/host exact report/state comparisons and randomized
multikey differential regression cover the shared C mapper.

Cached native suite PASS, including all65,536 signed codes on four axes through
production builder and XUSB adapter, endpoint/clamping/mouse ties,40,000 exact
onboard/host comparisons and multikey parity. Existing research-reference checks
passed with their pinned inputs unchanged; no --record was needed.

Ordinary tools/build_release.ps1 EXIT0: complete native catalog, production-linked
backend self-tests, embedded resources/licenses, profile/autoselection/layout
catalog,87 Supported layout records and startup recovery18 scenarios plus repeated
startup PASS. Exact installed image equals the gated candidate. Logs are
native-checks.log and build-release.log in the private task directory.

Delivery: build/bin/Release/x64/HallJoy.exe, version1.6.14.0,
11336704 bytes, SHA256 `b78bd90c332807f70ee6cb7a435b3ae071d73bce644c916b4248228a0dc51f73`.
This supersedes the prior92c2001b local EXE and retains Keydous revision3 and all
approved functions. No GitHub publication, hardware/visual run or firmware flash.
Already-flashed K4 onboard output retains its firmware behavior, as explained above.
No device-specific support conclusion changed; no Sheet/status/layout mutation
was involved.

Private evidence: .local/research/stick-range-20261106/.
Verified source/docs/previous-EXE backup: .local/backups/stick-range-20261106.zip;
backup-manifest.json records each path/hash. Rollback restores the archived bytes.

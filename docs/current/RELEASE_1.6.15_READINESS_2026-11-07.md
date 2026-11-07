# HallJoy 1.6.15 release readiness — 2026-11-07

GitHub latest is [v1.6.14](https://github.com/PashOK7/HallJoy/releases/tag/v1.6.14).
Live main matches the workspace publication baseline
`2e027da920dcbdfaba7819cf5b03a59b28008b36`. The v1.6.15 tag was absent at review.

Current ordinary EXE is 1.6.14.0, SHA-256
`60d591d6c3ed2c63bc131aca78bbd9d0dd719322ccd1a9451614be976aae9498`.
The native suite, full-catalog candidate, linked profile/layout tests and startup
recovery passed for this source. The fresh Main catalog contains 669 model rows.
All 88 Supported rows match compiled layouts and all 284 yellow rows match
runtime notices. K70 PRO TKL and AK680 MAX HE are green. Keydous NJ98-CP V4S TMR,
Apex Pro TKL Gen 3 and VANGUARD PRO 96 are yellow. IPI Flash 75 PRO is yellow
with the layout-pending qualifier. HZ-94 has no Sheet row.

A scratch staged checkout of the verified baseline had 120 changed or new files
before this review note. The index audit passed. The private-data scan initially
found one workstation-specific build evidence directory in the VANGUARD report.
The report now points to its existing private build log. The staged scan passed
after the correction. No publication state was changed. Scratch evidence is
under `.local/research/release-readiness-1.6.15-20261107/`.

[Draft notes](../releases/RELEASE_NOTES_NEXT.md) reflect the actual delta.
K70 PRO TKL and AK680 MAX HE are ready to announce. Keydous wired input and the
Apex Gen 3 native path can be described with their observed experimental scope.
Keydous wireless startup and the Apex in-game symptom remain open. The K70
startup fix passed a transport test with simulated I/O. Physical confirmation
covers the preceding ISO build.

For the full proposed scope, VANGUARD PRO 96 still needs the exact ISO preset
and automatic selection for its known tester. The compiled catalog currently
has Corsair K70 PRO TKL presets. The VANGUARD ISO tester log validates depth and
release. Its virtual pad had no configured bindings. HZ-94 is a gated
diagnostic trial with an ANSI preset. The previous hardware log predates that
trial and cannot establish analog operation. Resolve its public scope before
listing it as working support.

Once scope is settled, write versioned notes and run the standard v1.6.15
versioned build, packaging and publication gates.

## Authorized release scope

The owner authorized v1.6.15 with the full negative stick range, completed
VANGUARD presets and no HZ-94 support announcement. VANGUARD now has all
three firmware-selected ANSI, ISO and JIS presets and remains Experimental.
The wired HZ-94 diagnostic trial remains available for its tester.

Fresh pre-publication Sheet readback covered 669 models and verified the exact
six affected rows, including strict dropdowns and base/effective status fills.
The 88 Supported-layout entries and all 284 runtime yellow notices reconcile.
No status edit was needed. See the [versioned patch notes](../releases/RELEASE_NOTES_v1.6.15.md).

The versioned profile gate exposed an export failure on the exact VANGUARD
ISO/JIS key legends. The simulator now writes the inventory explicitly as
UTF-8. Runtime layout geometry and labels are preserved.

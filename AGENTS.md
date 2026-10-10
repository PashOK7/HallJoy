# HallJoy task completion rules

Read `docs/current/OWNER_CONTEXT.md`, then `docs/README.md` and relevant current
documents before work. Parent AGENTS.md instructions continue to apply.

## Mandatory support-status synchronization

Whenever support changes, tester feedback changes a support conclusion, a
restriction is removed, or a release declares keyboard support, follow
`docs/development/SUPPORT_STATUS_SYNC.md` in the same task.

Do not finish after editing only code or README. Reconcile the exact affected
models with docs/SUPPORTED_HARDWARE.md and the live Google Sheet, read back Sheet
values/validation/colors, and record the result in current documentation.
Before publishing a release, check every support change since the prior release.
The owner has authorized these corresponding Sheet status updates; do not ask
again. If Sheet access fails, explicitly retain and report a pending sync item.
No physical test per model is required solely to award Supported when the known
protocol and implemented compatibility are established. Do not invent testing.

## Repository organization

Keep versioned patch notes in `docs/releases/`, detailed compatibility in
`docs/SUPPORTED_HARDWARE.md`, commercial terms in `docs/legal/`, and contribution
guidance in `.github/CONTRIBUTING.md`. Read `docs/current/PROJECT_LAYOUT.md` before
adding root files. Keep correspondence marked local-only out of publication.

## Firmware analysis decisions

Before proposing a firmware protocol integration, follow
`docs/development/FIRMWARE_BEHAVIOR_REVIEW.md`. Separate execution PASS from
suitability; surface typing loss, state changes and unknowns. Compare alternatives
and retain the best reviewed limited analog path with explicit restrictions when
no unrestricted path is established. Do not infer exhaustive absence from bounded
emulation. Do not use the retired MAD68 diagnostic directory for new builds.

## Distribution provenance

Before publishing vendor firmware, captured configurator code or vendor artwork,
record the source-specific redistribution permission. A public download URL and
HallJoy's root license are not such permission. Keep unclear acquisitions local.
Consult `docs/legal/DISTRIBUTION_AUDIT_2026-09-27.md` for unresolved published
materials and runtime notices. Preserve original third-party attribution; never
claim exclusive ownership or royalty clearance merely from a scanner result.

## Publication after the 2026-09-27 history cleanup

Vendor firmware/configurator/art/manual/copied-code files with unestablished
redistribution rights remain local. Run tools/check_publication_inputs.py against
the publication Git index before every push; adding a source-specific permission
record requires review, not just a download URL or the root project license.
Old local mirrors/backups contain removed history. Base future publication on
the cleaned remote main; never force-push stale mirrors or reintroduce old tags.
Preserve existing release asset IDs and download counters; do not delete/reupload
historical EXEs for source-history or legal-document cleanup.

## Tester delivery and diagnostics (owner decision, 2026-09-27)

Use HallJoy's normal workflow for tester investigations: deliver one HallJoy.exe;
collect the result in the existing HallJoy.log, accessible through Open log.
Do not require archives, CMD/PowerShell/Python scripts, command-line flags,
separate research exports, or manual assembly of diagnostic files from testers.
Integrate device-specific diagnostics into HallJoy's bounded background work and
existing log writer. Keep normal privacy/performance/lifecycle and logging rules;
this does not authorize forced logging for instability or limited-support banners.
Local agent research tools/artifacts remain allowed; they are not a tester workflow.
See docs/development/SUPPORT_DIAGNOSTICS_CONTRACT.md.


## Complete ordinary builds (owner decision, 2026-09-27)

Every delivered ordinary HallJoy.exe must retain all approved functionality,
including MAD68 V2 Dual limited stock analog with its red warning and no forced
logging, alongside the bounded Alumix104 investigation. Do not leave retained
implementations behind trial-only compile flags when delivering newer features.
Use tools/build_release.ps1 and its exact-image --halljoy-require-full-catalog
check. Keep the independent expected catalog explicit; do not generate the test
expectation from the same conditional manifest it must validate. New protocol
admissions must update this expectation; deliberate exclusions need an explicit
owner decision. Local builds are not GitHub publication authorization.

## Lean checks and releases (owner decision, 2026-10-11)

Checks must cost little compared with the change. For an ordinary change: run
`tools/run_native_backend_checks.py --require-compiler` once (the test cache is
valid; never `HALLJOY_NO_TEST_CACHE=1`) and `tools/build_release.ps1` once. Run
`research_reference_checks.py --record` only when a referenced source changed.
Release: write `docs/releases/RELEASE_NOTES_v<version>.md`, then
`python tools/release.py <version> "v<version>: title" --publish`. It bumps the
version, transfers under the publication policy, scans for private data, runs
the suite and the release build once, packages, tags, publishes, checks the
server-side asset hashes and records the publication. GitHub CI (Windows only)
is the clean-checkout run; do not repeat it locally. Do not rerun a passed
check without a change that affects it.

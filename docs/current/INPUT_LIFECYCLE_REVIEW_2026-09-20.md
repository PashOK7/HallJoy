# Input lifecycle review — 2026-09-20

Owner reports Block Bound Keys works normally on their keyboard. Review scope is
shared HallJoy input/output lifecycle, not renewed ATTACK SHARK protocol research.
That family remains paused pending a tester. No Forza root cause is established.

## Confirmed defect and fix

ExecutePause closed admission, stopped recovery, reset/published neutral state,
and only then joined realtime. Closing admission is an atomic flag checked at
Backend_Tick entry; it does not drain a tick already executing. Therefore the
owner could reset non-atomic report arrays and output schedulers while that tick
still accessed them, and the tick could publish active output after neutral.

Pause now closes admission, stops recovery, joins realtime, then resets/publishes
neutral and releases input/providers/backend leases. Failed-Resume cleanup now
uses the same join-before-reset ordering. A failed join returns a fault without
touching state still owned by the producer; existing child producer-freshness
protection remains in place. Normal analog scaling and keyboard block policy
are unchanged. Startup neutral publication still runs with admission closed.

The transaction regression models an already admitted tick finishing during join.
It failed before the production fix at the neutral-before-join assertion and
passes afterward. It also covers a failed join: no reset or resource release is
allowed after failure. This is deterministic transaction coverage, not an OS
thread-race reproduction or a hardware/game test.

## Shared path review

KeyboardBlockHookProc notifies the backend before deciding Windows suppression.
Block Bound Keys is not a gate in real analog reads or configured report building;
its reads in that path are diagnostic counters. Held digital presses deliberately
retain their original pass/block route until release, avoiding missing key-up.
Digital preview indicators are distinct from analog fill.

Missing/unowned source values go through source arbitration rather than digital
travel synthesis. The report-builder test covers release to zero; scheduler tests
cover obsolete pending updates; producer lease tests cover stale/wrong generations.
Profile read leases prevent partial profile application. These checks do not
prove every individual hardware backend's disconnect timing or game raw-input
behavior. No new fault was confirmed in Block Bound Keys itself in this review.

## Validation and delivery

Passed seven C++ test executables: engine runtime transaction, block key policy,
output scheduler, profile runtime gate, producer lease, fake child transport,
and configured XUSB builder. Five static audits passed: pause UI, admission,
runtime owner, profile gate and producer freshness. The initial manually assembled
builder test command omitted xusb_output_adapter.cpp; it was corrected to match
the existing runner, and the complete test passed.

Ordinary MSVC Release build and four linked-image checks passed via
`tools/build_release.ps1`. Existing ViGEm missing-PDB linker warning only.
The script handles replacement only after successful build/checks. No visual run,
physical-device/game test, or GitHub publication was performed.

Artifact: `build/bin/Release/x64/HallJoy.exe`.
SHA256: `7a139eebfac7786aca6647d77970a69c43dda7880e76b4f4c342cec87218e408`.
Evidence: `.local/input-lifecycle-review-tests.txt`,
`.local/input-lifecycle-builder-tests.txt`, `.local/input-lifecycle-release-build.txt`.
Source backup: `.local/backups/pause-order-before-20260920.zip`.

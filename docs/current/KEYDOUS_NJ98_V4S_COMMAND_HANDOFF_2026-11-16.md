# Keydous NJ98 V4S — late-page publication and control handoff, revision8

Owner requested actual fixes after [log59](KEYDOUS_NJ98_V4S_LOG59_2026-11-16.md).
No new model, digital-derived depth, calibration or persisted setting is added.
Existing97-key ANSI layout and auto-selection token6CB4C60ADBE352F9 are retained.

## Late responses now complete their original request

Previously Page() retried through Prepare(), which drained a late reply and then
issued another request. Production kept its ordering ticket in a loop-local
variable, so simply publishing a drained reply would also lose its original order.

Page() now resumes the same pending physical page. A late remote-ready reply
continues through F7/FC; a pending FC resumes ONLY GETs. The result is decoded
with the original units and returned as that original successful Page(), without
another E5 or another send callback. The backend keeps its original stream-order
ticket and retention policy across retries. Normal publication/release handling
therefore consumes the late value, while newer stream samples still supersede it.
Malformed replies, changed in-flight units and cancellation cannot publish it.
Cleanup draining for a different command remains separate from live Page() resume.

## Why remote requests can disappear

New own `tools/review_keydous_command_handoff.py` executes actual sibling0801
receiver SET_REPORT staging08007084..0800709A and forwarding080066F2..08006796.
Pinned binary SHA256
`ad3ab42828cf31ed9a7a1e7646462621c556d27ece5dfce590a11152e4ba2f55`.
Six cases cover8F,1B and all four E5 pages. SET_REPORT stores one pending flag and
one64-byte mailbox. Another SET, including F7, overwrites that mailbox before
the main-loop worker runs. USB completion alone is not forwarding completion.
This reproduces a way for HallJoy to wait forever for a command it erased.

Forwarding clears the old RF reply cache/read-ready and stages SPI TX, but does
NOT change the USB response buffer. Thus passive GETs cannot prove forwarding;
an initially considered USB-clear acknowledgement was disproved by execution and
is NOT implemented. The replay uses synthetic SRAM, not complete USB/RF timing,
and0801 is not the tester's0802 receiver. This is a source-backed counterexample,
not proof that every wait in log59 has this cause.

Exact retained official v4-index.js SHA256
`919b104944fb60e0c5c8a721b84138991087d69a45d6a7b42e7bd610d67aec0d`
has readMsg(t=10), waits t BEFORE checking remote-read readiness, and commonMsg
defaults to10ms send/read timing. Source: qmk.top/v4/, the previously acquired
manufacturer configurator. The zero-gap receiver rewrite removed that handoff
interval while keeping F7 SET polls. The new mailbox evidence invalidates the
assumption that readiness polling alone can safely replace every vendor wait.

For3151:5038 only, restore the manufacturer's minimum10ms handoff after a remote
SET or target selection and before any subsequent SET. Local F0/F7/FC exchanges
keep their existing response-driven proof. The interval is waited on the feature
worker, outside the publication lock; the independent1B reader has no new wait,
rate cap, deadzone or release-age rule. Fire-and-forget1B enable/disable also
finishes this handoff before returning. Cancellation is checked before/after the
bounded wait. HATOR379A and wired transport are unchanged. The shared HE108
3151:5038 receiver uses the same handoff; no admission or mapping change.

This is protocol pacing backed by the official client, not an asserted universal
MCU scheduling bound. A receiver whose worker takes longer, a genuinely lost RF
reply or a stale sensor cache may still fail. Pending requests are never blindly
resent, retyped or converted to analog zeros. New progress evidence reports their
original pending page/age so such a result remains visible. No arbitrary session
duration or new tester procedure is imposed.

## Executable coverage and diagnostics

The exact production Client, decoder, stream-order gate and physical publication
are used in six new positive cases: late remote-ready zero; late FC zero; a newer
stream sample beating a late zero; independent release of all97 channels; a
single-mailbox counterexample with/without the vendor interval; progress fields.
Four new negatives cover changed pending units, cancellation during handoff with
ambiguous-enable cleanup, cancellation during resume and malformed late depth.
The mailbox fixture uses a synthetic2ms worker within the vendor10ms interval;
it does not model unknown radio firmware scheduling. Existing16/4 synchronization
and4/3 trace cases remain: total26positive/11negative, registered for the ordinary
EXE. No physical keyboard is opened by these tests.

All necessary admission checks are retained: safe exact collection/receiver and
remote identity, transport/ownership, decoded scale/framing, cancellation and
live depth. Assignment/profile metadata, remaps, aliases/unbound keys, counts and
layout-cache availability remain nonblocking. A pending scale-change negative is
an internal ordering invariant; runtime units stay fixed within a session.

Revision8 adds bounded trace.keydous_rf.progress: resumed_pages, dispatch_waits,
dispatch_gap_us, pending_page and pending_age_us. Control evidence no longer
claims fixed_delay_us=0. Bootstrap/mode records expose the actual dispatch gap.
Existing bound-slot applied-zero, original stream/table timestamps and gamepad
evidence remain. Logging is optional; cached Open log still saves these records.
No raw USB payload, new key scope, serials or paths are added.

## Verification and support

Private `.local/research/keydous-command-handoff-20261116/` holds the verified
before-edit backup/manifest, sibling replay, vendor timing receipt and build/check
logs. Native suite and ordinary exact-image delivery gates passed. No GitHub
publication or physical test.
RF stays Experimental; USB confirmation and built-in ANSI layout are retained.
Fresh Sheet readback is recorded with the final receipt; no status promotion.

Delivered ordinary EXE `build/bin/Release/x64/HallJoy.exe`, version1.6.15.0,
11,895,296 bytes, SHA256
`a3f9341813eae34474c69434989f31358177d7a0a7160e5ab26ecb895d3e72d8`.
Native suite ran once with cache and passed. The ordinary wrapper ran once:
exact-image13-family gate passed, including Keydous26positive/11negative with
late_page_publication=1 and mailbox_handoff=1. Compiled catalog contains97 rows
for the exact Keydous preset; unchanged auto-selection token is verified by the
production-linked cases. Full catalog/K4/embedded notices/license checks, profile
transactions,92Supported-layout requirements and18startup-recovery cases passed.
No UI visual run or physical device command was performed by the tests.

Receipt: `.local/research/keydous-command-handoff-20261116/delivery.json` and
`.local/tester-delivery/<full-image-sha256>.json`. Fresh live Main!A473:C475
readback confirms Keydous row474 unchanged, strict status dropdown and yellow
B474:C474 RGB(1,.9019608,.6392157). All286 yellow rows reconcile against runtime
notices using the fresh full color/status snapshot. README/runtime notices and
hardware documentation retain USB confirmation and Experimental RF. No Sheet
write was necessary. The next tester result is a targeted revision8 hardware
check of these two fixes, using the ordinary EXE and Open log; no prescribed
duration or additional tester export. Hardware success remains unclaimed.

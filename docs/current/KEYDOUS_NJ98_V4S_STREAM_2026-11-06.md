# Keydous NJ98-CP V4S: direct RF stream confirmed — 2026-11-06

Follow-up: [revision3 connection/hold correction](KEYDOUS_NJ98_V4S_LIVENESS_2026-11-06.md) fixes the host depth-age policy discussed below. Initial access contention remains unresolved.

Latest tester feedback supersedes the unmeasured-stream conclusion of the
[access-failure report](KEYDOUS_NJ98_V4S_OPEN32_2026-11-06.md), while preserving
its observed error32 failure. Same local revision2 implementation as the
[gaming correction](KEYDOUS_NJ98_V4S_GAMING_2026-11-05.md); no new EXE built.

## Evidence

HallJoy-wireless-8k-v2.log,227404 bytes, SHA256
77cac736bf857788fe2a84d7e33d8c6c4026bbd1ababd37903d176647477d74d.
Header UTC2026-11-05T21:12:24Z / November6 Moscow. Four snapshots are complete,
summary-consistent, catalog27, with no queue loss. Filename/tester setting8k is
not independent proof of hardware polling configuration or8k analog delivery.

Exact receiver3151:5038 IF2 opens successfully on this run: ZERO recorded open
failures. Identity proves remote3793, USB0410, precision0,100 units/mm,4mm scale;
97 mapped keys. Exact_stream_pairs=1; mode revision2 reports fixed_delay_us=0,
stream_eligible=1,stream_requested=1,stream_start_error=0.

At uptime259031 ordinary continuous events report4,277 accepted, mapped1B
interrupt samples and stream I/O error0. These are real sensor depths consumed
on the dedicated reader thread; the implementation increments this counter only
for valid mapped stream samples, not table publications. This establishes actual
RF forwarding, not just the existence of a vendor method or a successful enable.
The latest native snapshot at253250 reports connected,4,256 combined updates,
141 failed table attempts and age0. Different timestamp/counter scopes explain
why combined updates there are below the later stream count.

819 trace.pad observations,723 nonzero, show varying depth, simultaneous axis
activity and return to neutral. Tester says tracking worked alright. Median
trace interval31ms is the bounded output-log cadence, not USB/analog polling rate
or measured end-to-end latency. No throughput benchmark or8k analog claim follows
from packet totals; delta streams include idle time and per-key packets.

## Table delays are not stream delays

First sweep66,494us; first8 mean50,419us. Later sweep summary has60 successful
pages /15 sweeps, mean2,373,344us, maximum17,968,984us and169 transient retries.
Retained attempts identify send_ready, online1/send0/read0, commandE5. No reader
restart or stream I/O failure is recorded; stream and gamepad continue while
feature commands wait. These table timeout counts do NOT count dropped1B packets.
Nor does zero I/O errors prove there were no RF-level losses; sequence/loss
information is not present. Table sweep frequency must not be presented as the
active stream's update frequency. The new separate metrics were useful here.

No final stream_cleanup or end evidence is present in the captured file.
SupportLog_Evidence is retained in memory rather than continuously appended;
a later ordinary session.end does not prove that cleanup failed or succeeded.
Physical exit/crash recovery remains unconfirmed.

## Startup reliability remains unresolved

The preceding file had73 consecutive error32 control-open failures. The tester
now explains: both builds initially failed, including after reboot; switching
back and forth eventually made the older build track, then the new one tracked.
This is observational order, NOT proof that the older version initialized or
unlocked firmware. No failed-old-build log or holder inventory was provided.
A reboot alone is not an established solution, and no specific vendor app/game
or HallJoy process is identified as the conflicting handle owner.

Current conclusion: real direct RF analog and virtual gamepad input are confirmed
once connected; reproducible startup and gaming suitability are not established.
Keep Experimental. No blind shared RF command handle or fixed delay is introduced
on the strength of this feedback. Preserve the genuine interrupt stream.

## Focused follow-up

Capture normal Open log when startup fails and when it succeeds using only the
new build, avoiding switching versions as a prerequisite. Note whether a game or
configurator was open; do not assume one was. Exercise a steady partial hold for
5–10s, simultaneous held keys and explicit releases. Code review still finds the
original1500ms per-key freshness policy in the RF Get path while table traffic
can remain busy much longer. This is a potential long-hold issue requiring a
focused check; the current trace does not establish a false release. Do not
claim that stream silence, which may be legitimate delta behavior, means loss.

Normal typing, physical shallow/full-range accuracy, loss recovery and cleanup
remain scoped unknowns. Exact MCU firmware still unavailable: no execution PASS.
No hardware event or sensor value is fabricated by HallJoy.

## Synchronization and artifacts

2026-11-06 fresh Sheet metadata Main/id0/grid1301x26; exact brand/model search
finds row472. Main!A471:C473 readback keeps C472 Implemented; awaiting hardware
testing, strict dropdown, yellow base/effective colors. No status mutation:
remaining startup/hold/lifecycle uncertainty prevents a general support claim.
Fresh all-row runtime notice reconciliation PASS283. README Experimental scope
remains accurate; hardware evidence and NEXT notes now acknowledge actual RF stream.

Private log, analysis.json, pad-summary.json, Sheet snapshot/readback and tester
feedback are in .local/research/keydous-nj98v4s-stream-confirmed-20261106/.
Verified documentation backup:
.local/backups/keydous-nj98v4s-stream-confirmed-20261106.zip.
Evidence-only change; source, EXE and release untouched. No build rerun necessary.

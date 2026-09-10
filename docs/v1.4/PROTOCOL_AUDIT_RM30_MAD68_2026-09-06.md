# RM-30-MAD68 — MADLIONS MAD 68 Pro R A0 protocol review

Date: 2026-09-06

## Exact route

Admission is restricted to `373B:1109`, audited `bcdDevice 0102`, the matching
vendor-HID envelope and the fixed 68-descriptor layout. There are 67 published
HID keys; the Fn descriptor is intentionally diagnostic-only. A matching brand
or PID alone does not acquire ownership.

## Wire and parser contract

| Traffic | Contract |
|---|---|
| A0 stream | 64-byte payload, header `A0`; descriptor bytes 1..3 must match the audited table; raw is big-endian bytes 4..5 and must not exceed 1600 |
| Control request | 64-byte `55` or `5F` framed zero payload with only opcode `A8` (arm/snapshot) or `A9` (restore ordinary input) |
| Control response | `AA` response with expected framing/opcode/checksum; checksum error `AB` is not success |

The private send boundary rejects every opcode other than A8/A9. Both are
reversible controls needed to leave the temporary stream arrangement in a
normal-input state; the primary transport receives a clean retry before finite
fallbacks. Recovery is bounded to the audited 13 strategies and two cycles per
60-second window. Exhaustion becomes passive/fail-closed rather than an
unbounded command loop.

## Publication and safety

- A raw sample is known only after descriptor lookup and 0..1600 validation.
  Startup sweep coverage cannot by itself grant full authority: a post-sweep
  physical edge must yield a fresh correlated A0, or the route remains passive
  / emergency-WASD as appropriate.
- Per-HID digital/A0 sequencing prevents a pre-edge packet from satisfying a
  later keyboard edge. A healthy key remains available when another key's A0
  sample is stale; W/A/S/D recovery is escalated only when the global stream is
  dead.
- Normal, short, unknown and checksum/control-shaped packets are separated.
  Stop uses cancellation and a bounded join, and an exit watchdog has one
  idempotent best-effort A9 restoration path.
- The common registry carries validated values through curve/UI/XUSB while the
  dedicated UAP child excludes only the exact claimed interface, leaving other
  analogue devices intact.

## Evidence and remaining gates

Physical evidence exists for the named MAD 68 Pro R and its W/A travel, but it
does not certify different firmware or a second keyboard. The portable parser
test and MAD68 protocol safety, route and cooperative-shutdown audits passed in
the full static suite. This review did not start HallJoy or send A8/A9 traffic;
post-change timing, recovery and multi-device behavior remain physical gates.

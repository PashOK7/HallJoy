# Frozen keyboard support notices — 2026-09-15

> 2026-09-19: [Ordinary NA87 and MINI60 support](NA87_MINI60_STANDARD_SUPPORT_2026-09-19.md) supersedes earlier experimental-only delivery and NA87 frozen status. Other frozen models are unchanged.

## Owner decision and evidence

IROK NA87 support is frozen pending further tester evidence. The tester verbally
reported success, thanked the owner and left the Discord server; no requested
follow-up log arrived within a day. Earlier captured analog-depth evidence remains
valid. This is not verified end-to-end controller behavior or a failed-support
report. NA87 remains enabled, including its normal gamepad output.

The owner approved keeping other frozen experimental protocols disabled. Registry:

| Model | Current build | Notice |
|---|---|---|
| IROK NA87 Mag ANSI | Enabled | Available, testing incomplete; may be unstable or not work |
| IROK NA87 Pro | Disabled | Frozen and unavailable in this build |
| IROK ND75 | Disabled | Frozen and unavailable in this build |
| AULA HERO84 HE | Enabled by subsequent owner decision | Available, testing incomplete; see HERO84_ENABLED_UNVERIFIED_2026-09-15.md |
| ROG Azoth 96 HE | Disabled | Frozen and unavailable in this build |
| Attack Shark X68 HE | Disabled | Frozen and unavailable in this build |

This list does not freeze every keyboard lacking a local hardware test. Keychron
custom UAP firmware decisions and other existing supported families are unchanged.
The initial UI-only change enabled no protocols. Subsequent owner authorization
enables HERO84; see HERO84_ENABLED_UNVERIFIED_2026-09-15.md.

## Implementation

keyboard_support_status.h keeps the model bits and metadata classifier. The
existing support card gains an amber variant, model-specific heading, English
explanation, existing Discord invite QR, Join Discord and Copy link actions.
The amber card is 154 logical pixels high, with wrapped text and a separate button
row. The red no-source card retains its existing dimensions and behavior.

Detection is independent of manual/automatic layout selection. A verified native
NA87 identity also raises its warning while analog works. Other keyboard sources
do not suppress a confirmed frozen-model warning. Startup/pause does not claim a
completed search. Status updates repaint/reposition the existing card, without
rebuilding keyboard keys.

A background Windows USB metadata inventory runs on first use and device topology
changes. There is no new timed rescan, HID open or vendor request. Results are
cached; superseded topology results are discarded. Product strings and USB IDs
are used only in memory and are not logged by the new inventory.

Shared VID/PID values alone cannot identify NA87 versus ND75, or HERO84 versus
other Addressed devices. Exact allowlisted names qualify a model-specific warning;
otherwise an unmatched experimental family gets a separate unverified-identity
notice only when no analog source is working. It does not claim a particular model
is connected or supported. Firmware with generic product names therefore gets a
family warning rather than a guessed model name. Pro secondary firmware IDs and
HERO84 UUID are not queried: their protocols remain disabled.

Identity evidence: IROK_NA87_RECON_2026-09-13.md and
IROK_ND75_M484_STATIC_ANALYSIS_2026-08-17.md under docs/research; existing ROG
backend constants; AULA_HERO84HE_DIAGNOSTIC_DESIGN.md; ATTACK_SHARK_X68_HE_RECON_2026-09-09.md.

Backup: .local/backups/frozen-support-20260915-170840/.
Validation results will be recorded after final build and checks. Visual approval
belongs to the owner; no interactive visual run is performed by the agent.

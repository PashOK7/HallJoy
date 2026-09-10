# RM-33 scenario qualification boundary

This is the authoritative boundary for performance and stability evidence.
Time alone never qualifies HallJoy: a passing duration window means only that
the measured resource signals stayed inside their stated limits for that exact
window and workload.

| Risk / question | Evidence | What it does not prove |
|---|---|---|
| Current supported analogue route reaches output and overlay | Isolated `run_input_pipeline_profile.ps1`: live UAP device plane, ViGEm publication, browser overlay and unchanged user state | Every keyboard family, key layout, reconnect, or sleep behaviour |
| Resource creep during a chosen workload | `run_long_soak.ps1` with an explicit duration and workload reason | Absence of faults that do not occur in that workload or window |
| Repeated normal startup/shutdown | `run_release_qualification.ps1` cycle evidence | Suspend/resume, unplug/reconnect, or held-key semantics |
| Overlay framing and hostile HTTP input | `run_production_smoke.ps1` overlay checks | Physical input semantics |
| Device removal/reconnect, sleep/resume, held-key recovery | Exact physical-device observation plus trace | A different keyboard, firmware, or transport |
| Driver missing/refused installation | Explicit owner-controlled driver test | Normal hardware input correctness |

No row may be promoted by a duration number. A duration run must name its
resource-risk hypothesis, workload and result; a 60-minute, 2-hour or 100-hour
window remains evidence only for that hypothesis. Hardware-specific rows stay
pending when that hardware is unavailable; this does not require the owner to
acquire hardware solely to run unrelated shared-path qualification.

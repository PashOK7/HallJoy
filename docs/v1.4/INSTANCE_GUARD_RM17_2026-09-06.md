# RM-17 per-user instance guard (2026-09-06)

HallJoy has one mutable settings root and one aggregate runtime/device-owner
role per Windows user. It now reserves that ownership with a named mutex in
the global namespace whose suffix is the current process user's SID. Therefore
two sessions of the same Windows account conflict, while different Windows
users do not share a guard.

The reservation is acquired after same-image child-role dispatch, but before
the logger, settings-root work, provider preparation, qualification report, or
application startup. A duplicate or a pre-created conflicting object is denied
before any of those resources are touched. Failure to determine the current
user or to create the object is also fail-closed.

This is intentionally distinct from protocol-specific locks such as
`DrunkDeerMtx`: the guard prevents two parent HallJoy processes from competing
for shared ownership; it does not claim that a device protocol is ready after
an abandoned or independently-held per-device lease.

The guard has no special bypass for the ordinary parent executable. Same-image
output host, installer, analog-host, and watchdog roles branch before it so
the parent can create/manage its required child roles. The explicit UAP
dual-capture test remains a parent-like operation and is consequently guarded.

Evidence currently consists of a structural startup-order audit and a focused
process-only Windows primitive harness: its child process proves that it cannot
acquire while the parent holds the guard, then the parent proves acquisition
after release. No HallJoy runtime, HID, controller, output child, or ROG
diagnostic was started.

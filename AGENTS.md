# HallJoy task completion rules

Read `docs/current/OWNER_CONTEXT.md`, then `docs/README.md` and relevant current
documents before work. Parent AGENTS.md instructions continue to apply.

## Mandatory support-status synchronization

Whenever support changes, tester feedback changes a support conclusion, a
restriction is removed, or a release declares keyboard support, follow
`docs/development/SUPPORT_STATUS_SYNC.md` in the same task.

Do not finish after editing only code or README. Reconcile the exact affected
models with SUPPORTED_HARDWARE.md and the live Google Sheet, read back Sheet
values/validation/colors, and record the result in current documentation.
Before publishing a release, check every support change since the prior release.
The owner has authorized these corresponding Sheet status updates; do not ask
again. If Sheet access fails, explicitly retain and report a pending sync item.
No physical test per model is required solely to award Supported when the known
protocol and implemented compatibility are established. Do not invent testing.

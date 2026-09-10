# RM-16 runtime command state design (2026-09-06)

`runtime_command_state.h` is the pure, allocation-free authority for future
global Pause/Resume transitions. Only the runtime command owner may mutate it;
UI may request an explicit transition and read `SnapshotV1`.

The state machine starts `Paused`: before the runtime owner has acquired every
provider and published a fresh neutral generation, it must not imply an active
lease. It closes admission before neutralization, exposes `Paused` after a
confirmed provider stop/release, and never opens admission on resume until a
fresh enumeration, proof and neutral-generation publication complete. A failed
fresh resume returns to `Paused` only after that attempt released all resources;
an unconfirmed stop is a terminal `PauseFaulted` state and rejects both requests.
The final `Active` state is followed by an explicit `ConfirmAdmissionOpen` only
after the concrete backend gate has opened; the snapshot never reports admitted
opens before that confirmation.

The portable test proves duplicate request semantics, monotonic command
generations, closed admission through the whole pause/resume path, safe
resume-abort semantics, and the fault barrier. It does not claim physical HID
release; wiring the one runtime owner is the next RM-16 step.

The backend now also exposes a single fail-closed runtime-admission gate. The
future owner closes it before pause neutralization, so realtime publication,
input callbacks and device-change refresh cannot revive a releasing generation.
It is intentionally default-open until the owner is wired, preserving the
current startup contract without presenting this foundation as Pause/Resume.

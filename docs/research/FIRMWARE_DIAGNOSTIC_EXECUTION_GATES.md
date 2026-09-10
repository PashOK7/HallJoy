# Firmware diagnostic execution gates

This file prevents a reverse-engineering observation from being presented as a
usable HallJoy implementation.

## Before changing production or diagnostic source

- State the exact user-visible outcome: log-only, correct per-key visual
  analogue, or game-input ownership. These are different deliverables.
- Establish the whole chain: firmware command -> report framing -> physical key
  identity -> HallJoy HID identity -> value normalization -> lifecycle exit.
- Mark every unresolved link explicitly. An unknown key-index-to-HID mapping
  blocks a per-key visualisation and all input ownership.

## Before starting a build

All of the following must be true:

- The emitted command set is proven reversible and its exit path is included in
  `Stop`, cancellation, and normal application shutdown.
- The reported record has a verified mapping to the keyboard key shown by
  HallJoy. Internal matrix indices must never be treated as HID usages.
- The value units and polarity have a documented normalization rule.
- The backend's `ownsHid` and `getMilli` behaviour matches the requested user
  outcome; a logger must not be described as a visual analogue backend.
- A static test/audit covers the new invariant.

If any gate is open, the work item remains reverse engineering. It must not be
called a test build, a working visualisation, or a candidate for a user.

## Reporting rule

Report only completed facts: a finished command, a completed build with an
artifact path/hash, or a verified test result. Do not report an intention as
ongoing work or say that a build is running. On discovering a contradiction,
correct the earlier claim immediately and withdraw the affected artifact.

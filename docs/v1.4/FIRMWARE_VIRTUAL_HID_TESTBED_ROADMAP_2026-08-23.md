# HallJoy firmware and virtual HID testbed roadmap

Date: 2026-08-23.

Status: mandatory long-term program; explicitly not a blocker for the next
stable HallJoy release.

## Objective

Build one reusable testbed that can present a hash-pinned keyboard firmware as a
virtual Windows HID device to an unmodified HallJoy, drive arbitrary analogue
matrix activity, and report exactly how strongly each observed behavior is
proved. The first target is IROK ND75 `X86HERGB`; the architecture must accept
additional firmware and MCU families without rewriting the HallJoy-facing
transport or scenario suite.

The testbed is a development tool. It never becomes an alternate production
input route and never changes HallJoy device admission.

## Non-negotiable invariants

- Every executable firmware image is identified by an exact cryptographic hash
  and a provenance record. Similar model names or VID/PID values are not enough.
- Unknown CPU instructions, calls, memory, MMIO, descriptors, commands, layouts,
  scales or timing behavior stop the affected test. They are never guessed.
- Digital Windows key events do not identify, learn or trigger analogue keys.
  Synthetic input enters before the selected sensor/depth boundary.
- Firmware execution stays in user mode. The Windows kernel component is a
  minimal bounded virtual-HID transport with strict report-size validation.
- HallJoy runs through its ordinary HID discovery, write/read, reconnect and
  output paths. A test-only HallJoy bypass is forbidden.
- Every result states its evidence level. Replay/model/slice/full-board results
  are not interchangeable, and none silently replaces physical qualification.
- Test traffic, reports and traces use explicit retention/privacy controls; raw
  user travel is not collected by default.

## Layered architecture

```text
scenario runner / matrix UI / fault injector
                    |
synthetic raw Hall samples or declared depth boundary
                    |
firmware profile + CPU executor + optional peripheral family pack
                    |
exact firmware-generated or explicitly modelled HID reports
                    |
bounded user-mode transport service
                    |
minimal Windows VHF driver
                    |
normal Windows HID stack -> unmodified HallJoy -> output/result oracle
```

The reverse path is equally important: HallJoy output/feature requests travel
through VHF to the user-mode target, then through the selected firmware command
handler or declared protocol model before a response is submitted.

## Evidence levels

| Level | Name | Minimum claim |
|---|---|---|
| E0 | Recorded trace replay | HallJoy handles one exact previously observed byte sequence |
| E1 | Protocol state model | HallJoy handles the proved descriptor/protocol state machine and generated edge cases |
| E2 | Original-code vertical slice | Selected original firmware instructions transform controlled inputs into observed reports |
| E3 | Reset-to-main board emulation | Original firmware boots against the declared CPU/peripheral model and drives virtual USB |
| P | Physical hardware | Real sensor, board, USB timing, reconnect and output behavior on the exact candidate |

An E3 result is stronger than E2 only for the peripherals and behavior actually
modelled. Physical level P remains separate.

## Firmware profile contract

Each profile records at least:

- firmware/container hash, source, extraction steps and redistribution status;
- CPU architecture, load base, vector table and memory regions;
- exact HID descriptors, VID/PID, usage, report IDs and report lengths;
- command/response handlers and firmware entry points used by E2;
- RAM structures, calibration/depth representation and matrix identity;
- allowed external calls and MMIO/peripheral models;
- timing/scheduler assumptions and declared synthetic-input boundary;
- supported evidence levels, expected failures and physical validation links.

Profiles are versioned data plus narrow adapters. Common CPU, memory, USB,
sensor and scenario code belongs to reusable family/core modules.

## Ordered implementation packages

### LAB-01 - Catalog, provenance and profile/evidence schema

- Inventory the existing M484 emulator, firmware corpus and exact ND75 material.
- Define machine-readable profile, provenance and evidence-result schemas.
- Preserve fail-closed firmware-byte and instruction-byte verification.
- Add deterministic intake that classifies known images, containers, likely CPU
  and closest family without claiming unsupported execution.

Gate: two existing M484 profiles and exact ND75 are distinguishable by hash and
cannot borrow each other's protocol claims accidentally.

### LAB-02 - Windows virtual HID transport

- Implement one minimal KMDF/VHF driver with fixed validation and no firmware
  parser or emulator in kernel mode.
- Implement the bounded user-mode service/control plane for descriptors, output
  requests, input submission, disconnect/reconnect and explicit fault injection.
- Test installation/uninstallation and local development signing separately from
  any future distributable signing decision.

Gate: an unmodified HallJoy discovers only the declared virtual device, sends
its real startup requests, receives reports, observes removal/reappearance and
cannot overrun either transport direction.

### LAB-03 - Exact ND75 X86HERGB hybrid vertical slice

- Create a separate exact ND75 profile rather than reusing a generic M484
  command profile.
- Execute the strongest recoverable original-code chain from synthetic raw Hall
  input through calibration/normalization, depth/change/subscription logic and
  the 64-byte live report.
- Extend execution only when a missing component prevents a named observation;
  unknown dependencies remain visible failures.

Gate: controlled partial press, full press, release and simultaneous keys reach
HallJoy through reports produced by verified bytes from the exact image, with an
instruction trace and no digital correlation.

### LAB-04 - Automated HallJoy qualification scenarios

- Cover every declared matrix position and depth range, multi-key transitions,
  high-rate streams, lost release, malformed/truncated/duplicate traffic, stalls,
  endpoint busy, device removal and reconnect.
- Observe HallJoy mappings, neutralization, freshness, lifecycle, ViGEm progress
  and clean shutdown through production-linked output/result oracles.
- Produce a bounded report containing artifact hashes, profile, evidence level,
  scenario coverage and explicit unknown/unproved boundaries.

Gate: deterministic repeated runs produce the same verdict and a weaker
simulator level can never satisfy a physical or exact-final-artifact gate.

### LAB-05 - Multi-firmware and family packs

- Add firmware versions by structural diff/symbol relocation before manual
  address duplication.
- Add reusable M484 peripheral/sensor models, then other Cortex-M, RISC-V or
  additional CPU families when a real target requires them.
- Classify encrypted, compressed, partial updater and bootloader-only images as
  such; never execute unrelated host updaters as firmware.

Gate: onboarding a related firmware normally adds a profile/diff, while a new
MCU adds a family pack. Neither requires a new Windows HID driver or HallJoy test
suite.

### LAB-06 - Optional full reset-to-main emulation

- Extend the same executor with clock, NVIC/SysTick, USB device controller,
  timers, DMA, GPIO/ADC, flash, watchdog and scheduler models as demanded by a
  concrete unanswered test.
- Reuse LAB-01 profiles, LAB-02 transport and LAB-04 scenarios unchanged.
- Compare E3 output against E2 and physical traces to detect incorrect peripheral
  assumptions.

Gate: the exact image reaches its normal USB/application loop from reset without
patched control flow; every modelled peripheral and every remaining stub is
enumerated in the result.

## Scheduling and release relationship

Release-blocking HallJoy packages `R0..R8` retain priority. LAB work may proceed
in isolated checkpoints when it does not change production routing, invalidate
current evidence or delay a named release blocker. Lack of LAB completion cannot
hold the next stable release, but the program remains in `ROADMAP.md` until all
six stages are either completed or explicitly replaced by a documented better
architecture.

The testbed can later reduce repeated user diagnostics and strengthen regression
coverage. It cannot retroactively certify hardware that was never physically
tested, nor justify admitting an unknown keyboard to production.

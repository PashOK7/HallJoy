# HallJoy v1.4 decision log

Decisions are append-only. A superseding decision references the old ID instead
of silently rewriting it.

## D-001 - Product version is v1.4

Date: 2026-07-31

The next public HallJoy version is `v1.4`. The imported `v3.9.0` label describes
the source archive only and is retained solely in historical evidence paths and
provenance records.

## D-002 - The advanced archive is the integration baseline

Date: 2026-07-31

The advanced archive contains the more complete architecture and becomes the
v1.4 code baseline. v1.3 is not merged file-by-file. Valuable v1.3 behavior is
ported deliberately and tested against the new architecture.

## D-003 - Preserve both source lines before integration

Date: 2026-07-31

The v1.3 self-contained SDK work is preserved at commit `b3fefce`. The imported
archive is preserved at commit `f5e8c18`. Later packages must remain reviewable
against both points.

## D-004 - v1.4 documentation is normative

Date: 2026-07-31

`docs/v1.4` owns current status. Imported `docs/stability` files are immutable
historical evidence unless a correction is clearly marked as an erratum.

## D-005 - Documentation is part of each package gate

Date: 2026-07-31

Code, tests, roadmap, risk status, validation evidence, and worklog changes are
committed together. A code-only package cannot be marked complete.

## D-006 - No GitHub publication during integration

Date: 2026-07-31

Branches and commits remain local until explicit approval. `main`, tag `v1.4`,
and GitHub Releases are unchanged during implementation.

## D-007 - Analog support is self-contained

Date: 2026-07-31

HallJoy may require the ViGEmBus system driver, but it must not require a
system-wide Wooting Analog SDK or Universal Analog Plugin installation. The
private UAP runtime is versioned, integrity-checked, and stored in a writable
per-user location when the executable directory is protected.

## D-008 - Simulation and hardware evidence remain separate

Date: 2026-07-31

A development-only simulated analog backend will exercise the common
aggregation, curve, SOCD, ViGEm, telemetry, hotplug, and fault paths on machines
without an analog keyboard. It is excluded from production builds by default,
uses an explicit test launch mode, and cannot mark MAD68, Hex80, Addressed,
SparkLink, Sayo, or UAP device gates as verified.

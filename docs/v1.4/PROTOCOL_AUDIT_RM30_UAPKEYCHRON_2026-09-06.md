# RM-30-UAPKEYCHRON protocol audit — 2026-09-06

## Scope and admission

This card reviews the embedded Universal Analog Plugin (UAP) Keychron/Lemokey
reader, not a new native HallJoy backend. Admission is restricted to the
vendor analogue collection `FF60:0061` and the explicitly routed Keychron
`3434` / Lemokey `362D` product layouts. A VID alone is not protocol proof.

The K4 HE ANSI route is exact: `3434:0E40`, six rows by nineteen columns, and
the 114-slot layout is pinned to Keychron QMK commit
`bc56b3c611dcc1a8ed9a2acb8bdc4da5e1a80c27`. The companion static audit proves
that this matrix has 100 physical keys and is routed only for that PID.

## Wire contracts and performance boundary

The reader first asks `A9/01` for the analogue-matrix version. The established
custom full-report K4 firmware advertises its capability with marker `0x45` and
then accepts `A9/31`; one request returns four *untagged* 32-byte `A9/31`
reports. Each report supplies 30 travel bytes after its two-byte command
header, giving 120 bytes for the 114 matrix slots (the remaining six are
padding). This is an existing, physically proven read-only ABI, not an inferred
generic Keychron feature.

The fallback `A9/30` reads one matrix location at a time. It is retained for
protocol-compatible devices, but it is not a gaming-quality route for stock K4
firmware: a physical audit measured roughly 2–3 ms for one vendor round trip,
and a formerly idle key can wait for the incremental sweep. The K4 release
claim therefore remains limited to the custom `A9/31` firmware; this card does
not widen support or change firmware/configuration.

```
A9/01 -> validated reply length >= 3 -> version / capability marker
A9/31 -> four exact 32-byte A9/31 replies -> 4 × 30 ordered travels -> layout -> publication
A9/30 -> validated reply length before version-specific travel byte -> keyed publication
```

## Safety correction

The UAP reader previously treated every nonempty matching `A9/31` packet as a
payload fragment and used `size() - 2` as its copied length. A short matching
reply could underflow that expression and read outside the frame; a short
version/per-key response could similarly be indexed before its required byte
was present. The reader now:

- stops on a failed request send instead of consuming a possibly stale reply;
- requires version replies to contain byte 2 and per-key replies to contain the
  selected version-specific travel byte;
- accepts a full snapshot only when all four replies are exactly 32 bytes; and
- copies four fixed 30-byte payload spans only after that validation.

This preserves arrival order because the established firmware does not expose a
part index. It cannot prove that packets belong to separate open HID handles;
the existing process-isolation rule (D-076) remains necessary for physical
tests. No speculative timeout, exclusive-open policy, new layout, or
configuration write was added.

## Evidence and remaining gate

- Parser/static: PASS — `keychron_k4he_static_audit.py` now locks layout,
  admission, send/length gates and exact full-frame contract.
- Locked dependency overlay: PASS after recording the updated
  `AnalogueKeyboard.cpp` SHA-256.
- Existing physical evidence: the custom K4 `A9/31` path previously measured
  immediate early travel, coherent multi-key updates, releases, reconnect and
  about 182 Hz after the Windows receive fix.
- Current-session hardware: not run. No UAP executable, HID session, firmware
  action or ROG Azoth diagnostic was started.

The untagged multi-frame response has no host-visible transaction identity.
Cross-process contention is therefore an evidence-bound protocol limitation,
not something this parser can honestly solve without a changed device ABI or
an independently proven exclusive-open policy.

# MAD 68 Pro R A8/A9/A0 critique resolution and Native Stable v3.5

## Evidence

The review independently examined firmware image
`35ed6b7a73cfe7e7fd222e6862032e49.bin` (SHA-256
`2a7df4ffc491476b79da5333f51179f68fbbb5c5e146e987cfb5df764b79cead`),
RV32/WCH disassembly at base `0x5000` with `+xwchc`, the complete physical
`HallJoyMAD68ProR.log`, the independent A8/A9/A0 critique, and the HallJoy,
UAP/Soup, UI, and ViGEm paths.

## Corrected conclusion

`A9 -> A8 -> A9` leaves Hall telemetry enabled while restoring ordinary digital
keyboard service. The original apparent contradiction came from conflating two
independent state variables.

| State | RAM address | A8 | A9 |
|---|---:|---:|---:|
| service/input suppression | `0x20002B40` (`gp-0x760`) | set to 1 | cleared |
| forced sweep count | `0x20002B3F` (`gp-0x761`) | set to 3 | unchanged |
| telemetry gate | `0x2000580B`, bit `0x08` | set | unchanged |
| auxiliary 72-byte array | `0x20004BE4` | filled with `0xFF` | cleared |

A8's handler calls `0x530E`, which sets both suppression and telemetry plus the
three-sweep count. A9 clears only suppression and the auxiliary array; it never
loads `0x2000580B`. The A0 scheduler explicitly tests bit `0x08` there.

## Why post-A9 A0 traffic was not queued backlog

The physical log records A8 ACK at `06:22:26.751`, final A9 TX at the same
timestamp, and final A9 ACK at `06:22:26.761`. It then contains 272 A0 reports
after A9 TX, 271 after A9 ACK, four exact ordered 68-descriptor cycles after TX,
three complete cycles after ACK, a 15 ms median interval, and a fourth-cycle end
4.236 seconds after ACK.

Firmware limited to approximately one report per 15 ms could not have generated
272 reports during the 7-10 ms A8/A9 interval, and Windows cannot queue reports
that do not yet exist. The backlog explanation is therefore excluded.

Although A8 sets the forced count to three, a fourth cycle is expected: three
forced passes are followed by a change-driven catch-up pass while mirror/source
arrays differ. Once the mirror is synchronized, reporting becomes quiet until a
new change.

## Confirmed limitations from the critique

The stock scheduler's 15-tick limiter yields an aggregate ceiling near 66.7
reports/s. One report contains one key, one 72-slot pass takes about 1.08 s, and
steady scanning sends only the first changed slot before restarting at slot 0.
Low slots can therefore delay high slots during simultaneous movement. WASD
positions are A=9, D=10, S=55, and W=64; continuously changing A/D may delay S/W.
This is a stock-firmware constraint, not a HallJoy scheduling choice.

The original hardware capture proved forced/catch-up passes and live analogue
values, but did not include a distinct press beginning at least five seconds
after A8. A separate post-sweep steady-state test remained necessary.

HallJoy must keep command traffic (`AA`, `AB`, `5F`) separate from asynchronous
Hall reports (`A0` plus a known descriptor). It never sends host opcode A0; the
runtime allowlist contains A8 and A9 only.

The physical log confirms the main value as big-endian A0 bytes `4..5`:

```text
raw = (packet[4] << 8) | packet[5]
normalized = clamp(raw, 0, 1600) / 1600.0
```

A moved approximately `5 -> 1565 -> 0`; W moved `0 -> 1600 -> 0`. Descriptor
changes matched the physical keys, all 68 descriptors matched firmware, and no
checksum or malformed-report error occurred.

## Native Stable v3.5 safeguards

### Startup does not prove steady state

Initial 68/68 coverage enables only Emergency WASD. Full 67-key native ownership
requires a later A0 correlated with a physical Raw Input edge after the startup
window. The decisive evidence marker is `STEADY-STATE A0 CONFIRMED`.

Raw Input publishes the analogue sample sequence/value before releasing the
digital sequence. MAD68 owns a HID usage after an edge only when a newer A0 has
arrived, or a slightly earlier report already matches the new digital state.
Timestamp proximity alone cannot make stale input authoritative.

### Per-key fallback

If one key lacks fresh A0, only that usage returns to UAP/digital fallback. Other
fresh MAD68 keys continue natively; one starved high slot does not retrigger A8;
global recovery begins only when the entire A0 transport dies. This prevents a
stale ViGEm axis without repeatedly disrupting the firmware scheduler.

Emergency WASD uses the ordinary HallJoy path, not a test-only renderer:

```text
MAD68 A0 -> ReadRaw01Cached -> curves/filter -> g_uiAnalogM
          -> standard keyboard depth bar
          -> remap -> BuildReportForPad -> ViGEmBus
```

### UAP coexistence, startup, and recovery

Regular UAP targets are unchanged. Only the dedicated native UAP excludes VID
373B/PID 1109, before Soup calls `CreateFileW`; all other Wooting/UAP devices
retain the existing behavior and `UAP_DISABLE_HOTPLUG=1` policy.

Startup order is embedded-UAP preparation, Backend/UAP initialization,
target-scoped Raw Input registration, MAD68 worker start, then correlated
A9/A8/A9 activation. `GetAsyncKeyState` also blocks A8 when a key was already
held before HallJoy launched.

Normal shutdown sends best-effort A9; a separate exit watchdog repeats the
idempotent A9 after HallJoy finishes. The hard command allowlist permits only A8
and A9, and active commands require `bcdDevice 0x0102`. A9 restores digital
service; it is not intended to clear the persistent telemetry bit.

Every five seconds diagnostics summarized aggregate A0 rate and gaps, WASD
raw/age/ownership/digital state, coverage and ordered cycles, publication mode,
edge latency/semantic match, per-key starvation, and global transport failure.

## Required follow-up hardware scenario

1. Launch HallJoy and remain idle for at least five seconds.
2. Slowly press and release W, A, S, and D individually.
3. Confirm `STEADY-STATE A0 CONFIRMED`.
4. Confirm standard keyboard depth bars.
5. Bind WASD to a ViGEm stick/trigger.
6. Test A+D, W+S, and all four simultaneously.
7. Analyze per-key latency, fallback, A0 rate, and gaps.

## Historical production verdict

Proven: A9 does not disable telemetry; A9/A8/A9 produces a full stream after the
final A9; A0 bytes `4..5` are the `0..1600` analogue coordinate; all 68
descriptors are known; the standard UI/ViGEm path is integrated; UAP/Wooting is
preserved; and stale per-key values cannot hold ViGEm output.

Not yet proven in this record: steady-state event frequency/fairness after
startup catch-up and smoothness of four simultaneously moving WASD keys. If the
post-sweep test produced no correlated A0, stock firmware would be unusable. If
it worked but starved S/W under simultaneous movement, single/stable input would
remain functional while high-quality multi-key analogue would require a denser
firmware report.

# Titan68 Turbo trace analysis

After the tester returns `HallJoyStabilityTrace.log`, run:

```powershell
python .\tools\analyze_titan68_turbo_trace.py <path-to-HallJoyStabilityTrace.log>
```

The parser makes no USB calls. It verifies that the log contains exactly the
three approved frames (`0x37,01`, `0x36,01`, `0x36,00`), rejects an extra or
missing control frame, detects control rejection or transport errors, counts
each HID report ID, and summarises the decoded `raw12` range per firmware key
index. `0x37,00` is intentionally absent: the tester restores that RAM-only
state by reconnecting the keyboard after the diagnostic closes.

`VERDICT: REPORT-07 RAW12 OBSERVED` proves that this physical unit sent paired
raw values through the analysed transport. It does not alone prove typing
coexistence: review the target-scoped raw-key entries and the tester's
before/during/after observations. A missing report-07 stream is useful
negative evidence; do not retry the diagnostic automatically.

The parser contract is regression-tested without hardware by
`src/HallJoyProject/tests/titan68_turbo_trace_analyzer_test.py` against a
minimal valid fixture.

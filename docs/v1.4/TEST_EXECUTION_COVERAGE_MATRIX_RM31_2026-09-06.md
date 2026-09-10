# RM-31 execution coverage matrix — 2026-09-06

The authoritative, machine-checked matrix is the source list in
`tools/run_native_backend_checks.py`: each explicit entry lists the production
translation units linked with its test. `*_protocol_test.cpp` is a deliberate
second route: the runner derives the matching `<family>_protocol.cpp` and fails
if it is absent. This keeps a new protocol family from being silently skipped.

| Execution class | Production/function scope | Fixture/effect scope | Build configuration | Current result |
| --- | --- | --- | --- | --- |
| `portable-explicit` | Explicit source list in the native runner: providers, lifecycle, workers, persistence parsers, curves, scheduler, output channel, UAP ownership and other pure/process-safe units | In-memory fixtures, fake-only transports, temporary files or Windows child/process contracts as named by the test | Host C++ compiler; Windows-only entries are conditional | PASS/FAIL is produced by `run_native_backend_checks.py`; it is not hardware qualification |
| `portable-protocol-convention` | `<family>_protocol.cpp` paired with `<family>_protocol_test.cpp` (Aula, W669, Hero diagnostic, DrunkDeer, Hex80, IROK and MAD68 families currently present) | Valid, malformed, boundary and release fixtures owned by each protocol test | Host C++ compiler | Source counterpart is required before compilation |
| `sanitizer-health-only` | No production unit: intentional one-byte OOB proves the loaded sanitizer can report a fault | The failure itself is the oracle | ASan+UBSan portable runners only | Must fail with an ASan diagnostic before covered normal suites may pass |
| `isolated-simulator-only` | Production-linked profile transaction path, including startup/rejection behavior | A unique temporary data root; runner also asserts backend initialization is forbidden | Explicit Analog Simulator build | Unsupported in this session because launching HallJoy is intentionally prohibited while the owner may be gaming |

`test_execution_coverage_static_audit.py` enumerates every `*_test.cpp` and
requires exactly one declared execution class. At this audit point there are 65
test translation units: 63 portable routes, one sanitizer-only control and one
isolated-simulator route. The latter is intentionally **not** relabelled as a
portable PASS, while the health control is intentionally **not** linked into
ordinary production tests.

The two routes that cannot be treated as ordinary portable production evidence
remain explicit rather than unlinked. This matrix records the build and effect
boundary; it does not convert static, fake, sanitizer or simulator evidence into
real keyboard, controller or firmware evidence.

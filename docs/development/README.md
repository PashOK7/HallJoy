# Protocol development

Read these documents in order:

1. `ARCHITECTURE_OVERVIEW.md` — data flow, startup phases, and forbidden
   integration shortcuts.
2. `NEW_PROTOCOL_WORKSHEET.md` — evidence to collect before changing runtime
   code.
3. `ADDING_NATIVE_ANALOG_PROTOCOL.md` — generator and implementation process.
4. `NATIVE_BACKEND_CONTRACT.md` — exact discovery, lifecycle, and data
   boundaries.
5. `PROTOCOL_REVIEW_CHECKLIST.md` — safety and review gate.
6. `TESTING_NEW_PROTOCOL.md` — portable, Windows, and hardware validation.

Create an inert module with `tools/new_native_backend.py`, then validate it with:

```text
python tools/run_native_backend_checks.py --require-compiler
BUILD.cmd
```

Existing reference implementations:

- reversible streaming control: `mad68pr_backend.cpp`;
- full-matrix polling: `hex80_backend.cpp`;
- addressed priority polling: `addressed_analog_backend.cpp`;
- integrated row/depth protocols: `backend_sparklink.inc` and
  `backend_sayo.inc`.

## Runtime stabilization

Lifecycle, IPC, persistence, and performance work is managed separately in
`../stability/README.md`. Changes are delivered as small packages with a
mandatory baseline and regression gate.

# Protocol development

Read in this order:

1. `ARCHITECTURE_OVERVIEW.md` — data flow, startup phases and forbidden integration shortcuts.
2. `NEW_PROTOCOL_WORKSHEET.md` — evidence to collect before touching runtime code.
3. `ADDING_NATIVE_ANALOG_PROTOCOL.md` — generator and implementation procedure.
4. `NATIVE_BACKEND_CONTRACT.md` — exact descriptor/lifecycle/data boundaries.
5. `PROTOCOL_REVIEW_CHECKLIST.md` — safety and review gate.
6. `TESTING_NEW_PROTOCOL.md` — portable, Windows and hardware validation.

Create an inert module with `tools/new_native_backend.py`. Validate it with:

```text
python tools/run_native_backend_checks.py --require-compiler
BUILD.cmd
```

Existing reference implementations:

- stream/reversible control: `mad68pr_backend.cpp`;
- full matrix polling: `hex80_backend.cpp`;
- addressed priority polling: `addressed_analog_backend.cpp`;
- integrated row/depth protocols: `backend_sparklink.inc`, `backend_sayo.inc`.

## Стабилизация runtime

План lifecycle, IPC, persistence и performance-работ ведётся отдельно в `../stability/README_RU.md`. Изменения выполняются малыми пакетами с обязательным baseline/regression gate.

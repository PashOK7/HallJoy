# Titan68 Turbo static audit — 2026-09-05

This audit is intentionally limited to evidence available without a physical
keyboard. It supports the diagnostic executable; it does not claim that every
retail unit has passed a runtime test.

| Requirement | Evidence | Result |
|---|---|---|
| Official source artifacts | Normal updater SHA-256 `DA9003A241A102B16541341533FB43248175A71D67A89861E64B0FF680F7EFE6`; carbon updater SHA-256 `FDB08E5B311171B4C8E24A0D9FE0F3516DC9C2A1C34846BE7F1055542CDE0CF3`. | Proven |
| Reproducible normal application image | Overlay extraction from `0x391E80`: `0x1FE50` bytes, SHA-256 `119984FBC70A971A912E5190193B2349210AE3F69C11655A7906F5772D26CE96`, Cortex-M0 vector SP `0x20017068`, reset `0x0800822D`. | Proven |
| Exact admission interface | Official V2 configuration maps PID `0x31FD`; diagnostic requires VID:PID `28E9:31FD`, usage `FF87:0020`, input IDs `06`/`07`, output ID `06`, and 64-byte input/output report sizes. | Proven |
| Command safety boundary | The normal and carbon dispatchers share the `0x36` branch at `0x08005A70`: nonzero data sets and zero clears bit 1 of volatile `0x20010D0A`. `0x37,01` is the explicit host setter of bit `0x04`; it copies existing mapped calibration data to RAM. The diagnostic allows exactly `0x37,01`, `0x36,01`, and `0x36,00`. | Proven for the reviewed V1.21 images |
| No persistent write in allowed path | `0x36` changes volatile RAM state; `0x37,01` calls the read/copy helper `FUN_08007F94`. The persistent primitive is `FUN_08007FA4`, reached on `0x37,00` via `FUN_080015E8`; static audit rejects that command. The diagnostic has no firmware/reset/setting opcode or feature report API. | Proven for the reviewed V1.21 images |
| Raw analogue transport | Normal `FUN_08012508` and carbon `FUN_08012514` construct `07,keyIndex,valuePart` records, emit two bit-6-marked six-bit fragments, then send 3 bytes through USB IN endpoint `0x82`. | Proven |
| Static coexistence boundary | The report-07 scan path checks mode mask `0x3C`, excluding the bit-1 simulation flag. The outer event loop invokes the scan routine followed by normal key-delta handling. | Evidence against a simple scan shutdown; not runtime proof |
| Diagnostic implementation | Exact-device enumeration, receive/transmit logging, target-scoped raw-key logging, one attempt per launch, `0x36,00` without `0x37,00`, no game-input ownership, and pre-control shutdown gate. | Statically audited |
| Reproducible executable | `titan68_turbo_diagnostic_static_audit.py` passes; Release x64 rebuild succeeds. Output SHA-256: `8DC2269B540FCDD26A879B3241221272BA13FFAF03190863F10AEFD72C9602E4`. | Proven |

## What static analysis cannot prove

The remaining runtime gate is narrow but essential: a real exact-interface
keyboard must confirm that `0x37,01` followed by `0x36,01` produces report-ID
`07` records under its installed firmware, identify the corresponding physical
keys, and demonstrate typing during the ten-second combined-mode window.
The supplied diagnostic logs precisely those facts without making a gameplay
or firmware-changing claim.

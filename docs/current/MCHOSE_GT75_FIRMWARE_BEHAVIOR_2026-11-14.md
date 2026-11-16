# MCHOSE GT75 V1.08 — firmware behavior

**Protocol assessment: LIMITED_ONLY**

Execution: pass — expectations reproduced; this is not a support verdict.

Scope: Exact Oct6 V108 image first-bank file-offset fragments and manufacturer host API; not physical USB validation or full dual-core boot.

## Best reviewed option

Selected: a0_debug
Best reviewed limited normal analog path. No calibration, map, polling-rate or firmware writes; only guarded reversible debug-bit RMW.
Search limits: Bounded first-bank static/branch analysis. No claim of exhaustive commands, full boot, perfect physical USB or typing.

- **a0_debug [limited]**: Only reviewed normal multikey depth stream; preserves individual measured values andFn. Known firmware cutoff/snap and persistent flag are disclosed.
- **e0_mix87_memory [rejected]**: GT75 dispatches E0 to default, so a PID-only Mix87 patch cannot initialize its state reader.
- **a0_getter [rejected]**: Request opcodeA0 reads saved rapid-trigger configuration, not current depth.
- **a8_calibration [rejected]**: A8 changes calibration baseline and is not an analog subscription.

## Restrictions and unknowns

- **shallow_input [restricted]**: Firmware reports depths0..9 as zero before transmission. HallJoy cannot recover this discarded input.
  Evidence: branch_replay, branch-replay.json, /samples
- **temporal_filtering [restricted]**: Firmware snaps depth>=maximum-5 to maximum. Host adds no debounce, rate cap or release timer. ADC filtering before cached depth remains outside replay.
  Evidence: branch_replay, branch-replay.json, /samples
- **persistent_settings [restricted]**: 06 persists debug bit through a4096-byte flash RMW; replay confirms all other sector bytes preserved. Active RAM byte1 is reloaded and masked0x0e. Reset-armed settings[0]&1 trigger MCU reset, so automatic writes are forbidden there.
  Evidence: branch_replay, branch-replay.json, /writers
- **crash_recovery [restricted]**: Debug flag is persistent; process crash/power loss before cleanup can leave it enabled. A pre-enabled flag is preserved rather than stolen by a later session.
  Evidence: branch_replay, branch-replay.json, /writers
- **normal_typing [unknown]**: Normal HID typing coexistence remains unmeasured; debug producer alone does not establish text delivery.
- **packet_loss [unknown]**: Busy flag20178039 gates sends; cache is updated before frame construction. Physical USB queue/release loss cannot be ruled out by these bounded fragments.
- **configurator_conflict [unknown]**: Exclusive control handle prevents simultaneous normal access; previously changed settings/profile can invalidate transaction. Physical app handoff remains unmeasured.
- **simultaneous_keys [established]**: ServiceCB4C scans96 slots with81 physical descriptors and independent caches;648 builder executions cover every physical channel. This is source evidence, not lossless USB proof.
  Evidence: static, gt75-v108.bin, CB12..CBB4 and136F4 descriptor table
- **depth_scale [established]**: Builder carries big-endian depth and per-switch maximum. Host uses depth/declaredMaximum, includingFn; no fixed guessed travel.
  Evidence: branch_replay, branch-replay.json, /samples
- **release [established]**: Per-channel builder returns explicit zero; service compares each slot with its last cache. Host only applies received zero and never digitally guesses release. Physical delivery remains unknown.
  Evidence: static, gt75-v108.bin, CB4C..CBB4;C870..C9EC
- **physical_mapping [established]**: 81 descriptors in first96 slots;F0FF01 isFn. Manufacturer exact geometry has81 keys plus2 display-only RT controls.
  Evidence: static, gt75-v108.bin, 136F4..13874
- **normal_exit [established]**: Same writer supports bit clear with whole-sector preservation. Host attempts guarded restoration only for its owned bit; transport failure may prevent it.
  Evidence: branch_replay, branch-replay.json, /writers

## Execution boundaries

- outcome: pass
- entry_boundary: Writer5688; sampleC870
- stop_boundary: Return1000/reset5754; sampleC9EC before ADC/USB tail
- mocked: Synthetic RAM, flash helper effects and private CSR800; normalized file-offset PCs; exact instruction bytes.
- not_modeled: Boot/interrupts/ADC calibration/physical USB, second-core execution, actual typing and MCU flash endurance.

Evidence references/hashes validate provenance, not the truth of an assertion.
No unknown property is silently accepted; no automatic public status changes.

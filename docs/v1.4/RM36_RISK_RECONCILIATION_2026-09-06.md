# RM-36 — reconciliation P0/P1 register

Дата: 2026-09-06. Это текущая карта решений поверх исторического
`RISK_TEST_MANIFEST_V1.json`. Поле `MISSING` в исходном JSON означает, что на
момент создания manifest не была зарегистрирована нужная связь, а не что
сегодня автоматически доказан defect. Нельзя читать эту таблицу как blanket
release PASS.

## Источник и границы

- `python tools/run_native_backend_checks.py --require-compiler` завершился
  успешно 2026-09-06; это source/portable evidence, не hardware proof.
- `test_execution_coverage_static_audit.py` подтвердил 65/65 test translation
  units с ровно одной execution class.
- RM-33/RM-34 evidence привязано к SHA-256
  `F2068156F7874FCD4CE4D677BBA048793D6706179637932553104BE90ADAEC58` и
  описано в `RM34_RELEASE_EVIDENCE_2026-09-06.md`.

Статусы: **SOURCE_LOCAL** — найдены и пройдены named source/portable oracle;
**HARDWARE_PENDING** — только точная физическая проверка может закрыть
остаток; **RELEASE_PENDING** — нужен выбор/внешнее действие при выпуске.

| Risk ID | Current disposition | Current oracle / remaining exact gate |
|---|---|---|
| HJ-V14-P0-001 | SOURCE_LOCAL | `uap_provider_v2_static_audit`, `provider_v2_controller_shadow_test`; physical identity/UI/output remains RM-34 conditional. |
| HJ-V14-P0-002 | SOURCE_LOCAL | private UAP ABI/parser/lifecycle static and portable routes; each unowned device family remains conditional, not accepted by this result. |
| HJ-V14-P0-003 | SOURCE_LOCAL | `sayo_letter_matcher_test` and RM-03 ambiguity oracle; exact physical map/depth still needs its family gate. |
| HJ-V14-P0-004 | HARDWARE_PENDING | Aula W669 parser/session source gate passes; stream silence/lost release needs exact board evidence. |
| HJ-V14-P0-005 | HARDWARE_PENDING | ViGEm process/freshness/stress gates pass; long active final-artifact continuity is device-specific. |
| HJ-V14-P0-006 | HARDWARE_PENDING | Protected-handle/process containment source and real-child gates pass; corrected-artifact V75/IROK continuity remains physical. |
| HJ-V14-P1-008 | HARDWARE_PENDING | MAD68/UAP containment is source-reviewed; old MAD68 HE trace comparison and retest are unavailable here. |
| HJ-V14-P1-009 | HARDWARE_PENDING | `hex80_protocol_test` and routing/shutdown audits pass; permanent chunk failure needs exact device/firmware. |
| HJ-V14-P1-010 | HARDWARE_PENDING | Addressed scheduler/lifecycle tests and real established route are retained; variant-layout fallback is not guessed without that variant. |
| HJ-V14-P1-011 | SOURCE_LOCAL | shared identity and `extended_key_bindings_test`/Provider V2 source gates cover representation; device layout proof remains family-specific. |
| HJ-V14-P1-012 | HARDWARE_PENDING | Spark admission/freshness source gates pass; layout/scale/duplicate truth needs exact sibling hardware. |
| HJ-V14-P1-013 | HARDWARE_PENDING | descriptor admission is fail-closed; sibling protocol/layout equality requires each physical sibling. |
| HJ-V14-P1-014 | SOURCE_LOCAL | native interface-claim registry and source-arbitration tests pass; no broad VID/PID claim is inferred. |
| HJ-V14-P1-015 | SOURCE_LOCAL | transaction/profile runtime gates cover atomic settings/bindings; exact release profile remains state-verified by runners. |
| HJ-V14-P1-016 | SOURCE_LOCAL | bounded profile, binding, UI/overlay and output source gates pass; no claim about a new device layout. |
| HJ-V14-P1-017 | HARDWARE_PENDING | raw/float representation source gates pass; early physical travel precision needs an exact keyboard. |
| HJ-V14-P1-018 | SOURCE_LOCAL | RM-33 profiler plus hot-path/wake static checks; its scoped measurements do not establish a universal latency limit. |
| HJ-V14-P1-019 | HARDWARE_PENDING | poll pacing policy/source oracle passes; capability-derived rate needs device-specific observations. |
| HJ-V14-P1-020 | SOURCE_LOCAL | `provider_v2_data_plane_*` and controller-shadow gates reject fixed payload truncation. |
| HJ-V14-P1-021 | SOURCE_LOCAL | parent/child V2 plane and capability separation source gates pass; exact physical restart stays RM-34 conditional. |
| HJ-V14-P1-022 | SOURCE_LOCAL | common provider identity/snapshot broker tests and projection gates pass; protocol support remains separate. |
| HJ-V14-P1-023 | SOURCE_LOCAL | native worker exception/lifecycle/process-containment gates pass. |
| HJ-V14-P1-024 | SOURCE_LOCAL | roadmap compatibility contracts and named negative old-bug oracles are catalogued; no product behavior was silently rewritten. |
| HJ-V14-P1-025 | SOURCE_LOCAL | mutation/parser/negative source gates run through the unified native suite. |
| HJ-V14-P1-026 | SOURCE_LOCAL | `TEST_EXECUTION_COVERAGE_MATRIX_RM31_2026-09-06.md` plus 65-route static audit define execution tiers. |
| HJ-V14-P1-027 | SOURCE_LOCAL | `SANITIZER_HEALTH_RM31_2026-09-06.md` and sanitizer health control remain required source gates. |
| HJ-V14-P1-028 | SOURCE_LOCAL | tracked HID/native snapshot interleaving tests and claim registry audit pass. |
| HJ-V14-P1-029 | SOURCE_LOCAL | generation supervisor, completion and readiness callback source matrices pass. |
| HJ-V14-P1-030 | SOURCE_LOCAL | slow provider/realtime ownership and bounded publication source oracles pass; hardware latency is separately conditional. |
| HJ-V14-P1-031 | SOURCE_LOCAL | never-completing worker/child containment is covered by lifecycle and supervisor fault gates. |
| HJ-V14-P1-032 | SOURCE_LOCAL | pre-provisioned shutdown/supervisor failure matrices pass in source/portable gates. |
| HJ-V14-P1-033 | SOURCE_LOCAL | runtime command, release/neutral/resume transaction tests and static audit pass. |
| HJ-V14-P1-034 | SOURCE_LOCAL | inherited-handle/PID/generation and arbitrary-DLL boundary gates pass; signing remains a release task. |
| HJ-V14-P1-035 | SOURCE_LOCAL | bounded INI/document/parser fuzz and numeric tests pass. |
| HJ-V14-P1-036 | SOURCE_LOCAL | diagnostic logger redaction and production artifact diagnostic-log absence gates pass. |
| HJ-V14-P1-037 | RELEASE_PENDING | signed exact-artifact provenance requires a selected signing/publishing policy and final bytes. |
| HJ-V14-P1-038 | SOURCE_LOCAL | Sayo lifecycle/absent-backend shutdown is covered by native lifecycle source gates; physical Sayo remains family-specific. |
| HJ-V14-P1-039 | SOURCE_LOCAL | RM-04 per-row freshness old-bug oracle and `sparklink_row_freshness_test` pass; exact Spark hardware remains conditional. |

## Result

There is no unassigned P0/P1.  Three residual kinds are intentionally not
collapsed into PASS: physical behaviour for unavailable exact hardware,
final-release signing/provenance, and the explicit per-family protocol gates.
`check_rm36_risk_reconciliation.py` requires every historical P0/P1 ID to have
exactly one current row and rejects an unknown disposition.

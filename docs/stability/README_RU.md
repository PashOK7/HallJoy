# Стабилизация HallJoy v3.9.0

Этот каталог — единая точка управления работами по повышению стабильности и производительности без изменения пользовательского поведения.

## Документы

1. `STABILITY_MASTER_PLAN_RU.md` — обязательный порядок работ, тестовые шлюзы и критерии завершения.
2. `RISK_REGISTER_RU.md` — реестр всех проблем аудита и привязка к небольшим пакетам работ.
3. `BASELINE_V3_9_0_RU.md` — исходная точка, контрольные суммы и воспроизводимые проверки.
4. `WORKLOG_RU.md` — журнал фактически выполненных изменений и результатов тестов.
5. `DECISIONS_RU.md` — архитектурные решения и причины выбора.
6. `AUDIT_V3_9_0_RU.md` — полный исходный технический аудит.
7. `STAGE_S02A1_RESULT_RU.md` — разбор и исправление детерминированного build-blocker в Soup patcher.
8. `STAGE_S02B1_RESULT_RU.md` — exception barriers для MAD68/Hex80 native workers.
9. `STAGE_S02B2_RESULT_RU.md` — совмещённая C++/SEH exception boundary для SparkLink.
10. `STAGE_S02B3_RESULT_RU.md` — exception boundaries main/reader для Addressed backend.
11. `VALIDATION_MATRIX_RU.md` — доступные устройства, обязательные gates и правила отложенной внешней проверки.
12. `STAGE_S02V1_RESULT_RU.md` — временная evidence trace и критерии машинной приёмки.
13. `TRACE_PROTOCOL_S02V1_RU.md` — формат, privacy/performance contract и правила удаления логирования.
14. `STAGE_S06_RESULT_RU.md` — ownership-safe Addressed overlapped cancellation и bounded outer stop.
15. `STAGE_S07_HEX80_RESULT_RU.md` — bounded Hex80 lifecycle, ownership и timeout containment.
16. `STAGE_S07_MAD68_RESULT_RU.md` — bounded MAD68 lifecycle с сохранением A9 recovery.
17. `baseline/` — машинно проверяемые артефакты исходного состояния.

## Текущее состояние

- Этап 01: планирование и фиксация baseline — завершён.
- Пакет `S01 — lifecycle contracts and test scaffolding` — завершён.
- `S02A` и build-hotfix `S02A.1` прошли чистую Windows/MSVC x64 Release сборку и runtime smoke test.
- Realtime, diagnostic writer и overlay worker входят через общий allocation-free `noexcept` barrier.
- `S02B.1` локально завершён для MAD68 и Hex80: fault-safe publication reset и reap-before-replace для `std::thread`.
- Доступный SparkLink regression smoke полного S02B.1 пакета пройден; MAD68/Hex80 device gates отложены до предфинального внешнего теста владельцами устройств.
- `S02B.2` прошёл Windows/MSVC и реальный SparkLink hardware gate.
- `S02B.3` локально завершён для Addressed main/reader exception containment; общий ручной Windows/SparkLink smoke пройден, Addressed device gate отложен.
- `S06` реализован в V14-12B: cross-thread close активного Addressed HID I/O удалён, внешний stop ограничен тремя секундами, timeout сохраняет поколение и переводит registry в poisoned containment; реальный Addressed device gate отложен.
- Hex80-часть `S07` реализована в V14-12C: waitable generation, cooperative cancellation, трёхсекундный truthful join и simulator containment прошли; Hex80 device gate отложен, MAD68 остаётся следующим отдельным пакетом.
- MAD68-часть `S07` реализована в V14-12D: owner отменяет только persistent read, A8 после stop запрещён, финальный A9 сохранён, bounded containment прошёл. Кодовая часть S07 завершена; физические native gates отложены.
- V14-12E добавил воспроизводимый normal-operation runner: post-build 25/25
  запусков и штатных `WM_CLOSE` завершились exit 0, без оставшихся процессов,
  trace ERROR и изменений 11 пользовательских файлов. Это pilot S21, не
  замена 1000 циклам, 8-24-часовому soak и hardware matrix.
- Статусы всех 45 общих `HJ-AUD-*` синхронизированы с release-регистром по уже
  существующим evidence; остаются 8 Open, 3 Implemented, 1 Partial и 33 Verified.
- `S02V1` локально добавил временную bounded evidence trace, машинный анализатор и сборщик log bundle; Windows/SparkLink evidence gate ожидается.
- Normal MAD68/Hex80/SparkLink hot paths, Addressed polling loop, reader I/O loop и packet constructors сохранены относительно предыдущих пакетов.
- Риск `HJ-AUD-P1-004` остаётся `Partial` только из-за отложенных device-owner
  и финальных soak gates; Sayo и UAP code-level gates уже закрыты V14-06F и
  V14-07C.
- GitHub и любые удалённые репозитории в процессе не используются.

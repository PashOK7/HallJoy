# HallJoy release-readiness mega audit and mandatory roadmap

## 2026-09-06 — Owner clarification: preserve automatic Sayo letter mapping

The owner requires automatic user-configured letters with no manual assignment,
setup wizard or mandatory confirmation, and reports no complaints about current
behavior. This supersedes earlier blanket prohibitions on Sayo letter learning:
physical depth remains measured independently; automatic letter association is
intentional. RM-03 now preserves it and tests ambiguous correlation (addedCount==1
already guards multiple new HID keys in one report; cross-report candidates need
review). No per-press activation-threshold delay was established. Treat stale-depth
fallback separately according to its intended digital/analog contract. Do not
remove Sayo, replace letters with physical-only controls, or label learning itself
P0. Reconcile old HJ-V14-P0-003 subclaims rather than copying its blanket verdict.
See FULL_AUDIT_EXECUTION_ROADMAP_2026-09-06.md v1.1, section 0, for other potential
intent traps: fallback/shadow removal, single-instance, async save, process
isolation and release scope. Proposed rewrites require Purpose/compatibility
review and evidence; product behavior changes require an explicit product decision.
Only documentation changed in this clarification; no new runtime PASS is claimed.


## 2026-09-06 — Full audit execution roadmap (planning, not qualification)

New detailed execution entry point:
[FULL_AUDIT_EXECUTION_ROADMAP_2026-09-06.md](FULL_AUDIT_EXECUTION_ROADMAP_2026-09-06.md).
It contains 37 packages / 185 steps, 17 protocol-family review cards, dependencies,
negative checks and acceptance criteria, plus 234-file inventory with review depth
and hashes. All implementation tasks start TODO; historical P0/P1 and release gates
remain authoritative until reconciled with current source/evidence. Current reads
confirm Sayo digital-derived mapping and the SparkLink row-freshness gap; first
packages after RM-00/01 are RM-03/04/05. Native contract, containment and host DLL
trust also require dedicated work. Production files and EXE were not changed;
no runtime/input/controller/hardware tests ran for this audit. A comprehensive
roadmap is not a claim of a completed line-by-line or hardware audit.


Дата аудита: 21 августа 2026 года.

Статус следующей стабильной версии: **BLOCKED**.

Этот документ является единым планом исполнения перед следующим stable
release. Он не переписывает подробные профильные аудиты и не создаёт второй
конкурирующий risk register. Техническое доказательство остаётся в профильном
аудите, идентификатор и статус — в `RISK_REGISTER.md`, порядок — здесь и в
`ROADMAP.md`, а фактический результат — в `VALIDATION_MATRIX.md`.

Каждый package этого плана выполняется по обязательному
`ENGINEERING_WORKING_METHOD.md`: сначала evidence/invariants и old-bug oracle,
затем письменное сравнение локального root fix, staged migration и чистого
redesign/rewrite, и только после выбора лучшей архитектуры начинается
production edit. Скорость и размер diff не имеют приоритета над correctness.

## Итоговый вердикт

Текущий код нельзя выпускать как новую стабильную версию. Уже есть хороший
фундамент, физически подтверждённые DrunkDeer G65, Aula, Redragon и IROK
сценарии, пройденные build/test packages и безопасный one-click установщик
ViGEmBus. Но поздние end-to-end аудиты нашли системные P0/P1, а новые IROK-логи
физически воспроизвели потерю виртуального аналогового выхода при полностью
живом SparkLink input.

Непосредственно перед freeze IROK продолжал читать клавиатуру без ошибок:
`279189/279189` route queries. Остановился ViGEm output worker: ожидание его
thread handle вернуло `WAIT_FAILED` / `ERROR_INVALID_HANDLE`, lifecycle стал
`Poisoned`, а watchdog затем бесконечно повторял заведомо заблокированный
recovery. Во втором запуске ViGEm прожил 160 секунд, но shutdown был отравлен
независимым `sayo stop.lock_timeout`. Поэтому нельзя исправлять только
SparkLink, увеличивать timeout или просить пользователя снова собирать общую
матрицу.

Следующий release допустим только после корневых исправлений, old-bug negative
oracles и квалификации **одного и того же финального подписанного EXE**.

Текущий основной integration ledger содержит шесть открытых P0
(`HJ-V14-P0-001..006`) и 31 последовательный открытый P1
(`HJ-V14-P1-009..039`). Отдельно остаётся partial `HJ-V14-P1-008` и
исторические/UI/device P1 со статусами, которые нужно либо подтвердить на
финальном кандидате, либо честно исключить. Поэтому это не список из трёх
локальных hotfixes, а управляемая многоэтапная release-blocking программа.

## Источники истины без дублирования

| Область | Подробный владелец | Блокирующие ID | Роль этого документа |
|---|---|---|---|
| UAP/Soup families | `UAP_ALL_KEYBOARDS_AUDIT.md` | `HJ-V14-P0-001..002` | порядок и общий gate |
| Production native families | `NATIVE_ALL_KEYBOARDS_AUDIT.md` | `HJ-V14-P0-003..004`, `HJ-V14-P1-009..013`, `039` | порядок и hardware policy |
| Общий analog/XUSB path | `COMMON_ANALOG_PIPELINE_AUDIT.md` | `HJ-V14-P1-014..016` | зависимость от identity/provider contract |
| Precision, capacity, rate | `ARTIFICIAL_LIMITS_PERFORMANCE_AUDIT.md` | `HJ-V14-P1-017..020` | performance acceptance |
| Provider/IPC architecture | `INPUT_PROVIDER_ARCHITECTURE_AUDIT.md` | `HJ-V14-P1-021..023` | целевая архитектура миграции |
| Доверие к тестам | `TEST_EVIDENCE_TRUST_AUDIT.md` | `HJ-V14-P1-024..027` | regression-first и exact artifact |
| Concurrency/lifecycle | `CONCURRENCY_LIFECYCLE_OWNERSHIP_AUDIT.md` | `HJ-V14-P0-005..006`, `HJ-V14-P1-028..032`, `038` | hard liveness и ownership |
| Глобальная пауза/HID lease | `GLOBAL_PAUSE_DEVICE_LEASE_AUDIT.md` | `HJ-V14-P1-033` | neutral/release/resume transaction |
| Trust/security | `TRUST_SECURITY_BOUNDARY_AUDIT.md` | `HJ-V14-P1-034..037` | release trust chain |
| Исторические/UI/device ledgers | `RISK_REGISTER.md` | все P1 не в состоянии `Verified`/`Closed` | reconciliation перед freeze |

Если формулировки расходятся, запрещающая политика
`CORRECTNESS_RELEASE_BLOCKERS.md` и более новый физический факт имеют приоритет
над старым `PASS` с более узкой границей.

## Неприкосновенные продуктовые инварианты

1. Каждая реально аналоговая клавиша поддерживаемой модели, включая Fn,
   Menu, media/OEM и modifiers, имеет стабильную 16-битную identity через
   parser, provider, curve, binding, profile, UI, overlay и output mapping.
2. Цифровой Windows keydown никогда не выбирает, не обучает и не запускает
   аналоговую клавишу. Он может быть отдельным явно маркированным digital
   source только после независимого source arbitration.
3. Partial packet, умерший row/chunk, disconnect или provider crash никогда не
   оставляет ненулевой axis/button навсегда. Neutralization предшествует
   release/restart.
4. Realtime path не выполняет HID enumeration/open/proof/stop/join, файловый
   I/O или иной потенциально блокирующий lifecycle.
5. Resource живёт не меньше всех publishers/users. Atomic `HANDLE` или
   увеличенный timeout не заменяет generation ownership.
6. Source precision сохраняется до curve/output boundary; capacity и rate не
   обрезаются молча magic-константами.
7. Profiles/configuration публикуются одной проверенной immutable generation,
   а не частично во время работы.
8. Production route либо полностью проходит обязательные gates, либо физически
   отсутствует в release build и в публичном списке поддержки. Надпись
   `experimental` без compile/catalog exclusion недостаточна.
9. ViGEmBus ставится one-click из точного встроенного официального пакета без
   runtime download; пользователь явно подтверждает UAC.
10. Финальный релизный hash относится к уже подписанному и полностью
    квалифицированному artifact; после квалификации EXE не пересобирается.

## Новое физическое доказательство IROK/ViGEm

Полная privacy-safe выжимка и SHA-256 находятся в
`docs/stability/tests/V14_RELEASE_READINESS_IROK_FREEZE_FORENSICS_2026-08-21.txt`.

### Запуск 1: реальный freeze

- IROK MG75 Max: `1CA6:0529`, SparkLink `FFB0`.
- Через 30.797 с watchdog увидел несогласованное состояние output worker:
  `alive=1`, `fault_kind=0`, `has_thread=1`.
- `VigemOutput_Stop` немедленно получил `WAIT_FAILED` (`4294967295`) и Win32
  error 6 (`ERROR_INVALID_HANDLE`) на thread handle.
- Сам worker в тот же момент завершился с `fault_kind=0`; lifecycle всё равно
  остался poisoned и restart-blocked.
- Далее произошло 161 безрезультатное recovery attempt; аналоговый XUSB output
  уже не вернулся.
- SparkLink до закрытия выполнил `279189/279189`, `route_fail=0`, что отделяет
  вход клавиатуры от сломанного virtual output.
- За 235 мс до первого отказа UI удалил low-level keyboard hook. Это сильная
  временная корреляция для forced-interleaving oracle, но **не доказанная
  причина**.

### Запуск 2: независимый shutdown defect

- ViGEm output оставался жив 160 секунд; freeze не повторился.
- SparkLink выполнил `421858/421858`, `route_fail=0`.
- Shutdown получил `sayo stop.lock_timeout wait_ms=500`, хотя активным input
  route был SparkLink; native shutdown стал poisoned и завершился кодом 2.
- `HallJoyDiagnosticCrash.txt` имеет `source=exit_watchdog_synthetic`: это не
  exception dump и не доказательство настоящего crash.

### Сравнение со стабильной 1.4

Рабочий по сообщению владельца artifact
`C:\github\HallJoy_v1.4\build\release\HallJoy.exe` имеет SHA-256
`B21060D0FE5676A6301DDB2EEB0412DFBF5EDC4850BAD575B82045905FDE4243`.
Тестовый artifact имеет SHA-256
`7EB42CF1687D0DB1FF16721875C5F36B502FD96D1EB1DEC686C3AC4EEA85AB6B`.
Это обязательная A/B-пара. Однако участок `VigemOutput_Wake` —
`Backend_EnsureOutputWorkerRunning` в доступных исходниках совпадает
построчно, поэтому стабильный EXE является regression baseline, а не
доказательством, что общий lifecycle-код корректен. Нужно найти внешний
interleaving/ownership violation или memory corruption, а не слепо копировать
старый backend.

## Новые обязательные риски

### HJ-V14-P0-006 — ViGEm output теряет thread ownership и не восстанавливается

Release gate:

- каждый create/duplicate/wait/close thread/wake handle имеет одного владельца,
  generation и проверяемую provenance;
- watchdog различает alive, completed, invalid handle, no-progress и transport
  fault по одному coherent snapshot;
- invalid/stale generation не может отравить все будущие generations;
- потеря output вызывает немедленный neutral/rebuild либо жёстко ограниченное
  process-level восстановление;
- старый физический timeline воспроизводится deterministic old-bug oracle;
- IROK и DrunkDeer сохраняют output при hook/profile/device-change stress.

### HJ-V14-P1-038 — остановка неприменимого Sayo отравляет общий shutdown

Release gate:

- определён владелец `g_sayoLifecycleMutex` и доказана причина 500-ms hold;
- absent/unselected backend не может сделать native shutdown poisoned;
- stop/start/proof сериализованы вне realtime и имеют generation callbacks;
- lock contention, concurrent hotplug и shutdown проходят bounded tests без
  survivor и без ложного synthetic crash.

### HJ-V14-P1-039 — SparkLink не имеет per-row freshness

Успех одной строки сейчас сбрасывает общий failure streak и обновляет общий
`lastPacket`, поэтому постоянно не читаемая другая строка может удерживать
старые ненулевые значения бесконечно.

Release gate:

- freshness/failure учитывается по каждой active row и по целому matrix cycle;
- публикация является atomic complete generation либо имеет эквивалентную
  доказанную neutralization семантику;
- один permanently failing row не маскируется успешными соседями;
- failed row neutralizes/reconnects без потери живых rows и без realtime stall;
- malformed, alternating-failure, held-key lost-release и reconnect tests
  проходят до физического IROK regression.

## Обязательный порядок работ

Порядок ниже минимизирует повторные переделки. Package нельзя объявлять
закрытым по build-only или token-only результату.

### R0 — заморозить scope и сделать доказательства способными ловить дефекты

Владелец: `V14-22`.

1. Создать machine-readable manifest: risk ID -> old-bug oracle -> production
   code -> toolchain -> hardware -> exact artifact.
2. Добавить failing negative oracle для каждого открытого P0/P1 до его fix.
3. Удалить anti-fix requirements: disabled hotplug, milli-1000, 256 keys,
   fixed 1 kHz.
4. Связать tests с production core; static/model checks честно так и называть.
5. Добавить sanitizer health control, все parsers и stateful malformed corpus.
6. Зафиксировать A/B runner для стабильного и кандидата IROK, включая hook
   toggle, profile save, virtual-pad enable/disable и device change.

Выход: текущие P0/P1 воспроизводимо красные; manifest не позволяет получить
зелёный release при пропущенном gate.

### R1 — устранить P0 output/lifecycle до архитектурного расширения

Владелец: `V14-23A/B`, риски `P0-005`, `P0-006`.

Architecture decision: `VIGEM_OUTPUT_PROCESS_ARCHITECTURE_2026-08-21.md`.
Локальная RAII и in-process generation-owner схемы отвергнуты как конечное
решение, потому что не дают hard bound зависшему `vigem_target_x360_update`.
Выбран self-hosted output process с shared latest-value snapshot и одним
parent supervisor-owner.

1. Сделать thread/wake resources generation-owned или process-lifetime.
2. Исключить close/use/reuse race для всех output publishers.
3. Ввести ready/progress/completion acknowledgements и one-owner reap.
4. Исправить watchdog recovery: один bounded attempt, neutralization,
   проверяемый rebuild; никакого бесконечного poisoned retry loop.
5. Проверить corrupted/closed/reused handle, worker natural exit, stalled
   `vigem_target_x360_update`, reconnect и shutdown.

Progress on 22 August 2026:

- O1/O2 deterministically preserve the legacy close/use/reuse and invalid
  thread-handle failures;
- R1.1 proves the fixed 640-byte claimed-slot channel across real Windows
  processes and 100,000 complete four-pad snapshots;
- F1 proves the contained generic owner across 1,008 generations;
- F1.1 proves the same HallJoy image, persistent output session, all six O4
  boundaries, O3 fake stall/force/reap/replacement, newest-value equivalence
  and output-qualified clean stop across nine more generations;
- F2 proves one real child-owned ViGEm transport: generation-bound pad count,
  four target add/explicit initial-neutral, complete snapshot update, planned
  neutral/remove, exhaustive deterministic failure cleanup and exact-image PnP
  baseline restoration with zero survivor.

F3 progress on 22 August 2026:

- production now routes through one `OutputRuntime` and the self-hosted child;
- the legacy mailbox/thread/closeable wake/reconnect owner and all direct ViGEm
  calls were removed from `backend.cpp` in the same package;
- planned stop and forced recovery close publication admission before child
  stop/force/reap, then drain admitted producers before resource reuse; a
  replacement begins only after confirmed reap and drain;
- exact fake, exact real ViGEmBus, normal routed and routed exit/stall scenarios
  pass with child-side apply acknowledgement, strict non-overlap, clean
  neutral/remove and zero survivor;
- publication admission was separated from immutable child lifecycle identity,
  so closing parent publication no longer suppresses truthful terminal
  neutral/remove acknowledgement.

This completes the F3 code route and local routed O1/O2 gates. `P0-005/006`
remain `HARDWARE_PENDING`, not Verified. Local exact O5 passes 20 independent
runs: at least 2,000,000 publications across 2,020 child generations, 1,000
pad-topology changes and 200 disable/enable cycles with no rebuild/overlap/
survivor. Long-duration active-input soak and the same immutable final artifact
on IROK MG75 Max and DrunkDeer G65 remain mandatory.

Выход: deterministic defect tests и exact-EXE IROK stress PASS; ни один input
provider не теряет output и не требует ручного переподключения.

### R2 — утвердить общие типы до family fixes

Владельцы: `V14-20`, `V14-21`, риски `P1-011`, `016..023`.

1. Ввести versioned full key identity без byte truncation.
2. Ввести `AnalogProviderV2`: immutable snapshot с values, source ownership,
   freshness, identity, capacity и generations.
3. Сохранить raw/float precision до curve/output boundary.
4. Разделить IPC data/control/health directions и договориться о capacity.
5. Адаптировать UAP первым, native — family-by-family; не переписывать все
   parsers одним big bang.
6. Сохранить прежние adapters временно только за characterization tests.

R2-A/B1 progress on 22 August 2026:

- compared numeric-array growth, a big-bang rewrite, staged versioned adapters
  and string/UUID realtime identity; selected the staged contract;
- added production-compiled `KeyIdentityV1` namespaces for USB HID usages, UAP
  extended keys and HallJoy semantic controls, so equal numeric codes cannot
  collide across domains;
- added POD value/device/snapshot types with float plus optional raw domain,
  explicit ownership/freshness, provider/sample/value/ownership generations and
  honest count/capacity/required/truncated state;
- portable negative gates pass Fn/Menu/media/OEM identity, adjacent 12-bit
  precision, authoritative zero release, duplicate rejection, generation/wake
  semantics and complete 1/8/16/32-device snapshots;
- the full native suite and MSVC Release x64 build pass. `Backend_Tick`, UAP,
  native providers, profiles and UI are intentionally not switched yet.

This completes the common semantic foundation only. The next package is a
read-only UAP adapter and old/new XUSB equivalence gate; no production route
changes until that comparison passes.

R2-B2a progress on 22 August 2026:

- the UAP worker retains all Soup-key values before legacy projection and
  exports a versioned per-device V2 snapshot through isolated IPC to a validated
  read-only parent capture;
- USB Menu, Consumer media and UAP Fn remain distinct; explicit zero release,
  duplicate/NaN rejection and two-device ordinary-HID legacy projection pass;
- registry demand beyond the eight-owner pinned window is preserved. Snapshot
  capacity now describes the captured generation, so even an oversized caller
  buffer cannot hide internal truncation;
- the exact rebuilt ABI1 DLL returned one local device and 127 valid samples;
  the complete compiler suite, official MSVC build and production smoke pass;
- `Backend_Tick` remains unchanged. Separate legacy/V2 export calls are not
  accepted as final equivalence because a publication can occur between them.

The next R2 package must compare one immutable generation through the actual
configured bindings/curves/XUSB builder in read-only shadow. Only field-exact
XUSB equality can authorize a route switch.

R2-B2b progress on 22 August 2026:

- compared retrying separate calls, prematurely deriving/replacing the legacy
  route, and one pinned dual export; selected the last with a legacy fallback;
- one UAP call now pins and locks the source once, returns V2 plus the dense
  ordinary-HID view, and the exact ABI gate independently checks every one of
  the 256 compatibility cells for every device;
- the first physical local gate found and fixed a real false-fresh state before
  first acquisition (`generation=0`, `timestamp=0`);
- IPC V12 publishes the pair with an explicit coherence bit; the parent rejects
  unlabeled V2 while a failed dual contract leaves the qualified legacy route
  alive without digital-key correlation;
- the exact production image captured a coherent authoritative non-empty V2
  snapshot through its actual isolated child, and ordinary startup/shutdown
  passed with no exact-image survivor;
- `Backend_Tick` is still unchanged. The remaining B2 gate is configured
  bindings/curves/all-XUSB-fields shadow equivalence, not another hardware
  matrix collection.

Выход: Fn/Menu/media/OEM проходят все common surfaces; 1/8/16/32-device и
precision vectors не теряются и не будят realtime без новой generation.

### R3 — исправить production providers

Владельцы: `V14-19`, `V14-23C/D`.

Обязательная последовательность:

1. UAP: memory-safe parsing, response correlation, hotplug, disconnect,
   per-device release bookkeeping, truthful connectivity и special keys.
2. Sayo: удалить binary-derived mapping и fabricated full depth; исправить
   lifecycle lock (`P0-003`, `P1-038`).
3. W669: stream silence deadline, immediate neutralization и reconnect
   (`P0-004`).
4. Hex80: stage complete chunks, whole-cycle failure и lost-release handling.
5. Addressed: убрать непроверенный canonical layout fallback.
6. SparkLink: полный layout proof, explicit scale, max duplicate aggregation и
   per-row freshness (`P1-012`, `P1-039`).
7. MAD68/Aula/прочие siblings: extended identities и отдельный layout proof.
8. Вынести native I/O в common provider process boundary; hard timeout означает
   уничтожение provider process, а не живого `OVERLAPPED`.

Выход: каждый включённый production route проходит parser/fault/reconnect/
lost-release suite и representative hardware. Недоступный route исключается из
release build, а не получает выдуманный PASS.

### R4 — сделать downstream поведение транзакционным

Владелец: common pipeline, риски `P1-014..016`.

1. Catalog-driven source arbitration по device/path/HID, без глобального списка.
2. Immutable parse/validate/commit профилей и атомарное переключение пары
   settings+bindings.
3. Один key domain для axes/triggers/buttons/capture/UI/overlay/INI.
4. Сброс SOCD state при смене pad/axis/profile/provider generation.
5. Coherent layout/UI telemetry snapshots.

Выход: concurrent malformed profile никогда не даёт mixed output; digital
fallback не влияет на ранний analog travel и не смешивает устройства.

### R5 — завершить lifecycle, pause и realtime isolation

Владельцы: `V14-23`, `V14-24`, риски `P1-028..033`, `038`.

1. Immutable tracked-HID snapshot и truthful generation-bound registry.
2. Убрать discovery/proof/start/stop/join из realtime.
3. Pre-provision external/hard shutdown supervisor.
4. Реализовать один глобальный owner и транзакцию:
   neutral -> stop providers -> release HID/child/hook/XUSB -> paused ->
   re-enumerate/re-prove new generation.
5. Проверить второй HallJoy, web driver, pending I/O, reset/PID change, 1000
   pause/resume cycles и zero survivors.

Выход: realtime latency bound соблюдается при всех lifecycle faults; shutdown
не зависит от нового allocation и всегда завершает процесс в срок.

### R6 — закрыть trust, installer и внешние документы

Владелец: `V14-25`, риски `P1-034..037`.

1. Child принимает только exact embedded UAP от доказанного HallJoy owner.
2. Layout/profile limits применяются до allocation; commit только полного
   validated document.
3. Structured privacy/redaction schema, consent для raw travel и artifact scan.
4. Сохранить D-049 embedded ViGEmBus path; проверить его в чистой Windows VM
   без установленного bus, включая UAC success/cancel/restart/failure/timeout.
5. После code freeze подписать HallJoy и provenance manifest, проверить signer,
   timestamp, mitigations, SBOM и post-sign mutation rejection.

Выход: пользователь получает one-click install, а exact release artifact имеет
аутентифицированное происхождение и безопасные support artifacts.

### R7 — reconciliation старых ledgers без повторного аудита

1. `HJ-AUD-P1-001/002/004/005` закрыть только final lifecycle/fault evidence;
   старое `Implemented` не превращать в `Verified` автоматически.
2. UI `HJ-UI-P1-001..016`: не повторять уже принятую владельцем архитектуру;
   провести один focused smoke на exact candidate и привести противоречащие
   `owner/visual pending` статусы к факту.
3. W669 `HJ-W669-P1-003/005/006/007`: один final physical rerun, если W669
   остаётся production-supported.
4. `HJ-V14-P1-008` MAD68 shutdown: физический retest либо исключение tester
   route/artifact из release claim.
5. ND75 `HJ-ND75-*`: не блокирует основной release, пока macro/catalog/binary
   audit доказывает полное отсутствие experimental route. При включении все
   его P1 становятся блокерами.
6. Удалить или явно пометить superseded rows; один риск не должен одновременно
   быть `pending` и заявляться закрытым в README.

Выход: current evidence index имеет по одному актуальному статусу на риск.

### R8 — квалифицировать один финальный artifact

Порядок финального gate:

1. clean reproducible `Release|x64` build;
2. unit/parser/property/fuzz/sanitizer/race/fault suites из manifest;
3. sign EXE и provenance; зафиксировать hash и больше не rebuild;
4. missing-ViGEm clean-VM install;
5. 1000 start/close и 1000 pause/resume cycles;
6. не менее 8 часов exact-EXE soak с active analog input, UI, overlay,
   profile/hook/device-change stress и output progress monitoring;
7. targeted hardware matrix ниже;
8. privacy scan, package manifest, notices/SBOM, clean-machine launch;
9. только затем version/tag/release notes; GitHub остаётся вне текущей работы.

## Минимальная hardware matrix финального EXE

| Route | Обязательный финальный сценарий | Текущее положение |
|---|---|---|
| IROK MG75 Max / SparkLink | gameplay/continuous input, hook and settings toggle, unplug/reconnect, no stale row, no ViGEm output loss, clean exit | прежний кандидат FAIL и stable 1.4 A/B PASS; F3 local route PASS, immutable exact-final hardware rerun pending |
| DrunkDeer G65 | Turbo off, Fn/Menu/all arrows, early travel, multi-key, reconnect, output-liveness and clean exit | parser/map/rate PASS на diagnostic v3; F3 local route PASS, immutable exact-final hardware rerun pending |
| Aula WIN60 HE MAX | held-key unplug/reconnect, rollover/rate, complete neutral and output continuity | прежний physical gate PASS; exact-final regression нужен |
| Redragon K673 / W669 representative | stream silence/lost release, reconnect, subtype-only publication | прежний physical input PASS; исправления stale/reconnect требуют rerun |
| Keychron K4 HE custom full-report | latency, multi-key, reconnect and final output path | прежнее owner acceptance; exact-final smoke нужен, stock firmware не поддерживается |
| UAP representative | hotplug, short/malformed reports, special keys, per-device release and load | hardware pending; без него broad all-UAP claim запрещён |
| Sayo/Hex80/Addressed/MAD68 | family-specific P0/P1 gates | hardware pending: route либо квалифицируется, либо исключается из production claim/build |

Нельзя ещё раз просить пользователя «нажать всю клавиатуру», если статическая
карта уже доказана. Каждый hardware run должен закрывать конкретный остающийся
риск и собирать все нужные counters за один запуск.

## Definition of done следующего stable release

Release разрешён только если одновременно истинны все пункты:

- все production-reachable P0/P1 имеют статус `Verified`/`Closed` и evidence;
- все невалидированные routes действительно отсутствуют в final binary и docs;
- ни один stale/P0 old-bug oracle не проходит на старом defect и не падает на
  исправленном production code;
- final EXE не теряет XUSB output, не оставляет ненулевое состояние и не требует
  replug/restart при recoverable fault;
- realtime, shutdown и pause имеют измеренные hard bounds;
- hardware matrix относится к одному подписанному SHA-256 artifact;
- UI/status/risk/README/validation/release notes не противоречат друг другу;
- installation ViGEmBus физически проверена на чистой машине;
- package, license, SBOM, signature, provenance и privacy gates проходят;
- owner отдельно принимает release candidate после просмотра итогового отчёта.

До этого допустимы только явно маркированные diagnostic/experimental builds.
Успешный build, отдельный пользовательский PASS или старая v1.4 qualification
не снимают общий release blocker.

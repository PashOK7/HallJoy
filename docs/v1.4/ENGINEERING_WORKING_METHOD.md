# HallJoy engineering working method

## 2026-09-06 — Current filesystem contract

Read [PROJECT_LAYOUT](../current/PROJECT_LAYOUT.md) before building or moving files.
Use build/release for ordinary delivery; .local/backups for new snapshots.
Keep SHA-256 manifests, preserve unique evidence and user settings, verify each
move. Old backup/output addresses in historical records resolve through migration
journals. Do not recreate .analysis/backups, _backups or generated x64 under src.

## 2026-09-06 — Owner quality policy: no knowingly partial production solutions

When a technically feasible choice exists between a quick implementation that
leaves a known correctness, ownership, safety, lifecycle, compatibility, or
release-quality gap and a deeper implementation that closes that gap, choose
the deeper complete implementation. Time saved, smaller diffs, visible UI
progress, or a request for a temporary button are never sufficient reasons to
ship the known-incomplete alternative.

A staged migration remains valid only when every shipped stage is independently
safe and correct for its stated contract, has an explicit removal gate where it
is transitional, and does not pretend to satisfy the final requirement. Do not
raise this choice to the owner as a routine speed-versus-quality question. Ask
only when two complete, evidence-backed outcomes have materially different
product semantics or external costs that the owner must decide.


## 2026-09-06 — Disputed behavior: ask the owner; Discord feature planned

If intended behavior is disputed, unclear or contradicted by documents, ask the
owner what HallJoy should do in that concrete scenario before implementing the
contested change. Continue independent work; record the answer and do not re-ask
already resolved decisions. Routine implementation choices remain autonomous.
FULL_AUDIT_EXECUTION_ROADMAP_2026-09-06.md v1.2 adds RM-37 at the very end:
unsupported-keyboard status with a Discord community invitation, plus a permanent
Discord link in the UI. Total: 37 audit cards + 1 feature card. Official community
invite must be obtained from the owner before shipping buttons; no invite was
found in the inspected support docs. This is documentation only, not implemented UI.


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


Дата решения: 21 августа 2026 года.

Статус: обязательный engineering contract для всех следующих пакетов
`R0..R8`, исправлений, архитектурных изменений и release decisions.

## Главная цель

Цель работы — не быстро получить очередной зелёный EXE, а построить корректную,
предсказуемую, быструю и поддерживаемую программу без скрытых компромиссов.
Срок, размер diff и сохранение старой реализации сами по себе не имеют
приоритета над correctness, latency, liveness, ownership и честностью
поддержки.

«Не торопиться» означает:

- сначала доказать проблему и границы, затем выбирать архитектуру;
- не принимать первое работающее решение без сравнения альтернатив;
- не выдавать build/test-only PASS за решение системной проблемы;
- не перекладывать недостаток наблюдаемости на повторные бессмысленные
  пользовательские прогоны;
- останавливать пакет при недоказанном протоколе, layout или invariant.

## Обязательный порядок каждого implementation package

### 1. Восстановить контекст

Перед изменением прочитать mega-roadmap, owning audit, связанные risks,
decisions, validation evidence и актуальный код. Проверить, не было ли уже
принято или отвергнуто такое решение. Чат не является источником истины.

### 2. Зафиксировать проблему до кода

Записать:

- наблюдаемый failure и точную evidence boundary;
- root-cause hypothesis и что пока остаётся неизвестным;
- затронутые устройства, generations, threads/processes и data path;
- обязательные инварианты;
- old-bug oracle, который падает на текущей реализации;
- что не должно измениться для уже работающих устройств.

Если old-bug пока нельзя воспроизвести, сначала улучшить безопасное
логирование/инструментацию. Диагностический код не должен менять timing,
ownership или production semantics настолько, чтобы создавать новый класс
дефекта.

### 3. Рассмотреть несколько реализаций

До production edit обязательно сравнить не менее трёх осмысленных вариантов,
если они технически существуют:

1. локальное корневое исправление без изменения публичного контракта;
2. staged migration к более правильной архитектуре;
3. чистый redesign или полное переписывание дефектного компонента.

Дополнительно рассматриваются process isolation, reuse проверенного общего
primitive и полное исключение недоказанного route из release. Вариант
«оставить как есть и увеличить timeout/retry» не считается самостоятельной
архитектурой, если он не устраняет root cause.

Для каждого варианта письменно оценить:

| Критерий | Обязательный вопрос |
|---|---|
| Correctness | Устраняет ли root cause и все известные interleavings, а не один симптом? |
| Latency | Не добавляет ли binary-edge ожидание, polling lag, blocking I/O или лишнюю quantization? |
| Liveness | Есть ли hard bound, neutralization, recovery и zero-survivor behavior? |
| Ownership | Кто создаёт, публикует, pin/reap и закрывает каждый resource/generation? |
| Data contract | Полны ли identity, precision, freshness, capacity и transactional semantics? |
| Compatibility | Как мигрируют profiles, providers и уже проверенные устройства? |
| Testability | Может ли production-linked oracle воспроизвести старый дефект и доказать fix? |
| Security/privacy | Не расширяется ли trust boundary и не раскрываются ли raw user data? |
| Maintainability | Уменьшается ли число special cases и параллельных истин? |
| Rollback | Есть ли проверенный backup и изолированная граница возврата? |

### 4. Выбрать лучший вариант, а не самый маленький diff

Выбор фиксируется в `DECISIONS.md` до реализации, если меняется ownership,
thread/process boundary, ABI, data model, persistence, protocol admission,
security boundary или release policy.

Предпочтение получает решение, которое:

- устраняет класс дефектов, а не конкретный лог;
- имеет простой явный контракт;
- сохраняет ранний analog travel и realtime latency;
- fail-closed на неизвестных данных;
- допускает детерминированную проверку;
- удаляет obsolete path после доказанной миграции.

Полное переписывание выбирается, когда локальная правка сохраняет
противоречивые owners, неявный state machine, недоказуемые lifetime или
anti-fix ABI. Rewrite не является самоцелью: если staged replacement даёт ту же
правильную конечную архитектуру с меньшим migration risk, выбирается staged
replacement. Большой big-bang rewrite без characterization и rollback также
запрещён.

### 5. Подготовить безопасную точку изменения

До глобальных изменений:

- проверить, что файлы не изменились после чтения;
- сделать локальный hash-verified backup;
- сохранить unrelated user changes;
- добавить old-bug negative oracle и characterization working behavior;
- определить одну небольшую, но архитектурно завершённую границу package.

«Небольшой package» означает один coherent invariant, а не временную половину
решения, которая попадает в production.

### 6. Реализовать без скрытых fallback

Production-код не должен содержать:

- analog identity, выбранную или обученную по цифровому keydown;
- guessed layout/scale/protocol и broad VID/PID admission;
- stale nonzero fallback после потери freshness;
- бесконечный reconnect/retry или timeout, маскирующий broken ownership;
- realtime enumeration/open/proof/stop/join/storage;
- silently truncated key/device domains;
- parallel old/new path без явного generation/source ownership;
- подавление ошибки ради зелёного UI или теста;
- diagnostic-only behavior, ошибочно заявленный как production proof.

Временный compatibility adapter допустим только при явном owner, removal gate,
characterization tests и отсутствии влияния на correctness/latency.

### 7. Проверить слоями

Минимальный порядок evidence:

1. old-bug oracle красный на старом defect;
2. unit/property/parser/state-machine tests;
3. malformed, disconnect, lost-release, reconnect и concurrency/fault matrix;
4. sanitizer/race health control;
5. production wiring и end-to-end XUSB output;
6. exact-EXE smoke/soak/resource/lifecycle tests;
7. targeted representative hardware, если route включён в release;
8. A/B против последнего доказанно работающего artifact, когда это полезно.

Зелёный нижний слой не заменяет следующий слой. Hardware run закрывает заранее
названный риск; пользователь не должен повторно собирать уже доказанную карту.

### 8. Закрыть документацию и только потом package

Каждый package обновляет:

- owning audit и `RISK_REGISTER.md`;
- `ROADMAP.md` и mega-roadmap status;
- `VALIDATION_MATRIX.md` с exact artifact/evidence boundary;
- `WORKLOG.md`;
- `DECISIONS.md`, если принято архитектурное решение;
- handoff, если изменился следующий шаг или user test.

Статус `Verified` допустим только после всех применимых gates. `Implemented`,
`PASS static`, `hardware pending` и «пользователь не пожаловался» не являются
синонимами release-ready.

## Stop conditions

Работа над текущей реализацией останавливается и возвращается к сравнению
вариантов, если:

- fix требует ещё одного special-case поверх уже противоречивого state;
- невозможно назвать единственного owner ресурса или source value;
- hard deadline требует уничтожить память живого I/O request;
- latency зависит от позднего digital edge;
- test проходит без исполнения production path;
- неизвестный layout/protocol предлагается угадать;
- исправление одного backend ухудшает общий provider/output contract;
- новый факт опровергает исходную root-cause hypothesis.

Если лучший вариант требует более глубокого redesign, это не считается
блокировкой или неудачей. План обновляется, создаётся безопасная migration
sequence, и выполняется правильный вариант без искусственного ускорения.

## Шаблон решения перед реализацией

Каждый значимый implementation package начинает записью:

```text
Problem / evidence:
Affected invariants and scope:
Old-bug oracle:
Option A — local root correction:
Option B — staged architectural migration:
Option C — clean redesign/rewrite:
Comparison (correctness/latency/liveness/ownership/migration/tests/security):
Chosen option and why:
Rejected options and why:
Rollback boundary:
Required gates:
Documentation to update:
```

Код начинается только после заполнения этой записи настолько, насколько
позволяет известная evidence. Неизвестное отмечается явно и сначала получает
диагностику, а не догадку.

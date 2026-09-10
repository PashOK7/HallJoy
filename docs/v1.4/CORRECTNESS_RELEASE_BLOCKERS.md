# HallJoy protocol correctness release blockers

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


Дата решения: 20 августа 2026 года. Обновлено: 21 августа 2026 года.

## Обязательная политика

Ни одна следующая стабильная версия HallJoy не может быть опубликована, пока
не закрыты все P0 и P1 из следующих обязательных аудитов:

- `UAP_ALL_KEYBOARDS_AUDIT.md`;
- `NATIVE_ALL_KEYBOARDS_AUDIT.md`;
- `COMMON_ANALOG_PIPELINE_AUDIT.md`;
- `ARTIFICIAL_LIMITS_PERFORMANCE_AUDIT.md`;
- `INPUT_PROVIDER_ARCHITECTURE_AUDIT.md`;
- `TEST_EVIDENCE_TRUST_AUDIT.md`;
- `CONCURRENCY_LIFECYCLE_OWNERSHIP_AUDIT.md`;
- `GLOBAL_PAUSE_DEVICE_LEASE_AUDIT.md`;
- `TRUST_SECURITY_BOUNDARY_AUDIT.md`;
- `RELEASE_READINESS_MEGA_AUDIT_2026-08-21.md` как единый порядок исполнения и
  definition of done для всех перечисленных аудитов;
- последующие end-to-end аудиты, явно добавленные в этот список.

Предыдущая квалификация и уже опубликованный v1.4.1 остаются историческим
фактом, но не закрывают найденные позже дефекты и не могут быть переиспользованы
как основание для нового релиза.

## Условия снятия блокера

Для каждого P0/P1 одновременно требуются:

1. root cause и границы затронутых моделей/маршрутов;
2. исправление без binary-derived analog identity и без guessed layout;
3. parser, malformed input, disconnect, lost-release и reconnect regressions;
4. проверка общего пути до итогового XUSB report;
5. физическая квалификация для доступных representative devices;
6. честная пометка `hardware pending`, если нужного устройства нет;
7. обновлённые risk register, validation matrix и release notes.

Нельзя закрывать блокер одной сборкой, simulator-only тестом, отсутствием
жалоб пользователей или успешной проверкой другой клавиатуры того же бренда.

## Текущие блокирующие группы

- `HJ-V14-P0-001` / `002`: UAP key domain, parsers, hotplug и lifecycle;
- `HJ-V14-P0-003` / `004`: Sayo binary-derived identity и W669 stale stream;
- `HJ-V14-P1-009` .. `013`: Hex80, Addressed, общий key domain, SparkLink и
  protocol-vs-layout admission;
- `HJ-V14-P1-014` .. `016`: source arbitration/digital fallback,
  нетранзакционная загрузка профилей и несовместимый end-to-end key domain.
- `HJ-V14-P1-017` .. `020`: ранняя native-квантизация, квадратично-подобный
  generic UAP hot path, недоказанные 1 kHz ceilings и тихое ограничение private
  UAP snapshot до восьми устройств.
- `HJ-V14-P1-021` .. `023`: смешанный монолитный analog-host IPC, отсутствие
  единого UAP/native provider contract и отсутствие process containment для
  native protocol I/O.
- `HJ-V14-P1-024` .. `027`: anti-fix тестовые контракты, подмена behavioral
  доказательства source-token/model проверкой, неполный официальный release
  gate и недоказанное покрытие/здоровье sanitizer matrix.
- `HJ-V14-P0-005`, `HJ-V14-P1-028` .. `032`: close/use race ViGEm wake handle,
  data race tracked HID snapshot, lifecycle state без generation-bound
  ready/completion, HID Start/Stop/proof/join внутри realtime, неограниченный
  cancellation drain и watchdog, который может не вооружиться при shutdown.
- `HJ-V14-P0-006`: физически воспроизведённая потеря ViGEm output после
  `WAIT_FAILED/ERROR_INVALID_HANDLE` на thread handle; lifecycle навсегда
  остаётся poisoned, хотя SparkLink input продолжает работать без ошибок.
- `HJ-V14-P1-038`: неприменимый/неактивный Sayo способен получить timeout
  lifecycle lock и отравить общий native shutdown.
- `HJ-V14-P1-039`: глобальные SparkLink success/freshness скрывают постоянно
  не читаемую отдельную row и допускают бесконечный stale lost-release.
- `HJ-V14-P1-033`: нет глобального neutralize/release/resume рубильника и
  единственного HallJoy owner; web-драйвер, второй HallJoy и текущие providers
  могут конкурировать за HID и протокольную сессию.
- `HJ-V14-P1-034` .. `037`: internal analog-host принимает произвольный DLL
  path без exact embedded identity, внешний layout count не ограничен до
  allocation, support logs не имеют privacy/redaction contract, а release EXE
  не имеет аутентифицированной подписи/provenance.

Успешный `BUILD.cmd`, количество строк PASS или присутствие имени runner в
build script не снимают блокер. До исправления production code каждый P0/P1
должен получить отрицательный old-bug oracle, который воспроизводимо падает на
старом дефекте и проходит только после корневого исправления.

Production route с незакрытым P0/P1 можно исключить из release scope только
фактическим compile/catalog/binary exclusion и одновременным удалением claim из
публичной документации. Маркировка `experimental` или `hardware pending` при
достижимом production code path блокер не снимает.

## Разрешённая работа при открытом блокере

Разрешены диагностика, аудит, тестовые harness, документирование и исправления
в рамках блокирующего roadmap. Experimental/diagnostic builds допустимы только
с явной маркировкой. Stable release, тег «готово для всех клавиатур» и перенос
старой release qualification запрещены.

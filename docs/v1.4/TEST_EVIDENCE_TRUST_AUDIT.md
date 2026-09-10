# Аудит доверия к тестам и доказательствам HallJoy

Дата: 20 августа 2026 года.

Статус: завершённый статический и локальный исполняемый аудит. Исходный код
HallJoy, тестовые контракты и build/release scripts не изменялись. Физические
клавиатуры не использовались.

## Краткий вывод

Текущий набор проверок большой и содержит несколько действительно сильных
компонентов, однако зелёный официальный build пока не доказывает исправность
всех открытых P0/P1. Главные причины:

- 59 Python-аудитов проверяют текст исходников, а не исполняемое поведение;
- некоторые проверки требуют сохранить уже признанные ошибочными контракты:
  отключённый hotplug, `0..1000`, 256 значений и фиксированные 1000 Гц;
- официальный build и CI не запускают sanitizer/fuzz, production smoke,
  fault matrix, release qualification, soak и hardware shutdown matrix;
- наличие имени runner в `build.ps1` в нескольких местах ошибочно считается
  доказательством его выполнения;
- sanitizer-набор охватывает лишь часть parser implementations и не проверяет
  собственную работоспособность отрицательным контролем;
- исторический `VALIDATION_MATRIX.md` не даёт машине однозначно определить,
  каким исходникам, EXE и рискам соответствует каждый PASS.

Следовательно, число зелёных тестов само по себе нельзя использовать как
релизное доказательство. До исправления каждого P0/P1 сначала должен появиться
отрицательный regression oracle: он обязан воспроизводимо падать на старом
дефекте и проходить только после корневого исправления.

Рекомендуемый пакет — `V14-22`, Test/evidence trust and regression oracle
overhaul. Он является обязательной основой для `V14-19`..`V14-21`, а не
косметической уборкой тестов.

## Граница и метод аудита

Проверены:

- все файлы `src/HallJoyProject/tests`;
- общий portable runner `tools/run_native_backend_checks.py`;
- sanitizer runners;
- `BUILD.cmd`, `tools/build.ps1` и GitHub workflow;
- UAP ABI runtime check;
- production smoke, fault, qualification, soak, migration/reset и hardware
  runners, независимо от того, входят ли они в официальный gate;
- текущие P0/P1 из UAP, native, common-pipeline, limits/performance и provider
  architecture audits;
- `VALIDATION_MATRIX.md`, release evidence и правила воспроизводимости.

Для каждого важного утверждения задавались пять вопросов:

1. Какой риск или инвариант оно закрывает?
2. Выполняется ли настоящий production code или отдельная модель/поиск текста?
3. Существует ли отрицательный пример, который обязан упасть?
4. Запускает ли проверку официальный local/CI/release runner?
5. Привязано ли доказательство к точным исходникам и финальному EXE?

Уровни доказательства в этом документе различаются строго:

| Уровень | Что действительно доказывает |
|---|---|
| Static text | В выбранном файле присутствует/отсутствует ожидаемый текст |
| Compile | Выбранный код компилируется указанным компилятором и флагами |
| Unit/parser | Конкретная чистая функция правильно обрабатывает заданные vectors |
| Model | Отдельная модель выражает желаемую policy, но не доказывает production wiring |
| Runtime component | Реальная production implementation исполняется через контролируемый seam |
| Final EXE | Точный собранный образ проходит процессный/системный сценарий |
| Hardware | Точный EXE и firmware проходят сценарий на конкретном устройстве |

Нижний уровень полезен, но не может автоматически заменить верхний.

## Фактический инвентарь

В `src/HallJoyProject/tests` находится 98 файлов:

- 59 `*audit.py`;
- 34 `*_test.cpp`, включая шесть автоматически обнаруживаемых protocol tests;
- четыре других Python-инструмента анализа hardware/log evidence;
- один общий header с Aula oracle fixtures.

Все 59 static audits читают исходники как текст. Ни один не использует AST,
libclang, compiler IR или исполнение анализируемой функции. Часть аудитов умеет
выделять приблизительное тело функции и потому сильнее простого глобального
поиска, но всё равно не доказывает control flow, data flow, linkage или то, что
найденная ветка достижима.

`run_native_backend_checks.py`:

- запускает все 59 `*audit.py`;
- компилирует и исполняет 28 явно перечисленных C++ tests;
- автоматически добавляет шесть `*_protocol_test.cpp`;
- использует C++20, `-O2 -Wall -Wextra -pedantic`;
- не включает ASan/UBSan и не измеряет coverage.

Некоторые C++ parser/client tests действительно компилируют production `.cpp`.
Другие проверяют вынесенные primitives или независимо написанные policy models.
Оба вида нужны, но их нельзя одинаково называть production regression.

## Что уже сделано хорошо

Эти сильные части нужно сохранить при перестройке gate:

- чистые protocol parsers компилируются вместе с реальными production `.cpp`;
- Aula MAX fake transport проверяет большой transaction/fault matrix и точное
  взаимодействие настоящего client/parser;
- lifecycle, generation, snapshot publication, wake и output mailbox primitives
  имеют полезные детерминированные тесты;
- UAP ABI check загружает реально собранный DLL и проверяет identity, null/state,
  initialise/unload и повторный unload;
- release qualification, когда запускается явно, работает с точным EXE,
  проверяет hash, корректный `WM_CLOSE`, exit code, отсутствие survivor и
  неожиданных log/crash artifacts;
- production smoke и fault runners содержат полезные process-level assertions;
- dependency lock и linked-image marker scan сильны в своей узкой области;
- hardware logs обычно сохраняют model/firmware/route и не выдаются за
  универсальное доказательство другого семейства.

Проблема не в отсутствии тестовой работы. Проблема в несоответствии между тем,
что конкретный gate реально доказывает, и тем, что его название или итоговый
зелёный build обещает.

## TE-P01 — anti-fix contracts в обязательном gate (P1)

Часть текущих проверок закрепляет найденные аудитами дефекты как обязательную
норму.

### Отключённый UAP hotplug

`tools/build.ps1` требует, чтобы оба UAP targets содержали
`UAP_DISABLE_HOTPLUG=1`. То же ожидание присутствует в MAD/UAP static audits.
Но отключённый hotplug и ghost connectivity уже входят в `HJ-V14-P0-002`.

Корневое исправление hotplug сейчас будет отклонено build preflight ещё до
поведенческой проверки. Это anti-fix oracle.

### Ранняя milli-квантизация

Несколько static и protocol tests требуют:

- экспорт `NativeAnalogBackends_ReadMilli`;
- нормализацию ровно в `0..1000`;
- деление на 1000 в common path;
- конкретные значения 0, 500 и 1000;
- невозможность результата выше 1000 в fuzz smoke.

Такой набор не просто описывает legacy ABI: он блокирует `HJ-V14-P1-017`, где
ранняя потеря точности признана искусственным ограничением.

### Фиксированные 256 значений

`uap_snapshot_pinning_test.cpp` закрепляет 256-value body. Другие contracts
рассматривают стандартный HID как диапазон ниже 256. Это конфликтует с
`HJ-V14-P0-001`, `HJ-V14-P1-011`, `HJ-V14-P1-016` и будущим versioned key domain.

### Фиксированные 1000 Гц

`uap_poll_pacing_static_audit.py` требует compile definition
`UAP_POLL_TARGET_US=1000`; model test ожидает ровно 1000 paced calls;
ViGEm scheduler test также конфигурирует 1000. Эти tests закрепляют текущую
policy, хотя `HJ-V14-P1-019` требует capability-derived rate и доказательства
на 1/2/4/8 kHz sources.

### Генератор и development docs

Проблема воспроизводится при добавлении нового backend:

- checklist требует clamp `[0,1000]`;
- native contract описывает HID ниже 256 и milli depth;
- adding-backend guide предписывает `ReadMilli`;
- `tools/new_native_backend.py` генерирует 256 atomic slots,
  `NormalizeRawToMilli`, `GetMilli` и тесты на 0/500/1000.

То есть новый качественный backend по умолчанию получает старые ограничения.

### Обязательное исправление

Anti-fix gates нельзя просто удалить. Для каждого требуется миграция:

1. записать старый contract как legacy compatibility fixture;
2. добавить новый versioned contract и отрицательный regression;
3. переключить production consumer;
4. доказать migration/backward compatibility;
5. только после этого убрать требование старой внутренней архитектуры.

Риск: `HJ-V14-P1-024`.

## TE-P02 — static-token PASS подменяет behavioral PASS (P1)

Static source audits полезны для запрещённых API, manifests и простых
структурных инвариантов. Они недостаточны для lifecycle, ownership, freshness,
transaction, parser semantics и realtime behavior.

Типовые ложноположительные случаи:

- нужный token остаётся в комментарии или dead branch;
- функция существует, но production call site использует другой путь;
- обе правильная и неправильная ветки присутствуют, а test видит только
  правильный token;
- проверяется факт wake call, но не changed-only semantics;
- проверяется имя runner в build script, но runner не выполняется;
- модель policy проходит, а production wiring не вызывает её или передаёт
  неправильное состояние.

Особенно опасные текущие примеры:

- `prerelease_hardening_static_audit.py` заявляет sanitizer coverage всех
  standalone parsers, хотя общий sanitizer fuzz runner включает только Aula
  MAX, Hex80 и MAD68PR;
- тот же audit считает gates обязательными, если filenames runners просто
  присутствуют в `build.ps1`;
- `release_qualification_runner_static_audit.py` проверяет присутствие строки
  `run_release_qualification.ps1`, а не реальный вызов;
- `final_production_static_audit.py` называет пять native routes «all», не
  покрывая весь актуальный catalog;
- realtime wake проверяется по token presence, поэтому unchanged wake storm
  остаётся зелёным;
- UAP/native max preservation проверяет legacy `ReadMilli`, одновременно
  закрепляя потерю точности.

В проекте нет implementation mutation testing. Термин mutation fuzzing в
некоторых местах означает мутацию входных bytes, а не намеренное внесение
дефекта в production code с проверкой, что gate его убивает. Также нет общего
`llvm-cov`, gcov или OpenCppCoverage evidence.

Обязательное исправление:

- static checks переименовать и маркировать как `static`;
- запретить им закрывать behavioral risk без companion runtime oracle;
- для выбранных критических defects вести mutation kill matrix;
- вынести testable pure core из `Backend_Tick`, arbitration и profile load, но
  production должен линковать именно тот же core;
- отдельно проверять wiring от provider snapshot до финального XUSB report.

Риск: `HJ-V14-P1-025`.

## TE-P03 — официальный build/CI не исполняет полный release gate (P1)

`BUILD.cmd` вызывает `tools/build.ps1`. Тот запускает portable native checks,
UAP build/ABI checks, MSVC Release и packaging. Однако он не исполняет:

- `run_protocol_fuzz_sanitizers.py`;
- `run_aula_win60he_sanitizers.py`;
- `run_production_smoke.ps1`;
- `run_release_qualification.ps1`;
- `run_ui_scroll_stress.ps1`;
- `run_long_soak.ps1`;
- `run_storage_migration_test.ps1`;
- `run_factory_reset_test.ps1`;
- `run_keyboard_shutdown_matrix.ps1`;
- `run_analog_simulator.ps1`.

Некоторые из этих файлов перечислены среди required files. Это проверяет лишь
наличие runner, не его запуск.

GitHub workflow запускает на Ubuntu portable runner, а на Windows
`tools/build.ps1`, после чего публикует output artifact. Следовательно,
автоматически опубликованный зелёный artifact не имеет обязательного
sanitizer/runtime/fault/release-qualification evidence.

Не все тяжёлые проверки следует добавлять в каждый developer build. Требуется
честное разделение:

- fast PR gate: compile, focused units, selected runtime regressions;
- Windows integration gate: actual DLL/EXE, storage, process lifecycle;
- sanitizer/fuzz matrix;
- full release qualification: exact final artifact, fault matrix, soak и
  hardware-pending accounting.

Release нельзя объявлять готовым, если required gate пропущен. Пропуск должен
быть машинным `MISSING/BLOCKED`, а не молчаливым PASS.

Риск: `HJ-V14-P1-026`.

## TE-P04 — sanitizer/fuzz coverage и здоровье не доказаны (P1)

Общий protocol fuzz sanitizer runner компилирует только три implementations:

- Aula MAX;
- Hex80;
- MAD68PR.

Он не покрывает тем же способом W669, DrunkDeer, IROK, UAP family parsers,
Addressed, SparkLink и Sayo transports. Aula-specific runner глубже, но не
расширяет coverage на остальные families.

Локальный запуск обоих sanitizer runners прошёл, однако каждый Windows ASan
process вывел сообщение вида `interception_win: unhandled instruction`.
Само сообщение ещё не доказывает, что ASan не работает: точный эффект не
установлен. Но runner игнорирует его и не имеет отрицательного health control,
поэтому зелёный статус не доказывает, что ожидаемый класс memory violations
действительно был бы обнаружен.

Требуется:

- sanitizer matrix для всех independently parseable production parsers;
- malformed/truncated/oversized/stateful sequences, а не только happy vectors;
- отдельный специально нарушающий память health binary, который обязан
  завершиться как sanitizer failure;
- классификация неожиданного sanitizer stderr и interceptor degradation;
- Windows и Linux runs там, где platform boundary позволяет;
- machine summary по parser, sanitizer, seed/corpus hash и iteration count.

Риск: `HJ-V14-P1-027`.

## TE-P05 — evidence не имеет единого актуального индекса (P2)

`VALIDATION_MATRIX.md` содержит большой объём полезной истории, но является
append-only narrative. В нём одновременно живут ранние, superseded и текущие
PASS, hardware-pending notes и hashes разных artifacts.

Машина не может надёжно ответить:

- какой последний результат относится к каждому открытому risk ID;
- был ли test обязательным или optional;
- какие exact source files и build manifest тестировались;
- относится ли runtime evidence к тому же EXE, который упакован;
- заменён ли старый PASS более поздним FAIL/PENDING;
- какие gates не запускались вообще.

Отдельно нужен current evidence index. Историческую матрицу удалять нельзя.

Риск: `HJ-V14-P2-016`.

## TE-P06 — дублирование gate добавляет работу и точки расхождения (P2)

`run_native_backend_checks.py` уже автоматически запускает все `*audit.py`, но
`tools/build.ps1` после этого отдельно запускает
`pre_release_ui_static_audit.py` второй раз. Сам build script также содержит
ручные text preflights, частично дублирующие специальные static audits.

Повторный запуск одного дешёвого файла не является самостоятельной проблемой
производительности HallJoy. Архитектурный риск тестовой системы в другом:

- список обязательных проверок распределён между glob discovery, hard-coded
  test arrays, required-file lists и ручными PowerShell preflights;
- одно правило можно обновить в одном месте и оставить старым в другом;
- build тратит время повторно, но не получает независимого oracle;
- невозможно машинно доказать, что каждый gate был выполнен ровно один раз и
  какой result является authoritative.

Machine-readable manifest должен стать единственным планом исполнения. Один
test ID исполняется один раз на stage, а повторное использование его результата
разрешается только при совпадении source/toolchain/environment manifest.

Риск: `HJ-V14-P2-017`.

## Покрытие текущих P0/P1 реальными оракулами

| Риски | Что есть сейчас | Критический пробел |
|---|---|---|
| `P0-001`, `P1-011`, `P1-016` | static contracts и ordinary HID tests | нет executable end-to-end >255/Menu/Fn/OEM через input, serialization, UI/capture, overlay и XUSB |
| `P0-002` | UAP ABI lifecycle/no-device; отдельные legacy family tests | нет полного malformed/hotplug/reconnect matrix настоящих UAP parsers; disabled hotplug требуется gate |
| `P0-003` | Sayo routing/lifecycle static checks | нет behavioral oracle physical-map identity, modifier/NKRO, stale depth и запрета fabricated full press |
| `P0-004` | W669 parser/factory-layout tests | нет worker/session test: live-event silence, lost-release neutralization и reconnect |
| `P1-009` | Hex parser/chunk vectors | нет atomic whole-matrix generation и permanent-one-chunk-failure stale-state oracle |
| `P1-010` | Addressed scheduler/model/static checks | нет variant-layout admission и fallback-negative runtime oracle |
| `P1-012`, `P1-013` | Spark routing/static и historical hardware evidence | нет capability/layout/scale/duplicate behavioral oracle для каждого admitted sibling |
| `P1-014` | token check для fallback ownership | нет simultaneous UAP/native/digital per-device/per-HID matrix |
| `P1-015` | сильные save/migration/factory-reset tests | нет malformed/partial/wrong-kind paired-load transaction против работающего realtime |
| `P1-017` | protocol normalization tests | tests требуют legacy milli вместо сохранения source precision |
| `P1-018` | profiler и snapshot primitives | нет automatic complexity/copy/wake threshold actual hot path |
| `P1-019` | pacing/output models | tests фиксируют 1 kHz вместо capability-derived policy |
| `P1-020` | pinning/coherence при 256/8 | нет explicit truncation/required-size cases 1/8/16/32 devices |
| `P1-021`..`P1-023` | архитектурный audit | regression contract ещё не создан; он обязан предшествовать миграции |

Эта таблица не означает, что существующие tests бесполезны. Она определяет,
какого более высокого уровня доказательства не хватает для закрытия риска.

## Требуемая архитектура доказательств

### 1. Machine-readable test manifest

Для каждого gate должны храниться:

- стабильный test ID;
- связанные risk IDs;
- тип: static/unit/model/runtime/sanitizer/hardware/manual;
- точная команда и required environment;
- production components, которые реально компилируются/исполняются;
- negative oracle;
- timeout и ожидаемый exit classification;
- source manifest hash и, где применимо, EXE/DLL hash;
- output evidence path/schema.

Manifest не должен утверждать coverage по имени файла. Coverage объявляется
только через точный component list.

### 2. Regression-first для каждого P0/P1

Перед исправлением:

1. создать минимальный old-bug fixture;
2. подтвердить, что он падает именно по ожидаемой причине;
3. внести корневое исправление;
4. подтвердить PASS и соседние negative cases;
5. выполнить selected mutation, возвращающий старый дефект, и получить FAIL;
6. провести higher-level wiring test;
7. при protocol/rate/layout риске оставить hardware gate явным.

### 3. Один production core вместо параллельной модели

Модели сохраняются для exhaustive/property tests, но критические policy
functions должны быть pure modules, которые использует production. Нельзя
считать тест отдельного похожего алгоритма доказательством `Backend_Tick`.

Минимальные seams:

- provider snapshot acquisition и validation;
- source arbitration и freshness;
- profile parse/validate/atomic commit;
- full KeyIdentity mapping;
- curve/SOCD/XUSB transform;
- provider lifecycle/neutralization/restart.

### 4. Точный artifact chain

Release summary должен связывать:

```text
source manifest hash
  -> dependency lock hash
  -> compiler/toolchain/config
  -> DLL/EXE hashes
  -> executed gate IDs/results
  -> hardware evidence IDs/PENDING
  -> package hash
```

Так как проект сознательно не использует git в текущей работе, source manifest
hash особенно важен: он должен вычисляться по каноническому списку production,
test, tools и authoritative docs без зависимости от repository metadata.

### 5. Current evidence index

Исторический `VALIDATION_MATRIX.md` остаётся журналом. Рядом нужен generated
current index, где для каждого риска ровно один актуальный статус:

- `PASS`;
- `FAIL`;
- `MISSING`;
- `HARDWARE_PENDING`;
- `SUPERSEDED` только со ссылкой на новый evidence ID.

### 6. Hardware evidence schema

Каждый физический результат связывает model, VID/PID/interface, firmware,
route/provider, exact EXE SHA-256, действия, expected observations, raw/log hash,
результат и ограничения claim. Отсутствующее устройство не превращается в
PASS; это честный `HARDWARE_PENDING`.

## Порядок внедрения V14-22

1. Зафиксировать machine-readable manifest существующих gates без изменения
   их смысла; выявить отсутствующие команды и ложные labels.
2. Добавить sanitizer health control и честную parser coverage matrix.
3. Для каждого открытого P0 создать old-bug fixture и отрицательный oracle.
4. Заменить anti-fix requirements на versioned migration contracts.
5. Затем покрыть P1 по порядку зависимости: identity/provider snapshot,
   lifecycle/freshness, profile/arbitration, precision/rate/capacity.
6. Разделить fast build и exact-artifact release qualification.
7. Генерировать current evidence index и блокировать release при любом
   `FAIL`, `MISSING` или обязательном `HARDWARE_PENDING`.
8. Только после этого использовать зелёный gate как основание закрывать
   `V14-19`..`V14-21`.

## Acceptance gates V14-22

Пакет нельзя считать завершённым, пока одновременно не выполнено следующее:

- каждый открытый P0/P1 имеет test manifest entry и отрицательный oracle;
- ни один mandatory test не требует сохранить известный дефект;
- static/model result нигде не подписан runtime/final-artifact proof;
- selected defect mutations для P0 и архитектурных P1 reliably killed;
- sanitizer health negative control падает ожидаемым образом;
- sanitizer parser matrix соответствует фактическому production catalog;
- официальный release runner исполняет, а не только перечисляет required gates;
- exact final EXE hash присутствует во всех process/hardware evidence;
- current evidence index генерируется и не наследует superseded PASS;
- hardware gaps остаются явными и не заменяются simulator evidence;
- полный fast gate, integration gate и release qualification проходят на
  одном source manifest; финальный package hash совпадает с evidence summary.

## Выполненная локальная проверка

Запущено:

```text
python tools\run_native_backend_checks.py --require-compiler
python tools\run_protocol_fuzz_sanitizers.py
python tools\run_aula_win60he_sanitizers.py
```

Результаты:

- portable/static gate: PASS, 59 static audits и 34 C++ executables;
- общий sanitizer fuzz: PASS, 250000 iterations, 61998 accepted cases;
- Aula sanitizer suite: PASS, 6 executables;
- Windows ASan выдал `interception_win: unhandled instruction`; runner не
  классифицировал это как failure, поэтому sanitizer health остаётся
  недоказанным до отрицательного контроля.

Эти запуски подтверждают текущую baseline-воспроизводимость, но не закрывают
ни один из перечисленных protocol/architecture risks.

## Итоговое решение

`TEST_EVIDENCE_TRUST_AUDIT.md` входит в обязательный release blocker. Новые
риски: `HJ-V14-P1-024`..`027` и `HJ-V14-P2-016`..`017`. Roadmap package:
`V14-22`.

До выполнения V14-22 запрещено интерпретировать успешный `BUILD.cmd`, число
PASS в validation log или наличие runner-файла как достаточное доказательство
готовности следующей стабильной версии.

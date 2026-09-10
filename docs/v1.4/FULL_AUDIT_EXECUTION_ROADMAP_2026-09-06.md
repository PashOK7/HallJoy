# HallJoy — полный охват аудита и пошаговый Roadmap исполнения

## 2026-09-06 — Перестройка каталогов

Текущие пути задаёт [PROJECT_LAYOUT](../current/PROJECT_LAYOUT.md), а результат
проверки — [STRUCTURE_MIGRATION](../validation/STRUCTURE_MIGRATION_2026-09-06.md).
RM-00/RM-32/RM-36: учитывать выполненный структурный пакет, не повторять старые
переносы и не брать EXE из исторических x64/build/output. Исходное обследование
FS-00..05 сохранено как baseline. Это не закрывает весь RM-32 или runtime-аудит.
Discord RM-37 остаётся последней feature-карточкой.


## 2026-09-06 — Structure review and previously missed Sayo design notes

See [documentation index](../README.md) and
[file-structure review / FS-00..05](../PROJECT_STRUCTURE_REVIEW_2026-09-06.md).
The tree had 4707 files / 1840.57 MiB at measurement; 927 generated intermediates
in three source-local x64 roots account for 1130.35 MiB, subject to rebuild/owner
checks before cleanup. No source/output files were deleted or moved.
Important RM-03 input: [SAYO_DEVICE_NOTES.md](../../src/HallJoyProject/SAYO_DEVICE_NOTES.md)
explicitly documented automatic user-letter matching on 2026-05-14. The earlier
audit missed this source-adjacent document. Read it before proposing Sayo changes;
its capture paths are placeholders, not newly verified raw evidence. FS tasks
feed RM-00/32/36; Discord RM-37 remains the final feature card in this Roadmap.


Дата: 2026-09-06. Версия документа: 1.2 (вопросы владельцу и Discord-фича).
Рабочий проект: `W:\github\_halljoy_latest_20260817\HallJoy-main`.
Статус: **план исполнения аудита и исправлений; программа НЕ объявлена полностью проверенной**.


## 0. Сохранение продуктового замысла — уточнение владельца

Указание владельца 2026-09-06 имеет приоритет над прежними запретами Sayo learning:
**HallJoy автоматически определяет именно пользовательские буквы; ручных
назначений, мастера или обязательного подтверждения быть не должно.**
Автоматическое сопоставление сохраняется, неоднозначное обучение должно быть
защищено. Пользователь сообщает об отсутствии жалоб на текущую реализацию.
Это положительный опыт эксплуатации; воспроизведённого сбоя у него не установлено.

Прежняя классификация самого Sayo auto-learning как P0 и рекомендация заменить
его ручными/безбуквенными назначениями были чрезмерными. Их НЕ выполнять.
При reconciliation HJ-V14-P0-003 разделить допустимое обучение, проверяемый риск
неоднозначности и отдельный вопрос stale-depth fallback. Не переносить severity
исторического аудита на сам факт разрешённого автоматического сопоставления.

### 0.1. Обязательная проверка назначения текущей реализации

До production edit каждой карточки записать:
1. Какую пользовательскую задачу решает нынешняя ветка, что подтверждено
   владельцем/документами и что лишь предполагает аудитор.
2. Какой дефект воспроизведён или какой bottleneck измерен. Наличие mutex,
   fallback, нескольких потоков или большого файла само по себе не дефект.
3. Какие outputs, автоподхваты, настройки и совместимость сохраняются.
   Сначала characterization успешных сценариев, затем изменение.
4. Почему выбранный вариант лучше локального исправления без смены поведения.
   Архитектура из карточки — предложение, а не обязательная реализация.
5. Как доказать отсутствие регрессии; если изменения не нужны, завершить
   REVIEWED_NO_CHANGE. Не строить цепь больших рефакторингов ради локального fix.

Изменение автоматического UX на ручной, удаление поддержки или привычного режима
требует PRODUCT_DECISION_REQUIRED. Можно завершать evidence и совместимые fixes,
но нельзя самостоятельно менять продукт. По Sayo решение уже дано выше:
повторно запрашивать согласование сохранения auto-learning не нужно.

**Правило владельца:** в спорных ситуациях, когда ожидаемое поведение программы
неясно, документы противоречат друг другу или текущая архитектура могла быть
выбрана специально, спросить владельца: «Как программа должна вести себя в
этом сценарии?» Кратко описать конкретный сценарий, нынешнее поведение и варианты
с последствиями. До ответа не реализовывать спорное изменение; продолжать
независимые проверки и совместимую работу. Ответ записать в owning карточку и
handoff. Уже принятые владельцем решения не переспрашивать; обычные технические
детали, не меняющие поведение продукта, решать самостоятельно.

### 0.2. Остальные ловушки, обнаруженные при перепроверке плана

| Карточки | Возможный намеренный компромисс | Ограничение для исполнителя |
|---|---|---|
| 03/11 | цифровой fallback поддерживает устройство при недоступной глубине | установить режим/контракт; число 1000 само по себе не доказывает дефект; измеренную глубину и цифровое состояние различать |
| 04/09 | независимые rows/samples уменьшают задержку | freshness элемента не требует ждать полного медленного скана |
| 05/22 | дедупликация и событийный idle экономят CPU | неизменный отчёт/долгое удержание не равны stalled producer |
| 06/07 | atomics, короткий gate, редкий slow path экономят память | не вводить immutable config/общую таблицу без proof или измерений |
| 09/10/21 | milli, округление и deadzone задают совместимость/feel | не менять точность и пороги наугад; raw evidence и A/B обязательны |
| 11 | max/native/UAP/digital задаёт привычное управление | не заменять max на sum/priority и не убирать fallback под видом унификации |
| 12 | legacy/shadow нужны совместимости и квалификации | найти consumers и цену; допустимо сохранить; не удалять просто как старый код |
| 14/15 | retained resources/poisoned state защищают lifetime | не detach/force-close; child process для каждого backend не обязателен |
| 16 | полное освобождение devices и output-disable — разные функции | не менять существующую кнопку и controller continuity молча |
| 17 | несколько экземпляров обслуживают независимые устройства/roots | защищать конфликтующие ресурсы, а не автоматически запрещать все экземпляры |
| 18 | sync save даёт немедленную durability | фон/debounce только при доказанном выигрыше без потери сохранения |
| 19/20 | defaults/tolerant parsing поддерживают старые файлы | реальные legacy fixtures до ужесточения; неизвестное не всегда повреждённое |
| 23/24 | private roles/ASI ABI обеспечивают совместимость | threat boundary проверить, законные child/ASI не сломать чрезмерной проверкой |
| 27/35 | крупные файлы и диагностические эксперименты — рабочая организация | не менять framework и не удалять users ветки ради размера файла |
| 29/30 | доказательство поддержки может быть вне найденного архива | отсутствие найденных логов не равно поломке; release scope решать явно |

Кроме Sayo, назначения в этой таблице — **гипотезы для проверки**, а не уже
подтверждённые владельцем требования. Реальные release blockers остаются;
гипотетический риск не превращается в обязательное переписывание по одному P1.

## 1. Как понимать «полный аудит»

Этот файл охватывает основные подсистемы программы и задаёт конечный порядок их
проверки, исправления и квалификации. Он не утверждает, что все 90 тысяч строк
уже построчно проверены или что неизвестных ошибок больше не существует.
Включены подтверждённые текущим кодом проблемы, архитектурные риски, исторические
блокеры для повторной сверки и обязательные ещё не выполненные проверки.
Финальный RM-36 специально не позволяет выдать полный план за полный PASS.

В первичной инвентаризации 234 файла: 231 source/header/include/resource/project
файл в H и три ключевых translation units UAP/Soup. Суммарно 90 463 строки
в этой выборке, включая заголовки и проектные файлы. Это НЕ число полностью
прочитанных строк. Tests, tools, runtime DLL, UAP headers, generated/dependency
sources и research перечислены отдельными областями охвата; их compilation
closure требуется дополнить в RM-00/RM-32/RM-36.

37 карточек аудита и новая фича RM-37 в самом конце начинаются со статуса TODO.
Всего 38 карточек. Это статус работы, а не утверждение
о 37 доказанных багах. «Проверка» в карточке может закончиться REVIEWED_NO_CHANGE.
Код исправлять только там, где установлена причина или доказана полезность
архитектурного изменения. Никаких автоматических переписываний всего проекта.

В этой сессии выполнялись чтение кода/документации, поиск, инвентаризация и хеширование.
Производственные исходники, EXE, настройки и HID не изменялись. Runtime, input,
controller и аппаратные прогоны не выполнялись. Отчёт не содержит нового
performance/soak/hardware PASS.

## 2. Короткая инструкция для следующей нейросети

1. Работай только в указанном проекте HallJoy. Текущий cwd может быть
   `A:\forAI quicksave`: это ДРУГОЙ проект; Mafia skills сюда не применять.
2. Прочитай этот раздел, ENGINEERING_WORKING_METHOD.md, актуальный handoff,
   затем только выбранную карточку и названные ею файлы.
3. Не начинай несколько архитектурных пакетов одновременно. Выбери первую
   TODO-карточку, чьи зависимости закрыты; сбор evidence можно делать раньше,
   зависимые production edits — после prerequisites.
4. До изменения воспроизведи проверяемый сценарий. Если текущий код уже исправлен,
   докажи это тестом/ссылкой и отметь REVIEWED_NO_CHANGE, не создавай второй fix.
5. Для изменения ownership/ABI/data model сравни локальное исправление,
   поэтапную миграцию и полную замену. Запиши выбор в DECISIONS.md до production edit.
6. Перед каждой записью перечитай/хешируй target. Перед широкими изменениями
   сделай timestamped backup с проверкой равенства hashes. Используй apply_patch.
7. Выполни ровно указанный инвариант, целевые тесты и обязательные checks.
   Не расширяй scope до переписывания соседних компонентов «заодно».
8. Обнови карточку, owning risk, validation, worklog и handoff.
   Останови зависимое продвижение при неподтверждённом protocol/ownership.
9. При нехватке контекста оставь точные files/functions, команды, hashes,
   наблюдение, следующий шаг и что запрещено повторять; не пиши просто «почти готово».
10. Нельзя автоматически запускать тесты с реальным вводом/геймпадом.
    Пользователь просил не мешать игре. Смена WASD на другие клавиши не решает это.

### 2.1. Готовый стартовый запрос исполнителю

> Продолжи HallJoy по FULL_AUDIT_EXECUTION_ROADMAP_2026-09-06.md.
> Сначала прочитай правила, сверку baseline и dependencies.
> Возьми одну доступную карточку RM-XX, прочитай её файлы, проверь актуальность
> проблемы и сделай законченный пакет в её границах. Не используй Git при
> действующем локальном запрете. Перед edits перечитывай файлы и делай проверенные
> backups. Не запускай hardware/controller/input тесты. Сохраняй автоматические буквы Sayo и проверяй неоднозначность;
> не подменяй измеренную глубину цифровым состоянием. Не выдавай compile/static PASS за runtime proof.
> В конце запиши evidence, ограничения и одну следующую доступную карточку.

Этот шаблон — инструкция для БУДУЩЕГО исполнения, а не заявление, что текущий
запрос на аудит уже разрешил массовое переписывание или аппаратные действия.

### 2.2. Обозначения путей и терминов

Все сокращения относительны корню HallJoy:

| Обозначение | Каталог |
|---|---|
| H | src/HallJoyProject/HallJoy |
| T | src/HallJoyProject/tests |
| P | third_party/UniversalAnalogPluginFixed |
| tools | tools в корне проекта |
| docs | docs в корне проекта |

`H/file.cpp:Function` означает открыть файл и найти Function через rg; это
не номер строки. Номера строк старых аудитов могут сдвигаться.
Новые имена RuntimeConfig/InputSnapshot/RuntimeSupervisor в плане — предлагаемая
модель, а не гарантированно существующие сейчас классы.

- **Generation** — номер жизненного цикла/согласованной версии; поздние данные
  предыдущего номера не должны попасть в новый.
- **Freshness** — допустимый возраст действительного sample, не просто «поток жив».
- **Owned zero** — источник владеет key и сообщает корректный ноль.
- **Neutralization** — нулевые оси/triggers/buttons; это отдельное действие от stop.
- **Reap** — подтверждение завершения worker/process до освобождения его ресурсов.
- **Oracle** — проверка, способная отличить старый дефект от правильного поведения.
- **Characterization** — фиксация существующего корректного поведения до рефакторинга.
- **Removal gate** — конкретные условия, после которых можно удалить старый adapter.
- **P0/P1/P2** в этом файле — порядок работ по последствиям, не автоматическая
  severity-метка доказанного бага для каждой исследовательской карточки.

## 3. Факты исходной точки и что нельзя потерять

Последний указанный production artifact:
`build/release/HallJoy.exe`, также копия в `build/output`.
SHA-256: `44CFC0793A9CFD2A74B2B677AD8704708B1E16C6CD764EA8852C14962ECBE012`.
Перед исполнением перепроверить; root-level HallJoy.exe может быть старым.

Предыдущее evidence:
- 79 static audit invocations и 49 C++ compilations прошли source suite;
- profile transactions, CSV всего домена, пять стадий отказа записи, concurrent
  loads и сохранение файлов после rejected startup прошли отдельные проверки;
- MSVC дал 0 ошибок и baseline LNK4099; package сформирован;
- внешний runner финальной сборки вернул 1, несмотря на успешные стадии в логе;
- полный portable runtime suite не закрыт после отказа загрузки существующих данных;
- file-only portable startup на чистой копии прошёл;
- hardware/soak qualification прежнего EXE не переносится на новый hash.

Evidence: `.analysis/profile_audit_fixes_20260905_evidence/RESULT.md`.
Предыдущие исправления F-01..F-08 и startup autosave guard сохранять:
bounded layout/INI, строгая проверка numeric/CSV, staged load, отказ удаления
активного профиля, согласованное чтение profile config и единый bundle atomic save.

**Это не отменяет проверяемые риски:** защита неоднозначности Sayo auto-learning,
SparkLink row-freshness gap, незавершённый общий
provider contract и вопрос producer freshness. Старый документ сам по себе
не доказательство актуальности; здесь для этих пунктов дополнительно прочитан код.

Сохранять полезные части: versioned IPC slots, generation ownership,
UAP/output process isolation, snapshot broker, configured_xusb builder,
atomic profile replace. Не заменять их более простыми, но некорректными примитивами.

## 4. Карта программы и обязательные границы

```text
HID / UAP plugin / native sessions / Raw Input / mouse IPC
              |
    identity + value + freshness + generation
              |
    snapshots -> source arbitration
              |
       RuntimeConfig одного тика
              |
    curves -> configured XUSB builder
              |
       output mailbox / IPC
              |
        ViGEm child -> virtual pads

UI -> commands -> runtime owner / config publisher / persistence owner
                 |
       start / pause / recover / stop
Overlay <- immutable telemetry; не владелец realtime
```

Это целевая карта границ; текущая реализация часть стрелок проходит через backend.cpp
и глобальные setters. Не вводить сразу все новые классы: сначала один инвариант.

| Подсистема | Текущие точки входа | Главная проверка | Карточки |
|---|---|---|---|
| Startup/shutdown/roles | main.cpp, app.cpp | частичный startup, повтор stop, роли child | 02,15,17,23 |
| Native catalog/routing | native_analog_backend*, native_analog_routing* | proof, ownership, состав релиза | 09,11,29,30 |
| SparkLink/Sayo | backend_sparklink.inc, backend_sayo.inc | row freshness, digital identity | 03,04,30 |
| Остальные native | *_backend.cpp, *_protocol.cpp | parser + session + freshness | 14,30 |
| UAP/Soup | P/main.cpp, overlay/Soup | lifecycle, transport, scale, snapshots | 09,10,13,30 |
| IPC/broker | analog_host_client, provider_v2*, vigem_output*, mouse_ipc | generation, ABI, lifetime | 05,12,23,24 |
| Config/profiles/layouts | settings*, bindings*, global_profiles*, keyboard_layout* | согласованность, миграция, parsing | 06,07,18,19,20 |
| Расчёт управления | backend_curve, curve_math, configured_xusb_builder | точность, SOCD/LKP, replay | 08,10,11,21 |
| Realtime/output | realtime_loop, vigem_output_* | freshness, wake, deadline, neutral | 05,15,22 |
| UI/render/hooks | app, keyboard_*, remap_*, premium_* | ресурсы, capture, latency UI | 26,27 |
| Overlay | overlay_server | HTTP/Origin/session/resource bounds | 25 |
| Diagnostics/privacy | debug_log, stability_trace, analyzers | first failure, redaction, overhead | 28 |
| Storage/reset | app_paths, ini_util, factory_reset, file_name_policy | crash consistency, чужие файлы | 18,19,20 |
| Build/resources/deps | vcxproj, rc, tools, .github, runtime DLL | точный состав и provenance | 00,02,29,32,34 |
| Tests/research | T, tools, docs/research, LAB plan | coverage, отсутствие ложного PASS | 01,31,35,36 |

### 4.1. Общие инварианты — проверять после каждого затрагивающего пакета

1. Физический канал и измеренная глубина не выдумываются из digital keydown.
   Sayo автоматически сопоставляет этот канал с пользовательской буквой по
   однозначным событиям: это разрешённое продуктовое требование, не нарушение.
2. Stale/invalid sample не становится full press и не живёт бесконечно ненулевым.
3. Данные разных поколений не склеиваются; new session начинается очищенным.
4. UI не единственный наблюдатель критической liveness.
5. Realtime не делает HID open/enumeration, disk I/O, join или unbounded wait.
6. Один тик использует согласованную конфигурацию; lifecycle не освобождает
   ресурсы, которыми продолжает пользоваться старый reader.
7. Native/UAP arbitration сохраняет source ownership и различает valid zero.
8. Известные raw/precision не теряются раньше нужного output преобразования.
9. Сохранение либо целое, либо с проверяемым восстановлением; error не называется saved.
10. Test-only поведение не меняет production semantics и не генерирует ввод случайно.
11. Применимые P0/P1 не закрываются только сборкой; hardware pending виден.
12. Любой PASS относится к конкретному source/artifact и конкретному сценарию.

## 5. Что установлено сейчас, а что ещё проверять

| ID | Основание | Вывод текущего чтения | Дальше |
|---|---|---|---|
| E01 | backend_sayo.inc:SayoParseKeyboardReport | автоматические буквы разрешены владельцем; addedCount==1 уже есть; межсобытийная неоднозначность требует oracle | RM-03, сохранить UX |
| E02 | backend_sayo.inc:SayoSetIndexState | при несвежей глубине down может записывать 1000; назначение fallback ещё проверить | RM-03/11, отдельный contract |
| E03 | backend_sparklink.inc + backend.cpp | row timestamps есть; getMilli не проверяет age; общий failStreak сбрасывается другими успехами | RM-04 |
| E04 | vigem_output_process_host.cpp | timeout публикует child heartbeat без producer-progress deadline | RM-05 |
| E05 | key_settings.cpp | fast reader содержит for(;;), extended читает shared_mutex | RM-06/07 |
| E06 | backend_curve.cpp | cache.curves ограничен hid<256 | RM-07 |
| E07 | backend.cpp:BuildConfiguredReportPair | строятся qualified и shadow отчёты | RM-12, сначала qualification |
| E08 | native_analog_backend_registry.h/.cpp | общий native результат milli/owned/connected, per-key catalog scan/max | RM-09/10/11 |
| E09 | keyboard_bind_panel.cpp/settings_ini.cpp | UI action вызывает полный sync bundle save | RM-18 |
| E10 | app.cpp:watchdog | восстановление проверяется из UI timer | RM-15 |
| E11 | addressed_analog_backend.cpp | reader.join внутри worker сохраняет lifetime, но не изолирует отказ на отдельный process | RM-14, не ломать safety |
| E12 | analog_host_client.cpp | plugin path аргумента достигает LoadLibraryW; PID/nonce не являются exact DLL identity | RM-23, threat boundary |
| E13 | native_analog_backends.def | Hero84 ordinary, ND75 gated; единый release tier не выражен | RM-29 |
| E14 | backend_sparklink.inc | недостижимая if(false) burst ветка остаётся | RM-35, не включать её |
| E15 | sanitizer runner | один fuzz runner перечисляет три protocol cpp | RM-31, полный coverage inventory |
| E16 | main/app/API search | глобальный engine pause/instance guard не найден в просмотренном пути | RM-16/17, подтвердить полноту поиска |
| E17 | old/new docs | current claims и hashes разных поколений сосуществуют | RM-00/36 |

E01..E15 — наблюдения кода, а не все воспроизведённые runtime отказы.
E16 — результат ограниченного поиска, не доказательство отсутствия любой реализации.
Overlay, Raw Input, численная математика, все семьи прошивок, GDI и dependency
closure ещё требуют предметных проверок; им намеренно не приписаны выдуманные баги.

## 6. Порядок исполнения и зависимости

Сначала RM-00 и RM-01. Критическое ядро: RM-03, RM-04, RM-05.
RM-02 можно разбирать после RM-01. Проверку trust boundary RM-23 и catalog RM-29
можно делать независимо после их prerequisites, не откладывая ради оптимизаций.

Затем config/core RM-06..08; общий contract/arbitration/V2 RM-09..12;
UAP/native containment/runtime/pause RM-13..17; storage RM-18..20.
Performance RM-21/22/33 только с baseline, UI/HTTP/IPC RM-24..28 по зависимости.
Protocols RM-30 исполнять по одной семье, не как один огромный rewrite.
Tests/CI RM-31/32, release RM-34, experiment ledger RM-35 и финальное покрытие RM-36.

Зависимости означают необходимую готовность к ЗАВЕРШЕНИЮ карточки, а не запрет
собирать evidence раньше. Для RM-30 и RM-31 допускаются совместные малые
подпакеты parser+test одного семейства; нельзя ждать «все протоколы переписаны»,
чтобы написать первый тест. Неизвестное железо блокирует его hardware PASS,
но не остальные независимые source/file-only задачи.

### Индекс карточек

| ID | Приоритет | Задача | Зависимости | Статус |
|---|---|---|---|---|
| RM-00 | P0 | Зафиксировать исходную точку и актуальные статусы | — | TODO |
| RM-01 | P0 | Разделить проверки по побочным эффектам | 00 | IMPLEMENTED_PARTIAL |
| RM-02 | P1 | Разобрать exit 1 после успешной упаковки | 00,01 | TODO |
| RM-03 | P1 | Sayo: автоматические буквы и защита неоднозначности | 00,01 | IMPLEMENTED (portable/static; hardware timing pending) |
| RM-04 | P0 | Закрыть stale отдельных строк SparkLink | 00,01 | IMPLEMENTED (portable/static; hardware pending) |
| RM-05 | P0 | Независимая свежесть producer в output child | 00,01 | IMPLEMENTED (portable/static; hardware/output pending) |
| RM-06 | P1 | Единая immutable RuntimeConfig | 03,04,05 | IMPLEMENTED_PARTIAL (characterized gate; bounded admission) |
| RM-07 | P1 | Одинаковый путь для всех поддержанных keys | 06 | IMPLEMENTED (portable/static; hardware pending) |
| RM-08 | P1 | Чистый расчёт управления и replay | 06,07 | IMPLEMENTED (portable/static; hardware pending) |
| RM-09 | P1 | Общий snapshot-контракт native и UAP | 00,08 | IMPLEMENTED (portable/static; Spark hardware pending) |
| RM-10 | P1 | Сохранить точность до границы выходного формата | 09 | NEEDS_EVIDENCE (Spark raw domain) |
| RM-11 | P1 | Явное объединение нескольких источников | 08,09 | IMPLEMENTED (portable/static; hardware pending) |
| RM-12 | P1 | Завершить Provider V2 и убрать постоянный shadow | 08,09,11 | NEEDS_EVIDENCE (qualification removal gate) |
| RM-13 | P1 | Жизненный цикл UAP: reconnect и invalidation | 01,09 | IMPLEMENTED (static/simulator coverage; physical pending) |
| RM-14 | P1 | Ограничить последствия зависшего native HID | 01,09 | IMPLEMENTED_PARTIAL (fail-closed retained-resource boundary) |
| RM-15 | P1 | Runtime supervisor независимо от UI | 05,13,14 | IMPLEMENTED_PARTIAL (static lifecycle coverage; physical recovery pending) |
| RM-16 | P1 | Полная Pause/Resume с освобождением устройства | 15 | IN_PROGRESS (command/admission foundation; no user-visible pause) |
| RM-17 | P1 | Защита конфликтующего владения экземпляров | 15,16 | TODO |
| RM-18 | P1 | Один последовательный владелец сохранений | 06,15 | TODO |
| RM-19 | P1 | Довести migration, recovery и factory reset | 01,18 | TODO |
| RM-20 | P1 | Единый контракт внешних чисел и INI | 00,19 | TODO |
| RM-21 | P2 | Математика кривых и конечных XUSB значений | 08,10 | TODO |
| RM-22 | P2 | Планирование realtime и стоимость пробуждений | 05,08,12 | TODO |
| RM-23 | P1 | Доверенность внутреннего analog-host и DLL | 00,01 | TODO |
| RM-24 | P1 | Проверить все IPC и lifetime leases | 00,01 | TODO |
| RM-25 | P1 | Overlay HTTP: доступ, границы и доступность | 01,15 | TODO |
| RM-26 | P1 | Raw Input, hooks и mouse integration | 08,15,16,24 | TODO |
| RM-27 | P2 | UI: состояние, DPI, ресурсы и большие файлы | 06,18,26 | TODO |
| RM-28 | P1 | Наблюдаемость без постоянной тяжёлой записи | 05,15,23 | TODO |
| RM-29 | P1 | Единая граница release/experimental/diagnostic | 00 | TODO |
| RM-30 | P1 | Поштучный protocol audit всех enabled families | 01,03,04,09,10,11,14,29 | TODO |
| RM-31 | P1 | Поведенческие тесты, sanitizer и доказательство самого harness | 01,08,30 | TODO |
| RM-32 | P1 | Сборка, зависимости и CI как одна проверяемая цепочка | 00,01,02,29,31 | TODO |
| RM-33 | P2 | Измеримый performance/resource audit | 07,08,12,18,21,22,25,27 | IMPLEMENTED_PARTIAL (portable evidence; live UAP-provider hardware gate remains) |
| RM-34 | P1 | Квалификация одного точного release artifact | 02,03,04,05,12,14,16,19,23,24,25,26,28,29,30,31,32,33,17,20,35 | IN_PROGRESS (hash-bound controlled evidence; release gates remain) |
| RM-35 | P2 | Закончить или закрыть незавершённые эксперименты | 00,29,30 | IMPLEMENTED_PARTIAL (source/catalog disposition complete; physical gates remain) |
| RM-36 | P1 | Закрыть аудит по покрытию, а не по длине отчёта | 00,31,34,35 | IN_PROGRESS (source/risk inventory complete; final-artifact and physical gates remain) |
| RM-37 | Фича | Неподдерживаемая клавиатура и Discord-сообщество | 00,29; см. последний раздел | IN_PROGRESS (safe identity boundary and placeholder; official invite remains) |

## 7. Карточки исполнения

Каждая карточка — самостоятельный пакет после prerequisites. Все шаги ниже
требуют актуальной проверки исходников; предлагаемая реализация не подменяет
обязательное сравнение альтернатив из ENGINEERING_WORKING_METHOD.

### RM-00 — Зафиксировать исходную точку и актуальные статусы

- [x] Статус: DONE (documentation baseline; no runtime qualification). Приоритет: P0. Зависимости: —.

**Результат 2026-09-06:** [CURRENT_STATE_INDEX_2026-09-06.md](CURRENT_STATE_INDEX_2026-09-06.md)
разделяет новый exact release hash `A4D8…E37` от исторического `44CF…E012`,
фиксирует build/dependency contract и назначает owner каждому историческому
P0/P1. Source-only ROG diagnostic scaffold намеренно не приписан уже собранному
release EXE. Backup: `.local/backups/rm00_baseline_20260906_133258`.

**Основание:** Организация; прежние PASS относятся к разным EXE.

**Где работать:** docs/v1.4/CURRENT_HANDOFF_2026-08-20.md; docs/v1.4/RELEASE_READINESS_AUDIT_2026-09-05.md; H/version.h; tools/build.ps1.

**Зачем:** Без одной исходной точки исполнитель может проверить старый EXE или повторно «исправить» уже закрытую ошибку.

**Шаги:**

1. Прочитать этот файл, ENGINEERING_WORKING_METHOD и последние записи handoff; не считать нижние исторические записи актуальными.
2. Сверить SHA-256 build/release/HallJoy.exe с 44CFC0793A9CFD2A74B2B677AD8704708B1E16C6CD764EA8852C14962ECBE012; при отличии записать новый baseline, не подменять старый.
3. Зафиксировать исходники, build defines, версии компиляторов и dependency-lock в manifest текущего пакета.
4. Перенести все старые P0/P1 в таблицу reconciliation: текущий код / старый oracle / актуальный статус / owning RM.
5. Создать timestamped backup затрагиваемых файлов и проверить хеши копий; рабочие файлы перечитывать перед каждой заменой.

**Отрицательная проверка / oracle:** Подмена EXE старым экземпляром обязана сделать его evidence неприменимой.

**Готово, когда:** Есть один current-state index; каждому старому блокеру назначен владелец или явный NEEDS_REVIEW.

**Не делать:** Не выполнять git init/checkout: действующая локальная инструкция запрещает Git. До изменения этой инструкции использовать hashes и snapshots.

**Результат исполнителя 2026-09-06:** DONE как documentation baseline;
см. `CURRENT_STATE_INDEX_2026-09-06.md` и hash-verified backup
`.local/backups/rm00_baseline_20260906_133258`.

### RM-01 — Разделить проверки по побочным эффектам

- [ ] Статус: IMPLEMENTED_PARTIAL — runner registry and file-only backend guard; fake-transport operation counters remain TODO. Приоритет: P0. Зависимости: 00.

**Промежуточный результат 2026-09-06:**
[RUNNER_SIDE_EFFECT_REGISTRY_2026-09-06.md](RUNNER_SIDE_EFFECT_REGISTRY_2026-09-06.md)
классифицирует все 19 `run_*.ps1`; output-capable routes запрещены в текущей
сессии. Profile runner передаёт fail-closed `--halljoy-test-forbid-backend-init`,
который останавливает `Backend_Init` до ViGEm/HID lifecycle. Static checks PASS;
EXE не собирался и не запускался. Backup:
`.local/backups/rm01_file_only_guard_20260906_134455`.

Дополнительно simulator-only oracle-счётчик обязан остаться нулевым в обеих
file-only ветках; production-linked profile test записывает
`backend_init_attempts=0`. Подробности и границы fake-transport счётчиков — в
реестре. Backup документации:
`.local/backups/rm01_documentation_20260906_141500`.

**Основание:** Подтверждено: simulator и обычные smoke могут создавать контроллер.

**Где работать:** tools/run_analog_simulator.ps1; tools/run_storage_migration_test.ps1; tools/run_profile_transaction_tests.ps1; H/main.cpp; H/app.cpp.

**Зачем:** Проверка файлов не должна случайно отправлять игровой ввод; имя «simulator» не гарантирует изоляцию.

**Шаги:**

1. Составить реестр всех tools/run_*.ps1: что запускает, какие процессы, HID, gamepad, окна и файлы затрагивает.
2. Выделить file-only, fake-transport, process-without-I/O и real-device/output режимы; неизвестный эффект считать непроверенным.
3. Проверить ранний выход profile-transactions и profile-startup-only до Backend_Init; повторно проверить main до App_Run.
4. Для будущих harness использовать внедрённый fake transport; добавить счётчики запрещённых CreateTarget/SendInput/HID open, которые обязаны остаться нулём.
5. Полные simulator/migration/soak тесты обозначить как output-capable; не запускать их при текущем запрете игрового ввода.

**Отрицательная проверка / oracle:** Попытка backend initialization в file-only режиме приводит к FAIL, даже если сам профильный тест зелёный.

**Готово, когда:** Каждый runner имеет класс эффектов, изолированный root и cleanup только собственных ресурсов.

**Не делать:** Не заменять WASD другими клавишами как средство изоляции: виртуальный stick всё равно может двигаться.

**Результат исполнителя 2026-09-06:** pure command state now starts closed,
distinguishes safe failed fresh-resume cleanup from retained-resource fault, and
the backend has one tested admission gate for tick/input/topology paths. The
serialized closed-start owner, bounded UI bridge, initial startup/shutdown
migration, and state-derived Global Settings control are now wired together.
Physical release/coexistence evidence is deliberately still pending; see
`ENGINE_RUNTIME_OWNER_IMPLEMENTATION_RM16_2026-09-06.md`.

### RM-02 — Разобрать exit 1 после успешной упаковки

- [ ] Статус: TODO. Приоритет: P1. Зависимости: 00,01.

**Основание:** Подтверждена аномалия runner; причина неизвестна.

**Где работать:** BUILD.cmd; tools/build.ps1; .analysis/profile_audit_fixes_20260905_evidence/RESULT.md.

**Зачем:** Автоматизация не должна получать одновременно Build completed и неоднозначный статус завершения.

**Шаги:**

1. Сохранить final2 log и точную команду запуска; различить exit внешнего exec, powershell, MSBuild и дочерних self-tests.
2. В изолированном harness воспроизвести PowerShell redirection и нативные коды, не пересобирать весь проект для каждого опыта.
3. Проверить немедленный захват LASTEXITCODE после каждой native команды и проверку ExitCode после Start-Process.
4. Устранить установленную причину; не добавлять безусловный exit 0, скрывающий настоящую ошибку.
5. Выполнить один официальный build после проверки эффектов RM-01; архивировать стадии и окончательный manifest.

**Отрицательная проверка / oracle:** Намеренно упавшая стадия возвращает nonzero и не публикует готовый пакет; успешные стадии дают exit 0.

**Готово, когда:** Один и тот же результат виден из CI, PowerShell и BUILD.cmd.

**Не делать:** Не объявлять причину найденной по одному успешному повтору.

**Результат исполнителя:** пока не заполнен; использовать шаблон раздела 12.

### RM-03 — Sayo: сохранить автоматические буквы, защитить неоднозначное обучение

- [x] Статус: IMPLEMENTED (portable/static; hardware timing pending). Приоритет: P1. Зависимости: 00,01.

**Результат 2026-09-06:** Одно глобальное `pending` заменено production-used
`SayoLetterMatcher`: он принимает новую букву только при ровно одном физическом
кандидате в 80 ms окне. Два кандидата сохраняют карту без изменений; release,
timeout и reset очищают candidate state, а последующее одиночное нажатие учится
автоматически. Old-bug oracle и матрица состояний прошли portable C++ test;
полный `run_native_backend_checks.py --require-compiler` также прошёл. Обзор
вариантов и scope двух устройств: [SAYO_AUTOMATIC_LETTER_MATCHING_REVIEW_2026-09-06.md](SAYO_AUTOMATIC_LETTER_MATCHING_REVIEW_2026-09-06.md).
Ни EXE, ни HID/output тест не запускались. Backup:
`.local/backups/rm03_sayo_matcher_20260906_143000`.

**Продуктовый контракт:** трёхклавишный osu! O3C, свободные пользовательские бинды,
HallJoy автоматически определяет именно буквы. Никаких ручных назначений или
обязательных подтверждений. Владелец сообщает об отсутствии жалоб на нынешний UX.

**Основание:** depth опрашивается независимо от keydown; выученное index->HID
сохраняется между нажатиями, при новом SayoStart карта начинается с F/G/H.
Постоянного ожидания activation point на каждом нажатии в коде нет.
Защита addedCount == 1 УЖЕ существует: несколько новых HID в одном отчёте не
обучают карту. Проверить нужно несколько физических кандидатов и раздельные
keyboard reports, общий pending index, порядок и device/session scope.
Сам факт обучения не ошибка; пользовательский сбой пока не воспроизведён.

**Где работать:** H/backend_sayo.inc:SayoSetIndexState,SayoParseKeyboardReport,
SayoParseDepthResponse,Sayo_ResetKeyState; T — production-linked fake event harness.

**Шаги:**

1. Зафиксировать успешные одиночные нажатия, автоматические буквы и ранний depth после обучения. Отдельно записать default F/G/H, lifetime карты, external rebind и реальный порядок 0x21/keyboard reports; не менять эти политики попутно.
2. Создать oracle: два физических down в одном окне, затем HID reports вместе/раздельно; reverse order, auto-repeat, уже удержанная соседняя кнопка, одинаковые назначения, timeout, два устройства, reconnect. Первый сценарий: index A down -> index B down -> один новый HID A; проверить, не обучится ли pending B чужой букве.
3. При подтверждённой ошибке учитывать все релевантные кандидаты в одном device/session и окне; менять mapping только при однозначной паре physical event/new HID. При неоднозначности не обучать и не стирать уверенную карту. Не требовать отпускания всех соседних кнопок, если новая пара однозначна.
4. Сохранить автоматическую повторную попытку при следующем однозначном событии и автоматический подхват изменённых пользователем биндов. Определить reset/timeout кандидатов и порядок сообщений. Не замораживать карту навсегда и не добавлять persistence без отдельного обоснования.
5. Отдельно исследовать stale-depth->1000 как возможный digital fallback: текущий режим, UI/settings contract, возвращение к depth и release. Само значение 1000 не доказывает ошибку явно цифрового режима. Если оно выдаётся за измеренную глубину или создаёт неверный output, оформить отдельный oracle/совместимый fix; не путать это с разрешённым обучением букв.

**Отрицательная проверка / oracle:** неоднозначные события не меняют карту;
однозначное нажатие определяет букву без действий пользователя; после обучения
depth проходит до digital threshold; однозначный rebind подхватывается снова.

**Готово, когда:** сохранены успешные plug-and-play сценарии, защита неоднозначности
проверена production кодом и подтверждённый risk закрыт. Если oracle не выявил
дефекта, допускается REVIEWED_NO_CHANGE с evidence.

**Не делать:** удалять обучение как класс; ручной wizard; фиксированные F/G/H;
безбуквенные назначения вместо требуемых букв; отключать Sayo из-за отсутствия
универсальной карты; приписывать каждому нажатию задержку до activation point.

**Результат исполнителя:** пока не заполнен; использовать шаблон раздела 12.

### RM-04 — Закрыть stale отдельных строк SparkLink

- [x] Статус: IMPLEMENTED (portable/static; hardware pending). Приоритет: P0. Зависимости: 00,01.

**Результат 2026-09-06:** Row-local normalized HID values и fresh flags теперь
явно отделены от опубликованного aggregate. Expiry, RowLimit reduction и reset
исключают только соответствующую строку; затронутый HID пересчитывается как
максимум свежих row owners, поэтому duplicate HID свежей строки не обнуляется.
Deadline 2160 ms выведен из 8 × (250 ms transaction timeout + 20 ms safe poll).
Portable oracle покрывает never-seen, row B stale при живой A и duplicate HID;
static/portable native suite PASS. Burst `if(false)` не включён. Детали:
[SPARKLINK_ROW_FRESHNESS_REVIEW_2026-09-06.md](SPARKLINK_ROW_FRESHNESS_REVIEW_2026-09-06.md).
Ни EXE, ни HID/output тест не запускались. Backup:
`.local/backups/rm04_spark_row_freshness_20260906_145000`.

**Основание:** Подтверждён разрыв между row age telemetry и выдачей значений; HJ-V14-P1-039.

**Где работать:** H/backend_sparklink.inc:SparkRecordRouteResult,цикл SparkQueryRouteRow; H/backend.cpp:BackendNative_SparkGetMilli.

**Зачем:** Успех другой строки сбрасывает общий failStreak; неудачная строка сохраняет старые значения. g_sparkRowLastOkMs читается для telemetry, но просмотренный getMilli возвращает значение без проверки срока.

**Шаги:**

1. Сделать fake clock/row transport: две активные строки, одна постоянно отвечает, вторая перестаёт отвечать после ненулевого значения.
2. Определить freshness budget строки из протокола и измеренного полного цикла; различить never-seen, valid, expired.
3. По истечении возраста публиковать обнуление принадлежащих строке keys и wake; проверить duplicate HID между строками.
4. Проверить изменение RowLimit, отключение строки, disconnect и поздний ответ предыдущей generation.
5. Удалить или вынести в отдельный доказуемый эксперимент недостижимый if(false) burst path после characterization; не включать его ради скорости.

**Отрицательная проверка / oracle:** Постоянный успех строки A не удерживает ненулевые keys умершей строки B; никогда не читанная строка не считается свежей.

**Готово, когда:** Есть bounded neutralization для каждого элемента partial snapshot, а не только всего устройства.

**Не делать:** Не обнулять весь исправный keyboard на единственную временную ошибку без обоснованной политики.

**Результат исполнителя:** пока не заполнен; использовать шаблон раздела 12.

### RM-05 — Независимая свежесть producer в output child

- [x] Статус: IMPLEMENTED (portable/static; hardware/output pending). Приоритет: P0. Зависимости: 00,01.

**Основание:** AR-01, source-review риск.

**Где работать:** H/vigem_output_process_host.cpp:RunRealHost; H/vigem_output_shared.h; H/vigem_output_runtime.cpp; H/backend.cpp.

**Зачем:** Heartbeat живого child не доказывает, что parent realtime продолжает рассчитывать управление.

**Шаги:**

1. Описать состояния producer-live, stalled, paused, stopping; отделить frame sequence от progress lease.
2. Добавить versioned lease/progress, публикуемый после успешного расчёта; выбрать единые монотонные часы и generation semantics.
3. Проверять lease в child независимо от UI; после истечения отправить нейтральное состояние с явной телеметрией причины.
4. Задать поведение возобновления: принять только актуальную generation, исключить повтор старого ненулевого snapshot.
5. Связать этот сигнал с runtime supervisor; подтвердить bounded stop/reap существующего transport.

**Отрицательная проверка / oracle:** Живой parent со stalled tick обнуляется; долгое неизменное удержание у живого producer не обнуляется; stale generation отвергается.

**Готово, когда:** Fake-clock/fake-transport матрица проходит, deadline документирован; hardware/output gate пока отдельный.

**Не делать:** Не использовать время последнего изменившегося XUSB как единственный watchdog при дедупликации.

**Результат исполнителя (2026-09-06):** implemented source contract in
`vigem_output_producer_lease.h`, version-2 fixed-layout IPC and real child.
Parent updates an independent generation-bound lease after every calculated
enabled output tick; child uses a 200-ms fake-clock-tested deadline, emits one
neutral frame and sticky producer-stalled telemetry, and rejects a snapshot
bound to a lease at or before the stale episode. Static and portable C++ gates
pass; real ViGEm/hardware execution remains pending. See
`VIGEM_PRODUCER_FRESHNESS_REVIEW_2026-09-06.md`.

### RM-06 — Единая immutable RuntimeConfig

- [x] Статус: IMPLEMENTED_PARTIAL (characterized gate; bounded admission). Приоритет: P1. Зависимости: 03,04,05.

**Основание:** AR-02; вчерашний gate закрывает только часть переходов.

**Где работать:** H/settings.cpp; H/bindings.cpp; H/key_settings.cpp; H/profile_runtime_gate.h; H/global_profiles.cpp; H/backend.cpp.

**Зачем:** Разрозненные setters и загрузка профиля имеют разные гарантии целостности и времени чтения.

**Шаги:**

1. Перечислить все поля, влияющие на один XUSB tick, и все writers; отделить глобальные Window/Overlay от profile runtime.
2. Сравнить локальное закрытие доказанного interleaving с RuntimeConfig/snapshot slots и atomic shared_ptr с reclamation. Выбрать immutable migration только при обосновании; название карточки не предрешает rewrite.
3. Подготовку/валидацию/allocations выполнять вне realtime; тик pin-ит одну версию на всё вычисление.
4. Перевести и profile load, и обычные UI setters через одну публикацию; compatibility getters читать выбранный snapshot.
5. Удалить прежний CommitLease только после доказательства полного перевода writers; старые readers не должны смотреть в освобождённую конфигурацию.

**Отрицательная проверка / oracle:** Два поля одной команды меняются атомарно для тика при любых паузах writer; reader не ждёт и не освобождает большой объект.

**Готово, когда:** Нет смешанной конфигурации, unbounded retries и blocking locks на пути чтения config.

**Не делать:** Не ограничиваться замещением mutex на spinlock; это оставит проблему времени ожидания.

**Результат исполнителя (2026-09-06):** retained the proven prepared-profile
transaction boundary after writer/consumer inventory; it already provides
all-or-none profile settings, per-key values and bindings to `Backend_Tick`.
Read admission now retries a bounded three times when it races another reader,
without waiting behind a writer. The exact non-migration rationale, remaining
key-reader issue and regression scope are in `RUNTIME_CONFIG_RM06_REVIEW_2026-09-06.md`.
An immutable full snapshot is intentionally not claimed done: it requires a
demonstrated mixed-domain consumer beyond the protected profile transaction.

### RM-07 — Одинаковый путь для всех поддержанных keys

- [x] Статус: IMPLEMENTED (portable/static; hardware pending). Приоритет: P1. Зависимости: 06.

**Основание:** AR-03.

**Где работать:** H/key_settings.cpp:FastSnapshotLoad,KeySettings_Get; H/backend_curve.cpp:BuildCurveForHid; H/analog_key_codes.h.

**Зачем:** Диапазон >=256 использует map/shared_mutex и не участвует в том же cache.curves, что обычные keys.

**Шаги:**

1. Зафиксировать допустимый semantic domain из analog_key_codes.h; не путать USB usage, Fn semantic и mouse pseudo keys.
2. Выбрать компактную подготовленную таблицу всего домена либо только привязанных keys; посчитать стоимость памяти.
3. Одним чтением получать useUnique и описание кривой из той же RuntimeConfig.
4. Распространить cache generation на extended keys и инвалидацию при изменениях.
5. Сравнить обычный key и extended key в одинаковых сценариях, включая назначение нескольких pad controls.

**Отрицательная проверка / oracle:** Все 1033 допустимых кода сохраняются и читаются без alias/truncation; extended path не захватывает shared_mutex.

**Готово, когда:** Покрыты границы, Fn/Menu/pseudo-key semantics и измерено изменение памяти/тика.

**Не делать:** Не растягивать тяжелые LUT на весь 16-bit диапазон без расчёта.

**Результат исполнителя (2026-09-06):** all 1033 supported semantic codes now
use one fixed prepared table and lock-free atomic snapshot reader; arbitrary
16-bit values remain rejected. Fn/OEM retain their distinct codes. The curve
thread cache covers the same complete domain and reads `useUnique` plus the
curve definition from one key snapshot. Portable/static tests cover Fn/OEM,
out-of-domain rejection, profile preparation and full-domain cache wiring;
hardware timing remains pending.

### RM-08 — Чистый расчёт управления и replay

- [x] Статус: IMPLEMENTED (portable/static; hardware pending). Приоритет: P1. Зависимости: 06,07.

**Основание:** AR-06; хороший configured_xusb builder уже существует.

**Где работать:** H/configured_xusb_builder.cpp; H/backend.cpp:BuildConfiguredReportPair; H/backend_curve.cpp; H/curve_math.cpp.

**Зачем:** Один воспроизводимый core позволит проверить сложные сценарии без физического геймпада.

**Шаги:**

1. Описать вход InputSnapshot + RuntimeConfig + BuilderState и выход ControllerFrame + next state.
2. Отделить clocks, telemetry, provider reads и transport effects от вычисления.
3. Подключить существующий configured_xusb builder, а не переписывать SOCD/LKP с нуля.
4. Записать fixtures: press/release, opposition, simultaneous edges, mouse merge, profile change, reconnect.
5. Сделать replay harness, вызывающий производственные функции; сравнить прежнюю и новую реализацию до удаления adapter.

**Отрицательная проверка / oracle:** Одинаковые входы и previous state дают одинаковый выход; сброс generation не наследует lastDirection старого устройства.

**Готово, когда:** Core проверяется без Win32 input и hardware, production использует тот же код.

**Не делать:** Не переносить логику в независимую «правильную модель», которую production не вызывает.

**Результат исполнителя (2026-09-06):** verified that production already
captures bindings/settings/input/mouse outside `configured_xusb::BuildReport`
and calls this exact pure core for both qualified and shadow routes. Extended
the production-linked replay fixture with release and replacement-generation
state reset; SOCD/LKP, mouse merge, profile-shaped replacement and XUSB adapter
equivalence are now portable/static covered. Hardware execution remains pending.

### RM-09 — Общий snapshot-контракт native и UAP

- [ ] Статус: IMPLEMENTED (portable/static; Spark hardware pending). Приоритет: P1. Зависимости: 00,08.

**Результат 2026-09-06:** common V2 adapter и opt-in registry endpoint добавлены
без изменения legacy `ReadMilli`/arbitration. SparkLink — первый pilot: exact HID
interface fingerprint даёт separate device identity; legacy [0..1000] явно
маркируется `LegacyQuantized`; row-polled snapshot намеренно partial и экспортирует
только доказанно fresh rows. См.
[NATIVE_UAP_SNAPSHOT_RM09_REVIEW_2026-09-06.md](NATIVE_UAP_SNAPSHOT_RM09_REVIEW_2026-09-06.md).
Portable oracle покрывает equal usage/different interface и disconnect одного
источника без стирания второго. Backup:
`.local/backups/rm09_native_snapshot_pre_20260906_160000` (SHA256 manifest
`0CB1107329635D9ECD0AD4A19527F8FA85FB8B80AE27F65F7919E2B9B3042835`).
Hardware transport/timing и actual downstream arbitration остаются соответственно
RM-09 hardware gate и RM-11; production output route не менялся.

**Основание:** Подтверждено: NativeAnalogReadResult содержит только milli/owned/connected.

**Где работать:** H/native_analog_backend.h; H/native_analog_backend_registry.cpp:NativeAnalogBackends_ReadMilli; H/analog_provider_v2.h; P/halljoy_analog_provider_v2_contract.h.

**Зачем:** Native per-key max агрегация теряет источник, время, полноту и сырую шкалу; UAP уже имеет более богатый snapshot.

**Шаги:**

1. Сопоставить поля V2 и native ABI: identity, topology, sample generation, raw/domain, ownership, freshness, capacity.
2. Определить adapter native provider -> общий snapshot без выдуманной точности для старого milli backend.
3. Публиковать согласованную generation после пакета/доказанной группы строк; partial validity должна быть явной.
4. Перенести выбор источника в отдельный arbitration, перестать терять provenance при раннем max. При подтверждённой цене обхода подготовить таблицу ownership keys->sources вне realtime, а не сканировать весь каталог и карту SparkLink для каждого key.
5. Перевести один доказанный native backend, квалифицировать, затем повторять по семействам RM-30.

**Отрицательная проверка / oracle:** Два одинаковых usage от разных устройств сохраняют разную identity; disconnect одного не стирает свежий другой.

**Готово, когда:** Единый контракт применён к выбранному backend, legacy-adapter имеет явные ограничения.

**Не делать:** Не заставлять stream-протокол ждать полного скана, если отдельные samples имеют доказанную независимость.

**Результат исполнителя:** adapter/registry/Spark pilot реализованы; portable/static
gates PASS, hardware qualification pending.

### RM-10 — Сохранить точность до границы выходного формата

- [ ] Статус: NEEDS_EVIDENCE (Spark raw domain). Приоритет: P1. Зависимости: 09.

**Результат review 2026-09-06:** `routeRaw` в SparkLink имеет 16-bit transport
форму, но current denominator — эвристический observed max 3500..5000, не
доказанная firmware шкала. RM-09 честно маркирует существующий путь как
`LegacyQuantized`; протаскивать `routeRaw` как V2 raw/domain сейчас означало бы
сфабриковать точность. Нужны packet/firmware/hardware evidence; см.
[NUMERICAL_PRECISION_RM10_REVIEW_2026-09-06.md](NUMERICAL_PRECISION_RM10_REVIEW_2026-09-06.md).

**Основание:** Подтверждено раннее milli API; стоимость потери зависит от протокола.

**Где работать:** H/native_analog_backend.h; H/analog_provider_v2.cpp; H/backend_sparklink.inc; H/backend_sayo.inc; H/configured_xusb_builder.cpp.

**Зачем:** Квантизация 0..1000 до кривой и отсечения малых значений могут терять ранний ход.

**Шаги:**

1. Для каждого семейства заполнить raw min/max, scale, signedness, нулевой уровень и доказательство.
2. Протащить raw/domain или эквивалентную достаточную точность через snapshot до преобразования.
3. Обозначить compatibility milli источники явно, не восстанавливать несуществующие биты.
4. Проверить NaN/Inf, raw выше domain, нулевой denominator и округление осей/trigger.
5. Сравнить ступени near-zero и output equality на ранее допустимых значениях; намеренные изменения записать.

**Отрицательная проверка / oracle:** Слабое аналоговое нажатие не исчезает из-за промежуточного необоснованного порога; invalid числа не доходят до lround.

**Готово, когда:** Есть end-to-end numerical budget и тесты границ для каждого применённого adapter.

**Не делать:** Не обещать дополнительную физическую точность клавиатуре, которая её не отдаёт.

**Результат исполнителя:** NEEDS_EVIDENCE; legacy adapter limitation documented,
source math intentionally unchanged.

### RM-11 — Явное объединение нескольких источников

- [ ] Статус: IMPLEMENTED (portable/static; hardware pending). Приоритет: P1. Зависимости: 08,09.

**Результат 2026-09-06:** `Arbitrate` — чистая production-used функция с
явными availability/ownership/freshness state и source mask. Она сохраняет
характеризованную policy standard max, native extended authority и owned-zero
block digital fallback; mouse остаётся отдельным pseudo-source. См.
[ARBITRATION_RM11_REVIEW_2026-09-06.md](ARBITRATION_RM11_REVIEW_2026-09-06.md).
Legacy native catalog per-key max намеренно не выдаётся за provenance recovery:
RM-09 V2 endpoint сохраняет identity, его consumer promotion остаётся отдельной
integration/qualification работой. Backup:
`.local/backups/rm11_arbitration_pre_20260906_162000` (SHA256 manifest
`543034B141C2099E14818D1B792D81902F6147B3CAC567208B3A8E36060C7805`).

**Основание:** AR-04 и прежний P1-014; текущая политика требует characterization.

**Где работать:** H/backend.cpp:ReadRaw01Cached; H/provider_v2_controller_shadow.cpp; H/native_analog_backend_registry.cpp.

**Зачем:** connected, owned-zero, unsupported и stale — разные состояния; max без этих различий может скрывать потерю релиза.

**Шаги:**

1. Выписать текущую таблицу решений native/UAP/digital/mouse, включая allowFallback и extended keys.
2. Задать контракт owned zero, disconnect, stale, duplicate source и одно устройство в двух маршрутах.
3. Реализовать чистую функцию arbitration с metadata, независимую от порядка обхода каталога.
4. Сохранять digital fallback только как явно выбранный источник; он не определяет analog identity.
5. Проверить два устройства, одинаковые usage, одно исчезновение, hotplug, native exact-path exclusion.

**Отрицательная проверка / oracle:** Нулевой валидный analog не заменяется цифровым full press; старый источник не удерживает ненулевое значение.

**Готово, когда:** Таблица решений покрыта тестами и совпадает с UI semantics.

**Не делать:** Не менять привычную политику max/sum без отдельного описания пользовательского эффекта.

**Результат исполнителя:** pure policy + qualified/shadow integration и
portable/static gates PASS; physical concurrent-device/hotplug PASS pending.

### RM-12 — Завершить Provider V2 и убрать постоянный shadow

- [ ] Статус: NEEDS_EVIDENCE (qualification removal gate). Приоритет: P1. Зависимости: 08,09,11.

**Результат review 2026-09-06:** dedicated fail-closed qualification variant
уже проверяет same-transaction capture, capacity, coverage, mismatch, curve и
fallback, но текущему worktree не предоставлен finalized real-device `PASS`
artifact. Удалять dense/shadow по portable tests запрещено собственным removal
gate; обычный route не изменён. См.
[PROVIDER_V2_RM12_REMOVAL_REVIEW_2026-09-06.md](PROVIDER_V2_RM12_REMOVAL_REVIEW_2026-09-06.md).

**Основание:** AR-04, переход частично реализован.

**Где работать:** H/backend.cpp:BuildConfiguredReportPair; H/uap_parent_snapshot.cpp; H/provider_v2_snapshot_broker.cpp; H/provider_v2_qualification_model.cpp; P/main.cpp.

**Зачем:** Сейчас одновременно существуют dense capture, V2 lease, fallback и второй BuildReport.

**Шаги:**

1. Зафиксировать список потребителей legacy API и причины его сохранения.
2. Проверить same-transaction capture, capacity renegotiation, topology и lease lifetime под fault injection.
3. Разделить отсутствие V2 поддержки и испорченный/устаревший V2; второй случай не скрывать успешным legacy чтением.
4. После qualification и измерения цены решить, нужен ли shadow в ordinary path. Переносить/удалять только при закрытом removal gate; сохранить допустимое diagnostic/compatibility применение и consumers.
5. Удалить ненужные копии/reads только после A/B core equality и hardware evidence; обновить removal ledger.

**Отрицательная проверка / oracle:** Несовпадающие поколения не склеиваются; при отключённом shadow production строит один требуемый отчёт.

**Готово, когда:** Нет ненужного постоянного двойного вычисления на переведённых маршрутах; fallback scope документирован.

**Не делать:** Не выключать проверки до доказательства V2 только ради уменьшения CPU.

**Результат исполнителя:** NEEDS_EVIDENCE; qualification infrastructure reviewed,
no shadow/dense removal without real PASS artifact.

### RM-13 — Жизненный цикл UAP: reconnect и invalidation

- [ ] Статус: IMPLEMENTED (static/simulator coverage; physical pending). Приоритет: P1. Зависимости: 01,09.

**Результат review 2026-09-06:** current supervisor already owns coalesced
`WM_DEVICECHANGE` replacement, snapshot invalidation, confirmed child reap and
generation-bound V2 plane retirement; broad UAP hotplug scan remains disabled.
Static/simulator fault gates cover parent/child fault, timeout, startup rollback
and no-overlap behavior. См.
[UAP_RECONNECT_RM13_REVIEW_2026-09-06.md](UAP_RECONNECT_RM13_REVIEW_2026-09-06.md).

**Основание:** Смешанная новая/legacy инфраструктура; ревизия по текущему коду.

**Где работать:** H/analog_host_client.cpp; H/provider_v2_snapshot_broker.cpp; P/main.cpp; P/overlay/Soup/soup/hwHid.cpp.

**Зачем:** В UAP есть собственный supervisor и bridge; исправленный reconnect нельзя заменить старым broad scan.

**Шаги:**

1. Нарисовать ownership mapping/events/threads/process/job на одну generation и точки публикации disconnected.
2. Проверить crash, exception, unload timeout, потерю snapshot event и parent stop в каждой фазе startup.
3. Сверить поведение UAP_DISABLE_HOTPLUG=1 с точечным RequestDeviceRefresh; сохранить доказанное восстановление.
4. Проверить pin/reap живых readers до освобождения device vectors и mappings.
5. Сравнить использование общего process_generation_supervisor с текущим UAP owner; переиспользовать только после доказательства эквивалентных гарантий.

**Отрицательная проверка / oracle:** Старый child не публикует в новую generation; перезапуск после отказа не оставляет ненулевые samples.

**Готово, когда:** Есть таблица переходов и production-linked tests; physical reconnect отдельно.

**Не делать:** Не возвращать периодический широкий опрос всех HID как универсальный reconnect fix.

**Результат исполнителя:** REVIEWED_NO_REWRITE; existing implementation/static
and simulator evidence mapped, physical UAP reconnect pending.

### RM-14 — Ограничить последствия зависшего native HID

- [ ] Статус: IMPLEMENTED_PARTIAL (fail-closed retained-resource boundary). Приоритет: P1. Зависимости: 01,09.

**Результат review 2026-09-06:** active native providers already cancel
overlapped I/O, neutralize publication, use bounded join and poison/restart-block
on an unconfirmed worker; resource/`OVERLAPPED` lifetime remains retained rather
than unsafely freed. Addressed `reader.join` is intentional safety, not a
candidate for detach. No per-provider child transport is added without a
reproduced threat/evidence; см.
[NATIVE_HID_CONTAINMENT_RM14_REVIEW_2026-09-06.md](NATIVE_HID_CONTAINMENT_RM14_REVIEW_2026-09-06.md).

**Основание:** Подтверждено: native I/O в процессе; Addressed сохраняет worker и ждёт reader.join.

**Где работать:** H/addressed_analog_backend.cpp; H/native_analog_backend_registry.cpp; H/worker_join_policy.h; H/process_generation_supervisor.cpp; H/app.cpp:AppShutdownNoThrow.

**Зачем:** Текущая защита сохраняет lifetime при зависшем cancellation, но граница отказа может оставаться всей программой.

**Шаги:**

1. Для каждого native provider перечислить blocking calls, cancellation owner и условия safe-to-free.
2. Проверить уже существующие timeout/poison guards; не удалять их как «лишние».
3. Сравнить локальную overlapped-I/O коррекцию, отдельный process transport и полный provider host; выбрать границу по доказанной угрозе.
4. Сделать один вертикальный transport adapter в child при необходимости; использовать generation/job ownership и bounded reap.
5. Повторить pending I/O, timeout, exception, stop-before-start и unplug; не допускать старого reader после нового session.

**Отрицательная проверка / oracle:** Зависший fake HID не требует освобождения используемого OVERLAPPED и не отравляет unrelated providers.

**Готово, когда:** Есть доказанная граница отказа и zero-survivor тест либо честное retained-resource + fail-closed завершение.

**Не делать:** Не применять TerminateThread и не detach-ить поток с указателями на уничтожаемые объекты.

**Результат исполнителя:** existing fail-closed containment characterized;
vertical process adapter/hardware pending-I/O proof remains evidence-gated.

### RM-15 — Runtime supervisor независимо от UI

- [x] Статус: IMPLEMENTED_PARTIAL (static lifecycle coverage; physical recovery pending). Приоритет: P1. Зависимости: 05,13,14.

**Основание:** AR-05/06; watchdog сейчас вызывается из UI timer.

**Где работать:** H/app.cpp; H/backend.cpp; H/realtime_loop.cpp; H/vigem_output_runtime.cpp; H/worker_lifecycle.h.

**Зачем:** Занятый UI не должен быть единственным местом проверки и восстановления движка.

**Шаги:**

1. Составить таблицу owners: UI/runtime/provider/output, кто вызывает Start/Stop и кто читает status.
2. Ввести один runtime command owner для start, recover, pause и stop; команды generation-bound и идемпотентны.
3. Проверять именно progress/freshness и состояние ресурсов; не перезапускать healthy worker по одному missed timer.
4. Применить единый shutdown deadline budget: суммирование локальных timeout не должно неожиданно превышать watchdog.
5. UI получает snapshot статуса и отправляет команды, но не держит lifecycle locks при MessageBox/SendMessage.

**Отрицательная проверка / oracle:** UI заморожен, output fault обнаруживается; повтор stop/recover не создаёт overlapping generations.

**Готово, когда:** State transitions проверены, recovery bounded, общая цена shutdown задокументирована.

**Не делать:** Не заменять один UI-монолит вторым бесконтрольным набором supervisor threads.

**Результат исполнителя:** added one bounded `runtime_supervisor` after the
startup transaction. It is the sole periodic owner of realtime/output recovery;
UI retains presentation only. Startup rollback and shutdown stop it before all
recovered dependencies; incomplete join poisons and retains resources. Static
oracle is `runtime_supervisor_static_audit.py`. Actual UI-free recovery and
output-fault proof remain physical/runtime evidence gates. Pause/resume command
ownership is intentionally deferred to RM-16 because no safe global pause
contract existed.

### RM-16 — Полная Pause/Resume с освобождением устройства

- [ ] Статус: TODO. Приоритет: P1. Зависимости: 15.

**Основание:** Историческое требование; просмотренный API не показывает общего engine pause.

**Где работать:** H/app.cpp; H/backend.cpp; H/native_analog_routing.cpp; H/mouse_ipc.cpp; docs/v1.4/GLOBAL_PAUSE_DEVICE_LEASE_AUDIT.md.

**Зачем:** Отключение мышиного hook или остановка отчётов не равно освобождению vendor HID для web-драйвера.

**Шаги:**

1. Перепроверить весь UI и API на существующий общий pause, чтобы не создать второй механизм.
2. Реализовать последовательность: запрет opens -> neutral snapshots/output -> release hooks -> stop providers -> release HID/leases -> paused.
3. Успех Pause показывать только после подтверждения всех применимых ресурсов; при неполном stop показывать причину.
4. Resume выполняет новую proof/generation, очищает прежние данные и не стартует сам из watchdog.
5. Проверить pause на startup, удержанных keys, busy write, уже отсоединённом устройстве и при повторном клике.

**Отрицательная проверка / oracle:** Во время paused нет vendor handles и виртуальных targets, ввод не блокируется; resume не возвращает старое нажатие.

**Готово, когда:** Пользовательская семантика совпадает с фактическим ownership; physical web-driver coexistence проверяется позже.

**Не делать:** Не угадывать активность web-драйвера по имени браузера или окну.

**Результат исполнителя:** пока не заполнен; использовать шаблон раздела 12.

### RM-17 — Защита конфликтующего владения экземпляров

- [ ] Статус: TODO. Приоритет: P1. Зависимости: 15,16.

**Основание:** Поиск main/app не нашёл общего instance mutex; требуется полный cross-check.

**Где работать:** H/main.cpp; H/app.cpp; H/native_analog_routing.cpp; H/drunkdeer_backend.cpp; H/mouse_ipc.cpp.

**Зачем:** Два экземпляра могут конкурировать за настройки, output и протокольные сессии; отдельный DrunkDeerMtx не решает общий случай.

**Шаги:**

1. Проверить все startup роли, named objects и уже существующие ограничения на второй процесс.
2. Определить scope instance: Windows session/user и явные test roots; child roles не должны попадать под запрет parent.
3. Защитить конфликтующее владение до provider open и записи одного root. Глобальный single-instance допустим только при подтверждённом продуктовом контракте; независимые устройства/roots не запрещать автоматически.
4. Разделить общий instance guard и per-device protocol lease; abandoned lease не означает готовое устройство.
5. Проверить одновременный старт, crash первого, diagnostic рядом с ordinary и предсозданный named object.

**Отрицательная проверка / oracle:** Два parent startup не получают одновременно право сохранять один root и владеть одним device session.

**Готово, когда:** Instance policy явная и проверяется process-only harness без HID.

**Не делать:** Не убивать все HallJoy.exe по имени и не менять известный внешний protocol mutex несовместимо.

**Результат исполнителя:** пока не заполнен; использовать шаблон раздела 12.

### RM-18 — Один последовательный владелец сохранений

- [ ] Статус: TODO. Приоритет: P1. Зависимости: 06,15.

**Основание:** AR-05, синхронные полные записи из UI.

**Где работать:** H/global_profiles.cpp; H/settings_ini.cpp; H/ini_util.cpp; H/keyboard_ui.cpp; H/keyboard_bind_panel.cpp.

**Зачем:** Асинхронность полезна только вместе с versions, error propagation и сохранением последних изменений.

**Шаги:**

1. Перечислить все save callsites, включая WM_DESTROY, debounce/timer, bindings panel и overlay.
2. Сначала измерить цену sync save и зафиксировать ожидаемую durability. Только при выбранном async-варианте ввести immutable save job (profile identity + generation + destination); worker не читает изменяемые getters. Совместимый sync fix или REVIEWED_NO_CHANGE допустимы.
3. Объединять лишь не начатые записи той же destination, сохраняя порядок между переключениями профилей.
4. Добавить applied/dirty/saving/saved/failed status и барьеры перед switch/delete/exit.
5. Сохранить атомарную замену и legacy migration backup; удалить обходные writers после перевода всех callsites.

**Отрицательная проверка / oracle:** Отложенный save старого профиля не перезаписывает новый; отказ записи не выдаётся за saved.

**Готово, когда:** Один writer, проверенные barriers и ограниченное завершение очереди.

**Не делать:** Не делать fire-and-forget detached writer; не держать config lease во время disk I/O.

**Результат исполнителя:** пока не заполнен; использовать шаблон раздела 12.

### RM-19 — Довести migration, recovery и factory reset

- [ ] Статус: TODO. Приоритет: P1. Зависимости: 01,18.

**Основание:** Профильный fix проверен; полный portable runtime остался непроверенным.

**Где работать:** H/app_paths.cpp; H/factory_reset.cpp; H/settings_ini.cpp; H/global_profiles.cpp; tools/run_storage_migration_test.ps1.

**Зачем:** Смена формата и multi-file reset требуют восстановления после аварии на каждом шаге, а не только atomic write одного файла.

**Шаги:**

1. Разобрать сохранённый portable failure на копии данных; отличить повреждённый fixture от легального старого формата.
2. Перенести portable test в новый каталог с копией EXE/marker; не использовать состояние рабочего simulator output.
3. Зафиксировать правила .pre-bundle.bak, существующего backup, legacy bindings и rollback на старый EXE.
4. Для migration/reset прервать процесс после каждого copy/move/marker шага; следующий запуск должен продолжить или безопасно восстановиться.
5. Проверить Unicode, denied access, disk full, reparse directories, collision и удаление активного профиля.

**Отрицательная проверка / oracle:** После каждого прерывания есть исходные данные или проверяемая восстановимая версия; чужие файлы неизменны.

**Готово, когда:** File-only recovery matrix полная, отдельный portable runtime gate остаётся честно pending до исполнения.

**Не делать:** Не «чинить» fixture перезаписью пользовательских настроек; не удалять backup до доказанного восстановления.

**Результат исполнителя:** пока не заполнен; использовать шаблон раздела 12.

### RM-20 — Единый контракт внешних чисел и INI

- [ ] Статус: TODO. Приоритет: P1. Зависимости: 00,19.

**Основание:** Требуется расширить проверку за пределы вчерашних loader fixes.

**Где работать:** H/bounded_ini.h; H/profile_ini.cpp; H/settings_ini.cpp; H/keyboard_layout.cpp; H/keyboard_profiles.cpp; H/file_name_policy.cpp.

**Зачем:** Проверка Count и CSV не доказывает все section/key, numeric и filename parsers.

**Шаги:**

1. Составить таблицу каждого внешнего поля: тип, диапазон, default, required/version, maximum length и invalid policy.
2. Проверить полный файл/section/key budgets до allocations; различить отсутствующий и malformed параметр.
3. Добавить корпус overflow, signs, junk suffix, embedded NUL, BOM/encoding, duplicate keys, huge sections и unknown schema.
4. Проверить что подготовка всегда без runtime mutation, а writer+validator используют согласованную схему.
5. Где есть расхождение — переиспользовать общий строгий parser; не мигрировать формат без необходимости.

**Отрицательная проверка / oracle:** Невалидный параметр не alias-ится в допустимый, не уничтожает текущую конфигурацию и не вызывает unbounded allocation.

**Готово, когда:** Каждый persisted field имеет документированный roundtrip и error behavior.

**Не делать:** Не считать clamp универсальной валидацией и не отвергать допустимые legacy файлы без migration policy.

**Результат исполнителя:** пока не заполнен; использовать шаблон раздела 12.

### RM-21 — Математика кривых и конечных XUSB значений

- [ ] Статус: TODO. Приоритет: P2. Зависимости: 08,10.

**Основание:** Проверка; наличие binary search само по себе не дефект.

**Где работать:** H/curve_math.cpp; H/backend_curve.cpp; H/configured_xusb_builder.cpp; H/keyboard_keysettings_panel_graph.cpp.

**Зачем:** UI-график, вычисление кривой и фактический stick должны соответствовать друг другу.

**Шаги:**

1. Сверить формулы UI/runtime, значения weights, monotonic X и правила нормализации.
2. Проверить точки 0/1, low==high, близкие контрольные точки, крайние weights, NaN/Inf и допустимую немонотонность Y.
3. Зафиксировать погрешность rational inversion и rounding до XUSB; сравнить с независимым высокоточным reference только как oracle.
4. Профилировать pow/итерации; при существенной цене готовить coefficients/LUT вне realtime с явным error budget.
5. Сверить SOCD/LKP/hysteresis, mouse merge и изменение профиля со старыми fixtures.

**Отрицательная проверка / oracle:** Кривая конечна и соответствует разрешённой форме; UI и XUSB не расходятся больше установленной погрешности.

**Готово, когда:** Есть численный error budget и behavioral tests; оптимизация только при измеримой пользе.

**Не делать:** Не менять пороги и округление в процессе косметического рефакторинга.

**Результат исполнителя:** пока не заполнен; использовать шаблон раздела 12.

### RM-22 — Планирование realtime и стоимость пробуждений

- [ ] Статус: TODO. Приоритет: P2. Зависимости: 05,08,12.

**Основание:** Текущий event/deadline scheduler требует измерений, не переписывания наугад.

**Где работать:** H/realtime_loop.cpp; H/input_wake_sequence.h; H/vigem_output_scheduler.h; H/backend.cpp.

**Зачем:** Средний Hz не показывает задержку релиза, потерянный wake и p99/max под нагрузкой.

**Шаги:**

1. Зафиксировать входной generation, wake sequence, tick begin/end, publish и apply timestamps без синхронного логирования на тик.
2. Проверить lost/coalesced wakes и update прямо между чтением sequence и wait.
3. Сравнить idle, burst, 1/несколько устройств и 1/4 pad; измерять CPU, wake count и p50/p95/p99/max.
4. Проверить sleep/resume, смену частоты и limiter semantics; runtime timestamps должны быть монотонны.
5. Оптимизировать только установленный bottleneck, сохранив fixed output/report semantic.

**Отрицательная проверка / oracle:** Обновление в окне wait не теряется; pending output просыпается к deadline; idle не превращается в busy loop.

**Готово, когда:** Есть baseline и A/B на одинаковом workload, значения Hz не выдаются за latency.

**Не делать:** Не повышать thread priority и не крутить spinloop как первое средство ускорения.

**Результат исполнителя:** пока не заполнен; использовать шаблон раздела 12.

### RM-23 — Доверенность внутреннего analog-host и DLL

- [ ] Статус: TODO. Приоритет: P1. Зависимости: 00,01.

**Основание:** Подтверждено: command-line plugin path передаётся в LoadLibraryW; owner PID/nonce проверяются.

**Где работать:** H/analog_host_client.cpp:ParseHostCommand,RunHostImpl,LoadHostApi; H/embedded_analog_stack.cpp; H/main.cpp.

**Зачем:** PID/nonce в предоставленных объектах не равны доказательству доверенного image и нужного embedded plugin. Parent extraction и child validation — разные границы.

**Шаги:**

1. Перечитать полный путь extraction->spawn->child->LoadLibrary; установить, какие exact-image проверки уже существуют.
2. Составить threat boundary для same-user spoof, замены writable DLL и dependency search; не называть это доказанным повышением привилегий.
3. Проверить plugin identity против собственного embedded resource в child, удерживая файл от замены до загрузки.
4. Проверить owner executable identity и ограниченный список inherited handles; unknown internal role/options fail closed.
5. Добавить isolated negative tests на чужой plugin, испорченную DLL, неверный owner/nonce и замену между check/use.

**Отрицательная проверка / oracle:** Внутренняя роль отказывается загружать DLL, не соответствующую ожидаемому embedded содержимому.

**Готово, когда:** Identity проверяется в точке доверия; load/search policy документирована.

**Не делать:** Не отключать legitimate UAP isolation и не считать process isolation полноценной security sandbox.

**Результат исполнителя:** пока не заполнен; использовать шаблон раздела 12.

### RM-24 — Проверить все IPC и lifetime leases

- [ ] Статус: TODO. Приоритет: P1. Зависимости: 00,01.

**Основание:** Проверка; output slots/broker уже имеют хорошие гарантии.

**Где работать:** H/mouse_ipc.cpp; H/provider_v2_data_plane_windows.cpp; H/provider_v2_snapshot_broker.cpp; H/vigem_output_channel.cpp; H/latest_value_mailbox.h.

**Зачем:** ABI version, session identity, согласованность снимка и lifetime должны проверяться отдельно.

**Шаги:**

1. Для каждого mapping/event записать creator, readers/writers, ACL/naming, size/version, generation и close owner.
2. Проверить single-writer assumption и multi-scalar consistency mouse mapping; сохранить старый ASI ABI либо ввести явную новую версию.
3. Проверить oversized capacities, multiplication overflow, mapping too small, reader during resize и stale slots.
4. Прогнать contention/drop semantics mailbox/broker: NoFreeSlot не должен превращать старые значения в бесконечно свежие.
5. Проверить owner crash, предсозданный named mapping и second instance совместно с RM-17.

**Отрицательная проверка / oracle:** Неверная schema/size/generation отвергается до доступа к payload; data reader не видит освобождённую память.

**Готово, когда:** По каждому IPC заполнена ownership таблица и fault evidence.

**Не делать:** Не заменять корректные slot leases на plain-data seqlock с data race.

**Результат исполнителя:** пока не заполнен; использовать шаблон раздела 12.

### RM-25 — Overlay HTTP: доступ, границы и доступность

- [ ] Статус: TODO. Приоритет: P1. Зависимости: 01,15.

**Основание:** Проверка; loopback/origin/session и timeout уже реализованы.

**Где работать:** H/overlay_server.cpp; tools/check_overlay_http_framing.py; tools/check_overlay_concurrency_origin.py; tools/check_overlay_responsiveness.py; tools/fuzz_overlay_http.py.

**Зачем:** Localhost сервер тоже получает недоверенные запросы и не должен тормозить engine.

**Шаги:**

1. Перечислить endpoints, чтение/запись состояния, Host/Origin/session и лимиты request/body/keepalive.
2. Перепроверить conflicting Content-Length, Transfer-Encoding, pipelining, UTF/escaping, oversized input и slow client.
3. Проверить максимальное число clients/workers, timeouts на send/receive и shutdown при зависшем клиенте.
4. Проверить согласованность state JSON без длительных locks с realtime и корректность JS cache disposal.
5. Провести server-only harness с fake telemetry; полноценный smoke EXE считать output-capable.

**Отрицательная проверка / oracle:** Недопустимый origin/request отклоняется; медленные клиенты не удерживают shutdown бесконечно и не блокируют ticks.

**Готово, когда:** Endpoint/security/resource matrix закрыта; leak/perf проверены отдельно.

**Не делать:** Не снимать origin/session ради совместимости и не открывать bind на 0.0.0.0.

**Результат исполнителя:** пока не заполнен; использовать шаблон раздела 12.

### RM-26 — Raw Input, hooks и mouse integration

- [ ] Статус: TODO. Приоритет: P1. Зависимости: 08,15,16,24.

**Основание:** Проверка совместимости и освобождения; typed-size checks уже есть.

**Где работать:** H/app.cpp:WM_INPUT,KeyboardBlockHookProc; H/backend.cpp; H/mouse_ipc.cpp; H/mouse_bind_codes.h.

**Зачем:** Должны сохраняться обычный ввод, escape-путь из блокировки и очистка accumulated deltas при смене состояния.

**Шаги:**

1. Составить перечень hook/capture owners и условий blocking; сопоставить pseudo keys и raw devices.
2. Проверить packet bounds, RIM_INPUT cleanup, absolute/relative mouse, large deltas и malformed sizes без глобального SendInput.
3. Проверить pause, focus loss, device removal, lock/unlock session, startup failure и exception cleanup.
4. Сделать fake adapter для событий; осмотреть оригинальные return/CallNextHookEx и unhook paths.
5. Проверить mouse IPC disconnect/heartbeat и чтобы новый session не получил накопленное старое движение.

**Отрицательная проверка / oracle:** После pause/failed start/shutdown ввод не остаётся заблокированным; old deltas не оживают после resume.

**Готово, когда:** Есть cleanup matrix и targeted manual check позже, не во время игры.

**Не делать:** Не отправлять тестовые глобальные клавиши; не подменять measured depth цифровым значением. Автоматическое сопоставление букв Sayo разрешено и регулируется RM-03.

**Результат исполнителя:** пока не заполнен; использовать шаблон раздела 12.

### RM-27 — UI: состояние, DPI, ресурсы и большие файлы

- [ ] Статус: TODO. Приоритет: P2. Зависимости: 06,18,26.

**Основание:** AR-06; места GDI allocations просмотрены, утечка не доказана.

**Где работать:** H/keyboard_subpages.cpp; H/keyboard_render.cpp; H/custom_page_surface.cpp; H/premium_combo_core.cpp; H/remap_panel.cpp.

**Зачем:** Сложные страницы смешивают редактирование, хранение состояния, timers и рендеринг.

**Шаги:**

1. Разделить модель черновика, runtime apply, persistence status и painting; сначала одна профильная страница.
2. Составить таблицу HWND/state/font/brush/bitmap owners; проверить WM_NCDESTROY и restore selected object.
3. Проверить DPI/resize/multi-monitor, длинные Unicode labels, tab navigation и invalid persisted geometry.
4. Измерить GDI/user handles и allocations после повторного открытия страниц; кешировать лишь доказанно дорогие ресурсы.
5. Вынести завершённые компоненты в файлы после поведенческого characterization; не менять внешний UI попутно.

**Отрицательная проверка / oracle:** Закрытие/повторное создание страницы не наращивает ресурсы; DPI change не оставляет невалидные размеры и указатели.

**Готово, когда:** Есть ресурсная baseline и проверка взаимодействия UI с config/saves.

**Не делать:** Не переписывать интерфейс на новый framework только из-за размера файла.

**Результат исполнителя:** пока не заполнен; использовать шаблон раздела 12.

### RM-28 — Наблюдаемость без постоянной тяжёлой записи

- [ ] Статус: TODO. Приоритет: P1. Зависимости: 05,15,23.

**Основание:** Проверка; redaction и production log exclusion уже частично существуют.

**Где работать:** H/debug_log.cpp; H/stability_trace.cpp; H/main.cpp; tools/analyze_stability_trace.py.

**Зачем:** Для следующего отвала нужен понятный первичный failure, но логирование не должно менять latency и раскрывать лишние данные.

**Шаги:**

1. Разделить crash-only production, bounded in-memory diagnostics и explicit support capture.
2. Для всех lifecycle transitions задать component/generation/first-error/checkpoint; не затирать первопричину последующими cleanup errors.
3. Проверить scopes redaction по build defines, raw paths/serial/usernames/input sequences и все sinks.
4. Проверить ограничения объёма/rotation, disk-full и lock contention; telemetry нельзя писать синхронно из критического тика.
5. Сделать analyzer, выдающий INPUT_STALE/PRODUCER_STALLED/OUTPUT_FAILED/STORAGE_FAILED/INCOMPLETE на сохранённых fixtures.

**Отрицательная проверка / oracle:** Искусственный первичный отказ остаётся главным verdict; private path fixture не попадает в support log.

**Готово, когда:** Диагностический контракт проверяется автоматически, overhead измерен.

**Не делать:** Не включать постоянный подробный лог в ordinary build как универсальное лечение.

**Результат исполнителя:** пока не заполнен; использовать шаблон раздела 12.

### RM-29 — Единая граница release/experimental/diagnostic

- [ ] Статус: TODO. Приоритет: P1. Зависимости: 00.

**Основание:** AR-07; Hero84 находится в обычном каталоге.

**Где работать:** H/native_analog_backends.def; H/HallJoy.vcxproj; SUPPORTED_HARDWARE.md; docs/v1.4/RELEASE_READINESS_AUDIT_2026-09-05.md.

**Зачем:** Разные #if и раздельная документация делают состав релиза неочевидным.

**Шаги:**

1. Составить registry device family -> proof status -> build tier -> public claim -> owning evidence.
2. По Hero84 найти existing owner evidence и intended support. При нехватке evidence оставить qualification pending; изменение ordinary support требует явного scope decision. Не удалять claimant самостоятельно под видом архитектурного cleanup.
3. Генерировать или сверять catalog/build defines/support rows из единого manifest.
4. Проверить каждую diagnostic сборку на отсутствие unrelated claimants; бинарный output hash привязать к tier.
5. Проверить dependencies и embedded resources для обычной/diagnostic конфигурации.

**Отрицательная проверка / oracle:** Experimental-only descriptor не может случайно войти в ordinary catalog; поддержка не расширяется строкой в README.

**Готово, когда:** Manifest соответствует скомпилированному составу и scope evidence.

**Не делать:** Не объявлять модель поддержанной только по VID/PID или общей марке.

**Результат исполнителя:** пока не заполнен; использовать шаблон раздела 12.

### RM-30 — Поштучный protocol audit всех enabled families

- [ ] Статус: TODO. Приоритет: P1. Зависимости: 01,03,04,09,10,11,14,29.

**Основание:** Часть исторических находок могла быть исправлена; все требуют текущей сверки.

**Где работать:** H/*_protocol.cpp; H/*_backend.cpp; H/backend_sparklink.inc; H/backend_sayo.inc; P/main.cpp; P/overlay/Soup/soup/AnalogueKeyboard.cpp; P/overlay/Soup/soup/hwHid.cpp.

**Зачем:** Общие архитектурные улучшения не доказывают правильность каждой firmware/layout.

**Шаги:**

1. Исполнять отдельную карточку семейства из раздела Protocol matrix этого файла; не объединять все firmware в один PASS.
2. Для каждого write/read выписать endpoint, report ID/length, opcode, correlation, endian, count/offset, scale, map и источник доказательства.
3. Запустить production parser на valid/short/duplicate/reordered/wrong-row/out-of-range корпусе; transport/session отдельно.
4. Проверить partial freshness, lost release, reconnect, multiple devices, resource ownership и недопустимые команды.
5. При отсутствии proof держать NEEDS_EVIDENCE, искать existing evidence и не угадывать по похожему устройству. Новую stable qualification не заявлять; исключение существующего route — отдельное явное release-scope решение, не автоматический побочный эффект аудита.

**Отрицательная проверка / oracle:** Подмена index/layout/scale или потеря одного chunk приводит к отказу/neutral по контракту, а не корректно выглядящему чужому key.

**Готово, когда:** По каждой включённой family есть parser + session + XUSB + физические gates; недоступное железо честно pending.

**Не делать:** Никаких flash/calibration/remap writes для аудита; только доказанный разрешённый протокол.

**Результат исполнителя:** пока не заполнен; использовать шаблон раздела 12.

### RM-31 — Поведенческие тесты, sanitizer и доказательство самого harness

- [ ] Статус: TODO. Приоритет: P1. Зависимости: 01,08,30.

**Основание:** AR-08; sanitizer runner сейчас явно линкует три protocol cpp.

**Где работать:** tools/run_native_backend_checks.py; tools/run_protocol_fuzz_sanitizers.py; tools/run_aula_win60he_sanitizers.py; T/*.

**Зачем:** Число PASS не показывает покрытие: 49 собранных тестов, 54 test.cpp и 79 audit scripts — разные множества.

**Шаги:**

1. Построить matrix production function -> test -> fixture -> effect class -> build configuration; отметить unlinked test.cpp.
2. Для каждого изменяемого инварианта иметь old-bug oracle либо воспроизводимый сценарий проверки риска.
3. Добавить sanitizer health control, намеренно обнаруживаемый инструментом; отличить отсутствие runtime от PASS.
4. Расширить parser/fault coverage по enabled families; Windows concurrency проверять доступными корректными инструментами, не заявлять TSan PASS без запуска.
5. Сократить хрупкие source-token assertions там, где есть настоящий oracle; сохранить простые forbidden APIs/build guards.

**Отрицательная проверка / oracle:** Намеренно испорченная проверяемая функция приводит к FAIL; изменение пробелов в C++ не ломает behavioral evidence.

**Готово, когда:** Coverage matrix полна, skipped/unsupported явно отличаются от passed; проверяется production code.

**Не делать:** Не писать тест, который повторяет ту же ошибочную формулу и называет совпадение доказательством.

**Результат исполнителя:** пока не заполнен; использовать шаблон раздела 12.

### RM-32 — Сборка, зависимости и CI как одна проверяемая цепочка

- [ ] Статус: TODO. Приоритет: P1. Зависимости: 00,01,02,29,31.

**Основание:** Проверка; workflow portable + windows-release уже существует.

**Где работать:** .github/workflows/native-backend-checks.yml; tools/build.ps1; tools/dependency-lock.json; H/HallJoy.vcxproj; P/tools/build_fixed_plugin.ps1.

**Зачем:** Артефакт должен соответствовать выбранным sources/defines/dependencies; local environment не должен скрывать недостающие файлы.

**Шаги:**

1. Перечислить входы EXE/plugin/resources и generated overlays; проверить dependency hashes и patch application order.
2. Проверить чистую сборку в отдельном каталоге с path spaces/Unicode, без старых obj и локального SDK побочного поиска.
3. Разнести fast source gate, behavioral Windows gate и hardware qualification; CI без железа не маркирует hardware PASS.
4. Упаковывать только allowlist файлов; release artifacts не содержат user settings/logs/private symbols.
5. Публиковать manifest входов и итогового EXE; повторяемость hashes оценивать с учётом timestamps/compiler nondeterminism, не обещать побитовую воспроизводимость заранее.

**Отрицательная проверка / oracle:** Удалённый обязательный ресурс, изменённая dependency или ошибочный define останавливает pipeline до публикации.

**Готово, когда:** Clean build проходит; artifact manifest и коды стадий однозначны.

**Не делать:** Не обновлять зависимость до latest без отдельного совместимого пакета; не обходить failed gate.

**Результат исполнителя:** пока не заполнен; использовать шаблон раздела 12.

### RM-33 — Измеримый performance/resource audit

- [ ] Статус: TODO. Приоритет: P2. Зависимости: 07,08,12,18,21,22,25,27.

**Основание:** Новый benchmark в этом аудите не запускался.

**Где работать:** tools/run_input_pipeline_profile.ps1; tools/run_long_soak.ps1; H/realtime_loop.cpp; H/backend.cpp; H/overlay_server.cpp.

**Зачем:** «Быстрее» нужно проверять одновременно с release-to-zero, CPU, memory и shutdown.

**Шаги:**

1. До изменений сохранить baseline CPU idle/load, working set, private bytes, handles, GDI, thread/process count и latency percentiles.
2. Разделить сценарии: 0/1/multiple devices; 1/4 pad; default/custom curves; ordinary/extended keys; overlay off/on; burst/idle.
3. Согласовать budgets до измерения и записать среду/питание/build/hash; не подгонять пороги под результат.
4. После одного изменения выполнить A/B с одинаковыми входными captures; указать регрессии и trade-offs памяти/CPU.
5. Soak и real output выполнять только когда разрешены; fake benchmarks не считать доказательством HID/ViGEm latency.

**Отрицательная проверка / oracle:** Вставленная искусственная задержка/утечка обнаруживается метрикой; idle-only run не квалифицирует активный путь.

**Готово, когда:** Есть повторяемые до/после данные и разумный budget, ни одна оптимизация не меняет управление молча.

**Не делать:** Не ограничивать polling rate для маскировки высокого CPU без проверки раннего analog travel.

**Результат исполнителя:** пока не заполнен; использовать шаблон раздела 12.

### RM-34 — Квалификация одного точного release artifact

- [ ] Статус: IN PROGRESS (hash-bound automated evidence; release gates remain). Приоритет: P1. Зависимости: 02,03,04,05,12,14,16,19,23,24,25,26,28,29,30,31,32,33,17,20,35.

**Основание:** Прежний release-readiness остаётся обязательным.

**Где работать:** tools/run_release_qualification.ps1; tools/run_long_soak.ps1; tools/run_production_smoke.ps1; H/version.h; SUPPORTED_HARDWARE.md.

**Зачем:** Сборка и безопасные проверки ещё не означают готовность реального input/output на новой версии.

**Шаги:**

1. Свести все P0/P1, применимые к compiled routes; для исключённых маршрутов доказать compile/catalog/binary exclusion.
2. Выбрать release version, notes, support boundary и provenance; учитывать локальный запрет Git, не создавать commit/tag без изменения правил.
3. Собрать candidate один раз; выполнить hash-bound startup/shutdown, длительный soak и representative native/UAP input/output.
4. Отдельно проверить held-key unplug/reconnect, suspend/resume, Pause/Resume, driver absent и отказ установки; установщик не запускать без явного действия пользователя.
5. Если применяется signing, квалифицировать финальные подписанные bytes и проверить пакет; любые изменения EXE после этого инвалидируют прежнюю qualification.

**Отрицательная проверка / oracle:** Изменение hash после qualification требует нового применимого gate; INCOMPLETE device run не превращается в PASS.

**Готово, когда:** Один versioned artifact с полным evidence; пользовательские настройки не изменены тестами.

**Не делать:** Не выпускать stable при открытом применимом P0/P1 и не заявлять поддержку недоступного железа.

**Результат исполнителя (2026-09-06):** См.
`RM34_RELEASE_EVIDENCE_2026-09-06.md`.  10 isolated lifecycle cycles и
self-isolated overlay smoke прошли для SHA-256
`F2068156F7874FCD4CE4D677BBA048793D6706179637932553104BE90ADAEC58`;
сам runner теперь hash-bound portable и проверяет неизменность 115-файлового
live state. Это частичное automated evidence, не release PASS: version/support
boundary, package/signing/provenance и применимые реальные hardware gates ещё
не закрыты. Backup: `.local/backups/rm34_self_isolated_smoke_evidence_pre_20260906_203100`.

### RM-35 — Закончить или закрыть незавершённые эксперименты

- [x] Статус: DONE (source/catalog disposition; physical evidence remains row-specific). Приоритет: P2. Зависимости: 00,29,30.

**Основание:** Есть diagnostic families, if(false) route и отдельный LAB roadmap.

**Где работать:** docs/v1.4/FIRMWARE_VIRTUAL_HID_TESTBED_ROADMAP_2026-08-23.md; docs/research/*; H/titan68_turbo_diagnostic_backend.cpp; H/mchose_ace68_diagnostic_backend.cpp; H/backend_sparklink.inc.

**Зачем:** Исследовательский код должен иметь владельца, конечный результат и влияние на release.

**Шаги:**

1. Составить ledger каждого experiment/adapter/stub/future comment: reachable builds, реальный пользователь и последнее evidence.
2. Для каждого выбрать продолжить с конечным gate, оставить diagnostic-only с ограничением или архивировать после удаления consumers.
3. Virtual HID/firmware emulator не подменяет реальное firmware/hardware proof; выполнять LAB-01..06 только при измеримой пользе для конкретного риска.
4. Удалять dead code после reference/build scan и characterization; не активировать выключенный burst как случайный cleanup.
5. Обновить комментарии future/F2 для уже внедрённых компонентов, отдельно сохранить историю решения.

**Отрицательная проверка / oracle:** Удаление неиспользуемой ветки не меняет normal outputs; diagnostic недоступен из ordinary catalog.

**Готово, когда:** Ни один эксперимент не висит без статуса и removal/completion gate.

**Не делать:** Не пытаться завершить исследование неподтверждённого устройства угадыванием protocol/firmware.

**Результат исполнителя (2026-09-06):** Создан
`EXPERIMENT_LEDGER_RM35_2026-09-06.md`: 14 opt-in/research branches имеют
reachable-build boundary, текущий статус, последнее evidence и конкретный
removal/completion gate. `check_experiment_ledger.py` прошёл и fail-closed
сверяет все 14 экспериментальных свойств проекта с реестром; отдельно
подтверждает, что ROG Azoth 96 HE остаётся frozen и исключён из ordinary build.
Ни одна ветка не была активирована, собрана или запущена. Источник ROG,
прошивка, HID и обычный output не затрагивались. Backup roadmap:
`.local/backups/rm35_experiment_ledger_pre_20260906_201500`.

### RM-36 — Закрыть аудит по покрытию, а не по длине отчёта

- [ ] Статус: IN PROGRESS (complete source/risk inventory; final artifact and physical gates remain). Приоритет: P1. Зависимости: 00,31,34,35.

**Основание:** Финальный контроль полноты.

**Где работать:** Этот Roadmap; docs/v1.4/RISK_REGISTER.md; docs/v1.4/VALIDATION_MATRIX.md; docs/v1.4/WORKLOG.md; docs/v1.4/CURRENT_HANDOFF_2026-08-20.md.

**Зачем:** Большой документ сам по себе не означает полный завершённый аудит.

**Шаги:**

1. Пройти инвентаризацию в приложении: каждый исходник получает reviewer/date/hash, reachable build и итог reviewed/no-change/finding.
2. Проверить ресурсы, build scripts, tests, dependency overlay и внешний ABI отдельно от основной C++ директории.
3. Каждую историческую finding связать с нынешним oracle/кодом и итогом, не оставлять противоречивые current claims.
4. Приложить ограничения: недоступное железо, неподтверждённые firmware, sanitizer gaps и неподписанный artifact.
5. Записать короткий handoff с одним следующим шагом либо явно завершённой областью; абсолютное отсутствие неизвестных багов не обещать.

**Отрицательная проверка / oracle:** Пропущенный файл/семейство/старый P0 обнаруживается coverage/reconciliation matrix.

**Готово, когда:** Нет unassigned reachable source и unexplained applicable P0/P1; evidence соответствует final artifact.

**Не делать:** Не закрывать NEEDS_REVIEW как DONE только потому, что задача долго не воспроизводилась.

**Результат исполнителя (2026-09-06):** `RM36_SOURCE_INVENTORY_2026-09-06.json`
hash-pins 459 reachable project/tool/test records (231 ordinary-compiled, 10
ordinary-excluded opt-in, 218 tool/test) with date, reviewer and source-gate
outcome; generator fail-closes if it goes stale. `RM36_RISK_RECONCILIATION_2026-09-06.md`
assigns all 38 historical P0/P1: 27 source-local, 10 exact-hardware pending and
one signing/provenance release decision. Source suite and 65-route execution
matrix passed. RM-36 cannot yet be DONE because final versioned/signed artifact
and the explicitly conditional physical gates do not exist. Backup:
`.local/backups/rm36_inventory_reconciliation_pre_20260906_205500`.


## 8. Protocol matrix — как исполнять RM-30 небольшими шагами

**Единица работы — точная family + firmware/layout + transport**, а не бренд.
Создать подкарточку RM-30-<имя> и заполнить тот же шаблон evidence/steps/tests.
Каждая строка ниже означает работу, а не обещание поддержки.

| Подкарточка | Где читать | Что проверить особенно |
|---|---|---|
| RM-30-SPARK | H/backend_sparklink.inc; H/backend.cpp; native routing | row request/response correlation, 8x21 bounds, raw scale, row lease, partial failure, duplicate HID, RowLimit; сначала RM-04 |
| RM-30-SAYO | H/backend_sayo.inc | 0x22 depth framing, index range, readers/devices; сохранить автоматические буквы, усилить неоднозначные случаи; fallback проверить отдельно; сначала RM-03 |
| RM-30-MAD68 | H/mad68pr_backend.cpp; H/mad68pr_protocol.cpp | A0 descriptors, physical key map, ранний travel до binary edge, packet grouping, tracked keys, release и arrival после degraded startup |
| RM-30-HEX80 | H/hex80_backend.cpp; H/hex80_protocol.cpp | 104 slots/82 mapped keys согласно существующим fixtures; firmware-specific map, chunk correlation, потеря одного chunk, domain >255 и unknown entries |
| RM-30-ADDRESSED | H/addressed_analog_backend.cpp; H/addressed_poll_scheduler.cpp | address/request correlation, late response, pending cancellation, scheduler fairness, starvation, safe ownership reader stack |
| RM-30-AULA6X21 | H/aula_win60he_backend.cpp; H/aula_win60he_client.cpp; H/aula_win60he_protocol.cpp; H/aula_win60he_session_policy.cpp | точный board/profile admission, map/scale proof, GravaStar V75 варианты отдельно, Fn semantic, no-flood failure, topology и release |
| RM-30-W669 | H/aula_w669_backend.cpp; H/aula_w669_protocol.cpp | stream completeness, stale matrix, packet identity, scale/layout доказательства; не переносить исправления 6x21 без wire proof |
| RM-30-HERO84 | H/aula_hero84he_backend.cpp; H/aula_hero84he_diagnostic_backend.cpp; H/aula_hero84he_diagnostic_protocol.cpp | ordinary vs diagnostic protocol claims, точный endpoint и безопасная команда, недостаточное hardware evidence; RM-29 решает release scope |
| RM-30-ND75 | H/irok_nd75_backend.cpp; H/irok_nd75_protocol.cpp | exact capability 21/04, отказ soft firmware fallback, stopped/removed/reconnect generation, экспериментальный define |
| RM-30-DDNATIVE | H/drunkdeer_backend.cpp; H/drunkdeer_protocol.cpp | protocol mutex, offsets/chunks, точная G65 карта против других layouts, диагностическая граница и raw evidence |
| RM-30-UAPWOOTING | P/main.cpp; P/overlay/Soup/soup/AnalogueKeyboard.cpp | поддержанные поколения firmware/report, full key domain, zero/release bookkeeping по устройству, disconnect snapshot |
| RM-30-UAPRAZER | те же P файлы + hwHid.cpp | report IDs 7/11, зависимость analogue-report режима от vendor software, отсутствие binary identity, supported variants и честный unavailable |
| RM-30-UAPDD | P/overlay/Soup/soup/AnalogueKeyboard.cpp; P/main.cpp | G65/A75/G60/G75 layouts отдельно, NamedMutex, chunks, Menu/Fn, loss/reconnect; не переносить native PASS на UAP route |
| RM-30-UAPKEYCHRON | те же P файлы; docs/research/KEYCHRON_K4_HE_LATENCY_AUDIT_2026-08-30.md | Q/K/Lemokey модель/layout отдельно, len и row correlation, transport timeout, high-cost reads, раннее аналоговое движение |
| RM-30-UAPNUPHY | те же P файлы | identity bounds, report length before field access, raw>scale, initialized data, early precision; старые findings проверить заново |
| RM-30-UAPMADLIONS | те же P файлы | persistent failure позднего chunk, full snapshot/release, Fn/Menu, parser bounds и independent device lifetime |
| RM-30-RESEARCH | H/mchose_ace68_diagnostic_backend.cpp; H/titan68_turbo_diagnostic_backend.cpp; docs/research | оставлять diagnostic/transport-only, evidence profile/version, не угадывать opaque bytes по цифровой корреляции |

### Обязательные 12 пунктов для КАЖДОЙ подкарточки

1. Выписать точные VID/PID/interface/report IDs и supported firmware/layout.
   Равенство VID/PID не равно protocol proof.
2. Назвать источник map/scale: captured protocol, firmware/vendor readback,
   проверенный fixture. Если источника нет — NEEDS_EVIDENCE.
3. Нарисовать request -> expected response -> validated fields -> publication.
4. Проверить каждый индекс, длину, endian, signedness и arithmetic до доступа.
5. Проверить duplicate/unknown key, frame/row/chunk completeness и raw out-of-range.
6. Проверить последнюю успешную публикацию каждого элемента; большой общий
   packet counter не доказывает свежесть отдельной клавиши.
7. Сохранить физические held/release samples независимо от цифровых букв.
   Для Sayo цифровые события вправе однозначно менять letter mapping, но не
   измеренную depth. Проверять ambiguity/rebind по RM-03; запрет всех изменений
   semantic letters при digital input был бы неверным oracle.
8. Прервать transport/session на каждой фазе. Проверить stop ownership,
   нейтрализацию и отсутствие публикации предыдущей generation.
9. Проверить два устройства одной модели, одинаковый semantic HID, отключение одного.
10. Пропустить результаты через настоящие curve/arbitration/XUSB functions.
11. Отдельно записать parser PASS, fake transport PASS и physical PASS/PENDING.
12. Обновить support manifest и owning risk; не использовать compile PASS для
    продвижения diagnostic family в stable.

Универсальный минимальный корпус: valid empty/released; valid first travel;
valid full press; missing one byte; wrong report ID; wrong opcode; wrong row;
duplicate/reordered packet; raw=0/domain/domain+1; unknown key; повтор старой
generation; unplug после ненулевого значения; ошибка чтения при живом соседнем
устройстве. Для stream/polled семейств дополнить своими правилами, не отправлять
эти malformed пакеты настоящему устройству.

## 9. Проверки и команды: не путать безопасный harness с реальным вводом

Все команды ниже запускаются из корня HallJoy. Это справочник для будущего
исполнения. В ходе составления Roadmap они НЕ запускались.

| Проверка | Команда/runner | Условия |
|---|---|---|
| Поиск текущего места | rg -n 'FunctionName' src/HallJoyProject/HallJoy | только чтение |
| Проверка artifact | Get-FileHash -LiteralPath build/release/HallJoy.exe -Algorithm SHA256 | только чтение |
| Unified source/portable | python tools/run_native_backend_checks.py --require-compiler | сначала RM-01 проверяет состав и эффекты актуальной версии |
| Windows file/profile | powershell -NoProfile -ExecutionPolicy Bypass -File tools/run_profile_transaction_tests.ps1 | ранний exit до Backend_Init; при -SkipBuild сверить актуальность simulator |
| Protocol sanitizer | python tools/run_protocol_fuzz_sanitizers.py | parser-only; требует реального доступного sanitizer runtime |
| Официальная сборка | powershell -NoProfile -ExecutionPolicy Bypass -File tools/build.ps1 | меняет outputs, запускает self-tests/ABI; прочитать effects и backup |
| BUILD.cmd | оболочка официальной сборки | интерактивный pause; не использовать без учёта automation |
| Simulator/migration | run_analog_simulator.ps1 / run_storage_migration_test.ps1 | МОГУТ СЛАТЬ ИГРОВОЙ ВВОД; сейчас не запускать |
| Production smoke/soak/qualification | run_production_smoke.ps1 / run_long_soak.ps1 / run_release_qualification.ps1 | ordinary EXE может открыть HID и создать pads; сейчас не запускать |
| Real-child output test | run_vigem_output_real_child_test.ps1 | реальный ViGEm; сейчас не запускать |
| Self-host/fake тесты | run_vigem_output_self_host_test.ps1 и новые harness | имя не доказывает отсутствие real mode; проверить call chain до запуска |
| Overlay checks | check_overlay_*.py / fuzz_overlay_http.py | только отдельный server harness/fake telemetry; полный app output-capable |

Для будущих новых тестов использовать временный directory и контролируемые
fixtures. Общие требования: уникальный run ID; PID/handle ownership; ограниченный
timeout; остановка только своего процесса; hashes состояния до/после; cleanup
после проверки resolved path; evidence сохраняется при failure.
Нельзя перечислить чужие процессы/каталоги и массово удалить «похожие».

Проверки с global keypress, firmware flashing, calibration/remap writes,
установкой драйвера или публикацией не являются неявным продолжением аудита.

## 10. Измерения, которые нужны до обещаний производительности

| Метрика | Зачем | Как отличить правильный тест |
|---|---|---|
| acquisition -> publication | цена transport/parser | timestamps одной clock domain/явное преобразование |
| publication -> realtime wake | задержка доставки | burst и событие прямо перед wait |
| tick duration p50/p95/p99/max | contention и тяжёлые участки | обычные/extended keys, custom curves, 4 pads |
| publication -> applied output | конечная программная задержка | actual child apply acknowledgement, не только parent enqueue |
| release -> neutral | безопасность freshness | один потерянный chunk/row, producer stall, disconnect |
| CPU idle/load + wake count | отсутствие лишнего polling | одна среда, одинаковая capture sequence |
| allocations/locks на tick | предсказуемость | instrumentation вне production hot path либо с измеренным overhead |
| private bytes/handles/threads/GDI | утечки | после прогрева, серии lifecycle/UI циклов, тренд, не один screenshot |
| UI handler/save latency | responsiveness | медленный файл, many edits, concurrent overlay |
| shutdown/reap duration | граница отказа | fault matrix и общий deadline, не сумма несвязанных локальных PASS |

Не задавать произвольные «идеальные 1000 Hz», «ноль CPU» и проценты ускорения.
Исполнитель сначала фиксирует baseline и бюджет с обоснованием, затем сравнивает.
Снижение среднего времени при ухудшении релиза или p99 может быть регрессией.
Один тест на ноутбуке не квалифицирует все драйверы/USB контроллеры.

## 11. Связь с прежними аудитами и правилами релиза

Этот файл расширяет и детализирует порядок задач; он **не отменяет**
CORRECTNESS_RELEASE_BLOCKERS.md и ENGINEERING_WORKING_METHOD.md.
Исторический severity и status не копировать без проверки.

| Старый источник/группа | Владелец продолжения |
|---|---|
| INDEPENDENT_CODE_AUDIT F-01..08 и D-079 | RM-06/07/18/19/20, regression preservation |
| ARCHITECTURE_REVIEW AR-01..08 | RM-05..12/15/18/27/29/31/36 |
| UAP_ALL_KEYBOARDS_AUDIT | RM-09/10/11/13/30/31 |
| NATIVE_ALL_KEYBOARDS_AUDIT | RM-03/04/09/10/14/30 |
| COMMON_ANALOG_PIPELINE_AUDIT | RM-06/08/10/11/21 |
| ARTIFICIAL_LIMITS_PERFORMANCE_AUDIT | RM-07/09/10/12/22/33 |
| INPUT_PROVIDER_ARCHITECTURE_AUDIT | RM-09/12/13/14 |
| CONCURRENCY_LIFECYCLE_OWNERSHIP_AUDIT | RM-05/06/13/14/15/24 |
| GLOBAL_PAUSE_DEVICE_LEASE_AUDIT | RM-16/17 |
| TRUST_SECURITY_BOUNDARY_AUDIT | RM-20/23/24/25/28/32/34 |
| TEST_EVIDENCE_TRUST_AUDIT | RM-00/01/31/32/36 |
| RELEASE_READINESS_AUDIT_2026-09-05 | RM-02/12/19/29/34 |
| RELEASE_READINESS_MEGA_AUDIT | reconciliation в RM-00, final closure RM-36 |
| FIRMWARE_VIRTUAL_HID_TESTBED LAB-01..06 | RM-35, отдельная исследовательская ветка |

Конкретные старые ID для сверки, не список автоматически открытых новых багов:
P0-003 -> RM-03 (разделить по уточнению владельца, сам auto-learning не P0); P1-039 -> RM-04; P0-005/006 и P1-028..032/038 -> RM-05/14/15/24;
P1-014..020 -> RM-06..12/20/22/33; P1-021..023 -> RM-09/12/13/14;
P1-024..027 -> RM-01/31/32; P1-033 -> RM-16/17;
P1-034..037 -> RM-20/23/28/34. Все ID с префиксом HJ-V14.
P0-001/002/004 и P1-009..013 сверяются по конкретным provider/family карточкам.
Reconciliation обязана прочитать ПОЛНЫЕ risk rows: этот обзор не заменяет их текст.

## 12. Что считать завершением карточки

Допустимые статусы:
- TODO — работа не начата.
- IN_PROGRESS — evidence/implementation сейчас выполняются.
- NEEDS_EVIDENCE — нет достаточного protocol/artifact/сценария; не гадать.
- REVIEWED_NO_CHANGE — проверено, изменения не нужны; ссылка на evidence обязательна.
- IMPLEMENTED — код изменён, применимые проверки ещё не все выполнены.
- VERIFIED_LOCAL — завершены перечисленные локальные gates.
- HARDWARE_PENDING — обязательное железо ещё не проверено.
- DONE — завершены ВСЕ применимые требования карточки или доказано отсутствие проблемы.
- EXCLUDED_FROM_RELEASE — route фактически исключён; это не означает, что он исправлен.

Чекбокс карточки ставить только для DONE/REVIEWED_NO_CHANGE; остальные статусы
держать видимыми. EXCLUDED_FROM_RELEASE снимает только применимые требования
к отсутствующему release route, а не удаляет историю дефекта.

### Шаблон записи после одного пакета

```text
RM-XX / status:
Source baseline/hash:
Problem and current evidence:
Purpose / compatibility contract (owner-confirmed vs inferred):
Successful existing scenarios to preserve:
Product behavior changes required (none or PRODUCT_DECISION_REQUIRED):
Affected invariant:
Old-bug oracle or review question:
Option A — local:
Option B — staged:
Option C — rewrite:
Chosen option and why:
Changed files/functions:
Backup path + verification:
Exact commands and exit codes:
Behavioral result:
Artifact hash, build defines, dependency identity:
Not run / hardware pending:
Remaining risks and removal gate:
Documentation updated:
Next available task:
```

Не прикладывать только скриншот «Build succeeded». Из результата должно быть
понятно, какое неверное поведение больше невозможно в проверенном сценарии.

## 13. Дополнительные области, не покрытые только списком C++ файлов

- **Внешний build closure:** все ClCompile/ClInclude/ResourceCompile и custom steps
  HallJoy.vcxproj; generated Soup/Sun; точные versions/hashes overlays; linked SDK libs.
- **Runtime DLL:** src/HallJoyProject/runtime и embedded payload могут отличаться.
  Установить какой binary реально используется в каждой build configuration.
- **Resources/UI assets:** HallJoy.rc, manifests, fonts/icons/images, лицензии,
  graceful failure отсутствующего ресурса. Проверять decode bounds без
  объявления каждой картинки уязвимой.
- **Tests:** все T/*.py и T/*test.cpp сопоставить с runner, а не считать, что glob
  автоматически исполняет любой новый файл.
- **Tooling:** все tools/*, .github/workflows и вложенные P/tools scripts; effects,
  quoting, temporary root, exit propagation, path safety, reproducibility.
- **Documentation/support:** README, SUPPORTED_HARDWARE, THIRD_PARTY_NOTICES,
  release notes и existing audit IDs, включая внешние ABI потребители.
- **Исследования:** firmware captures/protocol assumptions не становятся
  production proof от одного упоминания в README.
- **Чужие бинарники/исходники:** полный reverse всего Windows/ViGEm/firmware не
  входит в заявленный выполненный объём. Их риски описываются dependency boundary,
  known assumptions и targeted qualification.

## 14. Реестр первичных исходников и глубина проверки

Ниже автоматическая инвентаризация при подготовке этого Roadmap.
Полный SHA-256 сохранён, чтобы следующий исполнитель мог проверить актуальность.
Обозначения глубины:
- **FOCAL**: прочитаны релевантные функции в текущем/предшествующем обзоре;
  не означает полного построчного аудита файла.
- **SEARCH**: сделан предметный поиск/просмотр фрагментов, нужен полный review.
- **INDEX**: файл только включён в inventory; аудит ещё обязателен.

Назначенная RM — точка входа, не единственная возможная зависимость.
До RM-36 каждый reachable файл должен получить итоговый per-file review record.
Файл с большим hash в таблице не становится «проверенным» автоматически.

| Файл | Строк | SHA-256 | Глубина | RM |
|---|---:|---|---|---|
| H/addressed_analog_backend.cpp | 1694 | 144B7D40A5F7FFD5A3BC8F8A60018C1BA146EA17D45E7C74FC16089AFDBCA53F | FOCAL | 14/30 |
| H/addressed_analog_backend.h | 37 | 10275BA90505D69FE06C3BA0D7A4C44899E196B27B6AA8E28421EED0D6C74457 | INDEX | 14/30 |
| H/addressed_poll_scheduler.cpp | 337 | CE66D7BA5DD068DA28D05E7C4935DEC901360879196E1C211302607906E79CCD | INDEX | 14/30 |
| H/addressed_poll_scheduler.h | 93 | 971073C9C48AE3F6E08CAFDF33EE6E083322731D21D08111BC05156C7B75BA05 | INDEX | 14/30 |
| H/analog_host_client.cpp | 3465 | 2E0B1570AD96B949933B4E91EA081672AD59FF70C936F4A8FFA0B6716682CE89 | FOCAL | 13/23 |
| H/analog_host_client.h | 92 | 00A89389B915C6371D96CFF29B5A25EF955CA5FA7958E1683A880E5C84923FC6 | INDEX | 13/23 |
| H/analog_host_shared.h | 155 | DF06AE174BAA9584498F88DE6E607759816D8072C7DAEB4C4848D2394E55F2BF | INDEX | 13/23 |
| H/analog_key_codes.h | 29 | E62F9FC82C395651DDDACD71CC4AAE3C1CC1C1B3518D44C4EA65F8A8749CD2C9 | INDEX | 00/36 |
| H/analog_provider_v2.cpp | 163 | 28CAFFD85FBE9947712A3811490C3C51857478A0382393FA4A20967803054546 | INDEX | 09/12/24 |
| H/analog_provider_v2.h | 54 | 0132C5D6A958C61B1B63953BFF8DA63AF4E2FAF9935E7FEE5E244824CD29432A | SEARCH | 09/12/24 |
| H/analog_simulator_backend.cpp | 258 | F88C5CB4F107FA03787B44A3EC4D648791ABCBDD11E98048AC6D73B49569978B | INDEX | 00/36 |
| H/analog_simulator_backend.h | 12 | F384F243FCAE4C52BEAFC314FAA1ECD8224EE3CC75156481A75948C83672E559 | INDEX | 00/36 |
| H/analog_simulator_model.cpp | 114 | 61811D1621F548490A73A65C76F0D8C58F87EBFB3B16CD5DD476BFA171B686B9 | INDEX | 00/36 |
| H/analog_simulator_model.h | 42 | 3269525EF49BBEE462EDA8572E25D8CFE23446401FF361E40D0992C1D62A2BF1 | INDEX | 00/36 |
| H/app.cpp | 1776 | 8CFC0C50B40C2EBE5ECC004928A4F052DCE31282B9034CC2EACF28948A742B81 | FOCAL | 08/15/26 |
| H/app.h | 10 | 363E435B5C275CA6E89D9AEDC536A62CC29652E91D4AEB55FBE5045E5DF86B19 | INDEX | 08/15/26 |
| H/app_deps.cpp | 251 | D92EB884353659B9378B09F4E4A12E38EA62A907799C8A33C155883F30E0B6B7 | INDEX | 32/34 |
| H/app_deps.h | 20 | 89DEDE69CC67F6E3387136401B5B56925724D351B82448BCB28254D291030AE7 | INDEX | 32/34 |
| H/app_paths.cpp | 513 | 87B9C319472829278448F2ACF2482CEA33BA2DF1689D26B73B3F167169138B81 | SEARCH | 19/20 |
| H/app_paths.h | 32 | 8DFA614BA2FB0EF5FE633986F3BD20A9230DC833A25449709A9FD5BDAA7980D4 | INDEX | 19/20 |
| H/aula_hero84he_backend.cpp | 627 | 05C37F22A5B5644C4EF8007140C40210FC4CB67709D65009AC8381A0D0723E4C | SEARCH | 14/30 |
| H/aula_hero84he_backend.h | 7 | EAF4C8B6F94EB8C7F7485658F633AA244A9A1C4EA134D4077928CAFCEBD2E0DF | INDEX | 14/30 |
| H/aula_hero84he_diagnostic_backend.cpp | 474 | D64A028FFC9547A984581CD0793CA6760736B78077EF7D99CA7AF5E1221CAC6E | INDEX | 14/30 |
| H/aula_hero84he_diagnostic_backend.h | 18 | A17C591CA1D9807F8D631F0FDEE05716904CB71DFAED448AB50B476DB722A075 | INDEX | 14/30 |
| H/aula_hero84he_diagnostic_protocol.cpp | 168 | 33818EEA04E6617944D24F97D07AEB940171E078B734E697043E51C3684655B8 | INDEX | 14/30 |
| H/aula_hero84he_diagnostic_protocol.h | 60 | 1263EA97F8E923987C9DE6E5214B70CBC1B1FB164C19F8A7EEBB2769FC446218 | INDEX | 14/30 |
| H/aula_w669_backend.cpp | 686 | 0ABB6D0ABE6B70DA351E5413C81AA1F6F8F880FA6F64805BE47306C8038AE59D | SEARCH | 14/30 |
| H/aula_w669_backend.h | 5 | 3CF2195ADF293C6DEDCF49D68546E241BD1CB4A4C599DFC711B1A51A392AEC0D | INDEX | 14/30 |
| H/aula_w669_protocol.cpp | 352 | 41AB84464AF82B9BA484C708B037BC3A73FB6C93DCE403C0ABBBFFAA3CBD8C8D | INDEX | 14/30 |
| H/aula_w669_protocol.h | 81 | BA56D5164C293392F3A002E03BDA25DE09651D8124FCB780862EF9AF4A0A214D | INDEX | 14/30 |
| H/aula_win60he_backend.cpp | 2770 | 1D17994C5E2B3B3B86720352140AC804CB3B0C02FE1C4179715B98DBA0BCFBDC | INDEX | 14/30 |
| H/aula_win60he_backend.h | 5 | E3877213656B06F845A1AFE886E05A79992F2651BD3FD98AF90C21855F724D79 | INDEX | 14/30 |
| H/aula_win60he_client.cpp | 667 | E3512AD628AD15F5D2C6272855209833536B63335316F6317ED6E0AE1EB0DA51 | INDEX | 14/30 |
| H/aula_win60he_client.h | 205 | 360493CE95E63F71FF2A5058FFBDD8C1FD5DA0A308BBF1F48B1720AE03DAB173 | INDEX | 14/30 |
| H/aula_win60he_diagnostic_metrics.cpp | 212 | 15A37FF66ACFDCBAF11BC9FF05BD7D87A4A7E632F4FB9C2F9BDFEA0C5672AD90 | INDEX | 14/30 |
| H/aula_win60he_diagnostic_metrics.h | 103 | DE1972F0A798483169FDA0351E5BFABE582958AAA96B625A00A74D127F43CB6F | INDEX | 14/30 |
| H/aula_win60he_protocol.cpp | 610 | AE697709D68BFF5A606AEAE1C38539E29EE92A635E3F24AD3776AD32F0E027C0 | INDEX | 14/30 |
| H/aula_win60he_protocol.h | 394 | CC34775CE21FBD6BA8F550F9FCA773AE00DA008EA8A813BD6058A44E7EA4173E | INDEX | 14/30 |
| H/aula_win60he_session_policy.cpp | 198 | 2152BC78E6AF5A652D7FD17B7AAA6A5B017BB2632645B3B98AC011CE4F4413F8 | INDEX | 14/30 |
| H/aula_win60he_session_policy.h | 55 | 9A33C977D015AEBEB3948CAFC662B70300AD2B1F056EC077E87A6CA489B41055 | INDEX | 14/30 |
| H/backend.cpp | 4420 | A51A1A8E43A0CB1415BF8CE041782963D2CB4F4497FAF70A13266494686B7654 | FOCAL | 08/15/26 |
| H/backend.h | 336 | DF33E570B253825E3E5FE8BBEBEDEC4D5035E71C3A82E32B5125FB11F1BF44E3 | INDEX | 08/15/26 |
| H/backend_curve.cpp | 210 | 0C396F2882AE8F296A6318F1C1B8774A4A335C195886F6199A2BDCED2E4F0B29 | FOCAL | 08/21 |
| H/backend_curve.h | 15 | 6C3721B8044ED46C47AF9AF2F17E10296D0241F5660DA2E932A5D31B743002D9 | INDEX | 08/21 |
| H/backend_sayo.inc | 1084 | 3E81C01A6C11AC6C16A9EF103797C7243DB5ED3725E2209F7859B33EEC493C6B | FOCAL | 03/30 |
| H/backend_sparklink.inc | 1753 | D55BA793D6B7EA1631239FFE774C8F830F56F210E3AB5B3D2CB2883415C520B6 | FOCAL | 04/30 |
| H/binding_actions.cpp | 228 | 2B914E2A7B948E6B5160027B11C337763E8969B81A2D0FF632750C68175226D7 | INDEX | 08/21 |
| H/binding_actions.h | 42 | 8DEF706149E939740B55967236B868D988FEB107214A3382F5FF478EF93D3943 | INDEX | 08/21 |
| H/bindings.cpp | 395 | AFA67B918339E694F24B74F08F325DC13E58B600428F1BB4F76B0A750D7043D0 | INDEX | 06/07/18/20 |
| H/bindings.h | 114 | ED35072F75A40798543AB31ABE74F7828EF21DDD9519F4BD77EAF2C28B781E7D | INDEX | 06/07/18/20 |
| H/bounded_ini.h | 69 | C23405F674FA8654B0A432316190B928F27B00E4BA161049FB4E6AB12EFB0A89 | INDEX | 19/20 |
| H/configured_xusb_builder.cpp | 199 | 43A0C3D7E2F1D56A1CB340BEE2FD22776AA9E86068A8C08ABA1E14CAB52CD2EF | FOCAL | 08/21 |
| H/configured_xusb_builder.h | 56 | E6D2FFFDEE714066ABAF8E0C00A90B4E485DA1B5FBABD3C7B62804ADCFA77326 | INDEX | 08/21 |
| H/curve_clipboard.h | 26 | A1518DA1CC77EA3A7691E55BC732D4EF1F0163E458CC8D61389D00344BCEBAEE | INDEX | 08/21 |
| H/curve_math.cpp | 110 | 6C52D51BBCF4865099A36CD1DD116791ACB01C0CD2FA3C2C199341C8366C9EB2 | FOCAL | 08/21 |
| H/curve_math.h | 89 | 6390E21B4C4B16F67B83F8A09875D951E39BE77C7E9F5DD696E92047C573CBFB | INDEX | 08/21 |
| H/custom_page_controls.cpp | 146 | 4060D8A3FF176105E8CD1033EDCEF4CC7CBA9087D557B16622CE1582534EC88D | INDEX | 27 |
| H/custom_page_controls.h | 24 | D93F80E58A136C7C9919B69EC5D80A2FE32D8BFDD602841C44F875AAECC1B2D1 | INDEX | 27 |
| H/custom_page_surface.cpp | 437 | DF58FC85E55B5953BF2B7E57C3992C00BDC49FDF6DF0978FBBDBD53269F235BF | SEARCH | 27 |
| H/custom_page_surface.h | 88 | 720B1181DB475A6232D272BBF730F6C2FF9DED76C8DB8715F19A919E622E3F1E | INDEX | 27 |
| H/debug_log.cpp | 1217 | 24524C217C93AD091F51B8572AC70CAF0D31E044DDBC344F25F41D77145FA18A | SEARCH | 28 |
| H/debug_log.h | 44 | D434EA214B287805274C0088889E20157BA0010BD1C3B326BCE4A4227AB452C2 | INDEX | 28 |
| H/dependency_guidance_policy.h | 26 | E717F80731AE6BFABC181FBF3D9875894B1F86B396B260D9A5C790D571A1ACAF | INDEX | 32/34 |
| H/drunkdeer_backend.cpp | 1443 | 52EBF208D6E56C2F8510726221CF5894C7E817F09E13C0D231803157C2907719 | INDEX | 14/30 |
| H/drunkdeer_backend.h | 21 | D3701D14CE3559A34FD6DACBE7E65F566C264842835E7D1F9EFE84E4DF27675F | INDEX | 14/30 |
| H/drunkdeer_protocol.cpp | 208 | F05DCF3052CDE6771B9C14DBBF555574FADAED3DB02AE997DBA1C7682C89F09D | INDEX | 14/30 |
| H/drunkdeer_protocol.h | 89 | B2760BC9CC082333382946333E232D1CD2CE652F94559E195979FED6D83B1063 | INDEX | 14/30 |
| H/embedded_analog_stack.cpp | 321 | EB2346553E6B435FD3AB6AE3D6D40678369C7365ADFE6EA964CFDA6FB3B4BC72 | SEARCH | 13/23 |
| H/embedded_analog_stack.h | 28 | C3EE20B1F2287CB5E0BA4A47FA5BE66959539DBC8F32664AE31ED618136CFC1E | INDEX | 13/23 |
| H/embedded_vigem_installer.cpp | 613 | 832676D9613831A36EBA332A8E7A4039531B7D01E3263AE4965353D155CFC47D | INDEX | 32/34 |
| H/embedded_vigem_installer.h | 36 | B191FEA48F78410D0AD0BAC6BC43280AF388D0C7A6DF526A20E8DD925D5ED5BB | INDEX | 32/34 |
| H/factory_reset.cpp | 387 | 2D14CE39FE2DC2B49FEA1EDBF09DD6C248A14DA022AECEAB6AC78D0EB20A8127 | SEARCH | 19/20 |
| H/factory_reset.h | 28 | C860F437550A5A1799D6E2244E39753A3DA05354F46AD885499F914DE34C94C9 | INDEX | 19/20 |
| H/file_name_policy.cpp | 237 | 0117BEA7500AEFF0474024690ACE2C1CA0C9E8256311C4AA9464951580FA1AEE | INDEX | 19/20 |
| H/file_name_policy.h | 29 | C6E3E1466451EF484F631909187A2473DAF486FB29ED50C0E716BA8ED4CFC0C1 | INDEX | 19/20 |
| H/framework.h | 15 | 325BBF4FBDDD2DAAF5110D091481E76C7E5EC6E32645B08797611F0E0A3D7896 | INDEX | 00/36 |
| H/gamepad_render.cpp | 125 | 51042D8D69B8CAC443B392BE6EDA99B80199C507218D7124BF3BE89F130278DF | INDEX | 27 |
| H/gamepad_render.h | 19 | EAB73B83818C8512FD7E82332675E8FBB4D2D9DA76CE23FE03A4A0900183F2B8 | INDEX | 27 |
| H/global_profiles.cpp | 295 | 53C359D31D464300FED6FC216AC9A7C66B7E7112C65A79DB6230D4ADB6D78AC6 | FOCAL | 06/07/18/20 |
| H/global_profiles.h | 27 | EE659050E1B5612FBD4E88F0974E576728F19BCBB65EEEA359C184E28F662EFC | INDEX | 06/07/18/20 |
| H/HallJoy.h | 3 | 005051C1E7E6305D567E92B02A84A198201D3479F924B9FF9F49410B67664231 | INDEX | 00/36 |
| H/HallJoy.rc | 188 | 0DECC6CE855058CBD9101495F957F81BA27847DF21F1C512449C5F037477278E | INDEX | 32/34 |
| H/HallJoy.vcxproj | 511 | A605653ED63D80228A4ADB435BE6652D0557D03C7A4E7739B554385243F94544 | SEARCH | 32/34 |
| H/HallJoy.vcxproj.filters | 604 | D429928ED244A8232A730332F26A73F77C7892BD8008E03E1B85E14F7369F438 | INDEX | 32/34 |
| H/hex80_backend.cpp | 984 | D8966E2698412F87F7D9ABCF3407712E197E29793A887C3201F56938419697E7 | SEARCH | 14/30 |
| H/hex80_backend.h | 49 | 336AE4348558B3D3ECFD4C824B7E4F1ECFADC5F44E7D66FD94E72F876807FA25 | INDEX | 14/30 |
| H/hex80_protocol.cpp | 140 | 3C756F032A875C6A04B7E5D8B2DC1D179CE7959B7682565F8997021B9288E3EF | INDEX | 14/30 |
| H/hex80_protocol.h | 91 | BD3828CD58BE9134EDDC4DD1FABBFB5C07ED6AE7A604B6E0695B34DB00886729 | INDEX | 14/30 |
| H/hid_io_operation.h | 167 | B6187F7F6BA22EA8BA4AC1D59C205BD639B775CB5474F799D98A40F3BF671C83 | INDEX | 00/36 |
| H/ini_util.cpp | 357 | 6546414D8AC0C71A91F7B919BD8F59162A36E089B5E8CA8A7F1049332D4A5D11 | SEARCH | 19/20 |
| H/ini_util.h | 47 | BA5C6507F91AA97115D8A24815651990F71FB996643671C80A84770864FB3303 | INDEX | 19/20 |
| H/input_wake_sequence.h | 43 | 30DB860F81C66FE02D4691D63DF41893DE5A8302F0BF926ED123C51F333043BE | INDEX | 22 |
| H/irok_nd75_backend.cpp | 931 | 2B07D565E6716AD0D3C661D9956183791262E6FA3FF84E97B097A3524E9F27CD | SEARCH | 14/30 |
| H/irok_nd75_backend.h | 5 | 810769BEED5DC100A9E11149A3426547D7D288438FBE75700E3ECCDFF7853312 | INDEX | 14/30 |
| H/irok_nd75_protocol.cpp | 197 | 9630342FAE90F0A80699A6CF1DB5ABEAADB6D5EA3F31689720117D9D07F2141B | INDEX | 14/30 |
| H/irok_nd75_protocol.h | 69 | 7C5ED0A25017EB017F17B593EE1F9F7C91EE7B39E76A66885AFAE42ACD981289 | INDEX | 14/30 |
| H/key_settings.cpp | 364 | 13ED963E76F8E979A1AEDE192CE206F1F79A8F80CF86ED6134CD05C3AEFDD8B9 | FOCAL | 06/07/18/20 |
| H/key_settings.h | 71 | AFE98C9A3EA9EE14006517FD6503113D06DDE6660BFB411902F4FC0742C11ABA | INDEX | 06/07/18/20 |
| H/keyboard_bind_panel.cpp | 151 | 3CC5D165A216CE2F8157AD3944D6D9C9A1F5A02AB078A4C2AE0E8A1DA33B65EE | FOCAL | 27 |
| H/keyboard_bind_panel.h | 7 | 587B73120E31E59D80C62CFE62FF1D903F76CF054CE33BC549B48BC6244D9E69 | INDEX | 27 |
| H/keyboard_keysettings_panel.cpp | 1029 | B611C1243D95D7F302BFBBD8E4C67B02C102FEC27205B781B3863ED2C3CFBC55 | INDEX | 06/07/18/20 |
| H/keyboard_keysettings_panel.h | 71 | F91BEF84F5113F11D65A0DDCBD4549C13B6F05B47EFD5ABEC5FCFE59E93D1882 | INDEX | 06/07/18/20 |
| H/keyboard_keysettings_panel_graph.cpp | 1124 | AD01F4CC23BE692A717AA98D6A5237E362BA0372DD71DC64C1EA02BF61D7A28C | INDEX | 06/07/18/20 |
| H/keyboard_keysettings_panel_internal.h | 101 | C52817A9E43DCD5F31DE91131A5D94304EAEEE842FC33EB667A9A99D0EBDABC5 | INDEX | 06/07/18/20 |
| H/keyboard_keysettings_panel_logic.cpp | 514 | BA9EE7D8FC6A1309E9C0622A3A4A760A07CCCC1F33BC2D68639EE4270490C72D | INDEX | 06/07/18/20 |
| H/keyboard_keysettings_panel_style.cpp | 523 | 5D2E8FF2D719EAE59E321609C2A1C49F30BE94DE08C32EA20E02BD61DC26E004 | INDEX | 06/07/18/20 |
| H/keyboard_layout.cpp | 1274 | 4F3DDACE00A08BBC6CB88B1884C96E602A9A5E40C4515AD7C7759E2454BF82EC | INDEX | 19/20 |
| H/keyboard_layout.h | 56 | 91C5E5468FEE7ADAE30D1E0EA74A850CD18663B14671CD55383B20BD29D85398 | INDEX | 19/20 |
| H/keyboard_page_main.cpp | 2392 | 0292F77CDD373CD1DE8FEAE8D8B81C68E18C23E94C20AEFDA2B45D65118DD58F | INDEX | 27 |
| H/keyboard_profiles.cpp | 544 | B1CD623C87C60706F3AFBBB8C50DAAD607522C07EFEF90975677FBB00FE7F58D | INDEX | 19/20 |
| H/keyboard_profiles.h | 59 | C78D8F3F6E4A1DF01ED69806414A96390AA37AFD52F007BC409FCF8EB8A88931 | INDEX | 19/20 |
| H/keyboard_render.cpp | 1219 | D024D5210C92B58FF8FB3EC97F753D6D5F65561CD5225C684C5C11BEA3D203F8 | SEARCH | 27 |
| H/keyboard_render.h | 39 | 5E5793A3B5A2737D9F737E30E21AB70004E9EF335EDEA0296EEE8AFB48E03D3A | INDEX | 27 |
| H/keyboard_subpages.cpp | 11161 | C26DE9D5188323CFCB8D76722507D2489331713C18157CB74488815DBFF2A880 | SEARCH | 27 |
| H/keyboard_ui.cpp | 307 | 005D56409E84528D96A9D225E238A6860132713BF465B11AC21B3E3098A00923 | FOCAL | 27 |
| H/keyboard_ui.h | 12 | 599E4C9CCA7E4A51658751019A682539816829FE43A47582F92A7B5DE333129C | INDEX | 27 |
| H/keyboard_ui_internal.h | 23 | C243D673605C34A0EA321A8692D28101B11FD8B67DFF3F132D0872B88476F689 | INDEX | 27 |
| H/keyboard_ui_state.h | 29 | EC0EC332F289A72F33317B0E85AEAFE082A0BB42D0D683534A4ADE3189F22DDF | INDEX | 27 |
| H/latest_value_mailbox.h | 112 | 2F3063DEAAF9C746081DBB8258B2982C4D90137823BDE6B0DC6CC54ECB12CBB3 | INDEX | 00/36 |
| H/mad68pr_backend.cpp | 2746 | 65E2C91C0C5A01E953BAAA1621646051A16AB96B92E5984789EE2CA347D4C679 | INDEX | 14/30 |
| H/mad68pr_backend.h | 58 | 8877F12794E2CF830AC8CBDA4508556BA94F7CDA55ECBA55E10DFCA1DD4896FF | INDEX | 14/30 |
| H/mad68pr_protocol.cpp | 193 | 11FA0B8EF349350AF131C9ECBF2861E1766551F705B285B886F6EEE4E3DFC7EC | INDEX | 14/30 |
| H/mad68pr_protocol.h | 170 | 0557F34DF8A97E770FD9CBB5A239748B907530EAB4AFB947A22B2E4D9E5E6FF7 | INDEX | 14/30 |
| H/main.cpp | 336 | 6CBBA484A61CDFA17210AB97FC2B0129A934434D32A8DF07E16D307DF70B67BD | FOCAL | 08/15/26 |
| H/mchose_ace68_diagnostic_backend.cpp | 403 | 22F21FB78931A3C29F0B186CE247A6F5B948A3622ABBD46F0667B914BA130B45 | INDEX | 14/30 |
| H/mchose_ace68_diagnostic_backend.h | 17 | E30412A2C96EB99D07B54AE2594C657F48A1E39DEE2E34AECB93AB30ED5EECB8 | INDEX | 14/30 |
| H/monotonic_time.h | 38 | DB06E5A2AF6D6E186264536381EFB920E43BD02E2690F4008696856B24709059 | INDEX | 22 |
| H/mouse_bind_codes.h | 33 | 2AAC4E0D61C3B4EF2C629A876436FDCA15BC12A441109C6D793CBA9FD83D9108 | INDEX | 24/26 |
| H/mouse_ipc.cpp | 236 | BF67A898F538D1C2AB6242A862B1D02E4F207A5B36CF6ABE0BA700695197C57F | FOCAL | 24/26 |
| H/mouse_ipc.h | 38 | F858B92C8BF547C959762D41938B4B9C63D87A0924BFA05CFA7E9F5879E2AA14 | INDEX | 24/26 |
| H/native_analog_backend.h | 120 | 2EAB5262EC9C2B6861DBAE3B9D83B8404E99AA6160BF6235C68AEF01F91EBD5A | FOCAL | 09/11/29 |
| H/native_analog_backend_registry.cpp | 321 | 80039ABF4BC4715D694578318BEB13C8B15D4C8A3A71B4826353048D313D9C71 | FOCAL | 09/11/29 |
| H/native_analog_backend_registry.h | 59 | 41C720BFDFA9836DB2706008C3865F087893C3CE13DDDB2B47DD17CC8A2289F8 | FOCAL | 09/11/29 |
| H/native_analog_backends.def | 34 | 52A201F7F401266ECB38F8CE1D0919B646C0F092B6A67631B6DA3ED4994E4A9B | FOCAL | 09/11/29 |
| H/native_analog_routing.cpp | 76 | 50CB13649D10C931724B3F3B6568FDD759C685AAA90D80277EDF9A8B64B6BFB3 | INDEX | 09/11/29 |
| H/native_analog_routing.h | 37 | 0A3E716FEE8198A75697CD7769452601799C3F24E8E6D93B859435B2DD10B9AA | INDEX | 09/11/29 |
| H/native_backend_lifecycle_registry.h | 128 | 883888E46CE60C0B34C1AA98E26A45A1F6AE10F9A85D6F9578C7A53463816576 | INDEX | 14/15 |
| H/native_hid_interface_claim_registry.h | 90 | 78E11186C84EB6DAFF25A054DBD99738397F2E187A68884C4ADE904E2D0AB179 | INDEX | 00/36 |
| H/overlay_server.cpp | 2333 | 8CCA7E304B603C47A9C778F5390B9094933F5F8B50B134E06CE6706F29D3623B | SEARCH | 25 |
| H/overlay_server.h | 56 | 16B19F326ABF3251BBE010BE73185B44B7F651ACA30F8864E9F1A84478EA1EDB | INDEX | 25 |
| H/premium_combo.h | 136 | 4AA2DBF2F8BD618814B40636D6E2A11D1898BD61AA50AE5974AA206A3C303463 | INDEX | 27 |
| H/premium_combo_anim.cpp | 343 | 9CEC2508C164CBC76F708889ABDCC4769199948B3C0291446C7A752B4A4FC89E | INDEX | 27 |
| H/premium_combo_core.cpp | 949 | CCA9FC116011D22B941B82DAE403659F9EFA24CF24EE5B94C461D4553BE46AC2 | INDEX | 27 |
| H/premium_combo_internal.h | 365 | 8423A8D5536C30A3487D64334434C5AC9D60FBBB622B7D3F4634D0040154B0D7 | INDEX | 27 |
| H/premium_combo_logic.cpp | 1106 | BDDF97710C59E1FA0D0CECE80A3C5FD6BFBD8A9794D84DC56E4646DB9FF3DF53 | INDEX | 27 |
| H/premium_combo_paint.cpp | 897 | 85EE3C84F36B815BAC75B7EF45C7C42FC59B977ED4F78D9FBB02ADDD4765EE97 | INDEX | 27 |
| H/process_generation_supervisor.cpp | 654 | A5BFCB3EE2BD86FCBF6A439D8A9FB83DE4A3E43B9ABE0E31B79F3FB320A275AD | INDEX | 14/15 |
| H/process_generation_supervisor.h | 138 | 271C72961073D3C7E082D024F3B101F466451414CFCE7E23F5A2C2E38C3F428E | INDEX | 14/15 |
| H/profile_ini.cpp | 294 | AF1044039E0BE149358CF21A247E853C5B0178741018808ED99C9756E1B1E3FD | INDEX | 19/20 |
| H/profile_ini.h | 8 | E91617622C914E3E4F5D941E6CE58B1D169457C446807BC44EEB9BE018804486 | INDEX | 19/20 |
| H/profile_runtime_gate.h | 48 | 82877AB0C91BDAC2AE3B1434D385E54FF3ED82E162AAC8B57F4EC09C88ED7558 | FOCAL | 06/07/18/20 |
| H/protected_native_handle.h | 124 | B083DC5A59E2B3810879513C4E9534BB3257EEC2F084867190168A296041F41F | INDEX | 00/36 |
| H/provider_v2_controller_shadow.cpp | 144 | 960B2DE113E27D7A51B0F51A6DE1DB72656B7BAF74B242E457E2C9AC2B4E9A4D | INDEX | 09/12/24 |
| H/provider_v2_controller_shadow.h | 74 | 35690727E5F10E2D63B14057669FEE556073166D9D47E979646B6EF02A6B0E26 | INDEX | 09/12/24 |
| H/provider_v2_data_plane_layout.cpp | 325 | 351850611E9F38E366475FE30F20529F0B0C99944F1CA31CC4576E54740975A1 | INDEX | 09/12/24 |
| H/provider_v2_data_plane_layout.h | 159 | 1EC83F2E17BFB704DFF7D74A1F697ED32D68F29B2D3F7F44D7C4BC44E5F19DD5 | INDEX | 09/12/24 |
| H/provider_v2_data_plane_windows.cpp | 432 | EE62E310928EB2B16942F178410D422FE7DD6B35B6D0CE07FA4F6BF5A5849FDE | INDEX | 09/12/24 |
| H/provider_v2_data_plane_windows.h | 125 | 872331A4E6208984BA21118D5C0F4C66AE9D92FC479C233D39764B53F2DB2C30 | INDEX | 09/12/24 |
| H/provider_v2_qualification_model.cpp | 106 | 5FC40132B44D1676BC10DFC7377D710D802E67DB3CA92FB927B8CD51F1163BD6 | INDEX | 09/12/24 |
| H/provider_v2_qualification_model.h | 97 | A2C5CC123C6544F75A3E5619D54665944D7143F26289E6FA0FEFE32BCDE511CF | INDEX | 09/12/24 |
| H/provider_v2_qualification_report.cpp | 231 | C009153A68D41EDDD21AA91D197048AAA2E88900F8487CEC93FBEEDD22C3549B | INDEX | 09/12/24 |
| H/provider_v2_qualification_report.h | 9 | 81F39FD4A1EA80D1950E2E030521E3DFF8BD018D01F55E061BBB64BCE74B7B4E | INDEX | 09/12/24 |
| H/provider_v2_snapshot_broker.cpp | 265 | 689B163B92DD58A06AAF288F35009B2D57220428F5785153310091BD50F60E2B | SEARCH | 09/12/24 |
| H/provider_v2_snapshot_broker.h | 127 | 0FA6D6FDDBE4841DE95F887FFEA0D2275342B69254F7A0E8F8E6362F5F298656 | INDEX | 09/12/24 |
| H/publication_generation.h | 27 | 9F6391AD38AF08C63ACF3A44DC2BDBF144C0686B4700AB464AD9E21E7DB6C533 | INDEX | 00/36 |
| H/raw_input_packet_size.h | 21 | 0F47F18B116CE84CCCE21226A1CAB05F725DC95F3B8EE3224939295A961A5FA0 | INDEX | 00/36 |
| H/realtime_loop.cpp | 598 | 7CA3C0CE40A1D6D8037DFD0FDFBBF0C1402DD4244D0DEFB5021F001C747491B9 | SEARCH | 22 |
| H/realtime_loop.h | 23 | 43E72C39D99B743974E9FF79963E3C8221E9A684D0905C41DC571F3A46A0404D | INDEX | 22 |
| H/remap_abxy.cpp | 156 | E1090A6C6905EBA1F90CAB7011C0A80B2F95A812362D86EF1E75E6D4D9D89DFC | INDEX | 27 |
| H/remap_abxy.h | 11 | 972E55E44E076C5F374A7076E78A97CD8368F6A275F397CA6CD3090E1798AFBF | INDEX | 27 |
| H/remap_bumpers.cpp | 265 | 00DE2C1B30310AEC8F4E60BAF51787248B2572A30DABE65C8E41E899D1812A10 | INDEX | 27 |
| H/remap_bumpers.h | 16 | 750D68A49058E793169862221C1B9F93F9D61A15C4D85DC7114B9295147D4E2F | INDEX | 27 |
| H/remap_dpad.cpp | 186 | 14BD222F672648E09F3A8CA36DAB5ACFDA741C8FBABBD18CCF7ED63BB605E77A | INDEX | 27 |
| H/remap_dpad.h | 17 | ACA9451B3D47C2634C10F5DE03DE1AF4FA6DB20CAEACC06F3D6867F7803D8A61 | INDEX | 27 |
| H/remap_guide.cpp | 352 | 3195589DBD71125DE540797AB7F8E627D422BFDAA7505C6FC29E6E18D5A1ECEF | INDEX | 27 |
| H/remap_guide.h | 7 | DA96819C7F18B9582777BA3AFDE96EF0AB2E28BD9479C08B57CA789B7F68C68C | INDEX | 27 |
| H/remap_icons.cpp | 256 | EFA8BAC50CF57699F6FA67A1EE525F66946721DDB5BCAB7E26D72013DE26E1B0 | INDEX | 27 |
| H/remap_icons.h | 32 | 8CE4D1A37286CA0446AA90B91DE0A4E3E0C3B0085F7F50FBF9369278F3CF4517 | INDEX | 27 |
| H/remap_panel.cpp | 2553 | 15CBB4F980A863FC27F056D2AA1BB29B47F2435CE39CF6FA4E122063FF42EE84 | INDEX | 27 |
| H/remap_panel.h | 8 | 11AA7F54D4F0A375DBC302C72C0AE0A2E069CA8490F8F16F6C2CC1FD2BCB2E74 | INDEX | 27 |
| H/remap_startselect.cpp | 214 | 8F9EAC1E62688E9A8F71ED228E912363920FA48EE58E941551CC5BBA646EC469 | INDEX | 27 |
| H/remap_startselect.h | 17 | 8CA70F3F5761048589D2204098426B236B7378582B8D3AD28B8094F887FD43CD | INDEX | 27 |
| H/remap_sticks.cpp | 201 | A06A999F61F1D76579C522115AE396F4FE5FD3705853B276EE5ACDCB3B8DB58B | INDEX | 27 |
| H/remap_sticks.h | 21 | A2A3D65AE066FD73EAA344C04B072BDBC653B972A064ADAF89835025A52FC1DC | INDEX | 27 |
| H/remap_triggers.cpp | 242 | 7BAD83636DD24B3D16A3201B61DBBAE2CFB0C6C6DB0BEEB103C9372E3B797958 | INDEX | 27 |
| H/remap_triggers.h | 15 | A785CD031C8C85A22ED9941EFB88381A01E9861B1485EBA05F826C4D401EFACE | INDEX | 27 |
| H/Resource.h | 34 | 739249F5013EE06546C565B478DFBA1BB9A6D886E01C190A2BB03D942DCF20CE | INDEX | 00/36 |
| H/saturating_int.h | 19 | 9381FE1338950B39BBD88733185CEA8527278F9AE76CB63FAA47EAD3C8E46C82 | INDEX | 00/36 |
| H/settings.cpp | 565 | 43253660FA1DA959CEA90DB806C77030DEF998D03B082DD4A497FB4401A6E318 | INDEX | 06/07/18/20 |
| H/settings.h | 162 | C0B4907BE8F099E466A0201855E4974FC838AF9AB40CB80323B061C961AC67D9 | INDEX | 06/07/18/20 |
| H/settings_ini.cpp | 853 | 07C090E947D07555810D8C1CE807D9027207CCFE7693AC37DA192C25012CC333 | FOCAL | 06/07/18/20 |
| H/settings_ini.h | 12 | 25D3A63EB6739983BF30731D0162F1261D3E13D0504DC9488B777A9C2589133A | INDEX | 06/07/18/20 |
| H/sparklink_hotplug_age.h | 24 | 5CE631B765C233B33CF7366BEAB19249A1D40AEBFD5C981489C2E6B092905933 | INDEX | 00/36 |
| H/stability_trace.cpp | 530 | 4964B68593B4F00E47C7354D811DDF574C6996700AA3DA2DEF4DA062579E72F6 | SEARCH | 28 |
| H/stability_trace.h | 39 | 2F7B5C7FB3503C3B9C11DC022BF76EA7CD2305583FEDAC199BABE45B47E608AF | INDEX | 28 |
| H/tab_dark.h | 239 | F9B780C3646F43E18D16A9DB2A2463E18FC7B0285E4C75C3CE7ADC4157967109 | INDEX | 00/36 |
| H/targetver.h | 6 | A81336C05E2659E95984B2B3B4FE64A689F2F966467257284E11C3C6A2B121F1 | INDEX | 00/36 |
| H/titan68_turbo_diagnostic_backend.cpp | 600 | 53C6336AEAE1C630F8C6AFB446D3BA1F4E491AAD956492F5D548F7BB020F09F2 | INDEX | 14/30 |
| H/titan68_turbo_diagnostic_backend.h | 16 | 6F2278B0FEEBE6F2F0C8E3255F29FE5F0D49D4F7ED43BEF0299B7DB657820C40 | INDEX | 14/30 |
| H/transactional_file_store.h | 57 | 5F2F4926339CBCB97A5300C50BD22E5EFF6BA88DDAE4EFD803B45FDE05D177ED | INDEX | 00/36 |
| H/uap_parent_snapshot.cpp | 125 | BF99FC34C2767D5DCD212DAFF0441E40EA3197B9CDF7CD9853EA63A8A1682BA0 | INDEX | 09/12/24 |
| H/uap_parent_snapshot.h | 56 | 83419FE74B31DA31ED67FD365E3A38FC9A8D17EBB333B2F6D86A33361DB23C33 | INDEX | 09/12/24 |
| H/ui_paint_audit.h | 63 | D45C2C0985C4CAF84E083613033F656B86D2B3FD577D3F924CB228AB1CDB060B | INDEX | 27 |
| H/ui_theme.cpp | 97 | D2A5630A00251D603F43C6C2596A3675E24826B2ED9F7F742578337C70971E01 | INDEX | 27 |
| H/ui_theme.h | 31 | B7C11EE35D01288B3BEDC05D7394198069C68A3C4CE500D128635CC9CAE5D013 | INDEX | 27 |
| H/version.h | 13 | 91C6A6FA70098949B75E6DE843489C3479C292567DEF5C6AA243B4B6021CD2F3 | INDEX | 32/34 |
| H/vigem_child_transport.cpp | 277 | 6AB79D3818C96F2F5C9AEDAB4C23BEE857E021D4F77D2F38D9A6D71B34F77F07 | INDEX | 05/15/24 |
| H/vigem_child_transport.h | 109 | 7FB15F03763CFFC5BB4B1E0D0A79A25A7E27AB798478182D8B8B702E916AFEBE | INDEX | 05/15/24 |
| H/vigem_output_channel.cpp | 632 | DC1E3CB6147AC2D56237B5BEE501EAE535155E62141882D8F54A8F018AEB6909 | INDEX | 05/15/24 |
| H/vigem_output_channel.h | 174 | 4E7D76563CEEE66752200059406F4DDE74E48CDB0D50A6D10B39A57E034CE043 | INDEX | 05/15/24 |
| H/vigem_output_process_client.cpp | 562 | 180346F88AB119B0DB1014C754569397D86473BEC1EFBAD3F30E54C50DA31D07 | INDEX | 05/15/24 |
| H/vigem_output_process_client.h | 117 | D083168910CC37C1EF3F552A5524DC5EE3E497A12DD81C453CC2A0F8C7921FCD | INDEX | 05/15/24 |
| H/vigem_output_process_host.cpp | 456 | D70C2769C04E392C04CBDC7406692A7ADA870F89FDD4E573FD56DB5105C79390 | FOCAL | 05/15/24 |
| H/vigem_output_process_host.h | 6 | A36A4EF91A465A6A886DA326FD2D72765C0AE33F319DFB7D71D026ABE94B5A5B | INDEX | 05/15/24 |
| H/vigem_output_process_protocol.h | 75 | 0A6A4D887217B66F4DC02FCDF720E3AA4CFA609875109C6D60444BBA5BDE46AB | INDEX | 05/15/24 |
| H/vigem_output_runtime.cpp | 708 | D8949FE6239BB4C8CDAB228F36FD7A3A3EA490D64138DDF1E74C74B789EB4AFD | SEARCH | 05/15/24 |
| H/vigem_output_runtime.h | 78 | D1C1BCC7B41B80192D1883CD7A2BA1F141C982F513F390C89EB3C4F30006A600 | INDEX | 05/15/24 |
| H/vigem_output_scheduler.h | 80 | C744D38F58F2279D3BCF769C41A6B70D7BE30FAF48C728F57658C98EA6AC6E4D | INDEX | 05/15/24 |
| H/vigem_output_self_host_test.cpp | 795 | A799C23B3553D9C7A329D392A5B82BD0F4E1182DB94ECA31C762AA5CD9E667E7 | INDEX | 05/15/24 |
| H/vigem_output_self_host_test.h | 5 | F0E72577AD9BC14140542F8BD9243A9D6C321D82D395C3D618D3DA497A98DBFF | INDEX | 05/15/24 |
| H/vigem_output_shared.h | 144 | 5A5106366EB2042AA909F827E5308E4233B0D1F7E704AA34F2BF31C75138E48E | FOCAL | 05/15/24 |
| H/virtual_controller_frame.h | 65 | 0F5FE44CF2444D35761AA349DA5067F358CA66E528B9B02EA18280812B089D71 | INDEX | 08/21 |
| H/win_util.cpp | 97 | 6C787917A1F288B073AC22A4026DD752F1E967D95961AC4F1B3F1B39B0B88AA2 | INDEX | 00/36 |
| H/win_util.h | 20 | DBC2FF3EAB6AF90722F13F945AE31461C30068B852BC23FB6225E3FC57223C2D | INDEX | 00/36 |
| H/windows_command_line.h | 36 | D531A212867153ED8AD158E8C6071E5AE507D20C636DF69A577EFEA57828067F | INDEX | 00/36 |
| H/worker_exception_barrier.h | 96 | 291977752F51CA054A3021B7EA7493C0D02A3A51931E739C266B7916DD9DB6AE | INDEX | 14/15 |
| H/worker_join_policy.h | 38 | 6EAD031C095F7BA037269D8E19E2CC8A57B459706FECF92C3161C80CA51B46C8 | INDEX | 14/15 |
| H/worker_lifecycle.h | 477 | 04E80CAB20D0558295DB4342E4FA623A0794184F850BEDE1250EB163073C44E1 | INDEX | 14/15 |
| H/worker_primitives.h | 144 | 4427D3CBEA9E08DFB03F106E4ADF774CABA8A5F622AA24A5BB0B4324AE7BCF19 | INDEX | 14/15 |
| H/xusb_output_adapter.cpp | 61 | DC8580653F4C8A720BCC277EAC4FE097D541717426840AAB8DA93DA3AE686343 | INDEX | 00/36 |
| H/xusb_output_adapter.h | 13 | BD9C402982DF3D70E19FB57791B63856CE472AAA434DAAB60DCF38670395775E | INDEX | 00/36 |
| P/main.cpp | 1501 | EAF9E99745E2FBBAD15AA020FB925375E7D4A5CB8533765CEE8D98B8AB7AA450 | SEARCH | 13/23/30 |
| P/overlay/Soup/soup/AnalogueKeyboard.cpp | 1332 | B454800F09C12CBEC480B5FCB9D0F8C1523CB0E63118AF9B97D5AA41C0EBADA7 | SEARCH | 13/23/30 |
| P/overlay/Soup/soup/hwHid.cpp | 1447 | CC01D673C138A57BB1F4AFD947C7C05233B211F5E780C1823C46BF41E376DEA4 | SEARCH | 13/23/30 |

## 15. Что делать следующим

Следующий исполнитель начинает с **RM-00**, затем **RM-01**. После них первые
предметные исправления — **RM-03 (Sayo)**, **RM-04 (SparkLink)** и
**RM-05 (свежесть producer/output)**, используя fake transport и управляемые часы.
Они имеют приоритет перед косметическим разделением файлов и оптимизацией UI.

Уже выполненная работа текущего запроса: расширенный source review, новая
инвентаризация, 37 карточек с 185 шагами, зависимости, protocol matrix,
проверки результата, правила безопасности и реестр глубины проверки.
Ни одна карточка реализации этим фактом не закрыта.

## 16. Проверка самого документа

При подготовке проверены 37 уникальных RM ID, существование всех dependencies
и отсутствие циклов; 185 шагов и 17 protocol cards присутствуют. Явные H/P/T
ссылки на исходники существуют (пример H/file.cpp намеренно является обозначением).
Все 234 hashes инвентаризации совпали с файлами после составления документа;
production EXE сохранил hash 44CFC0793A9CFD2A74B2B677AD8704708B1E16C6CD764EA8852C14962ECBE012.
Это проверка целостности Roadmap, не функциональные тесты HallJoy.
Навигационные документы перед обновлением скопированы с проверкой hashes в
.analysis/backups/full_audit_roadmap_20260906_114403.


## 17. Перепроверка продуктового замысла — 2026-09-06

Версия 1.1 исправляет рекомендации, а не production код. RM-03 переписан под
подтверждённое владельцем автоматическое сопоставление букв; добавлена проверка
уже существующего addedCount==1 и межсобытийной неоднозначности. Убраны указания
на ручную настройку, безбуквенную замену и запрет auto-learning как класса.
Для остальных архитектурных предложений введён обязательный Purpose/compatibility
этап; изменения продукта не выводятся автоматически из наличия fallback/mutex.
Прежние инженерные запреты по Sayo уточнены в навигации и owning документах.
Backup: .analysis/backups/product_intent_review_20260906_115731.

## 18. Новая фича в конце Roadmap — неподдерживаемая клавиатура и Discord

### RM-37 — Определение неподдерживаемой клавиатуры и ссылки на сообщество

- [~] Статус: IN PROGRESS. Тип: новая фича по запросу владельца, не найденный баг.
  Плановое место: после основной программы аудита/стабилизации. Зависимости:
  RM-00 и RM-29 для актуального каталога; RM-27 при изменении общего UI.

**Пользовательский результат:** HallJoy определяет, что подключённая клавиатура
не поддерживается текущей версией, понятно сообщает об этом и предлагает
вступить в Discord-сообщество, где можно попросить добавить поддержку.
Независимо от этого сообщения в интерфейсе всегда доступна ссылка на Discord.

**Где работать:** H/native_analog_backend_registry.cpp;
H/native_analog_routing.cpp; H/analog_host_client.cpp; H/backend.cpp;
H/keyboard_subpages.cpp; H/app.cpp; README.md; SUPPORTED_HARDWARE.md.
Точные UI-файлы определить по выбранному месту ссылки, не добавлять второй
механизм определения устройств рядом с существующим каталогом.

**Недостающие данные:** в просмотренных README/SUPPORTED_HARDWARE указан личный
контакт pash.ok, но приглашение на сервер сообщества не найдено. Перед реализацией
кнопок спросить владельца точный официальный invite URL; не придумывать адрес
и не подменять сообщество личным контактом. Запрос URL не блокирует проектирование
или fake tests; готовую функцию без действительной ссылки не объявлять завершённой.

**Шаги:**

1. Найти существующий результат discovery/proof и определить состояния: поиск ещё идёт, устройство не найдено, поддерживаемое устройство временно недоступно, неизвестная модель/вариант, подтверждённо неподдерживаемая клавиатура. Нулевой аналог, отпущенные клавиши, UAP startup failure или отсутствующий ViGEmBus не являются доказательством неподдерживаемой клавиатуры.
2. Определять статус по конкретному устройству после завершения discovery, используя имеющиеся безопасные identity/capability данные. Не расширять HID probing и не отправлять неизвестные vendor commands ради этого сообщения. При недостаточных данных писать «не удалось определить поддержку», а не категоричное «не поддерживается». Если неясно, как трактовать конкретный сценарий, спросить владельца по правилу раздела 0.1.
3. Добавить спокойное сообщение в списке/статусе устройств. Предлагаемый текст: «Эта клавиатура пока не поддерживается текущей версией HallJoy. Вступите в наше Discord-сообщество — там можно попросить добавить поддержку». Рядом кнопка «Вступить в Discord». Текст для неизвестного статуса должен отличаться от подтверждённого unsupported.
4. Не повторять приглашение на каждом тике или reconnect: сообщение обновляется по состоянию устройства и не мешает работе поддерживаемой клавиатуры, если подключено несколько устройств. Не показывать предупреждение обо всех обычных HID-интерфейсах мыши/клавиатуры. Политику отдельного popup и повторных уведомлений согласовать, если она спорна; по умолчанию планировать неблокирующую строку/карточку статуса.
5. Добавить постоянный пункт «Discord-сообщество» в доступном месте интерфейса, например «Помощь» или «О программе», независимо от наличия устройств и ошибок. Использовать один общий официальный URL для постоянной ссылки и кнопки в сообщении; точное размещение согласовать при неоднозначности текущего UI.
6. Открывать приглашение в браузере только по нажатию пользователя. Не вступать на сервер, не отправлять заявку, логи или данные устройства автоматически. При ошибке открытия дать понятное сообщение и возможность скопировать ссылку. Не проверять поддержку через сетевой запрос в критическом пути ввода.
7. Добавить fake discovery/UI-model проверки: поддерживаемое устройство, неизвестное, неподдерживаемое, отсутствующее; pending discovery, временный отказ backend, два устройства разных статусов, переподключение. Проверить доступность постоянной ссылки, единый URL и отсутствие повторного спама. Реальный игровой ввод для этих тестов не нужен.
8. Обновить справку и текущие UI-скриншоты/описания при наличии; записать подтверждённый URL, правила показа и результаты. Если фича входит в release candidate, выполнить её проверки до финальной RM-34 квалификации; изменение EXE после квалификации требует повторения применимых gates. Размещение задачи в конце документа не разрешает модифицировать уже квалифицированный пакет без проверки.

**Результат 2026-09-06 (не финальный):** после фактического Raw Input key-down
именно от клавиатуры HallJoy ждёт 250 ms появления аналогового значения в своём
потоке. Если значения нет, diagnostics показывает, что физическое нажатие
получено, но аналога HallJoy не получил, и предлагает Discord. Мышь, в том числе
с боковыми кнопками, забинженными в буквы, не участвует: это `RIM_TYPEMOUSE`.
Механизм не использует HID/VID/PID, не открывает интерфейс и не отправляет
команды. В UI остаётся Discord-плейсхолдер без выдуманного URL. AULA HERO84 HE и
ROG Azoth 96 HE остаются frozen и не поддерживаются; механизм не активирует их
диагностические ветки. Подробности: `RM37_KEYBOARD_SUPPORT_STATUS_2026-09-06.md`.

**Готово, когда:** статус не путает unsupported с временной ошибкой/отсутствием
нажатий; для неподдерживаемой клавиатуры видны объяснение и приглашение; постоянная
ссылка в UI доступна всегда; обе ведут на подтверждённое владельцем сообщество;
уведомления не мешают вводу и не повторяются бесконтрольно; fake tests проходят.

**Не делать:** определять отсутствие поддержки только по нулевым значениям или
одному таймауту; обещать сроки добавления устройства; автоматически слать что-либо
в Discord; придумывать invite; заставлять пользователя вступать для работы
с уже поддерживаемой клавиатурой.

#### RM-37 update — 2026-09-06 (supersedes the earlier Raw Input draft)

The Raw Input/key-press correlation draft was deliberately removed: it could
not establish that a keyboard is analogue, and the product must not infer that
from keyboard activity. The implemented UI is instead a non-blocking red banner
between the keyboard preview and sub-tabs. It is visible precisely when current
HallJoy native/UAP telemetry has no connected supported analogue source; it is
hidden as soon as such a source is reported. The update uses no HID identity,
Raw Input, vendor command, calibration control, or input suppression.

The banner says `Supported analogue keyboard not detected`, asks the user to
help add support through Discord, and visibly reserves Discord, copy-link, and
QR controls. All three are disabled placeholders until the owner provides the
official invite, so no URL or QR payload is fabricated. Its state is updated
only on a telemetry transition, not on every tick. Details and verification are
kept in `RM37_KEYBOARD_SUPPORT_STATUS_2026-09-06.md`.

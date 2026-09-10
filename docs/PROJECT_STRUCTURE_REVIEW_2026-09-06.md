# Структура HallJoy: инвентаризация и план упорядочивания — 2026-09-06

## 2026-09-06 — Статус после разрешения владельца

Этот документ сохраняет исходное обследование. Актуальная структура находится
в [PROJECT_LAYOUT](current/PROJECT_LAYOUT.md), фактический итог исполнения FS-00..05 —
в [STRUCTURE_MIGRATION](validation/STRUCTURE_MIGRATION_2026-09-06.md).
Слова «пока не переносили» ниже описывают момент обследования. Важное уточнение:
wooting_analog_common.lib/.a — входные зависимости, подключаемые main.cpp через
#pragma comment; они сохранены. Новые сборки не должны восстанавливать старую структуру.


## Результат

Да, есть сборочный мусор и структурный долг. Основная проблема — не размер C++
файлов, а перемешивание исходников, generated outputs, старых запусков, доказательств
и важных документов. Чистка диска и перестройка source tree — разные пакеты.

В этом обзоре ничего не удалено/перемещено, сборка и игровой ввод не запускались.
Размеры получены перечислением файлов до создания этого отчёта и новых backups;
это логический размер содержимого, не точное число освобождаемых кластеров.
Хеши всех файлов проекта на предмет глобального дублирования не вычислялись.

## Измерения

Всего на момент замера: **4707 файлов, 1840,57 MiB (~1,80 GiB)**.

| Область | Файлов | MiB | Что это означает |
|---|---:|---:|---|
| src | 1473 | 1412,99 | основная масса — вложенные результаты сборки |
| .analysis | 910 | 191,01 | backups, evidence, диагностические материалы и test binaries |
| third_party | 1544 | 138,64 | авторские overlays вместе с downloaded/build зависимостями |
| build | 106 | 58,99 | current release, diagnostic packages и evidence |
| _backups | 321 | 24,82 | старые snapshots, не автоматически мусор |
| docs | 275 | 3,79 | невелики по объёму, неоднозначны по навигации |
| _build_checks | 5 | 0,66 | локальные результаты проверок, требуется классификация |
| ._backups | 6 | 0,12 | ещё одно место хранения резервных копий |

Три непересекающихся output root внутри src:
- src/HallJoyProject/x64 — 1255,49 MiB, 954 файла.
- src/HallJoyProject/HallJoy/x64 — 21,88 MiB, 3 файла.
- src/HallJoyProject/HallJoy/HallJoy/x64 — 123,40 MiB, 109 файлов.
  Во вложенной HallJoy/HallJoy при просмотре найден только x64, а не второй source tree.

**927 промежуточных файлов, 1130,35 MiB (~1,10 GiB)** в этих трёх roots имеют
расширения obj/iobj/ipdb/tlog/res/idb/ilk/pch. Это кандидаты на удаление после
подтверждения, что сборка не идёт и они воспроизводимы. В эту оценку НЕ включены
PDB/MAP, EXE/DLL, настройки, архивы backups и результаты исследований.

В целом по проекту obj занимают 806,55 MiB, iobj — 195,79 MiB, PDB — 207,05 MiB;
эти общие числа включают разные области и не должны суммироваться с кандидатами
как ещё один независимый объём экономии.

## Конкретные проблемы

### 1. Корень содержит результаты запусков и компиляции

В корне находятся:
- HallJoy.exe — старый hash E71EDB6BA9734F6019F5C22EAF48ED6DD299EA91D1F00E4D564705D66312C3D5;
- build/release/HallJoy.exe — текущий hash 44CFC0793A9CFD2A74B2B677AD8704708B1E16C6CD764EA8852C14962ECBE012;
- HallJoy.log (~0,46 MiB), HallJoyUniversalAnalogHost.dll;
- aula_hero84he_diagnostic_protocol.obj и *_test.obj.

Старый root EXE легко запустить вместо текущего. OBJ — обычные промежуточные
результаты. Лог и EXE сначала сопоставить с evidence/пользовательским запуском:
возраст сам по себе не даёт права удалить единственный rollback artifact.

### 2. Сборки разнесены по нескольким деревьям

В x64 есть AnalogSimulator, ProviderV2Qualification, MAD68ProRNative,
AulaAggressiveTrace, Hero84, MCHOSE, Titan68 и другие конфигурации.
Наличие разных diagnostic builds оправдано. Неоправданно смешивать их выходы
с исходниками и оставлять старые вложенные x64 без единого источника истины.

Target: один build tree с разделением по configuration, а не одна общая папка,
где ordinary и diagnostic EXE перезаписывают друг друга.

### 3. Evidence, cache и backups перемешаны

.analysis/backups (~140 MiB), _backups, ._backups, docs/backups, build/evidence,
docs/stability и .analysis/gravastar_v75 решают разные задачи, но это плохо видно
из названия/расположения. Автор этого аудита тоже добавлял локальные backups и
разделяет ответственность за их накопление.

Политика:
- воспроизводимые временные файлы можно очищать;
- evidence привязывать к artifact hash/run ID и сохранять;
- snapshots хранить с manifest, причиной и rollback boundary;
- research captures могут быть единственным proof — не считать мусором.

### 4. Важные проектные документы спрятаны внутри src

В src/HallJoyProject есть SAYO_DEVICE_NOTES.md, CUSTOM_UI_ARCHITECTURE.md,
INPUT_OVERLAY_NOTES.md, PERFORMANCE_AND_STABILITY_GOALS.md, TESTING.md,
SOURCE_AUDIT_V6.md и другие документы.

**SAYO_DEVICE_NOTES.md от 2026-05-14 прямо описывает намеренное сопоставление
physical index с пользовательским keyboard HID и временный fallback F/G/H.**
При прежнем аудите автор просмотрел docs и код, но пропустил этот документ.
Это реальный пример того, как плохая навигация ведёт к неверному толкованию замысла.

Документ также описывает три depth offsets 8/10/12, PID 8089:0009 и captured scale
около 4000. Ссылки на исходные pcapng используют placeholder capture-directory:
найденные notes — evidence замысла, но не новый просмотр самих captures.

Нужно включить этот источник в RM-03 и индекс docs. Не переносить notes без
переадресации старых ссылок и сохранения даты/границы доказательства.

### 5. Дубликаты текста бывают разными

Root LICENSE и src/HallJoyProject/LICENSE совпали по SHA-256.
Root COMMERCIAL_LICENSE.md и вложенный совпали.
README.md и CONTRIBUTING.md в корне и src/HallJoyProject **различаются**.

Поэтому «оставить root и удалить всё одноимённое» потеряет содержание.
Даже byte-identical LICENSE может требоваться в отдельно распространяемом
source/dependency package; сначала проверить packaging references.

### 6. third_party совмещает поддерживаемые входы и generated cache

В UniversalAnalogPluginFixed:
- main.cpp, halljoy_*.h, overlay/, .sun recipes, tools/, lock и licences —
  поддерживаемые исходники/входы;
- Soup (~60,66 MiB) и .build-tools (~43,09 MiB) используются/создаются официальным
  build_fixed_plugin.ps1 из pinned источников и overlays;
- dist, int — outputs/кандидаты после проверки recipes;
- wooting_analog_common.a (~21,73 MiB) и .lib (~11,39 MiB) нельзя удалять только
  потому, что текущий верхний build script не упоминает их напрямую: проверить
  также .sun, build.bat/build.sh и alternative builds.

Deleted cache может потребовать сеть для восстановления; при offline работе
его сохранение — осмысленный выбор. Авторские overlays не равны downloaded Soup.

### 7. .gitignore не является политикой чистоты файловой системы

Игнорируются build/x64/binaries и часть generated dependencies, но root patterns
не покрывают все .analysis, _backups, ._backups, _build_checks.
Даже полный .gitignore ничего не удалит и не поможет выбрать правильный EXE.
В этой копии действуют локальные правила не использовать Git; его не запускали.

### 8. История docs выглядит как несколько current состояний

docs/current, docs/v1.4, docs/development, docs/stability и вложенные source notes
не имеют одного существовавшего верхнего docs/README.md. В handoff/worklog уже
добавлено много последовательных уточнений. Нужен короткий текущий индекс,
а история должна оставаться историей, не конкурирующим нормативным документом.

В рамках этого обзора добавлен docs/README.md со ссылками на реально существующие
источники, без массового переноса и потери старых путей.

## Предлагаемая структура

Первый этап сохраняет source paths и меняет только организацию generated outputs:

```text
HallJoy-main/
  README.md, BUILD.cmd, LICENSE, CONTRIBUTING.md, ...
  .github/
  src/HallJoyProject/
    HallJoy/                  текущие .cpp/.h/.inc, vcxproj, resources
    tests/                    текущие production-linked тесты
    third_party/              пока сохраняем пути SDK/lib
    HallJoy.sln
  third_party/UniversalAnalogPluginFixed/
    main.cpp, halljoy_*.h, overlay/, tools/, recipes, licences
  tools/                      основные entrypoints + совместимые wrappers
  docs/
    README.md                 единая точка входа
    current/                  короткий current index/контракты, не вся история
    protocols/                исходники знаний о wire/map/scale
    research/                 неподтверждённое, firmware/captures provenance
    validation/               индекс evidence, не смесь исполняемых файлов
    archive/                  исторические документы со ссылками
  build/
    obj/<configuration>/      воспроизводимые intermediates
    bin/<configuration>/      EXE/DLL/PDB/MAP для каждой конфигурации
    release/                  единственный пакет для передачи
    output/                   compatibility staging, пока есть consumers
    evidence/<run-id>/        logs, hashes, test verdicts
  .cache/                     downloaded/generated build dependencies
  .local/backups/<timestamp>/ локальные rollback snapshots + manifests
```

.cache и .local — предлагаемые каталоги, в этой сессии не создавались.
build/output не удалять, пока tools/CI используют его.
Переезд Soup/.build-tools в .cache требует изменения bootstrap и проверок:
это отдельный пакет, а не ручное перемещение папки.

Второй, необязательный этап после стабилизации build layout:
src/app, src/core, src/providers, src/platform/windows, src/ui, tests/, resources/.
Он требует согласованного изменения vcxproj, include paths, scripts и source-token
audits. Сам по себе новый layout не повышает скорость/стабильность runtime.
Не совмещать переименование файлов с изменением protocol/ownership/UX.

## Пошаговое исполнение

### FS-00 — Manifest и политика хранения

1. Снять файл -> hash/size/class -> consumers -> preserve/rebuild/archive manifest.
2. Проверить активные builds/processes и пользовательские portable data.
3. Назначить current artifact и сохранить matching PDB/MAP, нужные для crash analysis.
4. Уточнить спорную ценность старых captures/backups у владельца; не удалять по возрасту.
5. Зафиксировать planned source/destination для каждого переноса. Для удаления
   Windows paths должны быть resolved внутри утверждённой области.

### FS-01 — Только воспроизводимые intermediates

1. Проверить перечисленные 927 файлов и текущий build recipe для их воспроизведения.
2. Сначала dry-run список точных файлов/размеров, без wildcard cleanup всего src.
3. Чистить только выбранные generated intermediates после проверки ownership.
4. Сохранить EXE/PDB/MAP, evidence и настройки отдельными категориями.
5. Новый build должен воспроизвести intermediates; не запускать output-capable
   smoke без разрешённого режима проверки.

### FS-02 — Единый build tree

1. Найти все OutDir/IntDir, relative output paths, copied resources и runner defaults.
2. Согласованно направить конфигурации в build/obj и build/bin; не смешивать их.
3. Обновить tools, CI artifact paths и preflight assertions; старые entrypoints
   временно оставить wrappers, если ими пользуются.
4. Проверить ordinary + representative diagnostic build без использования старых obj.
5. Только после этого архивировать старые вложенные x64; проверять preserved data.

### FS-03 — Документы и evidence

1. Сначала индекс, затем класификация current/history/protocol/research/evidence.
2. Прочитать и сопоставить src/HallJoyProject/*.md, а не удалить дубликаты по имени.
3. Из SAYO notes и аналогичных документов сохранить замысел и ссылки на captures.
4. Переезды сопровождать таблицей old -> new, link checker и короткими redirect notes.
5. Свести current state в один короткий документ; длинные logs/handoffs сохранить
   в истории. Выбрать один main worklog, не копировать всё в каждую папку.

### FS-04 — Dependencies и локальные архивы

1. Отделить pinned editable overlays от скачиваемого/генерируемого.
2. Для candidate .a/.lib/old DLL найти consumers во всех recipes, не только ordinary.
3. Проверить воспроизводимый bootstrap перед переносом cache.
4. Объединить новые backups под одним root; старые переносить с manifest/hash
   и коррекцией ссылок, а не удалять.
5. Установить retention по ценности: known-good rollback и unique evidence сохраняются.

### FS-05 — Закрытие структурного пакета

1. Проверить source/resource/include closure, ordinary и diagnostic output.
2. Проверить документационные ссылки, tools paths и exact artifact manifest.
3. Сравнить до/после файлы настроек/исходники, не затронутые пакетом.
4. Записать реально освобождённое место отдельно от перемещённого/архивированного.
5. Обновить Roadmap RM-00/RM-32/RM-36 и handoff. Discord RM-37 остаётся последней
   feature-карточкой основного плана; этот structural report её не перемещает.

## Граница текущего результата

Выполнены чтение/размеры/выборочные hashes и подготовка документации.
Не выполнены удаление, source moves, исправления build recipes, clean rebuild,
проверка всех binary duplicates и поиск оригинальных pcapng.

# HallJoy — актуальная структура проекта

Обновлено 2026-09-06. Этот документ задаёт **текущие пути**. Датированные отчёты,
хеши и команды в старых worklog/handoff относятся к зафиксированным там сборкам.
Основной план работ: [полный Roadmap](../v1.4/FULL_AUDIT_EXECUTION_ROADMAP_2026-09-06.md).
История выполнения: [WORKLOG](../v1.4/WORKLOG.md). Начальная навигация: [docs/README](../README.md).

## Сборка

Из корня: `BUILD.cmd`, либо
`powershell -NoProfile -ExecutionPolicy Bypass -File tools/build.ps1`.

**Единственный обычный пакет: `build/release/HallJoy.exe`.**
Не искать актуальный EXE в корне, старых x64 или архивах. Само наличие EXE не
означает прохождение всех release/hardware gates. Результат текущей перестройки
и хеш находятся в [отчёте проверки](../validation/STRUCTURE_MIGRATION_2026-09-06.md).

| Путь | Назначение |
|---|---|
| build/release/ | EXE, dependency-lock.json, THIRD_PARTY_NOTICES.md, SHA256SUMS.txt |
| build/bin/<variant>/<configuration>/<platform>/ | Результат компиляции HallJoy и соответствующие PDB/MAP |
| build/obj/<variant>/<configuration>/<platform>/ | Объектные файлы и данные MSBuild |
| build/bin/UAP/<native или standard>/ | ABI0/ABI1 и архив отдельно собираемого плагина |
| build/obj/UAP/<native или standard>/ | Рабочая копия исходников плагина и Soup для Sun |
| build/obj/portable-tests/<run>/ | Временные компиляции portable/sanitizer tests; очищаются после прогона |
| build/runtime/ | DLL плагина для встраивания ресурсным компилятором |
| build/packages/ | Только именованные диагностические пакеты |
| build/evidence/<run>/ | Логи, измерения, отчёты и хеши |
| .cache/uap/Soup/ | Закреплённая зависимость с проверяемыми overlays |
| .cache/uap/build-tools/ | Закреплённый Sun и его bootstrap |
| .local/backups/ | Резервные копии, импортированные старые бекапы, старые сборки |

Обычный variant: `MAD68ProRNative`; configuration: `Release`; platform: `x64`.
Прямой Debug/Release без специальных флагов использует `Standard`.
Диагностические и Simulator variants из vcxproj сохраняются. Исторический
`HallJoyDiagnostic` отличается от обычного `HallJoy`; не выдавать его как релиз.

`build/release` больше не копируется из второго staging-каталога. Скрипт заменяет
только свои четыре файла и не удаляет пользовательские профили/неизвестные файлы.
Для распространения брать эти четыре файла; личные данные не включать в архив.
У PDB/MAP одна рабочая копия рядом с соответствующим EXE в build/bin.

Все официальные пути вычисляются относительно расположения скрипта/проекта.
Не задавать ручные OutDir/IntDir вне build. Исходники и SDK include/lib пути
сохранены. wooting_analog_common.lib/.a в third_party — обязательные входы линкера плагина,
а не результаты текущей сборки. Не удалять их как мусор.
DLL в build/runtime — вход ресурсной сборки: сначала BUILD.cmd;
изолированные diagnostic scripts требуют уже подготовленный runtime.
Windows x64 остаётся целевой платформой HallJoy; plugin build.bat направляет
к PowerShell entrypoint. Upstream Linux build.sh сохраняет обе прежние группы
ABI/flavours в build/bin/UAP/linux и рабочие деревья в build/obj/UAP/linux.
Его Linux runtime/компиляция в этой Windows-сессии не проверялись.

## Документация

- docs/current/ — короткие действующие контракты и указатели на текущий результат.
- docs/development/ — сборка, тестирование, UI и цели производительности.
- docs/protocols/ — знания об устройствах; [Sayo O3C](../protocols/SAYO_DEVICE_NOTES.md).
- docs/research/, docs/firmware/, docs/hex80-reference/ — исследования и материалы.
- docs/validation/, docs/stability/ — отчёты и сохранённые измерения.
- docs/archive/ — явно исторические документы, в том числе source-v6.
- docs/v1.4/ — существующий корпус аудита, главный Roadmap и основной WORKLOG.
  Даты/старые «current» в именах не делают исторический hash актуальным.
- docs/backups/ — исторические **описания** контрольных точек, не хранилище копий.

Из src/HallJoyProject перенесены десять документов; ещё шесть старых материалов
из docs/current распределены по archive, protocols и validation. На прежних местах оставлены
короткие ссылки, поэтому старые внешние ссылки не теряют документ.
README рядом с solution описывает именно дерево проекта; корневой README —
продукт. LICENSE и COMMERCIAL_LICENSE оставлены в обоих лицензионных контекстах;
это намеренные юридические файлы, а не мусор.

Пути к исходникам, записанные внутри старых source notes как HallJoy/foo.cpp,
исторически относились к src/HallJoyProject. При чтении применять этот корень.
Старые команды V6/build_all.ps1 не являются текущей инструкцией запуска.
Новые решения вести в DECISIONS, историю — в WORKLOG, текущие пути — здесь;
не размножать новые полные worklog по папкам.

## Бекапы и переносы

Перед широким изменением создавать `.local/backups/<дата-время>_<задача>/`.
Сохранять исходные относительные пути, SHA-256, размеры, назначение копии и способ
отката. Перед каждой записью перечитать/проверить хеш цели. Не затирать чужую правку.
Не удалять копии по возрасту: known-good EXE и matching PDB/MAP, профили,
уникальные captures/evidence сохраняются до отдельного решения владельца.

Полная контрольная копия этой миграции:
`.local/backups/structure_20260906_122303/snapshot/`, 4712 файлов;
`manifest.csv` содержит исходные пути, размеры и SHA-256.
Журналы `moves-Dependencies.csv`, `moves-Documents.csv`, `moves-Archives.csv`
рядом с ней связывают каждый перенесённый файл с новым адресом и хешем.
Одноразовый migrate-layout.ps1 — запись этой операции, не универсальная чистилка.
Не запускать его повторно: он отказывается перезаписывать назначения.

| Старое место | Новое место |
|---|---|
| .analysis/backups/ | .local/backups/imported/analysis/ |
| _backups/ | .local/backups/imported/underscore/ |
| ._backups/ | .local/backups/imported/dot-underscore/ |
| .analysis/ (остальное) | build/evidence/legacy-analysis/ |
| _build_checks/ | build/evidence/legacy-build-checks/ |
| src/HallJoyProject/x64/ | .local/backups/legacy-builds/project-x64/ |
| src/HallJoyProject/HallJoy/x64/ | .local/backups/legacy-builds/application-x64/ |
| src/HallJoyProject/HallJoy/HallJoy/ | .local/backups/legacy-builds/nested-application/ |
| build/output/ | .local/backups/legacy-builds/output/ |
| Старые EXE/DLL в корне | .local/backups/legacy-builds/root/ |
| Старые диагностические пакеты | .local/backups/legacy-builds/<имя>/ |
| third_party/UniversalAnalogPluginFixed/dist, int | .local/backups/legacy-builds/uap-dist, uap-int |

Остальные точные соответствия — в CSV. Исторические пути в отчётах не заменяются
массово: они являются частью происхождения evidence. Чтобы восстановить файл,
найти его Old в журнале и проверить SHA256 у New; откат выполнять адресно в
закрытом приложении. Не копировать весь snapshot поверх новых исходников вслепую.

## Границы работы

Перестройка каталогов не меняет Sayo learning, сопоставление букв, аппаратные
протоколы, fallback или UX. При спорном ожидаемом поведении спрашивать владельца.
Автоматическое обучение Sayo сохраняется без ручных назначений. Discord RM-37
остаётся последней feature-карточкой Roadmap и пока не реализован.

Во время этой работы запрещены тесты с клавиатурным/геймпадным выводом:
пользователь играет. Компиляция, статические и изолированные portable проверки
допустимы. Обычные smoke/simulator/ViGEm scripts не запускать без разрешённого
режима; даже диагностический build script может сам запускать runtime gates.

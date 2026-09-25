# HallJoy — актуальная структура проекта

Обновлено 2026-09-22. Этот документ задаёт **текущие пути**. Датированные отчёты,
хеши и команды в старых worklog/handoff относятся к зафиксированным там сборкам.
Основной план работ: [полный Roadmap](../v1.4/FULL_AUDIT_EXECUTION_ROADMAP_2026-09-06.md).
История выполнения: [WORKLOG](../v1.4/WORKLOG.md). Начальная навигация: [docs/README](../README.md).

## Repository root and public documents (2026-09-22)

- `README.md` — project overview and the two keyboard lists (one paragraph per brand).
- `LICENSE` and `THIRD_PARTY_NOTICES.md` — root legal notices; the latter is
  consumed by build/package scripts and shipped with releases.
- `BUILD.cmd`, `AGENTS.md` and dotfiles — build entry point and repository rules.
- `docs/releases/` — all versioned release notes, indexed by `README.md`.
- `docs/SUPPORTED_HARDWARE.md` — detailed support evidence and inventory.
- `docs/legal/COMMERCIAL_LICENSE.md` — commercial license terms.
- `.github/CONTRIBUTING.md` — contribution guidance in GitHub's conventional path.

Create future release notes under `docs/releases/`; do not accumulate them in
root. Keep local logs and stale release drafts under `.local/`, not in the public
root. Existing tagged release trees/assets are historical and remain unchanged.
The Pwnage correspondence is local-only and must not enter a publication mirror.
This reorganization changes paths/navigation only, not keyboard support or EXE.
Backup: `.local/backups/root-structure-1790061701.zip`.

## Build and replacement workflow (2026-09-19)

Use `tools/build_release.ps1` for an incremental ordinary build with the existing
pinned runtime. `BUILD.cmd` / `tools/build.ps1` retain the full dependency and
release-check workflow. Both stage the EXE under `build/obj/ReleaseCandidate/x64`
and use the same final atomic replacement helper.

**Ordinary delivery: `build/bin/Release/x64/HallJoy.exe`.**
The historical `build/release` path is superseded. Internal candidate files and
backups are build artifacts, not additional distribution packages.

Do not close HallJoy before reading code, editing or compiling a candidate.
The old app stays running through compilation and linked-image validation.
`tools/publish_halljoy_build.ps1` then checks the candidate hash; if unchanged,
it leaves the process untouched. Otherwise it closes only the exact destination
EXE in the current session, atomically replaces it, and restores the app only if
it was previously running. An atomic replacement failure keeps the old file and
restores that app. Never call the closing helper as a generic preparation step.

Raw MSBuild no longer closes processes. Use the staged wrapper when delivering
a new ordinary EXE. File-only simulator profile tests have their own ownership
namespace keyed by the isolated root; they must not close the interactive app.
Interactive/device tests still require their explicit environment contract.

| Path | Purpose |
|---|---|
| build/bin/Release/x64/ | Ordinary delivery; full build also updates notices/hash manifest |
| build/obj/ReleaseCandidate/x64/ | Candidate EXE and its matching PDB/MAP before replacement |
| build/obj/replacement-backups/ | Previous EXEs retained by atomic replacement |
| build/bin/<variant>/<configuration>/<platform>/ | Isolated diagnostic/simulator targets |
| build/obj/<variant>/<configuration>/<platform>/ | Compiler objects and MSBuild data |
| build/bin/UAP/<native or standard>/ | Rebuilt plugin ABI variants |
| build/runtime/ | Pinned DLL inputs embedded into the EXE |

All paths are checkout-relative. Packaging does not remove user profiles or
unknown files. Runtime DLLs and SDK import libraries remain required inputs.
See [replacement lifecycle evidence](BUILD_REPLACEMENT_LIFECYCLE_2026-09-19.md).

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

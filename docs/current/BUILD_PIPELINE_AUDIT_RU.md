# HallJoy MAD68 Pro R — аудит и переработка build-chain v3.5.0

## Что произошло в v3.4.3

Ошибка не была ошибкой C++ или патча MAD68.

Патч Soup завершился и вывел:

```text
Soup Madlions SafeHID fix v7 applied
```

Затем `git diff --check` написал в stderr предупреждение о будущей конвертации LF в CRLF. Старый PowerShell-скрипт запускал native-команду напрямую при `$ErrorActionPreference = 'Stop'`. Windows PowerShell преобразовал строку stderr в `NativeCommandError` до того, как скрипт надёжно оценил настоящий exit code Git.

Поэтому команда с фактическим exit code 0 могла остановить сборку.

Раньше цепочка доходила дальше только потому, что конкретная native-команда не выдала безвредную строку в stderr. Дефект уже существовал, но не проявлялся на каждом компьютере и каждом наборе глобальных Git-настроек.

## Почему исправление одной строки было бы недостаточным

В старом пути обнаружены связанные проблемы:

1. Любой stderr от Git, Clang, MSBuild, Sun, `vswhere` или дочернего PowerShell мог стать исключением.
2. `$LASTEXITCODE` мог содержать код не той команды, особенно после вызова другого `.ps1`.
3. Soup скачивался и патчился внутри распакованной пользовательской папки.
4. Неудачная попытка оставляла частично изменённый worktree.
5. Глобальные `core.autocrlf`, pager и color Git влияли на сборку.
6. `git diff` мог открыть интерактивный `less`.
7. Смешанные LF/CRLF создавали ложные trailing-whitespace ошибки.
8. Sun ранее мог загружаться из изменяемой ветки и повторно использовать старый EXE.
9. Два параллельных запуска могли одновременно изменять общий cache.
10. Длинный путь папки Downloads увеличивал риск проблем Windows toolchain.
11. Старые DLL/EXE могли остаться от предыдущей попытки.
12. Собиралась ABI0 DLL, которую HallJoy не загружает и не встраивает.
13. Часть инструментов обнаруживалась уже после начала сетевых и файловых операций.
14. Наличие выходного файла проверялось слабее, чем его формат и архитектура.

## Новый путь v3.5.0

```text
BUILD_HALLJOY.cmd
  -> tools/build_halljoy.ps1
      -> полный preflight инструментов и исходников
      -> mutex одного процесса сборки
      -> изолированный cache в LocalAppData
      -> отдельная сборка UAP
          -> точная ревизия Soup
          -> reset + clean
          -> патч пяти ожидаемых файлов
          -> проверка LF
          -> проверка точного списка diff
          -> git diff --check без pager
          -> точный tag Sun
          -> reset + clean + восстановление submodules
          -> новая сборка Sun
          -> сборка только используемого ABI1 plugin
          -> проверка DLL как x64 PE
      -> чистая копия HallJoyProject в короткий workspace
      -> копирование ABI1 DLL в runtime resource path
      -> MSBuild HallJoy с MAD68 native
      -> проверка EXE как x64 PE
      -> новый чистый BUILD_OUTPUT
```

## Native process runner

Все native-программы теперь запускаются одной функцией `Invoke-HJNative` через `Start-Process -Wait -PassThru`.

Решение об успехе принимается по:

```text
System.Diagnostics.Process.ExitCode
```

stderr сохраняется как диагностический текст, но сам по себе не означает неудачу.

Прямые вызовы `& git`, `& clang`, `& msbuild`, `& Sun` и зависимость от `$LASTEXITCODE` удалены.

## Изоляция Git

Каждая Git-команда получает локальную политику:

```text
core.autocrlf=false
core.eol=lf
core.safecrlf=false
core.pager=cat
pager.diff=false
color.ui=false
advice.detachedHead=false
GIT_PAGER=cat
GIT_TERMINAL_PROMPT=0
```

Глобальный пользовательский `core.autocrlf=true` больше не определяет результат проверки.

## Изоляция файлов

Исходный архив не является рабочим каталогом компилятора.

Используется:

```text
%LOCALAPPDATA%\HallJoyBuildCache\MAD68ProR-v3.5.0
```

Soup и HallJoy собираются в отдельных временных деревьях. Новый запуск очищает результаты предыдущей попытки. Исходная распакованная папка остаётся неизменной, кроме `BUILD_LOG.txt` и `BUILD_OUTPUT`.

## Soup

Закреплена ревизия:

```text
b02796b0b20276277c8a4b4d3759643eeab43ff7
```

Перед каждым патчем выполняются `reset --hard` и `clean -ffdx`. После патча должны измениться ровно пять известных файлов. Каждый из них проверяется на отсутствие CR, затем выполняется `git diff --check`.

## Sun

Используется tag `0.5.0`. Даже при наличии cache Sun перед каждой сборкой восстанавливается через reset/clean, принудительно восстанавливает submodules и заново компилируется. Старый `Sun.exe` не принимается как готовый результат.

## Почему убрана ABI0

HallJoy извлекает и загружает ресурс `IDR_UAP_ABIV1`. ABI0 DLL в runtime-пути не используется.

Поэтому v3.5.0 собирает только:

```text
abiv1-pluswooting-mad68native.dll -> abiv1.dll
```

Это не удаление функции HallJoy. Это устранение лишнего независимого compile/link этапа, результат которого приложение не потребляло.

## Проверки результата

До MSBuild проверяются:

- все ссылки `ClCompile`, `ClInclude`, `ResourceCompile`, `Image` из vcxproj;
- точный размер и SHA-256 `ViGEmClient.lib`;
- точный размер и SHA-256 `wooting_analog_common.lib`;
- ABI1/Wooting/UAP flags;
- MAD68 pre-open exclusion;
- A8/A9 allow-list;
- Raw Input-before-worker порядок;
- наличие steady-state и per-key fallback защит.

После сборки:

- UAP DLL проверяется на DOS header, PE signature и machine `AMD64`;
- HallJoy EXE проверяется тем же способом;
- создаётся новый `BUILD_OUTPUT` и SHA-256 manifest.

## Статическая валидация в этой среде

```text
Protocol test: PASS — 68 descriptors, 67 HID keys
Runtime source identity against v3.4.3: PASS
Build pipeline checks: 38/38 PASS
vcxproj references: PASS
Critical library hashes: PASS
PowerShell lexical balance: PASS
One CMD only: PASS
```

Runtime MAD68, UI, remap и ViGEm исходники в v3.5.0 не изменялись относительно v3.4.3. Переработан только путь подготовки зависимостей и сборки.

## Что всё ещё может законно остановить сборку

- отсутствует Git, MSBuild, Clang или Windows SDK;
- не установлена C++ workload;
- первая загрузка GitHub недоступна после трёх попыток;
- антивирус или другой процесс удерживает выходной файл;
- компилятор действительно возвращает ненулевой exit code;
- созданный DLL/EXE отсутствует, слишком мал или не является x64 PE;
- скачанная/локальная ревизия не соответствует проверяемой структуре патча.

В этих случаях сборка должна завершиться с конкретным сообщением, а не из-за безвредной строки stderr.

## Ограничение проверки

Полный запуск MSVC/Windows SDK невозможно выполнить в текущей Linux-среде. Поэтому v3.5.0 прошёл статическую, протокольную, структурную и упаковочную проверку, но финальная Windows-компиляция всё равно является обязательной проверкой.

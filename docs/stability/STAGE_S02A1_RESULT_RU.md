# Результат S02A.1 — hotfix чистой сборки UAP/Soup

Дата: 30 июля 2026 г.  
Исходная точка: `HallJoy_v3_9_0_STABILITY_S02A_EXCEPTION_BARRIERS_CORE.zip`.  
Статус: завершён и подтверждён чистой Windows-сборкой и runtime smoke test.

## 1. Повод

Пользовательский Windows build подтвердил:

- bootstrap Sun завершился успешно, 0 warnings / 0 errors;
- закреплённая ревизия Soup `b02796b0b20276277c8a4b4d3759643eeab43ff7` была загружена;
- build остановился в `Apply-Soup-Madlions-Fix.ps1` до компиляции plugin DLL;
- исключение: `native analogue exclusion is not applied before CreateFileW`.

Runtime-код S02A в этой попытке ещё не компилировался и не запускался, поэтому Windows gate S02A не был завершён.

## 2. Корневая причина

PowerShell patcher объявлял маркер:

```text
HallJoy native analogue pre-open exclusion
```

и после формирования изменённого `hwHid.cpp` требовал найти этот маркер перед `hid.handle = CreateFileW`.

Однако `$preOpenBlock`, который реально вставляется в `hwHid.cpp`, не содержал строку маркера. Маркер существовал только в переменной и validation-коде самого `.ps1`. Поэтому свежая, ранее не патченная копия Soup детерминированно завершала patch step ошибкой.

Старые static audits давали ложноположительный PASS, потому что проверяли наличие текста маркера где-либо в PowerShell-файле, но не внутри генерируемого C++ here-string.

## 3. Исправление

Выполнено минимальное изменение build pipeline:

1. точный маркер добавлен в комментарий внутри `$preOpenBlock`;
2. post-patch validation использует `$preOpenMarker`, а не второй независимый строковый литерал;
3. добавлен `plugin_preopen_patch_contract_audit.py`;
4. новый audit извлекает here-string `$preOpenBlock` и требует, чтобы он содержал:
   - точный validation marker;
   - `UAP_EXCLUDE_HALLJOY_NATIVE`;
   - `HALLJOY_UAP_NATIVE_HID_IDS`;
   - pre-open `continue`;
   - insertion в начало тела enumeration loop;
5. audit моделирует итоговый порядок и подтверждает, что marker предшествует `hid.handle = CreateFileW`.

## 4. Граница изменений

Не изменены:

- HallJoy runtime `.cpp/.h`;
- exception barriers S02A;
- алгоритмы native protocol backends;
- Soup revision;
- Sun flags;
- plugin ABI;
- MSVC project files;
- timing, polling и shutdown behavior.

Изменён только patch generator и добавлена статическая regression-проверка.

## 5. Локальные проверки

- исходный S02A script: marker отсутствует внутри `$preOpenBlock` — defect reproduced;
- исправленный script: marker присутствует внутри `$preOpenBlock` — PASS;
- новый pre-open patch contract audit — PASS;
- полный common gate GCC — PASS;
- полный common gate Clang — PASS;
- Python optimized audit run — PASS;
- MSVC `.vcxproj`/`.filters` XML parse — PASS;
- `HallJoy.vcxproj` byte-equal исходному S02A;
- runtime source diff относительно S02A — 0 файлов.

После локальных проверок пользователь повторил сборку из чистого каталога на Windows. `BUILD.cmd` завершился успешно; HallJoy, ViGEm и overlay прошли smoke test без краша, зависания или замеченной регрессии.

## 6. Windows gate

Пользовательский gate от 30 июля 2026 г.:

- чистая распаковка S02A.1;
- полный `BUILD.cmd` — PASS;
- запуск HallJoy — PASS;
- ViGEm — PASS;
- overlay start/stop и shutdown — PASS;
- крашей, зависаний и функциональных регрессий не обнаружено.

Доказательство сохранено в `tests/S02A1_WINDOWS_GATE_2026-07-30.txt`. Build blocker закрыт.

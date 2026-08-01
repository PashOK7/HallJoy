# Технический аудит HallJoy Universal Analog v3.9.0

Дата аудита: 30 июля 2026 г.  
Объект: `HallJoy_UniversalAnalog_v3_9_0_EXTENSIBLE_FINAL_Source(1).zip`  
Тип проверки: статический аудит исходников, проверка состава пакета, запуск переносимых тестов, повторная сборка переносимых тестов с sanitizers.

## 1. Итог

Проект содержит сильную протокольную основу: чистые парсеры MAD68/Hex80, планировщики, централизованный каталог native-backend'ов, точный manifest пакета и рабочий Linux-контур переносимых тестов. Все доступные автоматические проверки завершились успешно.

При этом статус «production / final» пока слишком оптимистичен. Наиболее опасные дефекты находятся не в формулах нормализации и не в пакетных парсерах, а в управлении жизненным циклом потоков и Windows HANDLE:

- штатный shutdown в пяти местах использует `TerminateThread`;
- три backend'а после «ограниченного» ожидания всё равно способны сделать бесконечный `std::thread::join()`;
- контракт `stop() -> bool` фактически нарушается: несколько descriptor'ов всегда возвращают `true`;
- аналоговый host после неполного завершения допускает повторное использование глобального контекста, который ещё может читаться старыми потоками;
- есть небезопасный сценарий закрытия HID HANDLE из другого потока при активном stack-based `OVERLAPPED`;
- фоновые C++-потоки в основном не имеют верхней границы исключений, поэтому редкий `std::bad_alloc`, `std::system_error` или исключение библиотеки может вызвать `std::terminate` всего процесса;
- сохранение настроек часто сообщает успех, не проверяя записи, и может атомарно заменить хороший файл неполным временным;
- встроенный установщик зависимостей имеет TOCTOU/целостностные проблемы и может бессрочно заморозить UI.

**Рекомендуемый статус:** не считать v3.9.0 полностью отказоустойчивой production-сборкой до устранения P1-дефектов и выполнения Windows fault-injection/soak тестов.

## 2. Что было проверено

### 2.1. Целостность архива

- 286 ZIP-записей, опасных traversal-путей не обнаружено.
- `docs/validation/CLEAN_PACKAGE_MANIFEST.json` полностью совпадает с содержимым поставки:
  - 246 перечисленных файлов;
  - отсутствующих и лишних файлов нет;
  - размеры и SHA-256 совпадают;
  - общий размер перечисленных файлов: 39 383 138 байт.

### 2.2. Автоматические проверки

Команда:

```text
python tools/run_native_backend_checks.py --require-compiler
```

Результат: PASS.

Пройдены:

- 8 Python static audits;
- `addressed_poll_scheduler_test`;
- `hid_io_operation_lifecycle_test`;
- `vigem_output_scheduler_test`;
- `native_analog_backend_contract_test`;
- `hex80_protocol_test`;
- `mad68pr_protocol_test`.

Переносимые C++-тесты также были пересобраны и запущены с AddressSanitizer + UndefinedBehaviorSanitizer. Ошибок в проверяемых чистых компонентах не обнаружено.

### 2.3. Ограничения аудита

Среда проверки — Linux. Поэтому не выполнены:

- полная сборка MSVC/Windows executable;
- реальные вызовы HID, ViGEm, WinHTTP, WinTrust, Job Objects и Raw Input;
- тесты физического оборудования;
- Application Verifier, PageHeap, Driver Verifier;
- reconnect/unplug soak на реальных клавиатурах;
- проверка поведения при зависшем драйвере/файловой системе/антивирусном фильтре.

Следовательно, отсутствие sanitizer-ошибок в чистых тестах не доказывает безопасность Windows lifecycle-кода.

## 3. Приоритеты

| Приоритет | Смысл | Количество в отчёте |
|---|---|---:|
| P0 | Детерминированный немедленный blocker | 0 |
| P1 | Высокий риск краша, зависания, повреждения состояния или локальной подмены | 16 |
| P2 | Существенная надёжность, целостность данных, производительность, архитектурный долг | 21 |
| P3 | Документация, воспроизводимость, защитное программирование, низкий риск | 8 |

P0 не выставлен: найденные самые опасные ветки обычно требуют тайм-аута, сбоя ОС/драйвера, исключения, дисковой ошибки либо локальной гонки. Это не делает их допустимыми — это означает, что они редкие и плохо воспроизводимые.

---

# 4. P1 — исправить в первую очередь

## HJ-AUD-P1-001. `TerminateThread` используется как обычный механизм shutdown

**Доказательства:**

- `src/HallJoyProject/HallJoy/realtime_loop.cpp:395-400`
- `src/HallJoyProject/HallJoy/debug_log.cpp:375-385`
- `src/HallJoyProject/HallJoy/overlay_server.cpp:1131-1143`
- `src/HallJoyProject/HallJoy/backend_sparklink.inc:1197-1208`
- `src/HallJoyProject/HallJoy/backend_sayo.inc:550-575`

**Отказ:** `TerminateThread` прекращает выполнение в произвольной инструкции. Поток может владеть mutex/SRW-lock, внутренним lock CRT/heap, сокетом, файловой операцией, HID/ViGEm состоянием или объектом с незавершённым деструктором. После этого процесс продолжает выполнять остальной shutdown. Возможны deadlock, heap corruption, зависший HANDLE, повреждение файла и редкий crash уже в другом месте.

Для realtime-потока принудительное завершение дополнительно пропускает нормальный cleanup таймера/MMCSS/time period, расположенный в конце worker'а.

**Исправление:** полностью удалить `TerminateThread` из штатных путей. Каждый worker должен ждать stop-token/event одновременно с внешним I/O. Небезопасный драйверный вызов следует изолировать в дочернем процессе либо в отдельном одноразовом worker-контексте, который можно оставить ОС при завершении процесса, но нельзя продолжать переиспользовать внутри живого процесса.

## HJ-AUD-P1-002. «Ограниченный shutdown» трёх native backend'ов всё равно может зависнуть навсегда

**Доказательства:**

- `addressed_analog_backend.cpp:1212-1220`, `1312-1320`
- `hex80_backend.cpp:645-650`
- `mad68pr_backend.cpp:2200-2205`

**Отказ:** после тайм-аутов или сигнала stop выполняется безусловный `std::thread::join()`. Если поток завис в HID/Windows API, allocator, filesystem, callback или библиотечном коде, UI/application shutdown блокируется без верхней границы.

**Исправление:** заменить глобальные `std::thread` на объект lifecycle с native waitable handle либо `std::jthread` + реально interruptible I/O. Stop должен возвращать `TimedOut/Faulted`, не уничтожать контекст, пока worker не подтверждённо завершился, и переводить модуль в `Poisoned`, запрещая restart.

## HJ-AUD-P1-003. Контракт `stop()` недостоверен

**Документированный контракт:** `docs/development/NATIVE_BACKEND_CONTRACT.md:19-24` — stop должен join'ить worker, закрыть ресурсы и вернуть, завершён ли shutdown.

**Потребитель результата:**

- `native_analog_backend_registry.cpp:96-110`
- `native_analog_backend_registry.cpp:113-124`

Registry сбрасывает `g_started[i]`, только когда callback вернул `true`.

**Нарушения:**

- `mad68pr_backend.cpp:2518` — `[] { Mad68ProR_Stop(); return true; }`
- `hex80_backend.cpp:789` — `[] { Hex80_Stop(); return true; }`
- `addressed_analog_backend.cpp:1435` — `[] { AddressedAnalog_Stop(); return true; }`
- `backend_sparklink.inc:1189-1229` — всегда `true`, включая forced termination;
- `backend_sayo.inc:550-587` — вычисляет `allJoined`, но всё равно возвращает `true`.

**Отказ:** registry считает backend чисто остановленным и разрешает повторный start, даже если worker завис, был аварийно убит либо завершение не подтверждено. Это маскирует дефект и открывает дорогу двум поколениям потоков/ресурсов.

**Исправление:** единый `StopResult {Stopped, AlreadyStopped, TimedOut, Faulted}`; descriptor должен возвращать реальный результат. При `TimedOut` состояние остаётся `Poisoned`, а не `Stopped`.

## HJ-AUD-P1-004. Нет верхней границы C++-исключений в ключевых worker-потоках

**Доказательства:**

- realtime worker: `realtime_loop.cpp:146-345`;
- MAD68 worker: `mad68pr_backend.cpp:2049-2102`;
- Hex80 worker: `hex80_backend.cpp:527-571`;
- Addressed worker: `addressed_analog_backend.cpp:1234-1262`;
- Addressed создаёт вложенный reader-thread внутри session-кода без локального exception boundary;
- UAP device worker: `third_party/UniversalAnalogPluginFixed/main.cpp:749-775`;
- UAP discovery worker: `main.cpp:925-939`.

**Отказ:** необработанное исключение в entry function потока вызывает `std::terminate`. Источник может быть редким: allocation failure, `std::thread` construction, `std::filesystem`, `std::vector`, `std::system_error`, исключение Soup или callback.

**Исправление:** каждый thread entry — `noexcept` wrapper с `try/catch (...)`, записью `exception_ptr`/ошибки, переводом state в `Faulted`, публикацией нулевых значений и пробуждением owner thread. Исключения не должны пересекать C ABI.

## HJ-AUD-P1-005. Addressed reader закрывает HID HANDLE из другого потока при активном stack `OVERLAPPED`

**Доказательства:**

- helpers: `addressed_analog_backend.cpp:787-827`;
- reader loop: `addressed_analog_backend.cpp:939-1003`;
- forced close: `addressed_analog_backend.cpp:1212-1220`;
- общий helper: `hid_io_operation.h:94-120`.

**Отказ:** `CancelIoEx` только запрашивает отмену. Пока completion не подтверждён на потоке-владельце, ОС потенциально ещё использует `OVERLAPPED` и буфер. Закрытие HANDLE из другого потока, последующий выход функции и уничтожение stack storage создают риск use-after-scope/невалидного completion состояния.

**Исправление:** reader сам владеет HANDLE, buffer и `OVERLAPPED`; stop-event входит в wait-set; owner инициирует `CancelIoEx`, затем обязательно reap'ит completion до уничтожения памяти. Другой поток не закрывает active I/O handle.

## HJ-AUD-P1-006. Analog host допускает повторную инициализацию поверх не завершившегося поколения потоков

**Доказательства:** `analog_host_client.cpp:1670-1751` и `1394-1517`.

При тайм-ауте shutdown код:

- закрывает thread HANDLE'ы;
- не освобождает mapping/events/job;
- устанавливает `started=false`;
- возвращает `WootingAnalogResult_Ok`.

`EnsureClientResources()` затем может заново заполнить те же поля глобального `g_client`, пока старый supervisor/bridge ещё обращается к ним.

**Отказ:** старый worker способен увидеть HANDLE/указатели нового поколения, сигналить/закрывать чужие объекты или читать новую shared mapping. Возможны гонки, UAF, утечки и хаотичный restart loop.

**Исправление:** immutable `ClientGeneration` в `shared_ptr`, передаваемый параметром потокам. Глобальный owner содержит только текущую generation. Если join не подтверждён, generation остаётся живой и состояние становится `Poisoned`; reinitialise запрещён до процесса/явного recovery.

## HJ-AUD-P1-007. Partial-start cleanup analog host может unmap'нуть память под живым bridge thread

**Доказательства:** `analog_host_client.cpp:1487-1509`.

Если создание supervisor thread не удалось, код сигналит bridge, ждёт 2 секунды, игнорирует результат ожидания, закрывает thread HANDLE и немедленно unmap'ит shared state/закрывает events.

**Отказ:** если bridge не вышел за 2 секунды, он продолжает использовать освобождённое отображение и закрытые HANDLE.

**Исправление:** проверять wait result. При тайм-ауте не освобождать контекст; переводить generation в poisoned/leaked-until-process-exit. Лучше создавать оба потока через объект, который публикуется глобально только после полного успешного старта.

## HJ-AUD-P1-008. Потеря device-change wakeup из-за manual-reset `ResetEvent` после ожидания

**Доказательства:**

- Addressed: `addressed_analog_backend.cpp:1247-1248`, `1256-1257`;
- Hex80: `hex80_backend.cpp:540-542`, `561-562`;
- MAD68: `mad68pr_backend.cpp:2066-2068`, `2088-2089`.

**Отказ:** `SetEvent` между возвратом `WaitForSingleObject` и `ResetEvent` будет стёрт. Переподключение/изменение устройства тогда обрабатывается только после fallback timeout.

**Исправление:** auto-reset event; либо generation counter + `WaitOnAddress`; либо reset-before-recheck с обязательной повторной проверкой condition/state.

## HJ-AUD-P1-009. Initial startup игнорирует отказ realtime и зависимых native phases

**Доказательства:** `app.cpp:775-820`.

`RealtimeLoop_Start()` и обе `NativeAnalogBackends_StartPhase(...)` вызываются без проверки результата. При late-retry тот же realtime start уже проверяется и backend откатывается: `app.cpp:939-951`.

**Отказ:** приложение может объявить backend ready, хотя realtime worker не стартовал; AfterRealtime backend'ы могут публиковать данные в отсутствующий consumer. UI продолжает работу в неявном частично инициализированном состоянии.

**Исправление:** единый startup transaction/state graph. `AfterRealtime` запускается только после подтверждённого realtime. Возвращаемые результаты фаз агрегируются, ошибки показываются в UI/telemetry, а уже стартовавшие зависимости откатываются в обратном порядке.

## HJ-AUD-P1-010. Синхронный ViGEm update находится внутри realtime worker

**Доказательство:** `backend.cpp:3018-3027`.

**Отказ:** зависший/очень медленный `vigem_target_x360_update` блокирует общий realtime worker, все устройства и shutdown; через 5 секунд текущий код переходит к `TerminateThread`.

**Исправление:** как минимум fault-injection wrapper и watchdog telemetry. Надёжнее — отдельный bounded output worker с latest-state coalescing и состоянием подключения, либо изоляция нестабильного driver-facing слоя в процессе. Нельзя освобождать client/target, пока вызов может продолжаться.

## HJ-AUD-P1-011. Mouse IPC неверно определяет, создано ли новое mapping

**Доказательство:** `mouse_ipc.cpp:15-39`.

`GetLastError()` проверяется после `MapViewOfFile`, хотя значение `ERROR_ALREADY_EXISTS` нужно сохранить сразу после `CreateFileMappingW`.

**Отказ:** `MapViewOfFile` может изменить last-error. Уже существующее mapping ошибочно будет принято за новое и обнулено, включая состояние второй стороны.

**Исправление:** `DWORD createError = GetLastError();` немедленно после успешного `CreateFileMappingW`; отдельно валидировать magic/version/structSize существующего mapping.

## HJ-AUD-P1-012. Named IPC analog host допускает precreation/spoofing в той же сессии пользователя

**Доказательства:** `analog_host_client.cpp:1404-1456`; имена передаются child через command line; объекты создаются с default security attributes.

Nonce строится из PID/QPC/tick, а не CSPRNG. Код не проверяет `ERROR_ALREADY_EXISTS` и сразу обнуляет mapping.

**Отказ:** другой процесс того же пользователя может предсоздать/открыть объекты, подменять shared state или сигналить events. Это прежде всего локальный integrity/robustness риск.

**Исправление:** наследуемые unnamed HANDLE'ы через `PROC_THREAD_ATTRIBUTE_HANDLE_LIST`; либо restrictive DACL, CSPRNG nonce, проверка creation disposition и handshake с cryptographically random token.

## HJ-AUD-P1-013. Встроенный dependency installer имеет TOCTOU и неполную проверку цепочки поставки

**Доказательства:**

- predictable temp location: `app_deps.cpp:344-363`;
- Authenticode: `108-125`, `446-459`;
- фиксированный elevated script: `487-520`;
- динамический latest download: `541-637`.

**Проблемы:**

- файл проверяется, затем позднее запускается по изменяемому пути;
- нет удержания immutable handle между verify и elevated execution;
- проверяется «любая доверенная подпись», но не ожидаемый publisher/certificate;
- revocation отключён и используется cache-only retrieval;
- UAP ZIP не имеет hash/signature проверки;
- elevated PowerShell распаковывает содержимое mutable ZIP;
- используются latest assets без pinned version/hash.

**Отказ:** локальный процесс может заменить installer/script/ZIP в окне между проверкой и запуском; compromised/replaced upstream latest asset автоматически получает elevation.

**Исправление:** приватный random temp directory с restrictive ACL, запретом reparse points; pinned SHA-256 и версия; expected signer/publisher; повторная проверка внутри минимального elevated helper непосредственно перед запуском/копированием; отказ от mutable generated PowerShell.

## HJ-AUD-P1-014. Dependency installer может навсегда заморозить UI

**Доказательство:** `app_deps.cpp:416-443`, `WaitForSingleObject(..., INFINITE)` на строке 436.

**Отказ:** зависший installer/msiexec/UAC child делает основной поток приложения неотзывчивым без upper bound.

**Исправление:** асинхронный state machine, UI message pumping, cancel/status, bounded watchdog и отдельный helper. Не завершать установку аварийным уничтожением `msiexec`; пользователь должен получить понятный статус и возможность закрыть HallJoy.

## HJ-AUD-P1-015. UAP plugin не защищает C ABI от исключений и использует ручные lock/unlock

**Доказательства:** `third_party/UniversalAnalogPluginFixed/main.cpp`, в частности:

- callback: `740-746`;
- `_device_info`: `635-647`;
- `_initialise`: `893-941`;
- `read_analog`: `948-975`;
- `_read_full_buffer`: `977-1082`;
- `unload`: `1085-1123`.

**Отказ:** исключение Soup/STL/callback может выйти через exported C ABI и завершить host. При исключении между `.lock()` и `.unlock()` mutex остаётся захваченным, вызывая постоянный deadlock.

**Исправление:** `std::lock_guard`/`unique_lock`; каждый export — `noexcept` facade с `try/catch (...)`, безопасным error code и telemetry. Внешний callback должен вызываться через exception barrier.

## HJ-AUD-P1-016. UAP unload выполняет неограниченный join, удерживая глобальный devices mutex

**Доказательство:** `third_party/UniversalAnalogPluginFixed/main.cpp:1101-1117`.

**Отказ:** `awaitCompletion()` выполняется под `devices_mtx`. При зависшем HID receive/Soup worker unload не ограничен; любой путь, которому нужен тот же mutex, дополнительно блокируется.

**Исправление:** под lock переместить коллекцию в локальную ownership-структуру, снять lock, инициировать cancel и join. На уровне HallJoy host полагаться на process isolation и ограниченное завершение child, а не пытаться принудительно убить DLL thread внутри основного процесса.

---

# 5. P2 — существенные дефекты и архитектурные риски

## HJ-AUD-P2-001. Overlay server обслуживает только одного клиента синхронно

**Доказательства:** `overlay_server.cpp:979-1035`.

Один client может удерживать server thread до 256 requests с receive timeout 5 секунд. Медленный локальный клиент блокирует OBS/browser и усложняет shutdown.

**Исправление:** nonblocking/select/IOCP либо небольшой bounded pool; короткий keepalive или `Connection: close`; stop-event должен входить в wait model.

## HJ-AUD-P2-002. Overlay HTTP parser не реализует framing

**Доказательства:** `overlay_server.cpp:985-999`.

Один `recv` принимается за полный request, buffer ограничен 2047 байтами; нет накопления до `\r\n\r\n`, корректной обработки split/coalesced/pipelined requests, лимита с ответом 431, проверки method/version.

**Отказ:** частичный request даёт неверный route; два request в одном packet десинхронизируются; oversized header обрабатывается неопределённо.

**Исправление:** минимальный конечный автомат HTTP/1.1 с накопительным buffer и жёсткими лимитами либо готовый loopback HTTP server.

## HJ-AUD-P2-003. Overlay telemetry parser допускает unsigned overflow

**Доказательство:** `overlay_server.cpp:912-925`.

`value = value * 10 + digit` не проверяет переполнение.

**Исправление:** `std::from_chars` с error handling и clamp каждого telemetry field.

## HJ-AUD-P2-004. Overlay endpoint открыт любому origin

**Доказательство:** `overlay_server.cpp:888-900` — `Access-Control-Allow-Origin: *`.

Loopback endpoint не имеет session token. В зависимости от browser/PNA политики сторонняя web-страница может попытаться прочитать состояние клавиш. Это privacy exposure risk, а не доказанный универсальный exploit.

**Исправление:** случайный session token в URL/header, отсутствие wildcard CORS, строгий origin для встроенной страницы.

## HJ-AUD-P2-005. Mouse IPC читает interlocked-written поля обычными volatile reads

**Доказательство:** `mouse_ipc.cpp:70-83`.

**Риск:** aligned `LONG` атомарен на Windows hardware, но C++ synchronization contract неочевиден и отличается от write side.

**Исправление:** `InterlockedCompareExchange(&field, 0, 0)` либо ABI-совместимый atomic access helper для всех shared fields.

## HJ-AUD-P2-006. UAP poll workers собраны без какого-либо pacing

**Доказательства:** все `abiv*.sun` содержат `-DUAP_POLL_SLEEP_MS=0`; worker loop — `main.cpp:749-769`.

Это намеренный режим измерения/низкой задержки, но при быстрой ошибке/немедленном ответе цикл способен занять ядро CPU и агрессивно обращаться к USB. При нескольких poll-устройствах нагрузка умножается.

**Исправление:** adaptive pacing: минимальный yield/deadline, rate target по устройству, exponential backoff на ошибках, telemetry CPU/transactions per second. Нулевая пауза может оставаться диагностическим режимом, а не production default.

## HJ-AUD-P2-007. UAP device identity нестабильна для двух одинаковых устройств

**Доказательства:** `main.cpp:291-309`, назначение occurrence — `839-869`, флаг `DuplicateSafeId` — `695`.

Identity состоит из VID/PID/usage/manufacturer/name, а различение одинаковых экземпляров зависит от enumeration order. После reconnect два одинаковых устройства могут поменяться ID, и профиль будет применён к другому физическому экземпляру.

**Исправление:** device path, serial, Windows Container ID/location path. Если стабильного идентификатора нет, явно маркировать ID как session-scoped, а не `DuplicateSafeId`.

## HJ-AUD-P2-008. `is_initialised()` plugin всегда возвращает `true`

**Доказательство:** `third_party/UniversalAnalogPluginFixed/main.cpp:272-275`.

До `_initialise` и после `unload` API сообщает неверное состояние.

**Исправление:** `return running.load(std::memory_order_acquire);` с учётом фактического ABI contract.

## HJ-AUD-P2-009. `_device_info` не проверяет `buffer == nullptr`

**Доказательство:** `main.cpp:635-647`.

При `len > 0` и null buffer child host упадёт. Даже если SDK обещает валидный pointer, защитная проверка на ABI-границе дёшева.

**Исправление:** null/length validation перед lock.

## HJ-AUD-P2-010. Snapshot export удерживает глобальный devices mutex при копировании всех значений

**Доказательство:** `main.cpp:671-706`.

Под `devices_mtx` дополнительно по очереди захватываются `snapshot_mtx` устройств и копируются dense arrays. Это увеличивает latency discovery/unload/telemetry и создаёт lock-order sensitivity.

**Исправление:** immutable per-device snapshot через atomic shared pointer/seqlock; под global lock копировать только ref-counted список устройств.

**Статус V14-11C:** `Verified`. Registry теперь хранит `shared_ptr<Device>`;
общий mutex удерживается только при копировании не более восьми owner pins.
Telemetry, ожидание `snapshot_mtx` и копирование 256 значений выполняются после
освобождения registry. Детерминированный blocked-reader/removal test, 100 000
lifetime cycles, coherent-copy stress, GCC/MSVC, ASan+UBSan, ABI и production
build прошли. Физическая UAP latency этим не измерялась.

## HJ-AUD-P2-011. Сохранение `settings.ini` может заменить хороший файл неполным temp

**Доказательства:**

- helper'ы игнорируют return `WritePrivateProfileStringW`: `settings_ini.cpp:30-61`;
- `SettingsIni_Save_Internal` безусловно возвращает true: `576-643`;
- затем temp атомарно заменяет final: `646-675`.

**Отказ:** disk full, read-only path, quota или filesystem error создаёт частичный temp, который всё равно заменяет рабочую конфигурацию.

**Исправление:** каждый write возвращает bool; aggregate failure; explicit flush; после записи перечитать/валидировать обязательные section/version keys; заменять final только после полного успеха.

## HJ-AUD-P2-012. Overlay settings сохраняются напрямую и всегда сообщают успех

**Доказательство:** `settings_ini.cpp:678-682`.

**Исправление:** тот же checked-temp-validate-replace protocol.

## HJ-AUD-P2-013. Profile stream не проверяется после flush/close

**Доказательство:** `profile_ini.cpp:101-177`.

Функция возвращает true сразу после writes, не проверяя `f.good()`, `flush`, final close state. Ошибка отложенной записи может проявиться только при flush/close, после чего truncated temp заменит final.

**Исправление:** `f.flush(); if (!f.good()) fail; f.close(); if (f.fail()) fail;` плюс валидация temp.

## HJ-AUD-P2-014. Layout preset очищается и пишется неатомарно без проверки ошибок

**Доказательство:** `keyboard_layout.cpp:464-488`.

Секция final-файла сначала удаляется, все writes игнорируются, затем функция возвращает true.

**Отказ:** crash/disk full оставит пустой или частичный preset.

**Исправление:** отдельный temp-файл, checked writes, parse-back validation, atomic replace.

## HJ-AUD-P2-015. Curve preset writer также игнорирует результаты записей

**Доказательство:** `keyboard_profiles.cpp:364-408`.

**Исправление:** унифицировать все INI/preset save через один transactional storage component.

## HJ-AUD-P2-016. Большинство вызовов сохранения игнорируют возвращаемую ошибку

Примеры:

- `app.cpp:208`, `223`, `1061`, `1082`;
- `keyboard_subpages.cpp:5663-5664`, `5921-5926`, `6047-6048`;
- `keyboard_bind_panel.cpp:137`, `144`;
- `keyboard_page_main.cpp:901`, `1817`, `2128`, `2159`, `2176`, `2193`, `2212`, `2220`;
- `remap_panel.cpp:1926`, `2293`.

**Отказ:** UI подтверждает изменение, но после restart оно исчезает; пользователю не сообщается причина.

**Исправление:** централизованный `SettingsStore::Commit()` с результатом и UI notification/retry. Не выполнять десятки независимых sync writes на каждое действие.

## HJ-AUD-P2-017. Writable state хранится рядом с executable

**Доказательство:** `app_paths.cpp:5-20`.

При установке в `Program Files` или read-only directory запись закономерно не работает; из-за предыдущих дефектов часто молча.

**Исправление:** `%LocalAppData%\HallJoy`; portable mode — только при явном marker-файле/ключе и проверке writable path. Выполнить one-time migration.

## HJ-AUD-P2-018. Имена профилей недостаточно нормализованы

**Доказательства:** `global_profiles.cpp:26-38` и аналогичный sanitizer keyboard profiles.

Не учтены Windows reserved names (`CON`, `PRN`, `AUX`, `NUL`, `COM1`...), case-insensitive collisions, trailing dot/space после нормализации, ограничение длины.

**Исправление:** единый canonical filename validator; reserved base names, Unicode/case normalization, max length, collision-safe suffix/ID.

## HJ-AUD-P2-019. Публикация curve settings имеет слабый memory-order contract

**Доказательства:** `backend_curve.cpp:27`, `97-107`, `174-176`.

Generation counter меняется и читается relaxed. Другие settings записываются отдельно. Формально realtime thread может увидеть новый generation, но закэшировать старый набор полей под новым stamp.

**Исправление:** release increment + acquire read как минимум; лучше публиковать единый immutable versioned `CurveSnapshot` atomic pointer/seqlock.

## HJ-AUD-P2-020. Глобальный lifecycle registry не защищён и не кодирует thread affinity

**Доказательство:** `native_analog_backend_registry.cpp:28` и lifecycle functions.

Если вызовы действительно только UI-thread, это не закреплено assertion'ом/type ownership. Любой будущий restart/recovery с worker thread создаст data race на `g_started`.

**Исправление:** app-owned registry instance с mutex/state machine либо debug thread-affinity assertion и документированный single-owner contract.

## HJ-AUD-P2-021. VID:PID ownership слишком крупнозернистый

**Доказательство:** `native_analog_routing.cpp:15-77`, контракт `NATIVE_BACKEND_CONTRACT.md:29-33`.

После доказательства одного interface/backend забирает всю пару VID:PID. Если устройство имеет несколько разных interfaces или разные модели/firmware используют одну пару, UAP исключается целиком.

**Исправление:** ownership по physical container/device path/interface fingerprint. До расширения Soup IPC хотя бы документировать limitation и тестировать multi-interface устройства.

---

# 6. P3 — документация, build и защитные улучшения

## HJ-AUD-P3-001. TESTING.md описывает отсутствующий тестовый контур

`src/HallJoyProject/TESTING.md` заявляет версию V11.0.2 и следующие сущности, которых нет в поставке:

- `HallJoyTests`;
- `tools/run_safety_hotfix_tests.ps1`;
- `tools/bench.ps1`;
- `tools/run_tests.ps1`;
- `tools/ensure_blend2d.ps1`;
- `tools/profile.ps1` и ряд связанных outputs.

Это делает официальный day-to-day test/benchmark flow невоспроизводимым из архива v3.9.0.

**Исправление:** обновить TESTING.md под фактический пакет или вернуть все заявленные targets/scripts.

## HJ-AUD-P3-002. Внутренние README/BUILD документы относятся к другим поколениям проекта

- `src/HallJoyProject/README.md` описывает старый SafeHID/V6-контекст;
- `src/HallJoyProject/BUILD_README_RU.txt` ссылается на отсутствующие файлы/скрипты;
- `TESTING.md` — V11.0.2 при корневой версии v3.9.0.

**Исправление:** один versioned documentation set; старые документы переместить в `docs/archive/<version>` и добавить крупный obsolete banner.

## HJ-AUD-P3-003. Один static audit сам устарел относительно текущей архитектуры

`src/HallJoyProject/tools/validate_addressed_protocol_backend.py` ожидает старую app-level интеграцию и не проходит на текущем catalog-driven коде.

**Исправление:** удалить/переписать и включить в единый runner, чтобы в репозитории не оставались «официальные» красные проверки.

## HJ-AUD-P3-004. Validation docs переоценивают lifecycle validation

`docs/validation/VALIDATION_PACKAGE_V3_9_0.txt:6-9`, `57-67` и native contract создают впечатление завершённой lifecycle проверки. Но static audits не обнаруживают unconditional-true stop, `TerminateThread`, безусловные join и partial generation reuse.

**Исправление:** явно разделить:

- source invariant checks;
- portable parser tests;
- Windows lifecycle tests;
- hardware validation.

Не использовать слово «validated» для поведения, которое проверено только regex/marker audit'ом.

## HJ-AUD-P3-005. Release Win32 конфигурация ссылается на x64 ViGEm library

**Доказательство:** `HallJoy.vcxproj:108-124`; `Release|Win32` использует `lib\release\x64`.

Это, вероятно, приводит к machine-type linker error. При этом custom output paths также жёстко x64-oriented.

**Исправление:** если продукт x64-only, удалить Win32 configurations. Если Win32 поддерживается — исправить library path и добавить CI build.

## HJ-AUD-P3-006. Проект компилируется только с Warning Level 3

**Доказательство:** `HallJoy.vcxproj:90`, `110`, `133`, `153`.

**Исправление:** перейти на `/W4`, постепенно включить selected warnings as errors, `/analyze`, CodeQL/clang-tidy. Сначала сформировать suppressions baseline, а не включать blanket `/WX` на весь legacy UI.

## HJ-AUD-P3-007. Корневой README не перечисляет реальные build dependencies

README сообщает только Visual Studio Desktop C++. Но `build_fixed_plugin.ps1` требует:

- Git;
- доступ к GitHub при первом build;
- рабочий x64 `clang.exe`;
- bootstrap Sun и загрузку Soup.

**Исправление:** полный prerequisites/offline-build раздел, либо vendor/cache точных зависимостей.

## HJ-AUD-P3-008. Sun build tool берётся с moving branch

**Доказательство:** `third_party/UniversalAnalogPluginFixed/tools/build_fixed_plugin.ps1:117-128` — shallow clone branch `senpai`. Soup при этом правильно pinned на commit `b02796...` (`168-189`).

**Отказ:** две сборки одного архива в разные даты могут использовать разный Sun и выдавать отличающиеся binary/поведение.

**Исправление:** pin exact Sun commit и проверять hash/commit после checkout.

---

# 7. Архитектурные нелогичности и рекомендуемое переписывание

## 7.1. Главная проблема — lifecycle распределён по глобальным переменным

Модули используют сочетания:

- global HANDLE;
- global `std::thread`/Win32 thread HANDLE;
- несколько atomic bool;
- ручные stop events;
- cross-thread close;
- implicit ownership;
- неодинаковые fallback-политики.

Это делает partial startup, restart и failure cleanup намного сложнее, чем основную протокольную логику.

### Предлагаемый базовый тип

```text
WorkerController<TContext>
  state: Stopped | Starting | Running | StopRequested | Joined | Faulted | Poisoned
  generation: uint64
  context: shared_ptr<const/owned GenerationContext>
  stop_source / stop_event
  completion_event
  thread_handle
  last_error / exception_ptr
  Start() -> StartResult
  RequestStop()
  Join(deadline) -> StopResult
```

Правила:

1. Worker получает весь generation context параметром и не читает mutable global HANDLE'ы.
2. Ресурсы уничтожаются только после подтверждённого completion.
3. Тайм-аут не превращается в `Stopped`; он превращается в `Poisoned`.
4. Повторный start запрещён, пока предыдущее поколение не завершено.
5. Нет cross-thread `CloseHandle` active I/O.
6. Каждый entry function ловит все C++ exceptions.
7. Один модуль — одна единая политика stop/result/telemetry.

## 7.2. Разделить driver-facing и realtime части

Текущий `backend.cpp` одновременно занимается агрегированием входа, кривыми, gamepad report, ViGEm targets и scheduler. Синхронный driver call способен остановить весь realtime тракт.

Предлагаемое разделение:

```text
Native/UAP Inputs
      ↓ immutable InputSnapshot
InputAggregator + Curves + SOCD
      ↓ immutable DesiredGamepadState (latest wins)
VigemOutputWorker
      ↓
ViGEm client/targets
```

Realtime producer не должен ждать driver call бесконечно. Output worker хранит только последнее состояние и coalesces старые updates. При driver fault публикуется понятное состояние, выполняется controlled reconnect или process isolation.

## 7.3. Разделить большие монолиты

Особенно крупные файлы:

- `keyboard_subpages.cpp` — примерно 386 KB;
- `backend.cpp` — примерно 155 KB;
- `mad68pr_backend.cpp` — примерно 108 KB;
- `analog_host_client.cpp` — примерно 86 KB;
- Spark/Sayo подключаются как `.inc` в `backend.cpp`.

Рекомендуемая структура:

- `input/InputAggregator.*`
- `output/VigemDeviceManager.*`
- `output/VigemOutputWorker.*`
- `mouse/MouseToStickProcessor.*`
- `uap/AnalogHostClient.*`
- `uap/AnalogHostGeneration.*`
- `native/spark/SparkBackend.*`
- `native/sayo/SayoBackend.*`
- UI subpages — отдельный controller/model/view на страницу;
- Win32 message handling отдельно от модели настроек и rendering.

`.inc` с stateful implementation следует убрать: он ухудшает границы, время компиляции и тестируемость.

## 7.4. Единое transactional storage API

Вместо нескольких наборов `WritePrivateProfileStringW`:

```text
SettingsStore::Load()
SettingsStore::BeginTransaction()
transaction.Set(...)
transaction.Validate()
transaction.CommitAtomic()
```

Commit должен:

1. писать в уникальный temp в той же директории;
2. проверять каждую операцию;
3. flush'ить;
4. перечитывать и валидировать schema/version;
5. atomically replace final;
6. возвращать rich error;
7. показывать пользователю ошибку один раз, а не молча терять изменение.

Желательно перейти с Win32 profile API на JSON/TOML/собственный строго типизированный формат либо хотя бы централизованный INI writer.

## 7.5. UAP host — правильное место для изоляции

То, что UAP загружается в child process, — хорошее решение. Его нужно довести до конца:

- parent никогда не переиспользует незавершённый generation;
- child получает inherited unnamed HANDLE'ы;
- shared state имеет sequence/version validation;
- child crash/restart имеет backoff и лимит;
- plugin exports имеют exception barriers, но parent всё равно считает child недоверенным;
- unbounded plugin unload можно обходить завершением child job после bounded graceful attempt.

---

# 8. Недостающие тесты

## 8.1. Windows lifecycle/fault injection — обязательный минимум

Добавить test hooks для отказа N-го вызова:

- `CreateThread` / `std::thread`;
- `CreateEventW`;
- `CreateFileMappingW` / `MapViewOfFile`;
- `CreateJobObjectW` / `SetInformationJobObject`;
- `CreateFileW` HID;
- `ReadFile` / `WriteFile` / `GetOverlappedResult`;
- `CancelIoEx`;
- `vigem_target_x360_update`;
- `ShellExecuteExW`;
- settings writes/atomic replace.

Для каждого injected failure проверять:

- нет живого worker после успешного stop;
- при timeout state = Poisoned, restart запрещён;
- нет закрытия HANDLE под активным I/O;
- нет двойного close/unmap;
- UI получает ошибку;
- значения устройства сброшены;
- повторный clean start после корректного stop работает.

## 8.2. Stress/soak

- 1000 циклов start/stop каждого backend без устройства;
- 1000 циклов с mock устройством;
- unplug во время pending read;
- unplug во время pending write;
- reconnect с изменившимся device path;
- два одинаковых UAP устройства с изменением enumeration order;
- 8–24 часа polling/streaming;
- sleep/resume, user logoff, fast shutdown;
- ViGEm disconnect/reinstall во время работы;
- child host crash каждые N polls с backoff/restart limit.

## 8.3. Windows diagnostic tools

- Application Verifier: Handles, Locks, Heaps, Exceptions;
- Full PageHeap для HallJoy и analog host;
- GFlags handle tracing;
- MSVC `/analyze`;
- `/W4` baseline;
- ASan в доступных MSVC/clang-cl конфигурациях;
- CodeQL/clang-tidy;
- ETW/WPA для CPU и USB polling;
- Wait Chain Traversal dump при зависании shutdown.

## 8.4. Persistence tests

- read-only install directory;
- disk full/quota;
- interrupted save после каждой записи;
- повреждённый/truncated temp;
- reserved profile names;
- case-only duplicate names;
- very long Unicode names;
- миграция из near-EXE path в LocalAppData.

## 8.5. Overlay tests

- HTTP header разбит на 1-byte chunks;
- два requests в одном send;
- header > limit;
- slowloris localhost client;
- 10 параллельных clients;
- malformed request line;
- overflow query numbers;
- stop во время blocked recv/send.

## 8.6. Hardware matrix

Для каждого поддерживаемого семейства фиксировать:

- VID/PID/interface/report lengths/firmware;
- cold boot;
- hotplug;
- reconnect;
- sleep/resume;
- 1/2 одинаковых устройства;
- одновременный native + UAP device;
- latency/CPU/USB transaction rate;
- безопасное восстановление временного режима устройства после crash.

---

# 9. Рекомендуемый порядок исправлений

## Этап A — прекратить потенциальное повреждение процесса

1. Удалить все `TerminateThread`.
2. Ввести единый `StopResult` и состояния `Faulted/Poisoned`.
3. Исправить Addressed overlapped ownership.
4. Запретить analog-host reinitialise после неполного join.
5. Добавить exception barriers во все worker entry и plugin exports.
6. Исправить initial startup transaction.

## Этап B — предотвратить потерю данных и локальную подмену

1. Transactional settings/profile/layout saves.
2. Перенос writable state в LocalAppData.
3. Inherited unnamed IPC handles для analog host.
4. Hardened dependency installer или удаление auto-elevation из основного приложения.

## Этап C — убрать редкие latency/hang сценарии

1. Изолировать/обернуть ViGEm output.
2. Исправить lost wakeups.
3. Переписать overlay server framing/concurrency.
4. Ввести adaptive pacing UAP poll workers.

## Этап D — сделать архитектуру поддерживаемой

1. WorkerController/generation contexts.
2. Разделить `backend.cpp` и `keyboard_subpages.cpp`.
3. Убрать implementation `.inc`.
4. Перевести UI/telemetry на immutable snapshots.
5. Обновить CI и документацию.

---

# 10. Что в проекте сделано хорошо

- Чистый пакет и проверяемый SHA-256 manifest.
- Переносимые протокольные parser/scheduler tests реально компилируются и проходят.
- Hex80/MAD68/Addressed parser logic отделена от части Windows transport лучше, чем legacy backend code.
- Native catalog/descriptor architecture уменьшает количество protocol-specific правок в центральном коде.
- UAP исключение выполняется до Soup HID open, что снижает конфликт native/UAP.
- ViGEm library в build script проверяется по точному размеру и SHA-256.
- Soup pinned на точный commit.
- UAP уже вынесен в отдельный host process — это правильное направление для недоверенного plugin/HID-кода.
- Late MAD68 recovery проверяет `RealtimeLoop_Start()` и делает rollback; этот подход следует распространить на initial startup.
- Авторы явно думают о bounded shutdown, telemetry и диагностике. Основная проблема — выбранные fallback-механизмы иногда нарушают те же гарантии, которые пытаются обеспечить.

# 11. Финальная оценка

| Область | Оценка |
|---|---|
| Протокольные парсеры и pure logic | Хорошо, доступные тесты проходят |
| Маршрутизация native/UAP | В целом продумана, есть coarse VID:PID limitation |
| Realtime/output | Хорошая идея, но driver call и forced termination опасны |
| Thread/HANDLE lifecycle | Не готово к заявлению о высокой отказоустойчивости |
| Analog host isolation | Правильная архитектурная идея, generation lifecycle требует исправления |
| UAP plugin robustness | Требуются RAII и exception barriers |
| Persistence | Высокий риск тихой потери/повреждения настроек при I/O ошибках |
| Overlay server | Достаточен для happy path, слаб при malformed/slow clients |
| Installer/security | Нужен существенный hardening |
| Build reproducibility | Частично хорошая; Sun и latest installers не pinned |
| Документация/тестовый контракт | Несогласован с фактической поставкой |

**Общий вывод:** основную функциональность можно сохранить. Полный rewrite протокольных алгоритмов не требуется. Нужен целевой архитектурный rewrite lifecycle/ownership слоя, persistence и IPC. Это даст намного больший прирост стабильности, чем дальнейшая микрооптимизация парсеров или realtime arithmetic.

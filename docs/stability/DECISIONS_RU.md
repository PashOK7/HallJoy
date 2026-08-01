# Журнал архитектурных решений

## ADR-001 — Локальная поэтапная работа без GitHub

**Статус:** принято.

Все изменения выполняются в локальной рабочей копии и передаются полными архивами этапов. Удалённые репозитории, pull request и GitHub Actions не являются частью рабочего процесса. Существующие workflow-файлы сохраняются как документация проекта, но не используются как доказательство текущего локального результата.

## ADR-002 — Никакого большого rewrite

**Статус:** принято.

Протокольные парсеры и маршрутизация имеют рабочую тестовую основу. Изменения выполняются небольшими пакетами с сохранением публичного поведения. Архитектурное разделение допускается только после добавления characterization-тестов и только по одному модулю за раз.

## ADR-003 — Тайм-аут означает `Poisoned`, а не `Stopped`

**Статус:** принято для новой lifecycle-модели.

Контекст, который не подтвердил завершение worker'а, нельзя повторно использовать или уничтожать как штатно остановленный. Restart запрещается до завершения старого поколения либо завершения процесса.

## ADR-004 — Cooperative shutdown вместо принудительного завершения потока

**Статус:** принято.

`TerminateThread` не допускается в штатных путях. Worker должен владеть своими I/O-ресурсами, ждать stop-сигнал в том же wait-set и подтверждать completion до освобождения памяти и HANDLE.

## ADR-005 — Измерение до оптимизации

**Статус:** принято.

Оптимизация допускается только при наличии baseline-метрики, воспроизводимого сценария и проверки отсутствия изменения выходных значений. CPU, latency, wake rate, USB transaction rate и allocation rate измеряются отдельно.

## ADR-006 — Монотонное поколение и отдельный `Joined`

**Статус:** принято.

Каждая попытка start получает новый ненулевой `GenerationId`; неудавшееся поколение не переиспользуется. `Joined` отделён от `Stopped`, потому что только подтверждённое завершение конкретного worker-поколения разрешает безопасную замену его ресурсов. Timeout переводит поколение в `Poisoned` и блокирует restart.

## ADR-007 — Allocation-free машинно-читаемая lifecycle-ошибка

**Статус:** принято.

`LifecycleError` не хранит `std::string` и остаётся trivially copyable. Диагностика кодирует operation, исходное/запрошенное состояние, supplied/active generation и native error. Форматирование текста выполняется вне noexcept/C ABI границы.

## ADR-008 — Platform primitives отделены от state machine

**Статус:** принято.

OS wait/time/thread операции предоставляются через `WorkerPrimitives`. Это позволяет воспроизводить timeout, start failure и join failure без реального HANDLE. В S01 интерфейс намеренно не подключён к существующим backend'ам; миграция выполняется отдельными пакетами после truthful stop contract.

## ADR-009 — Один allocation-free exception barrier для worker entry

**Статус:** принято.

OS thread entry остаётся тонким `noexcept` wrapper. Общий helper ловит только C++ exceptions, копирует `what()` в фиксированный буфер и вызывает `noexcept` callbacks. Нормальный worker algorithm остаётся отдельной функцией; исключения не используются как control flow, а hot path не получает новых mutex/allocations.

## ADR-010 — Fault закрывает публикации, но owner обязан reap'нуть generation

**Статус:** принято.

Worker при fault может закрыть producer/running publications и свои локальные ресурсы, но не уничтожает owner-owned thread HANDLE или WSA/file lifecycle. Повторная инициализация блокируется, пока owner-side `Stop/Shutdown` не подтвердит завершение и не освободит поколение.

## ADR-011 — Realtime fault публикует нейтральное состояние best-effort

**Статус:** принято.

После неожиданного C++-исключения realtime worker не оставляет UI и virtual gamepad в последнем нажатом состоянии. Allocation-free fail-safe очищает snapshots/reports и пытается отправить neutral XUSB report существующим targets. Ошибка этой попытки записывается в ViGEm status; полноценное вынесение ViGEm из realtime остаётся S12.


## ADR-S02A1-01 — Проверять генерируемый patch block, а не только исходный script

**Решение:** static gate для self-modifying patcher обязан извлекать и проверять содержимое here-string, которое будет записано в upstream source.

**Причина:** поиск маркера по всему `.ps1` не различал декларацию/validator и фактически генерируемый C++ block. Это дало ложноположительный PASS при детерминированно нерабочей clean build.

**Следствие:** marker хранится в одной переменной, эмитится в `$preOpenBlock`, а post-patch validator повторно использует эту переменную. Отдельный audit проверяет generator/validator contract до загрузки и сборки Soup.


## ADR-012 — Native workers мигрируются по сложности владения

**Статус:** принято.

MAD68 и Hex80 не владеют вложенными worker-потоками, поэтому их exception boundaries объединены в S02B.1. Addressed имеет отдельный reader с overlapped I/O, а Sayo/SparkLink используют Win32 HANDLE lifecycle; они не включаются в тот же пакет, чтобы exception cleanup не маскировал изменения ownership/shutdown.

## ADR-013 — Joinable завершившееся поколение reap'ится до замены `std::thread`

**Статус:** принято.

После normal completion или exception `std::thread` остаётся joinable. Присваивание нового thread поверх него вызывает `std::terminate`. Поэтому Start сначала получает ownership через существующий running CAS, затем join'ит завершившееся поколение, освобождает его wake HANDLE и только после этого создаёт новое поколение. Fault record сбрасывается после reap, а не до него.

## ADR-014 — Hardware validation имеет уровни, а не бинарный статус

**Статус:** принято.

SparkLink используется как непрерывный доступный hardware gate. Отсутствие MAD68/Hex80 не блокирует boundary-only пакеты с неизменным polling/protocol body и полным локальным gate, но такие изменения остаются `Implemented / device gate deferred`. Полный `Verified` допускается только после проверки владельцами соответствующих устройств на одном предфинальном архиве.

Это исключает две одинаково опасные крайности: остановку всей стабилизации из-за недоступного устройства и ложное объявление непроверенного backend'а готовым.

## ADR-015 — SparkLink сохраняет внешний SEH wrapper и получает внутренний C++ barrier

**Статус:** принято.

Win32 thread entry SparkLink остаётся тонким `noexcept` SEH wrapper. Внутри него отдельный `noexcept` C++ entry использует общий `RunWorkerEntryBarrier`. C++ и structured faults сходятся в одном idempotent neutral-publication contract, но native SEH code хранится отдельно.

Normal polling loop, HID protocol и pacing не изменяются. Cooperative shutdown и удаление `TerminateThread` не смешиваются с exception containment и остаются S08.

## ADR-016 — Worker completion проверяется при SparkLink startup publication

**Статус:** принято.

`g_sparkWorkerExited` сбрасывается до `CreateThread` и публикуется каждым C++/SEH completion path. `SparkStart()` проверяет его перед `connected=true` и сразу после публикации. Это закрывает race, при котором уже завершившийся worker мог быть ошибочно объявлен подключённым стартующим owner-потоком.


## ADR-017 — Вложенный reader должен быть reaped до rethrow

**Статус:** принято.

Внешняя exception boundary не защищает функцию, в стеке которой находится joinable `std::thread`: при unwinding его destructor вызывает `std::terminate` раньше внешнего catch. Поэтому Addressed session создаёт reader внутри protected block, а exceptional cleanup сначала останавливает/cancel'ит I/O, join'ит reader, очищает stack-backed publications и только затем повторно выбрасывает исключение в main worker barrier.

Сам reader имеет собственный `noexcept` barrier и RAII ownership зарегистрированного HANDLE. Это решение не заменяет корректную overlapped cancellation ownership; cross-thread force-close остаётся отдельным S06.

## ADR-018 — Runtime-пакет принимается по машинному evidence, а не по устному smoke

**Статус:** принято.

Начиная с S02V1 пользовательское наблюдение остаётся полезным, но не является достаточным доказательством. Изменение runtime получает положительный evidence gate только после формализованного сценария, bounded log bundle и детерминированного анализа.

`PASS` означает полный сценарий без запрещённых событий. `WARN` означает недостаточный охват и требует повторения. `FAIL` блокирует следующий feature-пакет.

## ADR-019 — Временная трассировка использует memory mapping и не пишет на событии

**Статус:** принято.

Worker completion/fault и shutdown paths не должны зависеть от задержки файловой системы. Поэтому event path не вызывает `WriteFile`, `FlushViewOfFile` или `FlushFileBuffers`: он копирует фиксированную строку в заранее отображённый 1 МиБ buffer. Единственный flush выполняется после final worker shutdown.

Sequence назначается внутри того же exclusive lock, что и append. Это обеспечивает машинно проверяемый порядок при конкурентных событиях. Переполнение — явный `FAIL`, а не циклическая потеря старых данных.

## ADR-020 — Логирование следует за изменением и удаляется после доказательства

**Статус:** принято.

Temporary trace не накапливается как постоянная телеметрия. После подтверждения конкретного изменения его специализированные события удаляются, evidence report и SHA-256 сохраняются, а следующая функция получает отдельные события и новый stage marker. Перед финальным релизом temporary trace/build flag/collector удаляются, если отдельным решением не утверждён opt-in support mode.

## D-S06 — pending Addressed I/O завершает и освобождает только reader

**Статус:** принято для V14-12B.

`CancelIoEx` является запросом, а не подтверждением завершения. Поэтому
Addressed reader единолично владеет HID HANDLE, buffer, event и stack
`OVERLAPPED`, вызывает blocking reap и только после terminal completion закрывает
HANDLE. Owner threads могут лишь запросить cancellation; cross-thread close
запрещён.

Чтобы неисправный HID driver не заставил UI shutdown зависнуть, вложенный reader
остаётся joinable внутри main worker и тем самым сохраняет весь session stack. Сам main
worker запускается через `_beginthreadex` и имеет waitable HANDLE. Registry ждёт
его не более трёх секунд; timeout сохраняет thread/event/session ownership,
возвращает `TimedOut`, poison'ит generation и требует немедленного process
containment без зависимого teardown. Это сознательное сохранение памяти до
завершения процесса, а не leak в продолжающей работу программе.

## D-S07-HEX80 — Hex80 session handle закрывает только worker

**Статус:** принято для V14-12C.

Активный Hex80 HID HANDLE принадлежит `Session` внутри worker. Owner-side stop
может только опубликовать stop, разбудить event и вызвать `CancelIoEx`; закрывать
HANDLE из другого потока запрещено. Registration scope объявляется после
`Session`, поэтому снимается до того, как RAII-деструктор session закрывает HANDLE.
Завершившийся одновременно со stop request отбрасывается до decode/publication.

Worker запускается через `_beginthreadex`. Owner ждёт его не более 3000 ms и
закрывает thread/event только после подтверждённого completion. Timeout сохраняет
все reachable generation resources, возвращает truthful `StopResult`, запрещает
restart и требует process containment. Протокол, command bytes и routing proof в
этом lifecycle-пакете не изменяются.

## D-S07-MAD68 — cancellation не должна отменять финальный A9

**Статус:** принято для V14-12D.

MAD68 может временно перевести клавиатуру в A8 service mode, поэтому shutdown
обязан оставить worker возможность отправить idempotent A9. Owner-side stop
отменяет только persistent overlapped read; write/control операции и финальный A9
остаются worker-owned. После stop новый A8 запрещён. Session снимает active-read
registration до собственного cancel-and-reap и закрытия трёх HANDLE.

Worker имеет waitable `_beginthreadex` generation и единый 3000 ms deadline.
Неподтверждённое завершение сохраняет весь session stack и HANDLE ownership,
poison'ит restart и требует process containment. Нельзя закрывать HID из owner
thread или объявлять timeout успехом. Protocol builders, transports, A8/A9
strategy, restore и decoder в этом пакете не меняются.

## D-SYNC-001 — release risk register является источником статусов

`docs/v1.4/RISK_REGISTER.md` хранит принятый статус текущего release, а
`docs/stability/RISK_REGISTER_RU.md` сохраняет исходные audit ID и расширенные
evidence notes. При расхождении статус переносится только вместе с уже
зафиксированным package evidence; наличие кода без обязательного gate не даёт
права повысить статус. Reconciliation 2026-08-01 синхронизировал все 45 общих
ID, не меняя hardware/soak ограничения V14-12.

## D-S18-001 — runtime не имеет права устанавливать privileged dependencies

Отказ от автоматической установки безопаснее отдельного helper: HallJoy больше
не получает полномочий скачивать mutable executable, писать его в temp и
запускать elevated. При отсутствии ViGEmBus приложение показывает точную
официальную страницу версии 1.22.0 и остаётся degraded до ручной установки и
перезапуска. URL, версия и `manual-only` закреплены в dependency lock и
production constant. Никакой timeout не используется как маскировка живого
installer process — самого process launch в HallJoy больше нет.

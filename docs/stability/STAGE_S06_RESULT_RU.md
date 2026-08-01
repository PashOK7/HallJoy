# Результат S06 — Addressed overlapped I/O ownership

Дата: 2026-08-01
Пакет: V14-12B
Статус: `Implemented / Addressed hardware gate deferred`

## Исправленный риск

Старый shutdown мог вызвать `CancelIoEx`, дождаться лишь короткого окна и затем
закрыть HID HANDLE из main/owner thread, пока в reader stack оставались живые
buffer и `OVERLAPPED`. `CancelIoEx` не подтверждает terminal completion, поэтому
такой force-close допускал use-after-lifetime и неопределённое поведение.

Теперь HID HANDLE закрывает только reader RAII после `CancelAndDrain`. Если driver
не завершает запрос, reader и весь session stack остаются живы внутри main
worker. Внешний stop ждёт native main-worker HANDLE не более 3000 мс; timeout
сохраняет все generation resources, возвращает `TimedOut`, poison'ит registry и
останавливает зависимый teardown приложения.

## Инварианты

- owner thread может вызвать `CancelIoEx`, но не `CloseHandle` reader HID;
- buffer, event и `OVERLAPPED` существуют до terminal reap;
- поздний read после stop не публикует ненулевой analog state;
- thread/events/claim освобождаются только после подтверждённого main-worker exit;
- timeout не разрешает restart и приводит к немедленному process containment;
- packet builders, command bytes и session polling cadence не изменены.

## Проверки

- targeted static ownership/containment audit: PASS;
- полный gate: 39 static audits + 26 portable C++ tests PASS;
- MSVC 19.44 `/W4 /WX` для Addressed TU: PASS;
- simulator-only Addressed stop timeout: PASS, ожидаемый exit code 2;
- официальный `BUILD.cmd`: PASS, 0 ошибок, только allowlisted LNK4099;
- `MakePacket`, `MakePollPacket` и session polling core: token-identical;
- Irok MG75 Max production smoke 15 секунд: 57 161 successful routes, один
  cancellation в shutdown window, balanced exit 0, zero trace ERROR;
- 11 пользовательских файлов до/после smoke: zero differences.

Physical Addressed device недоступен. Поэтому исправление принято как
implementation-tested, но не получает hardware-verified статус до внешнего
device-owner gate.

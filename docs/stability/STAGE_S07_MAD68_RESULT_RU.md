# S07 / V14-12D — MAD68 bounded lifecycle

Дата: 2026-08-01
Статус: `Implemented / MAD68 hardware gate deferred`

## Результат

MAD68 worker переведён со `std::thread` на waitable `_beginthreadex` generation.
Owner публикует stop, будит event и отменяет persistent overlapped read, но не
закрывает HID HANDLE. Session worker'а сам снимает регистрацию, reap'ит pending
I/O и закрывает read/write/control handles.

MAD68 требует особой shutdown-семантики: после stop новый A8 блокируется, а
финальный прямой A9 остаётся обязательным. Завершившиеся одновременно со stop
reads не доходят до decode/publication. Внешний join ограничен 3000 ms; timeout
сохраняет generation resources, блокирует restart и выбирает process containment.

## Регрессия

- protocol `.cpp/.h`, command send, interrupt/control transports,
  `BestEffortRestore`, `RunStrategy` и `ProcessPayload` не изменились;
- 41/41 static audits и 26/26 portable C++ tests прошли;
- MAD68 translation unit прошёл MSVC 19.44 `/W4 /WX`;
- simulator timeout подтвердил retained ownership и exit code 2;
- официальный build и 15-секундный Irok smoke прошли; 11 пользовательских
  файлов остались hash-identical.

## Ограничение

Без физического MAD68 не подтверждены A8/A9 hardware transition, full-matrix,
hotplug и реальный stuck-driver shutdown. Кодовая часть S07 завершена, но
device-owner qualification остаётся release gate.

Evidence: `tests/V14-12D_S07_MAD68_LIFECYCLE_2026-08-01.txt`.

# S07 / V14-12C — Hex80 bounded lifecycle

Дата: 2026-08-01
Статус: `Implemented / Hex80 hardware gate deferred`

## Результат

Hex80 worker больше не использует безусловный `std::thread::join`. Поколение
имеет waitable `_beginthreadex` HANDLE, сериализованный start/stop и единый
трёхсекундный deadline. Stop сначала публикует cooperative flag, будит event и
запрашивает cancellation активного HID I/O. HID HANDLE остаётся собственностью
worker и закрывается только после terminal reap.

После возврата каждого polling request stop проверяется до decode и публикации.
При timeout generation resources намеренно сохраняются, registry получает
`TimedOut`, запрещает restart, а приложение пропускает зависимый teardown и
завершает процесс containment-кодом 2.

## Регрессия

- Hex80 protocol `.cpp/.h`, command builders, response parser и GET-only routing
  не менялись;
- 40/40 static audits и 26/26 portable C++ tests прошли;
- Hex80 translation unit прошёл MSVC 19.44 `/W4 /WX`;
- simulator-only stop timeout подтвердил retained ownership и process containment;
- официальный `BUILD.cmd` завершился без ошибок и неожиданных предупреждений;
- 15-секундный Irok MG75 Max smoke выполнил 57 276/57 276 успешных SparkLink
  запросов, завершился штатно и не изменил ни один из 11 пользовательских файлов.

## Ограничение

Физического Hex80 нет, поэтому input, full-matrix publication, unplug/reconnect и
device-specific shutdown не объявляются проверенными. Следующий отдельный пакет
S07 — MAD68 lifecycle migration.

Evidence: `tests/V14-12C_S07_HEX80_LIFECYCLE_2026-08-01.txt`.

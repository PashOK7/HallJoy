# Аудит HallJoy / Madlions V6

## Данные

Проанализированы:
- восемь V5 analog-host crash events одной полевой сессии;
- два полных текстовых crash report и minidump;
- HallJoyAnalogHost.log и основной HallJoyDiagnostic log;
- HallJoy, Universal Analog Plugin и затронутые части Soup.

Все восемь сбоев имеют одну сигнатуру: execute `0x21` в
`madlions_before_receive`. Время жизни отдельных host-процессов различается,
поэтому событие не является специальным Steam-кодом или фиксированным счётчиком.

## Устранённые нарушения

1. Удалено одновременное незавершённое чтение и запись Madlions на одном handle.
2. Удалён стековый `OVERLAPPED` из Madlions transaction lifetime.
3. Для каждой операции создан отдельный manual-reset event.
4. CancelIoEx всегда сопровождается ожиданием окончательного completion.
5. Контекст и buffer не меняют адрес до завершения kernel I/O.
6. Stale reports дренируются до начала request.
7. Transport имеет timeout и точные checkpoints.
8. Win32 transport error экспортируется в shared diagnostic state.
9. Persistent I/O failure вызывает controlled reconnect с новым процессом/handle.
10. Родительский snapshot обнуляется до записи dump, а не после завершения host.
11. Сохранены bounds/length/clamp/per-device-state/HandleRaii fixes V5.
12. Soup закреплён на точном commit для воспроизводимости.

## Статические проверки

- все 50 translation units HallJoy проходят x64 Windows syntax compilation
  LLVM-MinGW/Clang с MS extensions и diagnostic defines;
- отдельные compile fixtures проходят для точного SafeHID implementation и
  заменённой Madlions parser function;
- project/filter XML и обязательные source/library paths проверены;
- архив проверяется через `unzip -t`;
- build script проверяет наличие всех обязательных файлов до MSBuild.

## Что требует физического теста

Linux-среда не может выполнить MSVC link, Windows HID I/O и реальную клавиатуру.
Поэтому подтверждением runtime-исправления является длительный PnP stress test
на том же Madlions/Windows компьютере. Критерий — ноль реальных analog-host
crash reports и отсутствие зависших клавиш/длительных разрывов ввода.

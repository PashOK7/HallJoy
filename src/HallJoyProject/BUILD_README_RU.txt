HALLJOY MADLIONS V6 — SAFE HID TRANSPORT + ИЗОЛИРОВАННЫЙ HOST

Главное изменение V6:
- старый Windows overlapped-I/O путь Madlions полностью заменён;
- write и read выполняются строго последовательно;
- у каждой операции собственный manual-reset event;
- pending I/O всегда завершён либо отменён и дренирован до повторного
  использования памяти;
- PnP/timeout/ошибка HID приводят к контролируемому открытию нового handle,
  а не к повреждению стека.

Дополнительные границы:
- Wooting Analog SDK исключён из пути Madlions;
- private Universal Analog Plugin ABI1 вызывается напрямую;
- plugin, Soup и HID-код работают только в дочернем процессе;
- основной HallJoy немедленно обнуляет snapshot при crash/hang;
- внешний supervisor сохраняет регистры, стек, модули и minidump;
- host автоматически перезапускается с новым HID handle.

Требования:
- Visual Studio 2022 / Build Tools: Desktop development with C++
- C++ Clang tools for Windows x64
- Git
- интернет при первой загрузке Sun и Soup

Сборка:
  powershell -NoProfile -ExecutionPolicy Bypass -File .\build_all.ps1

Результат:
  HallJoyProject\x64\MadlionsDiagnostic\SEND_TO_MADLIONS_TESTER\

Сначала проверить fault containment:
  .\HallJoyMadlionsSafeHID.exe --diagnostic-analog-host-crash-test

Затем выполнить стресс-тест из README_TEST.txt. Критерий V6 — не просто
сохранение окна HallJoy, а отсутствие новых реальных
HallJoyAnalogHostCrash_*.txt/.dmp при PnP-нагрузке.

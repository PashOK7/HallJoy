HALLJOY v1.4 — АКТУАЛЬНАЯ СБОРКА
================================

Поддерживаемая цель: Windows x64. Win32/x86 не поддерживается и не заявляется,
поскольку поставляемые ViGEm/UAP-компоненты собраны для x64.

Конфигурации Release|x64 и Debug|x64 собираются. Debug сохраняет symbols,
runtime checks и отключённую оптимизацию, но использует совместимый static
release CRT: bundled ViGEmClient.lib собран с /MT release.

Требования:
- Visual Studio 2022 Build Tools;
- workload Desktop development with C++;
- x64 Clang tools for Windows;
- Python 3.12;
- Git;
- PowerShell 5.1 или новее.

Собирать из корня репозитория единственной командой:

  BUILD.cmd

Эквивалентный прямой вызов:

  powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\build.ps1

Перед MSVC-сборкой сценарий проверяет dependency lock, static audits и
portable C++20 tests. Production собирается с Warning Level 4; любое новое
неразрешённое предупреждение прерывает официальную сборку.

Результат:

  build\output\HallJoy.exe

Не использовать старые build_all.ps1, V6/Madlions diagnostic paths или
HallJoyMadlionsSafeHID.exe — они не являются текущим release target.

HallJoy не скачивает и не запускает драйверные установщики с повышением прав.
Если ViGEmBus отсутствует, программа показывает точную официальную ссылку для
ручной установки. Private UAP runtime собирается и проверяется самим BUILD.cmd.

Проверки и release-гейты описаны в:

  src\HallJoyProject\TESTING.md
  docs\v1.4\VALIDATION_MATRIX.md
  docs\v1.4\RISK_REGISTER.md

Aula WIN 60 HE MAX пока имеет статус firmware-proven/hardware-unvalidated.
Разработка и автоматические проверки продолжаются, но релиз без физического
теста Aula не утверждается.

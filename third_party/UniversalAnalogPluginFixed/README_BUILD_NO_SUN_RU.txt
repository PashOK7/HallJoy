UNIVERSAL ANALOG PLUGIN — HALLJOY MADLIONS SAFEHID V6

Сборка запускается из корня полного архива:
  powershell -NoProfile -ExecutionPolicy Bypass -File .\build_all.ps1

Скрипт:
- найдёт или соберёт Sun;
- найдёт рабочий x64 clang.exe;
- загрузит точную ревизию Soup
  b02796b0b20276277c8a4b4d3759643eeab43ff7;
- применит SafeHID transport и остальные Madlions/HandleRaii fixes;
- соберёт abiv0.dll;
- соберёт private ABI1 с Wooting-device support;
- встроит ABI1 DLL в HallJoy;
- соберёт HallJoyMadlionsSafeHID.exe.

Требуются:
- Visual Studio 2022 C++ Build Tools;
- C++ Clang tools for Windows x64;
- Git;
- интернет при первой загрузке Sun и Soup.

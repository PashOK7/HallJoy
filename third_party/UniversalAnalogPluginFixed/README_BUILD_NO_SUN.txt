UNIVERSAL ANALOG PLUGIN - HALLJOY MADLIONS SAFEHID V6

Run the build from the root of the complete archive:

  powershell -NoProfile -ExecutionPolicy Bypass -File .\build_all.ps1

The script:

- locates or builds Sun;
- locates a working x64 clang.exe;
- downloads the pinned Soup revision
  b02796b0b20276277c8a4b4d3759643eeab43ff7;
- applies the SafeHID transport and the remaining Madlions/HandleRaii fixes;
- builds abiv0.dll;
- builds the private ABI1 library with Wooting-device support;
- embeds the ABI1 DLL in HallJoy;
- builds HallJoyMadlionsSafeHID.exe.

Requirements:

- Visual Studio 2022 C++ Build Tools;
- C++ Clang tools for Windows x64;
- Git;
- an internet connection for the initial Sun and Soup download.

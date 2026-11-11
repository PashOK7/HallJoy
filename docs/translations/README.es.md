# HallJoy — inicio rápido

[![C++20](https://img.shields.io/badge/C%2B%2B-20-00599C?style=flat-square)](../../src/HallJoyProject/HallJoy/HallJoy.vcxproj)
[![Windows x64](https://img.shields.io/badge/Windows-x64-0078D4?style=flat-square)](../../README.md#quick-start)
[![PowerShell 5.1+](https://img.shields.io/badge/PowerShell-5.1%2B-5391FE?style=flat-square)](../PORTABLE.md)
[![NSIS 3](https://img.shields.io/badge/NSIS-3-4A7C59?style=flat-square)](../development/WINDOWS_INSTALLER.md)

[English](../../README.md) · **Español** · [Français](README.fr.md) · [Italiano](README.it.md) · [简体中文](README.zh-CN.md) · [한국어](README.ko.md) · [Русский](README.ru.md) · [Deutsch](README.de.md) · [日本語](README.ja.md) · [हिन्दी](README.hi.md)

HallJoy convierte el recorrido real de las teclas de un teclado analógico
compatible en un mando Xbox virtual en Windows. No convierte un teclado digital
normal en uno analógico. Incluye asignación de botones, sticks y gatillos, curvas
de respuesta, zonas muertas, perfiles y Gamepad Tester. Autor: **PashOK7**.

Esta es una guía breve, no una traducción completa de la documentación técnica.
Consulta los modelos y sus límites en [Supported](../../README.md#supported) y
[Experimental support](../../README.md#experimental-support).

## Primeros pasos

1. Necesitas Windows 10/11 x64 y un teclado analógico compatible.
2. Descarga `HallJoy.exe` de las [versiones oficiales](https://github.com/PashOK7/HallJoy/releases).
3. Abre la aplicación. Si falta ViGEmBus, pulsa **Install ViGEmBus 1.22.0**
   y acepta la solicitud independiente de permisos de administrador.
4. Cierra los configuradores web del teclado que interfieran. Comprueba el
   dispositivo y la distribución; selecciona otra manualmente si hace falta.
5. Asigna controles en **Remap** y comprueba el recorrido, el rango completo
   y el retorno a cero en **Gamepad Tester**.

No es necesario instalar Wooting Analog SDK ni Universal Analog Plugin aparte.

## Instalación y datos

Se puede [compilar un instalador opcional](../development/WINDOWS_INSTALLER.md)
con icono, accesos directos y desinstalación que conserva los datos. Esto no
significa que ya exista un instalador en las versiones oficiales.
En un clon, `Update.cmd` descarga el EXE oficial en `.local/portable` y `Start.cmd`
lo inicia. Esa carpeta queda fuera de Git; las actualizaciones conservan perfiles.
La copia instalada usa normalmente `%LOCALAPPDATA%\HallJoy`.

Guías en inglés: [datos y copias de seguridad](../PORTABLE.md),
[privacidad](../INPUT_AND_PRIVACY.md), [controles de conducción](../DRIVING_SETUP.md).
[Vídeo](https://youtu.be/MI_ZTS6UFhM) · [Soporte](https://discord.gg/X5QsZJdN5a)
· [Licencia](../../LICENSE) · [Contribuir](../../.github/CONTRIBUTING.md).

# HallJoy — guida rapida

[![C++20](https://img.shields.io/badge/C%2B%2B-20-00599C?style=flat-square)](../../src/HallJoyProject/HallJoy/HallJoy.vcxproj)
[![Windows x64](https://img.shields.io/badge/Windows-x64-0078D4?style=flat-square)](../../README.md#quick-start)
[![PowerShell 5.1+](https://img.shields.io/badge/PowerShell-5.1%2B-5391FE?style=flat-square)](../PORTABLE.md)
[![NSIS 3](https://img.shields.io/badge/NSIS-3-4A7C59?style=flat-square)](../development/WINDOWS_INSTALLER.md)

[English](../../README.md) · [Español](README.es.md) · [Français](README.fr.md) · **Italiano** · [简体中文](README.zh-CN.md) · [한국어](README.ko.md) · [Русский](README.ru.md) · [Deutsch](README.de.md) · [日本語](README.ja.md) · [हिन्दी](README.hi.md)

HallJoy trasforma la corsa reale dei tasti di una tastiera analogica compatibile
in comandi di un controller Xbox virtuale su Windows. Non trasforma una normale
tastiera digitale in una tastiera analogica. Offre assegnazioni per pulsanti,
stick e grilletti, curve di risposta, zone morte, profili e **Gamepad Tester**.
Autore: **PashOK7**.

Questa guida presenta l'applicazione; non è una traduzione completa della
documentazione tecnica. Consulta i modelli e i loro limiti in [Supported](../../README.md#supported)
e [Experimental support](../../README.md#experimental-support).

## Primi passi

1. Servono Windows 10/11 x64 e una tastiera analogica compatibile.
2. Scarica `HallJoy.exe` dalle [release ufficiali](https://github.com/PashOK7/HallJoy/releases).
3. Avvia l'applicazione. Se manca ViGEmBus, fai clic su **Install ViGEmBus 1.22.0**
   e accetta la richiesta separata di privilegi di amministratore.
4. Chiudi i configuratori web della tastiera che interferiscono. Controlla il
   dispositivo e il layout; se necessario, seleziona manualmente un altro layout.
5. Assegna i comandi in **Remap**, poi controlla la corsa, l'intero intervallo
   dei valori e il ritorno a zero in **Gamepad Tester**.

Non occorre installare separatamente Wooting Analog SDK o Universal Analog Plugin.

## Installazione e dati

È possibile [compilare un installer Windows facoltativo](../development/WINDOWS_INSTALLER.md)
con icona, collegamenti e disinstallazione che conserva i dati. Questo non significa
che un installer sia già disponibile nelle release ufficiali.
In un clone, `Update.cmd` scarica l'EXE ufficiale in `.local/portable` e `Start.cmd`
lo avvia. Questa cartella è esclusa da Git; gli aggiornamenti conservano i profili.
La copia installata usa normalmente `%LOCALAPPDATA%\HallJoy`.

Guide in inglese: [dati e backup](../PORTABLE.md),
[privacy](../INPUT_AND_PRIVACY.md), [comandi di guida](../DRIVING_SETUP.md).
[Video](https://youtu.be/MI_ZTS6UFhM) · [Supporto](https://discord.gg/X5QsZJdN5a)
· [Licenza](../../LICENSE) · [Contribuire](../../.github/CONTRIBUTING.md).

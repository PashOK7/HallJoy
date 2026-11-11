# HallJoy — Schnellstart

[![C++20](https://img.shields.io/badge/C%2B%2B-20-00599C?style=flat-square)](../../src/HallJoyProject/HallJoy/HallJoy.vcxproj)
[![Windows x64](https://img.shields.io/badge/Windows-x64-0078D4?style=flat-square)](../../README.md#quick-start)
[![PowerShell 5.1+](https://img.shields.io/badge/PowerShell-5.1%2B-5391FE?style=flat-square)](../PORTABLE.md)
[![NSIS 3](https://img.shields.io/badge/NSIS-3-4A7C59?style=flat-square)](../development/WINDOWS_INSTALLER.md)

[English](../../README.md) · [Español](README.es.md) · [Français](README.fr.md) · [Italiano](README.it.md) · [简体中文](README.zh-CN.md) · [한국어](README.ko.md) · [Русский](README.ru.md) · **Deutsch** · [日本語](README.ja.md) · [हिन्दी](README.hi.md)

HallJoy wandelt den tatsächlichen Tastenhub einer unterstützten analogen Tastatur
unter Windows in Eingaben für einen virtuellen Xbox-Controller um. Eine normale
digitale Tastatur wird dadurch nicht analog. Es gibt Tasten-, Stick- und
Triggerbelegungen, Antwortkurven, Totzonen, Profile und Gamepad Tester.
Autor: **PashOK7**.

Dies ist eine Kurzanleitung, keine vollständige Übersetzung der technischen
Dokumentation. Modelle und Einschränkungen stehen unter
[Supported](../../README.md#supported) und [Experimental support](../../README.md#experimental-support).

## Erste Schritte

1. Benötigt werden Windows 10/11 x64 und eine unterstützte analoge Tastatur.
2. Lade `HallJoy.exe` aus den [offiziellen Releases](https://github.com/PashOK7/HallJoy/releases).
3. Starte die Anwendung. Fehlt ViGEmBus, wähle **Install ViGEmBus 1.22.0**
   und bestätige die separate Abfrage der Administratorrechte.
4. Schließe störende Web-Konfiguratoren der Tastatur. Prüfe Gerät und Layout;
   wähle das Layout bei Bedarf manuell.
5. Weise unter **Remap** die Steuerung zu. Prüfe in **Gamepad Tester** den
   Tastenhub, den gesamten Wertebereich und die Rückkehr auf null.

Wooting Analog SDK und Universal Analog Plugin müssen nicht separat installiert werden.

## Installation und Daten

Ein [optionaler Installer kann gebaut werden](../development/WINDOWS_INSTALLER.md).
Er bietet Symbol, Verknüpfungen und Deinstallation mit Erhalt der Benutzerdaten.
Das bedeutet nicht, dass bereits ein offizieller Installer veröffentlicht wurde.
In einem Klon lädt `Update.cmd` die offizielle EXE nach `.local/portable`;
`Start.cmd` startet sie. Dieser Ordner wird von Git ignoriert. Updates erhalten
Profile. Die installierte Kopie nutzt normalerweise `%LOCALAPPDATA%\HallJoy`.

Englische Anleitungen: [Daten und Sicherungen](../PORTABLE.md),
[Datenschutz](../INPUT_AND_PRIVACY.md), [Fahrsteuerung](../DRIVING_SETUP.md).
[Video](https://youtu.be/MI_ZTS6UFhM) · [Support](https://discord.gg/X5QsZJdN5a)
· [Lizenz](../../LICENSE) · [Mitwirken](../../.github/CONTRIBUTING.md).

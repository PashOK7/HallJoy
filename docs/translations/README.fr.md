# HallJoy — démarrage rapide

[![C++20](https://img.shields.io/badge/C%2B%2B-20-00599C?style=flat-square)](../../src/HallJoyProject/HallJoy/HallJoy.vcxproj)
[![Windows x64](https://img.shields.io/badge/Windows-x64-0078D4?style=flat-square)](../../README.md#quick-start)
[![PowerShell 5.1+](https://img.shields.io/badge/PowerShell-5.1%2B-5391FE?style=flat-square)](../PORTABLE.md)
[![NSIS 3](https://img.shields.io/badge/NSIS-3-4A7C59?style=flat-square)](../development/WINDOWS_INSTALLER.md)

[English](../../README.md) · [Español](README.es.md) · **Français** · [Italiano](README.it.md) · [简体中文](README.zh-CN.md) · [한국어](README.ko.md) · [Русский](README.ru.md) · [Deutsch](README.de.md) · [日本語](README.ja.md) · [हिन्दी](README.hi.md)

HallJoy transforme la course réelle des touches d'un clavier analogique compatible
en commandes de manette Xbox virtuelle sous Windows. Il ne transforme pas un
clavier numérique ordinaire en clavier analogique. Il propose l'affectation des
boutons, sticks et gâchettes, des courbes de réponse, des zones mortes, des profils
et un **Gamepad Tester**. Auteur : **PashOK7**.

Ce guide présente l'application ; il ne traduit pas toute la documentation
technique. Consultez les modèles et leurs limites dans [Supported](../../README.md#supported)
et [Experimental support](../../README.md#experimental-support).

## Premiers pas

1. Utilisez Windows 10/11 x64 et un clavier analogique compatible.
2. Téléchargez `HallJoy.exe` depuis les [versions officielles](https://github.com/PashOK7/HallJoy/releases).
3. Lancez l'application. Si ViGEmBus manque, cliquez sur **Install ViGEmBus 1.22.0**
   et acceptez la demande distincte de droits administrateur.
4. Fermez les configurateurs web du clavier qui interfèrent. Vérifiez le
   périphérique et la disposition ; choisissez-en une manuellement si nécessaire.
5. Affectez les commandes dans **Remap**, puis vérifiez la course, toute la plage
   de valeurs et le retour à zéro dans **Gamepad Tester**.

Il n'est pas nécessaire d'installer séparément Wooting Analog SDK ou Universal Analog Plugin.

## Installation et données

Un [installateur Windows facultatif peut être compilé](../development/WINDOWS_INSTALLER.md).
Il fournit une icône, des raccourcis et une désinstallation qui conserve les données.
Cela ne signifie pas qu'un installateur figure déjà dans les versions officielles.
Dans un clone, `Update.cmd` télécharge l'EXE officiel dans `.local/portable` et
`Start.cmd` le lance. Ce dossier est exclu de Git ; les mises à jour conservent les
profils. La copie installée utilise normalement `%LOCALAPPDATA%\HallJoy`.

Guides en anglais : [données et sauvegardes](../PORTABLE.md),
[confidentialité](../INPUT_AND_PRIVACY.md), [commandes de conduite](../DRIVING_SETUP.md).
[Vidéo](https://youtu.be/MI_ZTS6UFhM) · [Assistance](https://discord.gg/X5QsZJdN5a)
· [Licence](../../LICENSE) · [Contribuer](../../.github/CONTRIBUTING.md).

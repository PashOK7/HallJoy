# HallJoy — быстрый старт

[![C++20](https://img.shields.io/badge/C%2B%2B-20-00599C?style=flat-square)](../../src/HallJoyProject/HallJoy/HallJoy.vcxproj)
[![Windows x64](https://img.shields.io/badge/Windows-x64-0078D4?style=flat-square)](../../README.md#quick-start)
[![PowerShell 5.1+](https://img.shields.io/badge/PowerShell-5.1%2B-5391FE?style=flat-square)](../PORTABLE.md)
[![NSIS 3](https://img.shields.io/badge/NSIS-3-4A7C59?style=flat-square)](../development/WINDOWS_INSTALLER.md)

[English](../../README.md) · [Español](README.es.md) · [Français](README.fr.md) · [Italiano](README.it.md) · [简体中文](README.zh-CN.md) · [한국어](README.ko.md) · **Русский** · [Deutsch](README.de.md) · [日本語](README.ja.md) · [हिन्दी](README.hi.md)

HallJoy — приложение для Windows, которое превращает реальную глубину нажатия
поддерживаемой аналоговой или магнитной клавиатуры в управление виртуальным
Xbox-геймпадом. Обычная цифровая клавиатура не становится аналоговой.
Есть назначение кнопок, стиков и триггеров, кривые отклика, мёртвые зоны,
профили и Gamepad Tester. Автор приложения — **PashOK7**.

Это краткое руководство на русском, а не полный перевод технической документации.
Актуальные модели и ограничения: [Supported](../../README.md#supported) и
[Experimental support](../../README.md#experimental-support) в основном README.

## Запуск

1. Нужны Windows 10/11 x64 и поддерживаемая аналоговая клавиатура.
2. Скачайте `HallJoy.exe` из [официальных выпусков](https://github.com/PashOK7/HallJoy/releases).
3. Запустите приложение. Если ViGEmBus отсутствует, нажмите **Install ViGEmBus 1.22.0**
   в HallJoy и подтвердите отдельный запрос прав администратора.
4. Закройте конфликтующие веб-конфигураторы клавиатуры. Проверьте устройство и
   раскладку; при необходимости выберите раскладку вручную.
5. Назначьте управление на **Remap**, настройте отклик и проверьте глубину нажатия,
   полный диапазон и возврат в ноль в **Gamepad Tester**.

Отдельно устанавливать Wooting Analog SDK или Universal Analog Plugin не нужно.

## Установка и личные настройки

Доступна [сборка необязательного Windows-установщика](../development/WINDOWS_INSTALLER.md)
с иконкой, ярлыками и удалением с сохранением данных. Это возможность сборки,
а не обещание, что установщик уже опубликован в официальном выпуске.

Для сборки открой `Build-Windows.cmd` в корне проекта: он проверяет инструменты,
собирает приложение и создаёт установщик в новой папке внутри `build/packages/`.
Затем запусти полученный `HallJoy-<версия>-Windows-x64-setup.exe`.
Команда `Build-Windows.cmd -CheckOnly` проверяет наличие инструментов без сборки.
Требования к компьютеру разработчика перечислены в руководстве выше; получателю
готового установщика эти инструменты не нужны.

В клоне репозитория `Update.cmd` загружает официальный EXE в `.local/portable`,
а `Start.cmd` запускает его. Эта папка исключена из Git. Обновление сохраняет
настройки и профили; перенос данных между копиями выполняется явно после бекапа.
Установленная копия обычно хранит данные в `%LOCALAPPDATA%\HallJoy`.

[Настройки и резервные копии](../PORTABLE.md) · [Ввод и приватность](../INPUT_AND_PRIVACY.md)
· [Пример управления автомобилем](../DRIVING_SETUP.md) — подробные руководства на английском.

[Видео автора](https://youtu.be/MI_ZTS6UFhM) · [Поддержка](https://discord.gg/X5QsZJdN5a)
· [Лицензия](../../LICENSE) · [Правила вклада](../../.github/CONTRIBUTING.md).

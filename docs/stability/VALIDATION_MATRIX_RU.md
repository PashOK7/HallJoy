# Матрица валидации HallJoy

Дата фиксации: 30 июля 2026 г.

## 1. Назначение

Этот документ разделяет проверки, которые можно выполнять на каждом пакете, и проверки, для которых требуется конкретное физическое устройство. Отсутствие устройства не разрешает объявлять изменение полностью проверенным, но не должно блокировать безопасные boundary-only пакеты с неизменным протоколом и hot path.

## 2. Уровни проверки

### V0 — локальная воспроизводимость

Обязательно для каждого архива:

- полный `tools/run_native_backend_checks.py --require-compiler`;
- GCC и Clang;
- `-Wall -Wextra -pedantic`, для новых тестов также `-Werror`;
- ASan + UBSan для всех portable tests;
- static audits затронутой области;
- manifest и повторный gate после чистой распаковки.

### V1 — Windows/MSVC общий gate

Обязательно перед принятием runtime-пакета:

- чистая x64 Release сборка;
- запуск HallJoy;
- ViGEm input/output smoke;
- повторный запуск и штатное закрытие;
- отсутствие новых crash/hang и явных регрессий.

### V2 — непрерывный SparkLink hardware gate с evidence trace

SparkLink доступен владельцу проекта и используется как постоянный реальный hardware gate. Начиная с S02V1 результат принимается не по устному описанию, а по `HallJoyStabilityTraceBundle.zip` и отчёту `analyze_stability_trace.py`:

- cold start с подключённым устройством;
- hotplug/reconnect;
- обычный analog input и цифровые HID-клавиши;
- ViGEm output;
- все режимы `SparkPollMode`;
- изменение row limit;
- несколько циклов запуска/закрытия;
- unplug во время работы;
- контроль отсутствия stuck axis/trigger после unplug/restart.

Пакеты, меняющие SparkLink runtime, не переходят дальше без V1 + V2.

### V3 — отложенный device-owner gate

MAD68 и Hex80 будут проверены в предфинальном интеграционном архиве владельцами соответствующих клавиатур. До этого их изменения получают статус `Implemented / device gate deferred`, а не `Verified`.

Продолжение разработки допускается только если одновременно выполнено всё:

1. protocol parser, packet layout, HID mapping, command bytes и polling body не изменены;
2. изменение ограничено exception/lifecycle boundary или общей инфраструктурой;
3. V0 полностью пройден;
4. общая Windows-сборка и доступный SparkLink regression smoke не выявили побочных эффектов;
5. в risk register и stage result явно записан отложенный hardware gate.

Addressed и Sayo также требуют реального hardware gate до финального релиза. Пока доступ к их устройствам не подтверждён, изменения этих backend'ов принимаются только малыми пакетами с characterization/fault tests и остаются не выше статуса `Implemented`.

## 3. Предфинальный внешний пакет

Перед release candidate формируется один отдельный архив с неизменяемым manifest и тестовой формой. Владельцы устройств проверяют один и тот же бинарный пакет.

Минимум для каждого backend'а:

- чистый запуск;
- 10 циклов connect/disconnect/reconnect;
- удержание нескольких analog keys и проверка отпускания;
- unplug при активном вводе;
- закрытие HallJoy при подключённом устройстве;
- повторный запуск после аварийного отключения;
- не менее 30 минут обычной работы;
- отсутствие stuck values, crash, hang и потери обычного HID-ввода.

Для MAD68 дополнительно проверяются безопасный A8/A9 lifecycle и отсутствие лишних firmware-команд. Для Hex80 — full-matrix publication и GET-only routing probe.

## 4. Правила статусов

- `Implemented` — код и локальные gates готовы, но обязательного устройства нет.
- `Verified on SparkLink` — V0, V1 и V2 пройдены; это не доказывает MAD68/Hex80/Addressed/Sayo.
- `Verified` — все обязательные device-specific gates для затронутой области пройдены.
- `Deferred` — работа или проверка отложена; это не является положительным результатом.

## 5. Текущее состояние

- S02A/S02A.1: Windows build/runtime smoke подтверждён.
- S02B.1 MAD68/Hex80 exception boundaries: V0 пройден; доступный SparkLink regression smoke пакета пройден; MAD68/Hex80 device gates отложены до предфинала.
- S02B.2 SparkLink exception/SEH boundary: V0, V1 и V2 пройдены; ручной hardware-gate подтвердил SparkLink, ViGEm, режимы и lifecycle.
- S02B.3 Addressed main/reader exception boundaries: V0 и ручной общий Windows/SparkLink regression smoke пройдены; Addressed device gate отложен до предфинального внешнего архива.
- S02V1 evidence trace: локальный V0 пройден; следующий Windows/SparkLink gate должен предоставить machine-verifiable bundle. Устное «работает» сохраняется как наблюдение, но больше не переводит runtime-пакет в evidence-verified.

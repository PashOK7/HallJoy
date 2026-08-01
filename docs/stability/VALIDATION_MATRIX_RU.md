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
- S06/V14-12B Addressed I/O ownership: static ownership/containment gate, portable
  cancel-and-drain/lifecycle suites, MSVC `/W4 /WX`, simulator stop-timeout,
  официальный build и доступный Irok regression пройдены. Addressed hardware
  отсутствует, поэтому статус ограничен `Implemented / device gate deferred`.
- S07/V14-12C Hex80 lifecycle: static ownership/publication/containment gate,
  40 repository audits, 26 portable C++ tests, MSVC `/W4 /WX`, simulator
  stop-timeout, официальный build и доступный Irok regression пройдены. Hex80
  protocol `.cpp/.h` не менялись. Физического Hex80 нет, поэтому статус ограничен
  `Implemented / device gate deferred`; MAD68 остаётся следующим S07-пакетом.
- S07/V14-12D MAD68 lifecycle: persistent-read ownership, no-post-stop A8,
  mandatory final A9, bounded retention/containment, 41 repository audit, 26
  portable C++ tests, MSVC `/W4 /WX`, simulator stop-timeout, официальный build
  и Irok regression пройдены. Protocol, write transports, A8/A9 strategy,
  restore и decoder сохранены. Физического MAD68 нет: статус `Implemented /
  device gate deferred`; кодовая часть S07 завершена.
- S21/V14-12E normal-cycle pilot: новый runner запускает production без
  fault/simulator аргументов, закрывает точный процесс через bounded `WM_CLOSE`,
  проверяет exit 0, полный trace, отсутствие ERROR/оставшихся процессов и
  SHA-256 пользовательского состояния. После официальной сборки 25/25 циклов
  PASS; shutdown 138-326 ms, 11/11 файлов неизменны. Один transient peak 225
  HANDLE вернулся к обычным 218 и не накапливался. Финальные 1000 циклов,
  8-24-часовой soak, reconnect/input и device-owner matrix остаются открыты.
- S18/V14-12F: embedded dependency installer удалён. Negative audit запрещает
  download/WinHTTP/AuthentiCode/ShellExecute/runas/msiexec/wait/latest в
  production recovery; exact ViGEmBus 1.22.0 manual page совпадает с central
  lock. 43 static, 27 portable, MSVC `/W4 /WX`, официальный build и post-build
  Irok 3/3 PASS. Modal missing-ViGEm guidance не открывался из-за установленного
  драйвера; его policy проверен pure/static gates, не hardware наблюдением.
- S20/V14-12G: TESTING/README/BUILD соответствуют поставляемой v1.4, Addressed
  validator проверяет catalog/lifecycle и входит в unified runner, v3.9 validation
  помечен historical. Win32/x86 удалён; Debug/Release x64 — W4 с узким baseline.
  Current validator + 44 audits + 27 portable tests, официальный build с 0
  compiler warnings и Irok normal 3/3 PASS. S20 завершён; 1000 циклов, 8-24h,
  reconnect/input и недоступная hardware matrix остаются S21 qualification.
- S21/V14-12H qualification automation: normal-cycle runner сохраняет
  checkpoint после каждого цикла, terminal pass/fail, state manifests и Spark
  route counters. Новый long-soak runner пишет CSV HANDLE/thread/GDI/USER/
  memory/CPU, проверяет overlay, bounded `WM_CLOSE`, trace/analyzer и фиксированные
  leak limits после 10-секундного warm-up.
- Минутный overlay pilot: 53 samples, HANDLE 210 -> 210, private bytes -86 016,
  Spark 234 846/234 846, trace ERROR 0, процессов 0, 11 файлов без изменений.
  Post-build Irok 3/3: shutdown 259–325 ms, max 209 HANDLE, 16 229 успешных routes
  плюс две cancellation в shutdown window.
- Статус: automation Verified. Финальные 1000 циклов, 8–24 часа, ручные
  input/reconnect и внешняя Aula hardware validation пока не закрыты.

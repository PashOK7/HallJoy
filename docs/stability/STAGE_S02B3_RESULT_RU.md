# Результат S02B.3 — Addressed main/reader exception boundaries

Дата: 30 июля 2026 г.

## Цель

Не допустить выхода C++-исключений из двух потоковых границ Addressed backend — основного worker и вложенного HID reader — не изменяя протокол 09/94/02, polling scheduler, overlapped I/O state machine и существующий shutdown fallback.

## Изменения production-кода

Изменён только `src/HallJoyProject/HallJoy/addressed_analog_backend.cpp`.

Добавлено:

- `noexcept` main-worker entry через общий allocation-free `RunWorkerEntryBarrier`;
- отдельный `noexcept` reader entry через тот же barrier;
- фиксированные fault records для main worker и reader;
- neutral publication при fault;
- обязательный completion signal reader на любом выходе;
- RAII-регистрация reader HANDLE, закрывающая утечку/stale ownership при stack unwinding;
- protected session block, который join'ит вложенный reader до повторного выброса исключения;
- очистка stack-backed `g_scheduler` и pending response token в exceptional cleanup;
- owner-side reap завершившегося main `std::thread` перед новым поколением;
- startup rejection, если worker успел завершиться до возврата из `AddressedAnalog_Start()`.

`ResetPublished()` сделан `noexcept`; уведомление realtime выполняется best-effort и не может выпустить исключение из fault callback.

## Что намеренно не менялось

- команды `0x09/0x94/0x02`, `0x98/0x02` и packet layout;
- checksum и response parsing;
- scheduler classes, deadlines и polling cadence;
- response timeout/recovery thresholds;
- `HidIoOperation` и stack-based `OVERLAPPED` state machine;
- cross-thread `ForceCloseReaderHandle()` fallback;
- значения `kReaderStopWaitMs` и `kReaderForceCloseWaitMs`;
- routing/claim policy и calibration behavior.

Проблема cross-thread force-close активного overlapped HANDLE остаётся открытой для S06. S02B.3 только гарантирует, что C++ exception не обойдёт owner cleanup через `std::terminate`.

## Hot-path доказательство

Token-level сравнение с S02B.2:

```text
Addressed session polling loop: EQUAL
SHA-256: e187cbd4657f969c9911be4b5fef0676ac1a0ef391b2b96a09585bd43a1653a6

Addressed reader I/O loop: EQUAL
SHA-256: 2b9171b926230674ab334e7bdd2d98d5b1ef98747fbca39f42bfc7ed29a973ca

MakePacket: EQUAL
SHA-256: 85c081a6a2c6959540cfad6dc051c638a7419511af2b0d2a205c0399077f2f45

MakePollPacket: EQUAL
SHA-256: 0ab9e6c46fa4970281379ec7bb6c49accba1aff2ec817aa143d3c041c2fcc6d8
```

## Локальные проверки

Пройдено:

- 14 static audits;
- 9 portable C++ tests;
- GCC common gate;
- Clang common gate;
- ASan + UBSan со строгим `-Werror`;
- XML parse MSVC project files;
- targeted Addressed boundary audit;
- token-level hot-path comparison;
- self-excluding package manifest;
- повторный gate после чистой распаковки финального архива.

## Аппаратная проверка

Addressed-устройство недоступно. Поэтому статус пакета для Addressed: `Implemented / device gate deferred`.

Перед переходом к следующему runtime-пакету требуется общий V1 + SparkLink regression smoke:

- clean MSVC x64 Release build;
- HallJoy/ViGEm startup and shutdown;
- SparkLink input, modes, hotplug и neutral release.

Полный Addressed device gate обязателен на предфинальном внешнем архиве.

## Статус

`Implemented / Addressed device gate deferred; Windows + SparkLink regression gate pending`.

# HallJoy v1.4 risk register

This register was inherited intact from the imported advanced archive and is
now authoritative for v1.4. The old `v3.9.0` name identifies provenance only.
Statuses remain conservative until the corresponding v1.4 package reruns every
required gate.

Source audit: `docs/stability/AUDIT_V3_9_0_RU.md`. Current package mapping is
maintained in `ROADMAP.md`.

## Additional v1.4 integration risks

| ID | Priority | Description | Package | Status | Required validation |
|---|---|---|---|---|---|
| `HJ-V14-P1-001` | P1 | Private UAP extraction beside the EXE fails in protected locations | `V14-03` | Verified | forced protected-directory fallback, atomic repair, exact hash and child-load gates passed |
| `HJ-V14-P1-002` | P1 | Legacy recovery recommends a system Wooting SDK that the embedded architecture does not use | `V14-03` | Verified | installer path removed; static diagnostic truthfulness gate passed |
| `HJ-V14-P1-003` | P1 | Imported product/build strings expose the wrong public version | `V14-01` | Verified | version-resource and active-document scan passed |
| `HJ-V14-P1-004` | P1 | SparkLink hotplug watchdog can reconnect a worker after shutdown has already stopped the current generation | `V14-06D.1` | Verified | Irok held-key unplug/reconnect, analog recovery, balanced generations, final reconnect suppression and clean shutdown passed |
| `HJ-V14-P1-005` | P1 | One-shot overlay HTTP connections can block `/state` for the five-second keep-alive receive timeout | `V14-06C.1` | Verified | captured 5.001-5.002 s fetch stalls; simulator and production socket gates now return the next `/state` in under 1 ms |
| `HJ-V14-P2-001` | P2 | v1.3 self-contained runtime improvements can be lost during architectural migration | `V14-03` | Verified | checkpoint `b3fefce` compared; embedded self-contained behavior retained in isolated ABI1 architecture |
| `HJ-V14-P2-002` | P2 | Imported historical validation may be mistaken for validation of later v1.4 code | all | Open | per-package evidence enforcement |
| `HJ-V14-P2-003` | P2 | Simulation could be mistaken for real analog-protocol evidence or leak into production | `V14-02` | Verified | sources excluded from production compile; compile/runtime gates and evidence labels passed |
| `HJ-V14-P2-004` | P2 | Moving remote build inputs can silently change the private UAP or CI behavior | `V14-04` | Verified | independent clone passed locked fresh bootstrap, overlay verification and full build |
| `HJ-V14-P2-005` | P2 | Local build can omit portable tests or accept warnings rejected by CI | `V14-04` | Verified | independent clone ran required portable tests and enforced the production warning allowlist |
| `HJ-V14-P2-006` | P2 | Firmware-derived Aula support could be mistaken for physical hardware validation or could claim a sibling interface too broadly | `V14-12A` | Implemented | exact firmware/oracle reproduction, 17-read proof, exact-path ownership, ambiguity/session tests and Irok regression pass; physical Aula input/hotplug/multi-device gate remains open |

### Evidence boundary for HJ-V14-P2-006

V14-12A accepts only the exact Aula `1CA2:1902`, `FFA0:0001`, 65-byte HID
envelope and firmware `App V1.1.6 / Feb 4 2026`. The supplied firmware verifier
passed 57 checks; independently fetched fixed npm packages matched all ten
oracle sources and reproduced the archived oracle JSON hash. Production tests
exercise exact framing, response correlation, double-generation Fn0 mapping,
two-half travel, session poisoning, reconnect identity and ambiguous devices.

The risk remains `Implemented`, not `Verified`, because no physical Aula device
was available. Real analogue input, held-key unplug/reconnect, multiple Aula
interfaces and alternate firmware coexistence remain V14-12 hardware gates.

### Evidence for HJ-V14-P1-004

The 2026-07-31 Irok MG75 Max hardware traces proved native SparkLink discovery,
polling and analog row input on `VID 1CA6`, `PID 0529`, usage page `FFB0`. The
later production run recorded 515 changed rows and 516 input notifications.

The original shutdown trace showed the first worker exit followed by a hotplug
reconnect and an unmatched second generation. `V14-06D.1` closes an outer
service gate before stopping the active poller. Its production Irok trace has
three worker starts and three matching exits, two successful reconnects, analog
input before and after reconnect, clean process exit 0, and no reconnect, device
open or connection after `service.stop.begin`. The user confirmed correct
held-key neutralization/recovery with no stuck input. The risk is Verified.

### Evidence for HJ-V14-P1-005

The user runtime `overlay_perf.log` recorded isolated fetch averages of
5,001,400-5,002,000 microseconds, exactly matching the server's receive timeout.
The periodic `/client_perf` fetch could leave the single HTTP worker waiting on
an idle keep-alive connection and block the live `/state` stream.

V14-06C.1 closes one-shot telemetry and error responses immediately while
preserving keep-alive for high-rate state polling. The deterministic socket gate
measured the next `/state` at 0.3 ms in simulation and 0.4 ms in the production
`HallJoy.exe`; the normal and forced-timeout overlay lifecycle gates also pass.
The user subsequently confirmed that the browser input overlay no longer
exhibits the observed five-second freezes.

## Статусы

- `Open` — подтверждённая проблема или риск, production fix ещё не принят.
- `Partial` — часть риска закрыта отдельным малым пакетом; оставшаяся область явно указана.
- `Implemented` — код изменён, но обязательная платформенная/hardware проверка ещё не завершена.
- `Verified` — все обязательные gates пройдены.
- `Deferred` — отложено с явно записанной причиной; не означает исправление.

| ID | Приоритет | Краткое описание | Пакет | Статус | Ключевая проверка |
|---|---|---|---|---|---|
| `HJ-AUD-P1-001` | P1 | `TerminateThread` используется как обычный механизм shutdown | `S04/S05/S08` | Implemented | all ordinary occurrences removed in V14-06A-F and deterministic timeout containment passed; final soak/device qualification remains |
| `HJ-AUD-P1-002` | P1 | «Ограниченный shutdown» трёх native backend'ов всё равно может зависнуть навсегда | `S06/S07` | Partial | Addressed bounded join/retention fault gate passed in V14-12B; MAD68/Hex80 remain S07 |
| `HJ-AUD-P1-003` | P1 | Контракт `stop()` недостоверен | `S03` | Verified | generation-scoped StopResult, poison-on-failure and registry fault tests passed |
| `HJ-AUD-P1-004` | P1 | Нет верхней границы C++-исключений в ключевых worker-потоках | `S02` | Partial | UAP workers/C ABI verified in V14-07C; only deferred device-owner and final soak gates remain in release qualification |
| `HJ-AUD-P1-005` | P1 | Addressed reader закрывает HID HANDLE из другого потока при активном stack `OVERLAPPED` | `S06` | Implemented | reader-only close, cancel-and-reap ownership audit and simulator containment passed; Addressed hardware gate deferred |
| `HJ-AUD-P1-006` | P1 | Analog host допускает повторную инициализацию поверх не завершившегося поколения потоков | `S09` | Verified | parent generation, bounded group timeout, retained ownership and restart-rejection gates passed in V14-07A |
| `HJ-AUD-P1-007` | P1 | Partial-start cleanup analog host может unmap'нуть память под живым bridge thread | `S09` | Verified | injected supervisor-start failure joined bridge before IPC rollback in V14-07A |
| `HJ-AUD-P1-008` | P1 | Потеря device-change wakeup из-за manual-reset `ResetEvent` после ожидания | `S11` | Verified | durable monotonic wake sequence, notify/wait race tests and production Irok smoke passed in V14-08A |
| `HJ-AUD-P1-009` | P1 | Initial startup игнорирует отказ realtime и зависимых native phases | `S11` | Verified | explicit commit, reverse rollback and realtime/native-phase fault injections passed in V14-08A |
| `HJ-AUD-P1-010` | P1 | Синхронный ViGEm update находится внутри realtime worker | `S12` | Verified | dedicated output owner, latest-state/multi-pad equivalence and bounded 60-second driver-stall containment passed in V14-08B |
| `HJ-AUD-P1-011` | P1 | Mouse IPC неверно определяет, создано ли новое mapping | `S15` | Verified | immediate `CreateFileMappingW` disposition capture, preserve/validate-existing runtime self-test and production init trace passed in V14-10A |
| `HJ-AUD-P1-012` | P1 | Named IPC analog host допускает precreation/spoofing в той же сессии пользователя | `S15` | Verified | named transport removed; explicit inherited-handle list, v10 owner/token schema, child/parent identity checks and invalid-handle restart gate passed in V14-10B |
| `HJ-AUD-P1-013` | P1 | Встроенный dependency installer имеет TOCTOU и неполную проверку цепочки поставки | `S18` | Open | installer hash/TOCTOU/UI-thread tests |
| `HJ-AUD-P1-014` | P1 | Dependency installer может навсегда заморозить UI | `S18` | Open | installer hash/TOCTOU/UI-thread tests |
| `HJ-AUD-P1-015` | P1 | UAP plugin не защищает C ABI от исключений и использует ручные lock/unlock | `S10` | Verified | common C ABI barrier, portable exception/RAII test and no-manual-lock static audit passed in V14-07C |
| `HJ-AUD-P1-016` | P1 | UAP unload выполняет неограниченный join, удерживая глобальный devices mutex | `S10` | Verified | bounded plugin joins occur outside devices mutex; ABI runtime unload and child-process containment gates passed in V14-07C |
| `HJ-AUD-P2-001` | P2 | Overlay server обслуживает только одного клиента синхронно | `S16` | Verified | fixed 16-client worker table, 8 slow plus 8 parallel state requests, prompt saturation rejection and active-client shutdown passed in V14-10D |
| `HJ-AUD-P2-002` | P2 | Overlay HTTP parser не реализует framing | `S16` | Verified | incremental fragmentation/pipelining/body framing, strict length/transfer-coding rejection and 8/4/2 KiB socket limit gates passed in V14-10C |
| `HJ-AUD-P2-003` | P2 | Overlay telemetry parser допускает unsigned overflow | `S16` | Verified | exact-key `from_chars`, duplicate/junk/overflow rejection and one-billion bound passed simulator and production socket gates in V14-10C |
| `HJ-AUD-P2-004` | P2 | Overlay endpoint открыт любому origin | `S16` | Verified | per-generation 128-bit session cookie, exact loopback origin echo and hostile/null-origin rejection passed simulator and production gates in V14-10D |
| `HJ-AUD-P2-005` | P2 | Mouse IPC читает interlocked-written поля обычными volatile reads | `S15` | Verified | peer-owned attach/heartbeat and schema fields use interlocked reads; static audit and simulator policy self-test passed in V14-10A |
| `HJ-AUD-P2-006` | P2 | UAP poll workers собраны без какого-либо pacing | `S17` | Verified | exact production policy, 10,232 deadline properties, error-backoff/model tests, GCC/MSVC, ASan+UBSan, build and regression pass; no physical USB claim |
| `HJ-AUD-P2-007` | P2 | UAP device identity нестабильна для двух одинаковых устройств | `S17` | Verified | path-based production identity passed 40,320 permutations, 100,000 reconnect generations, 250,000-path collision smoke, fallbacks, golden vectors and three toolchains |
| `HJ-AUD-P2-008` | P2 | `is_initialised()` plugin всегда возвращает `true` | `S10` | Verified | runtime ABI gate proves false before init, true after init and false after bounded unload |
| `HJ-AUD-P2-009` | P2 | `_device_info` не проверяет `buffer == nullptr` | `S10` | Verified | runtime ABI gate proves null device-info and full-buffer arguments return zero without a fault |
| `HJ-AUD-P2-010` | P2 | Snapshot export удерживает глобальный devices mutex при копировании всех значений | `S17` | Verified | V14-11C bounded owner pins, blocked-reader removal, lifetime/coherence stress and sanitizer/build/ABI gates passed |
| `HJ-AUD-P2-011` | P2 | Сохранение `settings.ini` может заменить хороший файл неполным temp | `S13` | Verified | unique same-directory temp, checked flush/readback/replace and five injected failure stages preserve the known-good hash |
| `HJ-AUD-P2-012` | P2 | Overlay settings сохраняются напрямую и всегда сообщают успех | `S13` | Verified | overlay copies the base into a transaction, checks every write/readback and reports exact failure stage |
| `HJ-AUD-P2-013` | P2 | Profile stream не проверяется после flush/close | `S13` | Verified | stream flush/good/close state, schema readback and all-stage bindings fault probe passed |
| `HJ-AUD-P2-014` | P2 | Layout preset очищается и пишется неатомарно без проверки ошибок | `S13` | Verified | checked transaction, schema/exact-entry readback and five injected stages preserve the layout probe hash |
| `HJ-AUD-P2-015` | P2 | Curve preset writer также игнорирует результаты записей | `S13` | Verified | curve preset/state checked writes, exact readback and five injected stages preserve both probe hashes |
| `HJ-AUD-P2-016` | P2 | Большинство вызовов сохранения игнорируют возвращаемую ошибку | `S13` | Verified | central trace/UI reporting covers autosave; profile/layout/curve manual paths retain or roll back state and report partial rename |
| `HJ-AUD-P2-017` | P2 | Writable state хранится рядом с executable | `S14` | Verified | LocalAppData default, explicit writable portable marker, source-preserving transactional migration/replay and five-stage failure gate passed |
| `HJ-AUD-P2-018` | P2 | Имена профилей недостаточно нормализованы | `S14` | Verified | shared NFC/case/reserved-name/length/direct-child policy and Unicode collision tests passed |
| `HJ-AUD-P2-019` | P2 | Публикация curve settings имеет слабый memory-order contract | `S11` | Verified | release generation publication, acquire cache observation and concurrency test passed in V14-08A |
| `HJ-AUD-P2-020` | P2 | Глобальный lifecycle registry не защищён и не кодирует thread affinity | `S03` | Verified | mutex serialization, owner token and wrong-thread fault test passed |
| `HJ-AUD-P2-021` | P2 | VID:PID ownership слишком крупнозернистый | `S17` | Verified | V14-11D exact path claims, same-pair sibling/reorder/collision tests, shared Soup pre-open hook, three toolchains, ABI/build and Irok regression passed |
| `HJ-AUD-P3-001` | P3 | TESTING.md описывает отсутствующий тестовый контур | `S20` | Open | build/docs/manifest validation |
| `HJ-AUD-P3-002` | P3 | Внутренние README/BUILD документы относятся к другим поколениям проекта | `S20` | Open | build/docs/manifest validation |
| `HJ-AUD-P3-003` | P3 | Один static audit сам устарел относительно текущей архитектуры | `S20` | Open | build/docs/manifest validation |
| `HJ-AUD-P3-004` | P3 | Validation docs переоценивают lifecycle validation | `S20` | Open | build/docs/manifest validation |
| `HJ-AUD-P3-005` | P3 | Release Win32 конфигурация ссылается на x64 ViGEm library | `S20` | Open | build/docs/manifest validation |
| `HJ-AUD-P3-006` | P3 | Проект компилируется только с Warning Level 3 | `S20` | Open | build/docs/manifest validation |
| `HJ-AUD-P3-007` | P3 | Корневой README не перечисляет реальные build dependencies | `S20` | Verified | README dependency list and independent clean-room build pass |
| `HJ-AUD-P3-008` | P3 | Sun build tool берётся с moving branch | `S20` | Verified | exact locked commit, fresh fetch and independent build pass |

### Evidence for HJ-AUD-P2-006

V14-11A removes every production `UAP_POLL_SLEEP_MS=0` target and applies a
1000 us start-to-start deadline only when Soup classifies the device as a poll
transport. Fast successful cycles wait only for the unused deadline; cycles
that already took at least 1 ms receive no additional delay. Madlions report
failures back off 2, 4, 8, 16, 32 and 64 ms, remain capped at 64 ms and reset
after success. Report-stream Wooting, Razer and NuPhy devices do not enter the
pacing branch.

The deterministic portable model reduced a 50 us transaction from 20,000 to
1,000 calls per modeled second and from 1,000,000 to 50,000 us of modeled busy
time. The static audit, all 19 portable tests, rebuilt ABI1 load/name/null/
bounded-unload gate, official MSVC build and production Irok regression pass.
The Irok uses the native SparkLink route, not UAP, so no real UAP CPU, USB or
sampling claim is made. Because UAP hardware is unavailable, D-022 accepts the
exact production-code property/sanitizer/build gates for this code-level risk;
the risk is `Verified` without upgrading that evidence into a physical claim.

### Evidence for HJ-AUD-P2-007

V14-11B replaces metadata plus enumeration occurrence with a versioned hash of
the normalized Soup HID interface path and the numeric HID descriptor. Case and
slash variants normalize identically; length-framed fields prevent ambiguous
concatenation. When a path exists, occurrence is ignored and both telemetry
ABIs publish `DuplicateSafeId`. If path is absent, metadata plus occurrence is
still unique but the flag is deliberately absent and the UI says fallback.

The exact header used by production passed all 40,320 enumeration orders of
eight identical fake devices, 100,000 shuffled reconnect/subset generations,
250,000 unique fake paths without a collision, 1,024 fallback occurrences,
descriptor/path/case boundaries and four persisted-ID golden vectors. GCC 15,
MSVC 19.44 and Clang 21 ASan+UBSan pass. A 64-bit ABI can never offer a
mathematical no-collision proof; the test guards practical regressions. A
serial-less keyboard moved to a different USB port intentionally follows the
new interface path because software cannot infer physical sameness.

### Evidence for HJ-AUD-P2-010

V14-11C changes the device registry from exclusive pointers to ref-counted
owners. The exact production helper locks `devices_mtx` only while copying at
most eight `shared_ptr` values into a fixed array. Telemetry collection,
per-device snapshot locking and all 256-value copies happen after the helper
returns and the registry lock has been released. Hotplug removal pins the
callback owner, and unload pins its worker set before cancel/join outside the
registry lock.

The portable test removes the sole registry entry while its pinned reader is
deliberately blocked on the device snapshot mutex, proves destruction is
deferred until the reader releases its pin, executes 100,000 exact-destruction
cycles and validates at least 50,000 coherent 256-value reads during concurrent
publication. GCC 15, MSVC 19.44 `/W4 /WX`, Clang 21 ASan+UBSan, 37 static
audits, 21 portable tests, the official build, ABI1 v12 load/unload and native
Irok regression pass. These are code-level contention/lifetime proofs under
D-022; they do not measure physical UAP latency.

### Evidence for HJ-AUD-P2-021

V14-11D keys native ownership by a compact fingerprint of the complete
normalized HID interface path rather than VID/PID. The common implementation
normalizes ASCII case and slash direction, hashes UTF-16 units, includes the
unit count and performs exact semicolon-token matching. The first successful
protocol proof owns only that path; a sibling interface with identical VID/PID
remains independently classifiable. MAD68, Hex80, Addressed, SparkLink and Sayo
all reject foreign path claims before opening HID and claim the exact path they
proved. Soup calls the same plugin hook before its first `CreateFileW`.

The production implementation passes same-pair sibling vectors, 10,000
reorder/reconnect generations, exact prefix/suffix rejection and a 300,000-path
collision smoke under GCC 15, MSVC 19.44 `/W4 /WX` and Clang 21 ASan+UBSan.
All 38 static audits, 22 portable tests, the exact ABI1 v13 gate, official build
and a 45,873/45,874-query native Irok regression pass. D-022 permits code-level
verification because no UAP or multi-UAP hardware is available; this does not
claim physical UAP coexistence or mathematically exclude every 64-bit collision.

## Evidence package S01

S01 добавил общий lifecycle-контракт, generation-aware state machine и fault-injection seam. V14-05 подключил этот контракт к production registry и backend callbacks, поэтому `HJ-AUD-P1-003` и `HJ-AUD-P2-020` закрыты проверенными локальными gates. V14-06A-F удалили все ordinary `TerminateThread` и закрыли Sayo reader exception boundary локальными fault-injection gates. V14-07C закрыл UAP worker/C ABI часть. `HJ-AUD-P1-001` остаётся `Implemented`, потому что `Verified` требует финального soak/device qualification; открытая часть `HJ-AUD-P1-004` теперь ограничена отложенными device-owner и финальными soak gates.

## Evidence package S02A

S02A добавил общий allocation-free `RunWorkerEntryBarrier`, portable unit tests и `noexcept` wrappers для realtime, diagnostic writer и overlay worker. При C++-исключении:

- realtime закрывает run/alive publications, освобождает MMCSS/timer/time-period через RAII и публикует нейтральное UI/ViGEm состояние;
- logger немедленно закрывает producer gate и запрещает переинициализацию поверх не освобождённого writer generation;
- overlay сбрасывает running/port, закрывает worker-owned sockets, сохраняет WSA ownership до owner-side reap и блокирует restart до `Stop()`.

Normal scheduling/processing loops realtime, logger и overlay подтверждены побайтно одинаковыми относительно S01. S02A/S02A.1 затем прошли пользовательский clean Windows build и runtime smoke test. Риск остаётся `Partial`, потому что часть native backend workers и UAP exports ещё не покрыта.


## Evidence package S02B.1

S02B.1 намеренно ограничен двумя native backend'ами без вложенных worker-поколений: MAD68 и Hex80.

- OS entry обоих workers теперь `noexcept` и использует общий `RunWorkerEntryBarrier`;
- `std::exception` и неизвестные C++-исключения не выходят за thread boundary;
- fault path закрывает loop, сбрасывает native ownership/connected publications и обнуляет опубликованные analog values;
- completion всегда снимает `g_running`;
- перед заменой `std::thread` владелец reap'ит завершившееся поколение, поэтому restart после fault не вызывает `std::terminate`;
- normal polling bodies подтверждены посимвольно одинаковыми относительно S02A.1, кроме нового типа результата и финального `return 0u`;
- GCC/Clang common gates и ASan/UBSan portable suite прошли.

Статус риска остаётся `Partial`: доступный SparkLink regression smoke полного пакета S02B.1 пройден, но device-specific MAD68/Hex80 gate отложен до предфинального внешнего архива. Это не считается `Verified` для двух изменённых backend'ов.

## Evidence package S02B.2

S02B.2 ограничен SparkLink worker boundary и не меняет HID protocol/hot path.

- существующая SEH-защита сохранена внешним Win32 wrapper;
- C++-исключения проходят через общий allocation-free `RunWorkerEntryBarrier`;
- C++ и SEH fault paths обнуляют analog publications, очищают connected/freshness и будят realtime loop для neutral snapshot;
- fault закрывает write-capable publication и сохраняет fixed record/native SEH code;
- completion публикует `g_sparkWorkerExited`;
- `SparkStart()` проверяет ранний выход worker до и после connected publication, исключая ложный healthy start;
- Spark polling loop подтверждён посимвольно одинаковым относительно S02B.1;
- GCC/Clang common gates и ASan/UBSan portable suite прошли.

Пользовательский Windows/MSVC + реальный SparkLink gate S02B.2 пройден: analog input, ViGEm, режимы, reconnect и shutdown работали без регрессий до lifecycle-only V14-06D. V14-06D удалил `TerminateThread` и прошёл deterministic timeout containment; повторный device regression остаётся release qualification, а открытая часть S08 теперь ограничена Sayo.

## Evidence package S02B.3

S02B.3 ограничен Addressed exception containment и не меняет protocol/polling/overlapped hot path.

- main worker и вложенный HID reader получили отдельные `noexcept` barriers;
- reader HANDLE освобождается RAII при stack unwinding;
- owner session всегда join'ит reader до rethrow, поэтому joinable `std::thread` не вызывает `std::terminate`;
- exceptional cleanup очищает stack-backed scheduler pointer, pending response token и analog publication;
- повторный start reap'ит завершившееся main generation до замены `std::thread`;
- session polling loop, reader I/O loop и packet constructors token-identical S02B.2.

Статус риска остаётся `Partial`: Addressed device gate отложен до предфинального внешнего архива; UAP/C ABI ещё не покрыт. Sayo boundary закрыт локально в V14-06F, но его device gate остаётся release qualification. Риск `HJ-AUD-P1-005` не закрывается — cross-thread force-close активного overlapped HANDLE остаётся S06.

This paragraph records the historical S02B.3 result. V14-12B subsequently
removed the force-close path; the current status is the table entry above, with
physical Addressed qualification still deferred.

## Evidence package S02B.4 / V14-06F

V14-06F ограничен Sayo reader exception/completion boundary и не меняет discovery, depth protocol, mapping, polling interval или normalization.

- каждый Win32 reader входит через отдельный `noexcept` C++ barrier внутри SEH wrapper;
- C++ и structured faults сохраняют фиксированную per-reader диагностику, обнуляют published input и сигналят общий stop event;
- completion каждого reader публикуется независимо, а выход последнего reader снимает connected state и повторно нейтрализует input;
- startup publication отклоняет уже завершившуюся или faulted reader group, включая отдельную проверку publish race;
- simulator-only C++ exception injection, Sayo timeout containment, normal simulator, полный portable gate и production MSVC build прошли.

Статус `HJ-AUD-P1-004` остаётся `Partial` только из-за отложенных hardware/soak gates; UAP workers/C ABI закрыты V14-07C, а отсутствие реальной Sayo-клавиатуры не подменяется simulator evidence.

## Evidence package S09 / V14-07A

V14-07A ограничен parent-side analog-host generation и не меняет private UAP
ABI, plugin polling или keyboard protocol behavior.

- snapshot bridge и supervisor публикуются как одно lifecycle generation;
- успешный shutdown закрывает thread, IPC, event и job HANDLEs только после
  общего confirmed join;
- simulator-only supervisor-create failure останавливает и join'ит уже
  созданный bridge до освобождения IPC;
- две ограниченные shutdown-фазы покрывают graceful stop и child-job
  containment; незавершённый parent join сохраняет все reachable resources,
  переводит generation в `Poisoned` и запрещает restart;
- backend/application shutdown принимает этот результат и выбирает
  process-level containment без зависимого teardown;
- partial-start injection, bridge-timeout injection, normal simulator, полный
  portable/static gate и production MSVC build прошли.

На момент V14-07A эти доказательства закрывали `HJ-AUD-P1-006` и
`HJ-AUD-P1-007`, но ещё не меняли `HJ-AUD-P1-004`, `HJ-AUD-P1-015` или
`HJ-AUD-P1-016`; их последующее закрытие описано в V14-07C ниже.

## Evidence package S10 / V14-07C

V14-07C защищает приватный UAP на границе, непосредственно загружаемой
изолированным child host:

- общий `CAbiInvoke` не выпускает C++-исключения через C ABI и переводит plugin
  в faulted/restart-blocked state;
- все Soup mutex'ы освобождаются RAII guard'ами, ручных `lock/unlock` больше нет;
- `is_initialised()` отражает живое поколение, а null/zero-length входы
  `_device_info` и `_read_full_buffer` безопасно возвращают ноль;
- unload снимает global devices lock до cancel/join, ограничивает все ожидания
  общим deadline и очищает устройства только после confirmed completion;
- при неподтверждённом unload child host не делает `FreeLibrary`, а завершает
  disposable child process под уже проверенным parent/job containment.

Portable exception/RAII test, static audit, реальный ABI1 load/init/null/unload
gate, полный production build и process-clean smoke прошли. Это переводит
`HJ-AUD-P1-015`, `HJ-AUD-P1-016`, `HJ-AUD-P2-008` и `HJ-AUD-P2-009` в
`Verified`. `HJ-AUD-P1-004` остаётся `Partial` только до device-owner/soak
qualification в V14-12.

## Evidence package S11 / V14-08A

V14-08A делает запуск backend-зависимостей транзакцией и устраняет потерю
уведомлений без изменения keyboard protocols или ViGEm report construction:

- readiness публикуется только после успешного realtime, `AfterRealtime`, Raw
  Input prerequisite и `AfterRawInput`; отсутствие необязательного protocol
  family отличается от отказа уже обнаруженного устройства;
- любой отказ откатывает полученные стадии в обратном порядке; неподтверждённый
  stop останавливает дальнейший teardown и включает process containment;
- процессный монотонный input sequence не сбрасывается при старте/restart, а
  worker проверяет pending sequence до каждого `WaitOnAddress`;
- curve generation release-публикуется после atomic settings write и
  acquire-читается каждым thread-local cache до построения нового snapshot.

Portable race/publication tests, static audit, simulator-only realtime-start и
native-phase failure injections, normal common-pipeline simulator, официальный
production build и шестисекундный Irok MG75 Max smoke прошли. Это переводит
`HJ-AUD-P1-008`, `HJ-AUD-P1-009` и `HJ-AUD-P2-019` в `Verified`.
Синхронный ViGEm update был вынесен отдельно в V14-08B.

## Evidence package S12 / V14-08B

После начального подключения на startup-потоке runtime-владение ViGEm
передано отдельному output worker:

- realtime выполняет только неблокирующую публикацию полного newest-state
  batch; вызов драйвера, reconnect и destroy находятся вне `Backend_Tick`;
- при coalescing маски ожидающих virtual pads объединяются, а payload всех
  reports обновляется из самого нового полного snapshot;
- остановка worker ограничена тремя секундами. Неподтверждённый join сохраняет
  HANDLE и driver state, запрещает dependent teardown и выбирает process
  containment;
- simulator-only блокировка driver update на 60 секунд доказала, что realtime
  продолжает весь семисекундный сценарий и останавливается штатно до output
  timeout; production не содержит пути активации этой инъекции;
- normal simulator, полный native/portable gate, production build и Irok MG75
  Max smoke с чистым shutdown прошли.

`HJ-AUD-P1-010` переведён в `Verified`.

## Правило закрытия

Запись переводится в `Verified` только после выполнения acceptance criteria соответствующего пакета и сохранения test log в `docs/stability/tests/`. Один и тот же кодовый change может закрыть несколько записей, но каждая запись получает отдельное доказательство.

## Сводка

- P1: 4 open, 1 implemented, 1 partial, 10 verified.
- P2: 0 open, 21 verified.
- P3: 6 open, 2 verified.
- Всего: 10 open, 1 implemented, 1 partial, 33 verified.

## Дополнительный риск, обнаруженный при Windows gate S02A

| ID | Приоритет | Краткое описание | Пакет | Статус | Ключевая проверка |
|---|---|---|---|---|---|
| `HJ-DISC-P1-001` | P1 build blocker | Fresh Soup patch всегда падал: validator требовал marker, который generator не эмитил | `S02A.1` | Verified | clean Windows `BUILD.cmd` + generated `hwHid.cpp` ordering check |

S02A.1 добавляет marker внутрь генерируемого `$preOpenBlock` и отдельный audit generator/validator contract. Повторная чистая Windows-сборка и runtime smoke test пройдены; build blocker закрыт как `Verified`.

## Evidence infrastructure S02V1

S02V1 не меняет статус ни одного исходного риска и не считается исправлением production-дефекта. Пакет вводит временный доказательный слой для последующей приёмки:

- bounded 1 МиБ memory-mapped event trace;
- запрет disk I/O и flush в worker event paths;
- непрерывный sequence, balanced worker generations и обязательный final shutdown;
- машинный `PASS/WARN/FAIL`;
- проверяемый SparkLink/ViGEm сценарий;
- обязательное удаление специализированных событий после подтверждения пакета.

Ручной SparkLink smoke S02B.3 зафиксирован как положительное наблюдение, но evidence-verified статус для будущих runtime-пакетов требует trace bundle. `HJ-AUD-P1-004` остаётся `Partial`; UAP/C ABI boundaries ещё не завершены, а device-specific MAD68/Hex80/Addressed/Sayo gates отложены.

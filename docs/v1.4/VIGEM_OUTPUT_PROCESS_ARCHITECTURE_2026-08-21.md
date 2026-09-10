# ViGEm output process architecture decision

Дата: 21 августа 2026 года.

Пакеты: `R0`, `R1`, `V14-23A/B/D/F`.

Риски: `HJ-V14-P0-005`, `HJ-V14-P0-006`, частично
`HJ-V14-P1-029`, `031`, `032`.

Статус: F3 production route and local O1/O2/O3/O4/O5 gates are complete.
The legacy in-process owner is removed; exact fake, exact real ViGEmBus and
2,020-generation repeated runtime stress are green. Long-duration soak and the
immutable physical IROK/DrunkDeer candidate remain `HARDWARE_PENDING`.

## Problem / evidence

Текущий dedicated output thread правильно убирает ViGEm I/O из realtime, но
его lifecycle и Win32 resources остаются внутри главного процесса:

- realtime/control publishers читают обычный глобальный wake `HANDLE`, пока
  watchdog может закрыть и пересоздать его;
- health составлен из независимых `alive`, fault, lifecycle и raw thread-handle
  состояний;
- физический IROK run получил `WAIT_FAILED/ERROR_INVALID_HANDLE` на сохранённом
  thread handle, после чего generation навсегда стала poisoned;
- synchronous `vigem_target_x360_update` не имеет cancellation API; зависший
  вызов нельзя безопасно оборвать, не уничтожив process boundary;
- simulator уже подтверждает, что 60-second stall превышает join deadline и
  требует завершения всего HallJoy process.

Рабочий v1.4 EXE остаётся A/B baseline, но доступный output-worker source
совпадает с текущим. Следовательно, копирование старого участка не устраняет
класс defect.

## Affected invariants

1. Realtime только публикует newest complete XUSB snapshot и никогда не ждёт
   ViGEm/driver/lifecycle.
2. ViGEm client, targets, update, reconnect and destroy имеют одного owner.
3. Wake resource нельзя закрыть, пока существует publisher.
4. `Running` означает ready + progress живой generation.
5. Planned stop сначала публикует neutral и подтверждает его delivery.
6. Stalled/crashed output boundary завершается за hard deadline, reap-ится и
   только затем заменяется новой generation.
7. Старый child/generation не может прочитать или подтвердить новую command.
8. Recovery не требует переподключения клавиатуры и не зависит от digital input.
9. Parent shutdown всегда имеет zero-survivor bound.

## Рассмотренные варианты

### Option A — локальная in-process правка

Сделать wake event process-lifetime, обернуть thread handle в RAII, добавить
lock/pinning, ready/completion events и подробную provenance.

Плюсы:

- минимальный migration scope;
- сохраняет текущий mailbox и ViGEm code почти без изменений;
- устраняет известную close/use race wake event.

Минусы:

- зависший `vigem_target_x360_update` остаётся внутри HallJoy process;
- thread нельзя безопасно kill/restart из-за живого stack/request/ViGEm state;
- физический invalid thread handle можно лучше диагностировать, но hard recovery
  всё равно нельзя доказать;
- не закрывает общий процессный liveness requirement `P1-031`.

Verdict: отвергнут как конечная архитектура. Допустимы только инструментация и
characterization, которые потом используются новым boundary.

### Option B — новый generation-owner, но всё ещё in-process

Ввести `VigemOutputGeneration` с одним mutex-owner, private thread handle,
process-lifetime wake, ready/progress/completion, immutable health snapshot и
one-owner reap. Worker получает generation object вместо глобалов.

Плюсы:

- корректная ownership model;
- закрывает raw handle data race и stale callback;
- значительно улучшает диагностику и testability;
- полезен как staging design для child supervisor.

Минусы:

- OS thread остаётся неуничтожимой безопасной boundary при driver hang;
- timeout всё ещё означает poison/exit всего HallJoy;
- часть lifecycle затем будет переписана второй раз при process isolation.

Verdict: отвергнут как самостоятельный end state. Его generation/health model
нужно реализовать сразу на process supervisor side, не создавать временный
второй runtime.

### Option C — self-hosted ViGEm output process

Тот же `HallJoy.exe` запускается во внутреннем режиме
`--halljoy-vigem-output-host` до UI/logger initialization. Parent и child
получают только явно наследуемые unnamed handles и versioned shared state.

Parent:

- realtime публикует complete four-pad latest-value snapshot через bounded
  three-slot claimed-slot mailbox;
- process-lifetime wake event существует от output-client start до остановки
  realtime и не закрывается между child generations;
- один supervisor thread владеет child process/thread/job handles, commands,
  deadlines и reap;
- UI читает immutable health snapshot и не выполняет raw handle operations.

Child:

- единолично владеет ViGEm client/targets и всеми library calls;
- проверяет owner PID, inherited handles, nonce, magic/version/size и generation;
- применяет только newest complete snapshot;
- публикует ready/progress/update acknowledgement и exact error;
- на planned stop отправляет neutral, удаляет targets и подтверждает completion.

Supervisor:

- при crash/no-progress/stall закрывает generation для publication;
- требует child exit; после deadline завершает child job и ждёт process handle;
- никогда не запускает новую generation до подтверждённого reap старой;
- после нового ready запрашивает свежий realtime snapshot;
- имеет bounded startup, recovery and final shutdown независимо от UI timers.

Плюсы:

- единственный вариант с настоящим hard bound для synchronous ViGEm call;
- memory/SEH/driver-call failure child не повреждает UI/realtime process;
- naturally объединяет thread/process handle ownership у одного supervisor;
- повторно использует уже доказанные HallJoy self-host, inherited-handle, job,
  nonce и restart patterns analog-host;
- сохраняет lock-free/latest-value high-rate data plane и четыре XUSB targets.

Минусы:

- новый versioned IPC ABI и internal command mode;
- нужно тщательно доказать startup/install-error propagation, target removal,
  neutral-before-stop, child authenticity and no overlapping generations;
- larger migration and exact-EXE qualification scope.

Verdict: **выбран**. Это единственный вариант, закрывающий root ownership race и
hard-liveness boundary без `TerminateThread`, unsafe OVERLAPPED destruction или
полного завершения HallJoy.

## Target data/control contract

Shared state разделяется логически, даже если первая версия использует один
bounded mapping:

- header: magic, ABI version, byte size, owner PID, nonce;
- command plane: requested generation, run/stop/reconnect/config generations;
- data plane: three fixed slots with atomic `Empty/Writing/Ready/Reading`
  ownership, snapshot generation, timestamp, count, valid mask and complete
  SDK-independent `XusbReportV1[4]`;
- child telemetry plane: child PID, reported state, ready generation,
  heartbeat, progress/update acknowledgement and last ViGEm error;
- parent-authoritative lifecycle, deadlines and restart count remain private
  supervisor state and are never writable by the child;
- diagnostics: fixed enums/counters only; no paths, raw keys or free-form child
  strings in shared memory.

Every field has a single writer or an explicit atomic ownership transition.
Capacity is explicit. Parent never waits from realtime. Child never reads a
half-published report or accepts a stale generation. A claimed-slot protocol was
selected over the initially proposed plain-payload seqlock because concurrent
plain C++ reads/writes would retain a formal C++ data race even when sequence
validation detects the collision after copying. Only the owner of `Writing` or
`Reading` may touch a slot payload; the producer may reclaim `Ready`, never
`Reading`, so latest-value coalescing remains bounded and non-blocking.

## Old-bug oracles before production migration

### O1 — wake close/use/reuse

Deterministically pause a publisher after it acquires the legacy raw wake
handle, close/recreate the object on the owner path and reuse the numeric handle.
Legacy production contract must fail; target contract keeps the process-lifetime
wake object alive through all child restarts and passes at least 100,000
publish/restart interleavings.

Legacy result preserved on 22 August 2026: four forced production-path runs
paused the real publisher across the exact Stop/close/Create/Start sequence.
In the final run, stale generation-1 handle `0x284` was not the new HallJoy wake
(`0x29C`), yet `SetEvent(0x284)` returned success because the numeric value had
already been rebound to a foreign event object. This proves a silent wrong-
object signal, not merely a possible `ERROR_INVALID_HANDLE`. The target-safe
runner therefore remains RED. Evidence:
`../stability/tests/V14_R0_VIGEM_WAKE_CLOSE_USE_ORACLE_2026-08-22.txt`.

### O2 — invalid legacy thread handle

Simulator-only injection closes the legacy output thread handle immediately
before watchdog observation. Current exact path must reproduce
`WAIT_FAILED/ERROR_INVALID_HANDLE -> Poisoned -> recovery blocked`. Target
supervisor has no cross-thread-owned worker handle and must reap/restart the
child generation with exit zero.

Legacy result preserved on 21 August 2026: the simulator-only production-path
injection produced `WAIT_FAILED/ERROR_INVALID_HANDLE`, permanent
`watchdog.recover.blocked`, `shutdown.poisoned` and `session.end exit_code=2`.
The runner expects the target result `0`, therefore it remains RED until the
new supervisor actually recovers. Evidence:
`../stability/tests/V14_R0_VIGEM_INVALID_THREAD_HANDLE_ORACLE_2026-08-21.txt`.

### O3 — output call never returns

Child-only injection blocks inside the update path beyond the progress deadline.
Parent must neutralize by destroying/reaping the output job, start exactly one
new generation, resubmit newest complete state and remain responsive. No
HallJoy process exit and no child survivor are allowed.

### O4 — child exits at every lifecycle boundary

Inject exit before ready, after ready, before snapshot read, during update,
after acknowledgement and during planned neutral. Parent state must be truthful,
old acknowledgements ignored and generations balanced.

### O5 — stop/restart/output stress

Continuous changing four-pad reports plus hook toggle, profile save,
device-change and virtual-pad enable/count changes while repeatedly restarting
the child. Verify monotonic generation, newest-report equivalence, neutral final
state, bounded latency, no raw-handle failure and no overlap.

## Staged implementation

1. Add O1/O2 and legacy provenance; run against pre-change code and preserve
   failing evidence.
2. Add POD shared ABI and production-linked claimed-slot/state-machine tests.
3. Add self-host child dispatch with fake transport; no production routing yet.
4. Add parent supervisor/job/handle-list owner and O3/O4 process tests.
5. Move ViGEm create/update/reconnect/destroy unchanged into child.
6. Switch realtime publication to shared newest snapshot; remove legacy worker,
   lifecycle flags and generation-closeable wake.
7. Add install-error/UI recovery and planned neutral/target removal tests.
8. Run full simulator/fault/build/soak gates and exact-EXE IROK regression.

Stage 2 was reopened and completed as R1.1 on 22 August 2026. The fixed
640-byte V1 ABI and bounded three-slot channel compile in the production MSVC
target. Windows shared transitions use documented Interlocked operations; a
publisher lease closes disable/publication overlap; child telemetry cannot
impersonate parent lifecycle state. A real Windows parent/child test rejects a
wrong nonce, transfers 100,000 complete four-pad snapshots, quiesces
publication and reaps the child. The complete native suite passes. Legacy
production routing remains the sole ViGEm owner. Evidence:
`../stability/tests/V14_R1_1_VIGEM_OUTPUT_WINDOWS_IPC_CONTRACT_2026-08-22.txt`.

The next implementation stage is deliberately generic: prove one reusable
process-generation supervisor against a fake child (explicit inherited handle
list, job containment, startup/progress/stop deadlines, one-owner reap and
zero-overlap restart) before placing any ViGEm library calls in that child.

This generic F1 gate completed on 22 August 2026. The selected owner creates the
child suspended, assigns its private kill-on-close job before `ResumeThread`,
and retains process/job handles until confirmed reap. One instance completed
1,008 Windows generations across the full ready/progress/fault/timeout/stop
matrix with no overlap or survivor. It contains no ViGEm code and is not routed
from `backend.cpp`. Evidence:
`../stability/tests/V14_F1_PROCESS_GENERATION_SUPERVISOR_2026-08-22.txt`.

Stages 3/4 completed as F1.1 on 22 August 2026. The actual
`HallJoyV14Simulator.exe` branches before every normal startup action and runs
the fake output child through the same persistent session, R1.1 mapping and F1
owner selected for production migration. Nine generations cover normal clean
stop, all six O4 exit boundaries, O3 progress stall/force/reap and a clean
replacement which acknowledges the newest complete four-pad value. A process
exit during planned neutral is deliberately rejected as
`IncompletePlannedStop`. Transactional generation-bound child telemetry keeps
the ABI at 640 bytes and prevents a killed writer's odd transaction or stale
generation from admitting a replacement. No child survived. Evidence:
`../stability/tests/V14_F1_1_VIGEM_OUTPUT_SELF_HOST_FAKE_TRANSPORT_2026-08-22.txt`.

Stage 5 completed as F2 on 22 August 2026. A child-owned fixed-capacity RAII
transport now performs real ViGEm client connect, four target adds, explicit
initial neutral, snapshot update, planned neutral, target removal, disconnect
and free behind the same early command and session. A deterministic API matrix
exhausts every partial-start/update/stop failure edge. The actual simulator
image passed a four-target real ViGEmBus run and restored the pre-run PnP set;
the child was reaped with a qualified clean stop. Evidence:
`../stability/tests/V14_F2_VIGEM_OUTPUT_REAL_CHILD_TRANSPORT_2026-08-22.txt`.

Stage 6 completed as F3 on 22 August 2026. One `OutputRuntime` now owns the
proved process session and parent supervisor; `backend.cpp` contains no legacy
worker/wake/mailbox/reconnect owner or direct ViGEm client/target/update call.
Routed normal, O1/O2 exit/stall recovery, exact fake and exact real ViGEmBus
gates pass with strict reap-before-replacement and zero survivor.

Local O5 then added a same-image exact command: at least 100,000 publications
across 101 child generations, 50 pad-topology changes and 10 disable/enable
cycles. Twenty independent repetitions passed (at least 2,000,000 values and
2,020 child generations) with no overlap, unsafe generation, channel rebuild or
survivor. It also corrected topology/generation admission and separated pre-stop
admission closure from post-reap producer drain. Evidence:
`../stability/tests/V14_F3_VIGEM_OUTPUT_PRODUCTION_ROUTE_2026-08-22.txt`.

Physical V75 evidence subsequently reopened the ownership part of F3. At
135.641 seconds the parent supervisor received `ERROR_INVALID_HANDLE` for the
child-process handle, although the child had applied output immediately before
and the independent analogue reader continued without failure. Thus process
isolation removed the legacy thread handle but a private raw numeric process
handle was still not a kernel-enforced ownership boundary.

D-064 amends F3 without reintroducing a second owner. The same sole supervisor
now adopts process and job handles into non-copyable `ProtectedNativeHandle`
objects, sets `HANDLE_FLAG_PROTECT_FROM_CLOSE`, verifies protection and exact
PID, and clears protection only for confirmed owner close. A deliberate foreign
`CloseHandle` test must fail while the protected object remains usable. If any
other terminal invariant remains unprovable, the watchdog records one blocked
recovery and stops; it never constructs an overlapping session or floods the
log with retries. Hardware qualification is reopened until the exact amended
artifact passes long active output.

Each stage is a coherent testable package. There is no production mode where
both legacy and child output owners can submit to ViGEm simultaneously.

## Rollback boundary

Before each stage, hash-verified local backups preserve all edited sources,
tests, project files and documents. Until the routing switch, legacy production
behavior remains the sole owner. At the switch, rollback restores the complete
pre-switch set; individual old/new files are never mixed.

## Required release evidence

- old O1/O2 failure and new pass;
- fake-transport O3/O4/O5 pass under contention and compiler/static health;
- real ViGEm child startup/update/neutral/remove/restart exact-EXE pass;
- 1000 whole-application start/close cycles with zero survivors (pending);
- at least 1000 child restart cycles with zero survivors (local PASS: 2020);
- 8-hour active-input soak with progress and resource stability;
- physical IROK hook/profile/device-change/reconnect regression;
- physical DrunkDeer output-liveness and clean-shutdown regression;
- current risk, validation, worklog, security and release manifests updated.

Passing static tokens, increasing timeouts or retaining poison as the normal
recovery result does not satisfy this decision.

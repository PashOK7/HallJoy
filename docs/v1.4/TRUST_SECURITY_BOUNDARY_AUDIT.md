# Аудит границ доверия и безопасности HallJoy

Дата: 20 августа 2026 года.

Статус: завершённый статический аудит исходников, тестов, build pipeline и
текущих локальных release binaries. Исправления не выполнялись.

## Краткий вывод

HallJoy уже значительно безопаснее обычной «утилиты для клавиатуры»: web
overlay привязан к loopback и защищён origin/session contract, UAP работает в
job-owned child, parent/child IPC не имеет публичного имени, зависимости и CI
actions закреплены, persistence использует атомарную замену, приложение ничего
не скачивает и не просит elevation.

Однако границы доверия пока не замкнуты. Найдены четыре новые P1-группы:

- analog-host принимает произвольный DLL path от любого процесса, который сам
  создал согласованный набор inherited handles; child проверяет PID/nonce и
  ABI, но не доверенность owner image и не равенство DLL встроенному ресурсу;
- `LayoutPreset.Count` из внешнего INI не имеет верхней границы и до проверки
  строк напрямую управляет `vector::reserve` и числом итераций;
- диагностические сборки логируют полные HID paths, серийные номера, абсолютные
  пользовательские пути и последовательности аналоговых состояний без единого
  privacy/redaction contract;
- распространяемый `HallJoy.exe` и вложенный UAP не подписаны; соседний
  `SHA256SUMS.txt` обнаруживает случайную порчу, но сам не подтверждает автора.

Отдельно подтверждено, что существующий UAP child — fault-containment, но не
security sandbox: он работает с полным токеном пользователя. Native parsers и
HID I/O вообще остаются в UI process. Эта часть расширяет уже открытый
`HJ-V14-P1-023`, а не делает вид, что ещё один process автоматически создаёт
security boundary.

Рекомендуемый пакет — `V14-25`. Новые P1 `HJ-V14-P1-034` .. `037` являются
release blockers. Security-isolation требования присоединяются к
`HJ-V14-P1-023`; P2 hardening/debt не выдаются за доказанную эксплуатацию.

## Модель угроз и граница аудита

Недоверенными считаются:

- HID reports, descriptors, product/manufacturer/serial strings и timing;
- файлы layouts, profiles, settings и migration/reset markers;
- HTTP clients и web origins, обращающиеся к loopback overlay;
- command line и inherited objects внутренних process roles;
- содержимое writable runtime/log directories и замены файлов между проверкой
  и использованием;
- diagnostic artifacts, которые пользователь передаёт разработчику;
- release channel и полученный пользователем одиночный EXE.

Полностью враждебный administrator/kernel/driver вне границы приложения. Другой
процесс того же пользователя нельзя сделать бессильным на Windows, но HallJoy
не должен давать ему дополнительный signed-code execution primitive, принимать
поддельный IPC owner или случайно доверять pre-created objects.

Проверены:

- analog-host launch, inherited handles, owner identity, DLL extraction/load;
- overlay binding, HTTP framing, origin, session cookie и state endpoints;
- mouse bridge и остальные named kernel objects;
- app data paths, filename policy, migrations, reset и transactional writes;
- layouts/settings/bindings/profile parsers и их capacity limits;
- HID command/report boundaries и process placement native/UAP providers;
- diagnostic/crash logging и передаваемые identifiers/input data;
- dependency lock, CI action pinning, external download/elevation policy;
- MSVC/Sun flags и PE headers текущих `HallJoy.exe`/`abiv1.dll`;
- Authenticode status текущих release artifacts.

## Что уже сделано правильно

Следующие свойства нужно сохранить:

- analog-host mapping/events анонимны и передаются только inherited handles;
- `PROC_THREAD_ATTRIBUTE_HANDLE_LIST` ограничивает inheritance четырьмя
  требуемыми handles, несмотря на `bInheritHandles=TRUE`;
- shared state проверяет magic/version/size, owner PID, nonce и process handle;
- child находится в job с `KILL_ON_JOB_CLOSE`, а host PID сверяется parent;
- command line строится общим корректным Windows quoting helper;
- overlay слушает только `127.0.0.1`, ограничивает request/header/body/target,
  отвергает transfer encoding и дубли critical headers;
- hostile browser origins отвергаются, wildcard CORS отсутствует, `/state` и
  `/client_perf` требуют случайную session cookie;
- JSON labels экранируются, Origin отражается только после exact allow;
- private UAP bytes сравниваются со встроенным resource до обычного запуска;
- production не имеет downloader/updater/UAC flow; ViGEmBus ставится вручную;
- dependency sources, binary ViGEm input и GitHub actions закреплены commit/hash;
- file names нормализуются, path separators/reserved names отвергаются;
- data roots, reset targets и backup directories отвергают видимые reparse
  points, saves используют temporary-write/validate/atomic replace;
- protocol parsers имеют отдельные bounded tests/fuzz smoke, а analog-host
  отбрасывает invalid key codes, NaN и бесконечные значения;
- production logging выключен, crash-only report не содержит memory dump,
  command line или намеренно собранного hardware inventory;
- текущий HallJoy PE имеет ASLR, high-entropy VA, NX, security cookie и CFG
  instrumentation; UAP PE имеет ASLR, high-entropy VA, NX и security cookie.

Эти свойства не закрывают следующие defects.

## SEC-P01 — internal host может загрузить не встроенный UAP (P1)

### Доказательство

`AnalogHost_TryRunCommand()` распознаёт `--halljoy-analog-host` и получает:

- объявленный owner PID и nonce;
- mapping, stop event, snapshot event и owner-process handles;
- `privatePluginPath` как следующий произвольный command-line argument.

`RunHostImpl()` проверяет валидность handles, shared magic/version/size,
совпадение PID/nonce и `GetProcessId(ownerProcess)`. Это доказывает, что child
видит согласованные objects указанного owner. Но любой локальный процесс может
сам создать такие objects, записать согласованный PID/nonce, запустить
`HallJoy.exe` с inheritance и передать путь своей DLL.

Затем `LoadHostApi()` вызывает `LoadLibraryW(pluginPath)` и принимает DLL, если
она экспортирует требуемые имена и ABI version 1. Child не проверяет:

- что owner process image является тем же доверенным HallJoy artifact;
- что DLL bytes равны `IDR_UAP_ABIV1` текущего EXE;
- что path находится в контролируемом non-reparse runtime directory;
- что файл не заменён после parent `ResourceEqualsFile()` и до `LoadLibraryW`;
- безопасную dependency search policy загружаемой DLL.

Сейчас HallJoy не подписан, поэтому это не является повышением привилегий над
уже способным запускать процессы пользователем. После Authenticode/application
allow-list такой режим становится signed binary proxy execution primitive.
Независимо от подписи TOCTOU позволяет заменить проверенный writable DLL до
его фактической загрузки.

### Требуемое состояние

- internal host role не принимает arbitrary plugin path как authority;
- child самостоятельно доказывает exact embedded-resource digest перед load;
- owner process image/hash/signature и expected HallJoy role проверяются;
- runtime directory создаётся с owner-only policy и отвергает reparse points;
- verified DLL удерживается без write/delete sharing через проверку и load либо
  используется эквивалентная atomic identity scheme;
- `LoadLibraryExW` работает с явным safe search (`DLL_LOAD_DIR` + `SYSTEM32`),
  а process default DLL directories закрыты;
- чужая ABI-compatible DLL, path swap и поддельный owner reproducibly rejected;
- подпись HallJoy не превращает internal role в обход application control.

Риск: `HJ-V14-P1-034`.

## SEC-P02 — layout-файл управляет памятью без capacity bound (P1)

`KeyboardLayout::LoadPresetFile()` читает `LayoutPreset.Count` как signed int,
проверяет только `count > 0`, затем делает два `reserve((size_t)count)` и цикл до
`count`. Значение `2147483647` может вызвать огромную allocation, `bad_alloc`,
долгий startup/UI stall или crash ещё до семантической проверки элементов.

Кроме того, loader пропускает невалидные строки и принимает любой непустой
остаток. Это пересекается с уже открытым `HJ-V14-P2-011`; транзакционная загрузка
profiles — с `HJ-V14-P1-015`. Security-specific новый дефект здесь — управление
resource consumption внешним count до установленного лимита.

Требуемое состояние:

- общий `InputDocumentLimits` задаёт максимальные file bytes, sections, entries,
  label bytes/codepoints и keys;
- count проверяется до allocation и обязан совпадать с полностью разобранным
  количеством строк;
- truncated INI reads считаются ошибкой, а не частично валидным документом;
- parse/validate идёт в отдельную модель без live side effects;
- allocation/parse failure даёт понятную ошибку и сохраняет старую generation;
- malicious fixtures покрывают negative/zero/max-int, huge labels, duplicate
  fields, partial files, encoding errors и allocation-failure injection.

Риск: `HJ-V14-P1-035`.

## SEC-P03 — diagnostic logs не имеют единого privacy contract (P1)

В production обычный лог отключён, но support/diagnostic routes записывают:

- полные HID interface paths в SparkLink, Sayo, Aula MAX и MAD68 traces;
- сырые serial strings в MAD68 traces и устойчивые serial hashes в других;
- абсолютные settings/profile/runtime paths, раскрывающие имя Windows-профиля;
- manufacturer/product/firmware strings без общего удаления control characters;
- raw report bytes, analog positions/depth и последовательность активностей,
  по которой можно восстановить часть набранного пользователем текста.

Это особенно важно именно потому, что такие логи предлагается отправить
разработчику. Строки HID descriptor также являются внешними и могут вставить
переводы строк/поддельные события в текстовый журнал.

Требуемое состояние:

- один structured logger с обязательной schema и sanitation для external text;
- абсолютные user paths заменяются логическими roots и basename;
- serial/path identity использует случайно salted per-log correlation ID, не
  пригодный для сопоставления пользователя между логами;
- raw input diagnostics включаются отдельно, показывают понятное предупреждение
  о данных нажатий и ограничиваются нужным сценарием/устройством;
- обычная диагностика хранит агрегаты и изменения, а не поток всех матриц;
- лог явно содержит privacy schema/version и включённые data classes;
- CR/LF/control/bidi и oversized descriptor strings не могут подделать запись;
- automated privacy fixtures сканируют готовый support artifact на username,
  absolute home path, raw serial и запрещённые data classes.

Умное event/change logging и отсутствие общего ограничения размера файла не
конфликтуют с этой политикой: пользователь может тестировать сколько нужно,
но каждая записываемая строка должна быть необходимой и безопасной.

Риск: `HJ-V14-P1-036`.

## SEC-P04 — release EXE не имеет аутентифицированного происхождения (P1)

Проверка текущих файлов через `Get-AuthenticodeSignature` дала `NotSigned` для:

- `build/release/HallJoy.exe`;
- `third_party/UniversalAnalogPluginFixed/dist/universal-analog-plugin/abiv1.dll`.

PE Certificate Table у текущего EXE также пуст. Build создаёт
`SHA256SUMS.txt`, но файл лежит рядом и формируется тем же неаутентифицированным
процессом. Подмена EXE вместе с checksum не обнаруживается пользователем.

Требуемое состояние:

- production HallJoy получает Authenticode signature с timestamp;
- release manifest содержит hashes EXE, embedded UAP resource, source revision,
  dependency lock и toolchain identity;
- manifest/provenance подписан отдельным release identity;
- build сначала завершает exact-artifact tests, затем подписывает ровно этот
  artifact, после чего проверяет signature/hash/resource без пересборки;
- diagnostics имеют отдельное явное имя/channel и не маскируются под stable;
- release gate отклоняет unsigned, expired/untrusted, wrong-subject и
  post-signature modified artifacts;
- internal host fix `SEC-P01` выполняется до превращения HallJoy в trusted
  signed loader.

Риск: `HJ-V14-P1-037`.

## Security containment: расширение HJ-V14-P1-023

Job object и отдельный UAP process ограничивают hang/crash lifetime, но child
сохраняет полный пользовательский token и может обращаться к файлам, сети и
другим процессам как HallJoy. Native HID parsers работают в UI process.

Поэтому `V14-21` native isolation должен стать не только reliability boundary:

- один provider broker получает минимальный набор inherited capabilities;
- parent никогда не доверяет provider counts, floats, strings или health без
  validation/capacity/generation checks;
- применяются подходящие process mitigation policies и запрет child creation;
- filesystem/network/process access убираются, если HID access позволяет;
- если restricted token/AppContainer несовместим с нужным HID, ограничение
  документируется физическим тестом, а не молча считается sandbox;
- compromise/fuzz harness подтверждает, что provider не может изменить UI
  state иначе чем через валидный data/control protocol;
- native provider fault не исполняет код в UI/realtime address space.

Новый дублирующий risk ID не создаётся: это security acceptance criteria уже
открытого `HJ-V14-P1-023`.

## SEC-P05 — binary hardening есть, но нет явной политики exact artifact (P2)

Текущий `HallJoy.exe` имеет `DYNAMIC_BASE`, `HIGH_ENTROPY_VA`, `NX_COMPAT`,
security cookie и `CF_INSTRUMENTED`. Текущий `abiv1.dll` имеет ASLR/NX/cookie,
но `GuardFlags=0`. В project/build inputs явно закреплены `/sdl` и warning
policy, но отсутствует единый обязательный manifest для CFG, Spectre mitigation,
CET compatibility и loader/process mitigations каждого ship-артефакта.

Нужно проверять PE headers/load config после финальной линковки и подписи, иметь
обоснованный профиль для EXE/UAP/provider broker и запрещать silent downgrade.
Отсутствие конкретного флага не объявляется эксплуатацией без threat/benchmark
анализа; поэтому это P2 defense-in-depth.

Риск: `HJ-V14-P2-019`.

## SEC-P06 — named/local endpoints имеют неявную same-user модель (P2)

Mouse bridge использует стабильное имя `Local\HallJoy_MouseBridge_v1` и
принимает уже существующий mapping с корректной schema. Любой same-user process
может pre-create его, имитировать `asiAttached/asiHeartbeat` или вызвать
availability failure. `DrunkDeerMtx` также имеет стабильное общее имя.

Overlay хорошо защищён от hostile web origins, но session cookie выдаётся
любому локальному клиенту, получившему `/`; это защита browser-origin, а не
аутентификация другого same-user process. Такое ограничение нужно назвать
честно.

Требуемое состояние:

- HallJoy является creator/owner mouse mapping и не принимает чужой payload как
  authoritative startup state;
- mapping имеет явную current-user DACL, publisher PID/start identity и
  direction-separated fields/generations;
- stale/pre-created/wrong-owner fixtures проверяют fail-closed поведение;
- cross-process protocol mutex names namespace/version/device-scoped и bounded;
- overlay документирует browser-origin threat model; exact Host/port и random
  capability URL рассматриваются как дополнительное hardening;
- ни один local endpoint не описывается как security-authenticated без
  доказанного секрета, недоступного предполагаемому противнику.

Риск: `HJ-V14-P2-020`.

## SEC-P07 — нет общего command-mutation и dependency security manifest (P2)

У HallJoy нет произвольного firmware updater и большинство native providers
ограничены hardcoded read/subscription commands. Это хороший результат. Но
классификация `read-only`, `volatile subscription`, `persistent configuration`,
`calibration`, `bootloader/firmware` существует только в разрозненных audits и
backend knowledge. Также dependency lock фиксирует версии, но не является SBOM
и не содержит vulnerability/advisory review status.

Нужно:

- для каждого provider иметь machine-readable emitted-command allowlist с
  mutability/persistence и required consent;
- запрещать persistent/firmware commands в обычном analog provider process;
- проверять фактически скомпилированный command set, а не только текст docs;
- выпускать SPDX/CycloneDX SBOM для EXE/UAP/static libraries/tool inputs;
- вести review/advisory status pinned dependencies и осознанный upgrade gate;
- не обновлять зависимости вслепую: protocol characterization и regression
  suite обязательны для каждого изменения.

Риск: `HJ-V14-P2-021`.

## Порядок реализации V14-25

### V14-25A — отрицательные security oracles

- forged analog-host owner + arbitrary ABI-compatible DLL;
- verified-DLL swap/reparse race;
- layout max-int/huge/truncated/allocation-failure corpus;
- diagnostic privacy/log-injection fixtures;
- unsigned/wrong-signer/post-signature mutation gate.

Каждый oracle сначала обязан воспроизводимо показать текущий дефект.

### V14-25B — закрыть executable trust chain

- exact embedded plugin identity в child;
- owner/runtime/reparse/safe-loader contract;
- устранение signed proxy loading;
- exact binary provenance и Authenticode pipeline.

### V14-25C — bounded documents и privacy

- единые document limits и parse/validate/commit;
- общий structured redaction layer;
- явное согласие на raw-input diagnostics;
- готовые support-artifact privacy scans.

### V14-25D — provider security boundary

- выполнить process isolation `HJ-V14-P1-023`;
- добавить capability-minimized broker и parent-side validation;
- доказать фактические ограничения token/mitigations на Windows/HID.

### V14-25E — defense-in-depth и qualification

- PE mitigation manifest/gate;
- named endpoint ownership tests;
- command mutability manifest и SBOM;
- fuzz/mutation/exact-final-EXE/security regression qualification.

## Что не является исправлением

- проверить только ABI export загружаемой DLL;
- сравнить DLL со resource в parent, закрыть файл и позже загрузить по path;
- подписать HallJoy до закрытия arbitrary internal DLL load;
- поймать `bad_alloc`, продолжив частично загруженный layout;
- захешировать серийник постоянным несолёным hash и назвать его анонимным;
- спрятать raw input log без предупреждения в «обычную диагностику»;
- считать job object или отдельный процесс security sandbox;
- считать loopback равным аутентификации любого локального клиента;
- включить набор compiler flags без проверки финального PE;
- автоматически обновить Soup/UAP/ViGEm ради нового номера версии.

## Решение по релизу

`HJ-V14-P1-034` .. `037` блокируют следующую стабильную версию. Security
acceptance criteria `HJ-V14-P1-023` закрываются вместе с общей provider
архитектурой, а не отдельным формальным sandbox-флажком. P2 `019` .. `021`
должны войти в тот же архитектурный цикл, но сами по себе не объявляются
доказанной удалённой эксплуатацией.

До `V14-25` нельзя утверждать, что одиночный EXE имеет замкнутую цепочку доверия
от release artifact до загруженного UAP и всех обрабатываемых внешних данных.

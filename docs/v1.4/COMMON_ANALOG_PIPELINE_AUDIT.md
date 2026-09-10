# Аудит общего analog pipeline HallJoy

Дата: 20 августа 2026 года.

Статус: завершённый статический аудит общей цепочки от уже разобранных UAP/native
источников до XUSB, UI и сохранённых профилей. Исправления исходного кода и
сборка не выполнялись. Физическая корректность без соответствующей клавиатуры
не объявляется. Git не использовался.

Связанные обязательные аудиты:

- [`UAP_ALL_KEYBOARDS_AUDIT.md`](UAP_ALL_KEYBOARDS_AUDIT.md);
- [`NATIVE_ALL_KEYBOARDS_AUDIT.md`](NATIVE_ALL_KEYBOARDS_AUDIT.md);
- [`CORRECTNESS_RELEASE_BLOCKERS.md`](CORRECTNESS_RELEASE_BLOCKERS.md).

## Краткий итог

После протокольного backend данные проходят следующую цепочку:

`UAP/native/digital/mouse -> raw max merge -> per-key/global curve -> bindings ->
SOCD -> complete XUSB report -> output mailbox -> ViGEm`.

Основная математика этой цепочки статически выглядит аккуратно: входы
ограничиваются диапазоном, нечисловые значения нейтрализуются, оси и триггеры
формируются из одного tick-local cache, XUSB сравнивается без потери малых
изменений, а блокирующий вызов ViGEm вынесен из realtime worker. Однако найдены
три новые P1-группы, которые блокируют следующий стабильный релиз:

| Приоритет | Область | Статический вывод |
|---|---|---|
| P1 | source arbitration / digital fallback | решение о разрешении цифровой симуляции вручную знает только о пяти старых native backend, не знает Aula MAX/W669 и принимается глобально для всех HID, а не по источнику и владельцу конкретной клавиши |
| P1 | загрузка и переключение профилей | load не валидирует schema/kind/completeness, изменяет live state по частям и сообщает успех для любого существующего пути; переключатель игнорирует оба результата и заранее сохраняет новое активное имя |
| P1 | end-to-end key domain | оси/триггеры и INI допускают 16-bit identity, кнопки, native storage, UI, overlay и bind capture ограничены 1..255; некорректные signed INI числа дополнительно заворачиваются в `uint16_t` |

Также найдены P2-дефекты: SOCD-память не сбрасывается при смене смысла pad/axis,
а layout/UI допускает неполные и дублирующиеся HID-записи, которые затем
становятся неоднозначными или визуально недоступными.

## Проверенная область

Статически проверены:

- `backend.cpp`: объединение источников, raw/filtered cache, curves, bind
  capture, SOCD, отчёты, UI snapshots и ViGEm publication;
- `native_analog_backend_registry.cpp`: общий ownership/connected contract;
- `backend_curve.cpp`, `key_settings.cpp`, `settings.cpp`: global/per-key curve
  publication и нормализация;
- `bindings.cpp`: оси, триггеры, button masks и pad compaction;
- `profile_ini.cpp`, `settings_ini.cpp`, `keyboard_subpages.cpp`: save/load и
  global profile switch;
- `keyboard_layout.cpp`, `keyboard_page_main.cpp`, `keyboard_ui.cpp`:
  layout-domain и desktop selection/tracking;
- `overlay_server.cpp`: снимок layout и публикация raw/output;
- output mailbox, reconnect/resubmit и emergency-neutral участки общей ViGEm
  цепочки.

В этот документ не дублируются внутренние дефекты конкретного UAP/native
протокола. Они считаются входными рисками и описаны в двух связанных аудитах.

## COMMON-P01 — цифровой fallback не является корректным source policy (P1)

### Что делает код

`HidCache` вручную содержит только пять флагов:

- SparkLink;
- Sayo;
- Addressed;
- MAD68 Pro R;
- Hex80.

`Backend_Tick` разрешает digital fallback только когда пользователь включил
эту функцию и все пять флагов ложны. Aula MAX и W669 уже находятся в общем
native catalog, но в это решение не входят. Сам registry при этом уже имеет
универсальные `NativeAnalogBackends_AnyConnected()` и per-HID результат
`NativeAnalogBackends_ReadMilli()` с полями `connected`/`owned`.

После глобального решения `ReadRaw01Cached` отдельно запрещает fallback для
`native.owned`, но UAP не предоставляет аналогичный per-HID ownership этой
ступени. В результате политика зависит от имени backend, а не от фактического
источника конкретного значения.

### Реальные неправильные состояния

1. Подключённый Spark/Sayo/Hex80/Addressed/MAD68 запрещает fallback для всех
   обычных клавиш и других цифровых клавиатур, даже если конкретный HID этим
   analog backend не принадлежит.
2. Подключённый Aula MAX/W669 такого глобального запрета не создаёт. Для
   принадлежащих ему HID спасает `native.owned`, для остальных fallback
   остаётся разрешённым.
3. Готовый UAP host также не является отдельным участником policy. При нулевом
   UAP analog value и включённой функции Windows key state может породить
   искусственную глубину вместо честного нуля.
4. При добавлении следующего native backend разработчик может корректно внести
   его в единый catalog, но забыть ещё один скрытый список в `HidCache`.

Digital fallback сам по себе допустим как явно включённый режим для цифровой
клавиатуры. Ошибка состоит в смешивании его с аналоговыми маршрутами без
явной identity источника и per-device/per-HID policy. Он не должен называться
измеренной глубиной аналоговой клавиатуры.

### Требование к исправлению

- один catalog-driven snapshot источников вместо ручного списка backend;
- явное различие `measured analog`, `digital simulation`, `mouse pseudo-input`;
- решение по конкретному device/HID, а не глобальное «подключена любая из пяти»;
- отсутствие binary-derived analog identity;
- тест одновременно подключённых analog и digital клавиатур, включая owned и
  unowned HID, UAP, Aula MAX и W669.

Связанный риск: `HJ-V14-P1-014`.

## COMMON-P02 — profile load не валидируется и не публикуется атомарно (P1)

### Отдельные save уже надёжнее load

`Profile_SaveIni` и `SettingsIni_SaveProfile` пишут временный файл, проверяют
`SchemaVersion`/`Kind` и атомарно заменяют целевой файл. Это хорошая граница.
Но соответствующие load-функции не используют эту валидацию и не имеют
стадии `parse -> validate -> commit`.

### Bindings load

`Profile_LoadIni` проверяет лишь, что `GetFileAttributesW` не вернул ошибку.
Он не отвергает каталог, не проверяет `SchemaVersion=1`, `Kind=Bindings`, число
pad или полноту секций. Затем он немедленно очищает все live bindings, читает
отсутствующие поля как ноль и безусловно возвращает `true`.

Следствия:

- пустой или повреждённый существующий INI очищает bindings и считается
  успешно загруженным;
- частичный файл создаёт частичный live profile вместо fail-closed результата;
- realtime worker может увидеть последовательность промежуточных состояний:
  сначала очищенные bindings, затем pad 1, затем pad 2 и так далее;
- XUSB report в этот период может быть нейтральным или смешанным из старого и
  нового профиля.

### Settings/curve load

`SettingsIni_Load_Core` также проверяет только существование пути. Для profile
load отсутствующие значения превращаются в стабильные defaults, после чего
глобальные настройки применяются множеством отдельных setter-вызовов.
`KeySettingsIni_LoadFromSettingsIni` сначала очищает все per-key overrides, а
затем восстанавливает их по одному. Функция снова безусловно возвращает
`true`.

Каждый curve setter корректно инвалидирует cache, а одна per-key запись имеет
согласованный seqlock-like snapshot. Но транзакции уровня всего профиля нет:
один XUSB report может использовать переходный набор global curve/per-key
curve/SOCD settings.

### Переключение global profile

`Global_ApplyActiveGlobalProfile`:

1. независимо сохраняет settings и bindings предыдущего профиля;
2. при успехе первого save и ошибке второго оставляет пару файлов частично
   обновлённой;
3. сохраняет новое active profile name в основном `settings.ini` до загрузки;
4. игнорирует результаты `SettingsIni_LoadProfile` и `Profile_LoadIni`;
5. не имеет rollback runtime state или active name при ошибке/повреждении.

Поэтому отсутствующая половина пары, повреждённый файл или ошибка чтения могут
оставить runtime и сохранённое active name в разных состояниях.

### Требование к исправлению

- обе половины профиля сначала полностью читаются в отдельную immutable model;
- schema/kind, обязательные поля, диапазоны и семантические ограничения
  проверяются до live mutation;
- settings + per-key curves + bindings + pad count публикуются одной generation;
- realtime tick видит либо полностью старую, либо полностью новую generation;
- active name сохраняется только после успешного commit;
- ошибка любой половины не меняет runtime и не изменяет предыдущую пару файлов;
- legacy формат принимается только через отдельный явный migration path;
- нужны malformed/missing/partial/wrong-kind/negative/overflow и concurrent
  realtime regressions.

Связанный риск: `HJ-V14-P1-015`.

## COMMON-P03 — key identity несовместима между ступенями (P1)

Эта проблема продолжает UAP/native findings, но общий pipeline сам усиливает
несовместимость:

| Ступень | Фактический диапазон/поведение |
|---|---|
| Axis binding | два `uint16_t`, значение `1..65535` принимается |
| Trigger binding | `uint16_t`, значение `1..65535` принимается |
| Gamepad button binding | четыре 64-bit mask, только `1..255` |
| Native raw values | ABI `uint16_t`, production storage практически везде 256 |
| UAP full-buffer merge | коды `>=256` отбрасываются |
| UI raw/output tracking | массивы и dirty mask только на 256 |
| Bind capture | сканирует только `1..255` |
| Desktop keyboard selection | `g_btnByHid[256]`; key `>=256` нельзя выбрать |
| Browser overlay | key сохраняется в layout JSON, но raw/out принудительно равны нулю |
| Layout preset | parser и editor принимают `0..65535` |
| Per-key curve storage | имеет slow path для `>=256`, который UI/input path практически не может использовать |

Дополнительно `ReadU16` в bindings profile просто приводит результат
`GetPrivateProfileIntW` к `uint16_t`. Отрицательное или слишком большое число
может завернуться modulo 65536 и создать скрытый axis/trigger binding, который
невозможно увидеть или захватить обычным UI. Button CSV, напротив, отбрасывает
всё вне `1..255`, поэтому разные action type читают один INI-домен по-разному.

Это не чинится добавлением Menu/Fn в одну таблицу. Нужен единый versioned key
identity contract для transport, aggregation, serialization, bindings, UI и
overlay. До этого специальная клавиша может работать как ось, не работать как
кнопка, отображаться всегда отпущенной и теряться при другом способе rebinding.

### Требование к исправлению

- один тип identity с явными namespace/page/usage или другим versioned
  представлением, а не псевдо-HID внутри свободных чисел;
- одинаковая допустимость identity для axes, triggers и buttons;
- строгий parser без wrap, duplicate и invalid code ambiguity;
- migration старых 8-bit профилей;
- UI, bind capture и overlay поддерживают тот же домен;
- regression для Fn, Menu/Context, media/OEM, modifier, duplicate device и
  unknown identity.

Связанные риски: `HJ-V14-P0-001`, `HJ-V14-P1-011` и новый
`HJ-V14-P1-016`.

## COMMON-P04 — SOCD state переживает смену смысла axis/pad (P2)

Snappy/Last Key Priority хранит для каждого pad/axis:

- предыдущие `minusDown`/`plusDown`;
- последнее направление;
- analog valleys для повторного приоритета.

В общей цепочке нет найденного reset этих массивов при:

- загрузке или переключении профиля;
- rebinding minus/plus HID;
- выключении и повторном включении Snappy/LKP;
- удалении среднего virtual pad и compact последующих pad;
- disconnect/reconnect источника.

Когда оба режима выключены, функция возвращает до обновления state. После
повторного включения новые bindings могут унаследовать edge/last-direction от
старых клавиш. При pad compaction bindings сдвигаются, а SOCD state остаётся на
старом индексе. Это способно выбрать не то направление при двух уже нажатых
клавишах, пока не возникнет новый edge.

Требуется generation-tagged SOCD state или явный reset при любой смене
семантики оси/pad/mode, с тестами held-both во время profile switch, rebind,
toggle и pad compaction.

Связанный риск: `HJ-V14-P2-010`.

## COMMON-P05 — layout допускает неоднозначные и частичные модели (P2)

`LoadPresetFile` принимает объявленный `Count`, но пропускает отдельные
неразбираемые `K<n>` и считает preset годным, если осталась хотя бы одна
клавиша. Он не проверяет schema, точное совпадение count, уникальность HID или
поддерживаемый key domain. Editor также разрешает duplicate HID и значения до
65535.

В desktop UI каждый `hid < 256` сохраняется в единственный
`g_btnByHid[hid]`. При duplicate последняя кнопка перезаписывает ссылку на
предыдущую: selection/invalidation относятся только к последней, а первая
может отображать устаревшее состояние. Кнопка с `hid >= 256` создаётся, но не
попадает в tracking и не может стать обычной selected key. Overlay использует
безопасный immutable layout snapshot, однако публикует для такой клавиши
постоянные нули.

Нужны полная валидация preset до commit, явная политика duplicate physical
keys/identity и единый domain с COMMON-P03. Частично повреждённый custom preset
должен быть отвергнут целиком или восстановлен через явно показанный repair,
а не молча усечён.

Связанный риск: `HJ-V14-P2-011`.

## COMMON-P06 — UI telemetry не является одним согласованным снимком (P2)

Raw и filtered UI values хранятся в разных atomic arrays. Overlay для каждой
клавиши читает `BackendUI_GetRawMilli`, затем отдельно
`BackendUI_GetAnalogMilli`; между чтениями может пройти следующий realtime
tick. В пределах JSON один key или разные keys поэтому могут относиться к
соседним generations.

Это не затрагивает XUSB: отчёт строится из tick-local `HidCache`. Проблема
ограничена диагностической правдивостью UI/overlay. Для расследования
раскладки или raw-vs-curve графика важен generation-coherent snapshot с
timestamp/source identity, иначе редкая несовместимая пара может выглядеть как
ошибка кривой.

Этот пункт пока учитывается внутри `HJ-V14-P2-011`, чтобы не раздувать register
отдельным низкоуровневым UI риском.

## Что статически выглядит правильно

Следующие свойства подтверждены как сильные стороны текущей реализации, но не
заменяют физическую проверку:

- native catalog объединяет значения одного HID через `max`, а per-tick
  `HidCache` не выполняет повторные чтения одной клавиши;
- отрицательные/NaN UAP значения нейтрализуются, итоговый raw ограничивается
  `0..1`;
- global deadzone low/high хранится одной packed atomic парой;
- одна per-key curve публикуется согласованным seqlock-like snapshot;
- curve normalization ограничивает endpoints/control points и защищает от
  вырожденного X-диапазона;
- axis binding minus/plus читается одной packed atomic операцией;
- XUSB report строится целиком из одного tick-local cache;
- сравнение XUSB не отбрасывает изменения на один integer step;
- ViGEm update выполняет отдельный output owner, а не realtime worker;
- при driver error запрашивается reconnect/resubmit; realtime fault очищает
  UI/reports и запрашивает emergency neutral;
- layout для overlay публикуется immutable shared snapshot, поэтому сам vector
  не читается одновременно с его изменением.

## Почему существующие зелёные gates этого не обнаружили

Проверены и сами релевантные tests/gates. Они полезны, но их область уже, чем
может показаться по названию:

- `persistence_transaction_static_audit.py` строго проверяет атомарность
  **save** отдельных файлов. Он ищет transaction writer/flush/validate/replace
  и отказ переключения после ошибки сохранения старого профиля, но не требует
  schema validation в `Profile_LoadIni`/`SettingsIni_LoadProfile`, staging двух
  загружаемых файлов, проверки return values или rollback active name;
- `startup_wake_transaction_static_audit.py` проверяет ordered generation
  primitive для curve cache, но не один согласованный snapshot всего global
  profile и не atomic commit settings + per-key curves + bindings;
- native routing static tests требуют `cache.allowFallback && !native.owned`,
  то есть защищают принадлежащий native HID, но не проверяют ручной глобальный
  список connected backend, одновременные устройства или UAP ownership;
- обычный simulator проверяет только нейтрализацию противоположных W/S и A/D
  при стандартном сложении. Он не выполняет held-both switch/rebind/toggle/pad
  compaction для Snappy/LKP;
- не найден executable regression, который пропускает Fn/Menu/media/extended
  identity через layout -> capture -> каждый action type -> save/load -> UI ->
  XUSB.

Следовательно, прошлый зелёный persistence/output/curve gate остаётся честным
доказательством своей узкой области, но не закрывает COMMON-P01..P06. Для
будущего исправления нужны поведенческие tests состояния, а не только static
marker checks.

## Обязательная последовательность исправления

1. Спроектировать единый versioned key identity и migration; без него нельзя
   честно исправить специальные клавиши только на одной ступени.
2. Заменить ручной source arbitration на catalog/per-device/per-HID contract и
   отделить измеренный analog от digital simulation.
3. Ввести immutable complete runtime profile и atomic generation commit для
   settings, curves, bindings, pad count и SOCD reset.
4. Перевести buttons, UI, bind capture, layout и overlay на единый domain.
5. Добавить generation-coherent diagnostic snapshot с source/freshness.
6. Только затем исправлять отдельные UAP/native routes и проводить общий
   end-to-end XUSB matrix, иначе локальные fixes снова упрётся в несовместимый
   downstream contract.

## Граница этого аудита

Это доказательство по исходникам, а не готовый fix и не новый stable build.
Ни одна проблема не закрыта отсутствием пользовательских жалоб, simulator-only
проверкой или тестом другой модели. P0/P1 из трёх обязательных аудитов остаются
release blockers до выполнения критериев
`CORRECTNESS_RELEASE_BLOCKERS.md`.

## 2026-08-22 R2-A/B1 implementation progress

R2-A/B1 adds a production-compiled `KeyIdentityV1` with separate USB HID, UAP
extended and HallJoy semantic namespaces. Collision/invalid/Fn/Menu/media tests
pass, including equal numeric codes in different namespaces. This establishes
the target identity but does not migrate bindings, INI, UI, bind capture,
overlay or output. COMMON-P03 therefore remains open until those surfaces and
legacy profile migration pass end-to-end.

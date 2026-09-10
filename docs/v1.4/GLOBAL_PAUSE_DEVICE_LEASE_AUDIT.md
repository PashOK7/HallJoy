# Аудит глобального выключателя и передачи HID-владения HallJoy

Дата: 20 августа 2026 года.

Статус: завершённый статический аудит и обязательный проект. Исходный код не
изменялся; текущая production-версия не имеет этого контракта.

### Дополнение 2026-09-06 — внешний budget containment

Внешний процессный watchdog HallJoy установлен в 30 секунд. Это не меняет
семантику паузы и не разрешает суммировать локальные таймауты: каждый owner
по-прежнему обязан иметь bounded join, retain ресурсов и poison при
неподтверждённом завершении. 30 секунд — только последний абсолютный предел,
который покрывает один in-flight runtime-supervisor recovery и bounded
ViGEm/UAP containment до принудительного завершения процесса.

## Краткий вывод

HallJoy нельзя надёжно использовать одновременно с web-драйвером клавиатуры.
Это не дефект одного производителя: оба приложения могут открыть один
vendor-defined HID, читать один поток и отправлять несогласованные команды.
Часть протоколов дополнительно требует эксклюзивную сессию, потому что ответы
не имеют transaction ID.

Большинство HallJoy backends и UAP уже открывают HID с
`FILE_SHARE_READ | FILE_SHARE_WRITE`. Поэтому добавление share-флагов не решает
задачу: следующий клиент может требовать exclusive open, а две shared
управляющие сессии всё равно не получают протокольной сериализации.

Нужен один глобальный runtime-рубильник HallJoy, который оставляет UI открытым,
но атомарно нейтрализует весь создаваемый ввод, останавливает providers и
полностью освобождает HID до явного возобновления. Это пакет `V14-24` и
release-blocking риск `HJ-V14-P1-033`.

## Доказательства текущего конфликта

- UAP/Soup открывает vendor HID для read/write с shared read/write в
  `third_party/UniversalAnalogPluginFixed/overlay/Soup/soup/hwHid.cpp`;
- DrunkDeer, Addressed, MAD68 Pro R, Hex80, SparkLink и Sayo также используют
  shared handles;
- Aula WIN60HE MAX открывает command channel эксклюзивно: протокол не имеет
  transaction ID;
- W669 и IROK могут выбирать shared или exclusive сессию по доказанному режиму;
- внутренний exact-interface routing предотвращает конфликт UAP с native
  backend только внутри одного HallJoy и никак не координируется с браузером;
- единого app-level pause/lease state и единственного владельца HallJoy в
  Windows-сессии сейчас нет;
- `UAP_DISABLE_HOTPLUG=1` делает восстановление старой UAP generation после
  внешнего reset/re-enumeration недостоверным.

WebHID сообщает странице лишь успех или неуспех `open()`. Windows не даёт уже
работающему HallJoy надёжного уведомления «страница собирается открыть этот
HID». Автоматизация по имени Chrome, URL или наличию окна web-драйвера была бы
ложной эвристикой.

## Обязательная семантика глобального рубильника

Рубильник означает временно **выключить весь движок HallJoy**, а не только
перестать читать текущую клавиатуру:

1. запретить новые provider generations и новые HID opens;
2. опубликовать нулевой keyboard/mouse/provider snapshot;
3. отправить нейтральный XUSB report на каждый виртуальный gamepad;
4. прекратить блокировку клавиатуры и мыши, снять глобальные hooks/capture;
5. остановить UAP child и все native providers с bounded containment;
6. отменить и корректно drain-ить pending HID I/O;
7. закрыть все vendor HID handles и межпроцессные protocol mutexes;
8. после подтверждённой нейтрализации удалить/отключить виртуальные targets,
   чтобы игра не считала HallJoy активным контроллером;
9. оставить UI, настройки и runtime-only состояние выключателя доступными;
10. не возобновлять работу автоматически и не бороться с web-драйвером за HID.

Состояние выключателя не сохраняется как постоянное пользовательское
отключение: после нового запуска HallJoy стартует штатно. Пока процесс жив,
решение Resume всегда остаётся явным.

## Машина состояний

```text
Active
  -> PauseRequested
  -> Neutralizing
  -> StoppingProviders
  -> ReleasingHid
  -> Paused
  -> ResumeRequested
  -> Enumerating
  -> ProvingCapabilities
  -> PublishingNeutralGeneration
  -> Active
```

Любой fault при Pause не разрешает состояние `Paused`, пока жив хотя бы один
provider/HID owner. Он переводит процесс в `PauseFaulted`, сохраняет resources
живой poisoned generation и включает существующий hard process containment.
Нельзя закрыть handle поверх незавершённого `OVERLAPPED`.

Resume никогда не переиспользует старый handle, layout proof или device ID.
После web-драйвера клавиатура могла сменить профиль, firmware, report sizes,
runtime PID или пройти через bootloader. Поэтому обязательны полное
re-enumeration, exact-interface routing и новый capability/layout proof.

## External-owner состояние

Ошибки `ERROR_SHARING_VIOLATION`, `ERROR_ACCESS_DENIED` и `ERROR_BUSY` должны
иметь отдельную классификацию `BusyByOtherApplication`, а не выглядеть как
«неподдерживаемая клавиатура» или бесконечный transport fault.

При такой ошибке provider обязан:

- немедленно нейтрализовать свою публикацию;
- не создавать reconnect/open storm и не засорять журнал одинаковыми строками;
- использовать bounded backoff;
- показать в UI, что устройство занято другой программой;
- не считать доступность доказанной до новой полной сессии.

Это улучшает восстановление, но не заменяет явный Pause: когда HallJoy открыл
HID первым, он не узнает о ещё не состоявшейся попытке браузера.

## Единственный HallJoy owner

Второй экземпляр HallJoy сейчас может создать ещё один набор UAP/native HID и
ViGEm sessions. `V14-24` должен ввести одного authoritative owner на Windows
logon session. Повторный обычный запуск активирует уже существующее окно, а не
создаёт конкурирующий engine. Diagnostic/simulator child roles разрешаются
только через их доказанный inherited-handle/capability contract и не получают
обычный application ownership.

Per-device pause можно добавить позднее, но он не заменяет глобальный
рубильник. Для безопасной работы с web-драйвером первая версия освобождает все
клавиатуры и исключает ошибку выбора составного HID.

## Архитектурное место

Реализация должна быть частью общего `AnalogProviderV2`/lifecycle owner:

- `ApplicationRunState` владеет одним immutable generation state;
- `DeviceLeaseCoordinator` запрещает opens вне `Active`/`Resuming`;
- каждый provider подтверждает `StoppedAndReleased(generation)`;
- output, hooks и providers участвуют в одной pause-транзакции;
- UI лишь запрашивает transition и отображает authoritative state;
- никакой backend-specific кнопки «web driver» не требуется.

## Обязательные отрицательные oracles и gates

- exclusive fake web-driver open не проходит в `Active` и проходит только после
  подтверждённого `Paused`;
- shared dual-writer сессия никогда не допускается для command protocols;
- удерживаемые клавиша, stick, trigger, gamepad button и mouse-block всегда
  нейтрализуются до первого закрытого HID handle;
- все UAP/native handles, child processes, readers, protocol mutexes и virtual
  targets отсутствуют в `Paused`;
- 1000 Pause/Resume циклов не оставляют handles, threads, children или targets;
- Pause во время pending read/write, proof, reconnect и provider fault имеет
  hard deadline и не разрушает живой `OVERLAPPED`;
- browser profile change, device reset и bootloader/runtime PID transition
  требуют нового proof и не публикуют старую раскладку;
- второй обычный HallJoy не создаёт engine и активирует первого owner;
- UAP и каждый native family проходят одинаковый lease contract;
- exact final EXE подтверждает neutral-before-release и no-survivor invariants;
- физические web-driver Pause/Resume проверки выполняются на доступных
  representative UAP и native клавиатурах; остальные честно `hardware pending`.

## Что не является исправлением

- только `FILE_SHARE_READ | FILE_SHARE_WRITE`;
- закрыть один известный HID, оставив другие providers активными;
- приостановить realtime, но оставить UAP child и handles;
- убить worker и закрыть незавершённый `OVERLAPPED`;
- определять web-драйвер по браузерному процессу или окну;
- бесконечно пробовать переоткрыть HID во время настройки/прошивки;
- автоматически Resume по первому появившемуся PID;
- сопоставлять аналог по более позднему бинарному нажатию;
- показывать UI-флажок до подтверждённого release всех owners.

## Решение по релизу

`HJ-V14-P1-033` является обязательным блокером следующей стабильной версии.
Он реализуется после отрицательных lifecycle-oracles `V14-22` и на общей
provider/lifecycle архитектуре `V14-21`/`V14-23`. Диагностические сборки могут
не иметь рубильника, но не могут называться стабильными для совместной работы с
web-драйверами.

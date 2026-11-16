# HallJoy — действующий контекст владельца

Актуализировано 2026-10-31 по запросу адаптировать рабочий контур для Codex.
Это короткий вход. Все прежние записи сохранены в
[полной истории решений](../archive/OWNER_CONTEXT_BEFORE_CODEX_2026-10-31.md).
История читается по модели/функции; поздние решения владельца важнее ранних аудитов.
Не требовать повторного подтверждения записанного решения без новых данных.
Не использовать краткое резюме для отмены не упомянутого в нём решения.

## Работа и безопасность файлов

- Рабочий корень — `HallJoy-main/`; читать оба AGENTS.md. В локальной папке нет
  `.git`; публикация идёт через отдельный очищенный клон. Не делать `git init`.
- Общение с владельцем на русском. Обычный UI, логи, сообщения, комментарии кода,
  пользовательский README и патчноуты на английском. Исторические scoped исключения
  диагностических заголовков не расширять на обычное приложение.
- Всегда вести документацию. Перед каждой записью перечитать байты/проверить хеш
  цели, не затирать чужие изменения. Перед широкими правками — проверенный бекап
  с путями, SHA-256 и способом отката. Не удалять рабочие сборки, уникальные captures
  и бекапы по возрасту. Соблюдать [структуру проекта](PROJECT_LAYOUT.md).
- Визуал оценивает владелец. Агент проверяет код и автотесты без визуальных прогонов.
  Программный PASS не является аппаратным тестом, измерением FPS или гарантией USB.
- Главное окно не управляется игровыми нажатиями, кроме назначенных биндов,
  захвата и ввода в открытые мышью поля. Редактор имеет отдельную политику:
  [MAIN_WINDOW_KEYBOARD_POLICY](MAIN_WINDOW_KEYBOARD_POLICY.md).

## Аналог, идентификация и раскладки

- Production не имитирует глубину цифровыми состояниями, длительностью удержания,
  цифровым отпусканием и fallback-кривыми. Синтетика — только в явно изолированных
  автотестах. Отсутствующий источник не заменять скрытой симуляцией.
- Для рассматриваемых Keychron HE согласован путь кастомной прошивки по исходникам
  Keychron и FAR AnalogSense, а не универсальный бинарный патчер. Аналог считается
  обеспеченным этим путём; добавление геометрии не блокировать повторным поиском
  протокола или железа каждой модели. Проверять модель, ANSI/ISO/JIS, USB/матрицу и
  геометрию. Стоковую прошивку и образ соседней модели не считать подходящими.
- Supported: встроенная раскладка → `supported_layouts.json` → проверка собранного
  каталога и свежей Sheet → статус. Аппаратный тест каждой модели не обязателен при
  установленном протоколе/реализации; конкретные технические ограничения сохранять.
  При выполнении критериев агент сам синхронизирует статус, доверяя названию модели
  от тестировщика, без лишнего повторного запроса подтверждения.
- Yellow — идентификация, независимый аналог всех клавиш, бинды и геймпад.
  C35B исключён (2026-11-07). Исследование или выключенный backend не являются yellow. Generic admission
  требует безопасной идентификации до команд, доказательства протокола, живого
  чтения и отсутствия межпротокольной коллизии. Bare vendor usage page недостаточен.
  Отказанную идентичность не опрашивать бесконечно:
  [правило](SPARKLINK_GENERIC_ADMISSION_2026-10-12.md).
- При наличии тестировщика первый тестовый EXE включает точный пресет его
  модели и региона и проверенный автовыбор, даже для Experimental.
- Automatic layout включён по умолчанию и учитывает live remaps. Неоднозначность
  возвращает ручной выбор. Manual использует factory assignments. Автовыбор
  применяется и при повторных подключениях.
- Новые раскладки начинать с `layout_pipeline.py summary <Brand>` и сохранённых
  источников. Точный импорт не подгонять под старую ручную картинку. Не объединять
  разные бренды; сохранять изменённые пользователем legacy-пресеты. Трудную геометрию
  явно откладывать; приближение требует обоснования, как согласованная
  [RT75 Pro](YUNZII_RT75PRO_2026-10-28.md).
- Для hot-swap сохранять согласованную 4 mm шкалу там, где действует это правило;
  короткий ход — настройка end deadzone. У K4 сохранять индивидуальную калибровку,
  не хардкодить механику/ход клавиш владельца.

## Поддержка, таблица и диагностика

- Изменение поддержки, ограничений или вывода тестировщика проходит в одной задаче
  [SUPPORT_STATUS_SYNC](../development/SUPPORT_STATUS_SYNC.md): runtime/notices,
  README, SUPPORTED_HARDWARE, текущий документ и живая Google Sheet с readback.
  Соответствующие правки статусов уже разрешены. Недоступность — явный pending sync.
- [Таблица](https://docs.google.com/spreadsheets/d/1ueQ4labXpuBOmGjCkUcJllNJ68Jzx4py2MuQm-p-R7c/edit):
  Main / sheetId 0; A бренд, B модель, C статус. Читать живые границы, находить строки
  по бренду + модели. Сохранять validation, соседей, base/effective цвета. После
  вставки — полный структурный аудит. Notes/comments запрещены; источники в docs.
  Каталог охватывает все аналоговые технологии, включая inductive, независимо от
  поддержки HallJoy. [Обязательные правила](../development/KEYBOARD_SHEET_RULES.md).
- Тестировщик получает один обычный HallJoy.exe, возвращает HallJoy.log / Open log.
  Без обязательных ZIP, скриптов, CLI-флагов и отдельных экспортов. Агенту разрешены
  местные исследовательские инструменты.
- Enable logging — необязательный непрерывный лог. Предусмотренные автоотчёты о
  crash/missing-source/инцидентах сохраняются и при OFF. Instability/limited-support
  сами по себе не разрешают forced logging. Логи должны содержать данные для поиска
  ошибки; raw/trace исключения scoped:
  [контракт](../development/SUPPORT_DIAGNOSTICS_CONTRACT.md).
- Не выдавать догадку о драйвере/кабеле за причину. Тишина change-driven потока и
  нулевые счётчики сами по себе не доказывают неисправность.
- Firmware execution PASS отделять от пригодности. Проверять typing, multikey,
  releases, диапазон, режимы и cleanup, раскрывать неизвестное. Сохранять лучший
  обоснованный limited path при отсутствии unrestricted:
  [обязательное ревью](../development/FIRMWARE_BEHAVIOR_REVIEW.md).

## Сборка, lifecycle и публикация

- HallJoy остаётся портативным: один EXE без установки. Установщик строго опционален;
  отдельная команда без NSIS в обычной сборке. [Правило](../development/WINDOWS_INSTALLER.md), [PR11](PR11_OPTIONAL_INSTALLER_2026-11-15.md).
- Одна обычная выдача: `build/bin/Release/x64/HallJoy.exe`, все одобренные функции,
  включая limited MAD68 V2 Dual с red warning без forced logging. Полный каталог
  проверяется независимо в самом кандидате, не ожиданием из того же manifest.
- Закрывать HallJoy для сборки/замены/диагностики/автотестов уже разрешено. Обычно
  кандидат собирается/проверяется при открытом старом приложении, затем штатный
  `build_release.ps1` закрывает точный EXE, атомарно заменяет и восстанавливает его,
  если он работал. Не создавать папки выдачи ради занятого EXE. При зависании
  допустимо завершить проверенный процесс и детей; elevated access denied —
  taskkill через UAC без /F, как в AGENTS.md. Не восстанавливать интерактивный UI Hidden.
- Обычное изменение: один native suite с кешем и одна release build. Не включать
  `HALLJOY_NO_TEST_CACHE=1` и не повторять PASS без относящегося изменения.
  `research_reference_checks.py --record` только при изменении pinned source.
  Не запускать беспричинные длительные soak; проверки короткие и по гипотезе.
- Pause освобождает клавиатуру/providers, без скрытого удержания аналога ради Resume.
  Согласованный K4 PARK — отдельная реализация по актуальному контракту.
- Публикация требует соответствующего запроса владельца. Английский патчноут в
  `docs/releases/RELEASE_NOTES_v<version>.md`, затем `tools/release.py ... --publish`.
  Очищенный main и publication Git index до push; не возвращать старые mirrors,
  теги и force-push. Сохранять исторические asset ID и download counters.
- Vendor firmware/configurator/art/manual/copied source без установленного права
  распространения остаются local. Public URL и root license HallJoy не дают таких
  прав. Сохранять attribution и scoped dual-license; сканер не даёт полного legal
  clearance. Переписка Pwnage local-only.
- README краткий: **Brand:** модели, алфавит брендов, Supported / Experimental,
  один автор (singular). Без HTML-таблиц, длинного support checklist, приписки текущей
  версии сверху и несогласованных обещаний. Discord — простой вход. Заголовок релиза
  `v<version>: ...`. Для запуска достаточно EXE; LICENSE/THIRD_PARTY_NOTICES разрешены
  отдельными assets, не обязательны рядом с EXE.

## Текущий baseline и отложенные направления

Публикация по локальной записи: [1.6.15](RELEASE_1.6.15_PUBLICATION_2026-11-07.md); latest проверять live. EXE/хеш: `project_context.py --hash-exe`; версия: `version.h`.
[MIDI M2](MIDI_M2_TOUCH_2026-11-15.md), [настройки](MIDI_SETTINGS_UI_2026-11-15.md): отдельная вкладка с живым графиком при MIDI on. Default: 1 pad, MIDI off. Musical gate open.
2026-11-15: владелец запросил обычный тестовый EXE и следующий релиз без MIDI.
Код MIDI доступен для разработки с /p:HallJoyMidi=true. Обычная сборка оставляет
данные прежних MIDI-профилей для совместимости. GitHub-релиз после ручной проверки
владельца. [Состояние MIDI](../development/MIDI_ROADMAP.md).

K4 с HJO использует onboard; pad исчезает вне HallJoy, permanent pad не согласован.
[Радиопроба](K4_WIRELESS_GAMEPAD_2026-10-24.md) откатана к проводной рабочей прошивке;
wireless Route A ещё не реализован. Аппаратные действия связывать с конкретной
задачей и имеющимся разрешением. [Profiles v2](GAME_PROFILES_V2_2026-10-03.md)
реализованы; старое «скрыть Profiles» не актуально. Backend limits дают advisory
после активации, не запрещают профиль. [Единый UI](../development/CUSTOM_UI_ARCHITECTURE.md),
[переходы](TAB_TRANSITIONS_2026-10-03.md): 0.2 s, quintic, motion blur on;
не возвращать снятые polling/UI controls и раздельные scrolling/timer реализации.

Не возобновлять отложенные работы по старому handoff автоматически. MG75 V2 ждёт
тестировщика; Alumix 68 выключен из-за коллизии MINI60; AK820 Sonix 80B1 / AK680 V2
не поддерживаются. RT75/RT80 и Apex Pro TKL 2023 отличать от соседних ревизий.
[Apex TKL Gen3](STEELSERIES_APEX_PRO_TKL_GEN3_REVERSE_2026-11-06.md): Experimental.
Wired ANSI 1.19.7. Обратный отклик после кривой; Roblox ждёт A/B-теста.
[HZ-94](KEYBOARD_FEEDBACK_LOG39_41_2026-11-08.md): USB ANSI подтверждён в игре; Supported sync.
[K70 PRO TKL](CORSAIR_K70_PRO_TKL_2026-11-07.md): Supported USB 1.12.54; ISO log34 confirmed.

Рабочий порядок: [AGENT_WORKFLOW](../development/AGENT_WORKFLOW.md).

[R82 HE](ATTACK_SHARK_R82HE_LOG27_2026-10-31.md): Experimental, discovery rev2, ретест pending. [Redragon](REDRAGON_TESTER_FEEDBACK_2026-10-31.md): K552-M ISO/K686BG-RGB Supported, Sheet/build PASS.

2026-11-02 [CORSAIR VANGUARD PRO 96](CORSAIR_VANGUARD_PRO96_2026-11-02.md):
limited stock2.8.171 C6-calibration/C7-snapshot, exact1B1C:2B0E, protocol33.
ANSI/ISO/JIS; BR excluded(sensor0 collision). Close SESSION after each read to
avoid settings reload; no004B open, calibration-start or configuration writes.
Log23 proves ISO/saved calibration/97 keys/5026 snapshots/0 failures;
hold/range/gamepad/typing still need functional testing. Sheet C183 yellow.
Detailed revision2 admission, lifecycle and exact-image checks in linked report.

2026-11-05 [ZENITH PRO](RAMPAGE_ZENITH_PRO_2026-11-05.md): US driver board2310; test pending.

2026-11-05 [AK680 MAX HE](AJAZZ_AK680MAX_SUPPORTED_2026-11-05.md): Supported per tester/owner; Sheet/build PASS.

[F68](FREEWOLF_F68_2026-11-07.md): Supported USB ANSI (2026-11-07).

[SU75 revision identities](EVERGLIDE_SU75_REVISIONS_2026-11-14.md): XS117/V1=1CA6:3002; seller V2/Ultra XS125=1CA6:3007.
2026-11-15 [SU75 implementation](EVERGLIDE_SU75_IMPLEMENTATION_2026-11-15.md): V1/V2 automatic ANSI presets after board+live proof; physical channels retained through unbound/remapped metadata; cached ordinary-log identity/release aggregates. Existing admission/statuses retained; ordinary EXE6a12acdd installed. Release/image/layout/profile/recovery gates PASS. One shared menu entry; manual legacy names preserved. No GitHub publication or physical V2 result.

2026-11-16 [NJ98 RF rev8](KEYDOUS_NJ98_V4S_COMMAND_HANDOFF_2026-11-16.md): late pages applied; vendor10ms command handoff restored. RF retest pending; USB confirmed.

2026-11-03 [IPI Flash 75 PRO / Gemini Seven-Star](IPI_FLASH75PRO_TESTER_FEEDBACK_2026-11-03.md):
owner confirms it works; log28 wired1CA6:1A10 SparkLink,353121 updates/0 failures.
Exact preset missing: no Supported until geometry/catalog/Sheet gates.
Main!A386:C386 IPI/QBZ alias, yellow Implemented; layout pending. Drive/Sheets
read/write and structure verified; details in SHEETS_RECONNECT_2026-11-03.md.

2026-11-13 [ARDOR Viper](ARDOR_VIPER_2026-11-13.md): raw82-channel0x23
trial replaces lossy events. Tester confirmed operation 11-15. ANSI auto-layout
and no manual calibration. Depth scale remains provisional; Main!C61 yellow.

2026-11-13 [Akko 5087 V3 HE](AKKO_5087_V3_HE_2026-11-13.md): точный3131 /3151:5029,
read-only E5/FE full matrix87 + встроенная ANSI/автовыбор; калькулятор display-only.
Официальный драйвер называет3131 V3 HE; старое имя community не основание отказа.
Без ручной калибровки/событий/записи настроек. Соседние прошивки replay PASS;
точный3131 бинарник не получен, физические range/releases pending. Experimental.
EXE9d55f0fd,25 image-сценариев и полные build/profile/recovery gates PASS.
Sheet C35 yellow;284 notices/91 Supported-layouts live PASS.
2026-11-16 [F75 HE](AULA_F75_HE_2026-11-13.md): red; stock trials off, research retained; Sheet C134 verified.
[Akko MOD007 V5 HE](AKKO_MOD007_V5_HE_2026-11-16.md): log58=3113/5029; read-only matrix +80-key ANSI; physical test pending.
2026-11-14 [BOOM68-USB](BOOM68_USB_2026-11-14.md): Supported USB stock1.10 ANSI.
Tester confirmed. Log shows67 channels,9 exercised/9 released,3 simultaneous.
Sheet Main!C482 green. Trial notice retained for the alternate status getter.
2026-11-14 [MCHOSE GT75](MCHOSE_GT75_2026-11-14.md): exact3837:302D USB,
V108 firmware acquired/replayed; E0 absent, separate04/05/06 protocol36.
81 magnetic A0 channels includingFn +83-control ANSI auto-layout. Guarded debug
bit persists in flash; restore owned bit on exit, preserve pre-enabled. Source
cutoff<=9/bottomsnap5, physical typing/loss/range pending; LIMITED_ONLY review.
EXE605d4556:27 GT75 session cases +3 isolated Win32 I/O cases;8-family/full
catalog/profile/recovery PASS. Main!C548 yellow. Tester confirmed operation 11-14,
log pending.
2026-11-16 [1.6.16 prep](RELEASE_1.6.16_PREPARATION_2026-11-16.md): 3 support updates, CPU90%; no publish.

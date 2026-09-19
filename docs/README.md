# HallJoy — навигация по документации

- [HallJoy 1.5.3 candidate: owner EXE test before publication](current/RELEASE_1.5.3_CANDIDATE_2026-09-19.md).

- [ATTACK SHARK Pro: integrated X65 Pro tester build, continuous capture and validation](current/ATTACK_SHARK_PRO_DIAGNOSTIC_2026-09-19.md).

- [ATTACK SHARK Pro: exact X82 v503 firmware, independent page reads and six revision maps](research/ATTACK_SHARK_PRO_REVIEW_2026-09-19.md).

- [NA87 and AULA MINI60 HE Pro: ordinary HallJoy support](current/NA87_MINI60_STANDARD_SUPPORT_2026-09-19.md).

- [AULA and IROK tester logs 15/16: native success evidence and unresolved held-state interval](current/TESTER_LOGS15_16_2026-09-19.md).

- [AULA MINI60 HE Pro: playable native analog build, mapping, release policy and verification](current/AULA_MINI60_NATIVE_SUPPORT_2026-09-17.md).

- [AULA MINI60 HE Pro: integrated diagnostic build and evidence-based completion](current/AULA_MINI60_DIAGNOSTIC_2026-09-17.md).

- [AULA MINI60 HE Pro: tester log, official V1.55 and multi-key analog candidate](research/AULA_MINI60_HE_PRO_2026-09-17.md).


- [IO Type 84 Magnetic: reviewed firmware; selected-key telemetry, no complete multi-key support](firmware/io-type84-magnetic/DEEP_REVERSE.md).

- [Pwnage Zenblade 65 V2: recovered August 20 protocol research; no analog backend](v1.4/PWNAGE_ZENBLADE_65_V2_PROTOCOL_RECON.md).

- [Analog normalization: observed bounds versus device data](current/ANALOG_RANGE_AUDIT_2026-09-15.md).

- [HERO84 HE: enabled analog with unverified-support notice](current/HERO84_ENABLED_UNVERIFIED_2026-09-15.md).


- [Frozen keyboard support and amber testing notices](current/FROZEN_SUPPORT_NOTICES_2026-09-15.md).


- [Automatic layout: telemetry coherence and preview stability](current/AUTOMATIC_LAYOUT_COHERENCE_2026-09-14.md).

- [UAP automatic-layout identification fix](current/AUTOMATIC_LAYOUT_UAP_FIX_2026-09-14.md).

- [Automatic layout and session remapping](current/AUTOMATIC_LAYOUT_2026-09-14.md).

> 2026-09-14: [ATK Hex80 native fixes](current/ATK_HEX80_NATIVE_FIXES_2026-09-14.md).
> Corrected19 matrix slots;87 factory keys including Fn now publish analog.
> Supports32/128-byte payloads, per-key freshness and correlated chunk replies.
> Historical82-key table/report-size assumptions below are superseded.

> 2026-09-14 implementation update: [IPI native support](current/IPI_NATIVE_SUPPORT_2026-09-14.md).
> Exact UUID profiles, complete live maps, device calibration, Fn and alias
> publication now replace the historical IPI fallback. Eight models/four layouts.
> Earlier layout-only/no-EXE statements below describe the preceding step.
> Physical USB tests and AURORA65W receiver forwarding remain unverified.

> 2026-09-14: [IPI firmware reverse](current/IPI_FIRMWARE_REVERSE_2026-09-14.md) now covers18 images/eight UUIDs.
> Addressed analog handlers and exact physical-ID sets confirmed in firmware;
> 216 synthetic handler/helper cases PASS. Empty-map request and calibration
> policy gaps remain in HallJoy. No hardware test or EXE change in this step.

> 2026-09-14 follow-up: [IPI layouts](current/IPI_LAYOUTS_2026-09-14.md): four merged ANSI presets for
> eight models. Official demo defaults resolve the previously missing labels;
> the separate Addressed fallback discrepancy remains documented at its table.
> O3C and Plus revisions remain excluded. No new analog routes enabled.

> 2026-09-14: [добавлена ATK Hex80 ANSI](current/REMAINING_LAYOUTS_2026-09-14.md), 87 отображаемых клавиш.
> O3C исключён владельцем; другие кандидаты требуют отдельного разбора и отложены.
> Только раскладка: текущая Hex80-таблица аналоговых слотов отличается от официального
> профиля, протокол не изменён. Проверки/сборка PASS; основной HallJoy.exe обновлён.

> 2026-09-14: [устранена дорогая запись настроек при смене раскладки](current/SETTINGS_SAVE_LATENCY_2026-09-14.md).
> Полное сохранение настроек/привязок пакетное: 1117 мс → 17–27 мс в изолированном тесте.
> Полный набор тестов профилей и событий редактора PASS. По прямому запросу владельца
> работающий HallJoy закрыт, основной EXE заменён; папка Optimized удалена.
> Для обновлений использовать прежний путь EXE; владелец разрешил закрывать HallJoy
> для замены сборки, не создавать дополнительные папки выдачи.
> Отдельные K2/K3 — JIS; старый Q1 ANSI сохранён как override с UniformGap=6.

> 2026-09-14: [переработано хранение раскладок](current/LAYOUT_STORAGE_OPTIMIZATION_2026-09-14.md).
> Заводские раскладки остаются в памяти; при запуске INI не создаются и не переписываются.
> Пакетное чтение/запись, атомарное сохранение и старые пользовательские правки сохранены.
> Изолированный замер каталога: чистый запуск 67 с → 8 мс; 63 старых файла 1,31 с → 34 мс.
> Это время каталога, не всего приложения. Проверки и новая сборка EXE PASS.

> 2026-09-14: добавлены [четыре раскладки MADLIONS](current/MADLIONS_LAYOUTS.md) по запросу владельца:
> MAD60HE, MAD68HE, MAD68R (поддерживаемая ревизия10A7) и MAD 68 Pro R, ANSI.
> Проверки и сборка PASS; выбор вручную в MADLIONS. Визуал оценивает владелец.

> 2026-09-13: по новому запросу владельца добавлены [IROK MG75 Max / Pro ANSI](current/IROK_MG75_LAYOUTS.md),
> по81 клавише, выбор в каталоге IROK. Проверки/сборка пройдены; визуал оценивает владелец.

> Актуально: [первая сборка с реальным аналоговым вводом NA87](current/IROK_NA87_NATIVE_SUPPORT_2026-09-13.md).
> Поток глубин подтверждён аппаратным логом; новый backend публикует его в HallJoy.
> ANSI подтверждена владельцем. Прежний сборщик заменён непрерывным вводом,
> логирование сокращено до агрегатов. Автотесты пройдены; нужен аппаратный прогон
> нового EXE для проверки сочетаний и отпусканий.

> После локального лога: [исправления runtime](current/IROK_NA87_RUNTIME_FIXES_2026-09-13.md).
> Устранён дефект владения Windows debug handles, ограничены повторные логи,
> остановлен цикл перезапусков при невалидном command event. Автотесты пройдены;
> Повторный запуск16:08 завершился штатно: прежние сбои не повторились.
> Текущий EXE можно передавать NA87-тестировщику; аналог NA87 ещё не подтверждён.

> Актуально: [обычный HallJoy.exe с автоматической диагностикой NA87](current/IROK_NA87_HALLJOY_DIAGNOSTIC_2026-09-13.md).
> Прежний ZIP отклонён. Передавать только EXE: обычный геймпад сохранён,
> пользователь нажимает клавиши30–60 секунд, закрывает окно и присылает HallJoy.log.
> На локальном ПК Keychron; NA87 здесь нет. Работающий HallJoy владельца не закрывать.
> Полный тест instance guard/профилей здесь блокируется уже запущенным HallJoy
> (Windows error5); остальные доступные проверки и самотесты диагностики пройдены.

> Продолжение после ревью: [исправления ND75](research/IROK_ND75_IMPLEMENTATION_FIXES_2026-09-13.md).
> Wire format и отбрасывание некорректной глубины исправлены; полнота состояния
> при потере событий остаётся нерешённой. Итоги проверок — в новом документе.


> Актуальное уточнение 2026-09-13: [ревью ND75](research/IROK_ND75_REVIEW_2026-09-13.md).
> Найдены ошибки wire opcode и публикации состояния в experimental backend;
> исправлена одна serializer fixture. Прежний вывод об отсутствии любых
> альтернативных чтений слишком широк: сохранён частичный результат52/81.
> Pro заморожен; эксперименты вне штатной таблицы настроек приостановлены
> по последнему согласованному направлению, продолжается обычное ревью.


Начать здесь. Историческая дата или слово current в имени файла не гарантируют,
что все старые утверждения внутри остаются актуальными: читать верхние уточнения.

## Текущая работа и правила

- [Восстановление профиля при запуске 1.5.1](current/STARTUP_PROFILE_RECOVERY_2026-09-11.md).

- [Подготовка релиза 1.5.0](current/RELEASE_PREPARATION_2026-09-10.md).
- [Объединение одинаковых раскладок](current/LAYOUT_DUPLICATION_REVIEW.md).
- [Шаблон обращения в Discord](SUPPORT_REPORT.md).

- [Конвейер раскладок: начинать добавление бренда здесь](current/LAYOUT_PIPELINE.md).
- [Завершающий пакет Razer/NuPhy/Wooting — охват и ограничения](current/FINAL_LAYOUT_BATCH.md).
- [Aula: подготовленные раскладки и оставшиеся проверки](current/AULA_LAYOUT_PIPELINE.md).
- [Redragon K673: ANSI/ISO и отдельное расхождение BR](current/REDRAGON_LAYOUTS.md).

- [DrunkDeer — семь раскладок, распознавание и модельные карты UAP](current/DRUNKDEER_LAYOUTS.md).

- [Lemokey P1 HE ANSI/ISO — отдельный этап раскладок](current/LEMOKEY_LAYOUTS.md).

- [Клавиатурный ввод в основном окне и исправление составного Enter](current/MAIN_WINDOW_KEYBOARD_POLICY.md).

- **[Постоянный контекст владельца — читать в новой сессии](current/OWNER_CONTEXT.md)**,
  включая подтверждение о кастомных прошивках Keychron HE под UAP.
- [Keychron HE: каталог моделей и вариантов раскладок](current/KEYCHRON_HE_LAYOUT_CATALOG.md).
- [36 импортированных раскладок Keychron и составные клавиши](current/KEYCHRON_COMPOUND_LAYOUTS.md).
- [Выбор раскладки: блок Layout с Brand и Model](current/LAYOUT_BRAND_MODEL.md).
- [Автовыбор точной раскладки при первом запуске](current/FIRST_RUN_LAYOUT.md).
- [Импорт раскладок из веб-драйвера: K4 HE и Q1 HE](current/LAYOUT_IMPORT.md).
- [Discord в Global settings](current/DISCORD_SETTINGS_2026-09-09.md).
- [Remap: анимация-подсказка для пустого профиля](current/REMAP_HINT_2026-09-09.md).
- [Положение окна и несколько мониторов](current/WINDOW_PLACEMENT_2026-09-09.md).
- [Редактирование порта и HEX в Input Overlay](current/OVERLAY_TEXT_EDIT_2026-09-09.md).
- [Редактор раскладок и отдельный выбор в Input Overlay](current/LAYOUT_EDITOR_OVERLAY_2026-09-09.md).
- [Новый рабочий холст и точные координаты раскладок](current/LAYOUT_WORKSPACE_2026-09-09.md).
- [Attack Shark X68 HE: прошивка и аналоговый протокол](research/ATTACK_SHARK_X68_HE_RECON_2026-09-09.md).
- [Attack Shark X68 Pro / X82 Pro: официальный драйвер, ревизии и возможность поддержки](research/ATTACK_SHARK_PRO_RECON_2026-09-12.md).
- [Block Bound Keys: исключения и горячая клавиша](current/BLOCK_BOUND_KEYS_CONTROLS_2026-09-09.md).
- **[Актуальная структура и сборка](current/PROJECT_LAYOUT.md)** — единственный
  пакет build/release, каталоги компиляции, бекапов, таблица переносов.
- [Результат перестройки](validation/STRUCTURE_MIGRATION_2026-09-06.md).

- [Главный Roadmap](v1.4/FULL_AUDIT_EXECUTION_ROADMAP_2026-09-06.md): карточки аудита,
  продуктовые требования, Sayo auto-learning, вопросы владельцу, Discord RM-37.
- [Текущий handoff](v1.4/CURRENT_HANDOFF_2026-08-20.md): последний контекст и artifact.
- [Метод работы](v1.4/ENGINEERING_WORKING_METHOD.md): evidence, backups, проверки.
- [Решения](v1.4/DECISIONS.md), [риски](v1.4/RISK_REGISTER.md),
  [validation](v1.4/VALIDATION_MATRIX.md), [история работы](v1.4/WORKLOG.md).
- [Обзор структуры файлов](PROJECT_STRUCTURE_REVIEW_2026-09-06.md):
  исходное обследование и FS-00..05; текущий результат — по ссылке выше.
- [Архитектурный обзор](v1.4/ARCHITECTURE_REVIEW_2026-09-06.md):
  читать с уточнениями владельца; гипотезы не являются приказом переписать.

## Документы по архитектуре и устройствам

- [SayoDevice O3C: исходный замысел и протокол](protocols/SAYO_DEVICE_NOTES.md).
  Описывает автоматическое сопоставление пользовательских букв и три depth channels;
  это не «случайно появившаяся» архитектура. Capture paths в notes — placeholders.
- [Архитектура custom UI](development/CUSTOM_UI_ARCHITECTURE.md).
- [Input overlay](development/INPUT_OVERLAY_NOTES.md).
- [Цели производительности и стабильности](development/PERFORMANCE_AND_STABILITY_GOALS.md).
- [Тестирование](development/TESTING.md).
- [Вложенный README](../src/HallJoyProject/README.md): отличается от корневого,
  перед объединением сверить содержание.
- [Старый SOURCE_AUDIT_V6](archive/source-v6/SOURCE_AUDIT_V6.md): история,
  не автоматическое описание нынешней версии.

## Протоколы, исследования и выпуск

- [Поддерживаемое оборудование](../SUPPORTED_HARDWARE.md).
- [Release blockers](v1.4/CORRECTNESS_RELEASE_BLOCKERS.md):
  учитывать верхнее уточнение разрешённого Sayo auto-learning.
- [Release-readiness](v1.4/RELEASE_READINESS_AUDIT_2026-09-05.md):
  hashes квалифицированных прошлых сборок не переносятся на новый EXE.
- [Native audit](v1.4/NATIVE_ALL_KEYBOARDS_AUDIT.md) и
  [UAP audit](v1.4/UAP_ALL_KEYBOARDS_AUDIT.md): сверять findings с текущим кодом.
- Каталоги protocols/, research/, firmware/, stability/ содержат материалы
  разных поколений; не удалять как cache и не принимать research за hardware PASS.

- [IROK NA87 Mag: поиск прошивки и разбор драйвера](research/IROK_NA87_RECON_2026-09-13.md).
- [IROK/IYX: подробный handoff реверса и дальнейший план](current/IROK_REVERSE_HANDOFF_2026-09-13.md).

Переносы не отменяют продуктовые решения владельца.

- [IROK ND75: проверка альтернативных чтений и уточнение Witmod SDK](research/IROK_ND75_READ_PATHS_2026-09-13.md).

- [Witmod: SDK wire opcode, полный scanner ND75, serializer coverage](research/IROK_WITMOD_OFFLINE_CLOSURE_2026-09-13.md)

- [IROK: результаты перед аппаратной проверкой и реальные блокеры](research/IROK_PRE_HARDWARE_STATUS_2026-09-13.md)

- [IROK Pro: замороженный прогресс; активная работа ND75/Witmod](current/IROK_PRO_FROZEN_2026-09-13.md).

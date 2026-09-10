# HallJoy — навигация по документации

Начать здесь. Историческая дата или слово current в имени файла не гарантируют,
что все старые утверждения внутри остаются актуальными: читать верхние уточнения.

## Текущая работа и правила

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

Переносы не отменяют продуктовые решения владельца.

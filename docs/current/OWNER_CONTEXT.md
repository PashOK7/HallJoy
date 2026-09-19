# Постоянный контекст владельца

> 2026-09-15: [HERO84 enabled with testing notice](HERO84_ENABLED_UNVERIFIED_2026-09-15.md). Owner explicitly enabled its retained analog backend in ordinary HallJoy, like NA87. Earlier HERO84 disabled-build requirements are superseded; other frozen protocols stay disabled.


> 2026-09-15: [Frozen support notices](FROZEN_SUPPORT_NOTICES_2026-09-15.md): NA87 remains enabled but frozen pending tester logs; amber warning. Other frozen protocols stay disabled by owner decision.


> 2026-09-14: [preview stability fix](AUTOMATIC_LAYOUT_COHERENCE_2026-09-14.md):
> failed seqlock reads are unavailable evidence, not device removal. Coherent
> metadata publication and source-count checks prevent false layout switching.
> Status-only changes no longer rebuild keyboard controls or cancel dragging.

> 2026-09-14: [UAP automatic-layout fix](AUTOMATIC_LAYOUT_UAP_FIX_2026-09-14.md):
> DuplicateSafeId denotes a collision-resistant device ID, not multiple devices.
> Removing its incorrect rejection fixes a general UAP selection defect.
> UAP matrix telemetry now uses the shared reviewed Keychron identity table.

> 2026-09-14: [Automatic layout](AUTOMATIC_LAYOUT_2026-09-14.md): enabled by default;
> exact match locks layout choice, ambiguity/failure restores manual choice.
> NA87/IPI session remaps apply only in successful automatic mode. Manual mode
> uses factory assignments. This supersedes the old first-run-only policy.

> 2026-09-14: [ATK Hex80 native fixes](ATK_HEX80_NATIVE_FIXES_2026-09-14.md).
> Corrected19 matrix slots;87 factory keys including Fn now publish analog.
> Supports32/128-byte payloads, per-key freshness and correlated chunk replies.
> Historical82-key table/report-size assumptions below are superseded.

> 2026-09-14 owner clarification after IPI delivery: lack of local hardware is
> not a blocker for continuing support/layout work. Complete source/firmware
> analysis and software checks, record evidence limits, and proceed. Users will
> report physical-device issues; do not repeatedly stop for hardware confirmation.

> 2026-09-14 implementation update: [IPI native support](IPI_NATIVE_SUPPORT_2026-09-14.md).
> Exact UUID profiles, complete live maps, device calibration, Fn and alias
> publication now replace the historical IPI fallback. Eight models/four layouts.
> Earlier layout-only/no-EXE statements below describe the preceding step.
> Physical USB tests and AURORA65W receiver forwarding remain unverified.

> 2026-09-14: [IPI firmware reverse](IPI_FIRMWARE_REVERSE_2026-09-14.md) now covers18 images/eight UUIDs.
> Addressed analog handlers and exact physical-ID sets confirmed in firmware;
> 216 synthetic handler/helper cases PASS. Empty-map request and calibration
> policy gaps remain in HallJoy. No hardware test or EXE change in this step.

> 2026-09-14 follow-up: [IPI layouts](IPI_LAYOUTS_2026-09-14.md): four merged ANSI presets for
> eight models. Official demo defaults resolve the previously missing labels;
> the separate Addressed fallback discrepancy remains documented at its table.
> O3C and Plus revisions remain excluded. No new analog routes enabled.

> 2026-09-14: [добавлена ATK Hex80 ANSI](REMAINING_LAYOUTS_2026-09-14.md), 87 отображаемых клавиш.
> O3C исключён владельцем; другие кандидаты требуют отдельного разбора и отложены.
> Только раскладка: текущая Hex80-таблица аналоговых слотов отличается от официального
> профиля, протокол не изменён. Проверки/сборка PASS; основной HallJoy.exe обновлён.

> 2026-09-14: [устранена дорогая запись настроек при смене раскладки](SETTINGS_SAVE_LATENCY_2026-09-14.md).
> Полное сохранение настроек/привязок пакетное: 1117 мс → 17–27 мс в изолированном тесте.
> Полный набор тестов профилей и событий редактора PASS. По прямому запросу владельца
> работающий HallJoy закрыт, основной EXE заменён; папка Optimized удалена.
> Для обновлений использовать прежний путь EXE; владелец разрешил закрывать HallJoy
> для замены сборки, не создавать дополнительные папки выдачи.
> Отдельные K2/K3 — JIS; старый Q1 ANSI сохранён как override с UniformGap=6.

> 2026-09-14: [переработано хранение раскладок](LAYOUT_STORAGE_OPTIMIZATION_2026-09-14.md).
> Заводские раскладки остаются в памяти; при запуске INI не создаются и не переписываются.
> Пакетное чтение/запись, атомарное сохранение и старые пользовательские правки сохранены.
> Изолированный замер каталога: чистый запуск 67 с → 8 мс; 63 старых файла 1,31 с → 34 мс.
> Это время каталога, не всего приложения. Проверки и новая сборка EXE PASS.

> 2026-09-14: добавлены [четыре раскладки MADLIONS](MADLIONS_LAYOUTS.md) по запросу владельца:
> MAD60HE, MAD68HE, MAD68R (поддерживаемая ревизия10A7) и MAD 68 Pro R, ANSI.
> Проверки и сборка PASS; выбор вручную в MADLIONS. Визуал оценивает владелец.

> 2026-09-13: по новому запросу владельца добавлены [IROK MG75 Max / Pro ANSI](IROK_MG75_LAYOUTS.md),
> по81 клавише, выбор в каталоге IROK. Проверки/сборка пройдены; визуал оценивает владелец.

> Актуально: [первая сборка с реальным аналоговым вводом NA87](IROK_NA87_NATIVE_SUPPORT_2026-09-13.md).
> Поток глубин подтверждён аппаратным логом; новый backend публикует его в HallJoy.
> ANSI подтверждена владельцем. Прежний сборщик заменён непрерывным вводом,
> логирование сокращено до агрегатов. Автотесты пройдены; нужен аппаратный прогон
> нового EXE для проверки сочетаний и отпусканий.

> После локального лога: [исправления runtime](IROK_NA87_RUNTIME_FIXES_2026-09-13.md).
> Устранён дефект владения Windows debug handles, ограничены повторные логи,
> остановлен цикл перезапусков при невалидном command event. Автотесты пройдены;
> Повторный запуск16:08 завершился штатно: прежние сбои не повторились.
> Текущий EXE можно передавать NA87-тестировщику; аналог NA87 ещё не подтверждён.

> Правило языка владельца: интерфейс HallJoy, все сообщения диагностического EXE,
> логи и комментарии в коде — только на английском. Общение с владельцем — на русском.
> Язык общения не переносить в приложение.

> Актуально: [обычный HallJoy.exe с автоматической диагностикой NA87](IROK_NA87_HALLJOY_DIAGNOSTIC_2026-09-13.md).
> Прежний ZIP отклонён. Передавать только EXE: обычный геймпад сохранён,
> пользователь нажимает клавиши30–60 секунд, закрывает окно и присылает HallJoy.log.
> На локальном ПК Keychron; NA87 здесь нет. Работающий HallJoy владельца не закрывать.
> Полный тест instance guard/профилей здесь блокируется уже запущенным HallJoy
> (Windows error5); остальные доступные проверки и самотесты диагностики пройдены.

> Продолжение после ревью: [исправления ND75](../research/IROK_ND75_IMPLEMENTATION_FIXES_2026-09-13.md).
> Wire format и отбрасывание некорректной глубины исправлены; полнота состояния
> при потере событий остаётся нерешённой. Итоги проверок — в новом документе.


> Актуальное уточнение 2026-09-13: [ревью ND75](../research/IROK_ND75_REVIEW_2026-09-13.md).
> Найдены ошибки wire opcode и публикации состояния в experimental backend;
> исправлена одна serializer fixture. Прежний вывод об отсутствии любых
> альтернативных чтений слишком широк: сохранён частичный результат52/81.
> Pro заморожен; эксперименты вне штатной таблицы настроек приостановлены
> по последнему согласованному направлению, продолжается обычное ревью.


Этот документ читается в начале новой сессии через корневой AGENTS.md.
Здесь фиксируются уточнения владельца, которые нельзя каждый раз заново
превращать в вопросы или блокеры. Новые решения добавлять с датой и областью
применения; при реальном противоречии сначала изучить данные, затем объяснить его.

## 2026-09-09 — Keychron HE: кастомные прошивки под UAP

Владелец подтвердил: используются кастомные прошивки. Уточнение 2026-09-12:
это не готовый инструмент-патчер. Keychron публикует исходники, а AnalogSense
показывает изменения full analog report, которые переносятся в исходники
нужной модели с последующей сборкой. Для обсуждаемых Keychron HE поддержка
аналога считается обеспеченной этим путём.

Контекст обсуждения: расширение каталога раскладок K4 HE, Q1 HE, Q3 HE, Q5 HE,
K2 HE и соответствующих вариантов ANSI/ISO/JIS. При добавлении их раскладок
не ставить повторное исследование аналогового протокола или наличие всех этих
физических клавиатур условием работы. Не предлагать снова проверять, «есть ли
вообще аналог», ссылаясь только на ограничения заводской прошивки.

Что действительно требуется проверять:

- соответствие модели и варианта ANSI/ISO/JIS;
- правильные VID/PID, интерфейс и матрицу для идентификации;
- соответствие координат, размеров и назначений клавиш;
- запись в каталоге автоподбора без неоднозначного выбора устройства.

Границы факта: это подтверждённая владельцем исходная информация, а не новый
аппаратный тест агента. Она не означает, что все стоковые прошивки дают хороший
аналог, что все будущие модели автоматически внесены в код, или что можно
прошивать устройство образом другой модели. Источник: https://analogsense.org/firmware/.
Проверенный коммит AnalogSense/qmk_firmware:
e21916b695ec3b31fbbbb325e32849be5cdb9e46 (keychron-far, Add full analog report).
Изменены analog_matrix.c (команда полного отчёта 0x31 и маркер 0x45) и
keychron_raw_hid.c (суффикс +far). Это изменения исходников, не бинарный патчер.
Не блокировать добавление раскладок повторным исследованием этого протокола.

## Уже принятые решения по раскладкам

- Импортированные координаты могут быть точнее ручных. Старая K4 — ориентир,
  не эталон для подгонки.
- Новая импортированная K4 одобрена владельцем. Старый пресет K4 снят с каталога;
  старые сохранённые ссылки перенаправляются на новую раскладку.
- Автовыбор только при первом запуске после завершения поиска, при одном
  подходящем устройстве и точном соответствии. Ручной/сохранённый выбор сохранять.
- Подробности: `LAYOUT_IMPORT.md` и `FIRST_RUN_LAYOUT.md` в этой папке.

## 2026-09-09 — Основное окно не управляется клавиатурой

Игровые нажатия не должны открывать списки, нажимать кнопки, переключать вкладки,
двигать ползунки или запускать встроенные сочетания команд. Разрешены только
явно назначенные пользователем бинды (например Block Bound Keys), режим их
захвата и ввод текста/чисел в поля, которые пользователь открыл мышью.
Последнее отдельно подтверждено владельцем. Самостоятельное окно редактора
раскладок не входит в эту задачу: его точное клавиатурное редактирование сохраняется.
Подробности: `MAIN_WINDOW_KEYBOARD_POLICY.md`.

## 2026-09-09 — Охват раскладок остальных брендов

Владелец явно подтвердил: добавляем раскладки для поддерживаемых устройств,
включая модели встроенного UAP, а не только физически проверенные командой.
Это не разрешение добавлять все модели бренда по сходству названия или VID.
Экспериментальные и замороженные ROG Azoth 96 HE, AULA HERO84 HE, IROK ND75,
Attack Shark X68 HE в этот этап не входят. Не переспрашивать, включать ли UAP.

Добавлять строго по одному бренду за этап, затем останавливаться на проверку
владельцем. Не выкатывать несколько новых брендов одним пакетом. Следующий
после Keychron этап: Lemokey P1 HE ANSI/ISO, явно перечисленные во встроенном UAP.

Lemokey ANSI/ISO принят владельцем. Текущий следующий бренд — DrunkDeer.
UK/FR/DE не являются отдельными категориями раскладок: использовать английские
подписи и различать физическую геометрию. Для A75/Pro/ISO исследован запрос
модели официального Antler; не считать одинаковый VID/PID неразрешимой
неоднозначностью. См. `DRUNKDEER_LAYOUT_IDENTIFICATION.md`.

Для расхождения четырёх навигационных клавиш G65 владелец ответил:
«доверяем официальному драйверу и его позициям». Использовать Antler offsets
35/56/77/98 для Delete/End/Page Up/Page Down вместо ранее предположенных
36/57/78/99. Повторно требовать пользователя с G65 для этого решения не нужно.
Остальные измеренные позиции сохранять. Это выбор источника, а не новый
физический тест; не распространять автоматически на противоречия внутри Antler.

Этап DrunkDeer реализован и собран: семь раскладок, подтверждённая запросом
модель, автоподбор первого запуска и модельные карты UAP. Текущий результат:
`DRUNKDEER_LAYOUTS.md`, EXE `F9FF13072B96C85FC0078EB5B0A45B98D9BAE6599FF14844E75914102213680A`.
Ждём оценки владельцем этого бренда, следующий бренд пока не начинать.

## 2026-09-09 — Экономный конвейер добавления раскладок

Позднее владелец разрешил подготовить общую архитектуру импорта на примере Aula.
Новая работа начинается с `LAYOUT_PIPELINE.md` и команды
`py tools/layout_pipeline.py summary <Brand>`, а не с чтения огромных UI-файлов
или повторного исследования уже поддерживаемого протокола. Источники и решения
сохранять; новое семейство драйвера требует адаптера, модель в известном семействе
добавляется данными. Экономия контекста не отменяет проверок и точной геометрии.
Aula Standard WIN60/WIN68 и WIN60 MAX встроены через общий генератор;
подтверждённая идентификация передаётся из нативной сессии в first-run.
Подробности: `AULA_LAYOUT_PIPELINE.md`. После проверки этого бренда владельцем
выбирать следующий; HERO84 остаётся замороженной.
Не выдавать подготовленные файлы за выпущенную поддержку/аппаратный тест.

Следующий разрешённый этап после Aula — Redragon: K673WB-RGB-M ANSI и
K673RGB-M ISO. См. `REDRAGON_LAYOUTS.md`. BR-карта требует отдельного решения:
официальный источник и существующий backend публикуют IntlRo вместо правого
Shift. Не менять работающий аналоговый протокол и не подставлять UK-идентичность.
Этот этап не даёт разрешения выкатывать следующий бренд без проверки владельцем.

## 2026-09-10 — Завершающий пакет раскладок перед релизом

Владелец принял обе раскладки Redragon и явно разрешил одним пакетом добавить
Razer, NuPhy и Wooting, затем остановить расширение каталога ради выпуска версии.
Это исключение из прежнего правила «один бренд за этап». Не начинать остальные
бренды после этого пакета. Непроверенные геометрии не заменять похожими.

## 2026-09-10 — Объединение раскладок и подготовка релиза

Владелец разрешил объединить проверенные одинаковые раскладки и выполнить всю
локальную подготовку релиза, затем сообщить, что осталось ему. Объединены только
13 пар из LAYOUT_DUPLICATION_REVIEW.md; изменённые пользователем старые модели
сохраняются отдельно. Политику протоколов и распознавания не менять. Кандидат
готовится как 1.5.0; публикация на GitHub и подпись требуют отдельного решения.
Актуальное состояние подготовки: RELEASE_PREPARATION_2026-09-10.md.

## 2026-09-10 — Пользовательская документация релиза

README и патчноут для GitHub остаются английскими. Не писать о «команде HallJoy»
и ежедневном использовании K4 автором. Таблицы должны помогать выбрать устройство,
а не объяснять внутренние протоколы: сохранить полезные ограничения MG75 v2 и O3C.
У Redragon отделять подтверждённую работу от ожидаемой, но не протестированной;
пока подтверждение есть для K673RGB-M BR, не объявлять остальные проверенными.
Веб-драйверы часто конфликтуют; закрытие десктопного ПО вроде Synapse не обязательно,
если всё работает. Не повторять инструкции обращения за поддержкой в нескольких
разделах: использовать один раздел и шаблон. В патчноуте явно отметить появление
Discord-сообщества. Эти уточнения не меняют поддержку устройств или код программы.

## 2026-09-10 — Отзывы перед релизом 1.5.0

Владелец сообщил: последнюю сборку отправил четырём пользователям, все сообщили
о нормальной работе без жалоб. Это пользовательская проверка, а не утверждение
о тестировании всех моделей клавиатур. Запрошена финальная проверка кандидата
и текущего GitHub перед релизом. Проверенный EXE не менять без причины;
публикацию, коммит и тег не считать уже выполненными. Результат проверки:
FINAL_RELEASE_AUDIT_2026-09-10.md.

Владелец разрешил загрузить текущие исходники и выпустить релиз на GitHub.
Уведомление о 100% Actions storage требует экономного хранения артефактов;
не повышать платные бюджеты и не удалять чужие/старые данные без разрешения.
Автоматические CI-проверки сохраняются; архив Actions только при ручном запуске,
срок хранения 3 дня. Пользовательские релизы размещаются в GitHub Releases.

Релиз v1.5.0 опубликован: https://github.com/PashOK7/HallJoy/releases/tag/v1.5.0.
Обе CI-проверки успешны; тег указывает на 31470d22db96111095e5625a5707f44bfcd053c3.
Проверенный пользователями EXE не заменён. Итог: GITHUB_PUBLICATION_2026-09-10.md.

## 2026-09-10 — Один EXE для пользователя

Пользовательский релиз распространяется одним HallJoy.exe, без обязательного
архива и сопутствующих файлов. Владелец явно потребовал убрать ZIP из v1.5.0.
Публиковать проверенный EXE напрямую; архивы сборки остаются внутренними.
SHA-256 EXE: F6CF016FA3D8B15D80EB2AF83BCAE1D8E16F989FE142EBC92D96CA454232079B.

Уточнение владельца: отдельные лицензионные файлы в Assets разрешены и нужны;
ограничение касается запуска — пользователь скачивает только HallJoy.exe.
Публиковать THIRD_PARTY_NOTICES.md и LICENSE как сопроводительную документацию,
с явным объяснением в релизе, что класть их рядом с EXE для запуска не требуется.

## 2026-09-11 — Ошибка профиля не должна закрывать HallJoy

Владелец столкнулся с отказом запуска 1.5.0 на другом ПК со старыми настройками.
Разрешено автоматическое восстановление: сохранить читаемые части и проверить
бекапы, затем сохранить оригиналы и запуститься со сбросом невосстановимого.
Не требовать ручного удаления INI. При невозможности безопасной записи продолжать
в памяти без автосохранения, не уничтожать оригиналы. Не сбрасывать чужие профили
и раскладки. Подробности: STARTUP_PROFILE_RECOVERY_2026-09-11.md.

Владелец подтвердил запуск исправленного EXE на проблемном ПК и разрешил
публикацию 1.5.1. Выпускать именно проверенный EXE с SHA-256
4BA1CD93D1F1BED775B67DB9B65DBB9EE294F03D47AFE6EC948147303DF776C4.

1.5.1 опубликован как Latest: https://github.com/PashOK7/HallJoy/releases/tag/v1.5.1.
Windows/Linux CI успешно прошли на e72cca95d1e1d4cb091768889e8aa4eb4645b037.

## 2026-09-12 — README и единый стиль релизов

README — краткая пользовательская инструкция, не технический отчёт. Не добавлять
в начало текущую версию, ссылки на патчноут/внутренние документы или обещания
работы с прошивками от имени владельца без согласования. Владелец удалил
верхнюю приписку на GitHub; не возвращать её из старой локальной копии.
Таблица совместимости должна быть понятна сама по себе, без extended compatibility
list и дублирующей страницы. Сохранить полезные ограничения и различие между
проверенными и предполагаемо совместимыми устройствами. Experimental firmware
route удалён как несогласованное обещание. Технические документы не удаляются.
Названия релизов: «v<версия>: <краткое описание>», без префикса HallJoy.
Правки оформления не меняют теги, EXE и исторические патчноуты.
Раздел Support располагается сразу после таблицы совместимости и её оговорки,
перед Input Overlay; текст раздела при переносе не меняется.

Владелец явно попросил ссылку на кастомные прошивки в строке Keychron:
https://analogsense.org/firmware/ — готовые образы, изменения исходников и
инструкции сборки. Это согласованная полезная ссылка, не лишняя техническая
страница. Указать возможность подготовки прошивки для других Keychron HE;
не выдавать чужой образ за подходящий для любой модели или ANSI/ISO/JIS.

## 2026-09-12 — Релиз 1.5.2

Владелец подтвердил исправления нумпадного бинда и задержки Block Bound Keys.
По его разрешению опубликован 1.5.2, только с коротким английским патчноутом,
без изменений README. Linux/Windows CI PASS; тег c2e5d287f439311b655afc577642b93e41192661.
Итог: RELEASE_1.5.2_2026-09-12.md. Старый нумпадный бинд рекомендуется назначить
заново один раз; в новой версии он не зависит от Num Lock.

## 2026-09-13 — Текущее исследование IROK/IYX, переход в новый чат

Для продолжения реверса NA87/ND75/MU68 и Pro сначала читать
`IROK_REVERSE_HANDOFF_2026-09-13.md` в этой папке. Там источники, локальные
бинарники, команды, точные результаты, ограничения и порядок дальнейшей работы.
Обычная NA87 ближе к ND75/Witmod, не к Pro/JingTai. В ND75 компонентной эмуляцией
найдена потеря событий (включая отпускание); в десяти Pro-образах найдены массивы
глубин. Это не выпущенная поддержка и не аппаратные тесты. Владелец просил
продолжать исследование без железа: полезная статическая работа ещё не исчерпана.


## 2026-09-13 — Продолжение IROK до внешнего блокера

Владелец попросил продолжать до блокера или выполнения работ перед реальной
клавиатурой. Итог в `../research/IROK_PRE_HARDWARE_STATUS_2026-09-13.md`.
Witmod SDK преобразует host0x29 в wire0x21; полный ND75 scanner теряет releases.
Pro: дескрипторы10 образов, карты87/68/64, serializers эмулированы, полный
producer NA871.0.8 проверен; queue теряет chunks при full/busy. Есть offline
parser и общий прогон11 scripts. Это не аппаратный PASS и не включение поддержки.
Дальнейший вывод об ordinary NA87/MU68 требует точного firmware/capture;
вывод о качестве Pro — аппаратных USB timing/input tests. Старое указание
«offline работа ещё не исчерпана» относилось к этапу до этого продолжения.


## 2026-09-13 — Новое решение: Pro заморожен, углублённое ревью ND75

Владелец попросил сохранить/заморозить NA87 Pro и копать дальше ND75/Witmod.
См. `IROK_PRO_FROZEN_2026-09-13.md`: проверенный ZIP+SHA256 manifest,71 файл.
Активно ревьюить код, эмуляцию, возможные альтернативные read paths и
обоснованность выводов об ordinary NA87. Не превращать прежний вывод
о блокере в запрет продолжать исследование; не продвигать Pro без нового решения.


## 2026-09-13 — Реальный тестировщик только NA87; один расширенный пакет

Владелец уточнил: тестировщика с ND75 нет, есть с обычной NA87.
Метаданных недостаточно: нужен один пакет, который перебирает доступные
стандартные способы обмена и проводит несколько аналоговых сценариев за один
запуск, без выдачи новой сборки под каждую гипотезу. Создаётся отдельный
инструмент tools/irok_na87_diagnostic; план и статус в его PLAN.md.
ND75 diagnostic не выдавать за NA87 test. Pro остаётся замороженным.


## 2026-09-17 — AULA MINI60 HE Pro diagnostic, completion by evidence

The owner requested one robust integrated HallJoy.exe for the MINI60 HE Pro tester,
and then questioned a fixed-duration run. Follow the current implementation/status in
`AULA_MINI60_DIAGNOSTIC_2026-09-17.md`: complete by observed presses, depth variation,
multiple held keys and releases; do not restore a fixed 40/60-second typing schedule.
Keep normal HallJoy/gamepad functionality, English UI/logs/comments, one returned
HallJoy.log, and the existing IrokNa87Diagnostic delivery directory. Wired 0C45:80A2
is the command target; 0C45:FEFE is the receiver and receives no speculative commands.
Software checks passed; the first real hardware diagnostic result is still pending.


## 2026-09-17 — AULA MINI60 HE Pro playable native support

Supersedes the previous pending-first-hardware-result statement. Tester logs 13
and 14 confirmed changing analog depth on firmware V1.52; log 14 confirms three
simultaneously held keys. The owner requested real gamepad use with continued
logging. See `AULA_MINI60_NATIVE_SUPPORT_2026-09-17.md` for the completed
experimental implementation, exact artifact and checks. Same existing delivery
folder and single HallJoy.log; ordinary HallJoy bindings/gamepad remain active.
No fixed diagnostic time window, no eight-key completion requirement for gameplay.
Wired only. Automatic layout reads supported assignments, manual uses factory map.
Per-position 50 ms freshness is an initial policy, not a hardware quality guarantee.
Software validation passed; first gameplay feedback remains pending.


## 2026-09-19 — NA87 and MINI60 ordinary support approved

The owner accepted the existing NA87 hold behavior after logs15/16 and requested
ordinary support for both NA87 and AULA MINI60 HE Pro. NA87 is no longer frozen;
remove its amber notice, preserve the other frozen models. Do not add arbitrary
NA87 expiry or require another tester confirmation as a release gate. This does
not establish the missing final depths from log16. Current implementation and
artifact: `NA87_MINI60_STANDARD_SUPPORT_2026-09-19.md`. Both native backends are
included by default; wired AULA only; one support log, no timed test or special
window title, same delivery path. Native AULA operation must not depend on logging.


## 2026-09-19 — ATTACK SHARK research, all three are Pro

Owner has heard from three users and requests further research of X65 Pro HE,
X68 Pro HE and X82 Pro HE. Explicit clarification: all Pro. Preserve exact
revision distinction; old non-Pro X65 reference is not their firmware. Newly
available exact X82 dev2935 v503 has been acquired and its table getter/scanner
publication blocks component-tested. See ../research/ATTACK_SHARK_PRO_REVIEW_2026-09-19.md.
No new diagnostic or production backend was delivered by this research step.


## 2026-09-19 — ATTACK SHARK integrated test build delivered

Owner currently has a tester only for X65 Pro HE and requested a test build.
Supersedes the research-only delivery status above. See
`ATTACK_SHARK_PRO_DIAGNOSTIC_2026-09-19.md` for the exact artifact and validation.
One HallJoy.exe in the existing IrokNa87Diagnostic directory; one adjacent log.
The reader recognizes six exact Pro revision/PID pairs but X65 hardware remains
untested. Page reads run continuously, completion depends on evidence, not time.
No stream/calibration/settings modes are enabled. ATTACK SHARK data is diagnostic
only in this build, not gameplay analog. Existing ordinary native support and
gamepad behavior remain enabled. All UI, logs and code comments stay English.


## 2026-09-19 — neutral delivery directory and ATTACK SHARK preflight

Owner explicitly objected to continued use of the IROK-named delivery folder.
Current delivery is build/bin/Release/x64/HallJoy.exe, with adjacent HallJoy.log;
this supersedes earlier instructions to keep the IrokNa87Diagnostic directory.
Existing artifacts and owner log were moved, not duplicated. The reviewed owner
log shows Keychron, zero eligible ATTACK SHARK collections, no Feature commands,
and clean shutdown. Revised shark-2 build improves rejection/failure evidence;
see ATTACK_SHARK_PRO_DIAGNOSTIC_2026-09-19.md. Hardware read validation is pending.


## 2026-09-19 — ordinary release candidate, owner test before publication

Owner requested GitHub publication of NA87 and AULA MINI60 HE Pro support, then
explicitly required testing the ordinary EXE first. This is a publication gate:
do not push release sources, create/publish a GitHub release or upload its assets
before the owner's confirmation after testing. Prepare a normal build with
ATTACK SHARK diagnostic disabled. Preserve the already-sent shark-3 tester EXE
in .local/backups/HallJoy-attackshark-shark3.exe. Delivery remains the neutral
build/bin/Release/x64/HallJoy.exe. Do not create an application ZIP.

Candidate1.5.3.0 is ready for owner testing; exact artifact and checks are in
RELEASE_1.5.3_CANDIDATE_2026-09-19.md. Publication remains gated on owner feedback.


Owner caught forced release logging despite Enable logging off. The ordinary
1.5.3 candidate now excludes unconditional diagnostic trace/support macros;
native NA87/AULA do not imply tester logging. Existing settings-aware support
logging and automatic incident reports remain. Candidate document records the
fixed artifact; publication still waits for owner testing.


## 2026-09-19 — publication authorized after owner check

Owner reported the corrected EXE looks fine and explicitly requested GitHub
publication. The test-before-publication gate is now satisfied. Publish1.5.3
with the tested ordinary EXE SHA256
5481aa57e84ae7378193ef559d530b20cd34f771f8904d615e3e39afbbd26621.
Keep ATTACK SHARK diagnostic disabled. Verify source and CI before publication;
ship HallJoy.exe with license notices, no application ZIP.


## 2026-09-19 — standing authorization to close HallJoy

Owner explicitly authorizes closing running HallJoy whenever needed for builds,
EXE replacement, diagnosis or automated tests, without asking again. Prefer
graceful window closure and wait for shutdown; if hung, terminate the verified
HallJoy process and its children. Do not create extra build directories merely
to avoid closing a running EXE. This permission persists across sessions.
The ordinary HallJoy and its children closed gracefully before local release
profile checks; no additional owner confirmation is required.


## 2026-09-19 — v1.5.3 published

Stable/latest GitHub release v1.5.3 is public, at source2d9cf1395aeaa3b2bf02966bbe1b55e5fa194186.
Both jobs in CI35444148705 passed. Published EXE matches the owner-tested
SHA2565481aa57e84ae7378193ef559d530b20cd34f771f8904d615e3e39afbbd26621.
See RELEASE_1.5.3_CANDIDATE_2026-09-19.md for final verification and history.

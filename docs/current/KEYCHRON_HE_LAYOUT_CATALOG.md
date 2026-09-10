# Keychron HE: каталог кандидатов на импорт раскладок

Проверено 2026-09-09. Это каталог источников геометрии, не перечень уже
добавленных в HallJoy пресетов. Контекст UAP: OWNER_CONTEXT.md.
Обновление: в код встроено **36** точных вариантов; детали реализации,
проверок и оставшихся исключений — [KEYCHRON_COMPOUND_LAYOUTS.md](KEYCHRON_COMPOUND_LAYOUTS.md).
Старое описание ниже сохраняет историю поиска, не статус готовности.

## Варианты с собственными ссылками на JSON производителя

| Модель | Варианты |
| --- | --- |
| K2 HE | ANSI, ISO, JIS |
| K4 HE | ANSI, ISO, JIS |
| K6 HE | ANSI, ISO |
| K8 HE | ANSI, ISO, JIS |
| K10 HE | ANSI, ISO |
| Q0 HE | Numpad, encoder |
| Q1 HE | ANSI, ISO, JIS; encoder |
| Q2 HE | ANSI, encoder |
| Q3 HE | ANSI, ISO, JIS; encoder |
| Q4 HE | ANSI |
| Q5 HE | ANSI, ISO, JIS; encoder |
| Q6 HE | ANSI, ISO, JIS; encoder |
| Q12 HE | ANSI, encoder |
| Q1 HE 8K | ANSI, ISO, JIS |
| Q3 HE 8K | ANSI, ISO, JIS |
| Q6 HE 8K | ANSI; ISO/JIS обозначены в разделе firmware, JSON ещё не подтверждены |

Источник: https://www.keychron.com/pages/firmware-and-json-files-of-the-keychron-he-series-keyboards

Важно: наличие ссылки не равно завершённому импорту/проверке всех клавиш.
На странице встречаются скопированные ссылки на другие модели: Q2/Q4 ISO/JIS
ведут на Q1, Q12 ISO/JIS на Q6, старый K6 JIS на K2. Не считать их
доказательством существования соответствующей модификации.

## Дополнительные модели из каталога, извлечение геометрии впереди

- K3 HE: основной вариант и ISO (Nordic/German/UK).
- J12 HE: 75%, основной ANSI; другие геометрии не подтверждены.
- J14 HE: 96%, основной вариант; другие геометрии не подтверждены.
- Q2 HE 8K: основной вариант.
- Q5 HE 8K: основной вариант с энкодером.
- Q16 HE 8K: основной вариант и ISO.
- C0 HE 8K: одноручная клавиатура, отдельная геометрия.

Источники:

- https://www.keychron.com/collections/keychron-he-keyboards
- https://www.keychron.com/collections/q-he-8k-series
- https://www.keychron.com/products/keychron-k3-he-wireless-magnetic-switch-custom-keyboard
- https://www.keychron.com/products/keychron-j12-he-wireless-magnetic-switch-custom-keyboard
- https://www.keychron.com/products/keychron-j14-he-wireless-magnetic-switch-custom-keyboard
- https://www.keychron.com/products/keychron-q16-he-8k-magnetic-switch-keyboard

Проверялись также Shopify products.json этих коллекций и options карточек.
Отсутствие варианта в проверенных источниках не доказывает его отсутствие
во всех региональных магазинах. Не называть каталог гарантированно полным.

K2 HE Standard/Special/All-Wood/Concrete/Resin не считать отдельной геометрией
только по материалу/цвету. Перед объединением идентификаторов сравнить JSON.
Национальные ISO легенды не означают новую геометрию.
Lemokey P1/P2/P3 HE — отдельная линейка, не смешивать с Keychron K/Q/J.
DayZ Special Edition требует проверки базовой модели; не приписывать Q1
по случайному тексту страницы (официальная landing page называет P1 HE).

Для каждого импорта отдельно проверять VID/PID, матрицу, назначения клавиш,
геометрию и запись автоподбора. HE 8K не отождествлять с обычной HE по имени.

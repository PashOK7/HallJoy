# База знаний по прошивке

Архив `MAD68_Pro_R_Firmware_Knowledge_Base_2026-07-29.zip` проверен по внутреннему
manifest: 69 отслеживаемых файлов, расхождений SHA-256 и размеров не найдено.

Основные подтверждённые выводы:

- vendor HID: IF1, EP04 OUT, EP82 IN, 64-byte payload;
- активация: сериализованные `A9 -> A8 -> A9` с ACK;
- async packet: `A0`;
- основной analog: big-endian `A0[4..5]`, диапазон `0..1600`;
- 68 физических descriptors из 72 scanner slots;
- runtime allow-list команд: только volatile `A8` и `A9`;
- stock scheduler ограничивает aggregate stream примерно одним пакетом за 15 ticks.

Сначала читать `00_README_MASTER_RU.md` внутри архива.

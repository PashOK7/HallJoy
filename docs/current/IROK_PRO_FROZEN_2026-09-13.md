# IROK Pro: прогресс заморожен по решению владельца

2026-09-13: владелец попросил сохранить и заморозить NA87 Pro, продолжать
обычную NA87 через ND75/Witmod и провести ревью кода и доказательств.

Архив: `.local/research/irok-na87/frozen-pro-20260913-131658.zip`

SHA256: `7e26fd373c5cf0545d80320ffb028e9c7c5df1e66f9a91b3e4619a3a63588a8f`. 71 файл, каждый проверен после чтения из ZIP.
Рядом JSON manifest с исходными путями, размерами и SHA256 каждого файла.
Архив содержит firmware/updaters, listings, карты, scripts, pinned web bundles,
снимки research docs и последний общий offline verification report.
Не распространять vendor binaries в релизе HallJoy.

Состояние: независимые массивы глубин найдены во всех десяти образах;
serializer emulation PASS для всех десяти; полный producer эмулирован только
для NA87 Pro1.0.8. USB descriptors и physical maps проверены. Есть offline
parser. Остались аппаратные transport/input/calibration tests, известны
full/busy queue losses и отсутствие sequence/half ID. Hardware PASS нет.

Документы состояния: `../research/IROK_PRO_FIRMWARE_PROTOCOL_2026-09-13.md`
и Pro-разделы `../research/IROK_PRE_HARDWARE_STATUS_2026-09-13.md`.
Архив сохраняет их точную версию, даже если общие handoff docs обновятся.

Дальше не расширять Pro, не менять его scripts/firmware/maps и не перезапускать
работу по Pro без нового решения владельца. Активная ветка: ND75/Witmod,
обычная NA87(GK8260HERGB), обычная MU68(GK8152HERGB).

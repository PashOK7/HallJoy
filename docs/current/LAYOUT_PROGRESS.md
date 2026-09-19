# Layout progress — 2026-09-10

> 2026-09-14 follow-up: [IPI layouts](IPI_LAYOUTS_2026-09-14.md): four merged ANSI presets for
> eight models. Official demo defaults resolve the previously missing labels;
> the separate Addressed fallback discrepancy remains documented at its table.
> O3C and Plus revisions remain excluded. No new analog routes enabled.

> 2026-09-14: [добавлена ATK Hex80 ANSI](REMAINING_LAYOUTS_2026-09-14.md), 87 отображаемых клавиш.
> O3C исключён владельцем; другие кандидаты требуют отдельного разбора и отложены.
> Только раскладка: текущая Hex80-таблица аналоговых слотов отличается от официального
> профиля, протокол не изменён. Проверки/сборка PASS; основной HallJoy.exe обновлён.

> 2026-09-14: добавлены [четыре раскладки MADLIONS](MADLIONS_LAYOUTS.md) по запросу владельца:
> MAD60HE, MAD68HE, MAD68R (поддерживаемая ревизия10A7) и MAD 68 Pro R, ANSI.
> Проверки и сборка PASS; выбор вручную в MADLIONS. Визуал оценивает владелец.

> 2026-09-13: по новому запросу владельца добавлены [IROK MG75 Max / Pro ANSI](IROK_MG75_LAYOUTS.md),
> по81 клавише, выбор в каталоге IROK. Проверки/сборка пройдены; визуал оценивает владелец.

Owner accepted both Redragon ANSI/ISO layouts on this date.
Installed manufacturer presets: 68 (not user-created or technical presets).
Latest +18 and explicit limits: `FINAL_LAYOUT_BATCH.md`. Owner has not yet
visually assessed this final batch. Catalog expansion is now paused for release.

| Brand | Physical variants |
|---|---:|
| Keychron | 36 |
| Lemokey | 2 |
| DrunkDeer | 7 |
| Aula | 3 |
| Redragon | 2 |
| Razer | 3 |
| NuPhy | 2 |
| Wooting | 13 |

Count verified against layout_pipeline reports (+ original K4/Q1 ANSI) and the
built-in registry in keyboard_layout.cpp. ANSI/ISO/JIS count separately.

Historical remaining-work estimate BEFORE the final +18 batch (not current):
named model/profile work from SUPPORTED_HARDWARE.md and pinned Soup:
Razer (5), NuPhy Air60/Air75 (2), MADLIONS MAD60HE/MAD68HE/MAD68R/MAD68 Pro R (4),
Irok MG75 Max/Pro (2), ATK Hex80 (1), Sayo O3C (1), QBZ75 family (1),
KP-TE153 (1), Redragon K673 BR (1): 18 named models/profiles, plus Wooting
(at least One/Two explicitly named in the legacy route, modern route is open).
This is NOT an exact remaining-layout count: physical variants, shared geometry,
and dynamically admitted family members are not completely enumerated yet.
Do not report an exact denominator or completion percentage from these counts.
Frozen ROG/AULA HERO84/IROK ND75/Attack Shark X68 are outside this work.

BR's new layout remains pending; existing analog support is untouched. No new
brand was added during this read-only status audit. No build or runtime changes.

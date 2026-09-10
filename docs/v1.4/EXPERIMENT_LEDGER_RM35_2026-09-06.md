# RM-35 — реестр экспериментальных и диагностических маршрутов

Дата: 2026-09-06.  Статус: активный реестр решений, не список обещаний
поддержки.

## Правило выпуска

Обычная сборка не должна содержать экспериментальный источник, дескриптор или
автоматический HID-claim.  Каждая строка ниже либо остаётся отдельной
диагностической/тестовой целью, либо имеет конкретный путь к решению.  Наличие
исходника, VID/PID или прошивки не является доказательством поддержки.

| ID | Ветка / opt-in свойство | Нынешний статус и reachable build | Последнее применимое evidence | Условие изменения статуса |
|---|---|---|---|---|
| E-01 | `HallJoyMadlionsDiagnostic` | Историческая безопасная диагностическая цель; не ordinary catalog | `src/HallJoyProject/tools/build_madlions_diagnostic.ps1`, `backend.cpp` guarded branch | Сначала отдельная инвентаризация реального устройства и явный safety contract; иначе архивировать цель после reference scan. |
| E-02 | `HallJoyAnalogSimulator` | Simulator-only, не HID proof и не production route | `FIRMWARE_VIRTUAL_HID_TESTBED_ROADMAP_2026-08-23.md`, simulator source gates | Оставить как synthetic regression oracle; не использовать для продвижения клавиатуры в supported. |
| E-03 | `HallJoyUiAudit` | UI-audit цель, не release artifact | `HallJoy.vcxproj` target boundary | Нужен отдельный UI evidence package; не смешивать с input qualification. |
| E-04 | `HallJoyAulaAggressiveTrace` / общий `HallJoyDiagnostic` | Существующая диагностическая инструментализация известных Aula-маршрутов; обычная цель её не включает | `tools/build_aula_diagnostic.ps1`, production preflight в `tools/build.ps1` | Хранить только при named diagnostic procedure; удалить, если consumers и процедура исчезнут. |
| E-05 | `HallJoyIrokNd75Diagnostic` | IROK ND75 M484 experimental owner build, не ordinary build | `PROTOCOL_AUDIT_RM30_ND75_2026-09-06.md`, `IROK_ND75_M484_STATIC_ANALYSIS_2026-08-17.md` | Добровольный владелец + exact firmware/trace + simultaneous letters/analogue/release/reconnect/output gates. |
| E-06 | `HallJoyDrunkDeerDiagnostic` | DrunkDeer G65 diagnostic-only; source and descriptor excluded normally | `DRUNKDEER_DIAGNOSTIC.md`, `PROTOCOL_AUDIT_RM30_DDNATIVE_2026-09-06.md` | Exact-final-artifact physical rerun and layout-specific map/transport proof before any release proposal. |
| E-07 | `HallJoyMchoseAce68Diagnostic` | MCHOSE Ace68 transport recorder only; no gameplay ownership | `PROTOCOL_AUDIT_RM30_RESEARCH_2026-09-06.md` | Real trace must prove map, scale and simultaneous ordinary typing; never infer them from digital correlation. |
| E-08 | `HallJoyRogAzoth96HeDiagnostic` | Frozen, transport-only ROG Azoth 96 HE/M901 diagnostic; excluded from ordinary image | `docs/research/ROG_AZOTH_96_HE_M901_TEST_DIAGNOSTIC_PLAN_2026-09-06.md`; RM-32 build-chain exclusion | Wait for a consenting tester. Then build only named diagnostic, review log/typing observations, and make a new decision. No build, launch, HID access or distribution while frozen. |
| E-09 | `HallJoyAulaHero84HeDiagnostic` | Read-only AULA HERO84 HE diagnostic; one-backend target | `PROTOCOL_AUDIT_RM30_HERO84_2026-09-06.md` | Consenting owner must run diagnostic and prove identity/transport safety before an experimental gameplay target is considered. |
| E-10 | `HallJoyAulaHero84HeExperimental` | Frozen experimental AULA HERO84 HE gameplay target; source/catalog excluded normally | `AULA_HERO84HE_SCOPE_FREEZE_RM29_2026-09-06.md` | Consenting owner test: simultaneous letters and analogue, zero release, reconnect, safe stop, then full parser/session/XUSB gates and explicit release-scope decision. |
| E-11 | `HallJoyTitan68TurboDiagnostic` | Titan68 Turbo visual/transport diagnostic; isolated catalog | `PROTOCOL_AUDIT_RM30_RESEARCH_2026-09-06.md`, `MADLIONS_TITAN68_TURBO_REVERSE_ROADMAP.md` | Real trace and tested restoration of the reversible simulation state; still no calibration/persistent write. |
| E-12 | `HallJoyTitan68TurboExperimental` | Titan-only experimental gameplay image; excluded normally | `MADLIONS_TITAN68_TURBO_REVERSE_ROADMAP.md` | Real device must prove ordinary typing together with analogue, release/reconnect and restored state before any release decision. |
| E-13 | `HallJoyProviderV2Qualification` | Qualification artifact for the ordinary Provider V2 architecture, not a keyboard route or release artifact | `PROVIDER_V2_QUALIFICATION_EVIDENCE_DESIGN_2026-08-23.md` | Keep only as hash-bound qualification tool; physical evidence is separately recorded against the ordinary artifact. |
| E-14 | Virtual-HID/firmware LAB-01…06 | Design program, no implemented alternative production route | `FIRMWARE_VIRTUAL_HID_TESTBED_ROADMAP_2026-08-23.md` | Start only for a named unanswered risk; every result declares E0–E3/P and cannot substitute for physical proof. |

## Mechanical boundary check

`tools/check_experiment_ledger.py` compares every opt-in experimental,
diagnostic, simulator, audit and qualification property in `HallJoy.vcxproj`
with this ledger, checks its exact status field and confirms that the ROG source
has an ordinary-build exclusion.  It is deliberately source-only: it neither
builds nor launches an experimental image, contacts HID hardware, or makes an
analogue-support claim.

## Disposition

No branch is promoted or deleted by this card.  The two owner-waiting branches
(E-08 and E-10) are retained frozen because their isolated source and test plan
provide a safe future evidence path.  The remaining diagnostic branches stay
diagnostic-only until their row-specific gate is met.  The virtual-HID program
remains research infrastructure and explicitly cannot qualify physical hardware.

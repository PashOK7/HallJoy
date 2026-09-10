# RM-34 — точный artifact: имеющееся evidence и незакрытые границы

Дата: 2026-09-06. Статус: частичная автоматическая квалификация; это не
release approval.

## Зафиксированный ordinary artifact

Проверенный EXE:
`build/bin/MAD68ProRNative/Release/x64/HallJoy.exe`.

- SHA-256: `F2068156F7874FCD4CE4D677BBA048793D6706179637932553104BE90ADAEC58`.
- Все нижеприведённые smoke/cycle evidence относятся именно к этому hash или
  к хеш-проверенной портативной копии этих же bytes.
- ROG Azoth 96 HE diagnostic не включён, не собран и не запускался.

## Автоматически проверенное

| Проверка | Evidence | Что доказано | Чего не доказывает |
|---|---|---|---|
| 10 isolated lifecycle cycles | `build/evidence/rm34_release_cycles_20260906_194300/summary.json` | Обычная копия EXE запускается/закрывается, не оставляет HallJoy process и не меняет 115-файловый live state snapshot. | Длительную работу, сон/пробуждение, unplug/reconnect или работу на иной клавиатуре. |
| Overlay HTTP smoke | `build/evidence/rm34_overlay_smoke_retry_20260906_195100/` | Framing, origin/session, 16-client limit и 2000 malformed requests в прежней изолированной копии. | Самостоятельную изоляцию runner и физический HID. |
| Self-isolated overlay smoke | `build/evidence/rm34_production_smoke_self_isolated_20260906_202600/summary.json` | Новый runner создал portable copy, сверил hash, прошёл responsiveness/framing/concurrency/fuzz, штатно остановил EXE, не нашёл diagnostic/crash log и подтвердил неизменность 115-файлового live state. | Аналоговую точность, буквы вместе с analogue, controller continuity либо поддержку неподключённой модели. |

Последняя overlay-проверка: 0.6 ms response, 2.0 ms максимальная задержка
параллельного запроса, 2000 fuzz iterations (1462 response / 298 closed / 240
reset / 0 timeout). Это измерение конкретного локального окна, не общий SLA.

## Исправленная безопасность runner

`tools/run_production_smoke.ps1` теперь сам создаёт каталог evidence, копирует
исходный EXE и current HallJoy state только внутрь `portable-runtime`, создаёт
`HallJoy.portable`, проверяет SHA-256 копии и записывает до/после manifest
реального user state. Он никогда не удаляет trace/log из каталога исходного EXE.
Любое изменение live state делает проверку красной. Это устраняет зависимость от
ручной staging-процедуры, но не меняет поведение тестируемого artifact.

Статические проверки после изменения: `overlay_concurrency_origin_static_audit`,
`overlay_http_framing_static_audit`, `prerelease_hardening_static_audit` и
`provider_v2_data_plane_windows_static_audit` прошли. Отдельно PowerShell parser
принял runner.

## Остаётся обязательным до выпуска

1. Выбрать версию, support boundary, release notes и provenance policy.
2. Выполнить только применимые device-specific physical gates на доступных
   владельцах: native и UAP analogue + ordinary letters, release-to-zero,
   output continuity, unplug/reconnect, suspend/resume и driver-absent path.
3. Если нужен подписанный пакет, выполнить qualification на финальных
   подписанных bytes; изменение EXE после этого обнуляет evidence.
4. Сформировать allowlisted package manifest. Отсутствие Git repository на
   текущем дереве фиксируется как provenance gap, а не маскируется тегом.

Не следует требовать от одного владельца отсутствующие у него модели клавиатур.
Для каждой недоступной модели evidence остаётся честно conditional, а не PASS.

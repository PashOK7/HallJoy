# HallJoy Addressed Analog v5.0.1 — исправление сборки

Исправлены две проблемы исходного пакета v5 Stable Clean:

1. `g_physicalDown` использовался общим кодом `backend.cpp`, но его объявление ранее находилось в удалённом `backend_aula.inc`. Объявление перенесено в общий backend рядом с таблицами `g_hidToScan`/`g_hidToVk`.
2. Устранено предупреждение MSVC C4244 в расчёте percentile RTT. Преобразование `size_t → double` теперь явное, а индекс ограничивается в типе `size_t`.

Логика addressed polling, scheduler и протокол не изменялись.

Сборка:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\BUILD_ADDRESSED_ANALOG_V5_STABLE.ps1
```

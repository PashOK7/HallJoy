# HallJoy Madlions V6: полноценный SafeHID transport

## Что доказали полевые логи V5

За одну сессию внешний supervisor зарегистрировал восемь одинаковых сбоев
analog-host. Они происходили через разные интервалы — от нескольких секунд до
нескольких минут — и не были привязаны только к запуску Steam.

Во всех случаях:

- исключение `0xC0000005`;
- операция `execute`;
- адрес `0x21`;
- checkpoint `madlions_before_receive`;
- до сбоя все poll/snapshot были успешны;
- основной HallJoy продолжал работать и запускал новый host.

Следовательно, парсер ещё не начал разбирать новый ответ. Повреждение возникало
в границе между окончанием `sendReport` и ожиданием `receiveReport`.

## Нарушение в старом Windows HID пути

Soup открывает HID с `FILE_FLAG_OVERLAPPED`. Старый код:

1. заранее запускал `ReadFile` с постоянным `OVERLAPPED`, у которого
   `hEvent == NULL`;
2. запускал `WriteFile` с локальным стековым `OVERLAPPED`, также с
   `hEvent == NULL`;
3. ждал запись через состояние общего file handle;
4. затем ждал чтение.

Когда на одном handle есть несколько overlapped-операций, состояние общего
handle не является однозначным completion object конкретной операции. Если
ожидание записи было разбужено завершением чтения, стековый `OVERLAPPED` мог
перестать существовать до фактического окончания write I/O. Позднее завершение
драйвером записывало результат в уже переиспользованную память стека. Это
соответствует наблюдаемому отложенному `execute 0x21`.

## Реализация V6

Madlions больше не использует старую комбинацию
`discardStaleReports/sendReport/receiveReport`.

Новая транзакция:

```
drain stale input
  └─ pending probe обязательно CancelIoEx + drain
write request
  └─ собственный persistent OVERLAPPED + manual-reset event
  └─ полное completion/cancel-drain
read response
  └─ другой persistent OVERLAPPED + manual-reset event
  └─ полное completion/cancel-drain
validate length/layout/travel
publish snapshot
```

Write и read никогда не outstanding одновременно. Адреса контекстов и буфера
стабильны до завершения I/O. После timeout ни один объект не очищается и не
переиспользуется, пока Windows не сообщит окончательное completion.

## Ошибки и reconnect

Последний Win32 transport error передаётся в shared memory и попадает в crash
report. Восемь последовательных неудачных ответов переводят клавиатуру в
disconnected-state. Плагин возвращает контролируемую ошибку, host немедленно
публикует нейтральный snapshot и завершается. Supervisor создаёт новый процесс,
который заново перечисляет HID-интерфейсы и открывает новый handle.

Это reconnect, а не crash recovery: при штатной PnP-ошибке dump не создаётся.

## Изоляция остаётся

Process isolation сохраняется как независимая последняя граница. Корректный
transport должен устранить известный механизм повреждения. Изоляция защищает
основное приложение от неизвестных дефектов firmware, драйвера или стороннего
кода и не заменяет исправление transport.

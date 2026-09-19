# ND75 read-path follow-up and ordinary-family host evidence

> После ревью внесены [исправления ND75](IROK_ND75_IMPLEMENTATION_FIXES_2026-09-13.md):
> wire opcode и проверка диапазона исправлены. Полнота состояния остаётся
> нерешённой; актуальные проверки сборки перечислены в новом документе.


> Актуальное уточнение 2026-09-13: [ревью ND75](IROK_ND75_REVIEW_2026-09-13.md).
> Найдены ошибки wire opcode и публикации состояния в experimental backend;
> исправлена одна serializer fixture. Прежний вывод об отсутствии любых
> альтернативных чтений слишком широк: сохранён частичный результат52/81.
> Pro заморожен; эксперименты вне штатной таблицы настроек приостановлены
> по последнему согласованному направлению, продолжается обычное ревью.


> Исправление 2026-09-13: 0x29 — opcode входа Witmod SDK; фактический HID
> opcode — 0x21. Полный scanner и все serializer entry points теперь проверены
> offline. См. [IROK_WITMOD_OFFLINE_CLOSURE_2026-09-13.md](IROK_WITMOD_OFFLINE_CLOSURE_2026-09-13.md).


2026-09-13. Static analysis and actual ARM component emulation only.
Continuation of IROK_REVERSE_HANDOFF_2026-09-13.md. No runtime changes,
physical HID access, vendor executable execution, firmware installation or downloads.

## Image and method

ND75_V12_flash_512K.bin, SHA256
`A5168399CACA4BBD2C04A1E988F478265364577A46994199A73848ABF0AB735F`.
ARM addresses below are image/runtime addresses. Local reproducible test:
`.local/research/irok-na87/nd75_read_path_emulation.py`.
The test executes actual handler/scheduler instructions with Unicorn and synthetic
RAM. It pins the input hash, checks return to a sentinel (instruction budget cannot
silently count as completion), and audits all executed RAM writes.

## Addressed read 29/18/05 returns configuration, not depth

The subcommand dispatcher at 0xf5be reads byte 6. Its TBB at 0xf5ce uses
14 bytes at 0xf5d2. Subcommand 5 reaches 0xf6ac, saves request bytes 7/8
as row/column at `0x20003fec+0xdd/+0xde`, sets state 5 at +0xdc and
pending bit 21 at 0x20004a20. Scheduler table 0x136f0 selects serializer
0xf812 for bit 21; subcommand-5 serializer arm is 0xf856..0xf90e.

Source is `0x2000127c + (row*22+column)*8`, fields **0, 2, 1, 6, 7**,
written to response bytes 7..11. This is the SAME configuration record written
by subcommands 0/1 at 0xf604..0xf61c. Scanner branches 0x3660..0x371e
consume these fields as mode, actuation/release and deadband parameters.
It does not read live cache `0x20003fec+0x124+row*22+column`.

The firmware writes 4 to response byte 5 although five record fields are
written after subtype byte 6. Preserve the observed bytes; do not derive a
new transport length convention from this one handler.

Actual-code test: four distinct row/column pairs with distinct config fields;
change cached depths from [17,29,7,40] to [0,30,0,11] and then all zero.
Every addressed reply still contains its own config fields, unchanged.
The complete config region is unchanged. Executed writes are confined to stack,
request state, pending flags, transmit buffer and transmit-busy bookkeeping.
**PASS: this addressed read cannot recover the lost live depths.**
This is a component write audit, not authorization to send it to hardware.

## Six-row read is not an analog-array read

Internal request table at 0x13664, opcode 0x10 -> 0xfa25 (Thumb).
Handler 0xfa24 resets row cursor at 0x20004a1e and sets pending bit 8.
Serializer 0xfa34..0xfaac emits six reports, each with 22 bytes.
Source: flash table 0x131b0, 132 pointers; each non-null pointer is read at
**pointer+0x0c** (0xfa8c). Missing entries remain zero after scheduler clears TX.
Rows 0..4 have response bytes 3/4 = 00/01 through 00/05; last row uses FF/FF.
Byte 5 is 22; bytes 6..27 are the row values. Serializer returns 2 while rows
remain, keeping its pending bit set; last row returns 1 and clears it.

Actual-code test uses the unmodified flash pointer table and distinct synthetic
pointer+0x0c bytes. All six reports match. Replacing all 132 live-cache depths
with 40 leaves every report identical. Config remains unchanged and actual
writes are confined to TX/flags/stack and row cursor. **PASS.**

Independent code supports identifying +0x0c as a key code rather than depth:
0xeb16..0xeb26 compares this field with 0x47 and 0xe3 in special-key branches;
0x10838..0x10850 obtains the same field from the same matrix table and places it
into a key-event record. This is a code-based interpretation, not a recovered
vendor type name. Do not describe this six-row response as a depth snapshot.
The internal opcode is established; its outer USB transport route was not
executed or fully traced in this pass.

## Subcommand coverage and remaining search

Decoded exact TBB destinations:

| Subcommand | ARM target | Observed behavior |
| --- | --- | --- |
| 0, 1, 12, 13 | 0xf5e0 | Write selected configuration records, then persistence path 0xe2a0 |
| 2 | 0xf648 | Copy subscription masks |
| 3 | 0xf674 | Clear subscription masks |
| 4 | 0xf696 | Capability response (40) |
| 5 | 0xf6ac | Addressed config read, emulated above |
| 6 | 0xf6ce | Calls 0x3a44, 0x27f8, then 0xe2a0; not a pure read |
| 7 | 0xf6f2 | Mutates calibration state and selected record +4; calls 0xe598 |
| 8 | 0xf73e | Calibration/state mutation; one branch clears all record +4 values |
| 9 | 0xf7b2 | Can change USB-base+0xb5; calls 0xe2ae |
| 10 | 0xf7ae -> 0xf7ea | Can change USB-base+0xb4; calls 0xe2ae |
| 11 | 0xf670 | Return without requesting a reply |

This eliminates the obvious row/column read and six-row matrix read as live-depth
alternatives for this exact image. It is NOT an exhaustive proof that all vendor
commands, memory access paths or other firmware versions lack analog polling.
Other non-null request-table handlers still need systematic read-source coverage.
Scanner 0x3190 begins by waiting via 0x72a6 and loops physical columns/rows;
service calls scheduler at 0x25aa. Full task creation/priority/preemption tracing,
including the pointer at 0x1ff18, remains open. Event-loss reproduction stands.

## Ordinary NA87 host: interface and SDK delegation

Pinned Go service SHA256
`97D53F6447C6F5203436E2306B9C1321D1B05B473A1CD069B7104542754F2004`.
New local listings: go_NewMagnet.asm, go_connectMagnet.asm,
go_FirmwareInfo.asm, go_SendData.asm. They were created exclusively, without
replacing older listings. Addresses here are PE virtual addresses.

- connectMagnet 0x7abde6 checks VID 0416, then compares PID with object field +8.
  0x7abe0c references seven bytes at 0x8f7ac7: `&mi_02#`; the following search
  rejects paths lacking that substring. This confirms MI_02 selection in this
  ordinary magnetic host branch, not exact NA87 product admission.
- NewMagnet resolves named Witmod SDK procedures, including OpenPath, Init,
  Light_Init, HidWriteBuff, SetNonblocking, HidReadBuff. At 0x7ab239..0x7ab269,
  the exact 35-byte name at 0x90b67f is
  `CWitmodHid_GetHidAndFirmwareInfoStu`, stored in procedure-table field +0x18.
- FirmwareInfo 0x7ac52e..0x7ac54b invokes that +0x18 procedure with three
  arguments through the dynamic call wrapper. On success it converts a string
  from result+0x180 (0x7ac5c4..0x7ac5d5), then calls parseVersion 0x7ac2d0.
  Therefore this Go wrapper does not itself expose a new firmware-download URL
  or prove a new wire identity request. Next trace the SDK implementation.
- SendData 0x7aa6de..0x7aa731 builds a zero-filled 64-byte buffer and copies at
  most 64 caller-supplied bytes; 0x7aa797 calls sendData64. Do not interpret this
  generic method or watch reader loop as the mask-construction implementation.

Exact ordinary NA87/MU68 firmware remains absent. No new update endpoint was
established; no already-empty API was queried again. Pro work remains at the
previous handoff state, with its transport, maps and normal-input audit open.

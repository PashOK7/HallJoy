# AULA HERO84 HE (`AULA_2829`) factory key map

Source: official legacy HERO WebHID asset
`app-B6-MiDbc.js`, SHA-256
`16030C913FE6F6A4B0326A346EC9920A6A33BA16FE569BFCB4C5B6E9CE6BBAAB`.
The map below is the first `default_key_layer` / `WIN` layout attached to
`AULA_2829`; it contains 83 physical keys.  `position` is the two-byte ID to
use in a `94 02` request.  `assignment` is the factory HID meaning only.

This is not a permission to assume the live device is unremapped.  Production
admission must prove the Hero84 UUID and query read-only `83` before using a
non-WASD/default key.  The safe initial diagnostic may request only the known
movement positions `W=30`, `A=43`, `S=44`, `D=45`, and `Space=70`.

| Position | Factory key | HID assignment |
|---:|---|---|
| 1..13 | Esc, F1..F12 | `29`, `3A..45` |
| 14..27 | `~`, 1..0, `-`, `=`, Backspace | `35`, `1E..27`, `2D`, `2E`, `2A` |
| 28..41 | Tab, Q..P, `[`, `]`, `\\` | `2B`, `14,1A,08,15,17,1C,18,0C,12,13`, `2F,30,31` |
| 42..52 | Caps, A..L, `;` | `39`, `04,16,07,09,0A,0B,0D,0E,0F`, `33` |
| 54 | Enter | `28` |
| 55..65 | LShift, Z..M, `,`, `.`, `/` | modifier `02`, `1D,1B,06,19,05,11,10`, `36,37,38` |
| 66 | RShift | modifier `20` |
| 67..73 | LCtrl, LWin, LAlt, Space, RAlt, Fn1, RCtrl | modifiers `01,08,04`, `2C`, modifiers `40`, internal `0D000000`, modifier `10` |
| 74..77 | Up, Down, Left, Right | `52,51,50,4F` |
| 95 | Print Screen | `46` |
| 98..103 | Insert, Delete, Home, End, Page Up, Page Down | `49,4C,4A,4D,4B,4E` |

The gaps are intentional: physical position IDs are not a contiguous 1..83
sequence.  The direct `94 02` path accepts the mapped physical IDs, not HID
usage values.

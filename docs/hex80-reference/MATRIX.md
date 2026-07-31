# Hex80 Matrix Map

This document maps the polled matrix slots to keys.

Assumptions:

- columns per row: `17`
- visible grid: `6 rows x 17 columns`
- total polled slots: `104`
- slots `102` and `103` are currently treated as trailing unused slots

Notes:

- `0x409` is a vendor-specific Fn code, not a standard HID usage
- `0x65` is the standard HID context-menu usage
- blank slots are currently treated as unused

## Row 0

| Col | Index | HID | Key |
| --- | --- | --- | --- |
| 0 | 0 | `0x29` | Escape |
| 1 | 1 | `0x3A` | F1 |
| 2 | 2 | `0x3B` | F2 |
| 3 | 3 | `0x3C` | F3 |
| 4 | 4 | `0x3D` | F4 |
| 5 | 5 | `0x3E` | F5 |
| 6 | 6 | `0x3F` | F6 |
| 7 | 7 | `0x40` | F7 |
| 8 | 8 | `0x41` | F8 |
| 9 | 9 | `0x42` | F9 |
| 10 | 10 | `0x43` | F10 |
| 11 | 11 | `0x44` | F11 |
| 12 | 12 | `0x45` | F12 |
| 13 | 13 | `0x46` | Print Screen |
| 14 | 14 | `0x47` | Scroll Lock |
| 15 | 15 | `0x48` | Pause |
| 16 | 16 | `-` | Unused |

## Row 1

| Col | Index | HID | Key |
| --- | --- | --- | --- |
| 0 | 17 | `0x35` | Backquote |
| 1 | 18 | `0x1E` | 1 |
| 2 | 19 | `0x1F` | 2 |
| 3 | 20 | `0x20` | 3 |
| 4 | 21 | `0x21` | 4 |
| 5 | 22 | `0x22` | 5 |
| 6 | 23 | `0x23` | 6 |
| 7 | 24 | `0x24` | 7 |
| 8 | 25 | `0x25` | 8 |
| 9 | 26 | `0x26` | 9 |
| 10 | 27 | `0x27` | 0 |
| 11 | 28 | `0x2D` | Minus |
| 12 | 29 | `0x2E` | Equals |
| 13 | 30 | `0x2A` | Backspace |
| 14 | 31 | `0x49` | Insert |
| 15 | 32 | `-` | Unused |
| 16 | 33 | `-` | Unused |

## Row 2

| Col | Index | HID | Key |
| --- | --- | --- | --- |
| 0 | 34 | `0x2B` | Tab |
| 1 | 35 | `0x14` | Q |
| 2 | 36 | `0x1A` | W |
| 3 | 37 | `0x08` | E |
| 4 | 38 | `0x15` | R |
| 5 | 39 | `0x17` | T |
| 6 | 40 | `0x1C` | Y |
| 7 | 41 | `0x18` | U |
| 8 | 42 | `0x0C` | I |
| 9 | 43 | `0x12` | O |
| 10 | 44 | `0x13` | P |
| 11 | 45 | `0x2F` | Left Bracket |
| 12 | 46 | `0x30` | Right Bracket |
| 13 | 47 | `0x31` | Backslash |
| 14 | 48 | `0x4B` | Page Up |
| 15 | 49 | `-` | Unused |
| 16 | 50 | `-` | Unused |

## Row 3

| Col | Index | HID | Key |
| --- | --- | --- | --- |
| 0 | 51 | `0x39` | Caps Lock |
| 1 | 52 | `0x04` | A |
| 2 | 53 | `0x16` | S |
| 3 | 54 | `0x07` | D |
| 4 | 55 | `0x09` | F |
| 5 | 56 | `0x0A` | G |
| 6 | 57 | `0x0B` | H |
| 7 | 58 | `0x0D` | J |
| 8 | 59 | `0x0E` | K |
| 9 | 60 | `0x0F` | L |
| 10 | 61 | `0x33` | Semicolon |
| 11 | 62 | `0x34` | Quote |
| 12 | 63 | `-` | Unused |
| 13 | 64 | `0x28` | Enter |
| 14 | 65 | `0x4E` | Page Down |
| 15 | 66 | `-` | Unused |
| 16 | 67 | `-` | Unused |

## Row 4

| Col | Index | HID | Key |
| --- | --- | --- | --- |
| 0 | 68 | `0xE1` | Left Shift |
| 1 | 69 | `0x1D` | Z |
| 2 | 70 | `0x1B` | X |
| 3 | 71 | `0x06` | C |
| 4 | 72 | `0x19` | V |
| 5 | 73 | `0x05` | B |
| 6 | 74 | `0x11` | N |
| 7 | 75 | `0x10` | M |
| 8 | 76 | `0x36` | Comma |
| 9 | 77 | `0x37` | Period |
| 10 | 78 | `0x38` | Slash |
| 11 | 79 | `-` | Unused |
| 12 | 80 | `0xE5` | Right Shift |
| 13 | 81 | `-` | Unused |
| 14 | 82 | `0x52` | Arrow Up |
| 15 | 83 | `-` | Unused |
| 16 | 84 | `-` | Unused |

## Row 5

| Col | Index | HID | Key |
| --- | --- | --- | --- |
| 0 | 85 | `0xE0` | Left Ctrl |
| 1 | 86 | `0xE3` | Left Meta |
| 2 | 87 | `0xE2` | Left Alt |
| 3 | 88 | `-` | Unused |
| 4 | 89 | `-` | Unused |
| 5 | 90 | `0x2C` | Space |
| 6 | 91 | `-` | Unused |
| 7 | 92 | `-` | Unused |
| 8 | 93 | `-` | Unused |
| 9 | 94 | `0xE6` | Right Alt |
| 10 | 95 | `0x409` | Fn |
| 11 | 96 | `0x65` | Context Menu |
| 12 | 97 | `-` | Unused |
| 13 | 98 | `0x50` | Arrow Left |
| 14 | 99 | `0x51` | Arrow Down |
| 15 | 100 | `0x4F` | Arrow Right |
| 16 | 101 | `-` | Unused |

## Trailing Slots

| Slot | HID | Key |
| --- | --- | --- |
| 102 | `-` | Unused |
| 103 | `-` | Unused |

## Confirmed Indices From Live Testing

- `36 = W`
- `52 = A`
- `53 = S`
- `54 = D`
- `90 = Space`

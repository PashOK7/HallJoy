# IO Type 84 Magnetic firmware source

Downloaded 2026-08-28 from the update manifest served by the vendor web
driver, `https://web.io.vision/update.json`.

The manifest identifies both images as product ID `32982`, version `1.17`:

| Keyboard variant | File | Size | SHA-256 |
| --- | --- | ---: | --- |
| Black | `IO_Type_84_Magnetic_Black_V1.17.hex` | 255,277 bytes | `4F2E8B8B406A72ED4A4D4B34502478AA9B8B5D874C4A3CDFA1381ACAEA00EC44` |
| White | `IO_Type_84_Magnetic_White_V1.17.hex` | 255,277 bytes | `8F5EF507771C6795258EB7521CFC1B46269A5B301548767AE87A3F1A83A50D45` |

Direct source URLs:

- `https://web.io.vision/IO_Type_84_Magnetic_Black_V1.17.hex`
- `https://web.io.vision/IO_Type_84_Magnetic_White_V1.17.hex`

Both downloads are Intel HEX text files. They have equal length, but differ
in nine bytes; do not substitute one colour variant for the other when
flashing a physical keyboard.

The web driver implements an OTA protocol (commands `0x80`--`0x85`) and
identifies Type 84 Magnetic as product ID `32982`. This confirms the files
are firmware images for the magnetic Type 84, rather than configuration
exports.

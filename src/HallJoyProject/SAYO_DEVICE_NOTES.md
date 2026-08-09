# SayoDevice OSU O3C Backend Notes

Date: 2026-05-14

Source captures:

- `<capture-directory>\sayodevice.pcapng`
- `<capture-directory>\sayo2.pcapng`
- `<capture-directory>\sayo4.pcapng`
- `<capture-directory>\sayo5.pcapng`

Observed device:

- VID `0x8089`
- PID `0x0009`

Current implementation:

- Adds a native HID reader for SayoDevice OSU O3C.
- Reads `0x21` input reports from the device.
- Sends the observed `0x22 12 3C 13 05 00 15 01` polling command on writable 1024-byte HID interfaces.
- Reads `0x22` depth responses from the polling stream.
- Uses observed `0x21` edge reports:
  - byte `8` = `0x10` for down
  - byte `8` = `0x11` for up
  - byte `9` = physical button index
- Uses observed `0x22` response depth values:
  - button index `0` raw depth at u16 offset `8`
  - button index `1` raw depth at u16 offset `10`
  - button index `2` raw depth at u16 offset `12`
  - raw full press is approximately `4000`
- Maps the physical button index to the user's configured keyboard HID usage by matching the nearest normal keyboard HID report.
- Falls back to captured O3C defaults only until a live keyboard report remaps the index:
  - index `0` -> HID `0x09`
  - index `1` -> HID `0x0A`
  - index `2` -> HID `0x0B`

Limitations:

- The analog depth parser depends on the device/web-driver polling stream being active.
- HallJoy now tries to start that stream itself; if the HID interface is not writable, it can only use external polling from the web driver.
- The `4000` raw full-scale normalization is based on captures and may need calibration on other SayoDevice firmware/configs.

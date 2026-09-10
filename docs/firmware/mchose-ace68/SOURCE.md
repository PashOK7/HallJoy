# MCHOSE Ace 68 firmware retrieval record

Date checked: 2026-08-28 (Europe/Moscow).

## Exact compatible variant

The official M HUB web-driver configuration identifies the original wired
`Ace 68 I` as:

- USB vendor/product ID: `VID_41E4` / `PID_2114`;
- current configured firmware version: `109`;
- bootloader ID: `VID_41E4` / `PID_2115`.

Its exact official firmware asset is:

`https://cdn.mchose.com.cn/configCenter/static/binaries/update_ace68.75eed895_961f420a59da.bin`

Do not flash this file to Ace 68 Pro, Ace 68 Air, Ace 68 Turbo, Ace 68 GT, or
Ace 68 variants with a different USB ID.

## Provenance and retrieval result

- The URL and compatibility metadata were read from the live official M HUB
  configuration at `https://www.mchose.com.cn/#/home`.
- The official MCHOSE support page confirms that Ace 68 uses M HUB and that
  firmware updates are delivered from its firmware-update screen:
  `https://support.mchose.store/hc/en-us/articles/51066604806420-Ace-68-Ace-68-Air-M-HUB-Driver-Guide-Setup-Firmware-Lighting-Onboard-Profiles`.
- The official MCHOSE store page links to the desktop M HUB installer. Its
  downloaded ZIP SHA-256 was
  `6B851DD540C9B8283D0D1F41ABB1DAE88D39FD9CEE05255EA3CFCB6D772BA839`.
  The installer was inspected without running it; it does not bundle this
  firmware, and requests the same CDN asset.
- On this network the CDN reset the TLS connection before returning any bytes
  of the `.bin`. No firmware bytes were saved, and no alternative variant was
  substituted.

When the keyboard is available, connect it directly over USB and use M HUB in
Chrome or Edge. It will identify the USB ID and present only the compatible
update.

## Retry, 2026-08-28

- A subsequent byte-range request briefly received the official OSS response
  (`206 Partial Content`, ETag `17F1E74040C78CD5A426961F420A59DA`), which
  confirms the asset remains present, but full requests again timed out during
  the TLS connection phase before any file was created.
- Windows BITS could not run because the operating system paused it for an
  active Game Mode receiver owned by another process. That process was not
  changed. An isolated temporary Edge profile also produced no download file.
- Result remains unchanged: no firmware bytes have been saved locally and no
  firmware for another hardware revision has been substituted.

## Local file verification, 2026-08-28

The user-provided file
`C:\Users\PC\Downloads\update_ace68.75eed895_961f420a59da (1).bin` was read
without modifying it. It is the exact official asset described above:

- size: `164352` bytes;
- MD5: `17F1E74040C78CD5A426961F420A59DA` (matches the official CDN ETag);
- SHA-256: `05C1FB7C3646319FE5E87F470790FE04689BB72D444E8086423F392E8E52A215`.

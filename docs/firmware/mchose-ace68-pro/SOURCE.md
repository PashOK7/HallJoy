# MCHOSE Ace 68 Pro firmware retrieval record

Retrieved 2026-08-29 from the official M HUB catalogue entry for USB application
identity `VID_41E4 / PID_2116` (M HUB storage key: `Ace 68 Pro`).

| Field | Value |
| --- | --- |
| Official image | `update_ace68pro.4ed9aa6d_562fcc8c87ad.bin` |
| Direct source | `https://cdn.mchose.com.cn/configCenter/static/binaries/update_ace68pro.4ed9aa6d_562fcc8c87ad.bin` |
| Size | 143,616 bytes |
| SHA-256 | `105E3C69E2F0DF779EB66FB345949530829241F0F1F27125BD47C830FE2621E9` |
| MD5 / server ETag | `9C21309D9290460F2C64562FCC8C87AD` |
| Boot identity | `VID_41E4 / PID_2117` |

The image's USB device descriptor independently embeds application identity
`41E4:2116` and product string `Ace68-II`. It is not the previously analysed
Ace 68 I (`41E4:2114`) binary.

The returned `HallJoyStabilityTrace (7).log` is byte-for-byte identical to
`HallJoyStabilityTrace (6).log` (SHA-256
`4C41F02CD6B0819B4B794DC19673ADCD05531B64026FC0C00735A811BBCC1E13`),
so it contributes no new device observation.

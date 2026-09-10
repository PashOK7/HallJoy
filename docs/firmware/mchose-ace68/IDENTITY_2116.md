# MCHOSE `41E4:2116` identity correction

Confirmed 2026-08-29 from the official M HUB device catalogue and the returned
Windows Raw Input trace.

| USB application ID | Official M HUB storage key | Official update image | Boot ID |
| --- | --- | --- | --- |
| `41E4:2114` | Ace 68 I | `update_ace68.75eed895_961f420a59da.bin` | `41E4:2115` |
| `41E4:2116` | Ace 68 Pro | `update_ace68pro.4ed9aa6d_562fcc8c87ad.bin` | `41E4:2117` |

M HUB displays the user-facing `name` and `fullName` as `Ace 68` for both
rows, so a report of "MCHOSE Ace 68" without the suffix is ambiguous. The
returned trace records `41E4:2116` and therefore identifies the physical
device as the Ace 68 Pro row, not the Ace 68 I image previously analysed.

The Ace 68 I diagnostic must remain restricted to `41E4:2114`; widening it to
the adjacent PID would silently apply an unproven firmware protocol to a
different product. A Pro-specific diagnostic can be built only after analysing
the Pro image and its exact M HUB command path.

Official catalogue asset:
`https://www.mchose.com.cn/cizhou/CZ_SHARED_DATA/main.ebb3c142bf46475ea1b0.js`

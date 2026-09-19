# AULA MINI60 HE Pro: tester log and firmware review

Date: 2026-09-17. Scope: log inspection, official updater acquisition, static
protocol analysis and bounded firmware component execution. No HID access,
flashing, backend changes, or HallJoy.exe rebuild.

## Tester evidence

Input: `C:/Users/PC/Downloads/HallJoy (12).log`, 5903 bytes, SHA-256
`0c017e81f9150bc022e7d24ea1aee86429b12752226b31f81dbf162f090ec304`.
Do not republish unrelated device inventory as keyboard identity evidence.
The owner forwarded the tester's explicit identification:

- `0C45:80A2`: wired keyboard.
- `0C45:FEFE`: 2.4 GHz receiver.

Both appear in the log. The final discovery snapshot has `search_complete=1`,
`sdk_initialised=1`, `analogue_connected=0`, `devices=0`. UAP is available/ready
with zero reported transport errors, restarts and invalid snapshots. The report
contains no analog samples, command replies, manufacturer/product strings or
firmware version. It does not prove an analog protocol is absent or broken.
The recorded support-report window is near uptime 171.562..175.187 seconds,
not a complete trace of the entire application session. Shutdown is recorded.
No MINI60 VID/PID allowlist match was found in current native HallJoy sources.

## Source chain and exact updater

Official AULA driver page links MINI60 HE to `https://hec.aulacn.com/`:
`https://www.aulacn.com/index/index/driver.html`.
The legacy and HUB frontend requests returned HTTP 403 from this environment.
For discovery, the official client bundles preserved in Aether-HE were read at
pinned commit `2179fd768fcdbb86f59c4c240b4d4cfcbc18936f`, directory
`docs/context/issue-6-mini60-pro-raw/driver_src/`. These are archived client
sources, not a claim that today's frontend is identical. Repository instructions
were not executed or adopted.

The archived registry maps wired `0C45:80A2` to MINI 60 HE PRO, usage
`FF68:0061`; its receiver entry uses `0C45:FEFE`, `FF60:0061` plus product-name
matching. FEFE is shared and must not become an unconditional model allowlist.
The version page maps PID 32930 to product name `MINI 60 HE PRO`.

The current official API was then queried directly:
`https://hubapi.aulacn.com/user/EXE/getFile/MINI%2060%20HE%20PRO`.
It returned V1.55 and `MINI_60_HE_PRO_YAN_0.01_V1.55.exe`.
Download: `https://app.aulacn.com/commonAssets/MINI_60_HE_PRO_YAN_0.01_V1.55.exe`.
The executable was downloaded and parsed as data, never launched.

| Artifact in existing `.local` directory | Bytes | SHA-256 |
| --- | ---: | --- |
| `MINI_60_HE_PRO_YAN_0.01_V1.55.exe` | 2452384 | `0aa101156eb18ed9c1b9018ef7c9bffa7710456350b7f24e71db70d9bf6df8b1` |
| `aula-mini60-pro-155-resource-4000.bin` | 516096 | `c070e514ff1bef20a71abff12a6c30b04f152892b0fa22e1b5e0709be63eb7cd` |
| `aula-mini60-pro-155-resource-4011.bin` (ZIP) | 110048 | `f0e57d9b7fd3972f882194d7cce0f6cbaed69145b54a43ceae880e686a2f26fe` |
| `aula-mini60-pro-155.hex` (ZIP member) | 252331 | `600e15fe4692510dbc86a7920cb34cbae40c622a75c229e8a9fb7166dd7f56f4` |

PE metadata names Sonix; resources 4008/4009 encode `0C45`/`80A2`, 4010 names
`SN34F280`, 4013 carries build time `2026/05/28 17:03`. Resource 4000 has an ARM
Thumb vector table (SP `0x20007570`, reset `0x205`), the product string MINI 60 HE
PRO and matching VID/PID bytes at `0x9004`. Updater code at `0x427274..0x42729A`
loads RCDATA 4000 using its resource APIs.

Caution: ZIP resource 4011 contains another `SN34F280_User.hex` image. It differs
at 75018 mapped addresses from resource 4000. These are not interchangeable
representations; this review and its tests use **resource 4000 only**. The
bundled HEX's revision and role have not been established. The tester's installed
firmware version is also unknown; V1.55 is the current API-labelled package.

## Protocol similarities and important difference from IO

MINI60's client uses the HFD SDK. Wired requests begin `AA`, replies `55`,
with command, length, 16-bit offset and payload at byte 8 in a 64-byte frame.
Firmware dispatcher `0xE344` implements the `64/65` calibration and `66/67`
simulation pairs. Separate flags are `0x20000474` and `0x20000475`.
`60/68` have no live measurement case in that branch; acknowledgement alone
must not be interpreted as a snapshot.

Serializer `0xBF8` builds a 64-byte buffer at `0x20003352` with a 14-byte prefix:
`55 FB key status maxLE16 minLE16 adcLE16 travelLE16 maxStrokeLE16`.
This is the same report structure as IO Type 84/68 Magnetic. It is a different
family from the already implemented AULA HERO84 `09/94/02`, WIN60 Standard
W669, and WIN60 MAX SparkPlayJoy protocols. Brand or common HID usage does not
justify routing this keyboard through those backends.

Unlike the reviewed IO Type84 implementation, MINI60's reporting tail does not
select the deepest key. With calibration off, `0x2D82..0x2DB2` reads the current
position's ADC, tests simulation, serializes that position and calls USB send
`0x111B8` immediately. No selected-key comparison appears in that tail.

However, sending is conditional on `currentADC <= releasedEndpoint - 12`.
Those 12 units are sensor units, not millimetres. Values close to rest are not
sent by this path. USB send may return busy (2); this caller ignores that result
and does not retry within the same invocation. Subsequent scans may retry by
running the path again, but fairness, sustained delivery and latency remain
unverified. At `0x2E06`, calibration off can continue to ordinary processing
`0x521C` even when simulation is enabled. End-to-end typing is not yet tested.

A second dispatcher at `0xF524` accepts a 34-byte internal frame with prefix
`81`, sum check over the first 33 bytes and another `64..67` branch controlling
the same flags. This suggests a separate transport path, consistent with a
wireless design, but neither receiver firmware nor analog forwarding through
2.4 GHz was established. Do not send wired frames blindly to FEFE.

## Executable checks and limits

Run `python tools/review_aula_mini60pro_firmware.py` from the workspace root.
It pins resource 4000 by SHA-256 and uses Unicorn from the existing local
reverse-analysis dependencies. [13 checks PASS](AULA_MINI60_HE_PRO_2026-09-17_RESULTS.json).
[Annotated instructions](AULA_MINI60_HE_PRO_2026-09-17_DISASSEMBLY.txt).

The actual wired dispatcher toggles simulation without setting calibration.
Reporting-tail tests supply already-computed travel, endpoints and register
state; the actual serializer and branch instructions execute:

| Report key | ADC | Supplied travel | Send attempts |
| --- | ---: | ---: | ---: |
| 7 | 2050 | 170 | 1 |
| 16 | 2100 | 93 | 1 |
| 7 | 2150 | 43 | 1 |
| 7 | 2188 | 1 | 1 |
| 7 | 2189 | 0 | 0 |
| 7 | 2200 | 0 | 0 |

The sequence proves independent serialization in this reporting component,
including changing the shallower key. It does not prove the complete scanner,
physical travel calculation, actual USB throughput, key mapping, or wire-level
release behavior. USB send is stubbed; ordinary processing is checked only at
entry. An exploratory whole-scanner run with uninitialized synthetic history
hit the instruction budget in the interpolation loop; it is not counted as a
passing scanner test or reported as a physical firmware defect.

## Integration assessment

This is a credible multi-key analog candidate, stronger than the selected-key
IO path. It is not ready to be presented as working HallJoy support yet.
Next work should establish the exact key map and scale, behavior at release,
USB busy/fairness and a reliable completion mechanism for neutral values.
A hardware diagnostic should first use the confirmed wired 80A2 interface,
read firmware identity, compare actual packets with the pinned image and check
simultaneous holds/releases plus normal typing. Receiver support is separate.
No conclusion of full support, or of impossibility, follows from this log alone.

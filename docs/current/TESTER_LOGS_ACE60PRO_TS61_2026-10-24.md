# Tester logs: MCHOSE Ace 60 Pro and FuryCube TS61 — 2026-10-24

Two user logs, both HallJoy 1.6.11.0. Read as data. Ace 60 Pro fixes implemented the same day (owner); see the last section.

## "HallJoy (17).log" — MCHOSE Ace 60 Pro

- USB `41E4:2103`, product string "Ace 60 Pro", interfaces 00/01/02. Exactly one collection matches the Jet75
  backend filter (usage page 1, usage 0, 65/65 bytes; `jet75.hid_caps value=0x0001000000410041`).
- `jet75.model value=8451` (0x2103) and `jet75.firmware_version value=280` (0x0118) are logged 23 times, every
  ~5 s for ~110 s. No `jet75.firmware_unreviewed`: the firmware equals the reviewed Ace 60 Pro 1.18. So the info
  exchange (opcode 03) works and the reply is valid.
- After that the session ends without any event: no `jet75.admission_failed`, `jet75.profile_format`,
  `jet75.auto_enable_result`, `jet75.mode_enabled` or any `jet75.*_failed`. Exchange failures always log, so the
  exit is one of the silent returns in `Run()` (mchose_jet75_backend.cpp): (a) routing claim refused
  (`NativeAnalogRouting_Claim` false and not already ours), or (b) `LoadMode()` false via `tp::Profile(base)`
  rejecting the 6-byte profile base (opcode 04, flash 0x20200: `{current, count, table}`) or the second base read
  differing from the first. No other backend in the source claims 41E4:2103, so (b) is the likely branch; the log
  cannot tell which. A plausible cause is a base area never written by M HUB (e.g. erased bytes), which this
  firmware would treat with RAM defaults at boot while opcode 04 still returns the flash bytes; unproven.
- Result: `mchose_jet75 present_not_connected`, no analog; the user's keys worked only as ordinary keys
  (64 `trace.os_key`, nothing bound).
- Logging gap (against the "useful logs" rule): the silent exits must log the real data. Proposed: an event for a
  refused claim, and for a rejected base with its 6 bytes and both reads, plus the profile's AA BB magic state, so
  the next log decides between a HallJoy fix and a firmware limitation. Not done yet.

## "message (14).txt" — FuryCube TS61

- USB `0603:0361`, product string "RFG61", interfaces 00/01/02; also a 2.4G receiver `3554:F5D5`. VID 0603 is
  not used by any HallJoy protocol, so no backend probed it (no events for it at all); no analog source.
- FuryCube's TS61 product page (https://www.furycube.com/products/furycube-ts61-gaming-keyboards) calls it an
  "Esports Mechanical Keyboard" with "Neuron X" switches, but also a sensor that "converts real-time key position
  into signals", so it is probably analog. The FuryCube drivers page lists no TS61 entry (HE drivers exist only
  for M30HE, M68HE, M35HE, M68HE v2, M68HE XS, all on the BY/IPI platform `372E`). TS61 is a different platform.
- Not in the Sheet. Without the vendor configurator (or a firmware image) the protocol cannot be identified.
  Next step: ask the tester which software configures the TS61 (box/manual link, web driver URL).

## Ace 60 Pro: fixes and diagnostics (owner, 2026-10-24)

Owner: fix every possible failure point now and deliver an EXE that both logs in detail and already tries to fix it.

**Firmware finding.** Boot code of every image in `.local/research/mchose-mhub-20261002/dis/` was compared at the
profile-base read (flash 0x20200). Three rules exist; HallJoy's old check (`count` 1..4, `current<count`,
`table[current]<4`, else silent exit) matched none of them exactly:

| Rule | Models (image) | Active profile at boot |
|---|---|---|
| Counted | Jet 75 II 1.16, Zero75X 1.14 (6-byte base) | `current<count ? current : 0`; entry >3 -> 0 |
| Bounded | Ace 60 Pro 1.18, Ace 60 Pro Nordic 1.07, Ace 60X II 1.04, Ace 68 Air II 1.17 (8-byte base) | count ignored; `current>3 -> 0`; entry >3 -> 0 |
| BoundedUnchecked | Ace68-II 1.21, Jet 75 I 1.09, Ace68-I 1.09, Ace 60X I 1.05, Mix 87 I 1.12 | count ignored; `current>3 -> 0`; entry used unchecked |

Ace 60 Pro (0x8744): reads 8 bytes, `current>3 -> current=0`, `table[current]>3 -> profile 0`; `count` is used only
by the Fn profile cycle (`(current+1) % count`). A base with count 0, count >4 or current >= count is therefore
normal for the keyboard but was rejected by HallJoy without a log line: the likely cause of the tester's log.

**Changes (mchose_jet75_protocol.h / mchose_jet75_backend.cpp).**
- `tp::Profile(base, profile, BaseRule)` resolves the base exactly like the model's firmware; `Model::baseRule` per
  row; `ChangeFlag` uses the same rule. Every base the old check accepted resolves identically under all rules
  (tested exhaustively). Unresolvable cases (Counted with current >=4 below count; BoundedUnchecked with entry >3)
  are still refused and logged.
- Logged with real data on every exit: `jet75.profile_base` (6 base bytes, resolved profile or 0xff),
  `jet75.profile_base_unusable` (bytes + rule), `jet75.profile_base_changed` (both reads; the base is re-read up to
  3 times until two reads agree, recovery for a late profile save), `jet75.load_failed` (attempt + error),
  `jet75.profile_format` now carries the profile's first 8 settings bytes, `jet75.claim_refused` (pid + whether
  another protocol holds the path).
- Stream: an A0 sample of a key outside the model's reviewed set no longer ends the session (which also disabled
  analog until pause because auto-enable runs once per generation): it is ignored and logged once per key
  (`jet75.unknown_key`, hid + maximum + depth). Real faults (`jet75.invalid_analog`) now carry hid/maximum/depth.
- Several matching collections: were never run and never logged (unreachable branch); now each is tried in turn,
  `jet75.multiple_devices` logged on change.
- `jet75.hid_caps` now logged only when the collection set changes (was 7 lines every 5 s, which pushes useful
  events out of the 512-line history), with the PID, plus `jet75.control_collections`.
- Tests: protocol (all three rules, exhaustive equivalence for valid bases, Ace 60 Pro flag transaction on a count-0
  base) and session (Ace 60 Pro with base {1,0,0,2,..} enables profile 2 and streams; unresolvable base never
  written; unknown key ignored, session continues). Backups: `.local/backups/ace60pro-jet75-20261024/`.

Not hardware-tested. If the tester's base is valid after all, the next log shows exactly which step stops
(`profile_base`, `profile_format`, `claim_refused`, `auto_enable_result`, `unknown_key`/`invalid_analog`).

## "HallJoy (18).log" (2026-10-24)

Byte-identical to "HallJoy (17).log" (same MD5 8d3e1d03...): the same 1.6.11 run, no new data. The fixes above
need the tester to run the new EXE; the next log will show `jet75.profile_base` and either analog or the exact
failing step. Native checks after the fixes: `run_native_backend_checks.py --require-compiler` EXIT=0, no
assertion failures (Jet75 protocol + lifecycle PASS, including the Ace 60 Pro base rule and the X82HE `Known()`
test). Log comment fixed: `jet75.hid_caps` carries only the PID low byte at bit 56.
Tester EXE: `build_release.ps1` EXIT=0, SHA256 `53dcabd864e5ec7421cb15cb636de225442353097a947b14fac7c96115184945` (local, not released; also makes HE65 V2 Supported).

## "HallJoy (21).log" — Ace 60 Pro works: Supported (2026-10-24)

Same tester, HallJoy 1.6.12.0 (tester build with the fixes above). USB `41E4:2103` "Ace 60 Pro".

- `jet75.model 8451`, `jet75.firmware_version 280` (1.18), `jet75.profile_base` logged and resolved,
  `jet75.auto_enable_result 0`, `jet75.mode_enabled 1`, `jet75.profile 0`. No `claim_refused`,
  `profile_base_unusable`, `unknown_key` or `invalid_analog`. The silent exit of log (17) is gone.
- Native row `mchose_jet75` `connected`, 60 mapped keys, 796 updates, `failures=0`; the stream is change-driven,
  so updates stop while no key moves (age grows). The snapshot field analog_error=-1996 comes from the Wooting SDK
  path (sdk_sources=0); the native source is unaffected.
- Gamepad: W/A/S/D (HID 1A/04/16/07) bound to the left stick. `trace.pad` follows the key depth smoothly
  (W 62 -> 1000 -> 460 gives ly 0 -> 32767 -> 13697; D 334 -> 551 gives lx 9155 -> 16977).
- Result (owner: "can be moved to Supported"; criteria met: working analog, built-in layout "MCHOSE Ace 60 ANSI"):
  Ace 60 Pro -> Supported. MchoseFamily confirmed_devices token 0x9B4588C88D53AF15 pid 8451 (yellow notice off only
  for 41E4:2103; Ace 60X I/II and Ace 60 Pro Nordic share the token and keep the notice), model removed from the
  yellow list, `supported_layouts.json` (shownAs "Ace 60", the picker name), README, SUPPORTED_HARDWARE,
  RELEASE_NOTES_NEXT. Sheet Main!C532 -> "Supported", B532:C532 green; readback A531:C533: row 532 changed,
  531/533 unchanged, ONE_OF_LIST kept (same batch as X82 HE, see TESTER_LOGS_HE65V2_X82HE_2026-10-24.md).
Build and checks: see the last paragraph of TESTER_LOGS_HE65V2_X82HE_2026-10-24.md. EXE SHA256 `4cad3e1ab06f5604696e3b3b39dcb470e667e0460d3c2d94d786e7cd0d9c5e74` (local, not released).

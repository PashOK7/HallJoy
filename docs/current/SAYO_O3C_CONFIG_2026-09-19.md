# SayoDevice O3C layout and configuration reader

Date: 2026-09-19. Owner requested the remaining O3C layout with actual binding reads.
Hardware validation of the new configuration path is pending. Existing O3C depth
support had tester confirmation; that does not validate this new map reader.

## Evidence

Official firmware: https://a.sayobot.cn/firmware/update/9/firmware/app_O3C.bin
Encrypted SHA256: d81a3e001a2b5f13fcaabfe6a8e357ecaedc14a36ae21cce4f9dc6aed863068f.
Cached source/derived image, ELF and LLVM 21.1.8 +c,+m,+xwchc disassembly:
docs/research/sayo-config-sources. No firmware was flashed or vendor executable run.

The public O3C protocol notes and official SayoGroup streaming implementation
corroborate API-v2 framing. An older O2 config viewer and a Pulsar XPAD Mini
implementation were research leads only, not proof of O3C action semantics.

- https://gist.github.com/khang06/6186543b560548370ce7cc08cad7f710
- https://github.com/SayoGroup/SayoDeviceStreamingAssistant
- https://github.com/Sayobot/SayoDevice_manual

Exact O3C command dispatcher at 0x908E, command table at 0x1D480.
Command 0x10 handler 0x9628 takes an empty payload (command length 4) and an
index in the command header. It returns 56 payload bytes. It writes settings
only on the separate nonempty-payload branch, which HallJoy never requests.

Payload geometry: class byte 0, x/y at 4/6, width/height at 8/10.
Three magnetic keys: x=1000/3000/5000, y=3000, width=height=1800.
Five action records begin at 16 with stride 8: mode at 0, four action bytes at
4..7. The base default keyboard action has mode 0, modifier at 20, usage at 21.
Factory assignments from 0x1C778 are Z/X/C. F/G/H in the earlier backend were
capture-specific, not factory defaults. Encoder positions have no depth channel.

Command 0x15 handler 0x9914, index 1 returns three independent u16 travel values.
The existing poll and 4000 raw full-scale are retained. Frame parsing now checks
report ID, checksum, complete length, command, index and client echo. Config
responses cannot enter the depth parser. Error/continuation frames are rejected.

## Owner correction: three physical keys are unconditional

The owner clarified that failed binding reads must never remove the O3C layout.
Automatic identification now requires the exact known VID/PID and a validated
three-channel travel response; a contradictory model reply rejects O3C identity.
An identity/config timeout leaves the three physical buttons available.
Each successfully understood binding supplies its display label. Unknown,
unassigned or complex actions use Key 1 / Key 2 / Key 3 individually. Partial
configuration failures do not discard successfully read labels on other keys.

Automatic buttons use stable physical channels 0x470..0x472, independent of
labels. Duplicate letters remain separately bindable; receiving labels on a
subsequent connection does not change the physical binding IDs. Known ordinary
HID aliases remain readable for existing profiles. Manual mode publishes the
factory Z/X/C usages. Automatic-mode input capture prefers physical channels.
Block Bound Keys translates known physical assignments to their real Windows HID;
unknown actions cannot be inferred or blocked by inventing a digital mapping.

Session maps now support optional display metadata separate from assigned input
identity. Other native models retain their existing behavior. UI tests cover
missing, partial and duplicate labels, unchanged refreshes and manual restoration.
Portable tests also cover alias blocking and active/manual isolation.

The earlier behavior described below is historical and superseded by this section.

## Original runtime behavior (superseded)

One writable FF12/usage 2/1024-byte depth collection is selected, avoiding mixed
reports from different interfaces/devices. Other physical supported devices
continue to trigger the existing multiple-device/manual layout policy.

At connect, the worker reads model identity (must be 9) and all three KeyInfo
records. Each transaction has at most two attempts and a bounded response wait.
Only a complete validated map publishes the O3C layout token. UI never performs
USB work. Read configuration once per session; reconnect to refresh edits made
in the vendor app. No periodic configuration polling or layout rebuild.

Automatic mode uses the read base-layer assignments. Manual mode uses the Z/X/C
factory preset. Default-mode single keys, one modifier alone and unassigned
buttons are understood. Chords, other action modes, scripts, active Fn layers
and unknown geometry are not flattened to guessed letters; automatic selection
falls back to manual. This conservative action coverage can be extended with
separate firmware evidence. The encoder is not presented as an analog key.

The old digital-edge/keyboard-report matcher and remaining digital depth
substitution were removed. Depth now comes only from verified travel responses.
Physical values remain separate; duplicate assigned usages aggregate by maximum,
so releasing one duplicate does not erase another held key. Like other ordinary
HID aliases, duplicate labels share a logical HallJoy binding, not independently
bindable physical channels. Stale depth returns zero after 160 ms; stop/disconnect
neutralizes values and removes the session map. Normal logging remains opt-in.

## Verification

- tools/review_sayo_o3c_firmware.py executes the actual read handlers in Unicorn,
  with LLVM-decoded WCH compressed byte/halfword instructions. Three positions,
  all five action records, independent travel values and zero releases: PASS.
  Synthetic RAM proves serializer behavior, not physical USB transport.
- sayo_o3c_test.cpp: frame corruption/truncation, wrong command/index/echo,
  geometry, letters/modifiers/unassigned, rejected complex actions, duplicate
  assignments, releases and automatic/manual remap state: PASS.
- Updated static audits reject digital inference/substitution and require the
  stronger protocol proof. Existing shutdown and exception boundaries preserved.
- Production-linked profile, automatic layout, picker and catalog tests: PASS.
  103 source / 84 visible variants; 16 recovery scenarios + repeated startup PASS.
- Owner evaluates appearance; no app visual test or physical O3C test performed.

Backups: .local/backups/before-o3c-config.zip,
.local/backups/o3c-integration-before.zip, layout-integrate-ylmpqkw0.
Evidence: .local/o3c-profile-tests.txt, o3c-static-checks.txt,
o3c-remaining-audits.txt, o3c-final-audits.txt. Initial static audit failures were
obsolete source-pattern assertions; updated to assert the new validated parser
and accept the additional saturating freshness read.

## Original delivery (superseded)

Ordinary Release x64 build and four candidate self-checks: PASS. Delivered through
tools/build_release.ps1. Evidence: .local/o3c-release-build.txt.
Path: build/bin/Release/x64/HallJoy.exe
Size: 9377280 bytes
SHA256: 97789328fe58045e57a397c716094b26ce7073e8c92625bef951fcd91ebe7403.
No GitHub publication requested or performed.

## Corrected delivery: always three keys

O3C parser/layout/alias tests, block-key policy, Wooting physical-channel
regressions, shutdown/exception audits, production UI/profile/catalog tests and
16 startup recovery scenarios: PASS. Release x64 and four candidate self-checks:
PASS. No physical O3C or visual app test performed.

Evidence: .local/o3c-three-keys-profile-tests.txt and
.local/o3c-three-keys-release-build.txt. Backup: o3c-always-three-before.zip.
The first standalone Wooting test invocation omitted its link dependencies;
rerunning with the dependencies from the standard test runner passed.

EXE: build/bin/Release/x64/HallJoy.exe
SHA256: dde02eb78de79fa2e7ea812649b112b22b52ddc9157907c3033e33efa1eddf13
Size: 9378304 bytes.

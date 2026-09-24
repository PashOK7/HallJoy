# keypad-gamepad reference review - 2026-09-22

Owner asked whether https://github.com/jackuson14/keypad-gamepad can help HallJoy.
Reviewed README and hid_protocol.py; current upstream HEAD: 26becddfde2888bc74a22e4440667f2c9b3f5686.
Read-only review: no imported code, hardware commands or support-status changes.

Useful concrete lead for MonsGeek/Akko input integration. Upstream reports actual
M1 V5 HE wired 3151:5030 and dongle 3151:5038 verification. This is upstream
hardware evidence, not a HallJoy test. Other-family compatibility remains a claim
to evaluate per model; current known-device list only names that model/transports.

Implementation: 65-byte feature command, report ID 0, command 0x1B with enable
parameter 1/disable 0; checksum byte 7 = 255 - sum(first 7 bytes) modulo 256.
Config usage FFFF/0002; separate vendor input, report 05, event 1B, little-endian
u16 depth then key index. Observed full travel about 720 is model-specific and
must not become a family-wide calibration constant. Reader re-enables once after
an idle gap for sleep/wake. README points to echtzeit-solutions/monsgeek-akko-linux
as protocol research source. Existing HallJoy research lists Akko/MonsGeek as not
investigated; no current support declaration changed by this reference discovery.

Do not copy generic admission based only on vendor usage signature: code picks
first vendor input and groups by PID; review exact device grouping, release and
disconnect neutralization, multi-key updates, scaling and layouts for HallJoy.
1000 Hz output setting is not evidence of 1000 fresh analog reports or latency.

Next possible task: examine upstream protocol research and implement a bounded
native HallJoy route for established devices, then reconcile support status.

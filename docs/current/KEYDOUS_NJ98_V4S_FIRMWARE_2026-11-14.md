# Keydous NJ98-CP V4S — exact firmware/cache investigation, 2026-11-14

Owner explicitly requests continued research of exact keyboard and receiver
firmware and a fresh full-depth matrix without sticky retained values.
Starting evidence: [revision6 log51](KEYDOUS_NJ98_V4S_LOG51_2026-11-14.md).
Preserve current ordinary EXE605d4556 and all keyboard/MIDI features until an
evidenced implementation change passes the ordinary delivery gates.

Exact identities: keyboard board3793 /3151:5030, receiver3151:5038, remotely
proved3793; manufacturer class ry5088_qn_nj98cpv4s_3m_8k_8k. Do not substitute
NJ98-CP V4 board3496 or an older HE model image. No flashing/calibration/reset
is part of this research. Vendor software is inspected statically and kept local.

Previous November5 acquisition did not obtain exact keyboard/receiver MCU bytes:
get_fw_version dev_id3793 returned Record not found; desktop package contained
host code but no established exact image. A fresh search must follow current
manufacturer download paths and actual update-bundle selection, not infer absence
from one old endpoint. Publicly reachable downloads do not establish redistribution
permission. Local acquisitions and provenance go under
`.local/research/keydous-nj98v4s-firmware-20261114/`.

## Acquisition result and exact-identity gap

The current [Chinese manufacturer download page](http://www.keydous.com/xz_keydous)
links Keydous_v4_setup_3.2.33_WIN2026119.exe, newer than the previously inspected
3.2.23 package. Downloaded111490792 bytes; SHA256
6a2db6373754f62dea772defc0dcd82b6ca07ebffdd8f5e121d6bd22cb7ef432.
Static7zip extraction, no installer/vendor executable execution. APPVersion.json
confirms3.2.33 and CurrentCompany.json confirms Keydous. The1825-file application
payload contains current host implementations; no exact3793 or5038 MCU image has
been established in it. Electron snapshot blobs are not keyboard firmware.
The [international download page](https://keydous.store/pages/downloads) still
routes V4S to the V4 configurator and an older desktop download.

Fresh V4 index.fdae6f57.js SHA256
b6f93060b16308f833277303c1ca066165d5a165e617b5f46991b59f8eeeedfb,
loader294b8f87, exact3793 class1ea59434 and inherited d3637439 were acquired.
Desktop equivalents: index.f019ef1f, loader63d2d0e6, class18c25db0,
base1699dd3e. Both still select exact board3793, not neighboring3496.
The generic current configurator was also checked; its additional dongle ZIP
member support does not identify a matching5038 firmware package.

The actual updater posts {dev_id:3793} to
https://api2.rongyuan.tech:3816/api/v2/get_fw_version and downloads the returned
file_path. Fresh exact request returns HTTP500 /Record not found. Adding the
explicit Keydous company does not find it. Positive control2307 returns metadata;
Keydous2454 returns a real downloadable bundle, so the request path works.
3496 also returns Record not found. Alternative currently referenced API hosts
returned404 for this endpoint. This is a bounded acquisition failure, not proof
that the manufacturer has no V4S update or private support package.
Two filename candidates derived from the known updater pattern returned404;
there was no wide filename brute force. OSS bucket listing returned403.

Public reverse-engineering repositories
[ry5088-flasher](https://github.com/dot-agi/ry5088-flasher) and
[sharkfin](https://github.com/dniminenn/sharkfin) were inspected as primary
research references. Pinned trees2a7db6ad83cf20385897b4080a586764916656e5 /
8c999ba7d44254f5206fb0ff25ed7cea6ebbb496. Neither inspected inventory supplied
an exact target keyboard/receiver image. No third-party implementation was
copied into HallJoy. Its generic hardware/packet observations are not proof of
the V4S radio implementation.

## Retry after owner enabled VPN — 2026-11-14 08:16 UTC

Repeated on the current connection after the owner enabled VPN; the actual VPN
route was not independently measured.11 requests/receipts saved under the private
research root vpn-retry-01/complete-receipt.json. Exact3793 still returns500 with
16-byte Record not found, both with ordinary web-origin headers and without them;
3496 likewise500. Controls2454 and2307 return200 firmware metadata. The actual2454
package downloaded successfully:148866 bytes, SHA256
5a6f11195bad8c760910574f631520b19d3660ca9a93ecc65e63644a9e10e88f,
identical to the prior acquisition. Thus server access and this download work;
the exact3793 metadata remains unavailable through this updater endpoint.
Alternative api2.qmk endpoint404; api3 endpoint404 without Origin (400 origin
policy response with the qmk web Origin). Known3793_v410 candidate404. No new exact
keyboard/receiver image acquired and no implication that no private bundle exists.
No production/runtime/status change.

## Exact host protocol findings

- E5/FE/01/page is still the calibrated128-slot table getter, four64-byte pages.
  The exact3793 class inherits it; no forced-rescan/generation argument is sent.
- getJiaoZhunXinXiLoop is explicitly disabled in wireless mode in the current
  manufacturer base. That is a calibration-view limitation, not proof that all
  wireless analog is impossible: HallJoy already receives real1B reports.
- 1B uses one Boolean enable byte. No documented extra full-matrix/report mode
  was found in this current host implementation.
- F7 reports online/send-ready/read-ready/boot state. These are transport
  readiness fields, not a measurement generation or ADC freshness witness.
  F6/0A selects the remote keyboard, FC retrieves its pending response.
  Manufacturer caches the selected target, as HallJoy does.
- Dongle-own F1 identity is gated by a specific manufacturer VID/PID allowlist;
  3151:5038 is absent. Do not invent a receiver board ID from its shared PID or
  add an unreviewed F1 probe just to generate another tester build. Exact keyboard
  class getDongleUSBVersion/getDongleRFVersion methods are empty.
- The RF upgrader finds the SAME keyboard identity, enters RF boot and writes
  the radio image starting at65536; this does not prove that such an image is
  the USB receiver firmware. Normal keyboard upgrade starts at20480.
- Reviewed updater has boot/prepare/write/complete, not a documented read-flash
  export. Boot/calibration/reset writes are not read-only acquisition paths.
  No device was put into DFU, calibrated, reset or flashed.

## Newly acquired sibling binaries and own execution

These are comparison inputs, explicitly NOT3793 or5038:

| Board / manufacturer version | Exact keyboard SHA256 | E5 getter / post-conversion producer |
| --- | --- | --- |
| Keydous NJ81-CP2454 /v413_oledv111 | 52a8fe9473779083f4393cf3faa1701cce2bf2857d329c8b0010caf3da75d08b | 080065E8 /0800F35E..0800F3CE |
| H60Pro/SK61HE3708 /v507 | 78ad3546e6d5006cd1c5d32495c44274bc14dbc0afa42748a6b011c2dbece4aa | 08006384 /0800EB9A..0800EC0A and0800F386..0800F3F4 |

Images include the bootloader and are mapped at08000000. The2454 ZIP contains
keyboard and OLED members; no established receiver/radio member. The3708 package
is raw DEFLATE. Acquisition URLs/hashes are in family-firmware-acquisition.json.
The3708 vendor change note mentions a Bluetooth power-saving travel-test stutter
fix; it does not establish a V4S fix or that the two implementations are identical.

Own strict Thumb execution reuses tools/firmware_replay.py. cache-audit.py:
**PASS4608 producer passes,156 getter pages, two images /three producer branches**.
RAM/registers and already-converted input depths are synthetic. Entry is AFTER
sensor conversion; stop precedes further keyboard processing or the event sender.
Boot, ADC, scan scheduling, calibration, interrupts, RF, USB and timing are not
executed. This is execution PASS for these boundaries, not full support approval.

Concrete findings at those exact boundaries:

1. FE copies64 bytes from state+10BC+64*page. State is20001410 for2454,
   200087B4 for3708; depth arrays200024CC /20009870. Getter changes response
   buffer/stack only. It neither starts a scan nor refreshes the source table.
2. The post-conversion producer stores each depth to the same array before any
   1B sender call. Releases clear it before sending zero. This works with the
   report-enable flag off as well as on. All128 modeled slots, independent
   simultaneous positives, alternate-key releases and full release were checked.
   Floors10 /20 are properties of these sibling branches, NOT measured V4S limits.
3. A deliberately inconsistent synthetic state (inactive flag but poisoned
   nonzero cached depth) is not repaired by the getter or inactive-release branch.
   This demonstrates that reading is not recomputation; it does NOT show that
   normal hardware reaches that inconsistent state.
4. sender-audit.py separately executes eight calls in the two pinned sender
   fragments:2454 08013B4A..08013B90;3708 08013736..08013782. Report buffers
   20006BE4 /2000F130 and pending bits2000001C /20007018 are overwritten by
   successive calls with no intervening modeled consumer. This confirms a single
   pending report at this fragment. Mode query/readiness are supplied synthetic
   inputs; consumer/interrupt/USB/RF are not executed. It does not measure actual
   packet loss or prove the target uses this implementation.

## Implication for log51 and alternatives

Under the reviewed sibling producer, losing a zero event can leave a host stream
value stuck, but the independently read keyboard table would already contain
zero. Log51 instead includes repeated accepted RF table replies retaining a
positive residual. A lost event alone therefore does not explain the whole
observation IF the target has the same producer and replies are fresh reads of
that producer's table. Those two conditions remain unproved for exact V4S RF.

The next distinction is keyboard scan/cache versus radio/receiver forwarding
cache versus host transaction interpretation. A successful F7/FC exchange proves
an exchange, not a new sensor measurement. Re-requesting four cached pages, a
transport flush, or repeated1B off/on does not establish fresh conversion.
HallJoy already requests128 HID input buffers and uses an independent overlapped
reader; the third-party default32-buffer burst observation cannot simply be
rebranded as the cause here. Actual pre-application loss remains unmeasured.

| Candidate | Evidence / disposition |
| --- | --- |
| Existing1B plus E5/FE reconciliation | Best currently reviewed path; keep Experimental RF and its existing residual restriction. No new capability established. |
| Higher frequency of identical FE requests | Getter does not force measurement in executed siblings; no justified freshness fix. No artificial rate cap added. |
| New full-matrix/rescan opcode or alternate live buffer | Unresolved for exact3793/5038; no safe current producer command found that establishes it. |
| Raw ADC path | No established fresh full ADC+conversion path for exact RF target. Uncalibrated raw values are not equivalent to usable analog depth. |
| 1C/1E calibration, boot/reset/flash | State-changing operations; not analog activation or a safe dump mechanism. Rejected for this research. |
| Digital key-up, expiry timer, guessed baseline/floor | Would replace missing analog truth with a guess; rejected. |

See the [alternative acquisition / receiver cache continuation](KEYDOUS_NJ98_V4S_ALTERNATE_FIRMWARE_2026-11-14.md)
for two additional official offline packages, a newly acquired MonsGeek receiver/
PAN108 dump, six receiver branch replays and two production response-ambiguity
counterexamples. These do not establish exact Keydous target firmware identity.

## Remaining concrete acquisition and preserved delivery

Missing artifacts are the exact board3793 keyboard image matching the tester's
revision, its radio image if separate, and the image(s)/hardware identity for the
specific3151:5038 receiver. A manufacturer offline update/service bundle with
matching identity/version metadata is a useful source; another old NJ98 or
NJ81 image is not a substitute. No supported read-only flash dump route has been
established from the inspected updater. Further sibling replay cannot answer
which MCU/cache actually serves V4S RF FE replies.

Once exact bytes are available, trace the FE reply source and every writer of
that array across keyboard/RF/receiver, then the scan conditions/release scheduling
and report-buffer consumer. Validate any rescan/full-state candidate for typing,
simultaneous keys, release, persistence and cleanup before a new tester delivery.
No fresh-matrix fix, exact-target firmware replay or impossible-to-support verdict
is claimed now. No speculative EXE was handed out for these findings.

Private evidence root stated above includes cache-evidence-manifest.json,
cache-audit-result.json, sender-audit-result.json, acquisition receipts and pinned
sources. Vendor bytes remain local, not redistributed. Only research/documentation
changed. Ordinary EXE remains11791360 bytes /SHA256
605d45568d301d1eb525e246001e1a88f6d06dd023b9e664e347429aa463ac76.
Production code, support conclusion, release and live Sheet remain unchanged;
no native suite/build rerun is needed for this research-only update.

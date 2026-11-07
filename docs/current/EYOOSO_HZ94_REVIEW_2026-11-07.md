# E-YOOSO HZ-94 wired investigation — 2026-11-07

The tester reports no analog in HallJoy. The complete private
HallJoy (32).log matches the posted header at 2026-11-07T00:57:35Z. Its SHA-256
is `bcf26cdd9e9d08d5bdfc37c791f9c0d0611ee21e95f756bff29b6d12bea25135`.
Windows inventory reports `0416:7372`, product string Gaming Keyboard,
interfaces 00/01/02. The analyzer found 20 complete snapshots. The active
snapshots report no analog source and mask `0x40` (FamilyCandidate).
The NA87/AJAZZ Witmod backend reported zero updates. Its row prints the
backend's expected `FF1B:0091`; this field does not establish the attached
device's HID collection. The report lacks the candidate's caps and
identity response, so the failed admission stage is unknown.

## Exact configuration and firmware search

The [official HZ-94 download](https://e-yooso.com/download?page=6), the
[ZIFRIEND driver page](https://www.zifriend.net/pages/drivers-manuals-1) and
the HZ-94 manual's shortlink all deliver the same 17,085,187-byte
V2.03.04 archive, SHA-256
`3265d813398ce39abc0acff491ea5562f6d2840b05997599e473897da80b2670`.
The Inno package and Qt v1 resources were extracted without running the
installer, configurator or updater. The Qt registration passes data,
names and tree at PE VAs 620C18, A39F18 and A3D440.

- The SDK's product record pairs `likeName=HZ-94` with
  `hidName=7187_AUSXHERGB` and `nucNum=GDXXX XXX`. The SDK lists
  `0416:7372` under M484 in its transport registry. These are distinct
  evidence fields. The actual board's controller string is unverified.
- `KeyInfo_7187_AUSXHERGB.config` contains 94 positioned keys over six
  primary rows (16/17/17/16/16/12), horizontal Enter, long left Shift,
  and the compact numpad. The seventh row contains alternate locale
  positions. The generic 98/99 counts in catalog labels do not describe
  the primary geometry. Key profile SHA-256
  `c2a7bf29b7f7256fa00f3b7a65d7862d24888ecb2b1eb5e57fe779bc9bfd5e26`.
- The bundled `7186_AUSHERGB` V1.00.04 updater and equivalent embedded
  firmware target HZ-68, as proven by the SDK product record and its
  separate 68-key configuration. The other embedded binaries are
  X86HERGB and KC66RGB. No exact 7187 image was found in this package.
- The current official Control HUB link loads config for VID3554 and
  other PIDs. It offers no matched firmware route for the observed
  0416:7372. Private source bytes and extracted artifacts are under
  `.local/research/eyooso-hz94-20261107/`.

## Next evidence

Witmod transport reuse is a candidate. The shared VID/PID already covers
IROK NA87, IROK ND75 and AJAZZ AK820 MAX wired RGB with different product
gates. Keep the HZ-94 analog route unadmitted until its endpoint, live
identity, map and sensor reply format are established. Ask for the exact
firmware/version shown in the tester's working configurator and a full
keyboard photo to match the regional geometry. If the configurator gives
only a marketing name, the next test EXE should include the verified
layout and record HID caps and the read-only identity result in the normal
support log before trying any model-specific travel commands.

No device was opened locally. Support status has not changed.

## Trial build, discovery revision 1

The V2.03.04 SDK contains one explicitly named HZ-94 profile. The tester
build covers `0416:7372` from the report plus the SDK-listed M484
transports `0416:7402` and `0416:7472/7473/7474`. The latter are probe
targets with a compatible descriptor, not confirmed retail revisions.
Only the vendor collection `FF1B:0091` with 64-byte input/output is
probed. All other HID collections can contribute metadata only.

The trial reuses the Witmod worker. The read-only identity must decode
to `7187_AUSXHERGB`. There is no firmware-version pin. This lets variants
with the same reply contract proceed even if the controller field differs.
The capability reply must use the reviewed 0–40 path and the complete
6x22 assignment table must fit the 94-key factory positions. Both checks
precede the volatile `21/18/02` subscription. Wrong identities are held
until the endpoint disappears. The first valid nonzero travel event
connects the source and publishes the verified ANSI layout token.
Physical keys stay separate even when their assigned HIDs coincide.
Pause, transport loss and stop neutralize the source. The worker attempts
matching unsubscribe on exit. No firmware update or persistent setting
command is sent.

The exact 7187 firmware remains unavailable. Typing coexistence, sustained
multi-key delivery, shallow resolution, held-key and release reliability,
USB timing, crash cleanup and coexistence with the vendor configurator
require a real 7187 session. The 0–40 scale and event envelope are checked
against the known Witmod path. No raw 0x23 test mode or digital travel
substitution is used. The ANSI preset comes from the exact source profile.
Alternate locale data is not evidence for a second physical preset.

`evidence.witmod.*` survives ordinary Open log snapshots. It records
physical interface/collection, bcdDevice, HID page/usage/report lengths,
routing or open errors, identity controller/product/firmware, map rows
and assignment counts, first travel, aggregated releases/range/peak
active channels, rejected envelopes, heartbeat and unsubscribe. Identity
reply fallback captures only report ID/opcode and identity status/length.
No raw packets, key names/values, serials or device paths are added.
Evidence for transient discovery/identity failures is deduplicated and
stored within the shared bounded log. A quiet stream is recorded as
waiting for motion, not as a broken keyboard.

Test wired with the E-YOOSO configurator closed. Press and release WASD
slowly, try two keys together, and check the built-in graph and virtual
controller. After the attempt, use Open log even if HallJoy still shows
no supported source. Return the ordinary HallJoy.log. Live Main!A1:C1301
contained no HZ-94 entry. The trial remains private evidence pending the
real protocol result and no support status was promoted.

## Verification and delivery

Native suite and ordinary Release pipeline passed. The actual candidate's
Witmod self-test reads the simulated identity, capability and map via
overlapped I/O, rejects the 7186 neighbor, and exercises the production
depth publication, aliases, release, Fn and layout token. The compiled
profile/layout test selects the token and exports all 94 exact geometry
rectangles. Compiled catalog plus live Sheet checks pass for 87 Supported
models and 284 yellow notices. The local tester build is not a hardware
result or a published release.

Delivery is `build/bin/Release/x64/HallJoy.exe`, 1.6.14.0, 11,388,416 bytes,
SHA-256 `68dea0c2cb48fb4f54731c625df971d8124d193972f23e80f1ca2de77a9461a6`.
Installed binary matches the validated candidate. Private test logs and the
Sheet readback are in the task directory. Checked source backup:
`.local/backups/2026-11-07_eyooso-hz94/`. Generated output backup:
`.local/backups/layout-integrate-frlpt_77/`.

## v1.6.15 release scope

Owner decision on 2026-11-07. Await the tester's report before announcing
support for HZ-94. The gated trial and its evidence remain available for
testing. The README, public compatibility overview and release notes omit
the model. The Sheet has no HZ-94 support entry.



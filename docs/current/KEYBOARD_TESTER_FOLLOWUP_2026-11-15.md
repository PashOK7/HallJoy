# Keyboard tester follow-up, 2026-11-15

Evidence comes from saved project records, owner-provided screenshots and downloaded
logs. A local build receipt establishes which EXE was prepared. It does not
establish that the EXE was posted or received. Discord conversation history was
not checked. Dates below describe the build or review, not the posting date.

| Model and scope | Last known tester evidence | Outstanding result |
| --- | --- | --- |
| HATOR Skyfall 80 MAG Ultima 8K Wireless, 2.4 GHz | November 9 intake report 42 predates the dedicated receiver build. | Result from the receiver build with exact ANSI/ISO presets. |
| Akko 5087 V3 HE, wired board 3131 | November 13 intake shows identity, then old admission stops before depth reads. | Result from the E5/FE matrix build, hash 9d55f0fd. |
| Keydous NJ98-CP V4S, 2.4 GHz | Revision7 report59 received November16: three-key behavior better,4+ still stick. Synchronization passes; a pending remote reply blocks the table for41s. Wired confirmation retained. | Revision8 EXE a3f93418 now applies late pages and restores manufacturer command handoff pacing. Targeted RF multikey retest pending; see KEYDOUS_NJ98_V4S_COMMAND_HANDOFF_2026-11-16.md. |
| ATTACK SHARK R82 HE | October 31 report 27 saw the USB collection, but exclusive access stopped identity and analog. | Result from the updated discovery build with revision 2 telemetry. |

Evidence for these four branches is in [the unreleased notes](../releases/RELEASE_NOTES_NEXT.md),
[Akko](AKKO_5087_V3_HE_2026-11-13.md),
[Keydous revision 7](KEYDOUS_NJ98_V4S_F7FC_FIX_2026-11-14.md) and
[R82 HE](ATTACK_SHARK_R82HE_LOG27_2026-10-31.md). The HATOR build and intake log
are archived locally in .local/research/hator80-mchosegt-20261109/.

## Replies already received

- EPOMAKER HE108 USB/2.4GHz, MonsGeek FUN60 Pro USB and MCHOSE Ace68 Air
  II/III USB: owner accepts tester confirmations and requests Supported on
  November16. Testing notices removed for exact identities in1.6.16 preparation.
  [Scope and checks](RELEASE_1.6.16_PREPARATION_2026-11-16.md). No further trial
  gate or firmware prerequisite for this decision. Ace68 Air2 remains separate.
- Keydous NJ98-CP V4S. [Revision7 result and log59](KEYDOUS_NJ98_V4S_LOG59_2026-11-16.md)
  confirm working marker synchronization, remaining multikey sticking and a blocked
  remote table transaction. RF stays Experimental.
- ARDOR GAMING Viper. On November 15 the owner reported that the tester said
  everything works well. This closes the wait for a result on the wired matrix
  trial. The depth scale remains provisional. [Model record](ARDOR_VIPER_2026-11-13.md).
- GT75. The Discord screenshot supplied November 15 shows a test EXE posted
  November 14 and a same-day positive reply. The owner then requested HallJoy.log.
  See [the model record](MCHOSE_GT75_2026-11-14.md).
- AULA F75 HE. The receiver result is negative for analog depth. The receiver
  opens, resolves the keyboard and replies, but returns no usable depth. A wired
  result is still absent from the two saved tester reports. See
  [the log review](AULA_F75_BOOM68_FEEDBACK_2026-11-14.md).
- SteelSeries Apex Pro TKL Gen 3. Tester log and Roblox feedback are available.
  A linear-curve retest is missing to separate the observed descending curve from
  the reported mouse/controller stutter. See [the model record](STEELSERIES_APEX_PRO_TKL_GEN3_REVERSE_2026-11-06.md).
- Corsair VANGUARD PRO 96. ISO depth and release are present in log 23. Range,
  multikey use and functional gamepad/typing feedback remain incomplete. See
  [the model record](CORSAIR_VANGUARD_PRO96_2026-11-02.md).

Ace 68 GT (stock 1.16), BOOM68-USB, FREEWOLF F68, E-YOOSO HZ-94 and Corsair K70
PRO TKL have positive tester evidence. Logitech G PRO X TKL RAPID returned logs
confirming the release issue and remains unsupported on the reviewed stock path.
The SU75 Pro V2 implementation has no recorded targeted tester handoff. ZENITH PRO
has a prospective buyer's inquiry and no recorded completed hardware test.

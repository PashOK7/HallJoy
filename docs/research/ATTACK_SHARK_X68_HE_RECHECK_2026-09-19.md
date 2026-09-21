# ATTACK SHARK X68 HE (non-Pro) firmware acquisition recheck

2026-09-19. No exact X68 HE firmware acquired; no support enabled.

The current official product page links qmk.top and now points its desktop
package at https://download.attackshark.pro/ATTACKSHARK/X68HE/ATTACKSHARKX68HE.zip.
The package download timed out; no complete ZIP was retained or executed.

The current qmk.top index.9Prprr1v.js was downloaded and source-hashed in
.local/research/attackshark-x68he/acquisition-20260919.json. It contains no
X68/ry5088_x68 model records and uses the same public Rongyuan firmware API.
The previously extracted desktop client provides exact X68HE IDs 2270/2472/2902.
All three POST get_fw_version requests again returned HTTP 500 Record not found.
A control request for X65HE ID 2268 returned HTTP 200 and v309 metadata, proving
the endpoint itself remains operational. This does not prove unpublished
X68 images do not exist; the exact image remains an acquisition blocker.

The existing X68 vendor client contains E5/FE reads of four pages of 32 u16
travel values. The previously acquired X65HE v309 reference handler emulation
was rerun successfully (8 cases), but remains X65 evidence only. It is not proof
that a particular X68 revision maintains that table in ordinary typing mode.
The Pro family's playable support also must not be substituted for this evidence.

Next useful input: exact X68 non-Pro image from the manufacturer, or a tester's
model/dev_id/firmware version that identifies another published revision.
See ATTACK_SHARK_X68_HE_RECON_2026-09-09.md for the detailed earlier analysis.

## 2026-09-20 continuation

Repeated exact-ID lookup returned the same three Record not found responses;
X65 control 2268 still returns v309. Evidence: acquisition-20260920.json in
the same local research directory. No X68 firmware or validation claim added.

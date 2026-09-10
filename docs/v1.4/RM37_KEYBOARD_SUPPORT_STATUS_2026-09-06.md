# RM-37 analogue keyboard status and Discord

HallJoy states only what it can prove positively. The diagnostics page reports
`Analog keyboard: connected and visible to HallJoy` when current native or UAP
telemetry shows an actual connected analogue source. Otherwise it says that an
analogue keyboard is not currently detected and offers Discord as the place to
request support. It never labels an unknown, digital, idle, or temporarily
unavailable keyboard as unsupported.

The status derives solely from HallJoy's existing native/UAP telemetry. It does
not inspect Raw Input, open HID interfaces, identify a model by VID/PID, send a
vendor command, alter calibration, or suppress key input. Mouse buttons and
letter bindings are therefore outside this feature by construction.

The keyboard page now shows the status as a red banner directly between the
keyboard preview and its tabs. The banner stays hidden while the engine's
startup generation is still running. It appears only after that generation has
completed and HallJoy is not receiving analogue data from any supported native
route or UAP source. It
contains an invitation to help add the keyboard, an active Discord button, an
active copy-link button, and a scannable QR code for the official invite
`https://discord.gg/5FQ297yZh`.
The 100px card uses Microsoft Sans Serif, the scalable counterpart of the
MS Sans Serif native GUI controls, with a 24px bold rose heading and 16px
regular body/buttons (scaled for window DPI). This removes the banner-only
Segoe UI family; existing controls elsewhere are not restyled. The former
SYSTEM bitmap font could ignore requested size changes. The heading row
reserves the measured full font height plus 4px; top alignment preserves
descenders without clipping them into a fixed-height row. Buttons are
32px tall and use the shared CustomPage button renderer. It has
a muted burgundy background, rounded border and coral accent. Join Discord
follows the invitation inline when space permits, or on the next line at
narrow widths. The QR uses a 29×29 version-3 EC-M matrix with four quiet
modules. Its tile is 94px instead of 74px at 96 DPI, making the actual matrix
74px instead of 58px. Each module boundary is snapped individually to a pixel,
so the matrix fills the available space without interpolation or extra margins.
The QR uses burgundy ink on rose paper with no additional frame;
it is a passive image with no hover, hand cursor or click action. Join Discord
opens the URL after a click; Copy link writes it as Unicode clipboard text.
The card is composed offscreen and presented with one blit, preventing hover
repaints from exposing a blank QR/background. Button positions use measured
prompt width, so Join Discord immediately follows the text when it fits.
The obsolete invitation-pending diagnostics line has been removed.
The UI timer refreshes
this status from existing telemetry and relays only a state change to the page;
it does not poll HID or use key presses as a proxy.

AULA HERO84 HE and ROG Azoth 96 HE remain frozen unsupported branches; this
display neither enables nor validates them.

Verification: portable state test, static safety/UI audit, complete native
backend audit suite, and ordinary Release x64 compilation all pass. This is a
connection-status feature, not hardware validation or a claim that every unseen
keyboard is unsupported.

Visual correction validation: ordinary Release x64 build and support static
audit passed. Inspected the running application's captured window at
818px width; independent zxing-cpp decoding of that screenshot returned
https://discord.gg/5FQ297yZh. Local evidence: .local/discord-polish-app.png.

Follow-up typography/QR correction: Release x64 build and support static audit
passed; running-window screenshot `.local/banner-font-qr.png` was inspected
and independently decoded with zxing-cpp to the exact official invite above.
Pre-change source/document backup: `.local/backups/banner-font-20260906/`.

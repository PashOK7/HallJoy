# HallJoy 1.5.3 candidate — owner test pending

Owner authorized GitHub publication, then explicitly required receiving and
checking the EXE before any publication. No source push, tag, draft, release or
asset upload has occurred. Do not publish until the owner confirms the test.
Full clean-checkout/CI release qualification remains for the publication step.

The candidate contains current ordinary HallJoy with NA87 and wired AULA MINI60
HE Pro native support, normal gamepad/binds and single support log. Existing
NA87 behavior is preserved per the owner decision; no timeout release was added.
ATTACK SHARK diagnostic is disabled. Its already-sent shark-3 EXE is preserved
at .local/backups/HallJoy-attackshark-shark3.exe for tester-log correlation.
No new hardware or visual check was performed locally.

Delivery: build/bin/Release/x64/HallJoy.exe
Version: 1.5.3.0
SHA256: 5481aa57e84ae7378193ef559d530b20cd34f771f8904d615e3e39afbbd26621
Size: 9211392 bytes.

Validation: MSVC Release build PASS (only established ViGEm missing-PDB warning),
source encoding/version audits PASS; exact EXE native MINI60 and NA87 self-tests
PASS, embedded ViGEm installer verification PASS. Tests ran on a temporary copy,
preserving the owner's adjacent log. Privacy sentinel is absent from test log.
ATTACK SHARK worker command, tester title and shark build marker are absent
from the linked image. Verification: .local/release-1.5.3-candidate-verification.json.

The current local source tree includes earlier unpublished work since1.5.2;
this candidate is not a cherry-picked two-backend-only patch. Release notes and
source synchronization still need final review before publication.


## Owner logging correction

The initial candidate accidentally enabled HALLJOY_DEVICE_SUPPORT_LOG,
HALLJOY_SINGLE_LOG_DIAGNOSTIC and HALLJOY_STABILITY_TRACE in the unconditional
project definitions. This forced an adjacent HallJoy.log even with Enable
logging off. Removed these defaults; explicit ATTACK SHARK diagnostic retains
them only in its conditional build group. Native NA87/AULA stay enabled.
Also removed HALLJOY_IROK_NA87_NATIVE from support_log.cpp's diagnostic forwarding
condition: native support must not bypass the ordinary settings-aware writer.

Ordinary support logging again follows the existing policy: no file for healthy
operation with logging off, opt-in continuous recording, automatic incident/
missing-source reports. These automatic reports are separate from continuous
logging and remain unchanged. Existing logs are retained, not deleted.

Release isolation static regression added; production-flags Windows support-log
integration test PASS (off/no file, opt-in, automatic incidents, write recovery).
Full static gate PASS. Exact rebuilt EXE NA87/AULA/embedded installer tests PASS;
no adjacent HallJoy.log created by those tests, forced trace markers absent.
Verification: .local/release-1.5.3-logging-fix-verification.json.
The updated hash above supersedes the first candidate. Owner retest still pending;
no GitHub publication has occurred.


## Startup white-square report

Owner screenshot shows a small white rectangle at the client origin; owner
clarified it appears only during startup. Source review found keyboard BUTTONs
created WS_VISIBLE at(0,0),10x10 before subclass installation and final layout.
Changed creation to hidden, zero-sized controls; final positioning explicitly
uses SWP_SHOWWINDOW for both deferred and direct placement. No startup sleep or
paint overlay was introduced. This removes that transient placeholder path;
visual confirmation of the reported artifact remains with the owner.
Incremental Release build and source encoding gate passed. No visual run.
Backup: .local/backups/keyboard-page-before-initial-button-visibility.cpp.
Updated artifact hash above supersedes the logging-fix artifact; that fix remains.
Publication is still pending owner confirmation.

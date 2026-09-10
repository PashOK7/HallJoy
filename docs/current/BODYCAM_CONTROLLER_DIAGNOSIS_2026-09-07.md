# Bodycam controller diagnosis — 2026-09-07

User reports HallJoy worked before reboot while a subsequently connected physical
controller was ignored. After reboot HallJoy is ignored too. No changes made to
drivers, controllers, Steam, game settings or HallJoy during this diagnosis.

Read-only observations:

- HallJoy parent 28684 and its real ViGEm output host 31284 were running.
- Windows reports a physical USB Xbox 360 device
  USB/VID_045E&PID_028E/1187A89 under a USB root hub, and a virtual Xbox 360
  USB/VID_045E&PID_028E/01 under ROOT/SYSTEM/0004 (ViGEm).
- XInputGetState succeeds for slots 0 and 1; slots 2/3 return 1167.
  Snapshot: slot 0 packet 183643, LY=-256; slot 1 packet 1334, axes neutral.
  This snapshot alone does not conclusively map device identities to slots.
- Actual settings: one virtual gamepad, enabled; BlockBoundKeys=1.
- Steam controller log contains Bodycam app 2406770 activations.

Leading hypothesis: game selects one controller/index, and enumeration order
changed at reboot. Not proven; do not claim a Bodycam-specific slot-zero rule.
Suggested distinguishing experiment: remove physical controller/receiver, then
restart HallJoy and Bodycam with only the HallJoy controller present. If still
broken, compare XInput output activity and Steam routing while the game runs.

## After user closed everything

No HallJoy.exe processes remain. Its ViGEm Xbox device /01 is no longer present.
Xbox /1187A89 remains present under the physical USB root hub, location
Port_#0003.Hub_#0001, path PCIROOT(0)#PCI(1400)#USBROOT(0)#USB(3), service xusb22.
This establishes that the remaining Xbox entry is not the HallJoy ViGEm device.
It does not identify which piece of hardware presents that controller interface;
the earlier wording "physical gamepad" was too specific. User says no actual
gamepad is connected. A peripheral/adapter can expose an Xbox interface.
No devices disabled/uninstalled. A temporary targeted disable would require
user approval and must not target Keychron or keyboard/HID interfaces generally.

Incidental observation: Keychron K4 HE USB MI_02 reports problem code 28;
other listed Keychron HID interfaces are OK. No driver changes made and this
was not established as the cause of the Bodycam issue.

## After reported removal of all external USB peripherals

User reconnected only the keyboard to reply. Xbox /1187A89 still reports
IsPresent=True, ProblemCode=0; XInput slot 0 succeeds (packet 219049, LY=-256),
slots 1-3 return 1167. It is not merely an old browser Gamepad API entry.
Its USBHUB3 parent leads to Intel PCI VEN_8086 DEV_A12F; device arrived
2026-09-07 21:51:22, hub at 21:51:19. These observations do not identify the
physical or software source; do not insist an external gamepad is connected.
HallJoy's source creates Xbox targets through ViGEm (vigem_child_transport.cpp);
the observed remaining Intel-USB child differs from its removed ViGEm child.
Evidence therefore points away from a leftover HallJoy target, while the
identity of the remaining source is still unresolved.

## 2026-09-08 follow-up

At local time 10:16 Windows still reports LastBootUpTime 2026-09-07 21:51:04.
Latest Kernel-General event 12 is 2026-09-07 21:51:03. Kernel-Boot event 27
at 2026-09-08 07:06:52 reports boot type 0x1, versus 0x0 at the last full boot.
The Xbox device has the same instance and arrival timestamp (21:51:22).
No HallJoy process is running; Steam and VirtualDesktop.Service are running,
but that alone does not attribute the controller to either.
Evidence suggests hybrid/fast startup rather than a fresh kernel restart.
Next diagnostic step: explicitly choose Windows Restart, not shutdown/power-on;
verify the boot timestamp afterward. Do not infer that the user did nothing or
claim the controller survived a verified full restart on the basis of this run.
Microsoft documentation confirms Restart performs a full boot cycle:
https://learn.microsoft.com/en-us/troubleshoot/windows-client/setup-upgrade-and-drivers/fast-startup-causes-system-hibernation-shutdown-fail

## Verified full reboot and direct hub query

At 10:21 on 2026-09-08, LastBootUpTime is 10:19:34; Kernel-Boot event 27
at 10:19:35 is type 0x0. No HallJoy process. Xbox /1187A89 is present again,
with new arrival 10:19:56. Fast-startup state does not explain this result.

Read-only standard USB connection/descriptor queries against Intel root hub
port 3 succeeded using `.local/usb_identity_probe.cpp` (Windows SDK usbioctl.h).
IOCTL_USB_GET_NODE_CONNECTION_INFORMATION_EX: connection status 1 (connected),
VID 045E PID 028E bcdDevice 0110, device address 6.
String descriptors: manufacturer empty, product Controller, serial 1187A89.
This is stronger than a stale PnP/browser entry, but still does not identify
the hardware product. No vendor reports, configuration changes, device resets,
driver removal or device disable performed. Proposed next discriminating step
is user-approved reversible disable of this exact Xbox instance only, followed
by a HallJoy/Bodycam check; never disable the root hub or Keychron generically.

## User-authorized targeted disable

User explicitly approved disabling the unidentified Xbox on 2026-09-08.
Validated exact instance USB\VID_045E&PID_028E\1187A89, XnaComposite, Intel USB
parent, ProblemCode=0; no HallJoy process running.
Disable-PnpDevice on that exact instance returned HRESULT 0x80041001.
Follow-up pnputil /disable-device on the same instance explicitly reported
"Device is pending system reboot to complete a previous operation."
Post-check: ConfigFlags=1 (disabled configuration), but IsPresent=True and
ProblemCode=0: disable is requested, not verified effective in this session.
No driver removed, no hub/Keychron disabled, no automatic reboot performed.
Ask user to save work and use Windows Restart, then verify the device and
XInput slots. Re-enable only this instance when desired using
`pnputil /enable-device "USB\VID_045E&PID_028E\1187A89"`.

## User-requested restoration

User reported that everything worked after the disable/reboot and explicitly
requested restoring the controller. Pre-check confirmed ProblemCode=22 and
ConfigFlags=1 for the exact Xbox /1187A89 instance. Executed pnputil
/enable-device on that instance only. Windows reported success; post-check
Status=OK, ProblemCode=0, ConfigFlags=0. No reboot requested by this operation.
No other devices changed. The successful disable experiment supports a
controller-selection/interference issue but does not establish the exact
Bodycam mechanism or identify the underlying hardware.

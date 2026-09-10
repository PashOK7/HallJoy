# Digital keyboard preview

Green dots now consume canonical digital key state from Windows Raw Input.
The renderer's incomplete HID-to-VK lookup and GetAsyncKeyState polling were
removed. The existing app scan-code translation is the single ingress used
by both keyboard events and the preview, including Num Lock independent
numpad positions and separate main/numpad Enter keys.

Raw keyboard registration is enabled in ordinary builds as well as diagnostic
builds. Observation runs on the UI thread regardless of gameplay admission,
Pause, or optional key-blocking hooks. It neither changes analogue values nor
enables digital gameplay fallback. Mouse input does not enter this state.

State tracks held keys per Windows device, aggregates simultaneous ownership,
ignores typematic duplicates and removes a disconnected device's held keys.
The renderer accepts the entire canonical key domain; future event mappings
do not require another renderer table. Windows must actually deliver a digital
event: firmware-only Fn/layer actions with no Windows event cannot light a dot.
Keys held before registration are observed on the next delivered event.
The common ingress also maps ISO's extra key and F13-F24. Pause is rendered
as a 150ms pulse because Windows does not reliably deliver its release.

Portable regression covers every canonical code, repeat, independent devices,
foreign release, unplug while held, separate Enter keys, reset and invalid input.
Pulse expiry and device identity reuse are covered too. This regression,
the static audit suite and ordinary Release x64 compilation passed. Physical
Num Lock/Shift and hotplug behaviour still needs observation in the updated UI.

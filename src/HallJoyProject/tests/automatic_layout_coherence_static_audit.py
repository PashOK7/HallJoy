from pathlib import Path
root=Path(__file__).resolve().parents[3]/'src/HallJoyProject/HallJoy'
client=(root/'analog_host_client.cpp').read_text(encoding='utf-8')
reader=client.split('bool AnalogHostClient_GetTelemetry(',1)[1].split('bool AnalogHostClient_',1)[0]
assert 'result.deviceCount = 0;' not in reader
assert 'candidate.deviceCount==candidate.denseDeviceCount' in reader
assert 'g_uiDeviceSnapshots.Resolve' in reader
assert 'result.deviceSnapshotValid' in reader
backend=(root/'backend.cpp').read_text(encoding='utf-8')
assert 't.pluginDeviceSnapshotValid = host.deviceSnapshotValid;' in backend
layout=(root/'keyboard_layout.cpp').read_text(encoding='utf-8')
assert '!t.pluginDeviceSnapshotValid' in layout and 't.deviceCount!=t.pluginDeviceCount+nativeCount' in layout
page=(root/'keyboard_page_main.cpp').read_text(encoding='utf-8').split('case WM_APP_KEYBOARD_LAYOUT_CHANGED:',1)[1]
assert page.index('KeyboardLayoutChange_StatusOnly')<page.index('RebuildKeyboardButtons(')
print('AUTOMATIC_LAYOUT_COHERENCE_STATIC_AUDIT=PASS staged_read=1 explicit_validity=1 status_only_no_rebuild=1')

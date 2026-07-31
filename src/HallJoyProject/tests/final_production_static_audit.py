#!/usr/bin/env python3
from pathlib import Path
import re
import sys

root = Path(__file__).resolve().parents[1]
hall = root / 'HallJoy'

def read(name: str) -> str:
    return (hall / name).read_text(encoding='utf-8-sig')

project = read('HallJoy.vcxproj')
rt = read('realtime_loop.cpp')
mad = read('mad68pr_backend.cpp')
host = read('analog_host_client.cpp')
debug = read('debug_log.cpp')
backend = read('backend.cpp')
ui = read('keyboard_subpages.cpp')
app = read('app.cpp')
main = read('main.cpp')
render = read('keyboard_render.cpp')
scheduler = read('vigem_output_scheduler.h')
spark = read('backend_sparklink.inc')
sayo = read('backend_sayo.inc')
hex80 = read('hex80_backend.cpp')
addressed = read('addressed_analog_backend.cpp')

checks = {
    'native target is production': 'HALLJOY_MAD68PR_NATIVE;HALLJOY_PRODUCTION' in project,
    'native target does not force diagnostic': 'HALLJOY_MAD68PR_NATIVE;HALLJOY_DIAGNOSTIC' not in project,
    'address wake is used': all(x in rt for x in ('WaitOnAddress', 'WakeByAddressSingle', 'WakeByAddressAll')),
    'address wake import library is linked in every configuration': project.count('Synchronization.lib') >= 4,
    'precise output deadline timer is used': all(x in rt for x in ('CREATE_WAITABLE_TIMER_HIGH_RESOLUTION', 'Backend_GetNextOutputDeadlineQpc', 'precise_output_deadline')),
    'all native families participate in UAP arbitration': ('NativeAnalogProtocol::Mad68A0' in mad and 'NativeAnalogProtocol::Hex80' in hex80 and 'NativeAnalogProtocol::Addressed09402' in addressed and 'NativeAnalogProtocol::SparkLink' in spark and 'NativeAnalogProtocol::SayoDepth' in sayo),
    'universal native target can run without UAP': ('universal native continuation enabled; UAP unavailable for this run' in main and 'if (Mad68ProR_IsProtocolDevicePresent())' not in main),
    'native protocols preserve multi-device analogue max': ('NativeAnalogBackends_ReadMilli(hidKeycode)' in backend and 'v = std::max(v, std::clamp((float)native.milli / 1000.0f' in backend and 'if (cache.wootingReady && modeCode != 0)' in backend),
    'unified output scheduler is wired': all(x in backend for x in ('VigemOutputScheduler::Decision::DeferUntilDeadline', 'All analogue backends share the same scheduler', 'Backend_GetNextOutputDeadlineQpc')),
    'all analogue sources issue realtime wakes': ('RealtimeLoop_NotifyInputChangedAt' in mad and 'RealtimeLoop_NotifyInputChanged' in host and 'RealtimeLoop_NotifyInputChanged' in spark and 'RealtimeLoop_NotifyInputChanged' in sayo and 'RealtimeLoop_NotifyInputChangedAt' in hex80 and 'RealtimeLoop_NotifyInputChanged' in addressed),
    'fixed-deadline coalescing contract exists': all(x in scheduler for x in ('The first changed report after an idle', 'newest report and become due at a fixed deadline', 'dueTick_ = SaturatingAdd(lastSentTick_, minimumIntervalTicks_)')),
    'MAD68 special limiter bypass is removed': 'bypassing the generic 1 ms output limiter' not in backend,
    'duplicate ViGEm keepalive is removed': ('Periodic duplicate keepalives add no' in backend and 'kKeepAliveUs' not in backend),
    'closeable input event was removed': all(x not in rt for x in ('g_inputEvent', 'ResetEvent(waitInputEvent)', 'SetEvent(eventHandle)')),
    'production trace is hard-disabled': 'Final production builds never aggregate or write detailed latency data' in rt,
    'Addressed file trace is diagnostic-only': '#if defined(HALLJOY_DIAGNOSTIC)' in addressed and 'HallJoyAddressedAnalogTrace.log' in addressed,
    'MAD per-key file log is diagnostic-only': bool(re.search(r'void Log\(const wchar_t\* fmt, \.\.\.\).*?#if !defined\(HALLJOY_DIAGNOSTIC\).*?return;', mad, re.S)),
    'UAP host file log is diagnostic-only': bool(re.search(r'void HostLog\(const wchar_t\* fmt, \.\.\.\).*?#if !defined\(HALLJOY_DIAGNOSTIC\).*?return;', host, re.S)),
    'quiet native A9 watchdog remains': all(x in debug for x in ('Production watchdog is intentionally silent', 'Mad68ProR_EmergencyRestoreInputOnce', 'defined(HALLJOY_MAD68PR_NATIVE)')),
    'telemetry aggregator remains': all(x in backend for x in ('void Backend_GetAnalogTelemetry', 'NativeAnalogBackends_GetTelemetry', 'AnalogHostClient_GetTelemetry')),
    'generic native UI telemetry remains': ('nativeProtocolCount' in backend and 'Native protocol %S:' in ui),
    'configuration covers MADLIONS native A0': 'MADLIONS native A0: VID 373B / PID' in ui and 'native A0 stream' in ui,
    'configuration covers Hex80': 'ATK x QK Hex80:' in ui,
    'configuration covers Addressed': 'Addressed Analog 09/94/02:' in ui,
    'configuration covers Spark': 'SparkLink:' in ui,
    'configuration covers Sayo': 'SayoDevice:' in ui,
    'configuration covers plugin host': 'Analog host:' in ui,
    'gamepad tester covers MADLIONS native A0': 'Analog input: MADLIONS native A0' in ui,
    'gamepad tester covers Hex80': 'Analog input: ATK x QK Hex80' in ui,
    'gamepad tester covers Addressed': 'Analog input: Addressed 09/94/02' in ui,
    'gamepad tester covers Spark': 'Analog input: SparkLink' in ui,
    'gamepad tester covers Sayo': 'Analog input: SayoDevice' in ui,
    'gamepad tester covers UAP/Wooting': 'Analog input: Wooting Analog SDK' in ui,
    'both UI pages request live telemetry': ui.count('Backend_GetAnalogTelemetry') >= 4,
    'Raw Input registration remains': 'RegisterRawInputDevices(rid, 2' in app,
    'green digital indicator remains': 'DrawDigitalIndicatorAA' in render,
    'v1.4 build identity is present': 'HALLJOY_BUILD_ID_W' in mad,
}

failed = []
for name, ok in checks.items():
    print(('PASS' if ok else 'FAIL') + ': ' + name)
    if not ok:
        failed.append(name)
if failed:
    print('FINAL_PRODUCTION_STATIC_AUDIT=FAIL', file=sys.stderr)
    sys.exit(1)
print('FINAL_PRODUCTION_STATIC_AUDIT=PASS')

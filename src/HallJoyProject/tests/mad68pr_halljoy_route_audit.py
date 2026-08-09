#!/usr/bin/env python3
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
backend = (ROOT / 'HallJoy/backend.cpp').read_text(encoding='utf-8-sig')
registry = (ROOT / 'HallJoy/native_analog_backend_registry.cpp').read_text(encoding='utf-8-sig')
catalog = (ROOT / 'HallJoy/native_analog_backends.def').read_text(encoding='utf-8-sig')
render = (ROOT / 'HallJoy/keyboard_render.cpp').read_text(encoding='utf-8-sig')
app = (ROOT / 'HallJoy/app.cpp').read_text(encoding='utf-8-sig')
main = (ROOT / 'HallJoy/main.cpp').read_text(encoding='utf-8-sig')
resource = (ROOT / 'HallJoy/HallJoy.rc').read_text(encoding='utf-16')
uap_main = (ROOT.parent.parent / 'third_party/UniversalAnalogPluginFixed/main.cpp').read_text(encoding='utf-8-sig')
uap_patch = (ROOT.parent.parent / 'third_party/UniversalAnalogPluginFixed/tools/Apply-Soup-Madlions-Fix.ps1').read_text(encoding='utf-8-sig')
uap_standard_sun = (ROOT.parent.parent / 'third_party/UniversalAnalogPluginFixed/abiv1-pluswooting.sun').read_text(encoding='utf-8')
uap_native_sun = (ROOT.parent.parent / 'third_party/UniversalAnalogPluginFixed/abiv1-pluswooting-mad68native.sun').read_text(encoding='utf-8')
native_build = (ROOT.parent.parent / 'tools' / 'build.ps1').read_text(encoding='utf-8-sig')
mad = (ROOT / 'HallJoy/mad68pr_backend.cpp').read_text(encoding='utf-8-sig')

checks = {
    'MAD68 descriptor is in central catalog': 'Mad68ProR_GetNativeBackendDescriptor' in catalog,
    'MAD68 value enters common raw path': 'NativeAnalogBackends_ReadMilli(hidKeycode)' in backend,
    'MAD68 enters multi-device max arbitration': 'result.milli = std::max' in registry,
    'UAP stays available for other analogue keyboards': 'cache.wootingReady && modeCode != 0' in backend,
    'digital fallback stays blocked for native-owned HID': 'cache.allowFallback && !native.owned' in backend,
    'native UAP child excludes only capability-validated exact interfaces': 'halljoy_uap_native_hid_excluded' in uap_main and 'HALLJOY_UAP_NATIVE_HID_PATHS' in uap_main and 'UAP_EXCLUDE_HALLJOY_NATIVE=1' in uap_native_sun,
    'normal UAP target remains unchanged': 'UAP_EXCLUDE_HALLJOY_NATIVE=1' not in uap_standard_sun,
    'native build explicitly selects dedicated UAP': '-ExcludeMad68ProRNative' in native_build,
    'native UAP preserves baseline hotplug policy': 'UAP_DISABLE_HOTPLUG=1' in uap_native_sun and 'UAP_DISABLE_HOTPLUG=1' in uap_standard_sun,
    'native exclusion happens before Soup CreateFileW': 'HallJoy native analogue pre-open exclusion' in uap_patch and 'before CreateFileW' in uap_patch,
    'normal UAP initialization preserved': 'wootingInit = wooting_analog_initialise();' in backend,
    'normal embedded UAP preparation preserved': 'EmbeddedAnalogStack_Prepare' in main,
    'normal UAP resource preserved': 'universal_analog_abiv1.dll' in resource,
    'realtime curve path consumes common raw': 'float raw = ReadRaw01Cached(hidKeycode, cache);' in backend,
    'UI publication receives filtered analog': 'g_uiAnalogM[hid].store(newV' in backend,
    'blue keyboard render consumes standard UI analog': 'BackendUI_GetAnalogMilli(hid)' in render,
    'ViGEm report builder consumes filtered analog': 'static XUSB_REPORT BuildReportForPad' in backend and 'ReadFiltered01Cached' in backend,
    'ViGEm update receives built report': (
        'batch.reports[index] = report' in backend and
        'vigem_target_x360_update(g_client, pad, batch.reports' in backend),
    'routing and lifecycle use central manager': (
        'NativeAnalogBackends_PrepareRouting()' in app
        and 'NativeAnalogBackends_StartPhase(NativeAnalogStartPhase::AfterRawInput)' in app
        and 'NativeAnalogBackends_StopAll()' in app),
    'dedicated UAP applies validated routing before Soup CreateFileW': 'HallJoy native analogue pre-open exclusion' in uap_patch and '$hidSourceText.Insert($braceStart + 1, $preOpenBlock)' in uap_patch and 'halljoy_should_exclude_hid_interface(device_interface)' in uap_patch and 'HALLJOY_UAP_NATIVE_HID_IDS' not in uap_patch,
    'stale full-matrix key falls back per HID without dropping healthy keys': 'non-WASD per-key A0 starvation' in mad,
    'stale WASD falls back per HID and only dead global transport re-arms': 'W/A/S/D per-key scheduler starvation' in mad and 'global A0 transport is dead' in mad,
    'ownership freshness is explicitly guarded': 'CaptureAnalogSnapshot' in mad and 'relinquish this HID usage' in mad,
    'leading packet mismatch waits for post-edge A0': 'waiting for a true post-edge A0' in mad,
    'lost A8 ACK can be confirmed semantically': 'kA8SemanticEvidenceMinFresh' in mad and 'forced-sweep semantic evidence confirms execution' in mad,
    'startup snapshot cannot directly grant full ownership': 'steady-state remains UNCONFIRMED' in mad and 'STEADY-STATE A0 CONFIRMED' in mad,
    'exit watchdog performs idempotent A9 recovery': 'Mad68ProR_EmergencyRestoreInputOnce' in mad and 'mad68pr_emergency_A9_write_sent' in (ROOT / 'HallJoy/debug_log.cpp').read_text(encoding='utf-8-sig'),
}
failed = [name for name, ok in checks.items() if not ok]
for name, ok in checks.items():
    print(f"{'PASS' if ok else 'FAIL'}: {name}")
if failed:
    print('\nHALLJOY_ROUTE_AUDIT=FAIL')
    for name in failed:
        print(' - ' + name)
    sys.exit(1)
print('\nHALLJOY_ROUTE_AUDIT=PASS')
print('route=MAD68 descriptor -> NativeAnalogBackends_ReadMilli -> curve -> UI + ViGEm')

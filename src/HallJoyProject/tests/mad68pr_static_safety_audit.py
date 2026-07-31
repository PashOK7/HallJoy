#!/usr/bin/env python3
from pathlib import Path
import re
import sys
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]
backend = (ROOT / 'HallJoy/mad68pr_backend.cpp').read_text(encoding='utf-8-sig')
protocol_h = (ROOT / 'HallJoy/mad68pr_protocol.h').read_text(encoding='utf-8-sig')
protocol_cpp = (ROOT / 'HallJoy/mad68pr_protocol.cpp').read_text(encoding='utf-8-sig')
app = (ROOT / 'HallJoy/app.cpp').read_text(encoding='utf-8-sig')
backend_main = (ROOT / 'HallJoy/backend.cpp').read_text(encoding='utf-8-sig')
registry = (ROOT / 'HallJoy/native_analog_backend_registry.cpp').read_text(encoding='utf-8-sig')
catalog = (ROOT / 'HallJoy/native_analog_backends.def').read_text(encoding='utf-8-sig')
main = (ROOT / 'HallJoy/main.cpp').read_text(encoding='utf-8-sig')
resource = (ROOT / 'HallJoy/HallJoy.rc').read_text(encoding='utf-16')
uap_root = ROOT.parent.parent / 'third_party' / 'UniversalAnalogPluginFixed'
uap_main = (uap_root / 'main.cpp').read_text(encoding='utf-8-sig')
uap_standard_sun = '\n'.join((uap_root / name).read_text(encoding='utf-8') for name in [
    'abiv0.sun', 'abiv0-pluswooting.sun', 'abiv1.sun', 'abiv1-pluswooting.sun'])
uap_native_sun = '\n'.join((uap_root / name).read_text(encoding='utf-8') for name in [
    'abiv0-mad68native.sun', 'abiv1-pluswooting-mad68native.sun'])
uap_build = (uap_root / 'tools/build_fixed_plugin.ps1').read_text(encoding='utf-8-sig')
uap_apply = (uap_root / 'tools/Apply-Soup-Madlions-Fix.ps1').read_text(encoding='utf-8-sig')
native_build = (ROOT.parent.parent / 'tools' / 'build.ps1').read_text(encoding='utf-8-sig')
uap_soup_patch = (uap_root / 'tools/Apply-Soup-Madlions-Fix.ps1').read_text(encoding='utf-8-sig')
project_path = ROOT / 'HallJoy/HallJoy.vcxproj'
ET.parse(project_path)
project = project_path.read_text(encoding='utf-8-sig')

errors: list[str] = []

def require(cond: bool, message: str) -> None:
    if not cond:
        errors.append(message)

# Firmware/protocol invariants.
require('constexpr std::uint8_t kArmOpcode = 0xA8;' in protocol_h, 'A8 constant changed/missing')
require('constexpr std::uint8_t kRestoreInputOpcode = 0xA9;' in protocol_h, 'A9 constant changed/missing')
require('constexpr std::uint16_t kAnalogFullScale = 1600;' in protocol_h, 'full scale changed/missing')
require('constexpr std::uint16_t kAuditedBcdDevice = 0x0102;' in protocol_h, 'firmware gate missing')
require('constexpr std::size_t kPhysicalKeyCount = 68;' in protocol_h, '68-key count missing')
require('constexpr std::size_t kPublishedKeyCount = 67;' in protocol_h, '67 HID-key count missing')
require(protocol_h.count('{ 0x') >= 68, 'descriptor table appears incomplete')
require('{  7, 63, 0x00, "Fn", { 0xF0, 0xFF, 0x01 } }' in protocol_h, 'Fn diagnostic descriptor missing')
for marker in [
    '{ 64, 17, 0x1A, "W", { 0x10, 0x00, 0x1A } }',
    '{  9, 31, 0x04, "A", { 0x10, 0x00, 0x04 } }',
    '{ 55, 32, 0x16, "S", { 0x10, 0x00, 0x16 } }',
    '{ 10, 33, 0x07, "D", { 0x10, 0x00, 0x07 } }',
]:
    require(marker in protocol_h, f'WASD descriptor missing: {marker}')
require('(static_cast<std::uint16_t>(payload[4]) << 8) | payload[5]' in protocol_cpp,
        'A0 big-endian raw decode changed')
require('if (raw > kAnalogFullScale) return false;' in protocol_cpp, 'A0 range validation missing')

# The private Session::Send boundary must reject every opcode except A8/A9.
require('if (opcode != mad68pr::kArmOpcode && opcode != mad68pr::kRestoreInputOpcode)' in backend,
        'runtime opcode allow-list missing')
require('SAFETY BLOCK: refused opcode' in backend, 'blocked-opcode logging missing')
require('SendCommand(session, strategy, mad68pr::kArmOpcode' in backend, 'A8 arm call missing')
require(backend.count('SendCommand(session, strategy, mad68pr::kRestoreInputOpcode') >= 2,
        'serialized A9 normalization/restore calls missing')
for call in re.findall(r'(?:SendCommand|\.Send)\([^;]+?\)', backend):
    if re.search(r',\s*0x[0-9A-Fa-f]{2}\s*(?:,|\))', call):
        errors.append(f'direct numeric opcode send found: {call[:120]}')

# Finite, conservative recovery and emergency publication.
require('std::array<Strategy, 13>' in backend, '13-attempt finite matrix missing')
require('recoveryCycles >= kMaxRecoveryCyclesPerWindow' in backend and 'kRecoveryWindowMs = 60000' in backend, 'finite windowed automatic recovery limit missing')
require('safe A8/A9 strategies exhausted' in backend, 'exhaustion handling missing')
require('interrupt-caps-normal-strict-retry' in backend,
        'hardware-confirmed primary transaction does not get a clean retry before speculative fallbacks')
require('best-effort A9 secondary safety net via audited primary transport' in backend,
        'failed fallback does not finish with hardware-confirmed primary A9')
wm_create_start = app.find('case WM_CREATE:')
wm_create_end = app.find('case WM_INPUT:', wm_create_start)
wm_create = app[wm_create_start:wm_create_end]
require(wm_create_start >= 0 and wm_create_end > wm_create_start and
        wm_create.find('NativeAnalogBackends_PrepareRouting()') >= 0 and
        wm_create.find('NativeAnalogBackends_PrepareRouting()') < wm_create.find('Backend_Init();') and
        wm_create.find('g_backendReady = AppStartBackendDependents(rawInputRegistered') >
        wm_create.find('RegisterRawInputDevices(rid, 2'),
        'native routing/lifecycle order must be prepare -> UAP/backend -> Raw Input -> MAD68 start phase')
require('NativeAnalogBackends_StartPhase(NativeAnalogStartPhase::AfterRawInput)' in app,
        'transactional dependent-start helper must own the MAD68 phase')
require('Mad68ProR_GetNativeBackendDescriptor' in catalog and
        'NativeAnalogStartPhase::AfterRawInput' in backend,
        'MAD68 descriptor must defer A8-capable worker until target Raw Input registration')
require('AddressedAnalog_GetNativeBackendDescriptor' in catalog and
        'NativeAnalogStartPhase::AfterRealtime' in (ROOT / 'HallJoy/addressed_analog_backend.cpp').read_text(encoding='utf-8-sig') and
        'NativeAnalogBackends_StartPhase(NativeAnalogStartPhase::AfterRealtime)' in app and
        wm_create.find('g_backendReady = AppStartBackendDependents(rawInputRegistered') >
        wm_create.find('Backend_Init();'),
        'Addressed Analog must be capability-routed before UAP and started in the post-realtime phase')
require('HallJoy native analogue pre-open exclusion' in uap_soup_patch and
        'UAP_EXCLUDE_HALLJOY_NATIVE' in uap_soup_patch and
        'HALLJOY_UAP_NATIVE_HID_IDS' in uap_soup_patch and
        '$hidSourceText.Insert($braceStart + 1, $preOpenBlock)' in uap_soup_patch,
        'dedicated UAP does not apply validated native routing before Soup CreateFileW')
require('universal native continuation enabled; UAP unavailable for this run' in main and
        '#if defined(HALLJOY_MAD68PR_NATIVE)' in main and
        'Failed to prepare the private crash-isolated Universal Analog Plugin.' in main,
        'universal target must continue to native capability classification while ordinary builds remain fatal on UAP preparation failure')
require('if (Mad68ProR_IsProtocolDevicePresent())' not in main,
        'UAP preparation fallback is still hard-wired to one MAD68 model instead of the universal native route set')
require('NativeAnalogBackends_StartPhase(NativeAnalogStartPhase::BeforeUap)' in backend_main and
        'NativeAnalogBackends_AnyProtocolDevicePresent()' in backend_main,
        'universal startup must use the protocol catalog rather than one MAD68-specific readiness branch')
require('kDigitalAnalogDeadlineMs = 1000' in backend, 'steady-state mismatch deadline changed')

require('MaybeConfirmSteadyStateFromAnalogOnlyEdge' in backend, 'A0-only steady-state fallback is missing')
require('IsPostSweepAnalogProof' in backend and 'post-grace A0 analogue edge proof' in backend,
        'post-sweep threshold-crossing proof markers are missing')
require('kSteadyProofMinDelta = 64' in protocol_h, 'steady proof noise floor changed')
require('kForcedSweepGraceMs = 4500' in backend and 'kForcedSweepDigitalDeadlineMs = 1600' in backend,
        'firmware forced-sweep timing guard missing')
require('kGlobalStreamAliveMs = 350' in backend and 'kSchedulerStarvationDeadlineMs = 3000' in backend,
        'one-changed-slot scheduler extension missing')
require('per-key A0 delayed while global stream is alive' in backend,
        'scheduler-aware troubleshooting log missing')
require('Keep the same overlapped ReadFile pending' in backend and 'std::unique_ptr<HidIoOperation> pendingRead_' in backend,
        'persistent overlapped input read missing')
require('kAllReleasedStableMs = 500' in backend, '500 ms all-keys-up baseline guard missing')
require('Capture before A8' in backend, 'pre-A8 snapshot baseline fix missing')
require('fresh 68/68 startup snapshot received' in backend and 'steady-state remains UNCONFIRMED' in backend, 'startup snapshot/steady-state distinction missing')
require('fresh W/A/S/D received after A8' in backend, 'emergency W/A/S/D validation missing')
require('PublishMode::EmergencyWasd' in backend and 'ModeOwnsHid' in backend,
        'per-mode publication ownership missing')
require('activation snapshot salvage succeeded' in backend and 'kStreamSalvageAgeMs' in backend and 'steady_state=UNCONFIRMED' in backend,
        'current-session emergency snapshot salvage missing')
require('SynchroniseDigitalWatch(digital, false)' in backend and 'SynchroniseDigitalWatch(digital, true)' in backend,
        'stale digital-watch reset missing')
require('const bool urgent = mad68pr::IsWasdHid(hid);' in backend and 'request-recovery-WASD' in backend,
        'W/A/S/D correlation classification missing')
require('openedCandidate' in backend and 'trying next path' in backend,
        'multiple matching HID candidate failover missing')
require('Mad68ProR_NotifyKeyboardDeviceReset' in backend and 'Mad68ProR_NotifyKeyboardDeviceReset' in app,
        'Raw Input device-removal state reset missing')
require('g_digitalResetSeq.fetch_add' in backend and
        'for (auto& seq : g_digitalSeq)' not in backend[backend.find('void Mad68ProR_NotifyKeyboardDeviceReset'):backend.find('void Mad68ProR_NotifyKeyboardDeviceReset') + 700],
        'device reset still manufactures per-key release edges')
wm_device = app[app.find('case WM_DEVICECHANGE:'):app.find('case WM_TIMER:')]
require('g_mad68RawKeyboardCache.clear()' not in wm_device,
        'generic WM_DEVICECHANGE clears target Raw Input identity cache')
require('WM_INPUT_DEVICE_CHANGE' in app and 'g_mad68RawKeyboardCache.erase(changed);' in app,
        'target-scoped Raw Input arrival/removal cache lifecycle missing')
native_ready_start = backend_main.find('const bool nativeReady = NativeAnalogBackends_AnyProtocolDevicePresent();')
native_ready_region = backend_main[native_ready_start:native_ready_start + 700]
require(native_ready_start >= 0 and
        'NativeAnalogBackends_AnyProtocolDevicePresent()' in native_ready_region and
        'IsRunning()' not in native_ready_region,
        'unvalidated or merely running native workers must not hide UAP startup failures')
require('late device arrival while backend degraded' in app and 'g_lastMad68PresenceForBackendRetry' in app,
        'late-connect Backend_Init/ViGEm recovery missing')
require('return mad68pr::IsWasdHid(hid);' in backend, 'emergency mode is not restricted to W/A/S/D')
require('did not validate native A8/A9 framing; remaining passive' in backend, 'unvalidated protocol passive-only gate missing')
require('WM_DEVICECHANGE did not remove current MAD68 vendor path' in backend,
        'unrelated device-change protection missing')
require('final best-effort A9' in backend, 'shutdown A9 recovery missing')
require('Mad68ProR_EmergencyRestoreInputOnce' in backend and
        'watchdog emergency A9 result' in backend and
        'mad68pr_emergency_A9_write_sent' in (ROOT / 'HallJoy/debug_log.cpp').read_text(encoding='utf-8-sig'),
        'out-of-process crash-window A9 recovery missing')
require('STEADY-STATE A0 CONFIRMED' in backend and
        'post-sweep physical edge produced a fresh A0' in backend and
        'passive 68/68 snapshot available; emergency WASD only' in backend,
        'startup snapshot can still be mistaken for continuous steady-state')
require('W/A/S/D per-key scheduler starvation' in backend and
        'keeping other native keys' in backend and
        'global A0 transport is dead' in backend,
        'per-key starvation still restarts or disables the whole native backend')
require('raw64 ? 0u : 1u' in backend, 'report-ID/raw64 transport split missing')
require('HidD_SetOutputReport' in backend, 'control transport fallback missing')
require('previous == isKeyDown' in backend, 'Raw Input autorepeat suppression missing')
require('AnalogMatchesDigital' in backend and 'AnalogTransitionMatchesDigital' in protocol_cpp and 'pressFloor' in protocol_cpp,
        'meaningful digital/analog correlation missing')

require('digitalMs - sampleMs > kDigitalLeadToleranceMs' in backend and
        'W/A/S/D per-key scheduler starvation' in backend,
        'urgent stale WASD does not fall back per key')
require('CaptureAnalogSnapshot' in backend and
        'g_sampleSeqAtDigitalEvent' in backend and
        'if (sampleSeq > sampleSeqAtEdge) return true;' in backend and
        'Release-publish the digital sequence only after all edge snapshot fields' in backend,
        'digital/analogue edge correlation is not generation-based and race-safe')
require('GetAsyncKeyState(vk) & 0x8000' in backend,
        'a key held before Raw Input registration can still enter A8 service transition')
require('non-WASD per-key A0 starvation' in backend and
        'falling back only for this HID' in backend,
        'stale non-WASD can remain authoritative in ViGEm')
require('backgroundFullUpgradePending' in backend and
        'if (backgroundFullUpgradePending &&' in backend,
        'emergency mode can be promoted from an old full snapshot')

# Target-scoped digital diagnostics.
require('IsMad68RawKeyboard' in app and 'RIM_TYPEKEYBOARD' in app,
        'target-scoped Raw Input path missing')
require('Mad68ProR_NotifyKeyboardEvent(hid, isDown, false)' in app,
        'target digital forwarding missing')
require('Mad68ProR_NotifyKeyboardEvent(hidHint' not in backend_main,
        'generic all-keyboard hook forwards to MAD backend')

# Additive integration: never remove the normal UAP/Wooting stack.
require('HALLJOY_MAD68PR_NATIVE' in app, 'native-build app guard missing')
require('HallJoyMad68ProRNative' in project, 'native-build MSBuild property missing')
require('mad68pr_protocol.cpp' in project and 'mad68pr_backend.cpp' in project,
        'production sources missing from vcxproj')
require('EmbeddedAnalogStack_Prepare' in main, 'embedded UAP preparation was disabled')
require('universal_analog_abiv1.dll' in resource, 'embedded UAP resource was removed')
require('wootingInit = wooting_analog_initialise();' in backend_main, 'UAP/Wooting initialise path disabled')
require('UAP_EXCLUDE_HALLJOY_NATIVE' in uap_main and
        'halljoy_uap_native_hid_excluded' in uap_main and
        'HALLJOY_UAP_NATIVE_HID_IDS' in uap_main,
        'UAP dynamic native exclusion missing; native/UAP readers could race')
require('UAP_EXCLUDE_HALLJOY_NATIVE=1' not in uap_standard_sun,
        'normal UAP targets were globally changed to exclude native devices')
require(uap_native_sun.count('UAP_EXCLUDE_HALLJOY_NATIVE=1') == 2,
        'dedicated native UAP targets do not carry the routing gate')
require('[switch]$ExcludeMad68ProRNative' in uap_build and
        'abiv1-pluswooting-mad68native' in uap_build and
        '-ExcludeMad68ProRNative' in native_build,
        'native build does not explicitly select the dedicated UAP targets')
require(backend_main.count('wooting_analog_uninitialise();') >= 2, 'UAP/Wooting cleanup path disabled')
require('NativeAnalogBackends_ReadMilli(hidKeycode)' in backend_main and
        'Mad68ProR_GetNativeBackendDescriptor' in catalog,
        'MAD68 is not integrated into the common native analogue source path')
require('cache.allowFallback && !native.owned' in backend_main,
        'digital fallback is not guarded per authoritative native key')
require('result.milli = std::max' in registry and
        'cache.wootingReady && modeCode != 0' in backend_main,
        'registered native protocols do not preserve multi-analogue-device max arbitration')
require('cache.mad68Connected = Mad68ProR_IsConnected();' in backend_main,
        'MAD68 connection state not snapshotted on realtime tick')

require('UAP_DISABLE_HOTPLUG=1' in uap_native_sun,
        'native UAP must preserve the baseline HallJoy hotplug policy')
require('HallJoy native analogue pre-open exclusion' in uap_apply and
        'HALLJOY_UAP_NATIVE_HID_IDS' in uap_apply and
        'before CreateFileW' in uap_apply,
        'dedicated UAP does not apply dynamic native routing before Soup opens HID paths')

if errors:
    print('MAD68PR NATIVE STATIC SAFETY AUDIT FAILED')
    for error in errors:
        print(f' - {error}')
    sys.exit(1)

print('MAD68PR NATIVE STATIC SAFETY AUDIT PASSED')
print(f'root={ROOT}')
print('device opcode allow-list: A8, A9 only')
print('native mutation gate: exact 65-byte fingerprint + MAD68 layout identity + audited 0102 or reversible A9 ACK')
print('publication modes: full 67 HID keys or emergency real W/A/S/D')
print('UAP/Wooting: preserved and embedded')
print('strategies: hardware-confirmed primary + clean retry + 11 finite fallbacks; max 2 recovery cycles per 60-second window')
print('UAP/Wooting: preserved; only runtime-validated exact native VID/PID pairs are excluded before HID open')

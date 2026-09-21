from pathlib import Path
root = Path(__file__).resolve().parents[1] / 'HallJoy'
log = (root / 'support_log.cpp').read_text(encoding='utf-8-sig')
settings = (root / 'settings.cpp').read_text(encoding='utf-8-sig')
ini = (root / 'settings_ini.cpp').read_text(encoding='utf-8-sig')
ui = (root / 'keyboard_subpages.cpp').read_text(encoding='utf-8-sig')
overlay = (root / 'overlay_server.cpp').read_text(encoding='utf-8-sig')
assert 'g_diagnosticLogging{ false }' in settings
assert 's.searchCompleted && !s.analogSourceConnected' in log
assert 'TryAcquireSRWLockExclusive' in log and 'kQueueLines = 512' in log
assert 'kHistoryLines = 512' in log and '4 * 1024 * 1024' in log
assert 'MOVEFILE_REPLACE_EXISTING' in log
assert 'HidD_GetFeature' not in log and 'HidD_SetFeature' not in log
assert 'HidD_GetSerialNumberString' not in log
assert 't.lastAnalogError' in log and 't.pluginHostTransportError' in log
assert 'n.failedUpdates' in log and 'n.inputReportBytes' in log
assert 'overlay_perf.log' not in overlay and 'SupportLog_OverlaySummary(line)' in overlay
assert 'GLOB_ID_DIAGNOSTIC_LOGGING' in ui and 'GLOB_ID_HALLJOY_FOLDER' in ui
assert 'L"Enable logging"' in ui and 'L"Open HallJoy folder"' in ui
assert 'L"Continuous diagnostic logging"' not in ui
assert 'Off by default. Crash and missing-keyboard reports are automatic.' not in ui
assert 'const auto directory = AppPaths_DataRoot();' in ui
assert 'Settings_SetDiagnosticLogging(diagnosticLogging != 0)' in ini
required = ini.split('static bool ValidateProfileNumbers',1)[1].split('static bool SettingsIni_Load_Core',1)[0]
assert 'DiagnosticLogging' not in required, 'Application preference must not become a mandatory profile field'
assert 'if (saveWindow)\n    {\n        ok &= IniWriteI32(L"Main", L"DiagnosticLogging"' in ini
print('SUPPORT_LOG_STATIC_AUDIT=PASS')

# Native keyboard support must never force the verification trace into releases.
import xml.etree.ElementTree as ET
project=ET.parse(root/'HallJoy.vcxproj').getroot()
ns={'m':'http://schemas.microsoft.com/developer/msbuild/2003'}
for group in project.findall('m:ItemDefinitionGroup',ns):
 if not group.get('Condition'):
  definitions=group.find('m:ClCompile/m:PreprocessorDefinitions',ns)
  if definitions is not None:
   assert all(flag not in definitions.text for flag in ['HALLJOY_DEVICE_SUPPORT_LOG','HALLJOY_STABILITY_TRACE','HALLJOY_SINGLE_LOG_DIAGNOSTIC'])
assert '|| defined(HALLJOY_IROK_NA87_NATIVE)' not in log
print('SUPPORT_LOG_RELEASE_ISOLATION=PASS')

# Ordinary crash reports retain error identity but never raw stack/register data.
crash = (root / 'debug_log.cpp').read_text(encoding='utf-8-sig')
assert '#if defined(HALLJOY_DIAGNOSTIC)\n        WriteDiagnosticContext(file, ep);\n#else' in crash
assert 'memory_context=omitted_in_production' in crash
print('PRODUCTION_CRASH_MEMORY_PRIVACY=PASS')

# Crash destination is resolved before a crash, never beside the ordinary EXE.
assert 'const std::wstring& crashPath = g_productionCrashPath;' in crash
assert 'g_productionCrashPath = AppPaths_DataRoot()' in crash
assert 'BuildPathNearExe(L"HallJoyCrash.txt")' not in crash
print('PRODUCTION_CRASH_APPDATA_DESTINATION=PASS')

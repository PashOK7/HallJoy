"""M0 integration boundaries, complementary to the real output-owner test."""
from pathlib import Path
root=Path(__file__).resolve().parents[3]
hall=root/'src/HallJoyProject/HallJoy'
backend=(hall/'backend.cpp').read_text(encoding='utf-8-sig')
subpages=(hall/'keyboard_subpages.cpp').read_text(encoding='utf-8-sig')
remap=(hall/'remap_panel.cpp').read_text(encoding='utf-8-sig')
supervisor=(hall/'runtime_supervisor.cpp').read_text(encoding='utf-8-sig')
output=(hall/'midi_output.cpp').read_text(encoding='utf-8-sig')
assert 'outputs = halljoy::outputs::Start' in backend
assert 'if (initIssues == BackendInitIssue_None && !outputs.admitted)' in backend
assert backend.index('MidiOutput_Stop()') < backend.index('ResetPersistentFilteredCache();',backend.index('bool Backend_Init()'))
assert 'MidiOutput_Reconcile()' in supervisor and 'WaitOnAddress(&g_optionalWake' in supervisor
assert 'RuntimeSupervisor_NotifyOptionalOutputs();' in supervisor[supervisor.index('StopResult RuntimeSupervisor_Stop()'):]
assert 'if (showMidi) { midiData.emplace(); MidiOutput_GetTelemetry(*midiData); }' in subpages
assert 'std::optional<MidiTelemetry> midiData;' in subpages
assert 'if (config->enabled && (!st->displayedMidi || !st->displayedMidi->enabled' in remap
assert 'if (!NeedsAnalog()) return ports;' in output
assert 'std::unique_ptr<SessionState> session;' in output
assert 'std::unique_lock<std::mutex> source(producer, std::try_to_lock);' in output
assert 'WaitForSingleObject(thread, 0)' in output
assert 'bool enabled = false;' in (hall/'midi_profile.h').read_text()
settings=(hall/'settings_ini.cpp').read_text(encoding='utf-8-sig')
assert 'VirtualGamepadsEnabled' in settings
assert 'int padsDef = profileOnly ? 1' in settings and 'int padsEnabledDef = profileOnly ? 1' in settings
runtime_settings=(hall/'settings.cpp').read_text(encoding='utf-8-sig')
assert 'g_virtualGamepadCount{ 1 }' in runtime_settings and 'g_virtualGamepadsEnabled{ true }' in runtime_settings
print('MIDI_OPTIONAL_STATIC_AUDIT=PASS lazy_owner independent_start conditional_ui bounded_reap')

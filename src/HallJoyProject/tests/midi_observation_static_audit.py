"""Explicit provenance/timing/history integration; never certify unseen firmware samples."""
from pathlib import Path
root=Path(__file__).resolve().parents[3];hall=root/'src/HallJoyProject/HallJoy'
main=(hall/'main.cpp').read_text(encoding='utf-8-sig')
assert 'if(!d->getObservations)' in main
backend=(hall/'backend.cpp').read_text(encoding='utf-8-sig')
assert 'NativeAnalogBackends_VisitObservations(sink)' in backend
assert 'source.getObservations(sink)' in backend
assert 'observation::wake.store(&RealtimeLoop_NotifyInputChanged' in backend
physical=(hall/'physical_analog_state.h').read_text(encoding='utf-8-sig')
assert 'if(capture) history_.Publish' in physical and 'history_.Clear();' in physical
output=(hall/'midi_output.cpp').read_text(encoding='utf-8-sig')
assert 'work.observed.Replay' in output and 'work.history->Drain' in output
assert 'work.history->Discard();' in output and 'midi.history_loss' in output
assert 'halljoy::observation::SetCapture(false)' in output
journal=(hall/'analog_observation.h').read_text(encoding='utf-8-sig')
assert 'compare_exchange_weak' in journal and 'discardBefore_' in journal
assert 'std::unique_lock' not in journal[journal.index('class Journal'):journal.index('inline std::atomic<bool> enabled')]
snapshot=(hall/'analog_observation_snapshot.h').read_text(encoding='utf-8-sig')
assert 'Timing::HostSnapshot' in snapshot and '++gaps' in snapshot
assert 'RetainSnapshot' in snapshot
client=(hall/'analog_host_client.cpp').read_text(encoding='utf-8-sig')
assert 'tracker->Frame' in client and 'observationHistory.Capture' in client
provider=(hall/'analog_observation_provider.h').read_text(encoding='utf-8-sig')
assert 'DeviceFlag_DuplicateSafeId' in provider
wire=(hall/'analog_host_observation_history.h').read_text(encoding='utf-8-sig')
assert 'OpenFileMapping' not in wire and 'CreateFileMappingW' in wire
assert 'InterlockedExchange64(&shared.observationHistoryHandle,0)' in wire
assert 'SetObservationCapture' in client
assert 'expectedHostPid.load()' in client
app=(hall/'app.cpp').read_text(encoding='utf-8-sig')
assert 'if (!fileOnlyProfileStartup) halljoy::keyboard_canvas::WarmUpAsync();' in app
assert app.index('fileOnlyProfileStartup = wcsstr') < app.index('GlobalProfiles_InitializeStartup();')
print('MIDI_OBSERVATION_STATIC_AUDIT=PASS explicit_routes final_catalog history_loss zero_disabled snapshot_quality')

#include "profile_ini.h"
#include "ini_util.h"
#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>

// Save paths are not exercised in this loader-only executable.
HallJoyPersistence::SaveResult IniUtil_SaveAtomic(const wchar_t*, IniUtilWriteCallback,
    IniUtilValidateCallback, void*) { std::abort(); }
void IniUtil_ReportSaveFailure(const wchar_t*, const wchar_t*, const HallJoyPersistence::SaveResult&) { std::abort(); }
int main() {
    namespace fs = std::filesystem;
    const auto root = fs::temp_directory_path() / ("halljoy-profile-loader-" + std::to_string(GetCurrentProcessId()));
    assert(fs::create_directory(root));
    const auto path = root / "fixture.ini";
    Bindings_SetAxisPlus(Axis::LX, 26);
    assert(!Profile_LoadIni(path.c_str()));
    for (const auto invalid : {"", "unrelated", "[Pad1_Axes]\nUnrelated=7\n", "[Pad1_Axes]\nLX_Plus=65543\n",
            "[Pad1_Axes]\nLX_Plus=-1\n", "[Pad1_Axes]\nLX_Plus=7junk\n",
            "[Pad1_Buttons]\nA=7,65543\n"}) {
        { std::ofstream file(path); file << invalid; }
        assert(!Profile_LoadIni(path.c_str()));
        assert(Bindings_GetAxis(Axis::LX).plusHid()==26);
    }
    { std::ofstream file(path); file << "[Pad1_Buttons]\nA=";
      for (unsigned i=1; i<halljoy::keycode::kCount; ++i) file << (i==1?"":",") << i;
      file << "\n"; }
    assert(Profile_LoadIni(path.c_str()));
    for (unsigned i=1; i<halljoy::keycode::kCount; ++i)
        assert(Bindings_ButtonHasHid(GameButton::A, static_cast<uint16_t>(i)));
    // MIDI belongs to the transactional binding snapshot and exported bundles.
    halljoy::midi::Edit([](halljoy::midi::Profile& midi) {
        midi.enabled = true; midi.paletteChannel = 15; midi.paletteOctave = -1;
        midi.keys[halljoy::keycode::kFn] = 2048; midi.keys[4] = 1;
        midi.port = L"HallJoy \u041C\u0443\u0437\u044B\u043A\u0430"; midi.touchVersion=1; midi.touch = 2; midi.attackMilli = 350; midi.depthPressure = false;
        midi.releaseMilli = 120; midi.noiseMilli = 5; midi.fullSpeedMilli = 42000;
        midi.velocityCurve = {0, 200, 500, 900, 1000};
    });
    BindingsSnapshot snapshot;
    Bindings_Capture(snapshot);
    { std::ofstream file(path); file << "[HallJoyPersistence]\nSchemaVersion=1\nKind=ProfileSettings\n"; }
    assert(Profile_WriteBindingsSnapshot(path.c_str(), snapshot));
    BindingsSnapshot parsed;
    assert(Profile_PrepareIni(path.c_str(), parsed));
    assert(parsed.midi->enabled && parsed.midi->paletteChannel == 15 && parsed.midi->paletteOctave == -1);
    assert(parsed.midi->keys[4] == 1 && parsed.midi->keys[halljoy::keycode::kFn] == 2048);
    assert(parsed.midi->port == snapshot.midi->port && parsed.midi->touch == 2 && parsed.midi->attackMilli == 350 && !parsed.midi->depthPressure);
    assert(parsed.midi->releaseMilli == 120 && parsed.midi->noiseMilli == 5);
    assert(parsed.midi->fullSpeedMilli == 42000 && parsed.midi->touchVersion == 1);
    assert(parsed.midi->velocityCurve == snapshot.midi->velocityCurve);
    // Malformed metadata refuses the complete profile without changing live state.
    for (const auto invalid : {"Enabled=2", "Octave=-2", "Octave=10", "Channel=16", "K4=2049", "K4=-1", "K4=7junk", "TouchVersion=0", "TouchVersion=3", "ReleaseMilli=200", "NoiseMilli=51", "FullSpeedMilli=1000000001", "Velocity25=1001", "Velocity25=800", "Touch=3", "AttackMilli=50", "AttackMilli=801", "DepthPressure=2", "PortUtf16=000A", "PortUtf16=00FFx"}) {
        { std::ofstream file(path); file << "[Pad1_Axes]\nLX_Plus=26\n[Midi]\n" << invalid << "\n"; }
        assert(!Profile_LoadIni(path.c_str()));
        assert(halljoy::midi::Binding(4) == 1 && halljoy::midi::Get()->enabled);
    }
    { std::ofstream file(path); file << "[Pad1_Axes]\nLX_Plus=26\n[Midi]\nEnabled=1\nK4=61\nTouch=2\n"; }
    assert(Profile_PrepareIni(path.c_str(), parsed));
    assert(parsed.midi->touch == 2 && parsed.midi->fullSpeedMilli == 0);
    assert(parsed.midi->releaseMilli == 50 && parsed.midi->noiseMilli == 2 && parsed.midi->touchVersion == 1);
    assert(parsed.midi->velocityAxisMaximum == 70000);
    auto editable=std::make_shared<halljoy::midi::Profile>(*snapshot.midi);
    halljoy::midi::MakeEditableCurve(*editable);
    editable->attackMilli=950;editable->velocitySpeed={5000,20000,72000,112000,190000};
    editable->velocityCurve={0,125,400,750,958};editable->velocityAxisMaximum=60000;
    snapshot.midi=editable;
    { std::ofstream file(path); file << "[HallJoyPersistence]\nSchemaVersion=1\nKind=ProfileSettings\n"; }
    assert(Profile_WriteBindingsSnapshot(path.c_str(),snapshot) && Profile_PrepareIni(path.c_str(),parsed));
    assert(halljoy::midi::SamePerformance(*editable,*parsed.midi) && parsed.midi->velocityAxisMaximum==60000);
    for(const auto invalid:{"VelocityX1=0","VelocityY4=1","VelocityAxisMaximum=0"}) {
        {std::ofstream file(path);file<<"[Pad1_Axes]\nLX_Plus=26\n[Midi]\nTouchVersion=2\n"<<invalid<<"\n";}
        assert(!Profile_PrepareIni(path.c_str(),parsed));
    }
    // Previously valid v2 files can omit individual coordinates.
    { std::ofstream file(path); file << "[Pad1_Axes]\nLX_Plus=26\n[Midi]\nTouchVersion=2\nVelocityX3=93000\n"; }
    assert(Profile_PrepareIni(path.c_str(), parsed));
    assert(parsed.midi->velocitySpeed[3] == 93000 && parsed.midi->velocitySpeed[4] == 100000);
    // Replacing a bundle removes stale MIDI entries; legacy profiles reset MIDI to absent.
    { std::ofstream file(path); file << "[HallJoyPersistence]\nSchemaVersion=1\nKind=ProfileSettings\n"; }
    assert(Profile_WriteBindingsSnapshot(path.c_str(), snapshot));
    snapshot.midi = halljoy::midi::defaults;
    assert(Profile_WriteBindingsSnapshot(path.c_str(), snapshot));
    assert(Profile_PrepareIni(path.c_str(), parsed));
    assert(!parsed.midi->enabled && !parsed.midi->keys[4] && !parsed.midi->keys[halljoy::keycode::kFn]);
    { std::ofstream file(path); file << "[Pad1_Axes]\nLX_Plus=26\n"; }
    assert(Profile_LoadIni(path.c_str()));
    const auto withoutMidi = halljoy::midi::Get();
    assert(!withoutMidi->enabled && !halljoy::midi::Binding(4));
    assert(withoutMidi->touchVersion == 2 && withoutMidi->velocitySpeed.back() == 70000 &&
        withoutMidi->velocityAxisMaximum == 70000);
    fs::remove(path); fs::remove(root);
    std::cout << "PROFILE_LOADER_WINDOWS_TEST=PASS invalid_unchanged=1 full_domain=1\n";
}

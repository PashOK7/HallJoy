#include "../HallJoy/bindings.h"
#include "../HallJoy/binding_actions.h"
#include <cassert>
#include <iostream>

int main() {
    using namespace halljoy;
    assert(!midi::Get()->enabled);
    assert(midi::NoteName(0) == L"C-1" && midi::NoteName(60) == L"C4" && midi::NoteName(127) == L"G9");
    assert(midi::NoteName(128).empty());
    for (unsigned address = 0; address < midi::kAddressCount; ++address) {
        const auto action = BindingAction_Midi(address);
        assert(BindingAction_IsMidi(action));
        assert(BindingAction_MidiAddress(action) == address);
    }
    std::wstring decoded;
    const std::wstring unicodePort = L"HallJoy \u041C\u0443\u0437\u044B\u043A\u0430";
    assert(midi::DecodePort(midi::EncodePort(unicodePort), decoded) && decoded == unicodePort);
    assert(!midi::DecodePort(L"000A", decoded) && !midi::DecodePort(L"0000", decoded));
    assert(!midi::DecodePort(L"abcd", decoded) && !midi::DecodePort(L"1", decoded));
    assert(!BindingAction_IsMidi(BindAction::Btn_A));
    assert(!BindingAction_IsMidi(static_cast<BindAction>(0x1800)));
    midi::Edit([](midi::Profile& profile) { profile.enabled = true; midi::PianoPreset(profile); });
    const auto piano = midi::Get();
    unsigned count = 0;
    for (auto value : piano->keys) { if (value) { ++count; assert(value >= 61 && value <= 85); } }
    assert(count == 25 && piano->keys[0x1D] == 61 && piano->keys[0x14] == 73);
    assert(piano->keys[0x0C] == 85);
    // Immutable snapshots survive edits; maximum address and extended HID survive capture/apply.
    midi::Bind(keycode::kFn, 2048);
    assert(piano->keys[keycode::kFn] == 0 && midi::Binding(keycode::kFn) == 2048);
    Bindings_SetAxisPlus(Axis::LX, 0x1D);
    BindingsSnapshot saved;
    Bindings_Capture(saved);
    midi::Bind(keycode::kFn, 0);
    Bindings_SetAxisPlus(Axis::LX, 0);
    Bindings_Apply(saved);
    assert(midi::Binding(keycode::kFn) == 2048 && Bindings_GetAxis(Axis::LX).plusHid() == 0x1D);
    // Invalid edits are no-ops; all-action clear includes the MIDI metadata.
    midi::Bind(keycode::kFn, 2049);
    assert(midi::Binding(keycode::kFn) == 2048);
    Bindings_ClearHid(0x1D);
    assert(!midi::Binding(0x1D) && !Bindings_IsHidBound(0x1D));
    assert(midi::Binding(keycode::kFn) == 2048);
    // MIDI runtime adds bound notes to provider demand / configured key blocking.
    assert(Bindings_IsHidBound(keycode::kFn));
    for (int octave = -1; octave <= 9; ++octave)
        for (int channel = 0; channel < 16; ++channel) {
            midi::Profile config;
            config.paletteOctave = octave; config.paletteChannel = channel;
            midi::PianoPreset(config);
            for (unsigned value : config.keys)
                if (value) { assert(value <= 2048); assert((value - 1) / 128 == static_cast<unsigned>(channel)); }
        }
    // Removing pad zero compacts the other pad without touching MIDI assignments.
    Bindings_SetTriggerForPad(1, Trigger::LT, 7);
    Bindings_RemovePadAndCompact(0, 2);
    assert(Bindings_GetTriggerForPad(0, Trigger::LT) == 7);
    assert(Bindings_GetTriggerForPad(1, Trigger::LT) == 0);
    assert(midi::Binding(keycode::kFn) == 2048);
    std::cout << "MIDI_SETUP_TEST=PASS presets=176 immutable=1 independent=1 compact_pad_zero=1\n";
}

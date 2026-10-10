#include "input_config_summary.h"

#include "analog_key_codes.h"
#include "bindings.h"
#include "key_settings.h"
#include "settings.h"

#include <algorithm>
#include <bitset>
#include <cstdio>
#include <utility>
#include <vector>

namespace {
unsigned Permille(float value) noexcept {
    if (!(value > 0.0f)) return 0;
    if (value >= 1.0f) return 1000;
    return static_cast<unsigned>(value * 1000.0f + 0.5f);
}
} // namespace

void InputConfig_Summary(char* text, std::size_t capacity) noexcept {
    if (!text || !capacity) return;
    text[0] = 0;
    try {
        const bool globalInvert = Settings_GetInputInvert();
        const int pads = std::clamp(Settings_GetVirtualGamepadCount(), 1, BINDINGS_MAX_GAMEPADS);
        std::vector<std::pair<std::uint16_t, KeyDeadzone>> custom;
        KeySettings_Enumerate(custom);
        unsigned uniqueKeys = 0, uniqueInverted = 0;
        for (const auto& entry : custom)
            if (entry.second.useUnique) { ++uniqueKeys; uniqueInverted += entry.second.invert ? 1u : 0u; }
        const auto inverted = [&](std::uint16_t hid) {
            return KeySettings_GetUseUnique(hid) ? KeySettings_Get(hid).invert : globalInvert;
        };
        unsigned axisDirections = 0, axisInverted = 0, axisSameKey = 0;
        unsigned triggers = 0, triggersInverted = 0, buttonKeys = 0, buttonsInverted = 0;
        for (int pad = 0; pad < pads; ++pad) {
            for (const Axis axis : {Axis::LX, Axis::LY, Axis::RX, Axis::RY}) {
                const AxisBinding binding = Bindings_GetAxisForPad(pad, axis);
                for (const auto& side : {binding.minusHids, binding.plusHids})
                    for (const std::uint16_t hid : side)
                        if (hid) { ++axisDirections; axisInverted += inverted(hid) ? 1u : 0u; }
                for (const std::uint16_t hid : binding.minusHids)
                    if (hid && std::find(binding.plusHids.begin(), binding.plusHids.end(), hid) != binding.plusHids.end())
                        { ++axisSameKey; break; }
            }
            for (const Trigger trigger : {Trigger::LT, Trigger::RT})
                if (const std::uint16_t hid = Bindings_GetTriggerForPad(pad, trigger)) {
                    ++triggers; triggersInverted += inverted(hid) ? 1u : 0u;
                }
            std::bitset<halljoy::keycode::kCount> keys;
            for (int button = 0; button <= static_cast<int>(GameButton::DpadRight); ++button)
                for (int chunk = 0; chunk < Bindings_GetButtonMaskChunkCount(); ++chunk) {
                    const std::uint64_t mask = Bindings_GetButtonMaskChunkForPad(pad, static_cast<GameButton>(button), chunk);
                    for (unsigned bit = 0; bit < 64; ++bit) {
                        const std::size_t hid = static_cast<std::size_t>(chunk) * 64u + bit;
                        if ((mask >> bit & 1u) && hid > 0 && hid < keys.size()) keys.set(hid);
                    }
                }
            for (std::size_t hid = 1; hid < keys.size(); ++hid)
                if (keys.test(hid)) {
                    ++buttonKeys; buttonsInverted += inverted(static_cast<std::uint16_t>(hid)) ? 1u : 0u;
                }
        }
        snprintf(text, capacity,
            "virtual=%d pads=%d global_invert=%d global_curve=%u global_low=%u global_high=%u global_cap=%u "
            "unique_keys=%u unique_inverted=%u axis_directions=%u axis_inverted=%u axis_same_key=%u "
            "triggers=%u triggers_inverted=%u button_keys=%u buttons_inverted=%u",
            Settings_GetVirtualGamepadsEnabled() ? 1 : 0, pads, globalInvert ? 1 : 0,
            static_cast<unsigned>(Settings_GetInputCurveMode()), Permille(Settings_GetInputDeadzoneLow()),
            Permille(Settings_GetInputDeadzoneHigh()), Permille(Settings_GetInputOutputCap()),
            uniqueKeys, uniqueInverted, axisDirections, axisInverted, axisSameKey,
            triggers, triggersInverted, buttonKeys, buttonsInverted);
    } catch (...) {
        text[0] = 0;
    }
}

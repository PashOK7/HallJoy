#pragma once
#include "midi_output.h"
#include "custom_page_controls.h"
#include "gamepad_render.h"
#include "ui_theme.h"
#include "win_util.h"
#include <algorithm>

inline int MidiTester_Height(HWND window, const MidiTelemetry& telemetry) {
    const int rows = std::min<unsigned>(8, telemetry.generatedActive);
    return WinUtil_ScalePx(window, 236 + std::max(1, rows) * 36);
}
inline void MidiTester_Draw(HWND window, HDC dc, const RECT& card, const MidiTelemetry& telemetry) {
    auto scale = [&](int px) { return WinUtil_ScalePx(window, px); };
    Gdiplus::Graphics graphics(dc);
    graphics.SetSmoothingMode(Gdiplus::SmoothingModeHighQuality);
    CustomPage_DrawRoundRect(graphics, card, UiTheme::Color_ControlBg(), UiTheme::Color_Border(), (float)scale(8));
    const int x = card.left + scale(10), right = card.right - scale(10);
    int y = card.top + scale(8);
    auto line = [&](const std::wstring& text, COLORREF color = UiTheme::Color_Text()) {
        RECT rect{x, y, right, y + scale(18)};
        CustomPage_DrawText(dc, text.c_str(), rect, color, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        y += scale(20);
    };
    line(L"MIDI Tester");
    line(MidiOutput_Status(), UiTheme::Color_TextMuted());
    line(L"Computed notes: " + std::to_wstring(telemetry.generatedActive) + L"    Sent notes: " + std::to_wstring(telemetry.sentActive) +
        L"    Messages sent: " + std::to_wstring(telemetry.sentMessages));
    auto lastText = [](unsigned packed) {
        if (!(packed & 4095)) return std::wstring(L"none");
        const unsigned address = (packed & 4095) - 1;
        return halljoy::midi::NoteName(address % 128) + L" (#" + std::to_wstring(address % 128) +
            L", Ch " + std::to_wstring(address / 128 + 1) + L") velocity " + std::to_wstring(packed >> 12);
    };
    line(L"Last computed: " + lastText(telemetry.lastGenerated));
    line(L"Last sent: " + lastText(telemetry.lastSent));
    line(L"Send errors: " + std::to_wstring(telemetry.sendErrors) + L"    Queue resets: " + std::to_wstring(telemetry.overflows) +
        L"    Attacks with sparse observations: " + std::to_wstring(telemetry.sparseAttacks), UiTheme::Color_TextMuted());
    const auto quality = telemetry.touch.quality;
    const wchar_t* qualityText = quality == halljoy::midi::TouchQuality::CapturedMotion ? L"captured motion" :
        quality == halljoy::midi::TouchQuality::SnapshotMotion ? L"snapshot motion" :
        quality == halljoy::midi::TouchQuality::SparseInterval ? L"single interval / interrupted stroke" :
        quality == halljoy::midi::TouchQuality::UnresolvedTiming ? L"timing unresolved" : L"no attack";
    line(std::wstring(L"Touch v1: ") + qualityText + L"    Speed " +
        std::to_wstring(telemetry.touch.speedMilli / 1000.0) + L" travel/s    Interval " +
        std::to_wstring(telemetry.touch.intervalUs / 1000.0) + L" ms", UiTheme::Color_TextMuted());
    line(L"Input history: " + std::to_wstring(telemetry.historySamples) + L" samples    Loss resets: " +
        std::to_wstring(telemetry.historyLosses) + L"    Invalid: " + std::to_wstring(telemetry.invalidObservations), UiTheme::Color_TextMuted());
    const wchar_t* uapMode=telemetry.uapHistoryMode==1 ? L"ordered history" : telemetry.uapHistoryMode==2 ? L"snapshot fallback" : telemetry.uapHistoryMode==3 ? L"starting history" : L"idle";
    line(std::wstring(L"UAP: ") + uapMode + L"    Samples: " + std::to_wstring(telemetry.snapshotObservations) + L"    Gaps: " +
        std::to_wstring(telemetry.snapshotGaps) + L"    Max queue age: " +
        std::to_wstring(telemetry.maximumHistoryAgeUs / 1000.0) + L" ms", UiTheme::Color_TextMuted());
    y += scale(6);
    unsigned shown = 0;
    for (unsigned address = 0; address < halljoy::midi::kAddressCount && shown < 8; ++address) {
        const auto note = telemetry.generated[address];
        if (!note.velocity) continue;
        const bool sent = telemetry.sent[address].velocity != 0;
        RECT label{x, y, x + scale(178), y + scale(18)};
        const auto text = halljoy::midi::NoteName(address % 128) + L"  Ch " + std::to_wstring(address / 128 + 1) +
            L"  " + (sent ? L"sent" : L"monitor / pending");
        CustomPage_DrawText(dc, text.c_str(), label, UiTheme::Color_Text(), DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);
        const int left = std::min(right - scale(40), x + scale(180));
        const int width = std::max(scale(20), (right - left - scale(10)) / 2);
        RECT velocity{left, y, left + width, y + scale(14)};
        RECT pressure{left + width + scale(8), y, right, y + scale(14)};
        GamepadRender_DrawTriggerBar01(dc, velocity, static_cast<std::uint8_t>((note.velocity * 255u + 63) / 127));
        GamepadRender_DrawTriggerBar01(dc, pressure, static_cast<std::uint8_t>((note.pressure * 255u + 63) / 127));
        RECT values{left, y + scale(15), right, y + scale(32)};
        const auto numbers = L"Velocity " + std::to_wstring(note.velocity) + L"     Depth / aftertouch " + std::to_wstring(note.pressure);
        CustomPage_DrawText(dc, numbers.c_str(), values, UiTheme::Color_TextMuted(), DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);
        y += scale(36); ++shown;
    }
    if (!shown) line(L"Press a bound key to test notes, velocity and depth.", UiTheme::Color_TextMuted());
    if (telemetry.generatedActive > shown) line(L"Additional held notes: " + std::to_wstring(telemetry.generatedActive - shown), UiTheme::Color_TextMuted());
}

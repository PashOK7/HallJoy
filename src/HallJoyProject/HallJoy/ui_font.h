#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <algorithm>
#include <array>
#include <map>
#include <mutex>
#include <tuple>
#include "ui_language.h"

// Only request system-installed fonts by name. Never embed, copy or download
// Windows font files. See docs/development/INTERFACE_LANGUAGES.md.
inline bool UiUsesCjkFont() {
    using namespace halljoy::ui_language;
    return Get() == Language::Korean || Get() == Language::Japanese || Get() == Language::Chinese;
}
inline const wchar_t* UiFontFamily() {
    using namespace halljoy::ui_language;
    static const wchar_t* names[] = {L"Malgun Gothic", L"Yu Gothic UI", L"Microsoft YaHei UI"};
    static const auto installed = [] {
        std::array<bool, 3> found{};
        HDC dc = GetDC(nullptr);
        if (!dc) return found;
        for (unsigned i = 0; i < found.size(); ++i) {
            LOGFONTW query{}; query.lfCharSet = DEFAULT_CHARSET;
            wcscpy_s(query.lfFaceName, names[i]);
            bool exists = false;
            EnumFontFamiliesExW(dc, &query, [](const LOGFONTW*, const TEXTMETRICW*, DWORD, LPARAM p) -> int {
                *reinterpret_cast<bool*>(p) = true; return 0;
            }, reinterpret_cast<LPARAM>(&exists), 0);
            found[i] = exists;
        }
        ReleaseDC(nullptr, dc);
        return found;
    }();
    const int i = Get() == Language::Korean ? 0 : Get() == Language::Japanese ? 1 : Get() == Language::Chinese ? 2 : -1;
    return i >= 0 && installed[i] ? names[i] : L"Segoe UI";
}
inline HFONT UiCjkFont(HFONT original, unsigned dpi = 96) {
    if (!original) original = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
    if (!UiUsesCjkFont()) return original;
    LOGFONTW font{};
    if (!GetObjectW(original, sizeof(font), &font)) return original;
    // Positive stock-font heights include internal leading; use a readable
    // 13-pixel character height at 96 DPI. Preserve explicit heading sizes.
    const int height = font.lfHeight < 0 ? std::max(static_cast<int>(-font.lfHeight), MulDiv(13, dpi, 96)) : MulDiv(13, dpi, 96);
    using Key = std::tuple<int, LONG, BYTE, unsigned>;
    struct Cache {
        std::mutex mutex;
        std::map<Key, HFONT> fonts;
        ~Cache() { for (auto& entry : fonts) DeleteObject(entry.second); }
    };
    static Cache cache;
    std::lock_guard<std::mutex> guard(cache.mutex);
    const Key key{height, font.lfWeight, font.lfItalic, static_cast<unsigned>(halljoy::ui_language::Get())};
    if (auto it = cache.fonts.find(key); it != cache.fonts.end()) return it->second;
    HFONT result = CreateFontW(-height, 0, 0, 0, font.lfWeight, font.lfItalic, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE, UiFontFamily());
    if (!result) return original;
    cache.fonts.emplace(key, result);
    return result;
}

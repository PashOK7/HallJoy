#pragma once
#include <atomic>
#include <cstddef>
#include <string_view>

// Presentation only. Never translate persistence keys, device identities,
// filenames, key legends or user-created names through this catalog.
namespace halljoy::ui_language {
enum class Language : unsigned { English, Korean, Japanese, Russian, Spanish, Italian, French, German, Chinese, Count };
struct LanguageInfo { const wchar_t* code; const wchar_t* name; };
inline constexpr LanguageInfo languages[] = {
    {L"en", L"English"}, {L"ko", L"한국어"}, {L"ja", L"日本語"},
    {L"ru", L"Русский"}, {L"es", L"Español"}, {L"it", L"Italiano"},
    {L"fr", L"Français"}, {L"de", L"Deutsch"}, {L"zh-CN", L"简体中文"}
};
inline constexpr unsigned count = static_cast<unsigned>(Language::Count);
static_assert(sizeof(languages) / sizeof(languages[0]) == count);
struct Translation { const wchar_t* text[count]; };
inline constexpr Translation catalog[] = {
#include "ui_translations.inc"
};
inline std::atomic<unsigned> current{0};
inline std::atomic<unsigned> revision{0};
inline Language Get() noexcept { return static_cast<Language>(current.load()); }
inline Language Parse(std::wstring_view code) noexcept {
    for (unsigned i = 0; i < count; ++i) if (code == languages[i].code) return static_cast<Language>(i);
    return Language::English;
}
inline const wchar_t* Code() noexcept { return languages[static_cast<unsigned>(Get())].code; }
inline bool Set(Language language) noexcept {
    const auto index = static_cast<unsigned>(language);
    if (index >= count) return false;
    if (current.exchange(index) != index) ++revision;
    return true;
}
inline const wchar_t* Text(const wchar_t* english) noexcept {
    if (!english) return L"";
    const auto index = static_cast<unsigned>(Get());
    if (index == 0 || !*english) return english;
    for (const auto& row : catalog)
        if (std::wstring_view(row.text[0]) == english)
            return row.text[index] && *row.text[index] ? row.text[index] : english;
    return english;
}
} // namespace halljoy::ui_language

inline const wchar_t* UiText(const wchar_t* english) noexcept {
    return halljoy::ui_language::Text(english);
}

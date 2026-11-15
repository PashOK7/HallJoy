#include "ui_language.h"
#include <cassert>
#include <cwchar>
#include <iostream>
#include <set>
#include <string>
#include <vector>

static std::vector<std::wstring> Formats(const wchar_t* text) {
    std::vector<std::wstring> result;
    for (const wchar_t* p = text; *p; ++p) {
        if (*p != L'%') continue;
        const wchar_t* start = p++;
        if (*p == L'%') continue;
        while (*p && !std::wcschr(L"diuoxXfFeEgGaAcspn", *p)) ++p;
        assert(*p && *p != L'n');
        result.emplace_back(start, p + 1);
    }
    return result;
}
int main() {
    using namespace halljoy::ui_language;
    assert(Get() == Language::English && std::wstring(Code()) == L"en");
    const wchar_t* expected[] = {L"en", L"ko", L"ja", L"ru", L"es", L"it", L"fr", L"de", L"zh-CN"};
    static_assert(count == 9);
    for (unsigned i = 0; i < count; ++i) {
        assert(Parse(expected[i]) == static_cast<Language>(i));
        assert(Set(Parse(expected[i])) && std::wstring(Code()) == expected[i]);
        assert(languages[i].name && *languages[i].name);
        const wchar_t* identity = L"GamaKay TK75 HE V2 ISO UK";
        assert(UiText(identity) == identity); // Unknown text is preserved verbatim.
        assert(std::wstring(UiText(nullptr)).empty());
    }
    Set(Language::Russian);
    assert(std::wstring(UiText(L"Language")) == L"Язык");
    assert(std::wstring(UiText(L"Global settings")) == L"Общие настройки");
    assert(std::wstring(UiText(L"Help.Curve")).find(L"глубина") != std::wstring::npos);
    Set(Language::Japanese);
    assert(std::wstring(UiText(L"Language")) == L"言語");
    Set(Language::English);
    const auto before = revision.load();
    assert(Set(Language::English) && revision.load() == before);
    assert(!Set(static_cast<Language>(999)) && Get() == Language::English);
    assert(Parse(L"unknown") == Language::English && Parse(L"") == Language::English);
    assert(std::wstring(UiText(L"Global settings")) == L"Global settings");
    std::set<std::wstring> keys;
    for (const auto& row : catalog) {
        assert(row.text[0] && *row.text[0] && keys.insert(row.text[0]).second);
        const auto expectedFormats = Formats(row.text[0]);
        for (unsigned i = 1; i < count; ++i) {
            assert(row.text[i] && *row.text[i]);
            assert(Formats(row.text[i]) == expectedFormats);
        }
    }
    std::cout << "ui language: nine locales, fallback, refresh revision, catalog completeness and format contracts passed ("
              << keys.size() << " messages)\n";
}

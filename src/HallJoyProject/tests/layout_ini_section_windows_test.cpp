#include "layout_ini_section.h"
#include <cassert>
#include <iostream>
#include <filesystem>

int main() {
    wchar_t temporary[MAX_PATH]{}, name[MAX_PATH]{};
    assert(GetTempPathW(MAX_PATH, temporary));
    assert(GetTempFileNameW(temporary, L"HJL", 0, name));
    std::wstring text = L"[LayoutPreset]\r\nBrand=\"  Unicode \x0416 \x65E5  \"\r\n"
        L"Number=+17\r\nEmpty=\r\nQuoted='inside'\r\nDuplicate=first\r\ndUPLICATE=second\r\n"
        L"Padded =   spaced   \r\nLiteral=\"a\\b|c;d=e\"\r\n";
    for (int i = 0; i < 2000; ++i) text += L"K" + std::to_wstring(i) + L"=42|1|2|42|42|label\r\n";
    DWORD error = 0;
    assert(halljoy::layout_storage::WriteUtf16(name, text, &error));
    {
        halljoy::ini::ReadFile guard(name);
        assert(guard);
        halljoy::layout_storage::Section section;
        assert(section.Load(name, L"LayoutPreset"));
        for (const wchar_t* key : {L"Brand", L"Number", L"Empty", L"Quoted", L"Duplicate", L"Padded", L"Literal", L"K1999", L"Missing"}) {
            wchar_t legacy[1024]{};
            GetPrivateProfileStringW(L"LayoutPreset", key, L"", legacy, 1024, name);
            if (section.Get(key) != legacy) { std::wcerr << L"Mismatch: " << key << L"\n"; return 1; }
        }
        int number = 0;
        assert(section.Integer(L"Number", 0, 30, 0, number) && number == 17);
        assert(!section.Integer(L"Number", 0, 16, 0, number));
        assert(section.Integer(L"Missing", 0, 30, 9, number) && number == 9);
        // A reader lease blocks concurrent write access.
        assert(!halljoy::layout_storage::WriteUtf16(name, L"bad", &error));
    }
    // A shorter replacement must not leave a tail from the previous document.
    assert(halljoy::layout_storage::WriteUtf16(name, L"[LayoutPreset]\r\nCount=1\r\n", &error));
    assert(std::filesystem::file_size(name) == 2 + 2 * wcslen(L"[LayoutPreset]\r\nCount=1\r\n"));
    assert(DeleteFileW(name));
    std::cout << "LAYOUT_INI_SECTION_WINDOWS_TEST=PASS legacy_semantics=1 large_section=1 unicode=1 reader_lock=1 truncate=1\n";
}

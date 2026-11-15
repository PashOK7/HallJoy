#include "ui_font.h"
#include "ui_language_win32.h"
#include "test_thread_desktop.h"
#include <cassert>
#include <cwchar>
#include <iostream>

int main() {
    using namespace halljoy::ui_language;
    const auto original = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
    assert(UiCjkFont(original) == original);
    HDC dc = CreateCompatibleDC(nullptr);
    assert(dc);
    struct Case { Language language; const wchar_t* sample; };
    for (auto test : {Case{Language::Korean,L"언어 설정"}, Case{Language::Japanese,L"言語設定"},
                      Case{Language::Chinese,L"语言设置"}}) {
        Set(test.language);
        for (unsigned dpi : {96u,144u,192u}) {
            HFONT font = UiCjkFont(original,dpi);
            assert(font && font != original && font == UiCjkFont(original,dpi));
            LOGFONTW logical{}; GetObjectW(font,sizeof(logical),&logical);
            assert(-logical.lfHeight >= MulDiv(13,dpi,96));
            const auto previous = SelectObject(dc,font);
            wchar_t family[LF_FACESIZE]{}; GetTextFaceW(dc,LF_FACESIZE,family);
            assert(std::wcscmp(family,UiFontFamily()) == 0);
            TEXTMETRICW metrics{}; assert(GetTextMetricsW(dc,&metrics));
            assert(metrics.tmHeight <= MulDiv(20,dpi,96));
            WORD glyphs[16]{};
            const auto length = static_cast<int>(std::wcslen(test.sample));
            assert(GetGlyphIndicesW(dc,test.sample,length,glyphs,GGI_MARK_NONEXISTING_GLYPHS) != GDI_ERROR);
            // An installation without optional CJK fonts may use font linking.
            // When the requested family exists, require native sample coverage.
            if (std::wcscmp(UiFontFamily(),L"Segoe UI") != 0)
                for (int i=0; i<length; ++i) assert(glyphs[i] != 0xffff);
            std::wcout << Code() << L" " << family << L" dpi=" << dpi << L" line=" << metrics.tmHeight << L"\n";
            SelectObject(dc,previous);
        }
    }
    Set(Language::English);
    assert(UiCjkFont(original) == original);
    DeleteDC(dc);
    const bool nativeControlOk=halljoy::test_desktop::RunOnPrivateDesktop(L"HallJoyFontTest", [=] {
        Set(Language::Korean);
        HWND owner=CreateWindowW(L"STATIC",L"",WS_POPUP,0,0,200,100,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
        HWND label=UiCreateWindow(L"STATIC",L"Language",WS_CHILD,0,0,100,30,owner,nullptr,GetModuleHandleW(nullptr),nullptr);
        // Several callers assign their old stock font immediately after creation.
        SendMessageW(label,WM_SETFONT,reinterpret_cast<WPARAM>(original),FALSE);
        HFONT assigned=reinterpret_cast<HFONT>(SendMessageW(label,WM_GETFONT,0,0));
        bool ok=owner && label && assigned==UiCjkFont(original,WinUtil_GetDpiForWindowCompat(label));
        Set(Language::English); UiLanguageApply(owner);
        ok &= reinterpret_cast<HFONT>(SendMessageW(label,WM_GETFONT,0,0))==original;
        DestroyWindow(owner);
        return ok;
    });
    if (!nativeControlOk) { std::cerr << "CJK startup font was overwritten by a stock-font assignment\n"; return 1; }
    std::cout << "CJK system font selection, cache, DPI, metrics and installed glyph coverage passed\n";
}

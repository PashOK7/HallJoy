#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <commctrl.h>
#include <uxtheme.h>
#include <string>
#include "ui_language.h"
#include "ui_font.h"
#include "win_util.h"

inline UINT UiLanguageChangedMessage() {
    static const UINT message = RegisterWindowMessageW(L"HallJoy.UiLanguageChanged.v1");
    return message;
}

// Only explicitly marked UI controls retain their English source text.
// EDIT controls and user-created names are never registered here.
inline LRESULT CALLBACK UiLabelSubclass(HWND hwnd, UINT message, WPARAM wp, LPARAM lp,
    UINT_PTR id, DWORD_PTR data) {
    auto* english = reinterpret_cast<std::wstring*>(data);
    if (message == WM_SETFONT)
        wp = reinterpret_cast<WPARAM>(UiCjkFont(reinterpret_cast<HFONT>(wp), WinUtil_GetDpiForWindowCompat(hwnd)));
    if (message == UiLanguageChangedMessage()) {
        SendMessageW(hwnd, WM_SETFONT, reinterpret_cast<WPARAM>(UiCjkFont(
            static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT)), WinUtil_GetDpiForWindowCompat(hwnd))), FALSE);
        SetWindowTextW(hwnd, UiText(english->c_str()));
    }
    if (message == WM_NCDESTROY) {
        RemoveWindowSubclass(hwnd, UiLabelSubclass, id);
        delete english;
    }
    return DefSubclassProc(hwnd, message, wp, lp);
}
inline void UiLabel(HWND hwnd, const wchar_t* english) {
    if (!hwnd) return;
    DWORD_PTR previous = 0;
    if (GetWindowSubclass(hwnd, UiLabelSubclass, 1, &previous)) {
        *reinterpret_cast<std::wstring*>(previous) = english ? english : L"";
    } else {
        auto* source = new std::wstring(english ? english : L"");
        if (!SetWindowSubclass(hwnd, UiLabelSubclass, 1, reinterpret_cast<DWORD_PTR>(source))) delete source;
    }
    SendMessageW(hwnd, WM_SETFONT, reinterpret_cast<WPARAM>(UiCjkFont(
        static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT)), WinUtil_GetDpiForWindowCompat(hwnd))), FALSE);
    SetWindowTextW(hwnd, UiText(english));
}
inline HWND UiCreateWindow(LPCWSTR cls, LPCWSTR text, DWORD style, int x, int y, int w, int h,
    HWND parent, HMENU menu, HINSTANCE instance, LPVOID data) {
    HWND hwnd = CreateWindowW(cls, text, style, x, y, w, h, parent, menu, instance, data);
    UiLabel(hwnd, text);
    return hwnd;
}
inline HWND UiCreateWindowEx(DWORD ex, LPCWSTR cls, LPCWSTR text, DWORD style, int x, int y, int w, int h,
    HWND parent, HMENU menu, HINSTANCE instance, LPVOID data) {
    HWND hwnd = CreateWindowExW(ex, cls, text, style, x, y, w, h, parent, menu, instance, data);
    UiLabel(hwnd, text);
    return hwnd;
}
inline void UiLanguageApply(HWND root) {
    if (!root) return;
    SendMessageW(root, UiLanguageChangedMessage(), 0, 0);
    EnumChildWindows(root, [](HWND child, LPARAM) -> BOOL {
        SendMessageW(child, UiLanguageChangedMessage(), 0, 0);
        return TRUE;
    }, 0);
    RedrawWindow(root, nullptr, nullptr, RDW_INVALIDATE | RDW_ALLCHILDREN);
}

// Native delayed help for retained pages; it never creates a visible child
// control in the scrolling surface or needs a repeating application timer.
class UiHoverHelp {
    HWND tip_ = nullptr;
    HWND owner_ = nullptr;
    const wchar_t* key_ = nullptr;
    std::wstring text_;
public:
    UiHoverHelp() = default;
    UiHoverHelp(const UiHoverHelp&) = delete;
    UiHoverHelp& operator=(const UiHoverHelp&) = delete;
    ~UiHoverHelp() { if (tip_ && IsWindow(tip_)) DestroyWindow(tip_); }
#if defined(HALLJOY_ANALOG_SIMULATOR)
    bool TestVisible() const { return tip_ && IsWindowVisible(tip_); }
    const std::wstring& TestText() const { return text_; }
#endif
    void Hide() {
        if (tip_) {
            TOOLINFOW tool{}; tool.cbSize=sizeof(tool); tool.hwnd = owner_; tool.uId = 1;
            SendMessageW(tip_, TTM_TRACKACTIVATE, FALSE, reinterpret_cast<LPARAM>(&tool));
        }
    }
    void Handle(HWND page, UINT message, WPARAM wp, LPARAM lp, const wchar_t* key = nullptr) {
        if (message == WM_NCDESTROY) {
            if (tip_) DestroyWindow(tip_);
            tip_ = nullptr; owner_ = nullptr; key_ = nullptr; return;
        }
        if (message == UiLanguageChangedMessage() || message == WM_MOUSELEAVE ||
            message == WM_MOUSEWHEEL || message == WM_VSCROLL || message == WM_SIZE ||
            message == WM_LBUTTONDOWN || message == WM_RBUTTONDOWN || message == WM_KILLFOCUS ||
            message == WM_CANCELMODE || message == WM_CAPTURECHANGED || message == WM_SHOWWINDOW) {
            Hide(); key_ = nullptr;
        }
        if (message == WM_MOUSEMOVE) {
            if (GetCapture() || halljoy::ui_language::Get() == halljoy::ui_language::Language::English) key = nullptr;
            if (key != key_) { Hide(); key_ = key; }
            TRACKMOUSEEVENT track{sizeof(track), TME_LEAVE | TME_HOVER, page, 1800};
            TrackMouseEvent(&track);
        }
        if (message != WM_MOUSEHOVER || !key_ || !IsWindowVisible(page) || GetCapture()) return;
        if (!tip_) {
            tip_ = CreateWindowExW(WS_EX_TOPMOST | WS_EX_NOACTIVATE, TOOLTIPS_CLASSW, nullptr,
                WS_POPUP | TTS_NOPREFIX | TTS_ALWAYSTIP, CW_USEDEFAULT, CW_USEDEFAULT,
                CW_USEDEFAULT, CW_USEDEFAULT, page, nullptr, GetModuleHandleW(nullptr), nullptr);
            if (!tip_) return;
            owner_ = page;
            SetWindowTheme(tip_, L"", L"");
            SendMessageW(tip_, TTM_SETTIPBKCOLOR, RGB(38, 38, 40), 0);
            SendMessageW(tip_, TTM_SETTIPTEXTCOLOR, RGB(235, 235, 235), 0);
        }
        SendMessageW(tip_,WM_SETFONT,reinterpret_cast<WPARAM>(UiCjkFont(static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT)),WinUtil_GetDpiForWindowCompat(page))),FALSE);
        SendMessageW(tip_, TTM_SETMAXTIPWIDTH, 0, WinUtil_ScalePx(page, 380));
        text_ = UiText(key_);
        TOOLINFOW tool{}; tool.cbSize=sizeof(tool);
        tool.hwnd = page; tool.uId = 1; tool.uFlags = TTF_TRACK | TTF_ABSOLUTE;
        tool.lpszText = text_.data();
        SendMessageW(tip_, TTM_DELTOOLW, 0, reinterpret_cast<LPARAM>(&tool));
        SendMessageW(tip_, TTM_ADDTOOLW, 0, reinterpret_cast<LPARAM>(&tool));
        POINT point{static_cast<short>(LOWORD(lp)), static_cast<short>(HIWORD(lp))};
        ClientToScreen(page, &point);
        SendMessageW(tip_, TTM_TRACKPOSITION, 0, MAKELPARAM(point.x + 12, point.y + 22));
        SendMessageW(tip_, TTM_TRACKACTIVATE, TRUE, reinterpret_cast<LPARAM>(&tool));
        (void)wp;
    }
};

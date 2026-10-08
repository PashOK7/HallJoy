#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <windowsx.h>
#include <dwmapi.h>
#include <algorithm>

#include "ui_popup_menu.h"
#include "ui_theme.h"
#include "win_util.h"

#pragma comment(lib, "Dwmapi.lib")

namespace halljoy::ui_menu {
namespace {

struct State {
    const std::vector<Item>* items = nullptr;
    std::vector<RECT> rows;        // content coordinates
    int hot = -1;
    int scroll = 0, contentHeight = 0;
    UINT result = 0;
    bool running = true;
    HFONT font = nullptr;
};

int S(HWND h, int px) { return WinUtil_ScalePx(h, px); }

int RowAt(State* st, HWND h, POINT client) {
    RECT rc{}; GetClientRect(h, &rc);
    if (!PtInRect(&rc, client)) return -1;
    client.y += st->scroll;
    for (std::size_t i = 0; i < st->rows.size(); ++i) {
        const Item& item = (*st->items)[i];
        if (!item.separator && item.enabled && PtInRect(&st->rows[i], client)) return static_cast<int>(i);
    }
    return -1;
}

void Paint(HWND h, State* st, HDC target) {
    RECT rc{}; GetClientRect(h, &rc);
    HDC dc = CreateCompatibleDC(target);
    HBITMAP bmp = CreateCompatibleBitmap(target, std::max<LONG>(1, rc.right), std::max<LONG>(1, rc.bottom));
    HGDIOBJ oldBmp = SelectObject(dc, bmp);
    FillRect(dc, &rc, UiTheme::Brush_ControlBg());
    HGDIOBJ oldFont = SelectObject(dc, st->font);
    SetBkMode(dc, TRANSPARENT);
    const int pad = S(h, 10), icon = S(h, 16);
    for (std::size_t i = 0; i < st->rows.size(); ++i) {
        const Item& item = (*st->items)[i];
        RECT row = st->rows[i];
        OffsetRect(&row, 0, -st->scroll);
        if (row.bottom < 0 || row.top > rc.bottom) continue;
        if (item.separator) {
            RECT line{ row.left + pad, (row.top + row.bottom) / 2, row.right - pad, (row.top + row.bottom) / 2 + 1 };
            HBRUSH b = CreateSolidBrush(UiTheme::Color_Border());
            FillRect(dc, &line, b); DeleteObject(b);
            continue;
        }
        const bool hot = static_cast<int>(i) == st->hot;
        if (hot) { HBRUSH b = CreateSolidBrush(UiTheme::Color_Accent()); FillRect(dc, &row, b); DeleteObject(b); }
        RECT text = row; text.left += pad; text.right -= pad;
        if (item.icon) {
            DrawIconEx(dc, text.left, (row.top + row.bottom - icon) / 2, item.icon, icon, icon, 0, nullptr, DI_NORMAL);
            text.left += icon + S(h, 8);
        }
        if (!item.detail.empty()) {
            RECT detail = text;
            SetTextColor(dc, hot ? RGB(222, 228, 240) : UiTheme::Color_TextMuted());
            DrawTextW(dc, item.detail.c_str(), -1, &detail, DT_RIGHT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
            RECT measure{ 0, 0, 0, 0 };
            DrawTextW(dc, item.detail.c_str(), -1, &measure, DT_CALCRECT | DT_SINGLELINE | DT_NOPREFIX);
            text.right = std::max(text.left, text.right - (measure.right - measure.left) - S(h, 16));
        }
        SetTextColor(dc, !item.enabled ? UiTheme::Color_TextMuted() : UiTheme::Color_Text());
        DrawTextW(dc, item.text.c_str(), -1, &text, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
    }
    // Border over everything (rows never cover it).
    HBRUSH border = CreateSolidBrush(UiTheme::Color_Border());
    FrameRect(dc, &rc, border); DeleteObject(border);
    SelectObject(dc, oldFont);
    BitBlt(target, 0, 0, rc.right, rc.bottom, dc, 0, 0, SRCCOPY);
    SelectObject(dc, oldBmp); DeleteObject(bmp); DeleteDC(dc);
}

LRESULT CALLBACK Proc(HWND h, UINT m, WPARAM w, LPARAM l) {
    auto* st = reinterpret_cast<State*>(GetWindowLongPtrW(h, GWLP_USERDATA));
    switch (m) {
    case WM_NCCREATE:
        SetWindowLongPtrW(h, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(reinterpret_cast<CREATESTRUCTW*>(l)->lpCreateParams));
        break;
    case WM_MOUSEACTIVATE: return MA_NOACTIVATE;
    case WM_ERASEBKGND: return 1;
    case WM_PAINT: {
        PAINTSTRUCT ps{}; HDC dc = BeginPaint(h, &ps);
        if (st) Paint(h, st, dc);
        EndPaint(h, &ps);
        return 0;
    }
    case WM_MOUSEMOVE:
        if (st) {
            const int hot = RowAt(st, h, POINT{ GET_X_LPARAM(l), GET_Y_LPARAM(l) });
            if (hot != st->hot) { st->hot = hot; InvalidateRect(h, nullptr, FALSE); }
        }
        return 0;
    case WM_LBUTTONDOWN: case WM_RBUTTONDOWN: case WM_MBUTTONDOWN: {
        RECT rc{}; GetClientRect(h, &rc);
        POINT pt{ GET_X_LPARAM(l), GET_Y_LPARAM(l) };
        if (st && !PtInRect(&rc, pt)) st->running = false; // a click outside closes
        return 0;
    }
    case WM_LBUTTONUP: case WM_RBUTTONUP:
        if (st) {
            const int row = RowAt(st, h, POINT{ GET_X_LPARAM(l), GET_Y_LPARAM(l) });
            if (row >= 0) { st->result = (*st->items)[static_cast<std::size_t>(row)].id; st->running = false; }
        }
        return 0;
    case WM_MOUSEWHEEL:
        if (st) {
            RECT rc{}; GetClientRect(h, &rc);
            const int maximum = std::max(0, st->contentHeight - static_cast<int>(rc.bottom));
            st->scroll = std::clamp(st->scroll - GET_WHEEL_DELTA_WPARAM(w) / WHEEL_DELTA * S(h, 56), 0, maximum);
            InvalidateRect(h, nullptr, FALSE);
        }
        return 0;
    case WM_CAPTURECHANGED:
        if (st) st->running = false;
        return 0;
    }
    return DefWindowProcW(h, m, w, l);
}

} // namespace

UINT Track(HWND owner, const std::vector<Item>& items, POINT anchor) {
    if (items.empty()) return 0;
    const HINSTANCE instance = GetModuleHandleW(nullptr);
    static bool registered = false;
    if (!registered) {
        WNDCLASSW wc{};
        wc.style = CS_DROPSHADOW;
        wc.lpfnWndProc = Proc; wc.hInstance = instance;
        wc.lpszClassName = L"HallJoyPopupMenu"; wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        registered = RegisterClassW(&wc) != 0 || GetLastError() == ERROR_CLASS_ALREADY_EXISTS;
    }
    State st;
    st.items = &items;
    st.font = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
    const HWND root = owner ? GetAncestor(owner, GA_ROOT) : nullptr;
    const HWND h = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, L"HallJoyPopupMenu", L"", WS_POPUP,
        0, 0, 10, 10, root, nullptr, instance, &st);
    if (!h) return 0;

    // Measure rows (DPI of the anchor's monitor through the popup window).
    HDC dc = GetDC(h);
    HGDIOBJ oldFont = SelectObject(dc, st.font);
    const int pad = S(h, 10), rowH = S(h, 28), sepH = S(h, 9), top = S(h, 4);
    int width = S(h, 180), y = top;
    for (const auto& item : items) {
        if (item.separator) { st.rows.push_back(RECT{ 1, y, 0, y + sepH }); y += sepH; continue; }
        RECT t{ 0, 0, 0, 0 }, d{ 0, 0, 0, 0 };
        DrawTextW(dc, item.text.c_str(), -1, &t, DT_CALCRECT | DT_SINGLELINE | DT_NOPREFIX);
        if (!item.detail.empty()) DrawTextW(dc, item.detail.c_str(), -1, &d, DT_CALCRECT | DT_SINGLELINE | DT_NOPREFIX);
        const int w = pad * 2 + (item.icon ? S(h, 24) : 0) + (t.right - t.left) + (item.detail.empty() ? 0 : S(h, 16) + d.right - d.left);
        width = std::max(width, w);
        st.rows.push_back(RECT{ 1, y, 0, y + rowH });
        y += rowH;
    }
    SelectObject(dc, oldFont);
    ReleaseDC(h, dc);
    st.contentHeight = y + top;
    MONITORINFO mi{ sizeof(mi) };
    GetMonitorInfoW(MonitorFromPoint(anchor, MONITOR_DEFAULTTONEAREST), &mi);
    const RECT work = mi.rcWork;
    width = std::min(width, std::min<int>(S(h, 560), work.right - work.left));
    for (auto& row : st.rows) row.right = width - 1;
    const int height = std::min<int>(st.contentHeight, work.bottom - work.top);
    int x = std::clamp<int>(anchor.x, work.left, work.right - width);
    int yPos = anchor.y;
    if (yPos + height > work.bottom) yPos = std::max<int>(work.top, anchor.y - height); // open upwards
    // Rounded corners on Windows 11 (ignored elsewhere).
    const DWORD round = 3; // DWMWCP_ROUNDSMALL
    DwmSetWindowAttribute(h, 33 /* DWMWA_WINDOW_CORNER_PREFERENCE */, &round, sizeof(round));
    SetWindowPos(h, HWND_TOP, x, yPos, width, height, SWP_NOACTIVATE | SWP_SHOWWINDOW);
    UpdateWindow(h);
    SetCapture(h);

    // Modal loop. Keyboard input is swallowed except Esc (closes), so held
    // game keys can never pick an item; everything else keeps running.
    while (st.running) {
        MSG msg{};
        const BOOL got = GetMessageW(&msg, nullptr, 0, 0);
        if (got == 0) { PostQuitMessage(static_cast<int>(msg.wParam)); break; }
        if (got < 0) break;
        if (msg.message >= WM_KEYFIRST && msg.message <= WM_KEYLAST) {
            if (msg.message == WM_KEYDOWN && msg.wParam == VK_ESCAPE) st.running = false;
            continue;
        }
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
        if (GetCapture() != h || (root && GetForegroundWindow() != root)) st.running = false;
    }
    if (GetCapture() == h) ReleaseCapture();
    DestroyWindow(h);
    return st.result;
}

} // namespace halljoy::ui_menu

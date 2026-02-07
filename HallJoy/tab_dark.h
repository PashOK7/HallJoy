#pragma once
#ifndef _WIN32_IE
#define _WIN32_IE 0x0600
#endif

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>

#include "ui_theme.h"

// Dark painter for WC_TABCONTROL on Win10.
// Key properties:
//  - NEVER paints inside the page rect (TabCtrl_AdjustRect(FALSE)) -> prevents wiping pages.
//  - Posts a custom "selection changed" message to parent as a reliable fallback.

namespace TabDark
{
    inline UINT MsgSelChanged()
    {
        static UINT msg = RegisterWindowMessageW(L"TabDark_SelChanged");
        return msg;
    }

    static HPEN PenBorder()
    {
        static HPEN p = CreatePen(PS_SOLID, 1, UiTheme::Color_Border());
        return p;
    }

    static HPEN PenAccent()
    {
        static HPEN p = CreatePen(PS_SOLID, 2, UiTheme::Color_Accent());
        return p;
    }

    static void FillSafe(HDC hdc, const RECT& rc, HBRUSH br)
    {
        if (rc.right <= rc.left || rc.bottom <= rc.top) return;
        FillRect(hdc, &rc, br);
    }

    static void PaintTabControl(HWND hTab, HDC hdc)
    {
        RECT rcClient{};
        GetClientRect(hTab, &rcClient);

        // Page area (where child pages are located)
        RECT rcPage = rcClient;
        TabCtrl_AdjustRect(hTab, FALSE, &rcPage);

        // Paint ONLY around the page rect, never inside it (prevents wiping pages)
        RECT rcHeader = rcClient;
        rcHeader.bottom = rcPage.top;
        FillSafe(hdc, rcHeader, UiTheme::Brush_PanelBg());

        RECT rcLeft = { rcClient.left, rcPage.top, rcPage.left, rcPage.bottom };
        RECT rcRight = { rcPage.right, rcPage.top, rcClient.right, rcPage.bottom };
        RECT rcBottom = { rcClient.left, rcPage.bottom, rcClient.right, rcClient.bottom };

        FillSafe(hdc, rcLeft, UiTheme::Brush_PanelBg());
        FillSafe(hdc, rcRight, UiTheme::Brush_PanelBg());
        FillSafe(hdc, rcBottom, UiTheme::Brush_PanelBg());

        HFONT f = (HFONT)SendMessageW(hTab, WM_GETFONT, 0, 0);
        if (!f) f = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
        HGDIOBJ oldFont = SelectObject(hdc, f);

        SetBkMode(hdc, TRANSPARENT);

        int curSel = TabCtrl_GetCurSel(hTab);
        int count = TabCtrl_GetItemCount(hTab);

        for (int i = 0; i < count; ++i)
        {
            RECT rci{};
            if (!TabCtrl_GetItemRect(hTab, i, &rci))
                continue;

            bool selected = (i == curSel);

            FillRect(hdc, &rci, selected ? UiTheme::Brush_ControlBg() : UiTheme::Brush_PanelBg());

            // border
            HGDIOBJ oldPen = SelectObject(hdc, PenBorder());
            HGDIOBJ oldBrush = SelectObject(hdc, GetStockObject(HOLLOW_BRUSH));
            Rectangle(hdc, rci.left, rci.top, rci.right, rci.bottom);
            SelectObject(hdc, oldBrush);
            SelectObject(hdc, oldPen);

            // text
            wchar_t text[128]{};
            TCITEMW it{};
            it.mask = TCIF_TEXT;
            it.pszText = text;
            it.cchTextMax = (int)(sizeof(text) / sizeof(text[0]));
            TabCtrl_GetItem(hTab, i, &it);

            SetTextColor(hdc, selected ? UiTheme::Color_Text() : UiTheme::Color_TextMuted());
            DrawTextW(hdc, text, -1, &rci, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

            // accent underline
            if (selected)
            {
                HGDIOBJ oldPen2 = SelectObject(hdc, PenAccent());
                MoveToEx(hdc, rci.left + 2, rci.bottom - 2, nullptr);
                LineTo(hdc, rci.right - 2, rci.bottom - 2);
                SelectObject(hdc, oldPen2);
            }
        }

        // border around page
        {
            HGDIOBJ oldPen = SelectObject(hdc, PenBorder());
            HGDIOBJ oldBrush = SelectObject(hdc, GetStockObject(HOLLOW_BRUSH));
            Rectangle(hdc, rcPage.left, rcPage.top, rcPage.right, rcPage.bottom);
            SelectObject(hdc, oldBrush);
            SelectObject(hdc, oldPen);
        }

        SelectObject(hdc, oldFont);
    }

    static void PostSelChangedIfNeeded(HWND hTab, int oldSel)
    {
        int newSel = TabCtrl_GetCurSel(hTab);
        if (newSel != oldSel)
        {
            HWND parent = GetParent(hTab);
            if (parent)
                PostMessageW(parent, MsgSelChanged(), (WPARAM)hTab, 0);
        }
    }

    static LRESULT CALLBACK TabSubclassProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam,
        UINT_PTR, DWORD_PTR)
    {
        switch (msg)
        {
        case WM_ERASEBKGND:
            return 1;

        case WM_PRINTCLIENT:
        {
            HDC hdc = (HDC)wParam;
            if (hdc)
            {
                PaintTabControl(hWnd, hdc);
                return 0;
            }
            break;
        }

        case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);
            PaintTabControl(hWnd, hdc);
            EndPaint(hWnd, &ps);
            return 0;
        }

        // Reliable selection-change notification fallback:
        case WM_LBUTTONDOWN:
        case WM_LBUTTONUP:
        case WM_KEYDOWN:
        case WM_KEYUP:
        {
            int oldSel = TabCtrl_GetCurSel(hWnd);
            LRESULT r = DefSubclassProc(hWnd, msg, wParam, lParam);
            PostSelChangedIfNeeded(hWnd, oldSel);

            // ensure repaint on interaction
            InvalidateRect(hWnd, nullptr, FALSE);
            return r;
        }

        case WM_NCDESTROY:
            RemoveWindowSubclass(hWnd, TabSubclassProc, 1);
            break;
        }
        return DefSubclassProc(hWnd, msg, wParam, lParam);
    }

    inline void Apply(HWND hTab)
    {
        if (!hTab) return;

        // Ensure clipping helps too
        LONG_PTR style = GetWindowLongPtrW(hTab, GWL_STYLE);
        style |= WS_CLIPCHILDREN | WS_CLIPSIBLINGS;
        SetWindowLongPtrW(hTab, GWL_STYLE, style);
        SetWindowPos(hTab, nullptr, 0, 0, 0, 0,
            SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);

        SetWindowSubclass(hTab, TabSubclassProc, 1, 0);

        InvalidateRect(hTab, nullptr, TRUE);
    }
}
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "gamepad_render.h"
#include "ui_theme.h"

#include <algorithm>
#include <cmath>

static HPEN PenBorder()
{
    static HPEN p = CreatePen(PS_SOLID, 1, UiTheme::Color_Border());
    return p;
}

static HPEN PenMid()
{
    static HPEN p = CreatePen(PS_SOLID, 1, RGB(90, 90, 90));
    return p;
}

HBRUSH GamepadRender_GetAxisFillBrush()
{
    static HBRUSH br = CreateSolidBrush(UiTheme::Color_Accent());
    return br;
}

HBRUSH GamepadRender_GetTriggerFillBrush()
{
    static HBRUSH br = CreateSolidBrush(RGB(120, 210, 140));
    return br;
}

static void DrawFramedBox(HDC hdc, const RECT& rc)
{
    // fill background
    FillRect(hdc, &rc, UiTheme::Brush_ControlBg());

    // border
    HGDIOBJ oldPen = SelectObject(hdc, PenBorder());
    HGDIOBJ oldBrush = SelectObject(hdc, GetStockObject(HOLLOW_BRUSH));
    Rectangle(hdc, rc.left, rc.top, rc.right, rc.bottom);
    SelectObject(hdc, oldBrush);
    SelectObject(hdc, oldPen);
}

void GamepadRender_DrawAxisBarCentered(HDC hdc, RECT rc, SHORT v)
{
    DrawFramedBox(hdc, rc);

    // mid line
    int mid = (rc.left + rc.right) / 2;
    HGDIOBJ oldPen = SelectObject(hdc, PenMid());
    MoveToEx(hdc, mid, rc.top + 1, nullptr);
    LineTo(hdc, mid, rc.bottom - 1);
    SelectObject(hdc, oldPen);

    float x = (float)v / 32767.0f;
    x = std::clamp(x, -1.0f, 1.0f);

    RECT fill = rc;
    // keep inside border
    InflateRect(&fill, -1, -1);

    if (x >= 0.0f)
    {
        fill.left = mid;
        fill.right = mid + (int)lround((rc.right - mid - 1) * x);
    }
    else
    {
        fill.right = mid;
        fill.left = mid + (int)lround((mid - rc.left + 1) * x);
    }

    if (fill.right > fill.left)
        FillRect(hdc, &fill, GamepadRender_GetAxisFillBrush());
}

void GamepadRender_DrawTriggerBar01(HDC hdc, RECT rc, uint8_t v)
{
    DrawFramedBox(hdc, rc);

    float x = (float)v / 255.0f;
    x = std::clamp(x, 0.0f, 1.0f);

    RECT fill = rc;
    InflateRect(&fill, -1, -1);
    fill.right = fill.left + (int)lround((fill.right - fill.left) * x);

    if (fill.right > fill.left)
        FillRect(hdc, &fill, GamepadRender_GetTriggerFillBrush());
}

std::wstring GamepadRender_ButtonsToString(WORD w)
{
    std::wstring s;
    auto add = [&](WORD mask, const wchar_t* name)
        {
            if (w & mask)
            {
                if (!s.empty()) s += L" ";
                s += name;
            }
        };

    add(XUSB_GAMEPAD_A, L"A");
    add(XUSB_GAMEPAD_B, L"B");
    add(XUSB_GAMEPAD_X, L"X");
    add(XUSB_GAMEPAD_Y, L"Y");
    add(XUSB_GAMEPAD_LEFT_SHOULDER, L"LB");
    add(XUSB_GAMEPAD_RIGHT_SHOULDER, L"RB");
    add(XUSB_GAMEPAD_BACK, L"Back");
    add(XUSB_GAMEPAD_START, L"Start");
    add(XUSB_GAMEPAD_GUIDE, L"Guide");
    add(XUSB_GAMEPAD_LEFT_THUMB, L"LS");
    add(XUSB_GAMEPAD_RIGHT_THUMB, L"RS");
    add(XUSB_GAMEPAD_DPAD_UP, L"DU");
    add(XUSB_GAMEPAD_DPAD_DOWN, L"DD");
    add(XUSB_GAMEPAD_DPAD_LEFT, L"DL");
    add(XUSB_GAMEPAD_DPAD_RIGHT, L"DR");

    if (s.empty()) s = L"(none)";
    return s;
}
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>

#include <string>
#include <cstdint>

#include "keyboard_bind_panel.h"
#include "bindings.h"
#include "profile_ini.h"
#include "win_util.h"
#include "app_paths.h"
#include "binding_actions.h"

// Scaling shortcut
static int S(HWND hwnd, int px) { return WinUtil_ScalePx(hwnd, px); }

static uint16_t g_selectedHid = 0;

static HWND g_lblSelected = nullptr;
static HWND g_cbAction = nullptr;
static HWND g_btnBind = nullptr;
static HWND g_btnClear = nullptr;

static constexpr int ID_BIND = 3001;
static constexpr int ID_CLEAR = 3002;
static constexpr int ID_ACTION = 3005;

struct ActionItem
{
    const wchar_t* name;
    BindAction action;
};

static const ActionItem g_actions[] =
{
    {L"Axis LX -", BindAction::Axis_LX_Minus},
    {L"Axis LX +", BindAction::Axis_LX_Plus},
    {L"Axis LY -", BindAction::Axis_LY_Minus},
    {L"Axis LY +", BindAction::Axis_LY_Plus},
    {L"Axis RX -", BindAction::Axis_RX_Minus},
    {L"Axis RX +", BindAction::Axis_RX_Plus},
    {L"Axis RY -", BindAction::Axis_RY_Minus},
    {L"Axis RY +", BindAction::Axis_RY_Plus},

    {L"Trigger LT", BindAction::Trigger_LT},
    {L"Trigger RT", BindAction::Trigger_RT},

    {L"Button A", BindAction::Btn_A},
    {L"Button B", BindAction::Btn_B},
    {L"Button X", BindAction::Btn_X},
    {L"Button Y", BindAction::Btn_Y},
    {L"Button LB", BindAction::Btn_LB},
    {L"Button RB", BindAction::Btn_RB},
    {L"Button Back", BindAction::Btn_Back},
    {L"Button Start", BindAction::Btn_Start},
    {L"Button Guide", BindAction::Btn_Guide},
    {L"Button LS", BindAction::Btn_LS},
    {L"Button RS", BindAction::Btn_RS},
    {L"DPad Up", BindAction::Btn_DU},
    {L"DPad Down", BindAction::Btn_DD},
    {L"DPad Left", BindAction::Btn_DL},
    {L"DPad Right", BindAction::Btn_DR},
};

static void UpdateSelectedLabel()
{
    if (!g_lblSelected) return;
    wchar_t buf[128];
    if (g_selectedHid == 0)
        swprintf_s(buf, L"Selected HID: (none)");
    else
        swprintf_s(buf, L"Selected HID: 0x%02X (%u)", g_selectedHid, (unsigned)g_selectedHid);
    SetWindowTextW(g_lblSelected, buf);
}

static BindAction GetSelectedAction()
{
    int idx = (int)SendMessageW(g_cbAction, CB_GETCURSEL, 0, 0);
    if (idx < 0) idx = 0;
    if (idx >= (int)(sizeof(g_actions) / sizeof(g_actions[0])))
        idx = 0;
    return g_actions[idx].action;
}

HWND BindPanel_Create(HWND parent, HINSTANCE hInst)
{
    HFONT hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);

    int x = S(parent, 800);
    int y = S(parent, 12);
    int w = S(parent, 360);
    int h = S(parent, 20);

    g_lblSelected = CreateWindowW(L"STATIC", L"Selected HID: (none)",
        WS_CHILD | WS_VISIBLE, x, y, w, h, parent, nullptr, hInst, nullptr);
    SendMessageW(g_lblSelected, WM_SETFONT, (WPARAM)hFont, TRUE);

    g_cbAction = CreateWindowW(WC_COMBOBOXW, L"",
        WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST,
        x, y + S(parent, 26), w, S(parent, 300), parent, (HMENU)ID_ACTION, hInst, nullptr);
    SendMessageW(g_cbAction, WM_SETFONT, (WPARAM)hFont, TRUE);

    for (int i = 0; i < (int)(sizeof(g_actions) / sizeof(g_actions[0])); i++)
        SendMessageW(g_cbAction, CB_ADDSTRING, 0, (LPARAM)g_actions[i].name);
    SendMessageW(g_cbAction, CB_SETCURSEL, 0, 0);

    g_btnBind = CreateWindowW(L"BUTTON", L"Bind",
        WS_CHILD | WS_VISIBLE, x, y + S(parent, 58), S(parent, 80), S(parent, 26),
        parent, (HMENU)ID_BIND, hInst, nullptr);

    g_btnClear = CreateWindowW(L"BUTTON", L"Clear HID",
        WS_CHILD | WS_VISIBLE, x + S(parent, 86), y + S(parent, 58), S(parent, 100), S(parent, 26),
        parent, (HMENU)ID_CLEAR, hInst, nullptr);

    SendMessageW(g_btnBind, WM_SETFONT, (WPARAM)hFont, TRUE);
    SendMessageW(g_btnClear, WM_SETFONT, (WPARAM)hFont, TRUE);

    // still load automatically at startup
    Profile_LoadIni(AppPaths_ActiveBindingsIni().c_str());
    return g_lblSelected;
}

void BindPanel_SetSelectedHid(uint16_t hid)
{
    g_selectedHid = hid;
    UpdateSelectedLabel();
}

bool BindPanel_HandleCommand(HWND parent, WPARAM wParam, LPARAM)
{
    int id = LOWORD(wParam);

    if (id == ID_BIND)
    {
        BindingActions_Apply(GetSelectedAction(), g_selectedHid);
        Profile_SaveIni(AppPaths_ActiveBindingsIni().c_str()); // autosave
        InvalidateRect(parent, nullptr, FALSE);
        return true;
    }
    if (id == ID_CLEAR)
    {
        Bindings_ClearHid(g_selectedHid);
        Profile_SaveIni(AppPaths_ActiveBindingsIni().c_str()); // autosave
        InvalidateRect(parent, nullptr, FALSE);
        return true;
    }

    return false;
}

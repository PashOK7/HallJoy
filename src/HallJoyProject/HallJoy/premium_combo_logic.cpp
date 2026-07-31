// premium_combo_logic.cpp
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <commctrl.h>

#include <algorithm>
#include <cwctype>
#include <string>

#include "premium_combo_internal.h"
#include "ui_theme.h" // dark style for inline editor

using namespace PremiumComboInternal;

// ============================================================================
// Inline edit (premium rename/create) - DARK/PREMIUM STYLING
// ============================================================================
//
// IMPORTANT:
// The dropdown popup is a WS_EX_LAYERED window. Child windows inside a layered
// top-level window often flicker badly.
// Therefore we do NOT host EDIT inside the popup.
//
// Instead we create a tiny non-layered "host" WS_POPUP (dark painted),
// and put a borderless EDIT inside the host. The host is positioned exactly
// over the row text area (screen coords).
//
// NEW FIX (for Create New):
// Clicking the last row "+ Create New ..." should NOT close the dropdown.
// We start inline edit directly and keep dropdown open.

static LRESULT CALLBACK InlineEditSubclassProc(HWND hEdit, UINT msg, WPARAM wParam, LPARAM lParam,
    UINT_PTR, DWORD_PTR dwRefData);

static LRESULT CALLBACK EditHostProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

static constexpr const wchar_t* PC_EDIT_HOST_CLASS = L"PremiumCombo_EditHost";

static void EnsureEditHostClassRegistered(HINSTANCE hInst)
{
    static bool reg = false;
    if (reg) return;

    WNDCLASSW wc{};
    wc.lpfnWndProc = EditHostProc;
    wc.hInstance = hInst;
    wc.lpszClassName = PC_EDIT_HOST_CLASS;
    wc.hCursor = LoadCursorW(nullptr, IDC_IBEAM);
    wc.hbrBackground = nullptr;
    RegisterClassW(&wc);

    reg = true;
}

// Heuristic: treat the LAST item starting with '+' as "Create New..." row.
// This keeps PremiumCombo generic without extra API.
static bool IsCreateRow(State* st, int idx)
{
    if (!st) return false;
    if (idx < 0 || idx >= (int)st->items.size()) return false;
    if (idx != (int)st->items.size() - 1) return false;

    const std::wstring& t = st->items[idx];
    return (!t.empty() && t[0] == L'+');
}

bool PremiumComboInternal::IsInlineEditing(State* st)
{
    return (st && st->hwndEdit && IsWindow(st->hwndEdit) && st->editIndex >= 0);
}

RECT PremiumComboInternal::GetPopupItemTextRect(State* st, int idx)
{
    RECT empty{};
    if (!st || !st->hwndPopup || !st->hwnd) return empty;
    if (idx < 0 || idx >= (int)st->items.size()) return empty;

    RECT rc{};
    GetClientRect(st->hwndPopup, &rc);

    int w = rc.right - rc.left;
    int h = rc.bottom - rc.top;
    if (w <= 4 || h <= 4) return empty;

    int ih = GetItemHeightPx(st->hwnd, st);
    if (ih <= 0) return empty;

    int rows = GetVisibleRows(st);

    int row = idx - st->scrollTop;
    if (row < 0 || row >= rows) return empty;

    RECT rowRc{};
    rowRc.left = 0;
    rowRc.right = w;
    rowRc.top = row * ih;
    rowRc.bottom = rowRc.top + ih;

    RECT textRc = rowRc;
    textRc.left += S(st->hwnd, 10);
    textRc.right -= S(st->hwnd, 10);

    // shrink by buttons if any
    ItemBtnMask mask = BTN_NONE;
    if (idx >= 0 && idx < (int)st->itemBtnMask.size())
        mask = st->itemBtnMask[idx];

    if (mask != BTN_NONE)
    {
        RECT rDel = GetPopupItemButtonRect(st, idx, PremiumCombo::ItemButtonKind::Delete);
        RECT rRen = GetPopupItemButtonRect(st, idx, PremiumCombo::ItemButtonKind::Rename);

        int minLeft = INT_MAX;
        if (rDel.right > rDel.left) minLeft = std::min(minLeft, (int)rDel.left);
        if (rRen.right > rRen.left) minLeft = std::min(minLeft, (int)rRen.left);

        if (minLeft != INT_MAX)
            textRc.right = minLeft - S(st->hwnd, PC_ITEMBTN_GAP);
    }

    // inner pad so editor looks aligned with drawn text
    InflateRect(&textRc, -S(st->hwnd, PC_EDIT_PAD_X), -S(st->hwnd, PC_EDIT_PAD_Y));

    // clamp height a bit (avoid 0)
    if (textRc.bottom <= textRc.top + 6)
    {
        textRc.top = rowRc.top + 2;
        textRc.bottom = rowRc.bottom - 2;
    }

    return textRc;
}

static bool PopupClientRectToScreenRect(HWND hwndPopup, const RECT& rcClient, RECT& outScreen)
{
    outScreen = RECT{};
    if (!hwndPopup) return false;

    POINT p0{ rcClient.left, rcClient.top };
    POINT p1{ rcClient.right, rcClient.bottom };
    MapWindowPoints(hwndPopup, nullptr, &p0, 1);
    MapWindowPoints(hwndPopup, nullptr, &p1, 1);

    outScreen.left = p0.x;
    outScreen.top = p0.y;
    outScreen.right = p1.x;
    outScreen.bottom = p1.y;
    return true;
}

static HWND InlineEdit_GetHost(HWND hEdit)
{
    if (!hEdit) return nullptr;
    HWND host = GetParent(hEdit);
    if (!host) return nullptr;

    wchar_t cls[128]{};
    GetClassNameW(host, cls, 127);
    if (_wcsicmp(cls, PC_EDIT_HOST_CLASS) == 0)
        return host;

    return nullptr;
}

void PremiumComboInternal::UpdateInlineEditRect(State* st)
{
    if (!IsInlineEditing(st)) return;
    if (!st->hwndPopup) return;

    RECT trClient = GetPopupItemTextRect(st, st->editIndex);
    if (trClient.right <= trClient.left || trClient.bottom <= trClient.top) return;

    RECT trScreen{};
    if (!PopupClientRectToScreenRect(st->hwndPopup, trClient, trScreen))
        return;

    st->editRect = trClient;

    HWND host = InlineEdit_GetHost(st->hwndEdit);
    if (!host) return;

    int hostW = (int)(trScreen.right - trScreen.left);
    int hostH = (int)(trScreen.bottom - trScreen.top);
    if (hostW < 10) hostW = 10;
    if (hostH < 10) hostH = 10;

    // move host
    SetWindowPos(host, HWND_TOPMOST,
        trScreen.left, trScreen.top,
        hostW, hostH,
        SWP_NOACTIVATE | SWP_SHOWWINDOW);

    // keep edit inset so host border is visible
    int inset = 1;
    int editW = hostW - inset * 2;
    int editH = hostH - inset * 2;
    if (editW < 1) editW = 1;
    if (editH < 1) editH = 1;

    SetWindowPos(st->hwndEdit, nullptr,
        inset, inset,
        editW, editH,
        SWP_NOZORDER | SWP_NOACTIVATE | SWP_SHOWWINDOW);

    if (st->hwndPopup)
        InvalidateRect(st->hwndPopup, nullptr, FALSE);
}

void PremiumComboInternal::StoreCommittedText(State* st, int idx, PremiumCombo::ItemButtonKind kind, const std::wstring& txt)
{
    if (!st) return;
    st->committedText = txt;
    st->committedIndex = idx;
    st->committedKind = kind;

    if (st->parent && st->hwnd)
    {
        WPARAM wp = MAKEWPARAM((UINT)idx, (UINT)(int)kind);
        PostMessageW(st->parent, PremiumCombo::MsgItemTextCommit(), wp, (LPARAM)st->hwnd);
    }
}

void PremiumComboInternal::EndInlineEdit(State* st, bool commit)
{
    if (!IsInlineEditing(st)) return;

    HWND hEdit = st->hwndEdit;
    HWND host = InlineEdit_GetHost(hEdit);
    int idx = st->editIndex;

    std::wstring text;
    if (commit && hEdit && IsWindow(hEdit))
    {
        int len = GetWindowTextLengthW(hEdit);
        if (len < 0) len = 0;

        text.resize((size_t)len);
        if (len > 0)
            GetWindowTextW(hEdit, text.data(), len + 1);
        else
            text.clear();
    }

    // Destroy host (destroys edit as child)
    if (host && IsWindow(host))
        DestroyWindow(host);
    else if (hEdit && IsWindow(hEdit))
        DestroyWindow(hEdit);

    st->hwndEdit = nullptr;
    st->editIndex = -1;
    st->editRect = RECT{};

    if (commit)
        StoreCommittedText(st, idx, PremiumCombo::ItemButtonKind::Rename, text);

    if (st->hwndPopup)
        InvalidateRect(st->hwndPopup, nullptr, FALSE);
}

void PremiumComboInternal::BeginInlineEdit(State* st, int idx)
{
    if (!st) return;
    if (!st->dropped || !st->hwndPopup) return;
    if (!st->enabled) return;

    if (idx < 0 || idx >= (int)st->items.size()) return;

    // If editing already, commit current edit before switching
    if (IsInlineEditing(st))
        EndInlineEdit(st, true);

    EnsureIndexVisible(st, idx);

    RECT trClient = GetPopupItemTextRect(st, idx);
    if (trClient.right <= trClient.left || trClient.bottom <= trClient.top) return;

    RECT trScreen{};
    if (!PopupClientRectToScreenRect(st->hwndPopup, trClient, trScreen))
        return;

    HINSTANCE hInst = (HINSTANCE)GetWindowLongPtrW(st->hwnd, GWLP_HINSTANCE);
    HWND ownerTop = GetAncestor(st->hwnd, GA_ROOT);

    EnsureEditHostClassRegistered(hInst);

    int hostW = (int)(trScreen.right - trScreen.left);
    int hostH = (int)(trScreen.bottom - trScreen.top);
    if (hostW < 10) hostW = 10;
    if (hostH < 10) hostH = 10;

    // Host: topmost popup, dark-painted
    DWORD hostStyle = WS_POPUP | WS_VISIBLE;
    DWORD hostEx = WS_EX_TOOLWINDOW | WS_EX_TOPMOST;

    HWND hHost = CreateWindowExW(
        hostEx,
        PC_EDIT_HOST_CLASS,
        L"",
        hostStyle,
        trScreen.left, trScreen.top,
        hostW, hostH,
        ownerTop,
        nullptr,
        hInst,
        st);

    if (!hHost)
        return;

    // Edit: borderless child of host
    DWORD editStyle = WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL;

    int inset = 1;
    int editW = hostW - inset * 2;
    int editH = hostH - inset * 2;
    if (editW < 1) editW = 1;
    if (editH < 1) editH = 1;

    // NEW: For Create-row we want an EMPTY editor, not "+ Create New ..."
    const wchar_t* initialText = IsCreateRow(st, idx) ? L"" : st->items[idx].c_str();

    HWND hEdit = CreateWindowExW(
        0,
        L"EDIT",
        initialText,
        editStyle,
        inset, inset,
        editW, editH,
        hHost,
        nullptr,
        hInst,
        nullptr);

    if (!hEdit)
    {
        DestroyWindow(hHost);
        return;
    }

    st->hwndEdit = hEdit;
    st->editIndex = idx;
    st->editRect = trClient;

    // Font + margins
    SendMessageW(hEdit, WM_SETFONT, (WPARAM)GetFont(st), TRUE);

#ifndef EM_SETMARGINS
#define EM_SETMARGINS 0x00D3
#endif
#ifndef EC_LEFTMARGIN
#define EC_LEFTMARGIN 0x0001
#endif
#ifndef EC_RIGHTMARGIN
#define EC_RIGHTMARGIN 0x0002
#endif
    int m = S(st->hwnd, 6);
    SendMessageW(hEdit, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN, MAKELPARAM(m, m));

#ifndef EM_SETBKGNDCOLOR
#define EM_SETBKGNDCOLOR 0x043D
#endif
    SendMessageW(hEdit, EM_SETBKGNDCOLOR, 0, (LPARAM)UiTheme::Color_ControlBg());

    // Subclass for Enter/Esc and focus-loss commit
    SetWindowSubclass(hEdit, InlineEditSubclassProc, 1, (DWORD_PTR)st);

    // Focus + select all
    SetFocus(hEdit);
    SendMessageW(hEdit, EM_SETSEL, 0, -1);

    // Drop capture so EDIT can get mouse input properly
    if (st->captureActive && GetCapture() == st->hwnd)
    {
        st->captureActive = false;
        ReleaseCapture();
    }

    UpdateInlineEditRect(st);

    if (st->hwndPopup)
        InvalidateRect(st->hwndPopup, nullptr, FALSE);
}

// Host draws border + supplies colors for child EDIT
static LRESULT CALLBACK EditHostProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    auto* st = (State*)GetWindowLongPtrW(hWnd, GWLP_USERDATA);
    (void)st;

    switch (msg)
    {
    case WM_NCCREATE:
        return TRUE;

    case WM_CREATE:
    {
        auto* cs = (CREATESTRUCTW*)lParam;
        st = (State*)cs->lpCreateParams;
        SetWindowLongPtrW(hWnd, GWLP_USERDATA, (LONG_PTR)st);
        return 0;
    }

    case WM_ERASEBKGND:
        return 1;

    case WM_CTLCOLOREDIT:
    {
        HDC hdc = (HDC)wParam;
        SetBkMode(hdc, OPAQUE);
        SetBkColor(hdc, UiTheme::Color_ControlBg());
        SetTextColor(hdc, UiTheme::Color_Text());
        return (LRESULT)UiTheme::Brush_ControlBg();
    }

    case WM_PAINT:
    {
        PAINTSTRUCT ps{};
        HDC hdc = BeginPaint(hWnd, &ps);

        RECT rc{};
        GetClientRect(hWnd, &rc);

        FillRect(hdc, &rc, UiTheme::Brush_ControlBg());

        HPEN pen = CreatePen(PS_SOLID, 1, UiTheme::Color_Border());
        HGDIOBJ oldPen = SelectObject(hdc, pen);
        HGDIOBJ oldBr = SelectObject(hdc, GetStockObject(HOLLOW_BRUSH));
        Rectangle(hdc, rc.left, rc.top, rc.right, rc.bottom);
        SelectObject(hdc, oldBr);
        SelectObject(hdc, oldPen);
        DeleteObject(pen);

        EndPaint(hWnd, &ps);
        return 0;
    }

    case WM_NCDESTROY:
        SetWindowLongPtrW(hWnd, GWLP_USERDATA, 0);
        return 0;
    }

    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

static LRESULT CALLBACK InlineEditSubclassProc(HWND hEdit, UINT msg, WPARAM wParam, LPARAM lParam,
    UINT_PTR, DWORD_PTR dwRefData)
{
    auto* st = (State*)dwRefData;

    switch (msg)
    {
    case WM_KEYDOWN:
        if (!st) break;

        if (wParam == VK_RETURN)
        {
            PremiumComboInternal::EndInlineEdit(st, true);
            if (st->hwnd) SetFocus(st->hwnd);
            return 0;
        }
        if (wParam == VK_ESCAPE)
        {
            PremiumComboInternal::EndInlineEdit(st, false);
            if (st->hwnd) SetFocus(st->hwnd);
            return 0;
        }
        break;

    case WM_KILLFOCUS:
        if (st && PremiumComboInternal::IsInlineEditing(st))
        {
            PremiumComboInternal::EndInlineEdit(st, true);
            PremiumComboInternal::CloseDropDown(st, false);
            return 0;
        }
        break;

    case WM_NCDESTROY:
        RemoveWindowSubclass(hEdit, InlineEditSubclassProc, 1);
        break;
    }

    return DefSubclassProc(hEdit, msg, wParam, lParam);
}

// ============================================================================
// Metrics / geometry
// ============================================================================
int PremiumComboInternal::GetItemHeightPx(HWND hwndCombo, State* st)
{
    if (!hwndCombo) return 28;

    if (st && st->itemHeightPx > 0)
        return std::max(10, st->itemHeightPx);

    RECT rc{};
    GetClientRect(hwndCombo, &rc);
    int h = rc.bottom - rc.top;
    if (h <= 0) h = S(hwndCombo, 28);
    return std::max(10, h);
}

int PremiumComboInternal::GetVisibleRows(State* st)
{
    if (!st) return 1;
    int n = (int)st->items.size();
    int maxVis = std::clamp(st->dropMaxVisible, 1, 40);

    if (n <= 0) return 1;
    return std::clamp(n, 1, maxVis);
}

int PremiumComboInternal::GetMaxScrollTop(State* st)
{
    if (!st) return 0;
    int n = (int)st->items.size();
    int rows = GetVisibleRows(st);
    if (n <= rows) return 0;
    return std::max(0, n - rows);
}

void PremiumComboInternal::ClampScroll(State* st)
{
    if (!st) return;
    st->scrollTop = std::clamp(st->scrollTop, 0, GetMaxScrollTop(st));

    if (IsInlineEditing(st))
        UpdateInlineEditRect(st);
}

void PremiumComboInternal::EnsureIndexVisible(State* st, int idx)
{
    if (!st) return;
    if (idx < 0) return;

    int rows = GetVisibleRows(st);
    int top = st->scrollTop;
    int bot = top + rows - 1;

    if (idx < top) top = idx;
    else if (idx > bot) top = idx - (rows - 1);

    st->scrollTop = top;
    ClampScroll(st);

    if (IsInlineEditing(st))
        UpdateInlineEditRect(st);
}

void PremiumComboInternal::ClampPopupToMonitor(RECT& r)
{
    HMONITOR mon = MonitorFromRect(&r, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi{};
    mi.cbSize = sizeof(mi);
    if (!GetMonitorInfoW(mon, &mi)) return;

    RECT wa = mi.rcWork;

    int w = r.right - r.left;
    int h = r.bottom - r.top;

    if (r.left < wa.left) r.left = wa.left;
    if (r.top < wa.top) r.top = wa.top;
    if (r.left + w > wa.right) r.left = wa.right - w;
    if (r.top + h > wa.bottom) r.top = wa.bottom - h;

    r.right = r.left + w;
    r.bottom = r.top + h;
}

// ============================================================================
// Extra icon / item button geometry + hit-testing
// ============================================================================
RECT PremiumComboInternal::GetExtraIconRect(State* st)
{
    RECT empty{};
    if (!st || !st->hwnd) return empty;
    if (st->extraIcon == PremiumCombo::ExtraIconKind::None) return empty;

    RECT rc{};
    GetClientRect(st->hwnd, &rc);

    int w = rc.right - rc.left;
    int h = rc.bottom - rc.top;
    if (w <= 4 || h <= 4) return empty;

    int arrowW = std::clamp(S(st->hwnd, 26), 18, 34);
    int arrowLeft = rc.right - arrowW;

    int size = S(st->hwnd, PC_EXTRAICON_SIZE);
    size = std::clamp(size, 10, std::max(10, h - 6));

    int padX = S(st->hwnd, PC_EXTRAICON_PAD_X);

    int right = arrowLeft - padX;
    int left = right - size;
    int top = rc.top + (h - size) / 2;
    int bottom = top + size;

    if (left < rc.left + 2) return empty;

    RECT r{ left, top, right, bottom };
    return r;
}

bool PremiumComboInternal::HitTestExtraIcon(State* st, POINT ptClient)
{
    RECT r = GetExtraIconRect(st);
    if (r.right <= r.left) return false;

    return (ptClient.x >= r.left && ptClient.x < r.right &&
        ptClient.y >= r.top && ptClient.y < r.bottom);
}

RECT PremiumComboInternal::GetPopupItemButtonRect(State* st, int idx, PremiumCombo::ItemButtonKind kind)
{
    RECT empty{};
    if (!st || !st->hwndPopup || !st->hwnd) return empty;
    if (idx < 0) return empty;

    if (idx >= (int)st->items.size()) return empty;
    if (idx >= (int)st->itemBtnMask.size()) return empty;

    ItemBtnMask mask = st->itemBtnMask[idx];
    if (!MaskHas(mask, kind)) return empty;

    RECT rc{};
    GetClientRect(st->hwndPopup, &rc);

    int w = rc.right - rc.left;
    int h = rc.bottom - rc.top;
    if (w <= 4 || h <= 4) return empty;

    int ih = GetItemHeightPx(st->hwnd, st);
    if (ih <= 0) return empty;

    int rows = GetVisibleRows(st);
    int row = idx - st->scrollTop;
    if (row < 0 || row >= rows) return empty;

    int size = S(st->hwnd, PC_ITEMBTN_SIZE);
    size = std::clamp(size, 12, std::max(12, ih - 4));

    int padR = S(st->hwnd, PC_ITEMBTN_PAD_R);
    int gapBetween = S(st->hwnd, PC_ITEMBTN_GAP_BETWEEN);

    int rightEdge = rc.right - 2 - padR;

    bool hasDel = (mask & BTN_DELETE) != 0;

    int rightForKind = rightEdge;

    if (kind == PremiumCombo::ItemButtonKind::Delete)
        rightForKind = rightEdge;
    else if (kind == PremiumCombo::ItemButtonKind::Rename)
        rightForKind = hasDel ? (rightEdge - size - gapBetween) : rightEdge;
    else
        return empty;

    int left = rightForKind - size;
    int right = rightForKind;

    int top = row * ih + (ih - size) / 2;
    int bottom = top + size;

    RECT r{ left, top, right, bottom };
    return r;
}

bool PremiumComboInternal::HitTestPopupItemButtonFromScreen(State* st, POINT ptScreen,
    int& outIdx, PremiumCombo::ItemButtonKind& outKind, bool& outInsidePopup)
{
    outIdx = -1;
    outKind = PremiumCombo::ItemButtonKind::None;
    outInsidePopup = false;

    if (!st || !st->hwndPopup) return false;

    bool inside = false;
    int idx = HitTestPopupIndexFromScreen(st, ptScreen, inside);
    outInsidePopup = inside;

    if (!inside || idx < 0) return false;
    if (idx >= (int)st->items.size()) return false;
    if (idx >= (int)st->itemBtnMask.size()) return false;

    POINT pt = ptScreen;
    ScreenToClient(st->hwndPopup, &pt);

    if (MaskHas(st->itemBtnMask[idx], PremiumCombo::ItemButtonKind::Delete))
    {
        RECT br = GetPopupItemButtonRect(st, idx, PremiumCombo::ItemButtonKind::Delete);
        if (br.right > br.left &&
            pt.x >= br.left && pt.x < br.right &&
            pt.y >= br.top && pt.y < br.bottom)
        {
            outIdx = idx;
            outKind = PremiumCombo::ItemButtonKind::Delete;
            return true;
        }
    }

    if (MaskHas(st->itemBtnMask[idx], PremiumCombo::ItemButtonKind::Rename))
    {
        RECT br = GetPopupItemButtonRect(st, idx, PremiumCombo::ItemButtonKind::Rename);
        if (br.right > br.left &&
            pt.x >= br.left && pt.x < br.right &&
            pt.y >= br.top && pt.y < br.bottom)
        {
            outIdx = idx;
            outKind = PremiumCombo::ItemButtonKind::Rename;
            return true;
        }
    }

    return false;
}

// ============================================================================
// Type-to-search
// ============================================================================
void PremiumComboInternal::ResetTypeSearch(State* st)
{
    if (!st) return;
    st->typeBuf.clear();
    st->lastTypeTick = 0;
}

static bool StartsWithI(const std::wstring& s, const std::wstring& prefix)
{
    if (prefix.empty()) return true;
    if (s.size() < prefix.size()) return false;

    for (size_t i = 0; i < prefix.size(); ++i)
    {
        wchar_t a = towlower(s[i]);
        wchar_t b = towlower(prefix[i]);
        if (a != b) return false;
    }
    return true;
}

static int FindByPrefix(State* st, const std::wstring& prefix, int startIdx)
{
    if (!st) return -1;
    int n = (int)st->items.size();
    if (n <= 0) return -1;
    if (prefix.empty()) return -1;

    startIdx = std::clamp(startIdx, 0, n - 1);

    for (int pass = 0; pass < 2; ++pass)
    {
        int i0 = (pass == 0) ? startIdx : 0;
        int i1 = (pass == 0) ? n : startIdx;

        for (int i = i0; i < i1; ++i)
            if (StartsWithI(st->items[i], prefix))
                return i;
    }
    return -1;
}

void PremiumComboInternal::TypeSearchApply(State* st, wchar_t ch, bool droppedMode)
{
    if (!st) return;
    if (!st->enabled) return;
    if (IsInlineEditing(st)) return;

    DWORD now = GetTickCount();
    if (st->lastTypeTick != 0 && (now - st->lastTypeTick) > 900)
        st->typeBuf.clear();
    st->lastTypeTick = now;

    if (ch == L'\b')
    {
        if (!st->typeBuf.empty())
            st->typeBuf.pop_back();
        return;
    }

    if (!iswprint((wint_t)ch)) return;
    if (ch < 32) return;

    st->typeBuf.push_back(ch);

    int n = (int)st->items.size();
    if (n <= 0) return;

    int base = 0;
    if (droppedMode)
        base = (st->hotIndex >= 0) ? (st->hotIndex + 1) : ((st->curSel >= 0) ? (st->curSel + 1) : 0);
    else
        base = (st->curSel >= 0) ? (st->curSel + 1) : 0;

    if (base >= n) base = 0;

    int found = FindByPrefix(st, st->typeBuf, base);
    if (found < 0)
    {
        std::wstring one(1, ch);
        found = FindByPrefix(st, one, base);
        if (found >= 0)
            st->typeBuf = one;
    }

    if (found < 0) return;

    if (droppedMode)
    {
        st->hotIndex = found;
        EnsureIndexVisible(st, st->hotIndex);

        st->hotBtnIndex = -1;
        st->hotBtnKind = PremiumCombo::ItemButtonKind::None;

        if (st->hwndPopup)
            InvalidateRect(st->hwndPopup, nullptr, FALSE);
    }
    else
    {
        if (found != st->curSel)
        {
            st->curSel = found;
            NotifySelChange(st);
        }
        if (st->hwnd)
            InvalidateRect(st->hwnd, nullptr, FALSE);
    }
}

// ============================================================================
// Popup hit-test
// ============================================================================
int PremiumComboInternal::HitTestPopupIndexFromScreen(State* st, POINT ptScreen, bool& outInsidePopup)
{
    outInsidePopup = false;
    if (!st || !st->hwndPopup) return -1;

    RECT pr{};
    GetWindowRect(st->hwndPopup, &pr);

    if (!(ptScreen.x >= pr.left && ptScreen.x < pr.right &&
        ptScreen.y >= pr.top && ptScreen.y < pr.bottom))
        return -1;

    outInsidePopup = true;

    POINT pt = ptScreen;
    ScreenToClient(st->hwndPopup, &pt);

    int ih = GetItemHeightPx(st->hwnd, st);
    if (ih <= 0) return -1;

    int rel = pt.y / ih;
    if (rel < 0) return -1;

    int idx = st->scrollTop + rel;
    if (idx < 0) return -1;
    if (idx >= (int)st->items.size()) return -1;
    return idx;
}

// ============================================================================
// Dropdown input helpers
// ============================================================================
void PremiumComboInternal::DropdownMouseMove(State* st)
{
    if (!st || !st->dropped) return;
    if (!st->hwndPopup) return;
    if (IsInlineEditing(st)) return;

    POINT pt{};
    GetCursorPos(&pt);

    int newHotIndex = -1;
    {
        bool inside = false;
        int idxRow = HitTestPopupIndexFromScreen(st, pt, inside);
        newHotIndex = inside ? idxRow : -1;
    }

    int btnIdx = -1;
    PremiumCombo::ItemButtonKind btnKind = PremiumCombo::ItemButtonKind::None;
    bool insidePopup = false;
    bool onBtn = HitTestPopupItemButtonFromScreen(st, pt, btnIdx, btnKind, insidePopup);

    int newHotBtnIndex = onBtn ? btnIdx : -1;
    PremiumCombo::ItemButtonKind newHotBtnKind = onBtn ? btnKind : PremiumCombo::ItemButtonKind::None;

    bool changed =
        (newHotIndex != st->hotIndex) ||
        (newHotBtnIndex != st->hotBtnIndex) ||
        (newHotBtnKind != st->hotBtnKind);

    st->hotIndex = newHotIndex;
    st->hotBtnIndex = newHotBtnIndex;
    st->hotBtnKind = newHotBtnKind;

    if (changed)
        InvalidateRect(st->hwndPopup, nullptr, FALSE);
}

void PremiumComboInternal::DropdownClick(State* st)
{
    if (!st || !st->dropped) return;

    POINT pt{};
    GetCursorPos(&pt);

    // If click is on an item button:
    {
        int btnIdx = -1;
        PremiumCombo::ItemButtonKind btnKind = PremiumCombo::ItemButtonKind::None;
        bool insidePopup = false;
        if (HitTestPopupItemButtonFromScreen(st, pt, btnIdx, btnKind, insidePopup) && insidePopup)
        {
            if (btnKind == PremiumCombo::ItemButtonKind::Rename)
            {
                BeginInlineEdit(st, btnIdx);
                return;
            }

            if (btnKind == PremiumCombo::ItemButtonKind::Delete)
            {
                if (st->parent)
                {
                    WPARAM wp = MAKEWPARAM((UINT)btnIdx, (UINT)(int)btnKind);
                    PostMessageW(st->parent, PremiumCombo::MsgItemButton(), wp, (LPARAM)st->hwnd);
                }

                // IMPORTANT:
                // Do NOT close dropdown here.
                // Parent may implement confirmation (two-click delete).
                return;
            }
        }
    }

    if (IsInlineEditing(st)) return;

    bool inside = false;
    int idx = HitTestPopupIndexFromScreen(st, pt, inside);

    if (inside && idx >= 0 && idx < (int)st->items.size())
    {
        // NEW: clicking "+ Create New ..." starts inline edit and DOES NOT close dropdown
        if (IsCreateRow(st, idx))
        {
            BeginInlineEdit(st, idx);
            return;
        }

        if (idx != st->curSel)
        {
            st->curSel = idx;
            NotifySelChange(st);
        }
    }

    // Mouse-down selection, then close (normal rows)
    CloseDropDown(st, true);
}

void PremiumComboInternal::MoveHot(State* st, int delta)
{
    if (!st) return;
    if (IsInlineEditing(st)) return;

    int n = (int)st->items.size();
    if (n <= 0) return;

    int cur = st->hotIndex;
    if (cur < 0) cur = st->curSel;
    if (cur < 0) cur = 0;

    int nxt = std::clamp(cur + delta, 0, n - 1);
    if (nxt == st->hotIndex) return;

    st->hotIndex = nxt;
    EnsureIndexVisible(st, st->hotIndex);

    st->hotBtnIndex = -1;
    st->hotBtnKind = PremiumCombo::ItemButtonKind::None;

    if (st->hwndPopup)
        InvalidateRect(st->hwndPopup, nullptr, FALSE);
}

void PremiumComboInternal::PageHot(State* st, int pages)
{
    if (!st) return;
    if (IsInlineEditing(st)) return;
    int rows = GetVisibleRows(st);
    MoveHot(st, pages * rows);
}

void PremiumComboInternal::HotToEdge(State* st, bool toEnd)
{
    if (!st) return;
    if (IsInlineEditing(st)) return;

    int n = (int)st->items.size();
    if (n <= 0) return;

    st->hotIndex = toEnd ? (n - 1) : 0;
    EnsureIndexVisible(st, st->hotIndex);

    st->hotBtnIndex = -1;
    st->hotBtnKind = PremiumCombo::ItemButtonKind::None;

    if (st->hwndPopup)
        InvalidateRect(st->hwndPopup, nullptr, FALSE);
}

// ============================================================================
// Hover hit-test (closed combo)
// ============================================================================
void PremiumComboInternal::UpdateHoverState(HWND hWnd, State* st, LPARAM lParam)
{
    if (!st) return;

    RECT rc{};
    GetClientRect(hWnd, &rc);

    POINT pt{ (short)LOWORD(lParam), (short)HIWORD(lParam) };

    bool inside = (pt.x >= 0 && pt.x < (rc.right - rc.left) &&
        pt.y >= 0 && pt.y < (rc.bottom - rc.top));

    bool newHovered = inside;

    int arrowW = std::clamp(S(hWnd, 26), 18, 34);
    bool newArrowHot = inside && (pt.x >= (rc.right - arrowW));

    bool newExtraHot = false;
    if (inside && st->extraIcon != PremiumCombo::ExtraIconKind::None)
        newExtraHot = HitTestExtraIcon(st, pt);

    bool changed =
        (newHovered != st->hovered) ||
        (newArrowHot != st->arrowHot) ||
        (newExtraHot != st->extraIconHot);

    st->hovered = newHovered;
    st->arrowHot = newArrowHot;
    st->extraIconHot = newExtraHot;

    if (newHovered)
    {
        TRACKMOUSEEVENT tme{};
        tme.cbSize = sizeof(tme);
        tme.dwFlags = TME_LEAVE;
        tme.hwndTrack = hWnd;
        TrackMouseEvent(&tme);
    }

    if (changed)
        InvalidateRect(hWnd, nullptr, FALSE);
}
// premium_combo_core.cpp
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include "premium_combo.h"
#include "premium_combo_internal.h"

using namespace PremiumComboInternal;

// ============================================================================
// Internal: GWLP_USERDATA helpers
// ============================================================================
State* PremiumComboInternal::Get(HWND hWnd)
{
    return (State*)GetWindowLongPtrW(hWnd, GWLP_USERDATA);
}

void PremiumComboInternal::Set(HWND hWnd, State* st)
{
    SetWindowLongPtrW(hWnd, GWLP_USERDATA, (LONG_PTR)st);
}

HFONT PremiumComboInternal::GetFont(State* st)
{
    if (st && st->font) return st->font;
    return (HFONT)GetStockObject(DEFAULT_GUI_FONT);
}

void PremiumComboInternal::NotifySelChange(State* st)
{
    if (!st || !st->parent) return;
    SendMessageW(st->parent, WM_COMMAND,
        MAKEWPARAM((UINT)st->controlId, CBN_SELCHANGE),
        (LPARAM)st->hwnd);
}

// ============================================================================
// RegisterOnce
// ============================================================================
void PremiumComboInternal::RegisterOnce(HINSTANCE hInst)
{
    static bool done = false;
    if (done) return;
    done = true;

    {
        WNDCLASSW wc{};
        wc.lpfnWndProc = PremiumComboInternal::ComboProc;
        wc.hInstance = hInst;
        wc.lpszClassName = PC_COMBO_CLASS;
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.hbrBackground = nullptr;
        RegisterClassW(&wc);
    }

    {
        WNDCLASSW wc{};
        wc.lpfnWndProc = PremiumComboInternal::PopupProc;
        wc.hInstance = hInst;
        wc.lpszClassName = PC_POPUP_CLASS;
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.hbrBackground = nullptr;
        RegisterClassW(&wc);
    }
}

// ============================================================================
// Extra icon animation (local helpers)
// ============================================================================
static void StartExtraIconAnim(State* st, float to, DWORD durMs)
{
    if (!st || !st->hwnd) return;

    to = Clamp01(to);
    durMs = std::max<DWORD>(1, durMs);

    st->extraIconAnimRunning = true;
    st->extraIconAnimStartTick = GetTickCount();
    st->extraIconAnimDurationMs = durMs;

    st->extraIconFrom = Clamp01(st->extraIconT);
    st->extraIconTo = to;
    st->extraIconT = st->extraIconFrom;

    PremiumComboInternal::EnsureAnimTimer(st);

    InvalidateRect(st->hwnd, nullptr, FALSE);
}

static void TickExtraIconAnim(State* st)
{
    if (!st || !st->extraIconAnimRunning) return;

    DWORD now = GetTickCount();
    DWORD dt = now - st->extraIconAnimStartTick;
    DWORD dur = (st->extraIconAnimDurationMs > 0) ? st->extraIconAnimDurationMs : 1;

    float t = Clamp01((float)dt / (float)dur);
    float e = EaseOutCubic(t);

    st->extraIconT = st->extraIconFrom + (st->extraIconTo - st->extraIconFrom) * e;

    if (t >= 1.0f - 1e-4f)
    {
        st->extraIconT = st->extraIconTo;
        st->extraIconAnimRunning = false;

        // if fully hidden, stop drawing icon
        if (st->extraIconT <= 0.001f)
            st->extraIconDraw = PremiumCombo::ExtraIconKind::None;
        else
            st->extraIconDraw = st->extraIcon;
    }

    if (st->hwnd)
        InvalidateRect(st->hwnd, nullptr, FALSE);
}

// ============================================================================
// Selection change animation (closed combo scroll)
// ============================================================================
static void StartSelAnim(State* st, int fromSel, int toSel)
{
    if (!st || !st->hwnd) return;

    // First ever selection set: snap, no animation (prevents "boot animation")
    if (!st->selAnimInitialized)
    {
        st->selAnimInitialized = true;
        st->selAnimRunning = false;
        st->selAnimFromSel = toSel;
        st->selAnimToSel = toSel;
        st->selAnimDir = 1;
        st->selAnimDistPx = 0.0f;
        return;
    }

    if (fromSel == toSel)
        return;

    st->selAnimRunning = true;
    st->selAnimStartTick = GetTickCount();
    st->selAnimDurationMs = 160; // чуть дольше чем toggles, выглядит "премиальнее"

    st->selAnimFromSel = fromSel;
    st->selAnimToSel = toSel;

    // Direction: up/down by index (placeholder -1 considered)
    int dir = 1;
    if (fromSel < 0 && toSel >= 0) dir = +1;
    else if (fromSel >= 0 && toSel < 0) dir = -1;
    else if (toSel > fromSel) dir = +1;
    else if (toSel < fromSel) dir = -1;
    st->selAnimDir = dir;

    // Distance in pixels: based on control height (feels like vertical scroll)
    RECT rc{};
    GetClientRect(st->hwnd, &rc);
    int h = rc.bottom - rc.top;
    float dist = (float)std::clamp((int)std::lroundf(h * 0.78f), 12, 40);
    st->selAnimDistPx = dist;

    PremiumComboInternal::EnsureAnimTimer(st);
    InvalidateRect(st->hwnd, nullptr, FALSE);
}

static void TickSelAnim(State* st)
{
    if (!st || !st->selAnimRunning || !st->hwnd) return;

    DWORD now = GetTickCount();
    DWORD dt = now - st->selAnimStartTick;
    DWORD dur = (st->selAnimDurationMs > 0) ? st->selAnimDurationMs : 1;

    if (dt >= dur)
    {
        st->selAnimRunning = false;
        InvalidateRect(st->hwnd, nullptr, FALSE);
        return;
    }

    // keep animating
    InvalidateRect(st->hwnd, nullptr, FALSE);
}

static void StopAnimTimerIfIdle_Local(State* st)
{
    if (!st || !st->hwnd) return;

    bool idle = true;

    if (st->popupAnim != PopupAnimMode::None) idle = false;
    if (st->arrowAnimRunning) idle = false;
    if (st->hwndPopup) idle = false;

    if (st->extraIconAnimRunning) idle = false;

    if (st->hoverAnimRunning) idle = false;
    if (st->arrowHotAnimRunning) idle = false;

    if (st->selAnimRunning) idle = false;

    if (idle)
        KillTimer(st->hwnd, PC_TIMER_ANIM);
}

// ============================================================================
// Popup window proc
// ============================================================================
LRESULT CALLBACK PremiumComboInternal::PopupProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    State* st = (State*)GetWindowLongPtrW(hWnd, GWLP_USERDATA);

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

    case WM_PAINT:
    {
        PAINTSTRUCT ps{};
        HDC hdc = BeginPaint(hWnd, &ps);
        PremiumComboInternal::PaintPopup(hWnd, st, hdc);
        EndPaint(hWnd, &ps);
        return 0;
    }

    case WM_NCDESTROY:
        SetWindowLongPtrW(hWnd, GWLP_USERDATA, 0);
        return 0;
    }

    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

// ============================================================================
// Combo window proc
// ============================================================================
LRESULT CALLBACK PremiumComboInternal::ComboProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    State* st = PremiumComboInternal::Get(hWnd);

    auto forwardKeyToParent = [&](UINT m, WPARAM wp, LPARAM lp) -> bool
        {
            if (!st || !st->parent) return false;
            PostMessageW(st->parent, m, wp, lp);
            return true;
        };

    switch (msg)
    {
    case WM_NCCREATE:
        return TRUE;

    case WM_CREATE:
    {
        auto* cs = (CREATESTRUCTW*)lParam;
        State* init = (State*)cs->lpCreateParams;

        st = new State();
        st->hwnd = hWnd;
        st->parent = cs->hwndParent;
        st->enabled = (IsWindowEnabled(hWnd) != FALSE);

        // initial arrow: down
        st->arrowT = 0.0f;

        // placeholder default empty
        st->placeholderText.clear();

        // extra icon initial
        st->extraIcon = PremiumCombo::ExtraIconKind::None;
        st->extraIconDraw = PremiumCombo::ExtraIconKind::None;
        st->extraIconAnimRunning = false;
        st->extraIconT = 0.0f;

        if (init)
        {
            st->controlId = init->controlId;
            st->dropMaxVisible = init->dropMaxVisible;
            st->itemHeightPx = init->itemHeightPx;
        }

        PremiumComboInternal::Set(hWnd, st);
        return 0;
    }

    case WM_TIMER:
        if (st && wParam == PC_TIMER_ANIM)
        {
            // tick popup/arrow if needed
            if (st->popupAnim != PopupAnimMode::None || st->arrowAnimRunning || st->hwndPopup)
                PremiumComboInternal::TickPopupAndArrowAnim(st);

            // tick extra icon animation
            TickExtraIconAnim(st);

            // tick selection scroll animation (closed state)
            TickSelAnim(st);

            StopAnimTimerIfIdle_Local(st);
            return 0;
        }
        break;

    case WM_ENABLE:
        if (st)
        {
            st->enabled = (wParam != 0);

            if (!st->enabled && PremiumComboInternal::IsInlineEditing(st))
                PremiumComboInternal::EndInlineEdit(st, false);

            if (!st->enabled && (st->dropped || st->hwndPopup))
                PremiumComboInternal::CloseDropDown(st, false);

            InvalidateRect(hWnd, nullptr, FALSE);
        }
        return 0;

    case WM_SETFONT:
        if (st)
        {
            st->font = (HFONT)wParam;
            if (lParam) InvalidateRect(hWnd, nullptr, TRUE);
            if (st->hwndPopup) InvalidateRect(st->hwndPopup, nullptr, TRUE);

            if (st->hwndEdit && IsWindow(st->hwndEdit))
                SendMessageW(st->hwndEdit, WM_SETFONT, (WPARAM)PremiumComboInternal::GetFont(st), TRUE);
        }
        return 0;

    case WM_GETFONT:
        return (LRESULT)PremiumComboInternal::GetFont(st);

    case WM_ERASEBKGND:
        return 1;

    case WM_PAINT:
    {
        PAINTSTRUCT ps{};
        HDC hdc = BeginPaint(hWnd, &ps);
        PremiumComboInternal::PaintCombo(hWnd, st, hdc);
        EndPaint(hWnd, &ps);
        return 0;
    }

    case WM_SETFOCUS:
    case WM_KILLFOCUS:
        if (st) PremiumComboInternal::ResetTypeSearch(st);
        InvalidateRect(hWnd, nullptr, FALSE);
        return 0;

    case WM_MOUSEMOVE:
        if (st)
        {
            PremiumComboInternal::UpdateHoverState(hWnd, st, lParam);
            if (st->dropped)
                PremiumComboInternal::DropdownMouseMove(st);
        }
        return 0;

    case WM_MOUSELEAVE:
        if (st)
        {
            st->hovered = false;
            st->arrowHot = false;
            st->extraIconHot = false;
            InvalidateRect(hWnd, nullptr, FALSE);
        }
        return 0;

    case WM_LBUTTONDOWN:
        if (!st) break;

        SetFocus(hWnd);
        PremiumComboInternal::ResetTypeSearch(st);

        {
            POINT pt{ (short)LOWORD(lParam), (short)HIWORD(lParam) };
            if (st->extraIconDraw != PremiumCombo::ExtraIconKind::None &&
                st->extraIconT > 0.001f &&
                PremiumComboInternal::HitTestExtraIcon(st, pt))
            {
                if (st->parent)
                {
                    WPARAM wp = MAKEWPARAM((UINT)st->extraIconDraw, (UINT)st->controlId);
                    PostMessageW(st->parent, PremiumCombo::MsgExtraIcon(), wp, (LPARAM)hWnd);
                }
                return 0;
            }
        }

        if (st->dropped)
        {
            PremiumComboInternal::DropdownClick(st);
            return 0;
        }

        PremiumComboInternal::OpenDropDown(st);
        return 0;

    case WM_MOUSEWHEEL:
        if (st)
        {
            int delta = GET_WHEEL_DELTA_WPARAM(wParam);
            if (delta == 0) return 0;

            if (!st->dropped)
            {
                forwardKeyToParent(WM_MOUSEWHEEL, wParam, lParam);
                return 0;
            }

            if (PremiumComboInternal::IsInlineEditing(st))
                return 0;

            int dir = (delta > 0) ? -1 : 1;
            PremiumComboInternal::ResetTypeSearch(st);
            PremiumComboInternal::MoveHot(st, dir);
            return 0;
        }
        return 0;

    case WM_CHAR:
        if (!st) break;

        if (!st->dropped)
        {
            forwardKeyToParent(WM_CHAR, wParam, lParam);
            return 0;
        }

        PremiumComboInternal::TypeSearchApply(st, (wchar_t)wParam, st->dropped);
        return 0;

    case WM_SYSKEYDOWN:
    case WM_KEYDOWN:
        if (!st) break;

        if (PremiumComboInternal::IsInlineEditing(st))
            return 0;

        if (!st->dropped)
        {
            forwardKeyToParent(msg, wParam, lParam);
            return 0;
        }

        if (wParam == VK_ESCAPE)
        {
            PremiumComboInternal::CloseDropDown(st, true);
            return 0;
        }

        if (wParam == VK_RETURN)
        {
            int n = (int)st->items.size();
            if (st->hotIndex >= 0 && st->hotIndex < n && st->hotIndex != st->curSel)
            {
                st->curSel = st->hotIndex;
                PremiumComboInternal::NotifySelChange(st);
            }
            PremiumComboInternal::CloseDropDown(st, true);
            return 0;
        }

        {
            int n = (int)st->items.size();
            if (n > 0)
            {
                switch (wParam)
                {
                case VK_UP:    PremiumComboInternal::ResetTypeSearch(st); PremiumComboInternal::MoveHot(st, -1); return 0;
                case VK_DOWN:  PremiumComboInternal::ResetTypeSearch(st); PremiumComboInternal::MoveHot(st, +1); return 0;
                case VK_PRIOR: PremiumComboInternal::ResetTypeSearch(st); PremiumComboInternal::PageHot(st, -1); return 0;
                case VK_NEXT:  PremiumComboInternal::ResetTypeSearch(st); PremiumComboInternal::PageHot(st, +1); return 0;
                case VK_HOME:  PremiumComboInternal::ResetTypeSearch(st); PremiumComboInternal::HotToEdge(st, false); return 0;
                case VK_END:   PremiumComboInternal::ResetTypeSearch(st); PremiumComboInternal::HotToEdge(st, true);  return 0;
                }
            }
        }

        return 0;

    case WM_CAPTURECHANGED:
        if (st && st->dropped)
        {
            if (PremiumComboInternal::IsInlineEditing(st))
                return 0;

            PremiumComboInternal::CloseDropDown(st, false);
            return 0;
        }
        return 0;

    case WM_CANCELMODE:
        if (st && st->dropped)
        {
            if (PremiumComboInternal::IsInlineEditing(st))
                return 0;

            PremiumComboInternal::CloseDropDown(st, false);
            return 0;
        }
        return 0;

    case WM_NCDESTROY:
        if (st)
        {
            if (PremiumComboInternal::IsInlineEditing(st))
                PremiumComboInternal::EndInlineEdit(st, false);

            if (st->hwndPopup)
            {
                DestroyWindow(st->hwndPopup);
                st->hwndPopup = nullptr;
            }

            KillTimer(st->hwnd, PC_TIMER_ANIM);

            delete st;
            PremiumComboInternal::Set(hWnd, nullptr);
        }
        return 0;
    }

    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

// ============================================================================
// Public API implementation
// ============================================================================
namespace PremiumCombo
{
    HWND Create(HWND parent, HINSTANCE hInst, int x, int y, int w, int h, int controlId, DWORD style)
    {
        PremiumComboInternal::RegisterOnce(hInst);

        if (style == 0)
            style = WS_CHILD | WS_VISIBLE | WS_TABSTOP;

        PremiumComboInternal::State init{};
        init.controlId = controlId;

        return CreateWindowW(
            PremiumComboInternal::PC_COMBO_CLASS,
            L"",
            style,
            x, y, w, h,
            parent, (HMENU)(INT_PTR)controlId, hInst, &init);
    }

    void SetFont(HWND hCombo, HFONT hFont, bool redraw)
    {
        if (!hCombo) return;
        SendMessageW(hCombo, WM_SETFONT, (WPARAM)hFont, (LPARAM)(redraw ? TRUE : FALSE));
    }

    void Clear(HWND hCombo)
    {
        auto* st = PremiumComboInternal::Get(hCombo);
        if (!st) return;

        if (PremiumComboInternal::IsInlineEditing(st))
            PremiumComboInternal::EndInlineEdit(st, false);

        st->items.clear();
        st->itemBtnMask.clear();
        st->curSel = -1;
        st->hotIndex = -1;
        st->scrollTop = 0;

        st->committedText.clear();
        st->committedIndex = -1;
        st->committedKind = ItemButtonKind::None;

        PremiumComboInternal::ResetTypeSearch(st);

        if (st->dropped)
            PremiumComboInternal::CloseDropDown(st, false);

        st->selAnimInitialized = false;
        st->selAnimRunning = false;
        st->selAnimStartTick = 0;
        st->selAnimDurationMs = 160;
        st->selAnimFromSel = -1;
        st->selAnimToSel = -1;
        st->selAnimDir = 1;
        st->selAnimDistPx = 0.0f;

        InvalidateRect(hCombo, nullptr, FALSE);
    }

    int AddString(HWND hCombo, const wchar_t* text)
    {
        auto* st = PremiumComboInternal::Get(hCombo);
        if (!st) return -1;

        st->items.emplace_back(text ? text : L"");
        st->itemBtnMask.push_back(PremiumComboInternal::BTN_NONE);

        int idx = (int)st->items.size() - 1;

        // Keep legacy behavior: first added item auto-selects if there was none.
        if (st->curSel < 0)
            st->curSel = 0;

        InvalidateRect(hCombo, nullptr, FALSE);
        if (st->hwndPopup) InvalidateRect(st->hwndPopup, nullptr, FALSE);
        return idx;
    }

    int GetCount(HWND hCombo)
    {
        auto* st = PremiumComboInternal::Get(hCombo);
        if (!st) return 0;
        return (int)st->items.size();
    }

    int GetCurSel(HWND hCombo)
    {
        auto* st = PremiumComboInternal::Get(hCombo);
        if (!st) return -1;
        return st->curSel;
    }

    void SetCurSel(HWND hCombo, int idx, bool notify)
    {
        auto* st = PremiumComboInternal::Get(hCombo);
        if (!st) return;

        if (PremiumComboInternal::IsInlineEditing(st))
            PremiumComboInternal::EndInlineEdit(st, true);

        int oldSel = st->curSel;

        if (st->items.empty())
        {
            st->curSel = -1;
        }
        else
        {
            // allow idx == -1 as "no selection"
            if (idx < 0)
            {
                st->curSel = -1;
            }
            else
            {
                idx = std::clamp(idx, 0, (int)st->items.size() - 1);
                st->curSel = idx;
            }
        }

        // Start closed-state scroll animation (only makes sense when combo is closed)
        if (!st->dropped)
            StartSelAnim(st, oldSel, st->curSel);

        if (notify)
            PremiumComboInternal::NotifySelChange(st);

        InvalidateRect(hCombo, nullptr, FALSE);
        if (st->hwndPopup) InvalidateRect(st->hwndPopup, nullptr, FALSE);
    }

    void SetPlaceholderText(HWND hCombo, const wchar_t* text)
    {
        auto* st = PremiumComboInternal::Get(hCombo);
        if (!st) return;

        st->placeholderText = (text ? text : L"");

        // only affects closed rendering
        InvalidateRect(hCombo, nullptr, FALSE);
    }

    int GetLBText(HWND hCombo, int idx, wchar_t* out, int outCch)
    {
        if (!out || outCch <= 0) return 0;
        out[0] = 0;

        auto* st = PremiumComboInternal::Get(hCombo);
        if (!st) return 0;

        if (idx < 0 || idx >= (int)st->items.size()) return 0;

        const std::wstring& s = st->items[idx];
        wcsncpy_s(out, outCch, s.c_str(), _TRUNCATE);
        return (int)wcslen(out);
    }

    int GetLBTextLen(HWND hCombo, int idx)
    {
        auto* st = PremiumComboInternal::Get(hCombo);
        if (!st) return -1;

        if (idx < 0 || idx >= (int)st->items.size()) return -1;
        return (int)st->items[idx].size();
    }

    void SetDropMaxVisible(HWND hCombo, int count)
    {
        auto* st = PremiumComboInternal::Get(hCombo);
        if (!st) return;
        st->dropMaxVisible = std::clamp(count, 1, 40);
    }

    void SetItemHeightPx(HWND hCombo, int itemHeightPx)
    {
        auto* st = PremiumComboInternal::Get(hCombo);
        if (!st) return;
        st->itemHeightPx = std::max(0, itemHeightPx);
    }

    void ShowDropDown(HWND hCombo, bool show)
    {
        auto* st = PremiumComboInternal::Get(hCombo);
        if (!st) return;

        if (show)
        {
            if (!st->dropped) PremiumComboInternal::OpenDropDown(st);
        }
        else
        {
            if (st->dropped) PremiumComboInternal::CloseDropDown(st, true);
        }
    }

    bool GetDroppedState(HWND hCombo)
    {
        auto* st = PremiumComboInternal::Get(hCombo);
        if (!st) return false;
        return st->dropped;
    }

    void SetEnabled(HWND hCombo, bool enabled)
    {
        if (!hCombo) return;
        EnableWindow(hCombo, enabled ? TRUE : FALSE);
    }

    void Invalidate(HWND hCombo)
    {
        if (!hCombo) return;
        InvalidateRect(hCombo, nullptr, FALSE);
    }

    void SetExtraIcon(HWND hCombo, ExtraIconKind kind)
    {
        auto* st = PremiumComboInternal::Get(hCombo);
        if (!st) return;

        if (st->extraIcon == kind && !st->extraIconAnimRunning)
            return;

        PremiumCombo::ExtraIconKind old = st->extraIcon;
        st->extraIcon = kind;

        if (kind == ExtraIconKind::None)
        {
            // hide (keep drawing old icon during fade-out)
            st->extraIconDraw = old;
            StartExtraIconAnim(st, 0.0f, PC_EXTRAICON_HIDE_MS);
        }
        else
        {
            // show (switch draw icon immediately)
            st->extraIconDraw = kind;
            StartExtraIconAnim(st, 1.0f, PC_EXTRAICON_SHOW_MS);
        }

        InvalidateRect(hCombo, nullptr, FALSE);
    }

    void SetItemButtonKind(HWND hCombo, int idx, ItemButtonKind kind)
    {
        auto* st = PremiumComboInternal::Get(hCombo);
        if (!st) return;

        if (kind == ItemButtonKind::None) return;
        if (idx < 0 || idx >= (int)st->items.size()) return;

        if ((int)st->itemBtnMask.size() != (int)st->items.size())
            st->itemBtnMask.resize(st->items.size(), PremiumComboInternal::BTN_NONE);

        st->itemBtnMask[idx] |= PremiumComboInternal::KindToMask(kind);

        if (st->hwndPopup) InvalidateRect(st->hwndPopup, nullptr, FALSE);
    }

    void ResetVisualState(HWND hCombo)
    {
        auto* st = PremiumComboInternal::Get(hCombo);
        if (!st) return;

        if (PremiumComboInternal::IsInlineEditing(st))
            PremiumComboInternal::EndInlineEdit(st, false);

        st->hotIndex = -1;
        st->scrollTop = 0;
        st->hovered = false;
        st->arrowHot = false;
        st->extraIconHot = false;
        st->hotBtnIndex = -1;
        st->hotBtnKind = ItemButtonKind::None;

        st->committedText.clear();
        st->committedIndex = -1;
        st->committedKind = ItemButtonKind::None;

        PremiumComboInternal::ResetTypeSearch(st);

        st->arrowAnimRunning = false;
        st->arrowT = 0.0f;

        // reset extra icon animation state
        st->extraIconAnimRunning = false;
        st->extraIconT = (st->extraIcon != ExtraIconKind::None) ? 1.0f : 0.0f;
        st->extraIconDraw = st->extraIcon;

        InvalidateRect(hCombo, nullptr, FALSE);
        if (st->hwndPopup) InvalidateRect(st->hwndPopup, nullptr, FALSE);
    }

    bool IsEditingItem(HWND hCombo)
    {
        auto* st = PremiumComboInternal::Get(hCombo);
        return PremiumComboInternal::IsInlineEditing(st);
    }

    bool BeginInlineEdit(HWND hCombo, int idx, bool openDropDownIfNeeded)
    {
        auto* st = PremiumComboInternal::Get(hCombo);
        if (!st) return false;
        if (!st->enabled) return false;

        if (idx < 0 || idx >= (int)st->items.size())
            return false;

        if (!st->dropped || !st->hwndPopup)
        {
            if (!openDropDownIfNeeded) return false;
            PremiumComboInternal::OpenDropDown(st);
        }

        if (!st->dropped || !st->hwndPopup)
            return false;

        PremiumComboInternal::BeginInlineEdit(st, idx);
        return PremiumComboInternal::IsInlineEditing(st);
    }

    bool BeginInlineEditSelected(HWND hCombo, bool openDropDownIfNeeded)
    {
        auto* st = PremiumComboInternal::Get(hCombo);
        if (!st) return false;

        int idx = st->curSel;
        if (idx < 0) idx = 0;
        if (idx < 0 || idx >= (int)st->items.size())
            return false;

        return BeginInlineEdit(hCombo, idx, openDropDownIfNeeded);
    }

    int ConsumeCommittedText(HWND hCombo, wchar_t* out, int outCch)
    {
        if (!out || outCch <= 0) return 0;
        out[0] = 0;

        auto* st = PremiumComboInternal::Get(hCombo);
        if (!st) return 0;

        if (st->committedText.empty())
            return 0;

        wcsncpy_s(out, outCch, st->committedText.c_str(), _TRUNCATE);
        int n = (int)wcslen(out);

        st->committedText.clear();
        st->committedIndex = -1;
        st->committedKind = ItemButtonKind::None;

        return n;
    }
}
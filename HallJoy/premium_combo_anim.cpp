// premium_combo_anim.cpp
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <algorithm>
#include <cmath>

#include "premium_combo_internal.h"

using namespace PremiumComboInternal;

// ----------------------------------------------------------------------------
// Internal helpers (not in header)
// ----------------------------------------------------------------------------
static void PopupApplyAlpha(HWND hPopup, BYTE a)
{
    if (!hPopup) return;
    SetLayeredWindowAttributes(hPopup, 0, a, LWA_ALPHA);
}

static void PopupApplyAnimatedPos(State* st, float t01)
{
    if (!st || !st->hwndPopup) return;

    t01 = Clamp01(t01);

    RECT a = st->popupStartRect;
    RECT b = st->popupTargetRect;

    int w = b.right - b.left;
    int h = b.bottom - b.top;

    // "Premium" motion: slide only in Y, X stays fixed (no jitter).
    float y = (float)a.top + ((float)b.top - (float)a.top) * t01;

    int xi = b.left;
    int yi = (int)std::lroundf(y);

    SetWindowPos(st->hwndPopup, HWND_TOPMOST, xi, yi, w, h,
        SWP_NOACTIVATE | SWP_NOOWNERZORDER | SWP_NOSENDCHANGING | SWP_SHOWWINDOW);
}

static void StartArrowAnim(State* st, float to, DWORD durMs)
{
    if (!st) return;

    st->arrowAnimRunning = true;
    st->arrowAnimStartTick = GetTickCount();
    st->arrowAnimDurationMs = std::max<DWORD>(1, durMs);

    st->arrowFrom = std::clamp(st->arrowT, 0.0f, 1.0f);
    st->arrowTo = std::clamp(to, 0.0f, 1.0f);

    EnsureAnimTimer(st);

    if (st->hwnd)
        InvalidateRect(st->hwnd, nullptr, FALSE);
}

// ----------------------------------------------------------------------------
// Timer control
// ----------------------------------------------------------------------------
void PremiumComboInternal::EnsureAnimTimer(State* st)
{
    if (!st || !st->hwnd) return;
    SetTimer(st->hwnd, PC_TIMER_ANIM, PC_ANIM_TICK_MS, nullptr);
}

void PremiumComboInternal::StopAnimTimerIfIdle(State* st)
{
    if (!st || !st->hwnd) return;
    if (st->popupAnim == PopupAnimMode::None && !st->arrowAnimRunning)
        KillTimer(st->hwnd, PC_TIMER_ANIM);
}

// ----------------------------------------------------------------------------
// Popup animation start
// ----------------------------------------------------------------------------
void PremiumComboInternal::PopupStartAnim(State* st, PopupAnimMode mode)
{
    if (!st || !st->hwndPopup) return;

    st->popupAnim = mode;
    st->popupAnimStartTick = GetTickCount();

    if (mode == PopupAnimMode::Opening)
    {
        st->popupAnimDurationMs = PC_POPUP_OPEN_MS;
        st->popupAlphaFrom = PC_POPUP_ALPHA_MIN;
        st->popupAlphaTo = PC_POPUP_ALPHA_MAX;

        PopupApplyAlpha(st->hwndPopup, st->popupAlphaFrom);
        PopupApplyAnimatedPos(st, 0.0f);

        StartArrowAnim(st, 1.0f, PC_POPUP_OPEN_MS);
    }
    else if (mode == PopupAnimMode::Closing)
    {
        st->popupAnimDurationMs = PC_POPUP_CLOSE_MS;
        st->popupAlphaFrom = PC_POPUP_ALPHA_MAX;
        st->popupAlphaTo = PC_POPUP_ALPHA_MIN;

        PopupApplyAlpha(st->hwndPopup, st->popupAlphaFrom);
        PopupApplyAnimatedPos(st, 1.0f);

        StartArrowAnim(st, 0.0f, PC_POPUP_CLOSE_MS);
    }
    else
    {
        st->popupAnim = PopupAnimMode::None;
        return;
    }

    EnsureAnimTimer(st);
}

// ----------------------------------------------------------------------------
// Popup + arrow animation tick
// ----------------------------------------------------------------------------
void PremiumComboInternal::TickPopupAndArrowAnim(State* st)
{
    if (!st) return;

    // ---- Arrow animation tick ----
    if (st->arrowAnimRunning)
    {
        DWORD now = GetTickCount();
        DWORD dt = now - st->arrowAnimStartTick;
        DWORD dur = (st->arrowAnimDurationMs > 0) ? st->arrowAnimDurationMs : 1;

        float t = Clamp01((float)dt / (float)dur);
        float e = EaseOutCubic(t);

        st->arrowT = st->arrowFrom + (st->arrowTo - st->arrowFrom) * e;

        if (t >= 1.0f - 1e-4f)
        {
            st->arrowT = st->arrowTo;
            st->arrowAnimRunning = false;
        }

        if (st->hwnd)
            InvalidateRect(st->hwnd, nullptr, FALSE);
    }

    // ---- Popup animation tick ----
    if (st->popupAnim != PopupAnimMode::None && st->hwndPopup)
    {
        DWORD now = GetTickCount();
        DWORD dt = now - st->popupAnimStartTick;
        DWORD dur = (st->popupAnimDurationMs > 0) ? st->popupAnimDurationMs : 1;

        float t = Clamp01((float)dt / (float)dur);
        float e = EaseOutCubic(t);

        int a = (int)std::lroundf(
            (float)st->popupAlphaFrom + ((float)st->popupAlphaTo - (float)st->popupAlphaFrom) * e);
        a = std::clamp(a, (int)PC_POPUP_ALPHA_MIN, (int)PC_POPUP_ALPHA_MAX);
        PopupApplyAlpha(st->hwndPopup, (BYTE)a);

        // For Closing we slide back to "start" (slightly offset), for Opening we slide to target.
        float posT = (st->popupAnim == PopupAnimMode::Closing) ? (1.0f - e) : e;
        PopupApplyAnimatedPos(st, posT);

        if (t >= 1.0f - 1e-4f)
        {
            if (st->popupAnim == PopupAnimMode::Closing)
            {
                HWND pop = st->hwndPopup;
                st->hwndPopup = nullptr;
                st->popupAnim = PopupAnimMode::None;
                DestroyWindow(pop);
            }
            else
            {
                st->popupAnim = PopupAnimMode::None;
                PopupApplyAlpha(st->hwndPopup, PC_POPUP_ALPHA_MAX);
                PopupApplyAnimatedPos(st, 1.0f);
            }
        }
    }

    StopAnimTimerIfIdle(st);
}

// ----------------------------------------------------------------------------
// Open / Close dropdown
// ----------------------------------------------------------------------------
void PremiumComboInternal::OpenDropDown(State* st)
{
    if (!st) return;
    if (st->dropped) return;
    if (!st->enabled) return;

    // If a previous popup is still closing, kill it immediately.
    if (st->hwndPopup && st->popupAnim == PopupAnimMode::Closing)
    {
        DestroyWindow(st->hwndPopup);
        st->hwndPopup = nullptr;
        st->popupAnim = PopupAnimMode::None;
    }

    st->dropped = true;
    st->hotIndex = st->curSel;

    ResetTypeSearch(st);

    // initial scroll: center current selection if possible
    int rows = GetVisibleRows(st);
    int n = (int)st->items.size();
    if (n > 0 && st->curSel >= 0)
    {
        st->scrollTop = st->curSel - rows / 2;
        ClampScroll(st);
        EnsureIndexVisible(st, st->curSel);
    }
    else
    {
        st->scrollTop = 0;
    }

    if (!st->hwnd) return;

    HINSTANCE hInst = (HINSTANCE)GetWindowLongPtrW(st->hwnd, GWLP_HINSTANCE);

    RECT wr{};
    GetWindowRect(st->hwnd, &wr);

    int comboW = wr.right - wr.left;
    int itemH = GetItemHeightPx(st->hwnd, st);

    int popH = rows * itemH + 2; // border
    int popW = std::max(comboW, S(st->hwnd, 140));

    RECT pr{};
    pr.left = wr.left;
    pr.right = pr.left + popW;
    pr.top = wr.bottom;
    pr.bottom = pr.top + popH;

    // open above if needed
    st->popupOpensUp = false;
    HMONITOR mon = MonitorFromRect(&wr, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi{};
    mi.cbSize = sizeof(mi);
    if (GetMonitorInfoW(mon, &mi))
    {
        RECT wa = mi.rcWork;
        if (pr.bottom > wa.bottom && (wr.top - popH) >= wa.top)
        {
            st->popupOpensUp = true;
            pr.bottom = wr.top;
            pr.top = pr.bottom - popH;
        }
    }

    ClampPopupToMonitor(pr);

    st->popupTargetRect = pr;

    // start rect: slide by a few pixels
    st->popupStartRect = pr;
    int slide = std::clamp(S(st->hwnd, 8), 4, 14);
    if (st->popupOpensUp) st->popupStartRect.top += slide;
    else                  st->popupStartRect.top -= slide;
    st->popupStartRect.bottom = st->popupStartRect.top + (pr.bottom - pr.top);

    HWND ownerTop = GetAncestor(st->hwnd, GA_ROOT);

    // Create layered popup for alpha animation
    st->hwndPopup = CreateWindowExW(
        WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_NOACTIVATE | WS_EX_LAYERED,
        PC_POPUP_CLASS,
        L"",
        WS_POPUP,
        pr.left, pr.top, pr.right - pr.left, pr.bottom - pr.top,
        ownerTop, nullptr, hInst, st);

    if (!st->hwndPopup)
    {
        st->dropped = false;
        st->hotIndex = -1;
        st->scrollTop = 0;
        return;
    }

    ShowWindow(st->hwndPopup, SW_SHOWNOACTIVATE);
    UpdateWindow(st->hwndPopup);

    // Capture on combo => robust close on outside click
    SetCapture(st->hwnd);
    st->captureActive = true;

    // Start opening animation (also rotates arrow)
    PopupStartAnim(st, PopupAnimMode::Opening);

    InvalidateRect(st->hwnd, nullptr, FALSE);
}

void PremiumComboInternal::CloseDropDown(State* st, bool keepFocusOnCombo)
{
    if (!st) return;

    // close logically immediately
    st->dropped = false;
    st->hotIndex = -1;
    st->scrollTop = 0;

    ResetTypeSearch(st);

    if (st->captureActive)
    {
        st->captureActive = false;
        if (st->hwnd && GetCapture() == st->hwnd)
            ReleaseCapture();
    }

    if (st->hwndPopup)
    {
        // Animate closing, destroy in TickPopupAndArrowAnim
        st->popupAnim = PopupAnimMode::None;
        PopupStartAnim(st, PopupAnimMode::Closing);
    }
    else
    {
        // No popup window => ensure arrow ends down
        StartArrowAnim(st, 0.0f, PC_POPUP_CLOSE_MS);
    }

    if (st->hwnd)
        InvalidateRect(st->hwnd, nullptr, FALSE);

    if (keepFocusOnCombo && st->hwnd)
        SetFocus(st->hwnd);
}
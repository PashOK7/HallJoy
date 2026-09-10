// keyboard_subpages.cpp
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include "support_log.h"
#include "community_links.h"
#include "overlay_text_edit.h"
#include "layout_editor_model.h"
#include "key_shape_win.h"
#include "main_keyboard_input.h"
#include "bounded_ini.h"
#if defined(HALLJOY_ANALOG_SIMULATOR)
#include "premium_combo_internal.h"
#include "keyboard_render.h"
#endif
#include <commctrl.h>
#include <string>
#include <stdexcept>
#include <cstdint>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <cstdio>
#include <cstring>
#include <vector>
#include <cwctype>

#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

#include <objidl.h>
#include <gdiplus.h>
#include <shellapi.h>

#include "Resource.h"
#include "keyboard_ui_internal.h"
#include "keyboard_ui.h"
#include "keyboard_ui_state.h"
#include "input_privilege_warning.h"
#include "app.h"
#include "block_keys_policy.h"
#include "analog_key_codes.h"
#include "keyboard_keysettings_panel.h"
#include "keyboard_keysettings_panel_internal.h"

#include "backend.h"
#include "gamepad_render.h"
#include "ui_theme.h"
#include "settings.h"
#include "realtime_loop.h"
#include "engine_runtime_owner.h"
#include "win_util.h"
#include "keyboard_profiles.h"
#include "premium_combo.h"
#include "keyboard_layout.h"
#include "layout_picker.h"
#include "keyboard_support_status.h"
#include "settings_ini.h"
#include "factory_reset.h"
#include "profile_ini.h"
#include "app_paths.h"
#include "file_name_policy.h"
#include "global_profiles.h"
#include "mouse_ipc.h"
#include "overlay_server.h"
#include "debug_log.h"
#include "custom_page_surface.h"
#include "custom_page_controls.h"
#include "ui_paint_audit.h"
#if defined(HALLJOY_ANALOG_SIMULATOR)
#include "ini_util.h"
#endif

using namespace Gdiplus;
namespace fs = std::filesystem;

static constexpr UINT WM_APP_REQUEST_SAVE = WM_APP + 1;
static constexpr UINT WM_APP_APPLY_TIMING = WM_APP + 2;
static constexpr UINT WM_APP_FACTORY_RESET_RESTART = WM_APP + 3;
static constexpr UINT WM_APP_ENGINE_RUNTIME_TOGGLE = WM_APP + 262;
static constexpr UINT WM_APP_ENGINE_RUNTIME_STATE_CHANGED = WM_APP + 362;
static constexpr UINT WM_APP_PROFILE_BEGIN_CREATE = WM_APP + 120;
static constexpr UINT WM_APP_CONFIG_PROFILE_APPLIED = WM_APP + 121;
static constexpr UINT WM_APP_GLOBAL_PROFILE_DIRTY = WM_APP + 122;
static constexpr UINT WM_APP_CONFIG_MARK_SURFACE_DIRTY = WM_APP + 123;

static constexpr UINT_PTR TOAST_TIMER_ID = 8811;
static constexpr UINT_PTR ANALOG_SELF_TEST_TIMER_ID = 8812;
static constexpr DWORD    TOAST_SHOW_MS = 1600;
static constexpr const wchar_t* CONFIG_SCROLLY_PROP = L"DD_ConfigScrollY";
static constexpr bool kEnableSnappyDebug = false; // set true for temporary snappy toggle diagnostics

static constexpr int ID_SNAPPY = 7003;
static constexpr int ID_BLOCK_BOUND_KEYS = 7004;
static constexpr int ID_BLOCK_KEYS_ALLOW_ALT_TAB = 7014;
static constexpr int ID_BLOCK_KEYS_SHORTCUT = 7015;
static constexpr int ID_BLOCK_KEYS_CLEAR_SHORTCUT = 7016;
static constexpr int ID_LAST_KEY_PRIORITY = 7005;
static constexpr int ID_LAST_KEY_PRIORITY_SENS_SLIDER = 7006;
static constexpr int ID_LAST_KEY_PRIORITY_SENS_CHIP = 7007;
static constexpr int ID_ANALOG_SELF_TEST = 7008;
static constexpr int ID_SPARK_POLL_MODE = 7012;
static constexpr int ID_SPARK_ROW_LIMIT = 7013;
static constexpr bool kShowAnalogSelfTestControls = false;

static int S(HWND hwnd, int px) { return WinUtil_ScalePx(hwnd, px); }
static Color Gp(COLORREF c, BYTE a = 255);

static void SnappyDebugLog(const wchar_t* stage, HWND hBtn, int extraA = -1, int extraB = -1)
{
#if defined(HALLJOY_DEBUG_BUILD)
    if (!kEnableSnappyDebug) return;

    int check = -1;
    if (hBtn && IsWindow(hBtn))
        check = (int)SendMessageW(hBtn, BM_GETCHECK, 0, 0);

    int setting = Settings_GetSnappyJoystick() ? 1 : 0;

    wchar_t buf[320]{};
    swprintf_s(buf, L"[SnappyDbg] %s hwnd=%p check=%d setting=%d a=%d b=%d\n",
        stage ? stage : L"(null)", (void*)hBtn, check, setting, extraA, extraB);
    OutputDebugStringW(buf);
#else
    (void)stage; (void)hBtn; (void)extraA; (void)extraB;
#endif
}

// ---------------- Double-buffer helpers ----------------
static void BeginDoubleBufferPaint(HWND hWnd, PAINTSTRUCT& ps, HDC& outMemDC, HBITMAP& outBmp, HGDIOBJ& outOldBmp, bool dirtyOnly = false)
{
    HDC hdc = BeginPaint(hWnd, &ps);
    RECT rc{};
    GetClientRect(hWnd, &rc);
    if (dirtyOnly) rc = ps.rcPaint;
    outMemDC = CreateCompatibleDC(hdc);
    outBmp = CreateCompatibleBitmap(hdc, (std::max)(1L, rc.right - rc.left), (std::max)(1L, rc.bottom - rc.top));
    outOldBmp = SelectObject(outMemDC, outBmp);
    if (dirtyOnly) SetViewportOrgEx(outMemDC, -rc.left, -rc.top, nullptr);
    FillRect(outMemDC, &rc, UiTheme::Brush_PanelBg());
}

static void EndDoubleBufferPaint(HWND hWnd, PAINTSTRUCT& ps, HDC memDC, HBITMAP bmp, HGDIOBJ oldBmp)
{
    HDC hdc = ps.hdc;
    // Commit only the invalidated area. Several pages contain live regions;
    // copying the complete client bitmap for a small dirty region needlessly
    // replaces static pixels and makes input-driven updates visibly flash.
    const RECT& dirty = ps.rcPaint;
    if (dirty.right > dirty.left && dirty.bottom > dirty.top)
    {
        BitBlt(hdc, dirty.left, dirty.top,
            dirty.right - dirty.left, dirty.bottom - dirty.top,
            memDC, dirty.left, dirty.top, SRCCOPY);
    }
    SelectObject(memDC, oldBmp);
    DeleteObject(bmp);
    DeleteDC(memDC);
    EndPaint(hWnd, &ps);
}

static std::vector<std::wstring> BuildAnalogDiagnosticsLines(const BackendAnalogTelemetry& t);

struct TesterPageState
{
    CustomPageSurface surface;
    CustomPageScrollController scroll;
};

// ============================================================================
// Gamepad Tester page (DPI-scaled)
// ============================================================================
LRESULT CALLBACK KeyboardSubpages_TesterPageProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    static UiPaintAuditCounter paintAudit(L"tester");
    auto* state = (TesterPageState*)GetWindowLongPtrW(hWnd, GWLP_USERDATA);
    switch (msg)
    {
    case WM_CREATE:
        state = new TesterPageState();
        SetWindowLongPtrW(hWnd, GWLP_USERDATA, (LONG_PTR)state);
        return 0;

    case WM_ERASEBKGND:
        paintAudit.Event(WM_ERASEBKGND);
        return 1;

    case WM_APP_TESTER_LIVE_REFRESH:
        // Tester is the canonical live diagnostics surface. It repaints only
        // after a changed gamepad/telemetry hash is posted by the visible-tab gate.
        UiAuditTraceInvalidation(L"tester", wParam == TESTER_REFRESH_DIAGNOSTICS
            ? L"telemetry_hash_changed" : L"gamepad_hash_changed");
        InvalidateRect(hWnd, nullptr, FALSE);
        return 0;

    case WM_PAINT:
    {
        PAINTSTRUCT ps{};
        HDC memDC = nullptr;
        HBITMAP bmp = nullptr;
        HGDIOBJ oldBmp = nullptr;
        BeginDoubleBufferPaint(hWnd, ps, memDC, bmp, oldBmp);
        paintAudit.Event(WM_PAINT, &ps.rcPaint);

        RECT rcClient{};
        GetClientRect(hWnd, &rcClient);

        int padCount = std::clamp(Backend_GetVirtualGamepadCount(), 1, 4);
        int cols = (padCount >= 3) ? 2 : padCount;
        cols = std::max(1, cols);
        int rows = (padCount + cols - 1) / cols;

        const int margin = S(hWnd, 12);
        const int cardGap = S(hWnd, 12);
        int clientW = (int)(rcClient.right - rcClient.left);
        int clientH = (int)(rcClient.bottom - rcClient.top);
        BackendAnalogTelemetry analog{};
        Backend_GetAnalogTelemetry(&analog);
        const auto analogLines = BuildAnalogDiagnosticsLines(analog);
        const bool showAnalogInfo = !analogLines.empty();
        int analogInfoH = showAnalogInfo ? S(hWnd, 16 + (int)analogLines.size() * 18) : 0;
        int availW = std::max(1, clientW - margin * 2 - cardGap * (cols - 1));
        const int minCardH = S(hWnd, 156);
        int availH = std::max(minCardH * rows,
            clientH - margin * 2 - analogInfoH - (showAnalogInfo ? cardGap : 0) - cardGap * (rows - 1));
        int cardW = std::max(1, availW / cols);
        int cardH = std::max(1, availH / rows);
        const int contentHeight = margin * 2 + analogInfoH +
            (showAnalogInfo ? cardGap : 0) + rows * cardH + cardGap * (rows - 1);
        if (state)
        {
            CustomPageSurface_SetState(&state->surface, state->surface.scrollY, contentHeight);
            state->surface.scrollY = std::clamp(state->surface.scrollY, 0,
                CustomPageSurface_GetMaxScroll(hWnd, &state->surface));
        }
        const int scrollY = state ? state->surface.scrollY : 0;

        HPEN cardPen = CreatePen(PS_SOLID, 1, UiTheme::Color_Border());
        HGDIOBJ oldPenGlobal = SelectObject(memDC, cardPen);
        HGDIOBJ oldBrushGlobal = SelectObject(memDC, GetStockObject(HOLLOW_BRUSH));

        HFONT font = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
        HGDIOBJ oldFont = SelectObject(memDC, font);
        SetBkMode(memDC, TRANSPARENT);
        SetTextColor(memDC, UiTheme::Color_Text());

        auto textLine = [&](int x, int& y, const std::wstring& t, int lineH)
            {
                TextOutW(memDC, x, y, t.c_str(), (int)t.size());
                y += lineH;
            };

        if (showAnalogInfo)
        {
            RECT info{ margin, margin - scrollY, clientW - margin, margin - scrollY + analogInfoH };
            FillRect(memDC, &info, UiTheme::Brush_ControlBg());
            Rectangle(memDC, info.left, info.top, info.right, info.bottom);

            int x = info.left + S(hWnd, 10);
            int y = info.top + S(hWnd, 8);
            int lineH = S(hWnd, 18);

            for (const auto& line : analogLines)
                textLine(x, y, line, lineH);

#if 0 // Replaced by the single route-complete diagnostics builder above.
            wchar_t buf[256]{};
            if (analog.mad68Connected)
            {
                const wchar_t* mode = analog.mad68Full ? L"full native" : L"emergency W/A/S/D";
                swprintf_s(buf, L"Analog input: MADLIONS native A0 %04X:%04X | mode %s | firmware %04X | coverage %u/68 | published %u",
                    0x373B, (unsigned)analog.mad68ProductId, mode, (unsigned)analog.mad68FirmwareVersion,
                    (unsigned)analog.mad68Coverage, (unsigned)analog.mad68PublishedKeys);
                textLine(x, y, buf, lineH);
                swprintf_s(buf, L"Analog resolution: device raw 0..1600 -> HallJoy %u levels | UAP/Wooting and other native sources remain enabled",
                    (unsigned)analog.analogOutputLevels);
                textLine(x, y, buf, lineH);
            }
            else if (analog.hex80Connected)
            {
                swprintf_s(buf, L"Analog input: ATK x QK Hex80 %04X:%04X | keys %u | chunks %.1f Hz | matrix %.1f Hz | tx %u/%u us",
                    (unsigned)analog.hex80VendorId,
                    (unsigned)analog.hex80ProductId,
                    (unsigned)analog.hex80MappedKeys,
                    (double)analog.hex80ChunkHz10 / 10.0,
                    (double)analog.hex80MatrixHz10 / 10.0,
                    (unsigned)analog.hex80AvgTransactionUs,
                    (unsigned)analog.hex80MaxTransactionUs);
                textLine(x, y, buf, lineH);
                swprintf_s(buf, L"Analog resolution: travel 0..%u -> HallJoy %u levels | 104 slots / 26 requests | packet age %u ms",
                    (unsigned)analog.hex80TravelMax,
                    (unsigned)analog.analogOutputLevels,
                    (unsigned)analog.hex80LastPacketAgeMs);
                textLine(x, y, buf, lineH);
            }
            else if (analog.addressedConnected)
            {
                swprintf_s(buf, L"Analog input: Addressed 09/94/02 %04X:%04X | keys %u | active %u | responses %llu/%llu",
                    (unsigned)analog.addressedVendorId,
                    (unsigned)analog.addressedProductId,
                    (unsigned)analog.addressedMappedKeys,
                    (unsigned)analog.addressedActiveKeys,
                    (unsigned long long)analog.addressedPollSuccess,
                    (unsigned long long)analog.addressedPollAttempts);
                textLine(x, y, buf, lineH);
                swprintf_s(buf, L"Analog transport: FF60:0061 | HID reports %u/%u bytes | response age %u ms | HallJoy %u levels",
                    (unsigned)analog.addressedInputReportBytes,
                    (unsigned)analog.addressedOutputReportBytes,
                    (unsigned)analog.addressedLastResponseAgeMs,
                    (unsigned)analog.analogOutputLevels);
                textLine(x, y, buf, lineH);
            }
            else if (analog.sparkConnected)
            {
                swprintf_s(buf, L"Analog input: SparkLink %04X:%04X | keys %u | route %.1f Hz | matrix %.1f Hz | tx %u/%u us",
                    (unsigned)analog.sparkVendorId,
                    (unsigned)analog.sparkProductId,
                    (unsigned)analog.sparkMappedAnalogKeys,
                    (double)analog.sparkRouteHz10 / 10.0,
                    (double)analog.sparkMatrixHz10 / 10.0,
                    (unsigned)analog.sparkAvgRouteTxUs,
                    (unsigned)analog.sparkMaxRouteTxUs);
                textLine(x, y, buf, lineH);
                swprintf_s(buf, L"Analog resolution: HallJoy %u levels (0.1%%) | SparkLink raw full-scale %u..%u | rows %d/%d | ok/fail %u/%u",
                    (unsigned)analog.analogOutputLevels,
                    (unsigned)analog.sparkObservedRawMin,
                    (unsigned)analog.sparkObservedRawMax,
                    analog.sparkActiveRows,
                    analog.sparkRows,
                    (unsigned)analog.sparkRouteOk,
                    (unsigned)analog.sparkRouteFail);
                textLine(x, y, buf, lineH);
            }
            else if (analog.sayoConnected)
            {
                swprintf_s(buf, L"Analog input: SayoDevice %04X:%04X | readers %d | depth %.1f Hz | avg/max interval %u/%u us",
                    (unsigned)analog.sayoVendorId,
                    (unsigned)analog.sayoProductId,
                    analog.sayoReaders,
                    (double)analog.sayoDepthHz10 / 10.0,
                    (unsigned)analog.sayoAvgDepthIntervalUs,
                    (unsigned)analog.sayoMaxDepthIntervalUs);
                textLine(x, y, buf, lineH);
                swprintf_s(buf, L"Analog resolution: HallJoy %u levels (0.1%%) | Sayo raw %u levels | native HID depth polling",
                    (unsigned)analog.analogOutputLevels,
                    (unsigned)analog.sayoDepthRawLevels);
                textLine(x, y, buf, lineH);
            }
            else if (analog.addressedPresent)
            {
                swprintf_s(buf, L"Analog input: Addressed 09/94/02 candidate %04X:%04X | mapped %u | waiting for fresh polling",
                    (unsigned)analog.addressedVendorId,
                    (unsigned)analog.addressedProductId,
                    (unsigned)analog.addressedMappedKeys);
                textLine(x, y, buf, lineH);
                textLine(x, y, L"The device was capability-validated and reserved from UAP before startup", lineH);
            }
            else if (analog.hex80Present)
            {
                textLine(x, y, L"Analog input: ATK x QK Hex80 detected | waiting for validated native 0x96 polling", lineH);
                textLine(x, y, L"UAP ownership is retained unless both read-only capability probes validate", lineH);
            }
            else if (analog.mad68Present)
            {
                swprintf_s(buf, L"Analog input: MADLIONS native candidate 373B:%04X | firmware %04X | waiting for validated A0 stream | coverage %u/68",
                    (unsigned)analog.mad68ProductId, (unsigned)analog.mad68FirmwareVersion, (unsigned)analog.mad68Coverage);
                textLine(x, y, buf, lineH);
                textLine(x, y, L"Fallback remains active until native W/A/S/D or full-matrix publication is confirmed", lineH);
            }
            else
            {
                const BackendNativeProtocolTelemetry* genericNative = nullptr;
                for (int i = 0; i < analog.nativeProtocolCount && i < kBackendMaxNativeProtocols; ++i)
                {
                    const auto& candidate = analog.nativeProtocols[i];
                    const bool knownDetailed = std::strcmp(candidate.id, "mad68-a0") == 0 ||
                        std::strcmp(candidate.id, "hex80-0x96") == 0 ||
                        std::strcmp(candidate.id, "addressed-099402") == 0 ||
                        std::strcmp(candidate.id, "sparklink") == 0 ||
                        std::strcmp(candidate.id, "sayo-depth") == 0;
                    if (!knownDetailed && (candidate.connected || candidate.present))
                    {
                        genericNative = &candidate;
                        break;
                    }
                }
                if (genericNative)
                {
                    swprintf_s(buf, L"Analog input: %s %04X:%04X | %s | keys %u | active %u | %.1f Hz",
                        genericNative->name,
                        (unsigned)genericNative->vendorId,
                        (unsigned)genericNative->productId,
                        genericNative->connected ? L"connected" : L"detected",
                        (unsigned)genericNative->mappedKeys,
                        (unsigned)genericNative->activeKeys,
                        (double)genericNative->updateHz10 / 10.0);
                    textLine(x, y, buf, lineH);
                    swprintf_s(buf, L"Protocol status: %s | HID %04X:%04X | reports %u/%u bytes",
                        genericNative->status,
                        (unsigned)genericNative->usagePage,
                        (unsigned)genericNative->usage,
                        (unsigned)genericNative->inputReportBytes,
                        (unsigned)genericNative->outputReportBytes);
                    textLine(x, y, buf, lineH);
                }
                else if (analog.sdkInitialised)
                {
                    swprintf_s(buf, L"Analog input: Wooting Analog SDK | devices %d | HallJoy poll target %.1f Hz | keycode mode %d",
                        analog.deviceCount,
                        (double)analog.sdkPollHz10 / 10.0,
                        analog.keycodeMode);
                    textLine(x, y, buf, lineH);
                    swprintf_s(buf, L"Analog resolution: HallJoy %u levels (0.1%%) | SDK float source | tracked raw/out peak %u/%u",
                        (unsigned)analog.analogOutputLevels,
                        (unsigned)analog.trackedMaxRawMilli,
                        (unsigned)analog.trackedMaxOutMilli);
                    textLine(x, y, buf, lineH);
                }
                else
                {
                    textLine(x, y, L"Analog input: no analog source connected", lineH);
                }
            }
        }
#endif
        }

        for (int pad = 0; pad < padCount; ++pad)
        {
            int col = pad % cols;
            int row = pad / cols;
            int left = margin + col * (cardW + cardGap);
            int top = margin - scrollY + analogInfoH + (showAnalogInfo ? cardGap : 0) + row * (cardH + cardGap);

            RECT card{ left, top, left + cardW, top + cardH };
            FillRect(memDC, &card, UiTheme::Brush_ControlBg());
            Rectangle(memDC, card.left, card.top, card.right, card.bottom);

            XUSB_REPORT r = Backend_GetLastReportForPad(pad);

            int x0 = left + S(hWnd, 10);
            int y = top + S(hWnd, 8);
            int lineH = S(hWnd, 16);
            int barH = S(hWnd, 14);
            int trigH = S(hWnd, 12);
            int barGapX = S(hWnd, 8);
            int contentW = std::max(40, (int)(card.right - card.left) - S(hWnd, 20));
            int halfW = std::max(16, (contentW - barGapX) / 2);

            wchar_t buf[256]{};
            swprintf_s(buf, L"Gamepad %d", pad + 1);
            textLine(x0, y, buf, lineH + S(hWnd, 2));

            swprintf_s(buf, L"LX:%6d  LY:%6d", (int)r.sThumbLX, (int)r.sThumbLY);
            textLine(x0, y, buf, lineH);
            RECT barLX{ x0, y, x0 + halfW, y + barH };
            RECT barLY{ x0 + halfW + barGapX, y, x0 + halfW + barGapX + halfW, y + barH };
            GamepadRender_DrawAxisBarCentered(memDC, barLX, r.sThumbLX);
            GamepadRender_DrawAxisBarCentered(memDC, barLY, r.sThumbLY);
            y += barH + S(hWnd, 6);

            swprintf_s(buf, L"RX:%6d  RY:%6d", (int)r.sThumbRX, (int)r.sThumbRY);
            textLine(x0, y, buf, lineH);
            RECT barRX{ x0, y, x0 + halfW, y + barH };
            RECT barRY{ x0 + halfW + barGapX, y, x0 + halfW + barGapX + halfW, y + barH };
            GamepadRender_DrawAxisBarCentered(memDC, barRX, r.sThumbRX);
            GamepadRender_DrawAxisBarCentered(memDC, barRY, r.sThumbRY);
            y += barH + S(hWnd, 6);

            swprintf_s(buf, L"LT:%3u  RT:%3u", (unsigned)r.bLeftTrigger, (unsigned)r.bRightTrigger);
            textLine(x0, y, buf, lineH);
            RECT barLT{ x0, y, x0 + halfW, y + trigH };
            RECT barRT{ x0 + halfW + barGapX, y, x0 + halfW + barGapX + halfW, y + trigH };
            GamepadRender_DrawTriggerBar01(memDC, barLT, r.bLeftTrigger);
            GamepadRender_DrawTriggerBar01(memDC, barRT, r.bRightTrigger);
            y += trigH + S(hWnd, 6);

            textLine(x0, y, L"Buttons: " + GamepadRender_ButtonsToString(r.wButtons), lineH);
        }

        if (state)
            CustomPageSurface_DrawScrollbar(hWnd, memDC, &state->surface, state->scroll.draggingThumb);

        SelectObject(memDC, oldFont);
        SelectObject(memDC, oldBrushGlobal);
        SelectObject(memDC, oldPenGlobal);
        DeleteObject(cardPen);
        EndDoubleBufferPaint(hWnd, ps, memDC, bmp, oldBmp);
        return 0;
    }

    case WM_MOUSEWHEEL:
        if (state)
        {
            CustomPageSurface_HandleScrollMessage(hWnd, &state->surface, &state->scroll,
                msg, wParam, lParam, S(hWnd, 54));
            return 0;
        }
        break;

    case WM_LBUTTONDOWN:
        if (state)
        {
            if (CustomPageSurface_HandleScrollMessage(hWnd, &state->surface, &state->scroll,
                msg, wParam, lParam, S(hWnd, 54)) != CustomPageScrollResult::NotHandled)
                return 0;
        }
        break;

    case WM_MOUSEMOVE:
        if (state && state->scroll.draggingThumb)
        {
            CustomPageSurface_HandleScrollMessage(hWnd, &state->surface, &state->scroll,
                msg, wParam, lParam, S(hWnd, 54));
            return 0;
        }
        break;

    case WM_LBUTTONUP:
        if (state && state->scroll.draggingThumb)
        {
            CustomPageSurface_HandleScrollMessage(hWnd, &state->surface, &state->scroll,
                msg, wParam, lParam, S(hWnd, 54));
            return 0;
        }
        break;

    case WM_CAPTURECHANGED:
        if (state)
        {
            CustomPageSurface_HandleScrollMessage(hWnd, &state->surface, &state->scroll,
                msg, wParam, lParam, S(hWnd, 54));
        }
        return 0;

    case WM_SIZE:
        if (state)
            CustomPageSurface_SetScrollY(hWnd, &state->surface, state->surface.scrollY);
        return 0;

    case WM_NCDESTROY:
        if (state)
        {
            CustomPageSurface_Destroy(&state->surface);
            delete state;
            SetWindowLongPtrW(hWnd, GWLP_USERDATA, 0);
        }
        return DefWindowProcW(hWnd, msg, wParam, lParam);
    }

    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

// ============================================================================
// Input Overlay page
// ============================================================================
static void AddRoundRectPath(GraphicsPath& path, const RectF& r, float rad);
static HWND PremiumSlider_Create(HWND parent, HINSTANCE hInst, int x, int y, int w, int h, int id);
static HWND PremiumChip_Create(HWND parent, HINSTANCE hInst, int x, int y, int w, int h, int id);

static constexpr int OVERLAY_ID_TOGGLE = 9101;
static constexpr int OVERLAY_ID_OPEN = 9102;
static constexpr int OVERLAY_ID_COPY = 9103;
static constexpr int OVERLAY_ID_PORT = 9104;
static constexpr int OVERLAY_ID_DIRECTION = 9105;
static constexpr int OVERLAY_ID_DEPTH_SOURCE = 9106;
static constexpr int OVERLAY_ID_EFFECT_SMOOTHING = 9120;
static constexpr int OVERLAY_ID_EFFECT_GLASS = 9121;
static constexpr int OVERLAY_ID_EFFECT_BLOOM = 9122;
static constexpr int OVERLAY_ID_EFFECT_EDGE = 9123;
static constexpr int OVERLAY_ID_EFFECT_SCALE = 9124;
static constexpr int OVERLAY_ID_EFFECT_LABEL = 9125;
static constexpr int OVERLAY_ID_EFFECT_RIM_LIGHT = 9126;
static constexpr int OVERLAY_ID_COLOR_HUE = 9140;
static constexpr int OVERLAY_ID_COLOR_PREVIEW = 9141;
static constexpr int OVERLAY_ID_COLOR_HEX = 9142;
static constexpr int OVERLAY_ID_LABEL_FONT = 9143;
static constexpr int OVERLAY_ID_LABEL_SIZE = 9144;
static constexpr int OVERLAY_ID_LABEL_SHADOW = 9145;
static constexpr int OVERLAY_ID_LABEL_COLOR_PREVIEW = 9146;
static constexpr int OVERLAY_ID_LABEL_COLOR_HUE = 9147;
static constexpr int OVERLAY_ID_LABEL_COLOR_HEX = 9148;
static constexpr int OVERLAY_ID_STRENGTH_SMOOTHING = 9150;
static constexpr int OVERLAY_ID_STRENGTH_GLASS = 9151;
static constexpr int OVERLAY_ID_STRENGTH_BLOOM = 9152;
static constexpr int OVERLAY_ID_STRENGTH_EDGE = 9153;
static constexpr int OVERLAY_ID_STRENGTH_SCALE = 9154;
static constexpr int OVERLAY_ID_STRENGTH_LABEL = 9155;
static constexpr int OVERLAY_ID_STRENGTH_RIM_LIGHT = 9156;
static constexpr int OVERLAY_ID_REFRESH_MS = 9160;
static constexpr int OVERLAY_ID_LAYOUT = 9161;
static constexpr int OVERLAY_ID_LAYOUT_EDIT = 9162;
static constexpr int OVERLAY_ID_BRAND = 9163;
static constexpr int OVERLAY_ID_VARIANT = 9164;
static void LayoutEditor_OpenWindow(HWND hOwnerPage, int presetIdx = -1);

struct InputOverlayPageState
{
    HWND lblTitle = nullptr;
    HWND lblPort = nullptr;
    HWND edtPort = nullptr;
    HWND lblDirection = nullptr;
    HWND btnDirection = nullptr;
    HWND lblDepthSource = nullptr;
    HWND btnDepthSource = nullptr;
    HWND lblEffects = nullptr;
    HWND chkSmoothing = nullptr;
    HWND chkGlass = nullptr;
    HWND chkBloom = nullptr;
    HWND chkEdge = nullptr;
    HWND chkScale = nullptr;
    HWND chkLabel = nullptr;
    HWND chkRimLight = nullptr;
    HWND sldSmoothingStrength = nullptr;
    HWND chipSmoothingStrength = nullptr;
    HWND sldGlassStrength = nullptr;
    HWND chipGlassStrength = nullptr;
    HWND sldBloomStrength = nullptr;
    HWND chipBloomStrength = nullptr;
    HWND sldEdgeStrength = nullptr;
    HWND chipEdgeStrength = nullptr;
    HWND sldScaleStrength = nullptr;
    HWND chipScaleStrength = nullptr;
    HWND sldLabelStrength = nullptr;
    HWND chipLabelStrength = nullptr;
    HWND sldRimLightStrength = nullptr;
    HWND chipRimLightStrength = nullptr;
    HWND lblRefreshMs = nullptr;
    HWND sldRefreshMs = nullptr;
    HWND chipRefreshMs = nullptr;
    HWND lblColor = nullptr;
    HWND hueBar = nullptr;
    HWND colorPreview = nullptr;
    HWND lblHex = nullptr;
    HWND edtHex = nullptr;
    HWND lblUrlCaption = nullptr;
    HWND lblUrl = nullptr;
    HWND lblStatusCaption = nullptr;
    HWND lblStatus = nullptr;
    HWND btnToggle = nullptr;
    HWND btnOpen = nullptr;
    HWND btnCopy = nullptr;
    HWND lblHint = nullptr;
    int scrollY = 0;
    int contentHeight = 0;
    bool scrollDrag = false;
    int scrollDragGrabOffsetY = 0;
    int scrollDragThumbHeight = 0;
    int scrollDragMax = 0;
    bool updatingColorText = false;
};

static COLORREF OverlayPage_ColorRefFromRgb(uint32_t rgb)
{
    return RGB((rgb >> 16) & 0xffu, (rgb >> 8) & 0xffu, rgb & 0xffu);
}

static uint32_t OverlayPage_RgbFromColorRef(COLORREF c)
{
    return ((uint32_t)GetRValue(c) << 16) | ((uint32_t)GetGValue(c) << 8) | (uint32_t)GetBValue(c);
}

static uint32_t OverlayPage_RgbFromHue(double hue)
{
    hue = std::fmod(hue, 360.0);
    if (hue < 0.0) hue += 360.0;
    double c = 1.0;
    double x = c * (1.0 - std::fabs(std::fmod(hue / 60.0, 2.0) - 1.0));
    double r = 0.0, g = 0.0, b = 0.0;
    if (hue < 60.0) { r = c; g = x; }
    else if (hue < 120.0) { r = x; g = c; }
    else if (hue < 180.0) { g = c; b = x; }
    else if (hue < 240.0) { g = x; b = c; }
    else if (hue < 300.0) { r = x; b = c; }
    else { r = c; b = x; }
    return ((uint32_t)std::lround(r * 255.0) << 16) |
        ((uint32_t)std::lround(g * 255.0) << 8) |
        (uint32_t)std::lround(b * 255.0);
}

static uint32_t OverlayPage_RgbFromHueLightness(double hue, double lightness)
{
    hue = std::fmod(hue, 360.0);
    if (hue < 0.0) hue += 360.0;
    lightness = std::clamp(lightness, 0.0, 1.0);

    double c = 1.0 - std::fabs(2.0 * lightness - 1.0);
    double hp = hue / 60.0;
    double x = c * (1.0 - std::fabs(std::fmod(hp, 2.0) - 1.0));
    double r = 0.0, g = 0.0, b = 0.0;
    if (hp < 1.0) { r = c; g = x; }
    else if (hp < 2.0) { r = x; g = c; }
    else if (hp < 3.0) { g = c; b = x; }
    else if (hp < 4.0) { g = x; b = c; }
    else if (hp < 5.0) { r = x; b = c; }
    else { r = c; b = x; }
    double m = lightness - c * 0.5;
    auto ch = [](double v) -> uint32_t { return (uint32_t)std::clamp((int)std::lround(v * 255.0), 0, 255); };
    return (ch(r + m) << 16) | (ch(g + m) << 8) | ch(b + m);
}

static uint32_t OverlayPage_RgbFromHsv(double hue, double sat, double val)
{
    hue = std::fmod(hue, 360.0);
    if (hue < 0.0) hue += 360.0;
    sat = std::clamp(sat, 0.0, 1.0);
    val = std::clamp(val, 0.0, 1.0);
    double c = val * sat;
    double hp = hue / 60.0;
    double x = c * (1.0 - std::fabs(std::fmod(hp, 2.0) - 1.0));
    double r = 0.0, g = 0.0, b = 0.0;
    if (hp < 1.0) { r = c; g = x; }
    else if (hp < 2.0) { r = x; g = c; }
    else if (hp < 3.0) { g = c; b = x; }
    else if (hp < 4.0) { g = x; b = c; }
    else if (hp < 5.0) { r = x; b = c; }
    else { r = c; b = x; }
    double m = val - c;
    auto ch = [](double v) -> uint32_t { return (uint32_t)std::clamp((int)std::lround(v * 255.0), 0, 255); };
    return (ch(r + m) << 16) | (ch(g + m) << 8) | ch(b + m);
}

static double OverlayPage_HueFromRgb(uint32_t rgb)
{
    double r = (double)((rgb >> 16) & 0xffu) / 255.0;
    double g = (double)((rgb >> 8) & 0xffu) / 255.0;
    double b = (double)(rgb & 0xffu) / 255.0;
    double mx = std::max(r, std::max(g, b));
    double mn = std::min(r, std::min(g, b));
    double d = mx - mn;
    if (d <= 0.00001)
        return 200.0;
    double h = 0.0;
    if (mx == r)
        h = 60.0 * std::fmod(((g - b) / d), 6.0);
    else if (mx == g)
        h = 60.0 * (((b - r) / d) + 2.0);
    else
        h = 60.0 * (((r - g) / d) + 4.0);
    if (h < 0.0)
        h += 360.0;
    return h;
}

static double OverlayPage_LightnessFromRgb(uint32_t rgb)
{
    double r = (double)((rgb >> 16) & 0xffu) / 255.0;
    double g = (double)((rgb >> 8) & 0xffu) / 255.0;
    double b = (double)(rgb & 0xffu) / 255.0;
    double mx = std::max(r, std::max(g, b));
    double mn = std::min(r, std::min(g, b));
    return std::clamp((mx + mn) * 0.5, 0.0, 1.0);
}

static void OverlayPage_HsvFromRgb(uint32_t rgb, double& hue, double& sat, double& val)
{
    double r = (double)((rgb >> 16) & 0xffu) / 255.0;
    double g = (double)((rgb >> 8) & 0xffu) / 255.0;
    double b = (double)(rgb & 0xffu) / 255.0;
    double mx = std::max(r, std::max(g, b));
    double mn = std::min(r, std::min(g, b));
    double d = mx - mn;
    val = std::clamp(mx, 0.0, 1.0);
    sat = (mx <= 0.00001) ? 0.0 : std::clamp(d / mx, 0.0, 1.0);
    if (d <= 0.00001)
    {
        hue = 0.0;
        return;
    }
    if (mx == r)
        hue = 60.0 * std::fmod(((g - b) / d), 6.0);
    else if (mx == g)
        hue = 60.0 * (((b - r) / d) + 2.0);
    else
        hue = 60.0 * (((r - g) / d) + 4.0);
    if (hue < 0.0)
        hue += 360.0;
}

static std::wstring OverlayPage_FormatHex(uint32_t rgb)
{
    wchar_t buf[16]{};
    swprintf_s(buf, L"#%02X%02X%02X",
        (unsigned)((rgb >> 16) & 0xffu),
        (unsigned)((rgb >> 8) & 0xffu),
        (unsigned)(rgb & 0xffu));
    return buf;
}

static bool OverlayPage_ParseHex(const std::wstring& text, uint32_t* rgb)
{
    if (!rgb) return false;
    std::wstring s;
    for (wchar_t ch : text)
    {
        if (!iswspace(ch))
            s.push_back(ch);
    }
    if (!s.empty() && s[0] == L'#')
        s.erase(s.begin());
    if (s.size() != 6)
        return false;
    uint32_t value = 0;
    for (wchar_t ch : s)
    {
        value <<= 4;
        if (ch >= L'0' && ch <= L'9') value |= (uint32_t)(ch - L'0');
        else if (ch >= L'a' && ch <= L'f') value |= (uint32_t)(ch - L'a' + 10);
        else if (ch >= L'A' && ch <= L'F') value |= (uint32_t)(ch - L'A' + 10);
        else return false;
    }
    *rgb = value & 0x00ffffffu;
    return true;
}

static bool OverlayPage_EffectFromId(int id, uint32_t* flag)
{
    if (!flag) return false;
    switch (id)
    {
    case OVERLAY_ID_EFFECT_SMOOTHING: *flag = OverlayEffect_Smoothing; return true;
    case OVERLAY_ID_EFFECT_GLASS: *flag = OverlayEffect_Glass; return true;
    case OVERLAY_ID_EFFECT_BLOOM: *flag = OverlayEffect_Bloom; return true;
    case OVERLAY_ID_EFFECT_EDGE: *flag = OverlayEffect_EdgeSweep; return true;
    case OVERLAY_ID_EFFECT_SCALE: *flag = OverlayEffect_MicroScale; return true;
    case OVERLAY_ID_EFFECT_LABEL: *flag = OverlayEffect_LabelContrast; return true;
    case OVERLAY_ID_EFFECT_RIM_LIGHT: *flag = OverlayEffect_GlassRimLight; return true;
    default: return false;
    }
}

static bool OverlayPage_StrengthFromId(int id, uint32_t* flag)
{
    if (!flag) return false;
    switch (id)
    {
    case OVERLAY_ID_STRENGTH_SMOOTHING: *flag = OverlayEffect_Smoothing; return true;
    case OVERLAY_ID_STRENGTH_GLASS: *flag = OverlayEffect_Glass; return true;
    case OVERLAY_ID_STRENGTH_BLOOM: *flag = OverlayEffect_Bloom; return true;
    case OVERLAY_ID_STRENGTH_EDGE: *flag = OverlayEffect_EdgeSweep; return true;
    case OVERLAY_ID_STRENGTH_SCALE: *flag = OverlayEffect_MicroScale; return true;
    case OVERLAY_ID_STRENGTH_LABEL: *flag = OverlayEffect_LabelContrast; return true;
    case OVERLAY_ID_STRENGTH_RIM_LIGHT: *flag = OverlayEffect_GlassRimLight; return true;
    default: return false;
    }
}

static void OverlayPage_DrawButton(const DRAWITEMSTRUCT* dis)
{
    if (!dis) return;

    RECT rc = dis->rcItem;
    HDC hdc = dis->hDC;

    bool disabled = (dis->itemState & ODS_DISABLED) != 0;
    bool pressed = (dis->itemState & ODS_SELECTED) != 0;
    bool hot = (dis->itemState & ODS_HOTLIGHT) != 0;

    COLORREF bg = UiTheme::Color_ControlBg();
    if (pressed)
        bg = RGB(42, 42, 44);
    else if (hot && !disabled)
        bg = RGB(40, 40, 42);

    HBRUSH br = CreateSolidBrush(bg);
    FillRect(hdc, &rc, br);
    DeleteObject(br);

    HPEN pen = CreatePen(PS_SOLID, 1, UiTheme::Color_Border());
    HGDIOBJ oldPen = SelectObject(hdc, pen);
    HGDIOBJ oldBrush = SelectObject(hdc, GetStockObject(HOLLOW_BRUSH));
    Rectangle(hdc, rc.left, rc.top, rc.right, rc.bottom);
    SelectObject(hdc, oldBrush);
    SelectObject(hdc, oldPen);
    DeleteObject(pen);

    wchar_t text[128]{};
    GetWindowTextW(dis->hwndItem, text, (int)_countof(text));

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, disabled ? UiTheme::Color_TextMuted() : UiTheme::Color_Text());
    DrawTextW(hdc, text, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

    if (dis->itemState & ODS_FOCUS)
    {
        RECT focus = rc;
        InflateRect(&focus, -4, -4);
        DrawFocusRect(hdc, &focus);
    }
}

static void OverlayPage_DrawCheckbox(const DRAWITEMSTRUCT* dis)
{
    if (!dis) return;

    RECT rc = dis->rcItem;
    HDC hdc = dis->hDC;
    uint32_t effectFlag = 0;
    bool checked = OverlayPage_EffectFromId((int)dis->CtlID, &effectFlag)
        ? OverlayServer_GetEffectEnabled(effectFlag)
        : (SendMessageW(dis->hwndItem, BM_GETCHECK, 0, 0) == BST_CHECKED);
    bool disabled = (dis->itemState & ODS_DISABLED) != 0;

    HBRUSH bg = CreateSolidBrush(UiTheme::Color_PanelBg());
    FillRect(hdc, &rc, bg);
    DeleteObject(bg);

    int box = std::min((int)(rc.bottom - rc.top - 6), S(dis->hwndItem, 18));
    RECT brc{ rc.left + 1, rc.top + ((rc.bottom - rc.top) - box) / 2, rc.left + 1 + box, rc.top + ((rc.bottom - rc.top) + box) / 2 };

    HBRUSH boxBr = CreateSolidBrush(checked ? UiTheme::Color_Accent() : UiTheme::Color_ControlBg());
    FillRect(hdc, &brc, boxBr);
    DeleteObject(boxBr);

    HPEN pen = CreatePen(PS_SOLID, 1, checked ? UiTheme::Color_Accent() : UiTheme::Color_Border());
    HGDIOBJ oldPen = SelectObject(hdc, pen);
    HGDIOBJ oldBrush = SelectObject(hdc, GetStockObject(HOLLOW_BRUSH));
    Rectangle(hdc, brc.left, brc.top, brc.right, brc.bottom);

    if (checked)
    {
        HPEN checkPen = CreatePen(PS_SOLID, 2, RGB(255, 255, 255));
        SelectObject(hdc, checkPen);
        MoveToEx(hdc, brc.left + box / 4, brc.top + box / 2, nullptr);
        LineTo(hdc, brc.left + box / 2 - 1, brc.bottom - box / 4);
        LineTo(hdc, brc.right - box / 5, brc.top + box / 4);
        SelectObject(hdc, pen);
        DeleteObject(checkPen);
    }

    SelectObject(hdc, oldBrush);
    SelectObject(hdc, oldPen);
    DeleteObject(pen);

    wchar_t text[128]{};
    GetWindowTextW(dis->hwndItem, text, (int)_countof(text));
    RECT trc{ brc.right + S(dis->hwndItem, 8), rc.top, rc.right, rc.bottom };
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, disabled ? UiTheme::Color_TextMuted() : UiTheme::Color_Text());
    DrawTextW(hdc, text, -1, &trc, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
}

static void OverlayPage_DrawHueBar(const DRAWITEMSTRUCT* dis)
{
    if (!dis) return;

    RECT dstRc = dis->rcItem;
    int dcW = std::max(1, (int)(dstRc.right - dstRc.left));
    int dcH = std::max(1, (int)(dstRc.bottom - dstRc.top));

    HDC memDC = CreateCompatibleDC(dis->hDC);
    HBITMAP bmp = memDC ? CreateCompatibleBitmap(dis->hDC, dcW, dcH) : nullptr;
    if (!memDC || !bmp)
    {
        if (bmp) DeleteObject(bmp);
        if (memDC) DeleteDC(memDC);
        return;
    }

    HGDIOBJ oldBmp = SelectObject(memDC, bmp);
    HDC hdc = memDC;
    RECT rc{ 0, 0, dcW, dcH };

    HBRUSH bg = CreateSolidBrush(UiTheme::Color_PanelBg());
    FillRect(hdc, &rc, bg);
    DeleteObject(bg);

    RECT bar = rc;
    InflateRect(&bar, -1, -4);
    int width = std::max(1, (int)(bar.right - bar.left));
    for (int x = 0; x < width; ++x)
    {
        double hue = ((double)x / (double)std::max(1, width - 1)) * 360.0;
        RECT col{ bar.left + x, bar.top, bar.left + x + 1, bar.bottom };
        HBRUSH br = CreateSolidBrush(OverlayPage_ColorRefFromRgb(OverlayPage_RgbFromHue(hue)));
        FillRect(hdc, &col, br);
        DeleteObject(br);
    }

    HPEN pen = CreatePen(PS_SOLID, 1, UiTheme::Color_Border());
    HGDIOBJ oldPen = SelectObject(hdc, pen);
    HGDIOBJ oldBrush = SelectObject(hdc, GetStockObject(HOLLOW_BRUSH));
    Rectangle(hdc, bar.left, bar.top, bar.right, bar.bottom);

    double hue = OverlayPage_HueFromRgb(OverlayServer_GetAccentColor());
    int markerX = bar.left + (int)std::lround((hue / 360.0) * (double)(width - 1));
    HPEN markerPen = CreatePen(PS_SOLID, 2, RGB(255, 255, 255));
    SelectObject(hdc, markerPen);
    MoveToEx(hdc, markerX, bar.top - 2, nullptr);
    LineTo(hdc, markerX, bar.bottom + 2);
    SelectObject(hdc, pen);
    DeleteObject(markerPen);

    SelectObject(hdc, oldBrush);
    SelectObject(hdc, oldPen);
    DeleteObject(pen);

    BitBlt(dis->hDC, dstRc.left, dstRc.top, dcW, dcH, memDC, 0, 0, SRCCOPY);
    SelectObject(memDC, oldBmp);
    DeleteObject(bmp);
    DeleteDC(memDC);
}

static void OverlayPage_DrawColorPreview(const DRAWITEMSTRUCT* dis)
{
    if (!dis) return;

    RECT rc = dis->rcItem;
    HDC hdc = dis->hDC;

    HBRUSH bg = CreateSolidBrush(UiTheme::Color_PanelBg());
    FillRect(hdc, &rc, bg);
    DeleteObject(bg);

    RECT sw = rc;
    InflateRect(&sw, -2, -2);
    HBRUSH br = CreateSolidBrush(OverlayPage_ColorRefFromRgb(OverlayServer_GetAccentColor()));
    FillRect(hdc, &sw, br);
    DeleteObject(br);

    HPEN pen = CreatePen(PS_SOLID, 1, UiTheme::Color_Border());
    HGDIOBJ oldPen = SelectObject(hdc, pen);
    HGDIOBJ oldBrush = SelectObject(hdc, GetStockObject(HOLLOW_BRUSH));
    Rectangle(hdc, sw.left, sw.top, sw.right, sw.bottom);
    SelectObject(hdc, oldBrush);
    SelectObject(hdc, oldPen);
    DeleteObject(pen);
}

static uint16_t OverlayPage_GetPort(InputOverlayPageState* st)
{
    if (!st || !st->edtPort)
        return OverlayServer_GetConfiguredPort();

    wchar_t buf[32]{};
    GetWindowTextW(st->edtPort, buf, (int)_countof(buf));
    wchar_t* end = nullptr;
    unsigned long port = wcstoul(buf, &end, 10);
    if (port < 1 || port > 65535)
        return OverlayServer_GetConfiguredPort();
    return (uint16_t)port;
}

static std::wstring OverlayPage_BuildUrl(InputOverlayPageState* st)
{
    uint16_t port = OverlayServer_IsRunning() ? OverlayServer_GetPort() : OverlayPage_GetPort(st);
    if (port == 0)
        port = 8765;

    wchar_t buf[96]{};
    swprintf_s(buf, L"http://127.0.0.1:%u/", (unsigned)port);
    return buf;
}

static bool OverlayPage_SetClipboardText(HWND hWnd, const std::wstring& text)
{
    if (!OpenClipboard(hWnd))
        return false;
    if (!EmptyClipboard())
    {
        CloseClipboard();
        return false;
    }
    size_t bytes = (text.size() + 1u) * sizeof(wchar_t);
    bool copied = false;
    HGLOBAL mem = GlobalAlloc(GMEM_MOVEABLE, bytes);
    if (mem)
    {
        void* dst = GlobalLock(mem);
        if (dst)
        {
            memcpy(dst, text.c_str(), bytes);
            GlobalUnlock(mem);
            if (SetClipboardData(CF_UNICODETEXT, mem))
            {
                copied = true;
                mem = nullptr; // ownership transfers only after a successful call
            }
        }
    }
    if (mem)
        GlobalFree(mem);
    CloseClipboard();
    return copied;
}

static void OverlayPage_UpdateColorControls(InputOverlayPageState* st)
{
    if (!st) return;
    uint32_t color = OverlayServer_GetAccentColor();
    if (st->edtHex)
    {
        st->updatingColorText = true;
        std::wstring hex = OverlayPage_FormatHex(color);
        SetWindowTextW(st->edtHex, hex.c_str());
        st->updatingColorText = false;
    }
    if (st->hueBar)
        InvalidateRect(st->hueBar, nullptr, FALSE);
    if (st->colorPreview)
        InvalidateRect(st->colorPreview, nullptr, FALSE);
}

static void OverlayPage_UpdateSliderControls(InputOverlayPageState* st)
{
    if (!st) return;
    struct StrengthRow { HWND slider; HWND chip; uint32_t flag; };
    StrengthRow rows[] = {
        { st->sldSmoothingStrength, st->chipSmoothingStrength, OverlayEffect_Smoothing },
        { st->sldGlassStrength, st->chipGlassStrength, OverlayEffect_Glass },
        { st->sldBloomStrength, st->chipBloomStrength, OverlayEffect_Bloom },
        { st->sldEdgeStrength, st->chipEdgeStrength, OverlayEffect_EdgeSweep },
        { st->sldScaleStrength, st->chipScaleStrength, OverlayEffect_MicroScale },
        { st->sldLabelStrength, st->chipLabelStrength, OverlayEffect_LabelContrast },
        { st->sldRimLightStrength, st->chipRimLightStrength, OverlayEffect_GlassRimLight },
    };
    for (const StrengthRow& row : rows)
    {
        int value = OverlayServer_GetEffectStrengthPercent(row.flag);
        if (row.slider)
            SendMessageW(row.slider, TBM_SETPOS, TRUE, value);
        if (row.chip)
        {
            wchar_t b[32]{};
            swprintf_s(b, L"%d%%", value);
            SetWindowTextW(row.chip, b);
        }
    }
    if (st->sldRefreshMs)
        SendMessageW(st->sldRefreshMs, TBM_SETPOS, TRUE, OverlayServer_GetRefreshIntervalMs());
    if (st->chipRefreshMs)
    {
        wchar_t b[32]{};
        swprintf_s(b, L"%d ms", OverlayServer_GetRefreshIntervalMs());
        SetWindowTextW(st->chipRefreshMs, b);
    }
}

static void OverlayPage_RequestSave(HWND hWnd)
{
    HWND root = GetAncestor(hWnd, GA_ROOT);
    if (root)
        PostMessageW(root, WM_APP_REQUEST_SAVE, 0, 0);
}

static void OverlayPage_SetColorFromHuePoint(HWND hueBar, int x)
{
    if (!hueBar) return;
    RECT rc{};
    GetClientRect(hueBar, &rc);
    RECT bar = rc;
    InflateRect(&bar, -1, -4);
    int width = std::max(1, (int)(bar.right - bar.left));
    double t = (double)std::clamp((int)(x - bar.left), 0, std::max(1, width - 1)) / (double)std::max(1, width - 1);
    OverlayServer_SetAccentColor(OverlayPage_RgbFromHue(t * 360.0));

    HWND parent = GetParent(hueBar);
    auto* st = parent ? (InputOverlayPageState*)GetWindowLongPtrW(parent, GWLP_USERDATA) : nullptr;
    OverlayPage_UpdateColorControls(st);
    if (parent)
        OverlayPage_RequestSave(parent);
}

static LRESULT CALLBACK OverlayHueBar_SubclassProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam,
    UINT_PTR, DWORD_PTR)
{
    switch (msg)
    {
    case WM_LBUTTONDOWN:
    case WM_LBUTTONDBLCLK:
        SetCapture(hWnd);
        OverlayPage_SetColorFromHuePoint(hWnd, (short)LOWORD(lParam));
        return 0;

    case WM_MOUSEMOVE:
        if ((wParam & MK_LBUTTON) && GetCapture() == hWnd)
        {
            OverlayPage_SetColorFromHuePoint(hWnd, (short)LOWORD(lParam));
            return 0;
        }
        break;

    case WM_LBUTTONUP:
        if (GetCapture() == hWnd)
            ReleaseCapture();
        return 0;

    case WM_NCDESTROY:
        RemoveWindowSubclass(hWnd, OverlayHueBar_SubclassProc, 1);
        break;
    }
    return DefSubclassProc(hWnd, msg, wParam, lParam);
}

static void OverlayPage_Update(InputOverlayPageState* st)
{
    if (!st) return;

    bool running = OverlayServer_IsRunning();
    std::wstring url = OverlayPage_BuildUrl(st);
    std::wstring err = OverlayServer_GetLastError();

    if (st->lblUrl)
        SetWindowTextW(st->lblUrl, url.c_str());
    if (st->lblStatus)
    {
        std::wstring status = running ? L"Running" : L"Stopped";
        if (!err.empty())
            status += L" (" + err + L")";
        SetWindowTextW(st->lblStatus, status.c_str());
    }
    if (st->btnToggle)
        SetWindowTextW(st->btnToggle, running ? L"Stop server" : L"Start server");
    if (st->btnOpen)
        EnableWindow(st->btnOpen, running ? TRUE : FALSE);
    if (st->btnCopy)
        EnableWindow(st->btnCopy, TRUE);
    if (st->edtPort)
        EnableWindow(st->edtPort, running ? FALSE : TRUE);
    if (st->btnDirection)
    {
        OverlayFillDirection direction = OverlayServer_GetFillDirection();
        SetWindowTextW(st->btnDirection,
            direction == OverlayFillDirection::TopDown ? L"Top to bottom" : L"Bottom to top");
    }
    if (st->btnDepthSource)
    {
        SetWindowTextW(st->btnDepthSource,
            OverlayServer_GetUseRawDepth() ? L"Raw press depth" : L"After curves");
    }

    struct EffectCheck { HWND hwnd; uint32_t flag; };
    EffectCheck checks[] = {
        { st->chkSmoothing, OverlayEffect_Smoothing },
        { st->chkGlass, OverlayEffect_Glass },
        { st->chkBloom, OverlayEffect_Bloom },
        { st->chkEdge, OverlayEffect_EdgeSweep },
        { st->chkScale, OverlayEffect_MicroScale },
        { st->chkLabel, OverlayEffect_LabelContrast },
        { st->chkRimLight, OverlayEffect_GlassRimLight },
    };
    uint32_t flags = OverlayServer_GetEffectFlags();
    for (const EffectCheck& check : checks)
    {
        if (check.hwnd)
            SendMessageW(check.hwnd, BM_SETCHECK, (flags & check.flag) ? BST_CHECKED : BST_UNCHECKED, 0);
    }
    OverlayPage_UpdateColorControls(st);
    OverlayPage_UpdateSliderControls(st);
}

static void OverlayPage_SetControlRedraw(InputOverlayPageState* st, bool enabled)
{
    if (!st) return;
    HWND controls[] = {
        st->lblTitle, st->lblPort, st->edtPort, st->lblDirection, st->btnDirection,
        st->lblDepthSource, st->btnDepthSource,
        st->lblEffects, st->chkSmoothing, st->chkGlass, st->chkBloom,
        st->chkEdge, st->chkScale, st->chkLabel, st->chkRimLight,
        st->sldSmoothingStrength, st->chipSmoothingStrength,
        st->sldGlassStrength, st->chipGlassStrength,
        st->sldBloomStrength, st->chipBloomStrength,
        st->sldEdgeStrength, st->chipEdgeStrength,
        st->sldScaleStrength, st->chipScaleStrength,
        st->sldLabelStrength, st->chipLabelStrength,
        st->sldRimLightStrength, st->chipRimLightStrength,
        st->lblRefreshMs, st->sldRefreshMs, st->chipRefreshMs,
        st->lblColor,
        st->colorPreview, st->hueBar, st->lblHex, st->edtHex,
        st->lblUrlCaption, st->lblUrl, st->lblStatusCaption, st->lblStatus,
        st->btnToggle, st->btnOpen, st->btnCopy, st->lblHint
    };
    WPARAM value = enabled ? TRUE : FALSE;
    for (HWND control : controls)
    {
        if (control)
            SendMessageW(control, WM_SETREDRAW, value, 0);
    }
}

static int OverlayPage_ScrollbarWidthPx(HWND hWnd) { return S(hWnd, 12); }
static int OverlayPage_ScrollbarMarginPx(HWND hWnd) { return S(hWnd, 8); }

static int OverlayPage_GetMaxScroll(HWND hWnd, InputOverlayPageState* st)
{
    if (!st) return 0;
    RECT rc{};
    GetClientRect(hWnd, &rc);
    int clientH = std::max(0, (int)(rc.bottom - rc.top));
    return std::max(0, st->contentHeight - clientH);
}

static RECT OverlayPage_GetScrollTrackRect(HWND hWnd)
{
    RECT rc{};
    GetClientRect(hWnd, &rc);
    int w = OverlayPage_ScrollbarWidthPx(hWnd);
    int m = OverlayPage_ScrollbarMarginPx(hWnd);
    return RECT{
        rc.right - m - w,
        rc.top + m,
        rc.right - m,
        rc.bottom - m
    };
}

static RECT OverlayPage_GetScrollThumbRect(HWND hWnd, InputOverlayPageState* st)
{
    RECT tr = OverlayPage_GetScrollTrackRect(hWnd);
    int trackH = std::max(1, (int)(tr.bottom - tr.top));
    int maxScroll = OverlayPage_GetMaxScroll(hWnd, st);
    if (maxScroll <= 0)
        return tr;
    RECT rc{};
    GetClientRect(hWnd, &rc);
    int clientH = std::max(1, (int)(rc.bottom - rc.top));
    int thumbH = std::clamp((clientH * trackH) / std::max(clientH, st ? st->contentHeight : clientH), S(hWnd, 28), trackH);
    int travel = std::max(0, trackH - thumbH);
    int top = tr.top;
    if (travel > 0)
    {
        double t = (double)std::clamp(st ? st->scrollY : 0, 0, maxScroll) / (double)maxScroll;
        top += (int)std::lround(t * (double)travel);
    }
    return RECT{ tr.left, top, tr.right, top + thumbH };
}

static void OverlayPage_OffsetAllChildren(HWND hWnd, int dy)
{
    if (dy == 0) return;

    int count = 0;
    for (HWND c = GetWindow(hWnd, GW_CHILD); c; c = GetWindow(c, GW_HWNDNEXT))
        ++count;
    if (count <= 0) return;

    HDWP hdwp = BeginDeferWindowPos(count);
    for (HWND c = GetWindow(hWnd, GW_CHILD); c; c = GetWindow(c, GW_HWNDNEXT))
    {
        RECT r{};
        if (!GetWindowRect(c, &r))
            continue;
        MapWindowPoints(nullptr, hWnd, (LPPOINT)&r, 2);

        if (hdwp)
        {
            hdwp = DeferWindowPos(hdwp, c, nullptr, r.left, r.top + dy, 0, 0,
                SWP_NOZORDER | SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOOWNERZORDER | SWP_NOREDRAW);
        }
        else
        {
            SetWindowPos(c, nullptr, r.left, r.top + dy, 0, 0,
                SWP_NOZORDER | SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOOWNERZORDER | SWP_NOREDRAW);
        }
    }
    if (hdwp)
        EndDeferWindowPos(hdwp);
}

static void OverlayPage_RequestScrollRepaint(HWND hWnd)
{
    RedrawWindow(hWnd, nullptr, nullptr,
        RDW_INVALIDATE | RDW_NOERASE | RDW_ALLCHILDREN | RDW_UPDATENOW);
}

static void OverlayPage_Layout(HWND hWnd, InputOverlayPageState* st);

static void OverlayPage_SetScrollY(HWND hWnd, InputOverlayPageState* st, int newScrollY)
{
    if (!st) return;
    int target = std::clamp(newScrollY, 0, OverlayPage_GetMaxScroll(hWnd, st));
    if (target == st->scrollY)
        return;

    int dy = st->scrollY - target;
    st->scrollY = target;
    OverlayPage_OffsetAllChildren(hWnd, dy);
    OverlayPage_RequestScrollRepaint(hWnd);
}

static void OverlayPage_DrawScrollbar(HWND hWnd, HDC hdc, InputOverlayPageState* st)
{
    if (!st || OverlayPage_GetMaxScroll(hWnd, st) <= 0)
        return;

    RECT tr = OverlayPage_GetScrollTrackRect(hWnd);
    RECT th = OverlayPage_GetScrollThumbRect(hWnd, st);

    Gdiplus::Graphics g(hdc);
    g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);

    Gdiplus::RectF track((float)tr.left, (float)tr.top, (float)(tr.right - tr.left), (float)(tr.bottom - tr.top));
    Gdiplus::GraphicsPath trackPath;
    AddRoundRectPath(trackPath, track, track.Width * 0.5f);
    Gdiplus::SolidBrush trackBrush(Gp(RGB(44, 44, 48), 180));
    g.FillPath(&trackBrush, &trackPath);

    Gdiplus::RectF thumb((float)th.left, (float)th.top, (float)(th.right - th.left), (float)(th.bottom - th.top));
    Gdiplus::GraphicsPath thumbPath;
    AddRoundRectPath(thumbPath, thumb, thumb.Width * 0.5f);
    Gdiplus::Color thumbC = st->scrollDrag ? Gp(UiTheme::Color_Accent(), 240) : Gp(UiTheme::Color_Accent(), 205);
    Gdiplus::SolidBrush thumbBrush(thumbC);
    g.FillPath(&thumbBrush, &thumbPath);
}

static void OverlayPage_Layout(HWND hWnd, InputOverlayPageState* st)
{
    if (!st) return;

    RECT rc{};
    GetClientRect(hWnd, &rc);

    int margin = S(hWnd, 16);
    int scrollbarReserve = OverlayPage_ScrollbarWidthPx(hWnd) + OverlayPage_ScrollbarMarginPx(hWnd) * 2;
    int x = margin;
    int y = margin;
    int w = std::max(S(hWnd, 260), (int)(rc.right - rc.left) - margin * 2 - scrollbarReserve);
    int labelH = S(hWnd, 20);
    int valueH = S(hWnd, 24);
    int editH = S(hWnd, 26);
    int btnW = S(hWnd, 132);
    int btnH = S(hWnd, 30);
    int gap = S(hWnd, 10);
    int rowGap = S(hWnd, 14);
    int portW = S(hWnd, 110);
    int directionW = std::min(w, S(hWnd, 180));
    int depthW = std::min(w, S(hWnd, 190));
    auto pos = [&](HWND child, int px, int py, int pw, int ph)
    {
        if (child)
            SetWindowPos(child, nullptr, px, py - st->scrollY, pw, ph, SWP_NOZORDER);
    };

    pos(st->lblTitle, x, y, w, labelH);
    y += labelH + rowGap;

    pos(st->lblPort, x, y, portW, labelH);
    if (st->lblDirection && st->lblDepthSource && w >= portW + gap + directionW + gap + depthW)
    {
        pos(st->lblDirection, x + portW + gap, y, directionW, labelH);
        pos(st->lblDepthSource, x + portW + gap + directionW + gap, y, depthW, labelH);
    }
    else if (st->lblDirection)
    {
        int dirLabelX = (w >= portW + gap + directionW) ? x + portW + gap : x;
        int dirLabelY = (w >= portW + gap + directionW) ? y : y + labelH + S(hWnd, 6) + editH + S(hWnd, 8);
        pos(st->lblDirection, dirLabelX, dirLabelY, directionW, labelH);
        if (st->lblDepthSource)
        {
            int depthLabelY = (w >= portW + gap + directionW)
                ? y + labelH + S(hWnd, 6) + editH + S(hWnd, 8)
                : dirLabelY + labelH + S(hWnd, 6) + editH + S(hWnd, 8);
            pos(st->lblDepthSource, x, depthLabelY, depthW, labelH);
        }
    }
    y += labelH + S(hWnd, 6);

    pos(st->edtPort, x, y, portW, editH);
    if (st->btnDirection && st->btnDepthSource && w >= portW + gap + directionW + gap + depthW)
    {
        pos(st->btnDirection, x + portW + gap, y, directionW, editH);
        pos(st->btnDepthSource, x + portW + gap + directionW + gap, y, depthW, editH);
        y += editH + rowGap;
    }
    else if (st->btnDirection)
    {
        if (w >= portW + gap + directionW)
        {
            pos(st->btnDirection, x + portW + gap, y, directionW, editH);
            y += editH + S(hWnd, 8);
            if (st->btnDepthSource)
            {
                pos(st->btnDepthSource, x, y + labelH + S(hWnd, 6), depthW, editH);
                y += labelH + S(hWnd, 6) + editH + rowGap;
            }
            else
            {
                y += rowGap - S(hWnd, 8);
            }
        }
        else
        {
            y += editH + S(hWnd, 8);
            pos(st->btnDirection, x, y + labelH + S(hWnd, 6), directionW, editH);
            y += labelH + S(hWnd, 6) + editH + S(hWnd, 8);
            if (st->btnDepthSource)
            {
                pos(st->btnDepthSource, x, y + labelH + S(hWnd, 6), depthW, editH);
                y += labelH + S(hWnd, 6) + editH + rowGap;
            }
            else
            {
                y += rowGap - S(hWnd, 8);
            }
        }
    }
    else
    {
        y += editH + rowGap;
    }

    pos(st->lblEffects, x, y, w, labelH);
    y += labelH + S(hWnd, 6);

    struct EffectRow { HWND check; HWND slider; HWND chip; };
    EffectRow effectRows[] = {
        { st->chkSmoothing, st->sldSmoothingStrength, st->chipSmoothingStrength },
        { st->chkGlass, st->sldGlassStrength, st->chipGlassStrength },
        { st->chkBloom, st->sldBloomStrength, st->chipBloomStrength },
        { st->chkEdge, st->sldEdgeStrength, st->chipEdgeStrength },
        { st->chkScale, st->sldScaleStrength, st->chipScaleStrength },
        { st->chkLabel, st->sldLabelStrength, st->chipLabelStrength },
        { st->chkRimLight, st->sldRimLightStrength, st->chipRimLightStrength },
    };
    int checkW = std::min(S(hWnd, 180), std::max(S(hWnd, 130), w / 3));
    int chipW = S(hWnd, 74);
    int effectH = S(hWnd, 28);
    for (const EffectRow& row : effectRows)
    {
        int sliderX = x + checkW + gap;
        int sliderW = std::max(S(hWnd, 130), w - checkW - chipW - gap * 2);
        pos(row.check, x, y, checkW, effectH);
        pos(row.slider, sliderX, y, sliderW, effectH);
        pos(row.chip, sliderX + sliderW + gap, y - S(hWnd, 1), chipW, effectH + S(hWnd, 2));
        y += effectH + S(hWnd, 6);
    }
    y += rowGap;

    int sliderChipW = S(hWnd, 86);
    int sliderH = S(hWnd, 28);
    pos(st->lblRefreshMs, x, y, w, labelH);
    y += labelH + S(hWnd, 4);
    int refreshSliderW = std::max(S(hWnd, 160), w - sliderChipW - gap);
    pos(st->sldRefreshMs, x, y, refreshSliderW, sliderH);
    pos(st->chipRefreshMs, x + refreshSliderW + gap, y - S(hWnd, 1), sliderChipW, sliderH + S(hWnd, 2));
    y += sliderH + rowGap;

    pos(st->lblColor, x, y, w, labelH);
    y += labelH + S(hWnd, 6);

    int preview = S(hWnd, 34);
    int hexW = S(hWnd, 110);
    int hexLabelW = S(hWnd, 34);
    int hueW = std::max(S(hWnd, 180), w - preview - hexLabelW - hexW - gap * 3);
    if (w >= preview + gap + S(hWnd, 180) + gap + hexLabelW + hexW)
    {
        pos(st->colorPreview, x, y, preview, editH);
        pos(st->hueBar, x + preview + gap, y, hueW, editH);
        pos(st->lblHex, x + preview + gap + hueW + gap, y, hexLabelW, editH);
        pos(st->edtHex, x + preview + gap + hueW + gap + hexLabelW, y, hexW, editH);
        y += editH + rowGap;
    }
    else
    {
        pos(st->colorPreview, x, y, preview, editH);
        pos(st->hueBar, x + preview + gap, y, std::max(S(hWnd, 160), w - preview - gap), editH);
        y += editH + S(hWnd, 8);
        pos(st->lblHex, x, y, hexLabelW, editH);
        pos(st->edtHex, x + hexLabelW, y, hexW, editH);
        y += editH + rowGap;
    }

    pos(st->lblUrlCaption, x, y, w, labelH);
    y += labelH + S(hWnd, 6);

    pos(st->lblUrl, x, y, w, valueH);
    y += valueH + rowGap;

    pos(st->lblStatusCaption, x, y, w, labelH);
    y += labelH + S(hWnd, 6);

    pos(st->lblStatus, x, y, w, valueH);
    y += valueH + rowGap;

    int maxButtonsW = w;
    if (maxButtonsW >= btnW * 3 + gap * 2)
    {
        if (st->btnToggle)
            pos(st->btnToggle, x, y, btnW, btnH);
        if (st->btnOpen)
            pos(st->btnOpen, x + btnW + gap, y, btnW, btnH);
        if (st->btnCopy)
            pos(st->btnCopy, x + (btnW + gap) * 2, y, btnW, btnH);
        y += btnH + rowGap;
    }
    else
    {
        int narrowW = std::min(w, S(hWnd, 220));
        if (st->btnToggle)
            pos(st->btnToggle, x, y, narrowW, btnH);
        y += btnH + S(hWnd, 8);
        if (st->btnOpen)
            pos(st->btnOpen, x, y, narrowW, btnH);
        y += btnH + S(hWnd, 8);
        if (st->btnCopy)
            pos(st->btnCopy, x, y, narrowW, btnH);
        y += btnH + rowGap;
    }

    pos(st->lblHint, x, y, w, S(hWnd, 52));
    y += S(hWnd, 52) + margin;

    st->contentHeight = y;
    int maxScroll = OverlayPage_GetMaxScroll(hWnd, st);
    if (st->scrollY > maxScroll)
    {
        st->scrollY = maxScroll;
        OverlayPage_Layout(hWnd, st);
    }
}

enum class OverlayCustomKind
{
    Label,
    Button,
    Combo,
    Checkbox,
    Slider,
    Chip,
    Edit,
    Hue,
    ColorPreview,
    Hint,
    Status,
    CopyAddress
};

struct OverlayCustomItem
{
    int id = 0;
    OverlayCustomKind kind = OverlayCustomKind::Label;
    RECT rc{};
    std::wstring text;
    uint32_t flag = 0;
    int minV = 0;
    int maxV = 100;
    int value = 0;
    bool enabled = true;
};

struct OverlayColorBitmapCache
{
    HBITMAP svBitmap = nullptr;
    int svW = 0;
    int svH = 0;
    int hueBucket = -1;
    HBITMAP hueBitmap = nullptr;
    int hueW = 0;
    int hueH = 0;
};

struct OverlayColorUiState
{
    double hue = 0.0;
    double sat = 1.0;
    double val = 1.0;
    bool initialized = false;
};

struct OverlayCustomState
{
    std::vector<OverlayCustomItem> items;
    CustomPageSurface surface;
    CustomPageScrollController scroll;
    int scrollY = 0;
    int contentHeight = 0;
    int hotId = 0;
    int pressedId = 0;
    int focusId = 0;
    int dragId = 0;
    int colorDragMode = 0; // 1 = saturation/value square, 2 = hue strip
    std::wstring portText;
    std::wstring copyFeedback;
    std::wstring hexText;
    std::wstring labelHexText;
    halljoy::overlay_edit::Editor editor;
    bool selectingText = false;
    std::wstring editFeedback;
    OverlayColorBitmapCache indicatorColorCache;
    OverlayColorBitmapCache labelColorCache;
    OverlayColorUiState indicatorColorUi;
    OverlayColorUiState labelColorUi;
    HWND comboDirection = nullptr;
    HWND comboDepthSource = nullptr;
    HWND comboLabelFont = nullptr;
    HWND comboLayout = nullptr;
    LayoutPicker layoutPicker;
};

static bool OverlayCustom_IsEdit(int id)
{
    return id == OVERLAY_ID_PORT || id == OVERLAY_ID_COLOR_HEX || id == OVERLAY_ID_LABEL_COLOR_HEX;
}
static halljoy::overlay_edit::Kind OverlayCustom_EditKind(int id)
{
    return id == OVERLAY_ID_PORT ? halljoy::overlay_edit::Kind::Port : halljoy::overlay_edit::Kind::Hex;
}
static std::wstring& OverlayCustom_EditText(OverlayCustomState* st, int id)
{
    return id == OVERLAY_ID_PORT ? st->portText : id == OVERLAY_ID_LABEL_COLOR_HEX ? st->labelHexText : st->hexText;
}
static int OverlayCustom_TextWidth(HDC dc, const std::wstring& text, size_t count)
{
    SIZE size{};
    GetTextExtentPoint32W(dc, text.c_str(), (int)std::min(count, text.size()), &size);
    return size.cx;
}

static void OverlayCustom_DestroyColorCache(OverlayColorBitmapCache& cache)
{
    if (cache.svBitmap)
    {
        DeleteObject(cache.svBitmap);
        cache.svBitmap = nullptr;
    }
    if (cache.hueBitmap)
    {
        DeleteObject(cache.hueBitmap);
        cache.hueBitmap = nullptr;
    }
    cache.svW = 0;
    cache.svH = 0;
    cache.hueBucket = -1;
    cache.hueW = 0;
    cache.hueH = 0;
}

static void OverlayCustom_DestroyCache(OverlayCustomState* st)
{
    if (!st) return;
    CustomPageSurface_Destroy(&st->surface);
    OverlayCustom_DestroyColorCache(st->indicatorColorCache);
    OverlayCustom_DestroyColorCache(st->labelColorCache);
}

static void OverlayCustom_MarkCacheDirty(HWND hWnd, OverlayCustomState* st)
{
    if (!st) return;
    CustomPageSurface_MarkDirty(hWnd, &st->surface);
}

static std::wstring OverlayCustom_BuildUrl(OverlayCustomState* st)
{
    uint16_t port = OverlayServer_IsRunning() ? OverlayServer_GetPort() : OverlayServer_GetConfiguredPort();
    if (!OverlayServer_IsRunning() && st && !st->portText.empty())
    {
        wchar_t* end = nullptr;
        unsigned long p = wcstoul(st->portText.c_str(), &end, 10);
        if (p >= 1 && p <= 65535)
            port = (uint16_t)p;
    }
    if (port == 0) port = 8765;

    wchar_t buf[96]{};
    swprintf_s(buf, L"http://127.0.0.1:%u/", (unsigned)port);
    return buf;
}

static void OverlayCustom_RequestSave(HWND hWnd)
{
    HWND root = GetAncestor(hWnd, GA_ROOT);
    if (root)
        PostMessageW(root, WM_APP_REQUEST_SAVE, 0, 0);
}

static int OverlayCustom_ScrollbarWidthPx(HWND hWnd) { return S(hWnd, 12); }
static int OverlayCustom_ScrollbarMarginPx(HWND hWnd) { return S(hWnd, 8); }

static int OverlayCustom_GetMaxScroll(HWND hWnd, OverlayCustomState* st)
{
    if (!st) return 0;
    st->surface.contentHeight = st->contentHeight;
    st->surface.scrollY = st->scrollY;
    return CustomPageSurface_GetMaxScroll(hWnd, &st->surface);
}

static RECT OverlayCustom_GetScrollTrackRect(HWND hWnd)
{
    return CustomPageSurface_GetScrollTrackRect(hWnd);
}

static RECT OverlayCustom_GetScrollThumbRect(HWND hWnd, OverlayCustomState* st)
{
    if (!st) return CustomPageSurface_GetScrollTrackRect(hWnd);
    st->surface.contentHeight = st->contentHeight;
    st->surface.scrollY = st->scrollY;
    return CustomPageSurface_GetScrollThumbRect(hWnd, &st->surface);
}

static void OverlayCustom_SetScrollY(HWND hWnd, OverlayCustomState* st, int scrollY)
{
    if (!st) return;
    st->surface.contentHeight = st->contentHeight;
    st->surface.scrollY = st->scrollY;
    CustomPageSurface_SetScrollY(hWnd, &st->surface, scrollY);
    st->scrollY = st->surface.scrollY;
}

static void OverlayCustom_AddItem(OverlayCustomState* st, int id, OverlayCustomKind kind, RECT rc, const std::wstring& text = L"")
{
    if (!st) return;
    OverlayCustomItem it{};
    it.id = id;
    it.kind = kind;
    it.rc = rc;
    it.text = text;
    st->items.push_back(std::move(it));
}

static void OverlayCustom_AddStrengthRow(HWND hWnd, OverlayCustomState* st, int& y, int x, int w, const wchar_t* label, int chkId, int sldId, uint32_t flag)
{
    int gap = S(hWnd, 10);
    int effectH = S(hWnd, 28);
    int checkW = std::min(S(hWnd, 180), std::max(S(hWnd, 130), w / 3));
    int chipW = S(hWnd, 74);
    int sliderX = x + checkW + gap;
    int sliderW = std::max(S(hWnd, 130), w - checkW - chipW - gap * 2);

    OverlayCustom_AddItem(st, chkId, OverlayCustomKind::Checkbox, RECT{ x, y, x + checkW, y + effectH }, label);
    st->items.back().flag = flag;
    st->items.back().value = OverlayServer_GetEffectEnabled(flag) ? 1 : 0;

    OverlayCustom_AddItem(st, sldId, OverlayCustomKind::Slider, RECT{ sliderX, y, sliderX + sliderW, y + effectH });
    st->items.back().flag = flag;
    st->items.back().minV = 0;
    st->items.back().maxV = 100;
    st->items.back().value = OverlayServer_GetEffectStrengthPercent(flag);

    int chipX = sliderX + sliderW + gap;
    wchar_t b[32]{};
    swprintf_s(b, L"%d%%", OverlayServer_GetEffectStrengthPercent(flag));
    OverlayCustom_AddItem(st, sldId + 100, OverlayCustomKind::Chip, RECT{ chipX, y - S(hWnd, 1), chipX + chipW, y + effectH + S(hWnd, 1) }, b);

    y += effectH + S(hWnd, 6);
}

static const wchar_t* OverlayCustom_LabelFontName(int idx)
{
    switch (std::clamp(idx, 0, 12))
    {
    case 1: return L"Bahnschrift Cond.";
    case 2: return L"Arial Black";
    case 3: return L"Impact";
    case 4: return L"Trebuchet";
    case 5: return L"Cascadia Mono";
    case 6: return L"Franklin Gothic";
    case 7: return L"Tahoma";
    case 8: return L"Comic Sans";
    case 9: return L"Yu Gothic";
    case 10: return L"Yu Mincho";
    case 11: return L"MS Gothic";
    case 12: return L"Papyrus/Gabriola";
    default: return L"Segoe UI";
    }
}

static void OverlayCustom_RebuildLayout(HWND hWnd, OverlayCustomState* st)
{
    if (!st) return;
    st->items.clear();

    RECT rc{};
    GetClientRect(hWnd, &rc);
    int margin = S(hWnd, 16);
    int scrollbarReserve = OverlayCustom_ScrollbarWidthPx(hWnd) + OverlayCustom_ScrollbarMarginPx(hWnd) * 2;
    int x = margin;
    int y = margin;
    int w = std::max(S(hWnd, 260), (int)(rc.right - rc.left) - margin * 2 - scrollbarReserve);
    int labelH = S(hWnd, 20);
    int valueH = S(hWnd, 24);
    int editH = S(hWnd, 26);
    int btnW = S(hWnd, 132);
    int btnH = S(hWnd, 30);
    int gap = S(hWnd, 10);
    int rowGap = S(hWnd, 14);
    int portW = S(hWnd, 110);
    int directionW = std::min(w, S(hWnd, 180));
    int depthW = std::min(w, S(hWnd, 190));

    OverlayCustom_AddItem(st, 1, OverlayCustomKind::Label, RECT{ x, y, x + w, y + labelH }, L"Input overlay");
    y += labelH + rowGap;

    // Primary actions come first, in reading/tab order as well as on screen.
    const bool running = OverlayServer_IsRunning();
    const std::wstring serverError = OverlayServer_GetLastError();
    OverlayCustom_AddItem(st, 12, OverlayCustomKind::Status,
        RECT{ x, y, x + w, y + valueH }, running ? L"Server running" : L"Server stopped");
    st->items.back().value = running ? 1 : 0;
    y += valueH + gap;
    const int actionIds[] = { OVERLAY_ID_TOGGLE, OVERLAY_ID_OPEN };
    const wchar_t* actionLabels[] = { running ? L"Stop server" : L"Start server", L"Open overlay" };
    int actionX = x;
    for (int i = 0; i < 2; ++i)
    {
        if (actionX + btnW > x + w) { actionX = x; y += btnH + gap; }
        OverlayCustom_AddItem(st, actionIds[i], OverlayCustomKind::Button,
            RECT{ actionX, y, actionX + btnW, y + btnH }, actionLabels[i]);
        st->items.back().enabled = i != 1 || running;
        actionX += btnW + gap;
    }
    const int addressMinWidth = S(hWnd, 350);
    if (x + w - actionX < addressMinWidth)
    {
        actionX = x;
        y += btnH + gap;
    }
    OverlayCustom_AddItem(st, 10, OverlayCustomKind::CopyAddress,
        RECT{ actionX, y, x + w, y + S(hWnd, 32) }, OverlayCustom_BuildUrl(st));
    y += S(hWnd, 32) + gap;
    OverlayCustom_AddItem(st, 13, OverlayCustomKind::Hint,
        RECT{ x, y, x + w, y + S(hWnd, 40) }, L"In OBS, add a Browser Source and paste this URL.");
    y += S(hWnd, 40);
    if (!serverError.empty())
    {
        // Keep the complete server error available, including at narrow widths.
        HDC measure = GetDC(hWnd);
        HGDIOBJ previousFont = SelectObject(measure, GetStockObject(SYSTEM_FONT));
        RECT errorRect{ x, y, x + w, y };
        DrawTextW(measure, serverError.c_str(), -1, &errorRect, DT_CALCRECT | DT_WORDBREAK | DT_NOPREFIX);
        SelectObject(measure, previousFont);
        ReleaseDC(hWnd, measure);
        errorRect.bottom += S(hWnd, 12);
        OverlayCustom_AddItem(st, 11, OverlayCustomKind::Hint, errorRect, serverError);
        y = errorRect.bottom;
    }
    y += rowGap;

    OverlayCustom_AddItem(st, 30, OverlayCustomKind::Label,
        RECT{ x, y, x + w, y + labelH }, L"Layout");
    y += labelH + S(hWnd, 6);
    const bool followingLayout = st->layoutPicker.Following();
    const int brandW = followingLayout ? std::min(S(hWnd, 240), w) : std::min(S(hWnd, 160), (w - gap) / 2);
    const int variantW = st->layoutPicker.HasVariants() ? S(hWnd, 96) + gap : 0;
    OverlayCustom_AddItem(st, 31, OverlayCustomKind::Label,
        RECT{ x, y, x + brandW, y + labelH }, L"Brand");
    if (!followingLayout) OverlayCustom_AddItem(st, 32, OverlayCustomKind::Label,
        RECT{ x + brandW + gap, y, x + w - variantW, y + labelH }, L"Model");
    if (variantW) OverlayCustom_AddItem(st, 33, OverlayCustomKind::Label,
        RECT{ x + w - variantW + gap, y, x + w, y + labelH }, L"Variant");
    y += labelH + S(hWnd, 6);
    OverlayCustom_AddItem(st, OVERLAY_ID_BRAND, OverlayCustomKind::Combo,
        RECT{ x, y, x + brandW, y + editH }, L"");
    if (!followingLayout) OverlayCustom_AddItem(st, OVERLAY_ID_LAYOUT, OverlayCustomKind::Combo,
        RECT{ x + brandW + gap, y, x + w - variantW, y + editH }, L"");
    if (variantW) OverlayCustom_AddItem(st, OVERLAY_ID_VARIANT, OverlayCustomKind::Combo,
        RECT{ x + w - variantW + gap, y, x + w, y + editH }, L"");
    y += editH + gap;
    OverlayCustom_AddItem(st, OVERLAY_ID_LAYOUT_EDIT, OverlayCustomKind::Button,
        RECT{ x, y, x + btnW, y + editH }, L"Layout editor");
    y += editH + rowGap;

    bool compactTop = w < portW + gap + directionW + gap + depthW;
    OverlayCustom_AddItem(st, 2, OverlayCustomKind::Label, RECT{ x, y, x + portW, y + labelH }, L"Port");
    if (!compactTop)
    {
        OverlayCustom_AddItem(st, 3, OverlayCustomKind::Label, RECT{ x + portW + gap, y, x + portW + gap + directionW, y + labelH }, L"Fill direction");
        OverlayCustom_AddItem(st, 4, OverlayCustomKind::Label, RECT{ x + portW + gap + directionW + gap, y, x + portW + gap + directionW + gap + depthW, y + labelH }, L"Depth display");
        y += labelH + S(hWnd, 6);
        OverlayCustom_AddItem(st, OVERLAY_ID_PORT, OverlayCustomKind::Edit, RECT{ x, y, x + portW, y + editH }, st->portText);
        st->items.back().enabled = !OverlayServer_IsRunning();
        OverlayCustom_AddItem(st, OVERLAY_ID_DIRECTION, OverlayCustomKind::Combo, RECT{ x + portW + gap, y, x + portW + gap + directionW, y + editH },
            OverlayServer_GetFillDirection() == OverlayFillDirection::TopDown ? L"Top to bottom" : L"Bottom to top");
        OverlayCustom_AddItem(st, OVERLAY_ID_DEPTH_SOURCE, OverlayCustomKind::Combo, RECT{ x + portW + gap + directionW + gap, y, x + portW + gap + directionW + gap + depthW, y + editH },
            OverlayServer_GetUseRawDepth() ? L"Raw press depth" : L"After curves");
        y += editH + rowGap;
    }
    else
    {
        y += labelH + S(hWnd, 6);
        OverlayCustom_AddItem(st, OVERLAY_ID_PORT, OverlayCustomKind::Edit, RECT{ x, y, x + portW, y + editH }, st->portText);
        st->items.back().enabled = !OverlayServer_IsRunning();
        y += editH + S(hWnd, 8);
        OverlayCustom_AddItem(st, 3, OverlayCustomKind::Label, RECT{ x, y, x + directionW, y + labelH }, L"Fill direction");
        y += labelH + S(hWnd, 6);
        OverlayCustom_AddItem(st, OVERLAY_ID_DIRECTION, OverlayCustomKind::Combo, RECT{ x, y, x + directionW, y + editH },
            OverlayServer_GetFillDirection() == OverlayFillDirection::TopDown ? L"Top to bottom" : L"Bottom to top");
        y += editH + S(hWnd, 8);
        OverlayCustom_AddItem(st, 4, OverlayCustomKind::Label, RECT{ x, y, x + depthW, y + labelH }, L"Depth display");
        y += labelH + S(hWnd, 6);
        OverlayCustom_AddItem(st, OVERLAY_ID_DEPTH_SOURCE, OverlayCustomKind::Combo, RECT{ x, y, x + depthW, y + editH },
            OverlayServer_GetUseRawDepth() ? L"Raw press depth" : L"After curves");
        y += editH + rowGap;
    }

    OverlayCustom_AddItem(st, 5, OverlayCustomKind::Label, RECT{ x, y, x + w, y + labelH }, L"Visual effects");
    y += labelH + S(hWnd, 6);
    OverlayCustom_AddStrengthRow(hWnd, st, y, x, w, L"Smooth response", OVERLAY_ID_EFFECT_SMOOTHING, OVERLAY_ID_STRENGTH_SMOOTHING, OverlayEffect_Smoothing);
    OverlayCustom_AddStrengthRow(hWnd, st, y, x, w, L"Glass keys", OVERLAY_ID_EFFECT_GLASS, OVERLAY_ID_STRENGTH_GLASS, OverlayEffect_Glass);
    OverlayCustom_AddStrengthRow(hWnd, st, y, x, w, L"Bloom", OVERLAY_ID_EFFECT_BLOOM, OVERLAY_ID_STRENGTH_BLOOM, OverlayEffect_Bloom);
    OverlayCustom_AddStrengthRow(hWnd, st, y, x, w, L"Edge sweep", OVERLAY_ID_EFFECT_EDGE, OVERLAY_ID_STRENGTH_EDGE, OverlayEffect_EdgeSweep);
    OverlayCustom_AddStrengthRow(hWnd, st, y, x, w, L"Micro-scale", OVERLAY_ID_EFFECT_SCALE, OVERLAY_ID_STRENGTH_SCALE, OverlayEffect_MicroScale);
    OverlayCustom_AddStrengthRow(hWnd, st, y, x, w, L"Label contrast", OVERLAY_ID_EFFECT_LABEL, OVERLAY_ID_STRENGTH_LABEL, OverlayEffect_LabelContrast);
    OverlayCustom_AddStrengthRow(hWnd, st, y, x, w, L"Rim lighting", OVERLAY_ID_EFFECT_RIM_LIGHT, OVERLAY_ID_STRENGTH_RIM_LIGHT, OverlayEffect_GlassRimLight);
    y += rowGap;

    int sliderH = S(hWnd, 28);
    int preview = S(hWnd, 34);
    int hexW = S(hWnd, 110);
    int hexLabelW = S(hWnd, 34);

    OverlayCustom_AddItem(st, 14, OverlayCustomKind::Label, RECT{ x, y, x + w, y + labelH }, L"Label style");
    y += labelH + S(hWnd, 6);
    int styleBtnW = std::min(S(hWnd, 180), w);
    int styleChipW = S(hWnd, 74);
    int styleSliderW = std::max(S(hWnd, 140), w - styleBtnW - styleChipW - gap * 2);
    OverlayCustom_AddItem(st, OVERLAY_ID_LABEL_FONT, OverlayCustomKind::Combo, RECT{ x, y, x + styleBtnW, y + editH }, OverlayCustom_LabelFontName(OverlayServer_GetLabelFontIndex()));
    y += editH + S(hWnd, 8);

    OverlayCustom_AddItem(st, 15, OverlayCustomKind::Label, RECT{ x, y, x + styleBtnW, y + sliderH }, L"Size");
    OverlayCustom_AddItem(st, OVERLAY_ID_LABEL_SIZE, OverlayCustomKind::Slider, RECT{ x + styleBtnW + gap, y, x + styleBtnW + gap + styleSliderW, y + sliderH });
    st->items.back().minV = 8;
    st->items.back().maxV = 32;
    st->items.back().value = OverlayServer_GetLabelSizePx();
    wchar_t labelSizeText[32]{};
    swprintf_s(labelSizeText, L"%d px", OverlayServer_GetLabelSizePx());
    OverlayCustom_AddItem(st, OVERLAY_ID_LABEL_SIZE + 100, OverlayCustomKind::Chip, RECT{ x + styleBtnW + gap + styleSliderW + gap, y - S(hWnd, 1), x + styleBtnW + gap + styleSliderW + gap + styleChipW, y + sliderH + S(hWnd, 1) }, labelSizeText);
    y += sliderH + S(hWnd, 6);

    OverlayCustom_AddItem(st, 16, OverlayCustomKind::Label, RECT{ x, y, x + styleBtnW, y + sliderH }, L"Shadow");
    OverlayCustom_AddItem(st, OVERLAY_ID_LABEL_SHADOW, OverlayCustomKind::Slider, RECT{ x + styleBtnW + gap, y, x + styleBtnW + gap + styleSliderW, y + sliderH });
    st->items.back().minV = 0;
    st->items.back().maxV = 100;
    st->items.back().value = OverlayServer_GetLabelShadowPercent();
    wchar_t labelShadowText[32]{};
    swprintf_s(labelShadowText, L"%d%%", OverlayServer_GetLabelShadowPercent());
    OverlayCustom_AddItem(st, OVERLAY_ID_LABEL_SHADOW + 100, OverlayCustomKind::Chip, RECT{ x + styleBtnW + gap + styleSliderW + gap, y - S(hWnd, 1), x + styleBtnW + gap + styleSliderW + gap + styleChipW, y + sliderH + S(hWnd, 1) }, labelShadowText);
    y += sliderH + S(hWnd, 8);

    OverlayCustom_AddItem(st, 17, OverlayCustomKind::Label, RECT{ x, y, x + w, y + labelH }, L"Label color");
    y += labelH + S(hWnd, 6);
    int paletteH = S(hWnd, 96);
    int labelHueW = std::max(S(hWnd, 180), w - preview - hexLabelW - hexW - gap * 3);
    if (w >= preview + gap + S(hWnd, 180) + gap + hexLabelW + hexW)
    {
        OverlayCustom_AddItem(st, OVERLAY_ID_LABEL_COLOR_PREVIEW, OverlayCustomKind::ColorPreview, RECT{ x, y, x + preview, y + preview });
        OverlayCustom_AddItem(st, OVERLAY_ID_LABEL_COLOR_HUE, OverlayCustomKind::Hue, RECT{ x + preview + gap, y, x + preview + gap + labelHueW, y + paletteH });
        OverlayCustom_AddItem(st, 18, OverlayCustomKind::Label, RECT{ x + preview + gap + labelHueW + gap, y, x + preview + gap + labelHueW + gap + hexLabelW, y + editH }, L"HEX");
        OverlayCustom_AddItem(st, OVERLAY_ID_LABEL_COLOR_HEX, OverlayCustomKind::Edit, RECT{ x + preview + gap + labelHueW + gap + hexLabelW, y, x + preview + gap + labelHueW + gap + hexLabelW + hexW, y + editH }, st->labelHexText);
        y += paletteH + rowGap;
    }
    else
    {
        OverlayCustom_AddItem(st, OVERLAY_ID_LABEL_COLOR_PREVIEW, OverlayCustomKind::ColorPreview, RECT{ x, y, x + preview, y + preview });
        OverlayCustom_AddItem(st, OVERLAY_ID_LABEL_COLOR_HUE, OverlayCustomKind::Hue, RECT{ x + preview + gap, y, x + w, y + paletteH });
        y += paletteH + S(hWnd, 8);
        OverlayCustom_AddItem(st, 18, OverlayCustomKind::Label, RECT{ x, y, x + hexLabelW, y + editH }, L"HEX");
        OverlayCustom_AddItem(st, OVERLAY_ID_LABEL_COLOR_HEX, OverlayCustomKind::Edit, RECT{ x + hexLabelW, y, x + hexLabelW + hexW, y + editH }, st->labelHexText);
        y += editH + rowGap;
    }

    int sliderChipW = S(hWnd, 86);
    OverlayCustom_AddItem(st, 6, OverlayCustomKind::Label, RECT{ x, y, x + w, y + labelH }, L"Overlay refresh interval");
    y += labelH + S(hWnd, 4);
    int refreshSliderW = std::max(S(hWnd, 160), w - sliderChipW - gap);
    OverlayCustom_AddItem(st, OVERLAY_ID_REFRESH_MS, OverlayCustomKind::Slider, RECT{ x, y, x + refreshSliderW, y + sliderH });
    st->items.back().minV = 1;
    st->items.back().maxV = 100;
    st->items.back().value = OverlayServer_GetRefreshIntervalMs();
    wchar_t refreshText[32]{};
    swprintf_s(refreshText, L"%d ms", OverlayServer_GetRefreshIntervalMs());
    OverlayCustom_AddItem(st, OVERLAY_ID_REFRESH_MS + 100, OverlayCustomKind::Chip, RECT{ x + refreshSliderW + gap, y - S(hWnd, 1), x + refreshSliderW + gap + sliderChipW, y + sliderH + S(hWnd, 1) }, refreshText);
    y += sliderH + rowGap;

    OverlayCustom_AddItem(st, 7, OverlayCustomKind::Label, RECT{ x, y, x + w, y + labelH }, L"Indicator color");
    y += labelH + S(hWnd, 6);
    int hueW = std::max(S(hWnd, 180), w - preview - hexLabelW - hexW - gap * 3);
    if (w >= preview + gap + S(hWnd, 180) + gap + hexLabelW + hexW)
    {
        OverlayCustom_AddItem(st, OVERLAY_ID_COLOR_PREVIEW, OverlayCustomKind::ColorPreview, RECT{ x, y, x + preview, y + preview });
        OverlayCustom_AddItem(st, OVERLAY_ID_COLOR_HUE, OverlayCustomKind::Hue, RECT{ x + preview + gap, y, x + preview + gap + hueW, y + paletteH });
        OverlayCustom_AddItem(st, 8, OverlayCustomKind::Label, RECT{ x + preview + gap + hueW + gap, y, x + preview + gap + hueW + gap + hexLabelW, y + editH }, L"HEX");
        OverlayCustom_AddItem(st, OVERLAY_ID_COLOR_HEX, OverlayCustomKind::Edit, RECT{ x + preview + gap + hueW + gap + hexLabelW, y, x + preview + gap + hueW + gap + hexLabelW + hexW, y + editH }, st->hexText);
        y += paletteH + rowGap;
    }
    else
    {
        OverlayCustom_AddItem(st, OVERLAY_ID_COLOR_PREVIEW, OverlayCustomKind::ColorPreview, RECT{ x, y, x + preview, y + preview });
        OverlayCustom_AddItem(st, OVERLAY_ID_COLOR_HUE, OverlayCustomKind::Hue, RECT{ x + preview + gap, y, x + w, y + paletteH });
        y += paletteH + S(hWnd, 8);
        OverlayCustom_AddItem(st, 8, OverlayCustomKind::Label, RECT{ x, y, x + hexLabelW, y + editH }, L"HEX");
        OverlayCustom_AddItem(st, OVERLAY_ID_COLOR_HEX, OverlayCustomKind::Edit, RECT{ x + hexLabelW, y, x + hexLabelW + hexW, y + editH }, st->hexText);
        y += editH + rowGap;
    }

    y += margin;

    st->contentHeight = y;
    st->surface.scrollY = st->scrollY;
    CustomPageSurface_SetContentHeight(hWnd, &st->surface, y);
    st->scrollY = st->surface.scrollY;
}

static OverlayCustomItem* OverlayCustom_HitTest(OverlayCustomState* st, POINT pt)
{
    if (!st) return nullptr;
    pt.y += st->scrollY;
    for (auto it = st->items.rbegin(); it != st->items.rend(); ++it)
    {
        if (PtInRect(&it->rc, pt))
            return &(*it);
    }
    return nullptr;
}

static RECT OverlayCustom_ToView(const RECT& rc, int scrollY)
{
    RECT r = rc;
    OffsetRect(&r, 0, -scrollY);
    return r;
}

static void OverlayCustom_DrawText(HDC hdc, const std::wstring& text, RECT rc, COLORREF color, UINT fmt)
{
    CustomPage_DrawText(hdc, text, rc, color, fmt);
}

static void OverlayCustom_DrawRoundRect(Graphics& g, const RECT& rc, COLORREF fill, COLORREF border, float radius, BYTE alpha = 255)
{
    CustomPage_DrawRoundRect(g, rc, fill, border, radius, alpha);
}

static void OverlayCustom_DrawSlider(HWND hWnd, Graphics& g, const OverlayCustomItem& it, const RECT& rc)
{
    CustomPage_DrawSlider(g, hWnd, rc, it.minV, it.maxV, it.value);
}

static void OverlayCustom_ColorRects(const RECT& rc, RECT* svOut, RECT* hueOut)
{
    RECT area = rc;
    InflateRect(&area, -1, -1);
    int gap = 6;
    int hueW = std::clamp((int)((area.bottom - area.top) * 0.16), 12, 18);
    if (svOut)
        *svOut = RECT{ area.left, area.top, area.right - hueW - gap, area.bottom };
    if (hueOut)
        *hueOut = RECT{ area.right - hueW, area.top, area.right, area.bottom };
}

static OverlayColorUiState& OverlayCustom_ColorUi(OverlayCustomState* st, int itemId)
{
    return (itemId == OVERLAY_ID_LABEL_COLOR_HUE) ? st->labelColorUi : st->indicatorColorUi;
}

static void OverlayCustom_SyncColorUiFromRgb(OverlayColorUiState& ui, uint32_t rgb)
{
    double hue = 0.0, sat = 0.0, val = 0.0;
    OverlayPage_HsvFromRgb(rgb, hue, sat, val);
    if (!ui.initialized || sat > 0.001)
        ui.hue = hue;
    ui.sat = sat;
    ui.val = val;
    ui.initialized = true;
}

static HBITMAP OverlayCustom_GetPaletteBitmap(HDC hdc, OverlayColorBitmapCache& cache, int width, int height, double hue)
{
    int bucket = std::clamp((int)std::lround(hue), 0, 360);
    width = std::max(1, width);
    height = std::max(1, height);
    if (cache.svBitmap && cache.svW == width && cache.svH == height && cache.hueBucket == bucket)
        return cache.svBitmap;
    if (cache.svBitmap)
    {
        DeleteObject(cache.svBitmap);
        cache.svBitmap = nullptr;
    }
    BITMAPINFO bi{};
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = width;
    bi.bmiHeader.biHeight = -height;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    void* bits = nullptr;
    cache.svBitmap = CreateDIBSection(hdc, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!cache.svBitmap || !bits)
        return nullptr;
    uint32_t* px = (uint32_t*)bits;
    int satDen = std::max(1, width - 1);
    int valDen = std::max(1, height - 1);
    for (int y = 0; y < height; ++y)
    {
        double v = 1.0 - (double)y / (double)valDen;
        for (int x = 0; x < width; ++x)
        {
            double s = (double)x / (double)satDen;
            uint32_t rgb = OverlayPage_RgbFromHsv((double)bucket, s, v);
            px[y * width + x] = 0xff000000u | rgb;
        }
    }
    cache.svW = width;
    cache.svH = height;
    cache.hueBucket = bucket;
    return cache.svBitmap;
}

static HBITMAP OverlayCustom_GetHueBitmap(HDC hdc, OverlayColorBitmapCache& cache, int width, int height)
{
    width = std::max(1, width);
    height = std::max(1, height);
    if (cache.hueBitmap && cache.hueW == width && cache.hueH == height)
        return cache.hueBitmap;
    if (cache.hueBitmap)
    {
        DeleteObject(cache.hueBitmap);
        cache.hueBitmap = nullptr;
    }
    BITMAPINFO bi{};
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = width;
    bi.bmiHeader.biHeight = -height;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    void* bits = nullptr;
    cache.hueBitmap = CreateDIBSection(hdc, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!cache.hueBitmap || !bits)
        return nullptr;
    uint32_t* px = (uint32_t*)bits;
    int hueDen = std::max(1, height - 1);
    for (int y = 0; y < height; ++y)
    {
        double h = (double)y / (double)hueDen * 360.0;
        uint32_t rgb = OverlayPage_RgbFromHue(h);
        uint32_t bgra = 0xff000000u | rgb;
        for (int x = 0; x < width; ++x)
            px[y * width + x] = bgra;
    }
    cache.hueW = width;
    cache.hueH = height;
    return cache.hueBitmap;
}

static void OverlayCustom_DrawHue(OverlayCustomState* st, Graphics& g, const RECT& rc, int itemId, uint32_t color)
{
    RectF outer((REAL)rc.left, (REAL)rc.top, (REAL)(rc.right - rc.left), (REAL)(rc.bottom - rc.top));
    SolidBrush bg(Gp(UiTheme::Color_PanelBg()));
    g.FillRectangle(&bg, outer);

    RECT sv{}, hueBar{};
    OverlayCustom_ColorRects(rc, &sv, &hueBar);
    int width = std::max(1, (int)(sv.right - sv.left));
    int height = std::max(1, (int)(sv.bottom - sv.top));

    OverlayColorBitmapCache fallbackCache;
    OverlayColorBitmapCache& cache = st
        ? ((itemId == OVERLAY_ID_LABEL_COLOR_HUE) ? st->labelColorCache : st->indicatorColorCache)
        : fallbackCache;
    OverlayColorUiState fallbackUi;
    OverlayColorUiState& ui = st ? OverlayCustom_ColorUi(st, itemId) : fallbackUi;

    double colorHue = 0.0, colorSat = 0.0, colorVal = 0.0;
    OverlayPage_HsvFromRgb(color, colorHue, colorSat, colorVal);
    if (!ui.initialized)
        OverlayCustom_SyncColorUiFromRgb(ui, color);
    else if (colorSat > 0.001 && colorVal > 0.001)
    {
        ui.hue = colorHue;
        ui.sat = colorSat;
        ui.val = colorVal;
    }
    else
    {
        ui.val = colorVal;
        if (colorSat > 0.001)
            ui.sat = colorSat;
    }

    double hue = ui.hue;
    double sat = (colorVal <= 0.001) ? ui.sat : colorSat;
    double val = colorVal;

    HDC hdc = g.GetHDC();
    HDC src = CreateCompatibleDC(hdc);
    HBITMAP palette = OverlayCustom_GetPaletteBitmap(hdc, cache, width, height, hue);
    if (src && palette)
    {
        HGDIOBJ old = SelectObject(src, palette);
        BitBlt(hdc, sv.left, sv.top, width, height, src, 0, 0, SRCCOPY);
        SelectObject(src, old);
    }
    if (src)
        DeleteDC(src);
    g.ReleaseHDC(hdc);
    hdc = nullptr;

    Pen border(Gp(UiTheme::Color_Border()), 1.0f);
    g.DrawRectangle(&border, (INT)sv.left, (INT)sv.top, (INT)(sv.right - sv.left), (INT)(sv.bottom - sv.top));

    int hueH = std::max(1, (int)(hueBar.bottom - hueBar.top));
    hdc = g.GetHDC();
    src = CreateCompatibleDC(hdc);
    HBITMAP hueBmp = OverlayCustom_GetHueBitmap(hdc, cache, hueBar.right - hueBar.left, hueH);
    if (src && hueBmp)
    {
        HGDIOBJ old = SelectObject(src, hueBmp);
        BitBlt(hdc, hueBar.left, hueBar.top, hueBar.right - hueBar.left, hueH, src, 0, 0, SRCCOPY);
        SelectObject(src, old);
    }
    if (src)
        DeleteDC(src);
    g.ReleaseHDC(hdc);

    g.DrawRectangle(&border, (INT)hueBar.left, (INT)hueBar.top, (INT)(hueBar.right - hueBar.left), (INT)(hueBar.bottom - hueBar.top));

    int markerX = sv.left + (int)std::lround(sat * (double)(width - 1));
    int markerY = sv.top + (int)std::lround((1.0 - val) * (double)(height - 1));
    Pen markerDark(Gp(RGB(0, 0, 0)), 3.0f);
    Pen markerLight(Gp(RGB(255, 255, 255)), 1.6f);
    g.DrawEllipse(&markerDark, markerX - 5, markerY - 5, 10, 10);
    g.DrawEllipse(&markerLight, markerX - 5, markerY - 5, 10, 10);

    int hueY = hueBar.top + (int)std::lround((hue / 360.0) * (double)(hueH - 1));
    Pen huePen(Gp(RGB(255, 255, 255)), 2.0f);
    g.DrawLine(&markerDark, hueBar.left - 2, hueY, hueBar.right + 2, hueY);
    g.DrawLine(&huePen, hueBar.left - 1, hueY, hueBar.right + 1, hueY);
    if (!st)
        OverlayCustom_DestroyColorCache(fallbackCache);
}

static void OverlayCustom_DrawItem(HWND hWnd, HDC hdc, Graphics& g, OverlayCustomState* st, const OverlayCustomItem& it, int scrollY, const RECT& clipClient)
{
    RECT rc = OverlayCustom_ToView(it.rc, scrollY);
    RECT clip{};
    if (!IntersectRect(&clip, &rc, &clipClient))
        return;

    bool hot = st && st->hotId == it.id && it.enabled;
    bool pressed = st && st->pressedId == it.id && it.enabled;
    bool focused = st && st->focusId == it.id;
    COLORREF text = it.enabled ? UiTheme::Color_Text() : UiTheme::Color_TextMuted();

    switch (it.kind)
    {
    case OverlayCustomKind::CopyAddress:
    {
        OverlayCustom_DrawRoundRect(g, rc, hot ? RGB(43, 48, 53) : UiTheme::Color_ControlBg(),
            hot ? UiTheme::Color_Accent() : UiTheme::Color_Border(), (float)S(hWnd, 4));
        RECT url = rc;
        InflateRect(&url, -S(hWnd, 12), 0);
        RECT action = url;
        action.left = (std::max)(url.left, url.right - S(hWnd, 118));
        url.right = action.left - S(hWnd, 10);
        OverlayCustom_DrawText(hdc, it.text, url, text, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        OverlayCustom_DrawText(hdc, st->copyFeedback.empty() ? L"Click to copy" : st->copyFeedback,
            action, UiTheme::Color_Accent(), DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
        break;
    }
    case OverlayCustomKind::Status:
    {
        const COLORREF accent = it.value ? RGB(105, 199, 146) : UiTheme::Color_TextMuted();
        SolidBrush dot(Gp(accent));
        const int diameter = S(hWnd, 7);
        g.FillEllipse(&dot, (INT)rc.left, (INT)(rc.top + (rc.bottom - rc.top - diameter) / 2), diameter, diameter);
        rc.left += S(hWnd, 17);
        OverlayCustom_DrawText(hdc, it.text, rc, accent, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        break;
    }
    case OverlayCustomKind::Label:
        OverlayCustom_DrawText(hdc, it.text, rc, text, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        break;
    case OverlayCustomKind::Hint:
        OverlayCustom_DrawText(hdc, it.text, rc, UiTheme::Color_TextMuted(), DT_LEFT | DT_TOP | DT_WORDBREAK | DT_END_ELLIPSIS);
        break;
    case OverlayCustomKind::Button:
        CustomPage_DrawButton(g, hdc, rc, it.text, hot, pressed, it.enabled);
        break;
    case OverlayCustomKind::Combo:
    {
        HWND combo = nullptr;
        if (st)
        {
            if (it.id == OVERLAY_ID_DIRECTION) combo = st->comboDirection;
            else if (it.id == OVERLAY_ID_DEPTH_SOURCE) combo = st->comboDepthSource;
            else if (it.id == OVERLAY_ID_LABEL_FONT) combo = st->comboLabelFont;
            else if (it.id == OVERLAY_ID_LAYOUT) combo = st->comboLayout;
            else if (it.id == OVERLAY_ID_BRAND) combo = st->layoutPicker.brand;
            else if (it.id == OVERLAY_ID_VARIANT) combo = st->layoutPicker.variant;
        }
        if (combo)
            PremiumCombo::PaintRetainedFace(combo, hdc, rc, hot);
        break;
    }
    case OverlayCustomKind::Checkbox:
    {
        bool checked = OverlayServer_GetEffectEnabled(it.flag);
        CustomPage_DrawCheckbox(g, hdc, hWnd, rc, it.text, checked, it.enabled);
        break;
    }
    case OverlayCustomKind::Slider:
        OverlayCustom_DrawSlider(hWnd, g, it, rc);
        break;
    case OverlayCustomKind::Chip:
        CustomPage_DrawChip(g, hdc, rc, it.text, it.enabled);
        break;
    case OverlayCustomKind::Edit:
        {
            const std::wstring& editText = st ? OverlayCustom_EditText(st, it.id) : it.text;
            uint32_t value = 0;
            const bool valid = halljoy::overlay_edit::Value(editText, OverlayCustom_EditKind(it.id), value);
            OverlayCustom_DrawRoundRect(g, rc, UiTheme::Color_ControlBg(), !valid ? RGB(222, 104, 114) :
                focused ? UiTheme::Color_Accent() : UiTheme::Color_Border(), 4.0f, it.enabled ? 255 : 145);
            RECT trc = rc;
            InflateRect(&trc, -S(hWnd, 8), 0);
            const int dcState = SaveDC(hdc);
            IntersectClipRect(hdc, trc.left, trc.top, trc.right, trc.bottom);
            const int caretWidth = focused ? OverlayCustom_TextWidth(hdc, editText, st->editor.Caret()) : 0;
            const int offset = focused ? std::max<int>(0, caretWidth - (trc.right - trc.left) + 2) : 0;
            RECT textRect = trc;
            textRect.left -= offset;
            textRect.right = textRect.left + std::max<int>(OverlayCustom_TextWidth(hdc, editText, editText.size()) + 4, trc.right - trc.left);
            if (focused && st->editor.Selected()) {
                RECT selection{ textRect.left + OverlayCustom_TextWidth(hdc, editText, st->editor.Begin()), rc.top + S(hWnd, 4),
                    textRect.left + OverlayCustom_TextWidth(hdc, editText, st->editor.End()), rc.bottom - S(hWnd, 4) };
                HBRUSH brush = CreateSolidBrush(RGB(48, 100, 150));
                FillRect(hdc, &selection, brush);
                DeleteObject(brush);
            }
            OverlayCustom_DrawText(hdc, editText, textRect, text, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
            if (focused && it.enabled)
            {
                int cx = textRect.left + caretWidth;
                HPEN caret = CreatePen(PS_SOLID, 1, UiTheme::Color_Text());
                HGDIOBJ old = SelectObject(hdc, caret);
                MoveToEx(hdc, cx, rc.top + S(hWnd, 6), nullptr);
                LineTo(hdc, cx, rc.bottom - S(hWnd, 6));
                SelectObject(hdc, old);
                DeleteObject(caret);
            }
            RestoreDC(hdc, dcState);
        }
        break;
    case OverlayCustomKind::Hue:
        OverlayCustom_DrawHue(st, g, rc, it.id, it.id == OVERLAY_ID_LABEL_COLOR_HUE ? OverlayServer_GetLabelColor() : OverlayServer_GetAccentColor());
        break;
    case OverlayCustomKind::ColorPreview:
    {
        RECT sw = rc;
        InflateRect(&sw, -2, -2);
        uint32_t color = (it.id == OVERLAY_ID_LABEL_COLOR_PREVIEW) ? OverlayServer_GetLabelColor() : OverlayServer_GetAccentColor();
        OverlayCustom_DrawRoundRect(g, sw, OverlayPage_ColorRefFromRgb(color), UiTheme::Color_Border(), 2.0f);
        break;
    }
    }
}

static void OverlayCustom_DrawItem(HWND hWnd, HDC hdc, Graphics& g, OverlayCustomState* st, const OverlayCustomItem& it)
{
    RECT client{};
    GetClientRect(hWnd, &client);
    OverlayCustom_DrawItem(hWnd, hdc, g, st, it, st ? st->scrollY : 0, client);
}

static void OverlayCustom_RenderCacheContent(HWND hWnd, HDC hdc, const RECT& full, void* user)
{
    auto* st = (OverlayCustomState*)user;
    if (!st) return;

    // Match the shared retained Global/Configuration page typography.
    HGDIOBJ oldFont = SelectObject(hdc, GetStockObject(SYSTEM_FONT));
    Graphics g(hdc);
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    g.SetPixelOffsetMode(PixelOffsetModeHighQuality);
    g.SetTextRenderingHint(TextRenderingHintClearTypeGridFit);
    for (const auto& it : st->items)
        OverlayCustom_DrawItem(hWnd, hdc, g, st, it, 0, full);
    SelectObject(hdc, oldFont);
}

static bool OverlayCustom_RenderContentCache(HWND hWnd, HDC targetDC, OverlayCustomState* st)
{
    if (!st) return false;
    st->surface.contentHeight = st->contentHeight;
    st->surface.scrollY = st->scrollY;
    return CustomPageSurface_RenderCache(hWnd, targetDC, &st->surface, OverlayCustom_RenderCacheContent, st);
}

static void OverlayCustom_DrawScrollbar(HWND hWnd, HDC hdc, Graphics& g, OverlayCustomState* st)
{
    (void)g;
    if (!st) return;
    st->surface.contentHeight = st->contentHeight;
    st->surface.scrollY = st->scrollY;
    CustomPageSurface_DrawScrollbar(hWnd, hdc, &st->surface, st->scroll.draggingThumb);
}

static void OverlayCustom_SetSliderFromPoint(HWND hWnd, OverlayCustomState* st, OverlayCustomItem* it, int x)
{
    if (!st || !it) return;
    RECT rc = it->rc;
    int h = rc.bottom - rc.top;
    int pad = std::clamp(h / 3, 8, 14);
    int left = rc.left + pad;
    int right = rc.right - pad;
    int contentX = x;
    double t = (double)(contentX - left) / (double)std::max(1, right - left);
    int v = it->minV + (int)std::lround(std::clamp(t, 0.0, 1.0) * (double)(it->maxV - it->minV));
    if (it->id == OVERLAY_ID_REFRESH_MS)
        OverlayServer_SetRefreshIntervalMs(v);
    else if (it->id == OVERLAY_ID_LABEL_SIZE)
        OverlayServer_SetLabelSizePx(v);
    else if (it->id == OVERLAY_ID_LABEL_SHADOW)
        OverlayServer_SetLabelShadowPercent(v);
    else
        OverlayServer_SetEffectStrengthPercent(it->flag, v);
    OverlayCustom_RebuildLayout(hWnd, st);
    OverlayCustom_RequestSave(hWnd);
    InvalidateRect(hWnd, nullptr, FALSE);
}

static int OverlayCustom_ColorDragModeFromPoint(const OverlayCustomItem* it, POINT contentPt)
{
    if (!it) return 0;
    RECT sv{}, hueBar{};
    OverlayCustom_ColorRects(it->rc, &sv, &hueBar);
    return (contentPt.x >= hueBar.left) ? 2 : 1;
}

static void OverlayCustom_SetHueFromPoint(HWND hWnd, OverlayCustomState* st, OverlayCustomItem* it, POINT contentPt, int dragMode)
{
    if (!st || !it) return;
    RECT sv{}, hueBar{};
    OverlayCustom_ColorRects(it->rc, &sv, &hueBar);

    uint32_t oldRgb = (it->id == OVERLAY_ID_LABEL_COLOR_HUE) ? OverlayServer_GetLabelColor() : OverlayServer_GetAccentColor();
    OverlayColorUiState& ui = OverlayCustom_ColorUi(st, it->id);
    if (!ui.initialized)
        OverlayCustom_SyncColorUiFromRgb(ui, oldRgb);
    double oldHue = ui.hue;
    double oldSat = ui.sat;
    double oldVal = ui.val;
    double hue = ui.hue;
    double sat = ui.sat;
    double val = ui.val;

    if (dragMode == 0)
        dragMode = OverlayCustom_ColorDragModeFromPoint(it, contentPt);

    if (dragMode == 2)
    {
        int hueSpan = std::max(1, (int)(hueBar.bottom - hueBar.top - 1));
        int y = (int)contentPt.y;
        hue = 360.0 * (double)std::clamp(y - (int)hueBar.top, 0, hueSpan) / (double)hueSpan;
        if (sat <= 0.001)
            sat = 1.0;
        if (val <= 0.001)
            val = 1.0;
    }
    else
    {
        int satSpan = std::max(1, (int)(sv.right - sv.left - 1));
        int valSpan = std::max(1, (int)(sv.bottom - sv.top - 1));
        int x = (int)contentPt.x;
        int y = (int)contentPt.y;
        sat = (double)std::clamp(x - (int)sv.left, 0, satSpan) / (double)satSpan;
        val = 1.0 - (double)std::clamp(y - (int)sv.top, 0, valSpan) / (double)valSpan;
    }
    ui.hue = hue;
    ui.sat = sat;
    ui.val = val;
    uint32_t rgb = OverlayPage_RgbFromHsv(hue, sat, val);
    bool markerChanged =
        std::fabs(oldHue - ui.hue) > 0.001 ||
        std::fabs(oldSat - ui.sat) > 0.001 ||
        std::fabs(oldVal - ui.val) > 0.001;
    if (rgb == oldRgb && !markerChanged)
        return;
    if (it->id == OVERLAY_ID_LABEL_COLOR_HUE)
    {
        if (rgb != oldRgb)
        {
            OverlayServer_SetLabelColor(rgb);
            st->labelHexText = OverlayPage_FormatHex(OverlayServer_GetLabelColor());
        }
    }
    else
    {
        if (rgb != oldRgb)
        {
            OverlayServer_SetAccentColor(rgb);
            st->hexText = OverlayPage_FormatHex(OverlayServer_GetAccentColor());
        }
    }
    OverlayCustom_MarkCacheDirty(hWnd, st);
    if (rgb != oldRgb)
        OverlayCustom_RequestSave(hWnd);
    InvalidateRect(hWnd, nullptr, FALSE);
}

static HWND OverlayCustom_ComboForId(OverlayCustomState* st, int id)
{
    if (!st) return nullptr;
    if (id == OVERLAY_ID_DIRECTION) return st->comboDirection;
    if (id == OVERLAY_ID_DEPTH_SOURCE) return st->comboDepthSource;
    if (id == OVERLAY_ID_LABEL_FONT) return st->comboLabelFont;
    if (id == OVERLAY_ID_LAYOUT) return st->comboLayout;
    if (id == OVERLAY_ID_BRAND) return st->layoutPicker.brand;
    if (id == OVERLAY_ID_VARIANT) return st->layoutPicker.variant;
    return nullptr;
}

static void OverlayCustom_CloseComboAnchors(OverlayCustomState* st)
{
    if (!st) return;
    HWND combos[] = { st->comboDirection, st->comboDepthSource, st->comboLabelFont, st->comboLayout, st->layoutPicker.brand, st->layoutPicker.variant };
    for (HWND combo : combos)
    {
        if (!combo) continue;
        PremiumCombo::ShowDropDown(combo, false);
        ShowWindow(combo, SW_HIDE);
    }
}

static void OverlayCustom_RefreshLayoutCombo(OverlayCustomState* st)
{
    if (!st || !st->comboLayout) return;
    st->layoutPicker.Refresh(KeyboardLayout_GetOverlayPresetIndex());
}

static void OverlayCustom_OpenComboAnchor(HWND hWnd, OverlayCustomState* st, int id)
{
    if (id == OVERLAY_ID_LAYOUT || id == OVERLAY_ID_BRAND || id == OVERLAY_ID_VARIANT) OverlayCustom_RefreshLayoutCombo(st);
    HWND combo = OverlayCustom_ComboForId(st, id);
    if (!combo) return;
    const OverlayCustomItem* item = nullptr;
    for (const auto& candidate : st->items)
        if (candidate.id == id) { item = &candidate; break; }
    if (!item) return;

    OverlayCustom_CloseComboAnchors(st);
    RECT view = CustomPageSurface_ContentToClient(&st->surface, item->rc);
    SetWindowPos(combo, HWND_TOP, view.left, view.top,
        std::max(1L, view.right - view.left), std::max(1L, view.bottom - view.top),
        SWP_NOACTIVATE | SWP_SHOWWINDOW);
    SetFocus(combo);
    PremiumCombo::ShowDropDown(combo, true);
}

static void OverlayCustom_InitCombos(HWND hWnd, OverlayCustomState* st)
{
    if (!st) return;
    HINSTANCE hInst = (HINSTANCE)GetWindowLongPtrW(hWnd, GWLP_HINSTANCE);
    HFONT font = (HFONT)GetStockObject(SYSTEM_FONT);
    auto create = [&](int id)
    {
        HWND combo = PremiumCombo::Create(hWnd, hInst, 0, 0, 10, 10, id,
            WS_CHILD | WS_TABSTOP);
        PremiumCombo::SetFont(combo, font, false);
        PremiumCombo::SetDropMaxVisible(combo, 10);
        ShowWindow(combo, SW_HIDE);
        return combo;
    };

    st->comboDirection = create(OVERLAY_ID_DIRECTION);
    PremiumCombo::AddString(st->comboDirection, L"Top to bottom");
    PremiumCombo::AddString(st->comboDirection, L"Bottom to top");

    st->comboDepthSource = create(OVERLAY_ID_DEPTH_SOURCE);
    PremiumCombo::AddString(st->comboDepthSource, L"After curves");
    PremiumCombo::AddString(st->comboDepthSource, L"Raw press depth");

    st->comboLabelFont = create(OVERLAY_ID_LABEL_FONT);
    st->comboLayout = create(OVERLAY_ID_LAYOUT);
    st->layoutPicker.model = st->comboLayout;
    st->layoutPicker.brand = create(OVERLAY_ID_BRAND);
    st->layoutPicker.variant = create(OVERLAY_ID_VARIANT);
    st->layoutPicker.follow = true;
    OverlayCustom_RefreshLayoutCombo(st);
    for (int i = 0; i < 13; ++i)
        PremiumCombo::AddString(st->comboLabelFont, OverlayCustom_LabelFontName(i));

    PremiumCombo::SetCurSel(st->comboDirection,
        OverlayServer_GetFillDirection() == OverlayFillDirection::TopDown ? 0 : 1, false);
    PremiumCombo::SetCurSel(st->comboDepthSource, OverlayServer_GetUseRawDepth() ? 1 : 0, false);
    PremiumCombo::SetCurSel(st->comboLabelFont,
        std::clamp(OverlayServer_GetLabelFontIndex(), 0, 12), false);
}

static void OverlayCustom_Activate(HWND hWnd, OverlayCustomState* st, int id)
{
    if (!st) return;
    switch (id)
    {
    case OVERLAY_ID_TOGGLE:
        if (OverlayServer_IsRunning())
        {
            OverlayServer_Stop();
            OverlayServer_SetAutoStart(false);
        }
        else
        {
            uint32_t port = 0;
            if (!halljoy::overlay_edit::Value(st->portText, halljoy::overlay_edit::Kind::Port, port)) {
                st->focusId = OVERLAY_ID_PORT;
                st->editor.Reset(st->portText, halljoy::overlay_edit::Kind::Port);
                st->editor.SelectAll();
                st->editFeedback = L"Enter a port from 1 to 65535 before starting.";
                OverlayCustom_MarkCacheDirty(hWnd, st);
                return;
            }
            OverlayServer_Start((uint16_t)port);
            OverlayServer_SetAutoStart(OverlayServer_IsRunning());
        }
        break;
    case OVERLAY_ID_OPEN:
        if (OverlayServer_IsRunning())
        {
            std::wstring url = OverlayServer_GetUrl();
            ShellExecuteW(hWnd, L"open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
        }
        break;
    case OVERLAY_ID_COPY:
    case 10: // Address field uses exactly the same copy operation as Copy URL.
        st->copyFeedback = OverlayPage_SetClipboardText(hWnd, OverlayCustom_BuildUrl(st))
            ? L"Copied!" : L"Copy failed";
        SetTimer(hWnd, OVERLAY_ID_COPY, 2000, nullptr);
        OverlayCustom_MarkCacheDirty(hWnd, st);
        return; // Copying does not change settings or trigger an autosave.
    case OVERLAY_ID_DIRECTION:
    case OVERLAY_ID_DEPTH_SOURCE:
    case OVERLAY_ID_LABEL_FONT:
    case OVERLAY_ID_LAYOUT:
    case OVERLAY_ID_BRAND:
    case OVERLAY_ID_VARIANT:
        OverlayCustom_OpenComboAnchor(hWnd, st, id);
        return;
    case OVERLAY_ID_LAYOUT_EDIT:
        LayoutEditor_OpenWindow(hWnd, KeyboardLayout_GetOverlayPresetIndex());
        return;
    default:
    {
        uint32_t flag = 0;
        if (OverlayPage_EffectFromId(id, &flag))
            OverlayServer_SetEffectEnabled(flag, !OverlayServer_GetEffectEnabled(flag));
        break;
    }
    }
    OverlayCustom_RebuildLayout(hWnd, st);
    OverlayCustom_RequestSave(hWnd);
    InvalidateRect(hWnd, nullptr, FALSE);
}

static void OverlayCustom_ApplyEdit(HWND hWnd, OverlayCustomState* st)
{
    if (!st || !OverlayCustom_IsEdit(st->focusId)) return;
    OverlayCustom_EditText(st, st->focusId) = st->editor.Text();
    uint32_t value = 0;
    bool changed = false;
    if (halljoy::overlay_edit::Value(st->editor.Text(), st->editor.kind, value)) {
        if (st->focusId == OVERLAY_ID_PORT && !OverlayServer_IsRunning()) {
            changed = value != OverlayServer_GetConfiguredPort();
            if (changed) OverlayServer_SetConfiguredPort((uint16_t)value);
        } else if (st->focusId == OVERLAY_ID_LABEL_COLOR_HEX) {
            changed = value != OverlayServer_GetLabelColor();
            if (changed) OverlayServer_SetLabelColor(value);
            OverlayCustom_SyncColorUiFromRgb(st->labelColorUi, value);
        } else if (st->focusId == OVERLAY_ID_COLOR_HEX) {
            changed = value != OverlayServer_GetAccentColor();
            if (changed) OverlayServer_SetAccentColor(value);
            OverlayCustom_SyncColorUiFromRgb(st->indicatorColorUi, value);
        }
    }
    if (changed) {
        OverlayCustom_RebuildLayout(hWnd, st);
        OverlayCustom_RequestSave(hWnd);
    }
    OverlayCustom_MarkCacheDirty(hWnd, st);
}

static size_t OverlayCustom_EditHit(HWND hWnd, OverlayCustomState* st, const OverlayCustomItem& item, int x)
{
    HDC dc = GetDC(hWnd);
    HGDIOBJ font = SelectObject(dc, GetStockObject(SYSTEM_FONT));
    const auto& text = st->editor.Text();
    const int width = item.rc.right - item.rc.left - S(hWnd, 16);
    const int offset = std::max(0, OverlayCustom_TextWidth(dc, text, st->editor.Caret()) - width + 2);
    const int target = x - item.rc.left - S(hWnd, 8) + offset;
    size_t index = 0;
    for (; index < text.size(); ++index) {
        const int left = OverlayCustom_TextWidth(dc, text, index);
        const int right = OverlayCustom_TextWidth(dc, text, index + 1);
        if (target < (left + right) / 2) break;
    }
    SelectObject(dc, font); ReleaseDC(hWnd, dc);
    return index;
}

static bool OverlayCustom_Paste(HWND hWnd, OverlayCustomState* st)
{
    if (!OpenClipboard(hWnd)) { st->editFeedback = L"Clipboard is busy. Please try again."; return false; }
    HANDLE data = GetClipboardData(CF_UNICODETEXT);
    const size_t capacity = data ? GlobalSize(data) / sizeof(wchar_t) : 0;
    const auto* text = data ? (const wchar_t*)GlobalLock(data) : nullptr;
    std::wstring copied;
    bool bounded = false;
    if (text) {
        size_t length = 0;
        while (length < std::min<size_t>(capacity, 128) && text[length]) ++length;
        bounded = length < capacity && length < 128;
        if (bounded) copied.assign(text, length);
        GlobalUnlock(data);
    }
    CloseClipboard();
    if (!bounded || copied.empty() || !st->editor.Replace(copied, true)) {
        st->editFeedback = L"Paste a port number or a six-digit HEX color.";
        return false;
    }
    st->editFeedback.clear();
    return true;
}

enum { OVERLAY_EDIT_UNDO = 1, OVERLAY_EDIT_REDO, OVERLAY_EDIT_CUT, OVERLAY_EDIT_COPY, OVERLAY_EDIT_PASTE, OVERLAY_EDIT_ALL };
static void OverlayCustom_EditCommand(HWND hWnd, OverlayCustomState* st, int command)
{
    st->editFeedback.clear();
    switch (command) {
    case OVERLAY_EDIT_UNDO: st->editor.Undo(); break;
    case OVERLAY_EDIT_REDO: st->editor.Undo(true); break;
    case OVERLAY_EDIT_ALL: st->editor.SelectAll(); break;
    case OVERLAY_EDIT_COPY:
    case OVERLAY_EDIT_CUT:
        if (st->editor.Selected()) {
            if (OverlayPage_SetClipboardText(hWnd, st->editor.Selection())) {
                if (command == OVERLAY_EDIT_CUT) st->editor.Replace(L"");
            } else st->editFeedback = L"Could not copy. Please try again.";
        }
        break;
    case OVERLAY_EDIT_PASTE: OverlayCustom_Paste(hWnd, st); break;
    }
    OverlayCustom_ApplyEdit(hWnd, st);
}

static void OverlayCustom_DrawEditFeedback(HWND hWnd, HDC dc, OverlayCustomState* st)
{
    if (!st || !OverlayCustom_IsEdit(st->focusId) || GetFocus() != hWnd || st->selectingText) return;
    uint32_t value = 0;
    std::wstring message = st->editFeedback;
    if (message.empty() && !halljoy::overlay_edit::Value(st->editor.Text(), st->editor.kind, value))
        message = st->editor.kind == halljoy::overlay_edit::Kind::Port ? L"Enter a port from 1 to 65535." : L"Enter six HEX digits, for example #4A90D9.";
    if (message.empty()) return;
    for (const auto& item : st->items) if (item.id == st->focusId) {
        RECT client{}; GetClientRect(hWnd, &client);
        const RECT field = OverlayCustom_ToView(item.rc, st->scrollY);
        if (field.bottom <= 0 || field.top >= client.bottom) return;
        const int height = S(hWnd, 28), width = std::min<int>(S(hWnd, 340), client.right - S(hWnd, 16));
        const int left = std::clamp<int>(field.left, S(hWnd, 4), std::max(S(hWnd, 4), (int)client.right - width - S(hWnd, 4)));
        const int top = field.top >= height + S(hWnd, 4) ? field.top - height - S(hWnd, 3) : field.bottom + S(hWnd, 3);
        RECT tip{left, top, left + width, top + height};
        Graphics graphics(dc);
        OverlayCustom_DrawRoundRect(graphics, tip, RGB(55, 29, 34), RGB(190, 86, 99), (float)S(hWnd, 4), 255);
        InflateRect(&tip, -S(hWnd, 8), 0);
        HGDIOBJ font = SelectObject(dc, GetStockObject(SYSTEM_FONT));
        OverlayCustom_DrawText(dc, message, tip, RGB(245, 192, 198), DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        SelectObject(dc, font);
        break;
    }
}

static LRESULT OverlayCustom_PageProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    auto* st = (OverlayCustomState*)GetWindowLongPtrW(hWnd, GWLP_USERDATA);
    if (msg == halljoy::main_input::QueryExplicitInput())
        return st && GetFocus() == hWnd && OverlayCustom_IsEdit(st->focusId) ? 1 : 0;

    if (msg == PremiumCombo::MsgDropStateChanged())
    {
        if (st && !wParam)
        {
            ShowWindow((HWND)lParam, SW_HIDE);
            OverlayCustom_MarkCacheDirty(hWnd, st);
        }
        return 0;
    }

    if (msg == WM_COMMAND && st && HIWORD(wParam) == CBN_SELCHANGE)
    {
        const int id = LOWORD(wParam);
        HWND combo = (HWND)lParam;
        if (combo == OverlayCustom_ComboForId(st, id))
        {
            const int selection = PremiumCombo::GetCurSel(combo);
            if (id == OVERLAY_ID_DIRECTION)
                OverlayServer_SetFillDirection(selection == 1
                    ? OverlayFillDirection::BottomUp : OverlayFillDirection::TopDown);
            else if (id == OVERLAY_ID_DEPTH_SOURCE)
                OverlayServer_SetUseRawDepth(selection == 1);
            else if (id == OVERLAY_ID_LABEL_FONT)
                OverlayServer_SetLabelFontIndex(std::clamp(selection, 0, 12));
            else if (id == OVERLAY_ID_BRAND) {
                st->layoutPicker.Browse(KeyboardLayout_GetOverlayPresetIndex());
                if (st->layoutPicker.Following()) {
                    KeyboardLayout_SetOverlayPresetIndex(-1);
                    st->layoutPicker.Refresh(-1, true);
                    OverlayCustom_RequestSave(hWnd);
                }
                OverlayCustom_RebuildLayout(hWnd, st);
                OverlayCustom_CloseComboAnchors(st);
                OverlayCustom_MarkCacheDirty(hWnd, st);
                return 0;
            }
            else if (id == OVERLAY_ID_LAYOUT || id == OVERLAY_ID_VARIANT) {
                const int selected = id == OVERLAY_ID_LAYOUT ? st->layoutPicker.ChooseModel() : st->layoutPicker.Selected();
                if (selected < -1) return 0;
                KeyboardLayout_SetOverlayPresetIndex(selected);
                st->layoutPicker.Refresh(KeyboardLayout_GetOverlayPresetIndex(), true);
            }
            OverlayCustom_RebuildLayout(hWnd, st);
            OverlayCustom_RequestSave(hWnd);
            OverlayCustom_MarkCacheDirty(hWnd, st);
            return 0;
        }
    }

    switch (msg)
    {
    case WM_ERASEBKGND:
        return 1;

    case WM_CREATE:
    {
        st = new OverlayCustomState();
        st->portText = std::to_wstring((unsigned)OverlayServer_GetConfiguredPort());
        st->hexText = OverlayPage_FormatHex(OverlayServer_GetAccentColor());
        st->labelHexText = OverlayPage_FormatHex(OverlayServer_GetLabelColor());
        OverlayCustom_SyncColorUiFromRgb(st->indicatorColorUi, OverlayServer_GetAccentColor());
        OverlayCustom_SyncColorUiFromRgb(st->labelColorUi, OverlayServer_GetLabelColor());
        SetWindowLongPtrW(hWnd, GWLP_USERDATA, (LONG_PTR)st);
        OverlayCustom_InitCombos(hWnd, st);
        OverlayCustom_RebuildLayout(hWnd, st);
        return 0;
    }

    case WM_NCDESTROY:
        OverlayCustom_CloseComboAnchors(st);
        KillTimer(hWnd, OVERLAY_ID_COPY);
        OverlayCustom_DestroyCache(st);
        delete st;
        SetWindowLongPtrW(hWnd, GWLP_USERDATA, 0);
        return 0;

    case WM_SIZE:
        OverlayCustom_CloseComboAnchors(st);
        OverlayCustom_RebuildLayout(hWnd, st);
        InvalidateRect(hWnd, nullptr, FALSE);
        return 0;

    case WM_SHOWWINDOW:
        if (wParam && st)
        {
            OverlayCustom_RefreshLayoutCombo(st);
            PremiumCombo::SetCurSel(st->comboDirection,
                OverlayServer_GetFillDirection() == OverlayFillDirection::TopDown ? 0 : 1, false);
            PremiumCombo::SetCurSel(st->comboDepthSource, OverlayServer_GetUseRawDepth() ? 1 : 0, false);
            PremiumCombo::SetCurSel(st->comboLabelFont,
                std::clamp(OverlayServer_GetLabelFontIndex(), 0, 12), false);
            st->focusId = 0;
            st->selectingText = false;
            st->portText = std::to_wstring(OverlayServer_GetConfiguredPort());
            st->hexText = OverlayPage_FormatHex(OverlayServer_GetAccentColor());
            st->labelHexText = OverlayPage_FormatHex(OverlayServer_GetLabelColor());
            OverlayCustom_RebuildLayout(hWnd, st);
            InvalidateRect(hWnd, nullptr, FALSE);
        }
        else if (st)
            OverlayCustom_CloseComboAnchors(st);
        return 0;

    case WM_PAINT:
    {
        uint64_t paintStart = CustomPageSurface_QpcNow();
        PAINTSTRUCT ps{};
        HDC memDC = nullptr;
        HBITMAP bmp = nullptr;
        HGDIOBJ oldBmp = nullptr;
        BeginDoubleBufferPaint(hWnd, ps, memDC, bmp, oldBmp);
        if (st)
        {
            st->surface.contentHeight = st->contentHeight;
            st->surface.scrollY = st->scrollY;
            CustomPageSurface_Present(hWnd, memDC, &st->surface,
                OverlayCustom_RenderCacheContent, st, st->scroll.draggingThumb);
            OverlayCustom_DrawEditFeedback(hWnd, memDC, st);
        }
        EndDoubleBufferPaint(hWnd, ps, memDC, bmp, oldBmp);
        if (st && st->surface.scrollSampleStartMs != 0)
        {
            CustomPageSurface_BeginPaintSample(&st->surface, paintStart);
            CustomPageSurface_MaybeLogScrollPerf(&st->surface, L"ui.overlay.scroll");
        }
        return 0;
    }

    case WM_TIMER:
        if (st && wParam == OVERLAY_ID_COPY)
        {
            KillTimer(hWnd, OVERLAY_ID_COPY);
            st->copyFeedback.clear();
            OverlayCustom_MarkCacheDirty(hWnd, st);
            return 0;
        }
        break;

    case WM_SETCURSOR:
        if (LOWORD(lParam) == HTCLIENT)
        {
            POINT pt{};
            GetCursorPos(&pt);
            ScreenToClient(hWnd, &pt);
            auto* item = OverlayCustom_HitTest(st, pt);
            SetCursor(LoadCursorW(nullptr, item && item->kind == OverlayCustomKind::CopyAddress ? IDC_HAND :
                item && item->kind == OverlayCustomKind::Edit && item->enabled ? IDC_IBEAM : IDC_ARROW));
            return TRUE;
        }
        break;

    case WM_MOUSEMOVE:
    {
        if (!st) break;
        if (st->selectingText && GetCapture() == hWnd) {
            for (const auto& item : st->items) if (item.id == st->focusId) {
                st->editor.MoveTo(OverlayCustom_EditHit(hWnd, st, item, (short)LOWORD(lParam)), true);
                OverlayCustom_MarkCacheDirty(hWnd, st);
                break;
            }
            return 0;
        }
        POINT pt{ (short)LOWORD(lParam), (short)HIWORD(lParam) };
        if (st->scroll.draggingThumb)
        {
            CustomPageSurface_HandleScrollMessage(hWnd, &st->surface, &st->scroll,
                msg, wParam, lParam, S(hWnd, 54));
            st->scrollY = st->surface.scrollY;
            return 0;
        }
        if (st->dragId)
        {
            POINT contentPt = pt;
            contentPt.y += st->scrollY;
            OverlayCustomItem* it = nullptr;
            for (auto& item : st->items)
                if (item.id == st->dragId) { it = &item; break; }
            if (it && it->kind == OverlayCustomKind::Slider)
                OverlayCustom_SetSliderFromPoint(hWnd, st, it, contentPt.x);
            else if (it && it->kind == OverlayCustomKind::Hue)
                OverlayCustom_SetHueFromPoint(hWnd, st, it, contentPt, st->colorDragMode);
            return 0;
        }
        OverlayCustomItem* hot = OverlayCustom_HitTest(st, pt);
        int hotId = hot ? hot->id : 0;
        if (hotId != st->hotId)
        {
            st->hotId = hotId;
            OverlayCustom_MarkCacheDirty(hWnd, st);
        }
        return 0;
    }

    case WM_LBUTTONDBLCLK:
    case WM_LBUTTONDOWN:
    {
        if (!st) break;
        OverlayCustom_CloseComboAnchors(st);
        SetFocus(hWnd);
        POINT pt{ (short)LOWORD(lParam), (short)HIWORD(lParam) };
        st->surface.contentHeight = st->contentHeight;
        st->surface.scrollY = st->scrollY;
        if (CustomPageSurface_HandleScrollMessage(hWnd, &st->surface, &st->scroll,
            msg, wParam, lParam, S(hWnd, 54)) != CustomPageScrollResult::NotHandled)
        {
            st->scrollY = st->surface.scrollY;
            return 0;
        }

        OverlayCustomItem* hit = OverlayCustom_HitTest(st, pt);
        const int oldFocus = st->focusId;
        st->focusId = 0;
        st->editFeedback.clear();
        if (hit && hit->enabled)
        {
            st->pressedId = hit->id;
            if (hit->kind == OverlayCustomKind::Edit) {
                st->focusId = hit->id;
                if (oldFocus != hit->id) st->editor.Reset(OverlayCustom_EditText(st, hit->id), OverlayCustom_EditKind(hit->id));
                if (msg == WM_LBUTTONDBLCLK) st->editor.SelectAll();
                else st->editor.MoveTo(OverlayCustom_EditHit(hWnd, st, *hit, pt.x), oldFocus == hit->id && (GetKeyState(VK_SHIFT) & 0x8000));
                st->selectingText = msg != WM_LBUTTONDBLCLK;
            }
            if (hit->kind == OverlayCustomKind::Slider || hit->kind == OverlayCustomKind::Hue)
            {
                st->dragId = hit->id;
                POINT contentPt = pt;
                contentPt.y += st->scrollY;
                if (hit->kind == OverlayCustomKind::Slider)
                    OverlayCustom_SetSliderFromPoint(hWnd, st, hit, contentPt.x);
                else
                {
                    st->colorDragMode = OverlayCustom_ColorDragModeFromPoint(hit, contentPt);
                    OverlayCustom_SetHueFromPoint(hWnd, st, hit, contentPt, st->colorDragMode);
                }
            }
            SetCapture(hWnd);
        }
        OverlayCustom_MarkCacheDirty(hWnd, st);
        return 0;
    }

    case WM_LBUTTONUP:
    {
        if (!st) break;
        POINT pt{ (short)LOWORD(lParam), (short)HIWORD(lParam) };
        int pressed = st->pressedId;
        bool wasDrag = st->dragId != 0 || st->scroll.draggingThumb || OverlayCustom_IsEdit(pressed);
        st->selectingText = false;
        st->pressedId = 0;
        st->dragId = 0;
        st->colorDragMode = 0;
        CustomPageSurface_HandleScrollMessage(hWnd, &st->surface, &st->scroll,
            msg, wParam, lParam, S(hWnd, 54));
        if (GetCapture() == hWnd)
            ReleaseCapture();
        OverlayCustomItem* hit = OverlayCustom_HitTest(st, pt);
        if (!wasDrag && hit && hit->id == pressed && hit->enabled)
            OverlayCustom_Activate(hWnd, st, hit->id);
        else
            OverlayCustom_MarkCacheDirty(hWnd, st);
        return 0;
    }

    case WM_CAPTURECHANGED:
        if (st)
        {
            st->selectingText = false;
            st->pressedId = 0;
            st->dragId = 0;
            st->colorDragMode = 0;
            CustomPageSurface_HandleScrollMessage(hWnd, &st->surface, &st->scroll,
                msg, wParam, lParam, S(hWnd, 54));
            OverlayCustom_MarkCacheDirty(hWnd, st);
        }
        return 0;

    case WM_MOUSEWHEEL:
        if (st)
        {
            OverlayCustom_CloseComboAnchors(st);
            st->surface.contentHeight = st->contentHeight;
            st->surface.scrollY = st->scrollY;
            CustomPageSurface_HandleScrollMessage(hWnd, &st->surface, &st->scroll,
                msg, wParam, lParam, S(hWnd, 54));
            st->scrollY = st->surface.scrollY;
            return 0;
        }
        break;

    case WM_CHAR:
        if (st && GetFocus() == hWnd && OverlayCustom_IsEdit(st->focusId))
        {
            // Control characters are handled exactly once by WM_KEYDOWN.
            if (wParam < 32 || (GetKeyState(VK_CONTROL) & 0x8000)) return 0;
            st->editFeedback.clear();
            if (!st->editor.Replace(std::wstring(1, (wchar_t)wParam)))
                st->editFeedback = st->editor.kind == halljoy::overlay_edit::Kind::Port ?
                    L"Use up to five digits for the port." : L"Use six HEX digits, optionally starting with #.";
            OverlayCustom_ApplyEdit(hWnd, st);
            return 0;
        }
        break;

    case WM_CANCELMODE:
        if (st) {
            st->selectingText = false;
            if (GetCapture() == hWnd) ReleaseCapture();
            OverlayCustom_MarkCacheDirty(hWnd, st);
        }
        break;
    case WM_KILLFOCUS:
        if (st) {
            st->selectingText = false;
            st->focusId = 0;
            if (GetCapture() == hWnd) ReleaseCapture();
            OverlayCustom_MarkCacheDirty(hWnd, st);
        }
        break;
    case WM_GETDLGCODE:
        if (st && OverlayCustom_IsEdit(st->focusId)) return DLGC_WANTARROWS | DLGC_WANTCHARS | DLGC_WANTTAB;
        break;
    case WM_KEYDOWN:
        if (st && GetFocus() == hWnd && OverlayCustom_IsEdit(st->focusId))
        {
            const bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
            const bool ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
            int command = 0;
            if (ctrl) {
                if (wParam == 'A') command = OVERLAY_EDIT_ALL;
                if (wParam == 'C' || wParam == VK_INSERT) command = OVERLAY_EDIT_COPY;
                if (wParam == 'X') command = OVERLAY_EDIT_CUT;
                if (wParam == 'V') command = OVERLAY_EDIT_PASTE;
                if (wParam == 'Z') command = shift ? OVERLAY_EDIT_REDO : OVERLAY_EDIT_UNDO;
                if (wParam == 'Y') command = OVERLAY_EDIT_REDO;
            }
            if (shift && wParam == VK_INSERT) command = OVERLAY_EDIT_PASTE;
            if (shift && wParam == VK_DELETE) command = OVERLAY_EDIT_CUT;
            if (command) { OverlayCustom_EditCommand(hWnd, st, command); return 0; }
            st->editFeedback.clear();
            switch (wParam) {
            case VK_LEFT: st->editor.Move(-1, shift, ctrl); break;
            case VK_RIGHT: st->editor.Move(1, shift, ctrl); break;
            case VK_HOME: st->editor.MoveTo(0, shift); break;
            case VK_END: st->editor.MoveTo(st->editor.Text().size(), shift); break;
            case VK_BACK: st->editor.Erase(true, ctrl); break;
            case VK_DELETE: st->editor.Erase(false, ctrl); break;
            case VK_ESCAPE:
                // Discard only an incomplete draft; already valid auto-applied edits remain.
                OverlayCustom_EditText(st, st->focusId) = st->focusId == OVERLAY_ID_PORT ?
                    std::to_wstring(OverlayServer_GetConfiguredPort()) : OverlayPage_FormatHex(
                        st->focusId == OVERLAY_ID_LABEL_COLOR_HEX ? OverlayServer_GetLabelColor() : OverlayServer_GetAccentColor());
                st->focusId = 0;
                OverlayCustom_MarkCacheDirty(hWnd, st); return 0;
            case VK_RETURN: {
                uint32_t value = 0;
                if (halljoy::overlay_edit::Value(st->editor.Text(), st->editor.kind, value)) st->focusId = 0;
                OverlayCustom_MarkCacheDirty(hWnd, st); return 0;
            }
            case VK_TAB: {
                const int ids[] = {OVERLAY_ID_PORT, OVERLAY_ID_LABEL_COLOR_HEX, OVERLAY_ID_COLOR_HEX};
                int index = 0;
                while (index < 3 && ids[index] != st->focusId) ++index;
                for (int step = 1; step <= 3; ++step) {
                    const int next = ids[(index + (shift ? -step : step) + 6) % 3];
                    for (const auto& item : st->items) if (item.id == next && item.enabled) {
                        st->focusId = next;
                        st->editor.Reset(OverlayCustom_EditText(st, next), OverlayCustom_EditKind(next));
                        st->editor.SelectAll();
                        RECT viewport{}; GetClientRect(hWnd, &viewport);
                        int scroll = st->scrollY;
                        if (item.rc.top < scroll) scroll = item.rc.top;
                        else if (item.rc.bottom > scroll + viewport.bottom) scroll = item.rc.bottom - viewport.bottom;
                        CustomPageSurface_SetScrollY(hWnd, &st->surface, scroll);
                        st->scrollY = st->surface.scrollY;
                        OverlayCustom_MarkCacheDirty(hWnd, st); return 0;
                    }
                }
                return 0;
            }
            default: return 0;
            }
            OverlayCustom_ApplyEdit(hWnd, st);
            return 0;
        }
        break;

    case WM_CONTEXTMENU:
        if (st) {
            POINT screen{(short)LOWORD(lParam), (short)HIWORD(lParam)};
            POINT client = screen;
            const bool keyboard = screen.x == -1 && screen.y == -1;
            if (!keyboard) ScreenToClient(hWnd, &client);
            OverlayCustomItem* item = keyboard ? nullptr : OverlayCustom_HitTest(st, client);
            if (keyboard) for (auto& candidate : st->items) if (candidate.id == st->focusId) { item = &candidate; break; }
            if (!item || !item->enabled || item->kind != OverlayCustomKind::Edit) break;
            SetFocus(hWnd);
            if (st->focusId != item->id) {
                st->focusId = item->id;
                st->editor.Reset(OverlayCustom_EditText(st, item->id), OverlayCustom_EditKind(item->id));
                st->editor.SelectAll();
            }
            if (keyboard) { screen = {item->rc.left, item->rc.bottom - st->scrollY}; ClientToScreen(hWnd, &screen); }
            HMENU menu = CreatePopupMenu();
            if (!menu) break;
            AppendMenuW(menu, MF_STRING | (st->editor.CanUndo() ? 0 : MF_GRAYED), OVERLAY_EDIT_UNDO, L"Undo");
            AppendMenuW(menu, MF_STRING | (st->editor.CanRedo() ? 0 : MF_GRAYED), OVERLAY_EDIT_REDO, L"Redo");
            AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
            AppendMenuW(menu, MF_STRING | (st->editor.Selected() ? 0 : MF_GRAYED), OVERLAY_EDIT_CUT, L"Cut");
            AppendMenuW(menu, MF_STRING | (st->editor.Selected() ? 0 : MF_GRAYED), OVERLAY_EDIT_COPY, L"Copy");
            AppendMenuW(menu, MF_STRING | (IsClipboardFormatAvailable(CF_UNICODETEXT) ? 0 : MF_GRAYED), OVERLAY_EDIT_PASTE, L"Paste");
            AppendMenuW(menu, MF_STRING, OVERLAY_EDIT_ALL, L"Select all");
            const int chosen = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_NONOTIFY | TPM_RIGHTBUTTON, screen.x, screen.y, 0, hWnd, nullptr);
            DestroyMenu(menu);
            if (chosen && OverlayCustom_IsEdit(st->focusId)) OverlayCustom_EditCommand(hWnd, st, chosen);
            OverlayCustom_MarkCacheDirty(hWnd, st);
            return 0;
        }
        break;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

#if defined(HALLJOY_ANALOG_SIMULATOR)
// Production event wiring exercised on a private, never-displayed desktop.
// No physical input, user clipboard, profile writes or running overlay server.
bool KeyboardSubpages_TestOverlayTextEditing()
{
    HDESK previous = GetThreadDesktop(GetCurrentThreadId());
    wchar_t name[64]{}; swprintf_s(name, L"HallJoyEditTest-%lu", GetCurrentProcessId());
    HDESK desktop = CreateDesktopW(name, nullptr, nullptr, 0, GENERIC_ALL, nullptr);
    if (!desktop) return false;
    if (!SetThreadDesktop(desktop)) { CloseDesktop(desktop); return false; }
    const auto oldPort = OverlayServer_GetConfiguredPort();
    const auto oldColor = OverlayServer_GetAccentColor();
    OverlayServer_SetConfiguredPort(8765);
    WNDCLASSW wc{};
    wc.lpfnWndProc = OverlayCustom_PageProc; wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = L"HallJoyOverlayEditTest"; wc.style = CS_DBLCLKS;
    RegisterClassW(&wc);
    HWND parent = CreateWindowW(L"STATIC", L"", WS_OVERLAPPEDWINDOW, 0, 0, 800, 800, nullptr, nullptr, wc.hInstance, nullptr);
    HWND page = CreateWindowW(wc.lpszClassName, L"", WS_CHILD | WS_VISIBLE, 0, 0, 780, 740, parent, nullptr, wc.hInstance, nullptr);
    bool ok = page != nullptr;
    BYTE originalKeys[256]{}; GetKeyboardState(originalKeys);
    // SetThreadDesktop does not reset this thread's keyboard-state table.
    // Synthetic WM_CHAR must not inherit a held Ctrl/Shift from the caller.
    BYTE neutralKeys[256]{}; SetKeyboardState(neutralKeys);
    if (page) {
        ShowWindow(parent, SW_SHOWNOACTIVATE);
        SetFocus(page);
        auto* st = (OverlayCustomState*)GetWindowLongPtrW(page, GWLP_USERDATA);
        const auto key = [&](WPARAM code, bool control = false, bool shift = false) {
            BYTE keys[256]{};
            keys[VK_CONTROL] = control ? 0x80 : 0; keys[VK_SHIFT] = shift ? 0x80 : 0;
            SetKeyboardState(keys);
            SendMessageW(page, WM_KEYDOWN, code, 0);
            BYTE released[256]{}; SetKeyboardState(released);
        };
        const auto click = [&](int id, bool doubleClick) {
            RECT rect{};
            for (const auto& item : st->items) if (item.id == id) rect = item.rc;
            // Direct message coordinates exercise the page's content/view boundary.
            st->scrollY = std::max(0, (int)rect.top - 100);
            CustomPageSurface_SetScrollY(page, &st->surface, st->scrollY);
            st->scrollY = st->surface.scrollY;
            LPARAM pos = MAKELPARAM(rect.left + S(page, 10), (rect.top + rect.bottom) / 2 - st->scrollY);
            SendMessageW(page, doubleClick ? WM_LBUTTONDBLCLK : WM_LBUTTONDOWN, MK_LBUTTON, pos);
            SendMessageW(page, WM_LBUTTONUP, 0, pos);
        };
        click(OVERLAY_ID_PORT, true);
        MSG typed{};typed.hwnd=page;typed.message=WM_CHAR;typed.wParam='9';
        ok &= halljoy::main_input::Allow(typed,parent);
        ok &= st->editor.Selected() && st->editor.Selection() == L"8765";
        SendMessageW(page, WM_CHAR, '9', 0);
        SendMessageW(page, WM_CHAR, '0', 0);
        ok &= st->portText == L"90" && OverlayServer_GetConfiguredPort() == 90;
        if (!ok) throw std::runtime_error("overlay edit: initial focus/selection/typing failed");
        key(VK_HOME); key(VK_DELETE);
        ok &= st->portText == L"0" && OverlayServer_GetConfiguredPort() == 90;
        key('A', true); SendMessageW(page, WM_CHAR, 1, 0);
        ok &= st->editor.Selected();
        for (wchar_t c : std::wstring(L"65536")) SendMessageW(page, WM_CHAR, c, 0);
        ok &= st->portText == L"65536" && OverlayServer_GetConfiguredPort() == 6553;
        key(VK_ESCAPE);
        ok &= st->portText == L"6553" && st->focusId == 0;
        ok &= !halljoy::main_input::Allow(typed,parent);
        if (!ok) throw std::runtime_error("overlay edit: port validation/escape failed");
        click(OVERLAY_ID_COLOR_HEX, true);
        const auto original = st->hexText;
        // Paste model shares the exact Replace path, without touching user clipboard.
        ok &= st->editor.Replace(L" #aBcDeF ", true);
        OverlayCustom_ApplyEdit(page, st);
        ok &= st->hexText == L"#ABCDEF" && OverlayServer_GetAccentColor() == 0xABCDEF;
        key('Z', true);
        ok &= st->hexText == original;
        key('Y', true);
        ok &= st->hexText == L"#ABCDEF";
        key(VK_HOME); key(VK_RIGHT, false, true);
        ok &= st->editor.Selection() == L"#";
        SendMessageW(page, WM_CANCELMODE, 0, 0);
        ok &= st->focusId == OVERLAY_ID_COLOR_HEX; // Menu entry must retain the edit session.
        SendMessageW(page, WM_KILLFOCUS, 0, 0);
        const auto before = st->hexText;
        SendMessageW(page, WM_CHAR, 'F', 0);
        ok &= st->hexText == before && st->focusId == 0;
        if (!ok) throw std::runtime_error("overlay edit: HEX undo/selection/focus failed");
        const int oldLayout = KeyboardLayout_GetOverlayPresetIndex();
        const int mainLayout = KeyboardLayout_GetCurrentPresetIndex();
        const auto brandIt = std::find(st->layoutPicker.brands.begin(), st->layoutPicker.brands.end(), L"DrunkDeer");
        PremiumCombo::SetCurSel(st->layoutPicker.brand, (int)(brandIt - st->layoutPicker.brands.begin()), false);
        SendMessageW(page, WM_COMMAND, MAKEWPARAM(OVERLAY_ID_BRAND, CBN_SELCHANGE), (LPARAM)st->layoutPicker.brand);
        ok &= KeyboardLayout_GetOverlayPresetIndex() == oldLayout;
        ok &= st->layoutPicker.Row(1) >= 0 && st->layoutPicker.Row(2) == -1;
        PremiumCombo::SetCurSel(st->comboLayout, st->layoutPicker.Row(1), false);
        SendMessageW(page, WM_COMMAND, MAKEWPARAM(OVERLAY_ID_LAYOUT, CBN_SELCHANGE), (LPARAM)st->comboLayout);
        ok &= KeyboardLayout_GetOverlayPresetIndex() == 1 && KeyboardLayout_GetCurrentPresetIndex() == mainLayout;
        int iso=-1;
        for (int i=0;i<KeyboardLayout_GetPresetCount();++i)
            if (KeyboardLayout_GetPresetDisplayName(i)==L"DrunkDeer A75 ISO") iso=i;
        if (iso<0) throw std::runtime_error("overlay variant test: ISO missing");
        PremiumCombo::SetCurSel(st->comboLayout,st->layoutPicker.Row(iso),false);
        SendMessageW(page,WM_COMMAND,MAKEWPARAM(OVERLAY_ID_LAYOUT,CBN_SELCHANGE),(LPARAM)st->comboLayout);
        if (!st->layoutPicker.HasVariants()) throw std::runtime_error("overlay variant test: model has no variants");
        PremiumCombo::SetCurSel(st->layoutPicker.variant,1,false);
        SendMessageW(page,WM_COMMAND,MAKEWPARAM(OVERLAY_ID_VARIANT,CBN_SELCHANGE),(LPARAM)st->layoutPicker.variant);
        if (KeyboardLayout_GetOverlayPresetIndex()!=iso || KeyboardLayout_GetCurrentPresetIndex()!=mainLayout)
            throw std::runtime_error("overlay variant test: selection failed");
        PremiumCombo::SetCurSel(st->layoutPicker.brand, 0, false);
        SendMessageW(page, WM_COMMAND, MAKEWPARAM(OVERLAY_ID_BRAND, CBN_SELCHANGE), (LPARAM)st->layoutPicker.brand);
        ok &= KeyboardLayout_GetOverlayPresetIndex() == -1 && !st->layoutPicker.HasVariants();
        ok &= st->layoutPicker.Following() && st->layoutPicker.Selected() == -1;
        ok &= st->layoutPicker.groups.empty() && st->layoutPicker.variants.empty();
        for (const auto& item : st->items)
            ok &= item.id != OVERLAY_ID_LAYOUT && item.id != OVERLAY_ID_VARIANT && item.id != 32 && item.id != 33;
        // Reconstructed picker restores follow from the same persisted -1 mode;
        // selecting All only browses, exposing models without changing that mode.
        st->layoutPicker.Refresh(KeyboardLayout_GetOverlayPresetIndex(), true);
        ok &= PremiumCombo::GetCurSel(st->layoutPicker.brand) == 0 && st->layoutPicker.Following();
        PremiumCombo::SetCurSel(st->layoutPicker.brand, 1, false);
        SendMessageW(page, WM_COMMAND, MAKEWPARAM(OVERLAY_ID_BRAND, CBN_SELCHANGE), (LPARAM)st->layoutPicker.brand);
        ok &= !st->layoutPicker.Following() && KeyboardLayout_GetOverlayPresetIndex() == -1;
        ok &= !st->layoutPicker.groups.empty();
        for (int preset : st->layoutPicker.presets) ok &= preset >= 0;
        ok &= std::any_of(st->items.begin(), st->items.end(), [](const OverlayCustomItem& item) { return item.id == OVERLAY_ID_LAYOUT; });
        // Real catalog selection must publish every overlay-only key, including keypad.
        KeyboardLayout_SetOverlayPresetName(L"Keychron K4 HE"); // stable saved-name alias
        const auto fullOverlay = KeyboardLayout_GetOverlaySnapshot();
        bool hasKeypad = false;
        for (const auto& key : fullOverlay->keys) {
            if (key.hid == 0x59) hasKeypad = true;
            if (halljoy::keycode::IsSupported(key.hid)) ok &= BackendUI_TestIsTracked(key.hid);
        }
        ok &= hasKeypad && KeyboardLayout_GetCurrentPresetIndex() == mainLayout;
        KeyboardLayout_SetOverlayPresetIndex(oldLayout);
    }
    SetKeyboardState(originalKeys);
    if (parent) DestroyWindow(parent);
    UnregisterClassW(wc.lpszClassName, wc.hInstance);
    OverlayServer_SetConfiguredPort(oldPort); OverlayServer_SetAccentColor(oldColor);
    ok &= SetThreadDesktop(previous) != FALSE;
    CloseDesktop(desktop);
    return ok;
}
#endif

LRESULT CALLBACK KeyboardSubpages_InputOverlayPageProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    return OverlayCustom_PageProc(hWnd, msg, wParam, lParam);

    auto* st = (InputOverlayPageState*)GetWindowLongPtrW(hWnd, GWLP_USERDATA);

    switch (msg)
    {
    case WM_ERASEBKGND: return 1;

    case WM_CTLCOLORSTATIC:
    {
        HDC hdc = (HDC)wParam;
        SetBkMode(hdc, TRANSPARENT);

        HWND hCtl = (HWND)lParam;
        if (st && hCtl && st->lblTitle && hCtl == st->lblTitle)
            SetTextColor(hdc, UiTheme::Color_Text());
        else if (st && hCtl && st->lblUrl && hCtl == st->lblUrl)
            SetTextColor(hdc, UiTheme::Color_Text());
        else if (st && hCtl && st->lblStatus && hCtl == st->lblStatus)
            SetTextColor(hdc, OverlayServer_IsRunning() ? UiTheme::Color_Text() : UiTheme::Color_TextMuted());
        else
            SetTextColor(hdc, UiTheme::Color_TextMuted());

        return (LRESULT)UiTheme::Brush_PanelBg();
    }

    case WM_CTLCOLOREDIT:
    {
        HDC hdc = (HDC)wParam;
        SetBkMode(hdc, OPAQUE);
        SetTextColor(hdc, UiTheme::Color_Text());
        SetBkColor(hdc, UiTheme::Color_ControlBg());
        return (LRESULT)UiTheme::Brush_ControlBg();
    }

    case WM_CREATE:
    {
        auto* cs = (CREATESTRUCTW*)lParam;
        HINSTANCE hInst = cs ? cs->hInstance : (HINSTANCE)GetWindowLongPtrW(hWnd, GWLP_HINSTANCE);
        HFONT hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);

        st = new InputOverlayPageState();
        SetWindowLongPtrW(hWnd, GWLP_USERDATA, (LONG_PTR)st);

        st->lblTitle = CreateWindowW(L"STATIC", L"Input overlay",
            WS_CHILD | WS_VISIBLE, 0, 0, 100, 20, hWnd, nullptr, hInst, nullptr);
        st->lblPort = CreateWindowW(L"STATIC", L"Port",
            WS_CHILD | WS_VISIBLE, 0, 0, 100, 20, hWnd, nullptr, hInst, nullptr);
        wchar_t portText[16]{};
        swprintf_s(portText, L"%u", (unsigned)OverlayServer_GetConfiguredPort());
        st->edtPort = CreateWindowW(L"EDIT", portText,
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_NUMBER | ES_AUTOHSCROLL | WS_BORDER,
            0, 0, 100, 24, hWnd, (HMENU)(INT_PTR)OVERLAY_ID_PORT, hInst, nullptr);
        st->lblDirection = CreateWindowW(L"STATIC", L"Fill direction",
            WS_CHILD | WS_VISIBLE, 0, 0, 100, 20, hWnd, nullptr, hInst, nullptr);
        st->btnDirection = CreateWindowW(L"BUTTON", L"",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
            0, 0, 100, 28, hWnd, (HMENU)(INT_PTR)OVERLAY_ID_DIRECTION, hInst, nullptr);
        st->lblDepthSource = CreateWindowW(L"STATIC", L"Depth display",
            WS_CHILD | WS_VISIBLE, 0, 0, 100, 20, hWnd, nullptr, hInst, nullptr);
        st->btnDepthSource = CreateWindowW(L"BUTTON", L"",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
            0, 0, 100, 28, hWnd, (HMENU)(INT_PTR)OVERLAY_ID_DEPTH_SOURCE, hInst, nullptr);
        st->lblEffects = CreateWindowW(L"STATIC", L"Visual effects",
            WS_CHILD | WS_VISIBLE, 0, 0, 100, 20, hWnd, nullptr, hInst, nullptr);
        st->chkSmoothing = CreateWindowW(L"BUTTON", L"Smooth response",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_CHECKBOX | BS_OWNERDRAW,
            0, 0, 100, 28, hWnd, (HMENU)(INT_PTR)OVERLAY_ID_EFFECT_SMOOTHING, hInst, nullptr);
        st->chkGlass = CreateWindowW(L"BUTTON", L"Glass keys",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_CHECKBOX | BS_OWNERDRAW,
            0, 0, 100, 28, hWnd, (HMENU)(INT_PTR)OVERLAY_ID_EFFECT_GLASS, hInst, nullptr);
        st->chkBloom = CreateWindowW(L"BUTTON", L"Bloom",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_CHECKBOX | BS_OWNERDRAW,
            0, 0, 100, 28, hWnd, (HMENU)(INT_PTR)OVERLAY_ID_EFFECT_BLOOM, hInst, nullptr);
        st->chkEdge = CreateWindowW(L"BUTTON", L"Edge sweep",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_CHECKBOX | BS_OWNERDRAW,
            0, 0, 100, 28, hWnd, (HMENU)(INT_PTR)OVERLAY_ID_EFFECT_EDGE, hInst, nullptr);
        st->chkScale = CreateWindowW(L"BUTTON", L"Micro-scale",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_CHECKBOX | BS_OWNERDRAW,
            0, 0, 100, 28, hWnd, (HMENU)(INT_PTR)OVERLAY_ID_EFFECT_SCALE, hInst, nullptr);
        st->chkLabel = CreateWindowW(L"BUTTON", L"Label contrast",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_CHECKBOX | BS_OWNERDRAW,
            0, 0, 100, 28, hWnd, (HMENU)(INT_PTR)OVERLAY_ID_EFFECT_LABEL, hInst, nullptr);
        st->chkRimLight = CreateWindowW(L"BUTTON", L"Rim lighting",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_CHECKBOX | BS_OWNERDRAW,
            0, 0, 100, 28, hWnd, (HMENU)(INT_PTR)OVERLAY_ID_EFFECT_RIM_LIGHT, hInst, nullptr);
        st->sldSmoothingStrength = PremiumSlider_Create(hWnd, hInst, 0, 0, 10, 10, OVERLAY_ID_STRENGTH_SMOOTHING);
        SendMessageW(st->sldSmoothingStrength, TBM_SETRANGE, TRUE, MAKELONG(0, 100));
        st->chipSmoothingStrength = PremiumChip_Create(hWnd, hInst, 0, 0, 10, 10, OVERLAY_ID_STRENGTH_SMOOTHING + 100);
        st->sldGlassStrength = PremiumSlider_Create(hWnd, hInst, 0, 0, 10, 10, OVERLAY_ID_STRENGTH_GLASS);
        SendMessageW(st->sldGlassStrength, TBM_SETRANGE, TRUE, MAKELONG(0, 100));
        st->chipGlassStrength = PremiumChip_Create(hWnd, hInst, 0, 0, 10, 10, OVERLAY_ID_STRENGTH_GLASS + 100);
        st->sldBloomStrength = PremiumSlider_Create(hWnd, hInst, 0, 0, 10, 10, OVERLAY_ID_STRENGTH_BLOOM);
        SendMessageW(st->sldBloomStrength, TBM_SETRANGE, TRUE, MAKELONG(0, 100));
        st->chipBloomStrength = PremiumChip_Create(hWnd, hInst, 0, 0, 10, 10, OVERLAY_ID_STRENGTH_BLOOM + 100);
        st->sldEdgeStrength = PremiumSlider_Create(hWnd, hInst, 0, 0, 10, 10, OVERLAY_ID_STRENGTH_EDGE);
        SendMessageW(st->sldEdgeStrength, TBM_SETRANGE, TRUE, MAKELONG(0, 100));
        st->chipEdgeStrength = PremiumChip_Create(hWnd, hInst, 0, 0, 10, 10, OVERLAY_ID_STRENGTH_EDGE + 100);
        st->sldScaleStrength = PremiumSlider_Create(hWnd, hInst, 0, 0, 10, 10, OVERLAY_ID_STRENGTH_SCALE);
        SendMessageW(st->sldScaleStrength, TBM_SETRANGE, TRUE, MAKELONG(0, 100));
        st->chipScaleStrength = PremiumChip_Create(hWnd, hInst, 0, 0, 10, 10, OVERLAY_ID_STRENGTH_SCALE + 100);
        st->sldLabelStrength = PremiumSlider_Create(hWnd, hInst, 0, 0, 10, 10, OVERLAY_ID_STRENGTH_LABEL);
        SendMessageW(st->sldLabelStrength, TBM_SETRANGE, TRUE, MAKELONG(0, 100));
        st->chipLabelStrength = PremiumChip_Create(hWnd, hInst, 0, 0, 10, 10, OVERLAY_ID_STRENGTH_LABEL + 100);
        st->sldRimLightStrength = PremiumSlider_Create(hWnd, hInst, 0, 0, 10, 10, OVERLAY_ID_STRENGTH_RIM_LIGHT);
        SendMessageW(st->sldRimLightStrength, TBM_SETRANGE, TRUE, MAKELONG(0, 100));
        st->chipRimLightStrength = PremiumChip_Create(hWnd, hInst, 0, 0, 10, 10, OVERLAY_ID_STRENGTH_RIM_LIGHT + 100);

        st->lblRefreshMs = CreateWindowW(L"STATIC", L"Overlay refresh interval",
            WS_CHILD | WS_VISIBLE, 0, 0, 100, 20, hWnd, nullptr, hInst, nullptr);
        st->sldRefreshMs = PremiumSlider_Create(hWnd, hInst, 0, 0, 10, 10, OVERLAY_ID_REFRESH_MS);
        SendMessageW(st->sldRefreshMs, TBM_SETRANGE, TRUE, MAKELONG(1, 100));
        st->chipRefreshMs = PremiumChip_Create(hWnd, hInst, 0, 0, 10, 10, OVERLAY_ID_REFRESH_MS + 100);

        st->lblColor = CreateWindowW(L"STATIC", L"Indicator color",
            WS_CHILD | WS_VISIBLE, 0, 0, 100, 20, hWnd, nullptr, hInst, nullptr);
        st->colorPreview = CreateWindowW(L"STATIC", L"",
            WS_CHILD | WS_VISIBLE | SS_OWNERDRAW,
            0, 0, 34, 26, hWnd, (HMENU)(INT_PTR)OVERLAY_ID_COLOR_PREVIEW, hInst, nullptr);
        st->hueBar = CreateWindowW(L"BUTTON", L"",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
            0, 0, 180, 26, hWnd, (HMENU)(INT_PTR)OVERLAY_ID_COLOR_HUE, hInst, nullptr);
        st->lblHex = CreateWindowW(L"STATIC", L"HEX",
            WS_CHILD | WS_VISIBLE | SS_CENTERIMAGE, 0, 0, 34, 24, hWnd, nullptr, hInst, nullptr);
        st->edtHex = CreateWindowW(L"EDIT", L"",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_BORDER | ES_AUTOHSCROLL | ES_UPPERCASE,
            0, 0, 110, 24, hWnd, (HMENU)(INT_PTR)OVERLAY_ID_COLOR_HEX, hInst, nullptr);
        st->lblUrlCaption = CreateWindowW(L"STATIC", L"OBS browser source URL",
            WS_CHILD | WS_VISIBLE, 0, 0, 100, 20, hWnd, nullptr, hInst, nullptr);
        st->lblUrl = CreateWindowW(L"STATIC", L"",
            WS_CHILD | WS_VISIBLE | SS_ENDELLIPSIS, 0, 0, 100, 20, hWnd, nullptr, hInst, nullptr);
        st->lblStatusCaption = CreateWindowW(L"STATIC", L"Server status",
            WS_CHILD | WS_VISIBLE, 0, 0, 100, 20, hWnd, nullptr, hInst, nullptr);
        st->lblStatus = CreateWindowW(L"STATIC", L"",
            WS_CHILD | WS_VISIBLE | SS_ENDELLIPSIS, 0, 0, 100, 20, hWnd, nullptr, hInst, nullptr);
        st->lblHint = CreateWindowW(L"STATIC",
            L"Add the URL to OBS as a Browser Source. The page renders the current HallJoy keyboard layout and HE analog depth.",
            WS_CHILD | WS_VISIBLE, 0, 0, 100, 20, hWnd, nullptr, hInst, nullptr);
        st->btnToggle = CreateWindowW(L"BUTTON", L"",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW, 0, 0, 100, 28, hWnd, (HMENU)(INT_PTR)OVERLAY_ID_TOGGLE, hInst, nullptr);
        st->btnOpen = CreateWindowW(L"BUTTON", L"Open",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW, 0, 0, 100, 28, hWnd, (HMENU)(INT_PTR)OVERLAY_ID_OPEN, hInst, nullptr);
        st->btnCopy = CreateWindowW(L"BUTTON", L"Copy URL",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW, 0, 0, 100, 28, hWnd, (HMENU)(INT_PTR)OVERLAY_ID_COPY, hInst, nullptr);

        HWND ctrls[] = {
            st->lblTitle, st->lblPort, st->edtPort, st->lblDirection, st->btnDirection,
            st->lblDepthSource, st->btnDepthSource,
            st->lblEffects, st->chkSmoothing, st->chkGlass, st->chkBloom,
            st->chkEdge, st->chkScale, st->chkLabel, st->chkRimLight,
            st->sldSmoothingStrength, st->chipSmoothingStrength,
            st->sldGlassStrength, st->chipGlassStrength,
            st->sldBloomStrength, st->chipBloomStrength,
            st->sldEdgeStrength, st->chipEdgeStrength,
            st->sldScaleStrength, st->chipScaleStrength,
            st->sldLabelStrength, st->chipLabelStrength,
            st->sldRimLightStrength, st->chipRimLightStrength,
            st->lblRefreshMs, st->sldRefreshMs, st->chipRefreshMs,
            st->lblColor,
            st->colorPreview, st->hueBar, st->lblHex, st->edtHex,
            st->lblUrlCaption, st->lblUrl, st->lblStatusCaption, st->lblStatus,
            st->btnToggle, st->btnOpen, st->btnCopy, st->lblHint
        };
        for (HWND c : ctrls)
        {
            SendMessageW(c, WM_SETFONT, (WPARAM)hFont, TRUE);
            UiTheme::ApplyToControl(c);
        }
        if (st->hueBar)
            SetWindowSubclass(st->hueBar, OverlayHueBar_SubclassProc, 1, 0);

        OverlayPage_Update(st);
        OverlayPage_Layout(hWnd, st);
        return 0;
    }

    case WM_SIZE:
        if (st)
        {
            int keepScroll = st->scrollY;
            if (keepScroll != 0)
            {
                OverlayPage_OffsetAllChildren(hWnd, keepScroll);
                st->scrollY = 0;
            }
            OverlayPage_Layout(hWnd, st);
            OverlayPage_SetScrollY(hWnd, st, keepScroll);
        }
        else
        {
            OverlayPage_Layout(hWnd, st);
        }
        return 0;

    case WM_SHOWWINDOW:
        if (wParam && st)
        {
            OverlayPage_Update(st);
            InvalidateRect(hWnd, nullptr, FALSE);
        }
        break;

    case WM_PAINT:
    {
        PAINTSTRUCT ps{};
        HDC memDC = nullptr;
        HBITMAP bmp = nullptr;
        HGDIOBJ oldBmp = nullptr;
        BeginDoubleBufferPaint(hWnd, ps, memDC, bmp, oldBmp);

        RECT rc{};
        GetClientRect(hWnd, &rc);
        FillRect(memDC, &rc, UiTheme::Brush_PanelBg());
        OverlayPage_DrawScrollbar(hWnd, memDC, st);

        EndDoubleBufferPaint(hWnd, ps, memDC, bmp, oldBmp);
        return 0;
    }

    case WM_LBUTTONDOWN:
    {
        if (!st) break;
        POINT pt{ (short)LOWORD(lParam), (short)HIWORD(lParam) };
        RECT thumb = OverlayPage_GetScrollThumbRect(hWnd, st);
        RECT track = OverlayPage_GetScrollTrackRect(hWnd);
        int maxScroll = OverlayPage_GetMaxScroll(hWnd, st);
        if (maxScroll > 0 && PtInRect(&thumb, pt))
        {
            st->scrollDrag = true;
            st->scrollDragGrabOffsetY = pt.y - thumb.top;
            st->scrollDragThumbHeight = std::max(1, (int)thumb.bottom - (int)thumb.top);
            st->scrollDragMax = maxScroll;
            SetCapture(hWnd);
            InvalidateRect(hWnd, nullptr, FALSE);
            return 0;
        }
        if (maxScroll > 0 && PtInRect(&track, pt))
        {
            int page = std::max(S(hWnd, 80), (int)((track.bottom - track.top) * 0.75));
            OverlayPage_SetScrollY(hWnd, st, pt.y < thumb.top ? st->scrollY - page : st->scrollY + page);
            return 0;
        }
        break;
    }

    case WM_MOUSEMOVE:
        if (st && st->scrollDrag)
        {
            POINT pt{ (short)LOWORD(lParam), (short)HIWORD(lParam) };
            RECT track = OverlayPage_GetScrollTrackRect(hWnd);
            int thumbH = std::max(1, st->scrollDragThumbHeight);
            int travel = std::max(1, (int)(track.bottom - track.top) - thumbH);
            int maxScroll = std::max(1, st->scrollDragMax);
            int topWanted = pt.y - st->scrollDragGrabOffsetY;
            int topClamped = std::clamp(topWanted, (int)track.top, (int)track.bottom - thumbH);
            double t = (double)(topClamped - track.top) / (double)travel;
            OverlayPage_SetScrollY(hWnd, st, (int)std::lround(t * (double)maxScroll));
            return 0;
        }
        break;

    case WM_LBUTTONUP:
        if (st && st->scrollDrag)
        {
            st->scrollDrag = false;
            if (GetCapture() == hWnd)
                ReleaseCapture();
            InvalidateRect(hWnd, nullptr, FALSE);
            return 0;
        }
        break;

    case WM_CAPTURECHANGED:
        if (st && st->scrollDrag)
        {
            st->scrollDrag = false;
            InvalidateRect(hWnd, nullptr, FALSE);
            return 0;
        }
        break;

    case WM_MOUSEWHEEL:
        if (st)
        {
            int delta = GET_WHEEL_DELTA_WPARAM(wParam);
            int lines = 3;
            SystemParametersInfoW(SPI_GETWHEELSCROLLLINES, 0, &lines, 0);
            if (lines <= 0) lines = 3;

            int linePx = S(hWnd, 18);
            int step = std::max(S(hWnd, 24), lines * linePx);
            int notches = delta / WHEEL_DELTA;
            if (notches == 0)
                notches = (delta > 0) ? 1 : -1;
            OverlayPage_SetScrollY(hWnd, st, st->scrollY - (notches * step));
            return 0;
        }
        break;

    case WM_HSCROLL:
        if (st && lParam)
        {
            uint32_t strengthFlag = 0;
            int ctrlId = GetDlgCtrlID((HWND)lParam);
            if (OverlayPage_StrengthFromId(ctrlId, &strengthFlag))
            {
                int v = (int)SendMessageW((HWND)lParam, TBM_GETPOS, 0, 0);
                OverlayServer_SetEffectStrengthPercent(strengthFlag, v);
                OverlayPage_UpdateSliderControls(st);
                OverlayPage_RequestSave(hWnd);
                return 0;
            }
        }
        if (st && (HWND)lParam == st->sldRefreshMs)
        {
            int v = (int)SendMessageW(st->sldRefreshMs, TBM_GETPOS, 0, 0);
            OverlayServer_SetRefreshIntervalMs(v);
            OverlayPage_UpdateSliderControls(st);
            OverlayPage_RequestSave(hWnd);
            return 0;
        }
        break;

    case WM_DRAWITEM:
    {
        DRAWITEMSTRUCT* dis = (DRAWITEMSTRUCT*)lParam;
        if (!dis)
            break;

        uint32_t ignored = 0;
        if (OverlayPage_EffectFromId((int)dis->CtlID, &ignored))
        {
            OverlayPage_DrawCheckbox(dis);
            return TRUE;
        }
        if (dis->CtlID == OVERLAY_ID_COLOR_HUE)
        {
            OverlayPage_DrawHueBar(dis);
            return TRUE;
        }
        if (dis->CtlID == OVERLAY_ID_COLOR_PREVIEW)
        {
            OverlayPage_DrawColorPreview(dis);
            return TRUE;
        }
        if (dis->CtlID == OVERLAY_ID_TOGGLE ||
            dis->CtlID == OVERLAY_ID_OPEN ||
            dis->CtlID == OVERLAY_ID_COPY ||
            dis->CtlID == OVERLAY_ID_DIRECTION ||
            dis->CtlID == OVERLAY_ID_DEPTH_SOURCE)
        {
            OverlayPage_DrawButton(dis);
            return TRUE;
        }
        break;
    }

    case WM_COMMAND:
    {
        switch (LOWORD(wParam))
        {
        case OVERLAY_ID_TOGGLE:
            if (OverlayServer_IsRunning())
            {
                OverlayServer_Stop();
                OverlayServer_SetAutoStart(false);
            }
            else
            {
                OverlayServer_Start(OverlayPage_GetPort(st));
                OverlayServer_SetAutoStart(OverlayServer_IsRunning());
            }
            OverlayPage_Update(st);
            InvalidateRect(hWnd, nullptr, FALSE);
            OverlayPage_RequestSave(hWnd);
            return 0;

        case OVERLAY_ID_OPEN:
        {
            std::wstring url = OverlayServer_GetUrl();
            ShellExecuteW(hWnd, L"open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
            return 0;
        }

        case OVERLAY_ID_COPY:
            OverlayPage_SetClipboardText(hWnd, OverlayPage_BuildUrl(st));
            return 0;

        case OVERLAY_ID_DIRECTION:
        {
            OverlayFillDirection direction = OverlayServer_GetFillDirection();
            OverlayServer_SetFillDirection(direction == OverlayFillDirection::TopDown
                ? OverlayFillDirection::BottomUp
                : OverlayFillDirection::TopDown);
            OverlayPage_Update(st);
            OverlayPage_RequestSave(hWnd);
            return 0;
        }

        case OVERLAY_ID_DEPTH_SOURCE:
            OverlayServer_SetUseRawDepth(!OverlayServer_GetUseRawDepth());
            OverlayPage_Update(st);
            OverlayPage_RequestSave(hWnd);
            return 0;

        case OVERLAY_ID_PORT:
            if (HIWORD(wParam) == EN_CHANGE)
            {
                OverlayServer_SetConfiguredPort(OverlayPage_GetPort(st));
                OverlayPage_Update(st);
                OverlayPage_RequestSave(hWnd);
                return 0;
            }
            break;

        case OVERLAY_ID_COLOR_HEX:
            if (HIWORD(wParam) == EN_CHANGE && st && !st->updatingColorText)
            {
                wchar_t buf[32]{};
                GetWindowTextW(st->edtHex, buf, (int)_countof(buf));
                uint32_t color = 0;
                if (OverlayPage_ParseHex(buf, &color))
                {
                    OverlayServer_SetAccentColor(color);
                    if (st->hueBar)
                        InvalidateRect(st->hueBar, nullptr, FALSE);
                    if (st->colorPreview)
                        InvalidateRect(st->colorPreview, nullptr, FALSE);
                    OverlayPage_RequestSave(hWnd);
                }
                return 0;
            }
            break;

        default:
        {
            uint32_t effectFlag = 0;
            int id = LOWORD(wParam);
            if (OverlayPage_EffectFromId(id, &effectFlag))
            {
                bool enabled = !OverlayServer_GetEffectEnabled(effectFlag);
                OverlayServer_SetEffectEnabled(effectFlag, enabled);
                if ((HWND)lParam)
                    SendMessageW((HWND)lParam, BM_SETCHECK, enabled ? BST_CHECKED : BST_UNCHECKED, 0);
                if ((HWND)lParam)
                    InvalidateRect((HWND)lParam, nullptr, FALSE);
                OverlayPage_Update(st);
                OverlayPage_RequestSave(hWnd);
                return 0;
            }
            break;
        }
        }
        break;
    }

    case WM_DESTROY:
        delete st;
        SetWindowLongPtrW(hWnd, GWLP_USERDATA, 0);
        return 0;
    }

    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

// ============================================================================
// Keyboard Layout page (preset picker + visual editor)
// ============================================================================
static LRESULT CALLBACK Layout_ButtonMouseProc(HWND button, UINT msg, WPARAM wParam, LPARAM lParam,
    UINT_PTR subclassId, DWORD_PTR)
{
    // Owner-drawn BUTTON interprets the second press as BN_DOUBLECLICKED.
    // Treat it as a normal press so native release/cancel/disabled semantics
    // remain intact, instead of firing an extra action on mouse-down.
    if (msg == WM_LBUTTONDBLCLK) msg = WM_LBUTTONDOWN;
    if (msg == WM_NCDESTROY) RemoveWindowSubclass(button, Layout_ButtonMouseProc, subclassId);
    return DefSubclassProc(button, msg, wParam, lParam);
}

struct LayoutPageState
{
    HWND btnUndo = nullptr, btnRedo = nullptr, btnFit = nullptr, btnDuplicate = nullptr;
    HWND lblStatus = nullptr, lblEmpty = nullptr, lblY = nullptr, edtY = nullptr, lblLayoutTools = nullptr;
    HWND btnPrecision = nullptr, btnSnap = nullptr, btnActual = nullptr;
    halljoy::layout_editor::History history;
    halljoy::layout_editor::View view;
    bool restoringHistory = false, preciseY = false, snap = false, panning = false;
    POINT panPoint{};
    bool resizing = false;
    POINT resizeStart{};
    int resizeWidth = 0, resizeHeight = 0;
    int resizeEdges = 0;
    halljoy::layout_editor::Geometry resizeOrigin{};
    std::vector<halljoy::layout_editor::Guide> guides; // Editor-session aids, not layout geometry.
    int draggingGuide = -1, guideOriginal = 0;
    bool guideNew = false;
    RECT topRuler{}, leftRuler{};
    HWND lblPreset = nullptr;
    HWND cmbPreset = nullptr;
    LayoutPicker layoutPicker;
    HWND lblBrand = nullptr, lblModel = nullptr, lblVariant = nullptr;
    HWND btnAdd = nullptr;
    HWND btnDelete = nullptr;
    HWND btnReset = nullptr;
    HWND btnSave = nullptr;
    HWND btnUniformSpacing = nullptr;
    HWND lblUniformGap = nullptr;
    HWND edtUniformGap = nullptr;
    HWND lblLabel = nullptr;
    HWND edtLabel = nullptr;
    HWND btnBindKey = nullptr;
    HWND lblPos = nullptr;
    HWND edtPos = nullptr;
    HWND lblWidth = nullptr;
    HWND edtWidth = nullptr;
    HWND lblHeight = nullptr;
    HWND edtHeight = nullptr;
    HWND lblNotchW = nullptr, edtNotchW = nullptr, lblNotchY = nullptr, edtNotchY = nullptr;
    bool shapeControlsVisible = false;
    HWND lblBindState = nullptr;
    HWND lblKeys = nullptr;
    HWND lblHint = nullptr;

    int selectedIdx = -1;
    bool dragging = false;
    bool dirty = false;
    bool hasUnsaved = false;
    bool resolvingDraft = false;
    bool bindArmed = false;
    float dragOffsetX = 0;
    float dragOffsetY = 0;
    RECT canvasRc{};
    int editingPresetIdx = 0;
    std::vector<KeyDef> draftKeys;
    std::vector<std::wstring> draftLabels;
    uint32_t previewHash = 0;
    bool uniformSpacingEnabled = false;
    int uniformSpacingGap = 8;
};

static constexpr int ID_LAYOUT_PRESET = 8111;
static constexpr int ID_LAYOUT_BRAND = 8190;
static constexpr int ID_LAYOUT_VARIANT = 8191;
static constexpr int ID_LAYOUT_RESET = 8112;
static constexpr int ID_LAYOUT_SAVE = 8114;
static constexpr int ID_LAYOUT_ADD = 8115;
static constexpr int ID_LAYOUT_DELETE = 8116;
static constexpr int ID_LAYOUT_LABEL_EDIT = 8117;
static constexpr int ID_LAYOUT_BIND_KEY = 8119;
static constexpr int ID_LAYOUT_UNIFORM_SPACING = 8120;
static constexpr int ID_LAYOUT_POS_EDIT = 8122;
static constexpr int ID_LAYOUT_WIDTH_EDIT = 8123;
static constexpr int ID_LAYOUT_NOTCH_W = 8191, ID_LAYOUT_NOTCH_Y = 8192;
static constexpr int ID_LAYOUT_UNIFORM_GAP_EDIT = 8124;
static constexpr int ID_LAYOUT_HEIGHT_EDIT = 8125;
static constexpr UINT_PTR ID_LAYOUT_UI_TIMER = 8121;
static constexpr int ID_LAYOUT_UNDO = 8130, ID_LAYOUT_REDO = 8131, ID_LAYOUT_FIT = 8132;
static constexpr int ID_LAYOUT_DUPLICATE = 8133, ID_LAYOUT_Y = 8134;
static constexpr int ID_LAYOUT_PRECISION = 8135, ID_LAYOUT_SNAP = 8136;
static constexpr int ID_LAYOUT_ACTUAL = 8137;

static bool Layout_NudgeSelectedKey(HWND hWnd, LayoutPageState* st, int dRow, int dX, int dW);
static void Layout_SetUnsaved(LayoutPageState* st, bool on);
static void Layout_RefreshSelection(HWND hWnd, LayoutPageState* st);
static void Layout_NotifyMainPage(HWND hWnd);
static bool Layout_LoadDraftFromPreset(HWND hWnd, LayoutPageState* st, int presetIdx, bool clearUnsaved);
static void Layout_UpdateBindStateOnly(LayoutPageState* st);
static uint32_t Layout_ComputePreviewHash(LayoutPageState* st);
static void Layout_ApplyGeometryFromEdits(HWND hWnd, LayoutPageState* st, bool applyPos, bool applyWidth, bool applyHeight);
static void Layout_BuildDisplayXMap(const LayoutPageState* st, std::vector<int>& outDisplayX);
static void Layout_UpdateUniformSpacingButton(LayoutPageState* st);
static void Layout_ApplyUniformGapFromEdit(HWND hWnd, LayoutPageState* st);
static void Layout_UpdateHintText(LayoutPageState* st);
static bool Layout_BakeUniformSpacingIntoDraft(LayoutPageState* st);
static bool Layout_ResolveDraft(HWND hWnd, LayoutPageState* st, bool saveWithoutPrompt = false);
static bool Layout_SaveDraft(HWND hWnd, LayoutPageState* st);
static void Layout_UpdateMetaControls(LayoutPageState* st);
static void Layout_Arrange(HWND hWnd, LayoutPageState* st);

static halljoy::layout_editor::Draft Layout_CaptureDraft(const LayoutPageState* st)
{
    return { st->draftKeys, st->draftLabels, st->uniformSpacingEnabled, st->uniformSpacingGap, st->selectedIdx };
}

static void Layout_SetWindowTextIfChanged(HWND hWnd, const wchar_t* text)
{
    if (!hWnd || !IsWindow(hWnd)) return;
    const wchar_t* target = text ? text : L"";
    wchar_t cur[256]{};
    GetWindowTextW(hWnd, cur, (int)(sizeof(cur) / sizeof(cur[0])));
    if (wcscmp(cur, target) != 0)
        SetWindowTextW(hWnd, target);
}

static bool Layout_ReadNumber(HWND field, int minimum, int maximum, int& value)
{
    if (!field || GetWindowTextLengthW(field) > 9) return false;
    wchar_t text[16]{};
    if (!GetWindowTextW(field, text, 16)) return false;
    int parsed = 0;
    for (const wchar_t* c = text; *c; ++c)
    {
        if (*c < L'0' || *c > L'9') return false;
        parsed = parsed * 10 + (*c - L'0');
        if (parsed > maximum) return false;
    }
    if (parsed < minimum) return false;
    value = parsed; return true;
}

static void Layout_UpdateUniformSpacingButton(LayoutPageState* st)
{
    if (!st || !st->btnUniformSpacing) return;
    Layout_SetWindowTextIfChanged(
        st->btnUniformSpacing,
        st->uniformSpacingEnabled ? L"Auto spacing: On" : L"Auto spacing: Off");
}

static void Layout_UpdateHintText(LayoutPageState* st)
{
    if (!st || !st->lblHint) return;
    if (st->uniformSpacingEnabled)
    {
        Layout_SetWindowTextIfChanged(st->lblHint,
            L"Wheel: zoom  |  Middle-drag: pan\nExact values use layout pixels.");
    }
    else
    {
        Layout_SetWindowTextIfChanged(st->lblHint,
            L"Drag rulers: guides  |  Alt: no snapping\nWheel: zoom  |  Middle-drag: pan");
    }
}

static bool Layout_BakeUniformSpacingIntoDraft(LayoutPageState* st)
{
    if (!st || !st->uniformSpacingEnabled) return false;
    if (st->draftKeys.empty()) return false;

    std::vector<int> displayX;
    Layout_BuildDisplayXMap(st, displayX);
    if (displayX.size() < st->draftKeys.size()) return false;

    bool changed = false;
    for (size_t i = 0; i < st->draftKeys.size(); ++i)
    {
        int nx = std::clamp(displayX[i], 0, 4000);
        if (st->draftKeys[i].x != nx)
        {
            st->draftKeys[i].x = nx;
            changed = true;
        }
    }
    return changed;
}

static void Layout_ApplyUniformGapFromEdit(HWND hWnd, LayoutPageState* st)
{
    if (!st || !st->edtUniformGap) return;

    int v = 0;
    if (!Layout_ReadNumber(st->edtUniformGap, 0, 120, v)) return;

    if (v != st->uniformSpacingGap)
    {
        st->uniformSpacingGap = v;
        Layout_SetUnsaved(st, true);
        if (st->uniformSpacingEnabled)
            InvalidateRect(hWnd, &st->canvasRc, FALSE);
    }
}

static HWND ResolveAppMainWindow(HWND hWnd)
{
    if (!hWnd) return nullptr;

    auto isMainAppWindow = [](HWND w) -> bool
    {
        if (!w || !IsWindow(w)) return false;

        wchar_t cls[128]{};
        GetClassNameW(w, cls, (int)(sizeof(cls) / sizeof(cls[0])));
        if (_wcsicmp(cls, L"WootingVigemGui") == 0)
            return true;

        HWND page = FindWindowExW(w, nullptr, L"PageMainClass", nullptr);
        return (page != nullptr);
    };

    HWND rootOwner = GetAncestor(hWnd, GA_ROOTOWNER);
    if (isMainAppWindow(rootOwner))
        return rootOwner;

    HWND root = GetAncestor(hWnd, GA_ROOT);
    if (isMainAppWindow(root))
        return root;

    // Detached editor fallback: locate main app window by class name.
    HWND byClass = FindWindowW(L"WootingVigemGui", nullptr);
    if (isMainAppWindow(byClass))
        return byClass;

    return nullptr;
}

static void Layout_RequestSave(HWND hWnd)
{
    HWND root = ResolveAppMainWindow(hWnd);
    if (root) PostMessageW(root, WM_APP_REQUEST_SAVE, 0, 0);
}

static void Layout_RebindDraftLabels(LayoutPageState* st)
{
    if (!st) return;
    if (st->draftLabels.size() < st->draftKeys.size())
        st->draftLabels.resize(st->draftKeys.size());
    for (size_t i = 0; i < st->draftKeys.size(); ++i)
        st->draftKeys[i].label = st->draftLabels[i].c_str();
}

static int Layout_DraftCount(const LayoutPageState* st)
{
    return st ? (int)st->draftKeys.size() : 0;
}

static bool Layout_DraftGet(const LayoutPageState* st, int idx, KeyDef& out)
{
    if (!st) return false;
    if (idx < 0 || idx >= (int)st->draftKeys.size()) return false;
    out = st->draftKeys[idx];
    return true;
}

static bool Layout_LoadDraftFromPreset(HWND hWnd, LayoutPageState* st, int presetIdx, bool clearUnsaved)
{
    if (!st) return false;

    std::vector<KeyDef> keys;
    std::vector<std::wstring> labels;
    bool uniformSpacing = false;
    int uniformGap = 8;
    if (!KeyboardLayout_GetPresetSnapshot(presetIdx, keys, labels, &uniformSpacing, &uniformGap))
        return false;

    st->editingPresetIdx = presetIdx;
    st->draftKeys = std::move(keys);
    st->draftLabels = std::move(labels);
    st->uniformSpacingEnabled = uniformSpacing;
    st->uniformSpacingGap = std::clamp(uniformGap, 0, 120);
    Layout_RebindDraftLabels(st);
    Layout_UpdateUniformSpacingButton(st);
    Layout_UpdateHintText(st);

    st->selectedIdx = -1;
    st->view.ready = false;
    st->preciseY = std::any_of(st->draftKeys.begin(), st->draftKeys.end(), [](const KeyDef& k) { return k.y >= 0; });
    st->guides.clear(); st->draggingGuide = -1;
    st->history.Reset(Layout_CaptureDraft(st));
    Layout_RefreshSelection(hWnd, st);
    st->previewHash = Layout_ComputePreviewHash(st);
    if (clearUnsaved)
        Layout_SetUnsaved(st, false);
    InvalidateRect(hWnd, nullptr, FALSE);
    return true;
}

static void Layout_SetUnsaved(LayoutPageState* st, bool on)
{
    if (!st) return;
    if (on)
    {
        std::vector<KeyDef> saved;
        std::vector<std::wstring> labels;
        bool uniform = false;
        int gap = 8;
        if (KeyboardLayout_GetPresetSnapshot(st->editingPresetIdx, saved, labels, &uniform, &gap))
        {
            on = saved.size() != st->draftKeys.size() || labels != st->draftLabels ||
                uniform != st->uniformSpacingEnabled || gap != st->uniformSpacingGap;
            for (size_t i = 0; !on && i < saved.size(); ++i)
            {
                const auto& a = saved[i];
                const auto& b = st->draftKeys[i];
                on = a.hid != b.hid || a.row != b.row || a.x != b.x || a.w != b.w || a.h != b.h ||
                    a.notchW != b.notchW || a.notchY != b.notchY ||
                    KeyboardLayout_KeyY(a) != KeyboardLayout_KeyY(b);
            }
        }
    }
    st->hasUnsaved = on;
    if (!st->restoringHistory)
    {
        HWND focus = GetFocus();
        const bool field = focus && (focus == st->edtLabel || focus == st->edtPos || focus == st->edtY ||
            focus == st->edtWidth || focus == st->edtHeight || focus == st->edtUniformGap ||
            focus == st->edtNotchW || focus == st->edtNotchY);
        st->history.Record(Layout_CaptureDraft(st), field ? (uintptr_t)focus : 0);
    }
    if (st->btnUndo) EnableWindow(st->btnUndo, st->history.CanUndo());
    if (st->btnRedo) EnableWindow(st->btnRedo, st->history.CanRedo());
    Layout_SetWindowTextIfChanged(st->lblStatus, on ? L"Unsaved changes" : L"All changes saved");
    if (st->btnSave)
    {
        const HWND host = GetParent(GetParent(st->btnSave));
        std::wstring title = L"HallJoy - Layout Editor - ";
        title += KeyboardLayout_GetPresetDisplayName(st->editingPresetIdx);
        if (on) title += L" - Unsaved changes";
        Layout_SetWindowTextIfChanged(host, title.c_str());
    }
    if (st->btnSave && IsWindow(st->btnSave))
        EnableWindow(st->btnSave, on ? TRUE : FALSE);
    if (st->btnDelete && IsWindow(st->btnDelete))
        EnableWindow(st->btnDelete, st->selectedIdx >= 0 ? TRUE : FALSE);
}

static void Layout_UpdateMetaControls(LayoutPageState* st)
{
    if (!st) return;
    bool hasSel = (st->selectedIdx >= 0);
    HWND selectedControls[] = { st->lblLabel, st->edtLabel, st->lblPos, st->edtPos,
        st->lblY, st->edtY, st->lblWidth, st->edtWidth, st->lblHeight, st->edtHeight,
        st->btnBindKey, st->lblBindState, st->btnDelete, st->btnDuplicate };
    auto visible = [](HWND control, bool show) {
        if (control && ((GetWindowLongPtrW(control, GWL_STYLE) & WS_VISIBLE) != 0) != show)
            ShowWindow(control, show ? SW_SHOWNA : SW_HIDE);
    };
    for (HWND control : selectedControls) visible(control, hasSel);
    visible(st->lblEmpty, !hasSel);
    const bool shaped = hasSel && st->selectedIdx < (int)st->draftKeys.size() && st->draftKeys[st->selectedIdx].notchW > 0;
    for (HWND control : {st->lblNotchW, st->edtNotchW, st->lblNotchY, st->edtNotchY}) visible(control, shaped);
    if (shaped != st->shapeControlsVisible) {
        st->shapeControlsVisible = shaped;
        Layout_Arrange(GetParent(st->cmbPreset), st);
    }
    Layout_SetWindowTextIfChanged(st->lblWidth, shaped ? L"Width" : L"Width (px)");
    Layout_SetWindowTextIfChanged(st->lblHeight, shaped ? L"Height" : L"Height (px)");
    Layout_SetWindowTextIfChanged(st->btnPrecision, st->preciseY ? L"Vertical: exact pixels" : L"Vertical: keyboard rows");
    Layout_SetWindowTextIfChanged(st->btnSnap, st->snap ? L"Snap to 8 px: On" : L"Snap to 8 px: Off");
    Layout_SetWindowTextIfChanged(st->lblY, st->preciseY ? L"Y (px)" : L"Row");
    bool labelHasFocus = (st->edtLabel && GetFocus() == st->edtLabel);
    bool posHasFocus = (st->edtPos && GetFocus() == st->edtPos);
    bool widthHasFocus = (st->edtWidth && GetFocus() == st->edtWidth);
    bool heightHasFocus = (st->edtHeight && GetFocus() == st->edtHeight);
    bool uniformGapHasFocus = (st->edtUniformGap && GetFocus() == st->edtUniformGap);

    if (st->btnDelete && IsWindow(st->btnDelete))
        EnableWindow(st->btnDelete, hasSel ? TRUE : FALSE);
    if (st->btnBindKey && IsWindow(st->btnBindKey))
        EnableWindow(st->btnBindKey, hasSel ? TRUE : FALSE);
    if (st->edtLabel && IsWindow(st->edtLabel))
        EnableWindow(st->edtLabel, hasSel ? TRUE : FALSE);
    if (st->edtPos && IsWindow(st->edtPos))
        EnableWindow(st->edtPos, hasSel ? TRUE : FALSE);
    if (st->edtWidth && IsWindow(st->edtWidth))
        EnableWindow(st->edtWidth, hasSel ? TRUE : FALSE);
    if (st->edtHeight && IsWindow(st->edtHeight))
        EnableWindow(st->edtHeight, hasSel ? TRUE : FALSE);
    if (st->edtUniformGap && IsWindow(st->edtUniformGap))
        EnableWindow(st->edtUniformGap, st->uniformSpacingEnabled ? TRUE : FALSE);
    if (st->lblUniformGap && IsWindow(st->lblUniformGap))
        EnableWindow(st->lblUniformGap, st->uniformSpacingEnabled ? TRUE : FALSE);
    if (st->edtUniformGap && !uniformGapHasFocus)
    {
        wchar_t b[32]{};
        swprintf_s(b, L"%d", st->uniformSpacingGap);
        Layout_SetWindowTextIfChanged(st->edtUniformGap, b);
    }

    if (!hasSel)
    {
        if (st->edtLabel && !labelHasFocus) Layout_SetWindowTextIfChanged(st->edtLabel, L"");
        if (st->edtPos && !posHasFocus) Layout_SetWindowTextIfChanged(st->edtPos, L"");
        if (st->edtWidth && !widthHasFocus) Layout_SetWindowTextIfChanged(st->edtWidth, L"");
        if (st->edtHeight && !heightHasFocus) Layout_SetWindowTextIfChanged(st->edtHeight, L"");
        if (st->lblBindState) Layout_SetWindowTextIfChanged(st->lblBindState, L"");
        return;
    }

    KeyDef k{};
    if (Layout_DraftGet(st, st->selectedIdx, k))
    {
        if (GetFocus() != st->edtNotchW) Layout_SetWindowTextIfChanged(st->edtNotchW, std::to_wstring(k.notchW).c_str());
        if (GetFocus() != st->edtNotchY) Layout_SetWindowTextIfChanged(st->edtNotchY, std::to_wstring(k.notchY).c_str());
        if (st->edtY && GetFocus() != st->edtY)
            Layout_SetWindowTextIfChanged(st->edtY, std::to_wstring(st->preciseY ? KeyboardLayout_KeyY(k) : k.row + 1).c_str());
        if (st->edtLabel && !labelHasFocus)
            Layout_SetWindowTextIfChanged(st->edtLabel, (k.label && k.label[0]) ? k.label : L"");
        if (st->edtPos && !posHasFocus)
        {
            wchar_t b[32]{};
            std::vector<int> displayX;
            Layout_BuildDisplayXMap(st, displayX);
            swprintf_s(b, L"%d", displayX[st->selectedIdx]);
            Layout_SetWindowTextIfChanged(st->edtPos, b);
        }
        if (st->edtWidth && !widthHasFocus)
        {
            wchar_t b[32]{};
            swprintf_s(b, L"%d", k.w);
            Layout_SetWindowTextIfChanged(st->edtWidth, b);
        }
        if (st->edtHeight && !heightHasFocus)
        {
            wchar_t b[32]{};
            swprintf_s(b, L"%d", std::max(KEYBOARD_KEY_MIN_DIM, k.h));
            Layout_SetWindowTextIfChanged(st->edtHeight, b);
        }
        if (st->lblBindState)
        {
            wchar_t s[96]{};
            swprintf_s(s, k.hid ? L"Key assigned  |  Press depth: %u%%" : L"No physical key assigned", (unsigned)BackendUI_GetRawMilli(k.hid) / 10);
            Layout_SetWindowTextIfChanged(st->lblBindState, s);
        }
    }
}

static void Layout_UpdateBindStateOnly(LayoutPageState* st)
{
    if (!st || !st->lblBindState) return;
    if (st->selectedIdx < 0)
    {
        Layout_SetWindowTextIfChanged(st->lblBindState, L"");
        return;
    }

    KeyDef k{};
    if (!Layout_DraftGet(st, st->selectedIdx, k))
    {
        Layout_SetWindowTextIfChanged(st->lblBindState, L"");
        return;
    }

    wchar_t s[96]{};
    swprintf_s(s, k.hid ? L"Key assigned  |  Press depth: %u%%" : L"No physical key assigned", (unsigned)BackendUI_GetRawMilli(k.hid) / 10);
    Layout_SetWindowTextIfChanged(st->lblBindState, s);
}

static uint32_t Layout_ComputePreviewHash(LayoutPageState* st)
{
    if (!st) return 0;
    uint32_t h = 2166136261u;
    for (const KeyDef& k : st->draftKeys)
    {
        if (!halljoy::keycode::IsSupported(k.hid))
            continue;
        uint32_t raw = (uint32_t)BackendUI_GetRawMilli(k.hid);
        uint32_t v = ((uint32_t)k.hid << 16) ^ raw;
        h ^= v;
        h *= 16777619u;
    }
    return h;
}

static void Layout_StopBindCapture(HWND hWnd, LayoutPageState* st, const wchar_t* statusText = nullptr)
{
    if (!st) return;
    st->bindArmed = false;
    BackendUI_SetBindCapture(false);
    if (st->btnBindKey && IsWindow(st->btnBindKey))
        Layout_SetWindowTextIfChanged(st->btnBindKey, L"Bind Physical Key");
    if (st->lblBindState && statusText)
        Layout_SetWindowTextIfChanged(st->lblBindState, statusText);
}

static void Layout_StartBindCapture(HWND hWnd, LayoutPageState* st)
{
    if (!st || st->selectedIdx < 0) return;
    st->bindArmed = true;
    BackendUI_SetBindCapture(true);
    if (st->btnBindKey && IsWindow(st->btnBindKey))
        Layout_SetWindowTextIfChanged(st->btnBindKey, L"Press Physical Key...");
    if (st->lblBindState)
        Layout_SetWindowTextIfChanged(st->lblBindState, L"Waiting for key press...");
}

static void Layout_ApplyLabelFromEdit(HWND hWnd, LayoutPageState* st)
{
    if (!st || st->selectedIdx < 0 || !st->edtLabel) return;
    wchar_t txt[64]{};
    GetWindowTextW(st->edtLabel, txt, (int)(sizeof(txt) / sizeof(txt[0])));
    if (st->selectedIdx >= (int)st->draftLabels.size()) return;

    if (st->draftLabels[st->selectedIdx] == (txt[0] ? txt : L"Key")) return;
    st->draftLabels[st->selectedIdx] = (txt[0] ? txt : L"Key");
    Layout_RebindDraftLabels(st);
    Layout_RefreshSelection(hWnd, st);
    Layout_SetUnsaved(st, true);
    InvalidateRect(hWnd, nullptr, FALSE);
}

static void Layout_ApplyGeometryFromEdits(HWND hWnd, LayoutPageState* st, bool applyPos, bool applyWidth, bool applyHeight)
{
    if (!st || st->selectedIdx < 0) return;
    if (st->selectedIdx >= (int)st->draftKeys.size()) return;

    KeyDef& k = st->draftKeys[st->selectedIdx];
    bool changed = false;

    if (applyPos && st->edtPos)
    {
        int v = 0;
        if (Layout_ReadNumber(st->edtPos, 0, 4000, v))
        {
            std::vector<int> displayX;
            Layout_BuildDisplayXMap(st, displayX);
            if (displayX[st->selectedIdx] != v)
            {
                if (st->uniformSpacingEnabled)
                {
                    Layout_BakeUniformSpacingIntoDraft(st);
                    st->uniformSpacingEnabled = false;
                    Layout_UpdateUniformSpacingButton(st);
                }
                k.x = v;
                changed = true;
            }
        }
    }

    if (applyWidth && st->edtWidth)
    {
        int v = 0;
        if (Layout_ReadNumber(st->edtWidth, KEYBOARD_KEY_MIN_DIM, KEYBOARD_KEY_MAX_DIM, v))
        {
            if (k.w != v && v > k.notchW)
            {
                k.w = v;
                changed = true;
            }
        }
    }

    if (applyHeight && st->edtHeight)
    {
        int v = 0;
        if (Layout_ReadNumber(st->edtHeight, KEYBOARD_KEY_MIN_DIM, KEYBOARD_KEY_MAX_DIM, v))
        {
            if (k.h != v && v > k.notchY)
            {
                k.h = v;
                changed = true;
            }
        }
    }

    if (changed)
    {
        Layout_RefreshSelection(hWnd, st);
        Layout_SetUnsaved(st, true);
        InvalidateRect(hWnd, &st->canvasRc, FALSE);
    }
    Layout_UpdateMetaControls(st);
}

#if defined(HALLJOY_ANALOG_SIMULATOR)
static int g_layoutTestDecision = 0; // 0: real dialog; otherwise explicit test choice
#endif

static bool Layout_SaveDraft(HWND hWnd, LayoutPageState* st)
{
    if (!st) return true;
    if (!KeyboardLayout_StorePresetSnapshot(st->editingPresetIdx, st->draftKeys,
        st->draftLabels, true, st->uniformSpacingEnabled, st->uniformSpacingGap))
    {
        Layout_SetWindowTextIfChanged(st->lblHint,
            L"Could not save. Your changes are safe.\nCheck folder access and try again.");
        InvalidateRect(hWnd, nullptr, FALSE);
        return false;
    }
    Layout_NotifyMainPage(hWnd);
    Layout_RequestSave(hWnd);
    Layout_SetUnsaved(st, false);
    st->history.EndGroup();
    Layout_UpdateHintText(st);
    InvalidateRect(hWnd, nullptr, FALSE);
    return true;
}

static void Layout_UndoRedo(HWND hWnd, LayoutPageState* st, bool redo)
{
    if (!st || st->dragging || st->panning) return;
    SetFocus(hWnd);
    Layout_StopBindCapture(hWnd, st);
    const auto* entry = st->history.Step(redo);
    if (!entry) return;
    st->restoringHistory = true;
    st->draftKeys = entry->keys; st->draftLabels = entry->labels;
    st->uniformSpacingEnabled = entry->spacing; st->uniformSpacingGap = entry->gap;
    st->selectedIdx = entry->selected;
    if (std::any_of(st->draftKeys.begin(), st->draftKeys.end(), [](const KeyDef& k) { return k.y >= 0; })) st->preciseY = true;
    Layout_RebindDraftLabels(st);
    Layout_UpdateUniformSpacingButton(st);
    Layout_RefreshSelection(hWnd, st);
    Layout_SetUnsaved(st, true);
    st->restoringHistory = false;
    InvalidateRect(hWnd, nullptr, FALSE);
}

static INT_PTR CALLBACK Layout_SaveDialogProc(HWND dialog, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg) {
    case WM_INITDIALOG: {
        UiTheme::ApplyToTopLevelWindow(dialog);
        SetWindowTextW(dialog, L"HallJoy - Layout Editor");
        const int width = S(dialog, 520), height = S(dialog, 202);
        RECT frame{0, 0, width, height};
        AdjustWindowRectEx(&frame, (DWORD)GetWindowLongPtrW(dialog, GWL_STYLE), FALSE,
            (DWORD)GetWindowLongPtrW(dialog, GWL_EXSTYLE));
        RECT owner{}; GetWindowRect(GetParent(dialog), &owner);
        MONITORINFO monitor{sizeof(monitor)};
        GetMonitorInfoW(MonitorFromWindow(GetParent(dialog), MONITOR_DEFAULTTONEAREST), &monitor);
        const int outerW = frame.right - frame.left, outerH = frame.bottom - frame.top;
        const int x = std::clamp((owner.left + owner.right - outerW) / 2,
            monitor.rcWork.left, std::max(monitor.rcWork.left, monitor.rcWork.right - outerW));
        const int y = std::clamp((owner.top + owner.bottom - outerH) / 2,
            monitor.rcWork.top, std::max(monitor.rcWork.top, monitor.rcWork.bottom - outerH));
        SetWindowPos(dialog, nullptr, x, y, outerW, outerH, SWP_NOZORDER | SWP_NOACTIVATE);
        auto control = [&](const wchar_t* cls, const wchar_t* text, DWORD style,
            int id, int left, int top, int w, int h) {
            HWND child = CreateWindowExW(0, cls, text, WS_CHILD | WS_VISIBLE | style,
                S(dialog, left), S(dialog, top), S(dialog, w), S(dialog, h), dialog,
                (HMENU)(INT_PTR)id, GetModuleHandleW(nullptr), nullptr);
            SendMessageW(child, WM_SETFONT, (WPARAM)GetStockObject(SYSTEM_FONT), FALSE);
            return child;
        };
        control(L"STATIC", L"Save changes to this layout?", SS_LEFT, 101, 24, 20, 472, 28);
        control(L"STATIC", (const wchar_t*)lParam, SS_LEFT | SS_ENDELLIPSIS | SS_NOPREFIX,
            102, 24, 58, 472, 24);
        control(L"STATIC", L"This layout is shared by HallJoy and Input Overlay.", SS_LEFT,
            103, 24, 94, 472, 38);
        control(L"BUTTON", L"Save changes", BS_OWNERDRAW | WS_TABSTOP, IDYES, 24, 150, 152, 32);
        control(L"BUTTON", L"Discard changes", BS_OWNERDRAW | WS_TABSTOP, IDNO, 184, 150, 168, 32);
        HWND cancel = control(L"BUTTON", L"Cancel", BS_OWNERDRAW | WS_TABSTOP, IDCANCEL, 360, 150, 136, 32);
        SendMessageW(dialog, DM_SETDEFID, IDCANCEL, 0);
        SetFocus(cancel);
#if defined(HALLJOY_ANALOG_SIMULATOR)
        // Exercise the real modal dialog in production-linked tests, without human input.
        if (g_layoutTestDecision) PostMessageW(dialog, WM_COMMAND, g_layoutTestDecision, 0);
#endif
        return FALSE;
    }
    case WM_CTLCOLORDLG:
        return (INT_PTR)UiTheme::Brush_WindowBg();
    case WM_CTLCOLORSTATIC:
        SetBkMode((HDC)wParam, TRANSPARENT);
        SetTextColor((HDC)wParam, GetDlgCtrlID((HWND)lParam) == 103
            ? UiTheme::Color_TextMuted() : UiTheme::Color_Text());
        return (INT_PTR)UiTheme::Brush_WindowBg();
    case WM_DRAWITEM: {
        const auto* item = (const DRAWITEMSTRUCT*)lParam;
        if (!item || item->CtlType != ODT_BUTTON) return FALSE;
        Graphics g(item->hDC); g.SetSmoothingMode(SmoothingModeAntiAlias);
        HGDIOBJ oldFont = SelectObject(item->hDC, GetStockObject(SYSTEM_FONT));
        FillRect(item->hDC, &item->rcItem, UiTheme::Brush_WindowBg());
        wchar_t label[64]{}; GetWindowTextW(item->hwndItem, label, 64);
        if (item->CtlID == IDYES) {
            CustomPage_DrawRoundRect(g, item->rcItem,
                (item->itemState & ODS_SELECTED) ? RGB(32, 68, 94) : RGB(40, 86, 117),
                UiTheme::Color_Accent(), 5.0f);
            CustomPage_DrawText(item->hDC, label, item->rcItem, UiTheme::Color_Text(), DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        } else CustomPage_DrawButton(g, item->hDC, item->rcItem, label,
            (item->itemState & ODS_HOTLIGHT) != 0, (item->itemState & ODS_SELECTED) != 0, true);
        if (item->itemState & ODS_FOCUS) {
            RECT focus = item->rcItem; InflateRect(&focus, -4, -4); DrawFocusRect(item->hDC, &focus);
        }
        SelectObject(item->hDC, oldFont);
        return TRUE;
    }
    case WM_CLOSE:
        EndDialog(dialog, IDCANCEL); return TRUE;
    case WM_COMMAND:
        if (LOWORD(wParam) == IDYES || LOWORD(wParam) == IDNO || LOWORD(wParam) == IDCANCEL) {
            EndDialog(dialog, LOWORD(wParam)); return TRUE;
        }
        break;
    }
    return FALSE;
}

static bool Layout_ResolveDraft(HWND hWnd, LayoutPageState* st, bool saveWithoutPrompt)
{
    if (!st) return true;
    if (st->resolvingDraft) return false;
    // Capture loss must not leave a moved key outside dirty tracking.
    st->dragging = false;
    if (st->dirty) Layout_RefreshSelection(hWnd, st);
    st->dirty = false;
    if (GetCapture() == hWnd) ReleaseCapture();
    Layout_StopBindCapture(hWnd, st);
    Layout_SetUnsaved(st, true);
    if (!st->hasUnsaved) return true;
    if (saveWithoutPrompt) return Layout_SaveDraft(hWnd, st);
    st->resolvingDraft = true;
    int choice = IDCANCEL;
    {
        std::wstring content = L"Layout: ";
        content += KeyboardLayout_GetPresetDisplayName(st->editingPresetIdx);
        struct alignas(DWORD) EmptyDialog {
            DLGTEMPLATE dialog;
            WORD menu, windowClass, title;
        } definition{};
        definition.dialog.style = WS_POPUP | WS_CAPTION | WS_SYSMENU | DS_MODALFRAME;
        definition.dialog.cx = 320; definition.dialog.cy = 130;
        const INT_PTR result = DialogBoxIndirectParamW(GetModuleHandleW(nullptr), &definition.dialog,
            GetAncestor(hWnd, GA_ROOT), Layout_SaveDialogProc, (LPARAM)content.c_str());
        if (result == IDYES || result == IDNO) choice = (int)result;
    }
    st->resolvingDraft = false;
    if (choice == IDYES) return Layout_SaveDraft(hWnd, st);
    return choice == IDNO;
}

static void Layout_NotifyMainPage(HWND hWnd)
{
    HWND page = nullptr;

    HWND tab = GetParent(hWnd);
    if (tab) page = GetParent(tab);

    if (!page)
    {
        HWND root = ResolveAppMainWindow(hWnd);
        if (root)
            page = FindWindowExW(root, nullptr, L"PageMainClass", nullptr);
    }
    if (page) PostMessageW(page, WM_APP_KEYBOARD_LAYOUT_CHANGED, 0, 0);

    HWND root = ResolveAppMainWindow(hWnd);
    if (root) PostMessageW(root, WM_APP_KEYBOARD_LAYOUT_CHANGED, 0, 0);
}

static void Layout_RefreshPresetCombo(LayoutPageState* st)
{
    st->layoutPicker.Refresh(st->editingPresetIdx, true);
    Layout_Arrange(GetParent(st->cmbPreset), st);
}

static bool LayoutEditor_DeletePreset(int idx, bool keepEditor = false);

static void Layout_ComputeCanvasRect(HWND hWnd, LayoutPageState* st)
{
    RECT rc{}; GetClientRect(hWnd, &rc);
    st->canvasRc = RECT{ S(hWnd, 16), S(hWnd, 152),
        std::max(S(hWnd, 100), (int)rc.right - S(hWnd, 304)), (int)rc.bottom - S(hWnd, 66) };
    const int ruler = S(hWnd, 32);
    st->canvasRc.left += ruler; st->canvasRc.top += ruler;
    st->topRuler = {st->canvasRc.left, st->canvasRc.top - ruler, st->canvasRc.right, st->canvasRc.top};
    st->leftRuler = {st->canvasRc.left - ruler, st->canvasRc.top, st->canvasRc.left, st->canvasRc.bottom};
}

static void Layout_BuildDisplayXMap(const LayoutPageState* st, std::vector<int>& outDisplayX)
{
    outDisplayX.clear();
    if (!st) return;

    int n = (int)st->draftKeys.size();
    outDisplayX.resize((size_t)n, 0);
    for (int i = 0; i < n; ++i)
        outDisplayX[(size_t)i] = st->draftKeys[(size_t)i].x;

    if (!st->uniformSpacingEnabled || n <= 0) return;

    int kUniformGap = std::clamp(st->uniformSpacingGap, 0, 120);
    for (int row = 0; row <= 20; ++row)
    {
        std::vector<int> ids;
        ids.reserve((size_t)n);
        for (int i = 0; i < n; ++i)
        {
            if (st->draftKeys[(size_t)i].row == row)
                ids.push_back(i);
        }
        if (ids.empty()) continue;

        std::sort(ids.begin(), ids.end(), [&](int a, int b)
        {
            const KeyDef& ka = st->draftKeys[(size_t)a];
            const KeyDef& kb = st->draftKeys[(size_t)b];
            if (ka.x != kb.x) return ka.x < kb.x;
            return a < b;
        });

        int x = st->draftKeys[(size_t)ids[0]].x;
        for (int id : ids)
        {
            outDisplayX[(size_t)id] = x;
            x += st->draftKeys[(size_t)id].w + kUniformGap;
        }
    }
}

static void Layout_ComputeTransform(LayoutPageState* st, const RECT& canvas, const std::vector<int>* pDisplayX, float& scale, float& ox, float& oy)
{
    if (!st)
    {
        scale = 1.0f;
        ox = (float)canvas.left;
        oy = (float)canvas.top;
        return;
    }

    if (st->view.ready)
    {
        scale = st->view.scale; ox = canvas.left + st->view.x; oy = canvas.top + st->view.y;
        return;
    }

    int maxX = 1;
    int maxBottom = KEYBOARD_KEY_H;
    for (size_t i = 0; i < st->draftKeys.size(); ++i)
    {
        const KeyDef& k = st->draftKeys[i];
        int x = k.x;
        if (pDisplayX && i < pDisplayX->size())
            x = (*pDisplayX)[i];
        maxX = std::max(maxX, x + k.w);
        maxBottom = std::max(maxBottom, KeyboardLayout_KeyY(k) + std::max(KEYBOARD_KEY_MIN_DIM, k.h));
    }

    int modelW = KEYBOARD_MARGIN_X + maxX + KEYBOARD_MARGIN_X;
    int modelH = KEYBOARD_MARGIN_Y + maxBottom + KEYBOARD_MARGIN_Y;

    float cw = (float)(canvas.right - canvas.left);
    float ch = (float)(canvas.bottom - canvas.top);
    float sx = cw / (float)std::max(1, modelW);
    float sy = ch / (float)std::max(1, modelH);
    scale = std::max(0.1f, std::min(sx, sy));

    float drawW = (float)modelW * scale;
    float drawH = (float)modelH * scale;

    ox = (float)canvas.left + (cw - drawW) * 0.5f;
    oy = (float)canvas.top + (ch - drawH) * 0.5f;
    st->view.Fit((float)modelW, (float)modelH, cw, ch);
    scale = st->view.scale; ox = canvas.left + st->view.x; oy = canvas.top + st->view.y;
}

static RECT Layout_KeyRectOnCanvasFast(LayoutPageState* st, int idx, const std::vector<int>* pDisplayX, float scale, float ox, float oy)
{
    RECT r{};
    KeyDef k{};
    if (!Layout_DraftGet(st, idx, k)) return r;

    int modelX = k.x;
    if (pDisplayX && idx >= 0 && (size_t)idx < pDisplayX->size())
        modelX = (*pDisplayX)[(size_t)idx];

    int x = (int)std::lround(ox + (KEYBOARD_MARGIN_X + modelX) * scale);
    int y = (int)std::lround(oy + (KEYBOARD_MARGIN_Y + KeyboardLayout_KeyY(k)) * scale);
    int w = std::max(1, (int)std::lround(k.w * scale));
    int h = std::max(1, (int)std::lround(std::max(KEYBOARD_KEY_MIN_DIM, k.h) * scale));
    r = RECT{ x, y, x + w, y + h };
    return r;
}

static RECT Layout_KeyRectOnCanvas(LayoutPageState* st, int idx, const RECT& canvas)
{
    std::vector<int> displayX;
    Layout_BuildDisplayXMap(st, displayX);
    const std::vector<int>* pDisplayX = displayX.empty() ? nullptr : &displayX;
    float scale = 1.0f, ox = 0.0f, oy = 0.0f;
    Layout_ComputeTransform(st, canvas, pDisplayX, scale, ox, oy);
    return Layout_KeyRectOnCanvasFast(st, idx, pDisplayX, scale, ox, oy);
}

static int Layout_ResizeEdges(LayoutPageState* st, POINT point)
{
    if (st->selectedIdx < 0) return 0;
    const RECT r = Layout_KeyRectOnCanvas(st, st->selectedIdx, st->canvasRc);
    if (r.right - r.left < 18 || r.bottom - r.top < 18) return 0;
    RECT hit = r; InflateRect(&hit, 5, 5);
    if (!PtInRect(&hit, point)) return 0;
    const KeyDef& shape = st->draftKeys[st->selectedIdx];
    if (shape.notchW && point.x < r.left + (r.right-r.left)*shape.notchW/shape.w - 7 &&
        point.y > r.top + (r.bottom-r.top)*shape.notchY/shape.h + 7) return 0;
    using namespace halljoy::layout_editor;
    int edges = 0;
    if (std::abs(point.x - r.left) <= 7) edges |= Left;
    else if (std::abs(point.x - r.right) <= 7) edges |= Right;
    if (std::abs(point.y - r.top) <= 7 && st->preciseY) edges |= Top;
    else if (std::abs(point.y - r.bottom) <= 7) edges |= Bottom;
    return edges;
}

static int Layout_HitGuide(LayoutPageState* st, POINT point)
{
    if (!PtInRect(&st->canvasRc, point)) return -1;
    float scale, ox, oy; Layout_ComputeTransform(st, st->canvasRc, nullptr, scale, ox, oy);
    float nearest = 5.0f; int found = -1;
    for (int i = 0; i < (int)st->guides.size(); ++i) {
        const auto& guide = st->guides[i];
        const float position = guide.vertical ? ox + (KEYBOARD_MARGIN_X + guide.coordinate) * scale
            : oy + (KEYBOARD_MARGIN_Y + guide.coordinate) * scale;
        const float distance = std::abs(position - (guide.vertical ? point.x : point.y));
        if (distance <= nearest) { nearest = distance; found = i; }
    }
    return found;
}

static void Layout_MoveGuide(HWND hWnd, LayoutPageState* st, POINT point)
{
    float scale, ox, oy; Layout_ComputeTransform(st, st->canvasRc, nullptr, scale, ox, oy);
    auto& guide = st->guides[st->draggingGuide];
    guide.coordinate = std::clamp((int)std::lround(guide.vertical
        ? (point.x - ox) / scale - KEYBOARD_MARGIN_X
        : (point.y - oy) / scale - KEYBOARD_MARGIN_Y), 0, 4600);
    InvalidateRect(hWnd, nullptr, FALSE);
}

static int Layout_HitTestKey(LayoutPageState* st, POINT pt)
{
    std::vector<int> displayX;
    Layout_BuildDisplayXMap(st, displayX);
    const std::vector<int>* pDisplayX = displayX.empty() ? nullptr : &displayX;
    float scale = 1.0f, ox = 0.0f, oy = 0.0f;
    Layout_ComputeTransform(st, st->canvasRc, pDisplayX, scale, ox, oy);

    int n = Layout_DraftCount(st);
    for (int i = n - 1; i >= 0; --i)
    {
        RECT r = Layout_KeyRectOnCanvasFast(st, i, pDisplayX, scale, ox, oy);
        if (PtInRect(&r, pt) && KeyboardLayout_Contains(st->draftKeys[i],
                (double)(pt.x-r.left)*st->draftKeys[i].w/(r.right-r.left),
                (double)(pt.y-r.top)*st->draftKeys[i].h/(r.bottom-r.top))) return i;
    }
    return -1;
}

static void Layout_RefreshSelection(HWND, LayoutPageState* st)
{
    if (!st) return;
    if (st->selectedIdx >= (int)st->draftKeys.size()) st->selectedIdx = -1;
    Layout_UpdateMetaControls(st);
}

static void Layout_DrawFlatButton(const DRAWITEMSTRUCT* dis)
{
    if (!dis) return;

    RECT rc = dis->rcItem;
    HDC hdc = dis->hDC;

    bool disabled = (dis->itemState & ODS_DISABLED) != 0;
    bool pressed = (dis->itemState & ODS_SELECTED) != 0;
    bool hot = (dis->itemState & ODS_HOTLIGHT) != 0;

    COLORREF bg = UiTheme::Color_ControlBg();
    if (pressed)
        bg = RGB(42, 42, 44);
    else if (hot)
        bg = RGB(40, 40, 42);

    HBRUSH br = CreateSolidBrush(bg);
    FillRect(hdc, &rc, br);
    DeleteObject(br);

    HPEN pen = CreatePen(PS_SOLID, 1, UiTheme::Color_Border());
    HGDIOBJ oldPen = SelectObject(hdc, pen);
    HGDIOBJ oldBrush = SelectObject(hdc, GetStockObject(HOLLOW_BRUSH));
    Rectangle(hdc, rc.left, rc.top, rc.right, rc.bottom);
    SelectObject(hdc, oldBrush);
    SelectObject(hdc, oldPen);
    DeleteObject(pen);

    wchar_t text[128]{};
    GetWindowTextW(dis->hwndItem, text, (int)(sizeof(text) / sizeof(text[0])));

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, disabled ? UiTheme::Color_TextMuted() : UiTheme::Color_Text());
    DrawTextW(hdc, text, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

    if (dis->itemState & ODS_FOCUS)
    {
        RECT focus = rc;
        InflateRect(&focus, -4, -4);
        DrawFocusRect(hdc, &focus);
    }
}

static void Layout_DrawCanvas(HWND hWnd, HDC hdc, LayoutPageState* st)
{
    if (!st) return;
    Layout_ComputeCanvasRect(hWnd, st);
    std::vector<int> displayX;
    Layout_BuildDisplayXMap(st, displayX);
    const std::vector<int>* pDisplayX = displayX.empty() ? nullptr : &displayX;
    float scale = 1.0f, ox = 0.0f, oy = 0.0f;
    Layout_ComputeTransform(st, st->canvasRc, pDisplayX, scale, ox, oy);

    Graphics g(hdc);
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    g.SetPixelOffsetMode(PixelOffsetModeHighQuality);
    g.SetTextRenderingHint(TextRenderingHintAntiAliasGridFit);

    RectF canvas((REAL)st->canvasRc.left, (REAL)st->canvasRc.top,
        (REAL)(st->canvasRc.right - st->canvasRc.left), (REAL)(st->canvasRc.bottom - st->canvasRc.top));
    g.SetClip(canvas);

    SolidBrush bg(Gp(RGB(28, 28, 30)));
    g.FillRectangle(&bg, canvas);

    // World-anchored pixel grid. Line widths remain in screen pixels at every zoom.
    {
        const auto grid = halljoy::layout_editor::GridForScale(scale);
        Pen minorPen(Gp(RGB(100, 100, 110), (BYTE)std::lround(36 * grid.minorOpacity)), 0.7f);
        Pen majorPen(Gp(RGB(100, 100, 110), 85), 1.1f);
        const float gridX = ox + KEYBOARD_MARGIN_X * scale;
        const float gridY = oy + KEYBOARD_MARGIN_Y * scale;
        auto drawAxis = [&](bool vertical, int stride, Pen& pen, bool minor) {
            const float origin = vertical ? gridX : gridY;
            const float lo = vertical ? canvas.X : canvas.Y;
            const float hi = vertical ? canvas.GetRight() : canvas.GetBottom();
            const int first = (int)std::ceil((lo - origin) / (stride * scale));
            const int last = (int)std::floor((hi - origin) / (stride * scale));
            for (int index = first; index <= last; ++index) {
                const int coordinate = index * stride;
                if (minor && coordinate % grid.majorStep == 0) continue;
                const float position = origin + coordinate * scale;
                if (vertical) g.DrawLine(&pen, position, canvas.Y, position, canvas.GetBottom());
                else g.DrawLine(&pen, canvas.X, position, canvas.GetRight(), position);
            }
        };
        if (grid.minorOpacity > 0) {
            drawAxis(true, 1, minorPen, true);
            drawAxis(false, 1, minorPen, true);
        }
        drawAxis(true, grid.majorStep, majorPen, false);
        drawAxis(false, grid.majorStep, majorPen, false);
    }

    Pen border(Gp(UiTheme::Color_Border()), 1.0f);
    g.DrawRectangle(&border, canvas);

    int n = Layout_DraftCount(st);
    for (int i = 0; i < n; ++i)
    {
        KeyDef k{};
        if (!Layout_DraftGet(st, i, k)) continue;

        RECT rr = Layout_KeyRectOnCanvasFast(st, i, pDisplayX, scale, ox, oy);
        RectF r((REAL)rr.left, (REAL)rr.top, (REAL)(rr.right - rr.left), (REAL)(rr.bottom - rr.top));
        r.Inflate(-std::min(1.0f, r.Width * 0.1f), -std::min(1.0f, r.Height * 0.1f));

        bool sel = (i == st->selectedIdx);
        if (sel && st->dragging)
        {
            Pen guide(Gp(UiTheme::Color_Accent(), 130), 1.0f);
            guide.SetDashStyle(DashStyleDash);
            g.DrawLine(&guide, r.X, canvas.Y, r.X, canvas.GetBottom());
            g.DrawLine(&guide, canvas.X, r.Y, canvas.GetRight(), r.Y);
            g.DrawLine(&guide, r.GetRight(), canvas.Y, r.GetRight(), canvas.GetBottom());
            g.DrawLine(&guide, canvas.X, r.GetBottom(), canvas.GetRight(), r.GetBottom());
        }
        SolidBrush fill(sel ? Gp(UiTheme::Color_Accent(), 210) : Gp(RGB(48, 48, 52), 230));
        GraphicsPath shape;
        if (k.notchW) {
            const float nx = r.X + r.Width*k.notchW/k.w, ny = r.Y + r.Height*k.notchY/k.h;
            const PointF points[] = {{r.X,r.Y},{r.GetRight(),r.Y},{r.GetRight(),r.GetBottom()},
                {nx,r.GetBottom()},{nx,ny},{r.X,ny}};
            shape.AddPolygon(points, 6);
        } else shape.AddRectangle(r);
        g.FillPath(&fill, &shape);
        const auto shapeClip = g.Save();
        g.SetClip(&shape, CombineModeIntersect);

        // Live analog preview for bound HID keys (helps verify bind immediately).
        if (halljoy::keycode::IsSupported(k.hid))
        {
            float v = (float)BackendUI_GetRawMilli(k.hid) / 1000.0f;
            v = std::clamp(v, 0.0f, 1.0f);
            if (v > 0.001f)
            {
                RectF rf = r;
                rf.Height = r.Height * v;
                SolidBrush fb(Gp(UiTheme::Color_Accent(), 140));
                g.FillRectangle(&fb, rf);
            }
        }

        Pen keyBorder(sel ? Gp(RGB(245, 245, 245)) : Gp(UiTheme::Color_Border()), sel ? 2.0f : 1.0f);
        g.Restore(shapeClip);
        g.DrawPath(&keyBorder, &shape);
        if (sel && r.Width >= 16 && r.Height >= 16)
        {
            SolidBrush grip(Gp(UiTheme::Color_Text()));
            for (int column = 0; column < 3; ++column) for (int row = 0; row < 3; ++row) {
                if ((column == 1 && row == 1) || (row == 0 && !st->preciseY)) continue;
                const float x = r.X + column * r.Width / 2;
                const float y = r.Y + row * r.Height / 2;
                if (k.notchW && column == 0 && row * k.h / 2 >= k.notchY) continue;
                g.FillRectangle(&grip, x - 3, y - 3, 6.0f, 6.0f);
            }
        }

        if (k.label && k.label[0])
        {
            FontFamily ff(L"Segoe UI");
            float em = std::clamp(r.Height * 0.30f, 10.0f, 24.0f);
            Font font(&ff, em, FontStyleRegular, UnitPixel);
            StringFormat fmt;
            fmt.SetAlignment(StringAlignmentCenter);
            fmt.SetLineAlignment(StringAlignmentCenter);
            fmt.SetFormatFlags(StringFormatFlagsNoWrap);
            SolidBrush txt(sel ? Gp(RGB(12, 12, 12)) : Gp(UiTheme::Color_Text()));
            RectF labelRect = r;
            if (k.notchW) { const float offset = r.Width*k.notchW/k.w; labelRect.X += offset; labelRect.Width -= offset; }
            g.DrawString(k.label, -1, &font, labelRect, &fmt, &txt);
        }
    }
}

static void Layout_DrawRulersAndGuides(HWND hWnd, HDC hdc, LayoutPageState* st)
{
    float scale, ox, oy; Layout_ComputeTransform(st, st->canvasRc, nullptr, scale, ox, oy);
    Graphics g(hdc); g.SetSmoothingMode(SmoothingModeAntiAlias);
    g.SetClip(Rect(st->canvasRc.left, st->canvasRc.top,
        st->canvasRc.right - st->canvasRc.left, st->canvasRc.bottom - st->canvasRc.top));
    Pen guidePen(Gp(RGB(85, 190, 215), 170), 1.0f);
    for (const auto& guide : st->guides) {
        if (guide.vertical) {
            const float x = ox + (KEYBOARD_MARGIN_X + guide.coordinate) * scale;
            g.DrawLine(&guidePen, x, (float)st->canvasRc.top, x, (float)st->canvasRc.bottom);
        } else {
            const float y = oy + (KEYBOARD_MARGIN_Y + guide.coordinate) * scale;
            g.DrawLine(&guidePen, (float)st->canvasRc.left, y, (float)st->canvasRc.right, y);
        }
    }
    g.ResetClip();
    HGDIOBJ oldFont = SelectObject(hdc, GetStockObject(SYSTEM_FONT));
    int tickStep = 1;
    while (tickStep * scale < S(hWnd, 8)) tickStep *= 5;
    Pen tickPen(Gp(UiTheme::Color_TextMuted(), 140), 1.0f);
    for (bool vertical : {false, true}) {
        const RECT ruler = vertical ? st->leftRuler : st->topRuler;
        FillRect(hdc, &ruler, UiTheme::Brush_ControlBg());
        const int saved = SaveDC(hdc); IntersectClipRect(hdc, ruler.left, ruler.top, ruler.right, ruler.bottom);
        const float origin = vertical ? oy + KEYBOARD_MARGIN_Y * scale : ox + KEYBOARD_MARGIN_X * scale;
        const int lo = vertical ? ruler.top : ruler.left, hi = vertical ? ruler.bottom : ruler.right;
        const int first = (int)std::ceil((lo - origin) / (tickStep * scale));
        const int last = (int)std::floor((hi - origin) / (tickStep * scale));
        for (int i = first; i <= last; ++i) {
            const int coordinate = i * tickStep;
            const int position = (int)std::lround(origin + coordinate * scale);
            const bool major = i % 5 == 0;
            const int length = S(hWnd, major ? 9 : 4);
            if (vertical) g.DrawLine(&tickPen, ruler.right - length, position, ruler.right, position);
            else g.DrawLine(&tickPen, position, ruler.bottom - length, position, ruler.bottom);
            if (major) {
                const std::wstring label = std::to_wstring(coordinate);
                RECT text = vertical ? RECT{ruler.left, position - S(hWnd, 18), ruler.right - S(hWnd, 3), position}
                    : RECT{position + S(hWnd, 3), ruler.top, position + S(hWnd, 70), ruler.bottom - S(hWnd, 9)};
                CustomPage_DrawText(hdc, label, text, UiTheme::Color_TextMuted(), DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);
            }
        }
        RestoreDC(hdc, saved);
    }
    SelectObject(hdc, oldFont);
}

static void Layout_ApplyDrag(HWND hWnd, LayoutPageState* st, POINT point)
{
    if (!st || st->selectedIdx < 0) return;
    if (st->uniformSpacingEnabled) {
        std::vector<int> positions; Layout_BuildDisplayXMap(st, positions);
        if (std::any_of(positions.begin(), positions.end(), [](int x) { return x < 0 || x > 4000; })) {
            Layout_SetWindowTextIfChanged(st->lblHint,
                L"Automatic spacing exceeds the editable range.\nReduce spacing before dragging keys.");
            return;
        }
    }
    float scale, ox, oy;
    Layout_ComputeTransform(st, st->canvasRc, nullptr, scale, ox, oy);
    auto& key = st->draftKeys[st->selectedIdx];
    if (st->resizing)
    {
        using namespace halljoy::layout_editor;
        int dx = (int)std::lround((point.x - st->resizeStart.x) / scale);
        int dy = (int)std::lround((point.y - st->resizeStart.y) / scale);
        if (!(st->resizeEdges & (Left | Right))) dx = 0;
        if (!(st->resizeEdges & (Top | Bottom))) dy = 0;
        if (!dx && !dy && !st->dirty) return;
        const auto origin = st->resizeOrigin;
        if (!dx && !dy && key.x == origin.x && KeyboardLayout_KeyY(key) == origin.y &&
            key.w == origin.w && key.h == origin.h) return;
        const bool bypass = (GetKeyState(VK_MENU) & 0x8000) != 0;
        auto snapEdge = [&](int start, int delta, bool vertical) {
            int target = start + delta;
            if (!bypass) {
                if (st->snap) target = (int)std::lround(target / 8.0) * 8;
                target = SnapPosition(target, 0, st->guides, vertical, scale, 0, 4600);
            }
            return target - start;
        };
        dx = snapEdge(origin.x + ((st->resizeEdges & Right) ? origin.w : 0), dx, true);
        dy = snapEdge(origin.y + ((st->resizeEdges & Bottom) ? origin.h : 0), dy, false);
        auto geometry = Resize(origin, st->resizeEdges, dx, dy, st->preciseY);
        if (geometry.w <= key.notchW) {
            geometry.w = key.notchW + 1;
            if (st->resizeEdges & Left) geometry.x = origin.x + origin.w - geometry.w;
        }
        if (geometry.h <= key.notchY) {
            geometry.h = key.notchY + 1;
            if (st->resizeEdges & Top) geometry.y = origin.y + origin.h - geometry.h;
        }
        if (key.w != geometry.w || key.h != geometry.h || key.x != geometry.x || KeyboardLayout_KeyY(key) != geometry.y)
        {
            if (st->uniformSpacingEnabled) {
                Layout_BakeUniformSpacingIntoDraft(st); st->uniformSpacingEnabled = false;
                Layout_UpdateUniformSpacingButton(st);
            }
            key.x = geometry.x; key.w = geometry.w; key.h = geometry.h;
            if (st->resizeEdges & Top) key.y = geometry.y;
            st->dirty = true;
            Layout_UpdateMetaControls(st); InvalidateRect(hWnd, &st->canvasRc, FALSE);
        }
        return;
    }
    if (st->uniformSpacingEnabled)
    {
        Layout_BakeUniformSpacingIntoDraft(st);
        st->uniformSpacingEnabled = false;
        Layout_UpdateUniformSpacingButton(st);
        st->dirty = true;
    }
    int x = (int)std::lround((point.x - st->dragOffsetX - ox) / scale) - KEYBOARD_MARGIN_X;
    int y = (int)std::lround((point.y - st->dragOffsetY - oy) / scale) - KEYBOARD_MARGIN_Y;
    const bool bypass = (GetKeyState(VK_MENU) & 0x8000) != 0;
    if (st->snap && !bypass) { x = (int)std::lround(x / 8.0) * 8; if (st->preciseY) y = (int)std::lround(y / 8.0) * 8; }
    x = std::clamp(x, 0, 4000);
    const int offset = KeyboardLayout_KeyY(key) - key.row * KEYBOARD_ROW_PITCH_Y;
    int row = std::clamp((int)std::lround((y - offset) / (double)KEYBOARD_ROW_PITCH_Y), 0, 20);
    y = st->preciseY ? std::clamp(y, 0, 4000) : halljoy::layout_editor::RowY(key, row);
    if (!bypass) {
        x = halljoy::layout_editor::SnapPosition(x, key.w, st->guides, true, scale);
        y = halljoy::layout_editor::SnapPosition(y, key.h, st->guides, false, scale,
            st->preciseY ? 0 : std::max(0, offset),
            st->preciseY ? 4000 : std::min(4000, offset + 20 * KEYBOARD_ROW_PITCH_Y),
            offset, st->preciseY ? 0 : KEYBOARD_ROW_PITCH_Y);
        if (!st->preciseY) row = std::clamp((y - offset) / KEYBOARD_ROW_PITCH_Y, 0, 20);
    }
    if (key.x == x && KeyboardLayout_KeyY(key) == y) return;
    key.x = x; key.y = st->preciseY || key.y >= 0 ? y : -1;
    if (!st->preciseY) key.row = row;
    st->dirty = true;
    Layout_UpdateMetaControls(st);
    InvalidateRect(hWnd, &st->canvasRc, FALSE);
}

static void Layout_Arrange(HWND hWnd, LayoutPageState* st)
{
    RECT rc{}; GetClientRect(hWnd, &rc);
    const int m = S(hWnd, 16), gap = S(hWnd, 8), side = S(hWnd, 264);
    const int rightX = rc.right - side - m;
    bool lower = false;
    auto place = [&](HWND control, int x, int y, int w, int h) {
        if (lower) y += S(hWnd, 60);
        if (control) SetWindowPos(control, nullptr, x, y, w, h, SWP_NOZORDER | SWP_NOACTIVATE);
    };
    place(st->lblPreset, m, S(hWnd, 12), S(hWnd, 280), S(hWnd, 18));
    const int comboW = std::clamp((int)rc.right - S(hWnd, 560), S(hWnd, 180), S(hWnd, 300));
    int x = m;
    place(st->lblBrand, x, S(hWnd, 36), S(hWnd, 160), S(hWnd, 18));
    place(st->layoutPicker.brand, x, S(hWnd, 58), S(hWnd, 160), S(hWnd, 32));
    place(st->lblModel, x + S(hWnd, 160) + gap, S(hWnd, 36), comboW, S(hWnd, 18));
    place(st->cmbPreset, x + S(hWnd, 160) + gap, S(hWnd, 58), comboW, S(hWnd, 32));
    const int variantX = x + S(hWnd, 160) + gap + comboW + gap;
    place(st->lblVariant, variantX, S(hWnd, 36), S(hWnd, 96), S(hWnd, 18));
    place(st->layoutPicker.variant, variantX, S(hWnd, 58), S(hWnd, 96), S(hWnd, 32));
    ShowWindow(st->lblVariant, st->layoutPicker.HasVariants() ? SW_SHOWNA : SW_HIDE);
    ShowWindow(st->layoutPicker.variant, st->layoutPicker.HasVariants() ? SW_SHOWNA : SW_HIDE);
    for (HWND button : { st->btnUndo, st->btnRedo }) { place(button, x, S(hWnd, 102), S(hWnd, 64), S(hWnd, 32)); x += S(hWnd, 64) + gap; }
    place(st->btnFit, x, S(hWnd, 102), S(hWnd, 102), S(hWnd, 32)); x += S(hWnd, 102) + gap;
    place(st->btnAdd, x, S(hWnd, 102), S(hWnd, 88), S(hWnd, 32)); x += S(hWnd, 88) + gap;
    place(st->btnSave, x, S(hWnd, 102), S(hWnd, 116), S(hWnd, 32));
    place(st->lblStatus, rc.right - S(hWnd, 190), S(hWnd, 12), S(hWnd, 174), S(hWnd, 18));
    lower = true;
    place(st->lblKeys, rightX, S(hWnd, 100), side, S(hWnd, 22));
    place(st->lblEmpty, rightX, S(hWnd, 146), side, S(hWnd, 112));
    place(st->lblLabel, rightX, S(hWnd, 136), side, S(hWnd, 18));
    place(st->edtLabel, rightX, S(hWnd, 158), side, S(hWnd, 28));
    const int col = (side - gap) / 2;
    place(st->lblPos, rightX, S(hWnd, 200), col, S(hWnd, 18));
    place(st->lblY, rightX + col + gap, S(hWnd, 200), col, S(hWnd, 18));
    place(st->edtPos, rightX, S(hWnd, 222), col, S(hWnd, 28));
    place(st->edtY, rightX + col + gap, S(hWnd, 222), col, S(hWnd, 28));
    place(st->lblWidth, rightX, S(hWnd, 264), col, S(hWnd, 18));
    place(st->lblHeight, rightX + col + gap, S(hWnd, 264), col, S(hWnd, 18));
    place(st->edtWidth, rightX, S(hWnd, 286), col, S(hWnd, 28));
    place(st->edtHeight, rightX + col + gap, S(hWnd, 286), col, S(hWnd, 28));
    if (st->shapeControlsVisible) {
        const int quarter = (side - gap*3) / 4;
        const HWND labels[] = {st->lblWidth, st->lblHeight, st->lblNotchW, st->lblNotchY};
        const HWND edits[] = {st->edtWidth, st->edtHeight, st->edtNotchW, st->edtNotchY};
        for (int i = 0; i < 4; ++i) {
            place(labels[i], rightX + i*(quarter+gap), S(hWnd, 264), quarter, S(hWnd, 18));
            place(edits[i], rightX + i*(quarter+gap), S(hWnd, 286), quarter, S(hWnd, 28));
        }
    }
    place(st->btnBindKey, rightX, S(hWnd, 328), side, S(hWnd, 30));
    place(st->lblBindState, rightX, S(hWnd, 366), side, S(hWnd, 20));
    place(st->btnDuplicate, rightX, S(hWnd, 398), col, S(hWnd, 30));
    place(st->btnDelete, rightX + col + gap, S(hWnd, 398), col, S(hWnd, 30));
    place(st->lblLayoutTools, rightX, S(hWnd, 436), side, S(hWnd, 18));
    place(st->btnPrecision, rightX, S(hWnd, 460), side, S(hWnd, 30));
    place(st->btnSnap, rightX, S(hWnd, 498), side, S(hWnd, 30));
    place(st->btnUniformSpacing, rightX, S(hWnd, 536), side, S(hWnd, 30));
    place(st->lblUniformGap, rightX, S(hWnd, 578), S(hWnd, 164), S(hWnd, 24));
    place(st->edtUniformGap, rightX + side - S(hWnd, 80), S(hWnd, 574), S(hWnd, 80), S(hWnd, 28));
    lower = false;
    place(st->btnReset, rightX, std::max(S(hWnd, 678), (int)rc.bottom - S(hWnd, 48)), side, S(hWnd, 30));
    place(st->btnActual, rightX - S(hWnd, 104), rc.bottom - S(hWnd, 48), S(hWnd, 80), S(hWnd, 30));
    place(st->lblHint, m, rc.bottom - S(hWnd, 54), std::max(S(hWnd, 100), rightX - m - S(hWnd, 120)), S(hWnd, 46));
    Layout_ComputeCanvasRect(hWnd, st);
}

LRESULT CALLBACK KeyboardSubpages_LayoutPageProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    auto* st = (LayoutPageState*)GetWindowLongPtrW(hWnd, GWLP_USERDATA);
    if (st && msg == PremiumCombo::MsgItemTextCommit() && (HWND)lParam == st->cmbPreset) {
        wchar_t name[260]{}; PremiumCombo::ConsumeCommittedText(st->cmbPreset, name, 260);
        if (st->layoutPicker.PresetAt(LOWORD(wParam)) != LayoutPicker::Create || st->resolvingDraft) return 0;
        PremiumCombo::SetDeleteConfirmation(st->cmbPreset, -1);
        PremiumCombo::ShowDropDown(st->cmbPreset, false);
        if (!Layout_ResolveDraft(hWnd, st)) {
            Layout_RefreshPresetCombo(st); return 0;
        }
        int created = -1;
        if (KeyboardLayout_CreatePreset(name, &created, st->editingPresetIdx, false, st->layoutPicker.CreationBrand())) {
            Layout_LoadDraftFromPreset(hWnd, st, created, true);
            Layout_RefreshPresetCombo(st); Layout_NotifyMainPage(hWnd); Layout_RequestSave(hWnd);
        } else {
            Layout_RefreshPresetCombo(st);
            Layout_SetWindowTextIfChanged(st->lblHint, L"Could not create layout. Use a unique name\nand check that the layouts folder is writable.");
        }
        return 0;
    }
    if (st && msg == PremiumCombo::MsgItemButton() && ((HWND)lParam == st->cmbPreset || (HWND)lParam == st->layoutPicker.variant)) {
        if (st->resolvingDraft) return 0;
        const auto action = (PremiumCombo::ItemButtonKind)HIWORD(wParam);
        const int index = LOWORD(wParam);
        const HWND source = (HWND)lParam;
        const int preset = source == st->cmbPreset ? st->layoutPicker.PresetAt(index) : st->layoutPicker.VariantAt(index);
        if (source == st->cmbPreset && index < (int)st->layoutPicker.groups.size() && st->layoutPicker.groups[index].members.size()>1) return 0;
        if (preset < 0 || preset >= KeyboardLayout_GetPresetCount() || KeyboardLayout_GetPresetCount() <= 1) return 0;
        if (action == PremiumCombo::ItemButtonKind::Delete) {
            PremiumCombo::SetDeleteConfirmation(source, index); return 0;
        }
        if (action != PremiumCombo::ItemButtonKind::ConfirmDelete ||
            PremiumCombo::GetDeleteConfirmation(source) != index) return 0;
        PremiumCombo::SetDeleteConfirmation(source, -1);
        PremiumCombo::ShowDropDown(source, false);
        if (LayoutEditor_DeletePreset(preset, true)) {
            Layout_NotifyMainPage(hWnd); Layout_RequestSave(hWnd); Layout_UpdateHintText(st);
        }
        return 0;
    }
    if (msg == WM_SETCURSOR && st && (HWND)wParam == hWnd && LOWORD(lParam) == HTCLIENT)
    {
        POINT point{}; GetCursorPos(&point); ScreenToClient(hWnd, &point);
        if (PtInRect(&st->canvasRc, point))
        {
            using namespace halljoy::layout_editor;
            const int edges = st->resizing ? st->resizeEdges : st->dragging ? 0 : Layout_ResizeEdges(st, point);
            LPCWSTR cursor = IDC_ARROW;
            if ((edges & (Left | Right)) && (edges & (Top | Bottom)))
                cursor = (edges == (Left | Top) || edges == (Right | Bottom)) ? IDC_SIZENWSE : IDC_SIZENESW;
            else if (edges & (Left | Right)) cursor = IDC_SIZEWE;
            else if (edges & (Top | Bottom)) cursor = IDC_SIZENS;
            else if (st->panning || st->dragging) cursor = IDC_SIZEALL;
            else {
                const int guide = st->draggingGuide >= 0 ? st->draggingGuide : Layout_HitGuide(st, point);
                if (guide >= 0) cursor = st->guides[guide].vertical ? IDC_SIZEWE : IDC_SIZENS;
            }
            SetCursor(LoadCursorW(nullptr, cursor));
            return TRUE;
        }
        if (PtInRect(&st->topRuler, point) || PtInRect(&st->leftRuler, point)) {
            SetCursor(LoadCursorW(nullptr, IDC_CROSS)); return TRUE;
        }
    }

    switch (msg)
    {
    case WM_ERASEBKGND:
        return 1;

    case WM_PAINT:
    {
        PAINTSTRUCT ps{};
        HDC memDC = nullptr;
        HBITMAP bmp = nullptr;
        HGDIOBJ oldBmp = nullptr;
        BeginDoubleBufferPaint(hWnd, ps, memDC, bmp, oldBmp);

        Layout_DrawCanvas(hWnd, memDC, st);
        if (st) Layout_DrawRulersAndGuides(hWnd, memDC, st);

        EndDoubleBufferPaint(hWnd, ps, memDC, bmp, oldBmp);
        return 0;
    }

    case WM_CTLCOLORSTATIC:
    {
        HDC hdc = (HDC)wParam;
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, UiTheme::Color_TextMuted());
        return (LRESULT)UiTheme::Brush_PanelBg();
    }

    case WM_CTLCOLORLISTBOX:
    {
        HDC hdc = (HDC)wParam;
        SetBkMode(hdc, OPAQUE);
        SetBkColor(hdc, UiTheme::Color_ControlBg());
        SetTextColor(hdc, UiTheme::Color_Text());
        return (LRESULT)UiTheme::Brush_ControlBg();
    }

    case WM_CTLCOLOREDIT:
    {
        HDC hdc = (HDC)wParam;
        SetBkMode(hdc, OPAQUE);
        SetBkColor(hdc, UiTheme::Color_ControlBg());
        SetTextColor(hdc, UiTheme::Color_Text());
        return (LRESULT)UiTheme::Brush_ControlBg();
    }

    case WM_CREATE:
    {
        HINSTANCE hInst = (HINSTANCE)GetWindowLongPtrW(hWnd, GWLP_HINSTANCE);
        HFONT hFont = (HFONT)GetStockObject(SYSTEM_FONT);

        st = new LayoutPageState();
        SetWindowLongPtrW(hWnd, GWLP_USERDATA, (LONG_PTR)st);

        st->lblPreset = CreateWindowW(L"STATIC", L"Layout", WS_CHILD | WS_VISIBLE,
            0, 0, 10, 10, hWnd, nullptr, hInst, nullptr);
        SendMessageW(st->lblPreset, WM_SETFONT, (WPARAM)hFont, TRUE);

        st->cmbPreset = PremiumCombo::Create(hWnd, hInst, 0, 0, 10, 10, ID_LAYOUT_PRESET,
            WS_CHILD | WS_VISIBLE | WS_TABSTOP);
        PremiumCombo::SetFont(st->cmbPreset, hFont, true);
        st->layoutPicker.model = st->cmbPreset;
        st->layoutPicker.editable = true;
        st->layoutPicker.brand = PremiumCombo::Create(hWnd, hInst, 0, 0, 10, 10, ID_LAYOUT_BRAND,
            WS_CHILD | WS_VISIBLE | WS_TABSTOP);
        PremiumCombo::SetFont(st->layoutPicker.brand, hFont, false);
        st->layoutPicker.variant = PremiumCombo::Create(hWnd, hInst, 0, 0, 10, 10, ID_LAYOUT_VARIANT,
            WS_CHILD | WS_TABSTOP);
        PremiumCombo::SetFont(st->layoutPicker.variant, hFont, false);
        st->lblVariant = CreateWindowW(L"STATIC", L"Variant", WS_CHILD,
            0, 0, 10, 10, hWnd, nullptr, hInst, nullptr);
        SendMessageW(st->lblVariant, WM_SETFONT, (WPARAM)hFont, FALSE);
        st->lblBrand = CreateWindowW(L"STATIC", L"Brand", WS_CHILD | WS_VISIBLE,
            0, 0, 10, 10, hWnd, nullptr, hInst, nullptr);
        st->lblModel = CreateWindowW(L"STATIC", L"Model", WS_CHILD | WS_VISIBLE,
            0, 0, 10, 10, hWnd, nullptr, hInst, nullptr);
        SendMessageW(st->lblBrand, WM_SETFONT, (WPARAM)hFont, FALSE);
        SendMessageW(st->lblModel, WM_SETFONT, (WPARAM)hFont, FALSE);
        PremiumCombo::SetDropMaxVisible(st->cmbPreset, 8);

        st->btnReset = CreateWindowW(L"BUTTON", L"Reload saved", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
            0, 0, 10, 10, hWnd, (HMENU)(INT_PTR)ID_LAYOUT_RESET, hInst, nullptr);
        SendMessageW(st->btnReset, WM_SETFONT, (WPARAM)hFont, TRUE);

        st->btnAdd = CreateWindowW(L"BUTTON", L"Add Key", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
            0, 0, 10, 10, hWnd, (HMENU)(INT_PTR)ID_LAYOUT_ADD, hInst, nullptr);
        SendMessageW(st->btnAdd, WM_SETFONT, (WPARAM)hFont, TRUE);

        st->btnDelete = CreateWindowW(L"BUTTON", L"Delete Selected", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
            0, 0, 10, 10, hWnd, (HMENU)(INT_PTR)ID_LAYOUT_DELETE, hInst, nullptr);
        SendMessageW(st->btnDelete, WM_SETFONT, (WPARAM)hFont, TRUE);

        st->btnSave = CreateWindowW(L"BUTTON", L"Save Changes", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
            0, 0, 10, 10, hWnd, (HMENU)(INT_PTR)ID_LAYOUT_SAVE, hInst, nullptr);
        SendMessageW(st->btnSave, WM_SETFONT, (WPARAM)hFont, TRUE);

        st->btnUniformSpacing = CreateWindowW(L"BUTTON", L"",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
            0, 0, 10, 10, hWnd, (HMENU)(INT_PTR)ID_LAYOUT_UNIFORM_SPACING, hInst, nullptr);
        SendMessageW(st->btnUniformSpacing, WM_SETFONT, (WPARAM)hFont, TRUE);
        Layout_UpdateUniformSpacingButton(st);

        st->lblUniformGap = CreateWindowW(L"STATIC", L"Uniform gap",
            WS_CHILD | WS_VISIBLE,
            0, 0, 10, 10, hWnd, nullptr, hInst, nullptr);
        SendMessageW(st->lblUniformGap, WM_SETFONT, (WPARAM)hFont, TRUE);

        st->edtUniformGap = CreateWindowW(L"EDIT", L"",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_BORDER | ES_AUTOHSCROLL | ES_NUMBER,
            0, 0, 10, 10, hWnd, (HMENU)(INT_PTR)ID_LAYOUT_UNIFORM_GAP_EDIT, hInst, nullptr);
        SendMessageW(st->edtUniformGap, WM_SETFONT, (WPARAM)hFont, TRUE);

        st->lblLabel = CreateWindowW(L"STATIC", L"Label", WS_CHILD | WS_VISIBLE,
            0, 0, 10, 10, hWnd, nullptr, hInst, nullptr);
        SendMessageW(st->lblLabel, WM_SETFONT, (WPARAM)hFont, TRUE);

        st->edtLabel = CreateWindowW(L"EDIT", L"",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_BORDER | ES_AUTOHSCROLL,
            0, 0, 10, 10, hWnd, (HMENU)(INT_PTR)ID_LAYOUT_LABEL_EDIT, hInst, nullptr);
        SendMessageW(st->edtLabel, WM_SETFONT, (WPARAM)hFont, TRUE);

        SendMessageW(st->edtLabel, EM_SETLIMITTEXT, 63, 0);

        st->btnBindKey = CreateWindowW(L"BUTTON", L"Bind Physical Key", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
            0, 0, 10, 10, hWnd, (HMENU)(INT_PTR)ID_LAYOUT_BIND_KEY, hInst, nullptr);
        SendMessageW(st->btnBindKey, WM_SETFONT, (WPARAM)hFont, TRUE);

        st->lblPos = CreateWindowW(L"STATIC", L"X (px)", WS_CHILD | WS_VISIBLE,
            0, 0, 10, 10, hWnd, nullptr, hInst, nullptr);
        SendMessageW(st->lblPos, WM_SETFONT, (WPARAM)hFont, TRUE);

        st->edtPos = CreateWindowW(L"EDIT", L"",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_BORDER | ES_AUTOHSCROLL | ES_NUMBER,
            0, 0, 10, 10, hWnd, (HMENU)(INT_PTR)ID_LAYOUT_POS_EDIT, hInst, nullptr);
        SendMessageW(st->edtPos, WM_SETFONT, (WPARAM)hFont, TRUE);

        st->lblWidth = CreateWindowW(L"STATIC", L"Width (px)", WS_CHILD | WS_VISIBLE,
            0, 0, 10, 10, hWnd, nullptr, hInst, nullptr);
        SendMessageW(st->lblWidth, WM_SETFONT, (WPARAM)hFont, TRUE);

        st->edtWidth = CreateWindowW(L"EDIT", L"",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_BORDER | ES_AUTOHSCROLL | ES_NUMBER,
            0, 0, 10, 10, hWnd, (HMENU)(INT_PTR)ID_LAYOUT_WIDTH_EDIT, hInst, nullptr);
        SendMessageW(st->edtWidth, WM_SETFONT, (WPARAM)hFont, TRUE);

        st->lblHeight = CreateWindowW(L"STATIC", L"Height (px)", WS_CHILD | WS_VISIBLE,
            0, 0, 10, 10, hWnd, nullptr, hInst, nullptr);
        SendMessageW(st->lblHeight, WM_SETFONT, (WPARAM)hFont, TRUE);

        st->edtHeight = CreateWindowW(L"EDIT", L"",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_BORDER | ES_AUTOHSCROLL | ES_NUMBER,
            0, 0, 10, 10, hWnd, (HMENU)(INT_PTR)ID_LAYOUT_HEIGHT_EDIT, hInst, nullptr);
        SendMessageW(st->edtHeight, WM_SETFONT, (WPARAM)hFont, TRUE);
        st->lblNotchW = CreateWindowW(L"STATIC", L"Inset", WS_CHILD, 0,0,10,10,hWnd,nullptr,hInst,nullptr);
        st->lblNotchY = CreateWindowW(L"STATIC", L"Top arm", WS_CHILD, 0,0,10,10,hWnd,nullptr,hInst,nullptr);
        st->edtNotchW = CreateWindowW(L"EDIT", L"", WS_CHILD | WS_TABSTOP | WS_BORDER | ES_AUTOHSCROLL,
            0,0,10,10,hWnd,(HMENU)(INT_PTR)ID_LAYOUT_NOTCH_W,hInst,nullptr);
        st->edtNotchY = CreateWindowW(L"EDIT", L"", WS_CHILD | WS_TABSTOP | WS_BORDER | ES_AUTOHSCROLL,
            0,0,10,10,hWnd,(HMENU)(INT_PTR)ID_LAYOUT_NOTCH_Y,hInst,nullptr);
        for (HWND control : {st->lblNotchW, st->lblNotchY, st->edtNotchW, st->edtNotchY})
            SendMessageW(control, WM_SETFONT, (WPARAM)hFont, FALSE);

        st->lblBindState = CreateWindowW(L"STATIC", L"", WS_CHILD | WS_VISIBLE,
            0, 0, 10, 10, hWnd, nullptr, hInst, nullptr);
        SendMessageW(st->lblBindState, WM_SETFONT, (WPARAM)hFont, TRUE);

        st->lblKeys = CreateWindowW(L"STATIC", L"Key properties", WS_CHILD | WS_VISIBLE,
            0, 0, 10, 10, hWnd, nullptr, hInst, nullptr);
        SendMessageW(st->lblKeys, WM_SETFONT, (WPARAM)hFont, TRUE);

        // Selection is owned by the canvas; no hidden technical list is needed.
        auto make = [&](const wchar_t* cls, const wchar_t* text, int id, DWORD style) {
            HWND control = CreateWindowW(cls, text, WS_CHILD | WS_VISIBLE | style,
                0, 0, 10, 10, hWnd, (HMENU)(INT_PTR)id, hInst, nullptr);
            SendMessageW(control, WM_SETFONT, (WPARAM)hFont, FALSE);
            return control;
        };
        const DWORD buttonStyle = WS_TABSTOP | BS_OWNERDRAW;
        st->btnUndo = make(L"BUTTON", L"Undo", ID_LAYOUT_UNDO, buttonStyle);
        st->btnRedo = make(L"BUTTON", L"Redo", ID_LAYOUT_REDO, buttonStyle);
        st->btnFit = make(L"BUTTON", L"Fit to window", ID_LAYOUT_FIT, buttonStyle);
        st->btnActual = make(L"BUTTON", L"100%", ID_LAYOUT_ACTUAL, buttonStyle);
        st->btnDuplicate = make(L"BUTTON", L"Duplicate", ID_LAYOUT_DUPLICATE, buttonStyle);
        st->btnPrecision = make(L"BUTTON", L"", ID_LAYOUT_PRECISION, buttonStyle);
        st->btnSnap = make(L"BUTTON", L"", ID_LAYOUT_SNAP, buttonStyle);
        for (HWND button : {st->btnReset, st->btnAdd, st->btnDelete, st->btnSave,
            st->btnUniformSpacing, st->btnBindKey, st->btnUndo, st->btnRedo,
            st->btnFit, st->btnActual, st->btnDuplicate, st->btnPrecision, st->btnSnap})
            SetWindowSubclass(button, Layout_ButtonMouseProc, 1, 0);
        st->lblStatus = make(L"STATIC", L"All changes saved", 0, SS_RIGHT);
        st->lblLayoutTools = make(L"STATIC", L"Canvas & spacing", 0, SS_NOPREFIX);
        st->lblEmpty = make(L"STATIC", L"Select a key on the canvas to edit it.\n\nDrag to move it, or enter exact dimensions here.", 0, 0);
        st->lblY = make(L"STATIC", L"Row", 0, 0);
        st->edtY = make(L"EDIT", L"", ID_LAYOUT_Y, WS_TABSTOP | WS_BORDER | ES_AUTOHSCROLL | ES_NUMBER);

        st->lblHint = CreateWindowW(L"STATIC",
            L"",
            WS_CHILD | WS_VISIBLE,
            0, 0, 10, 10, hWnd, nullptr, hInst, nullptr);
        SendMessageW(st->lblHint, WM_SETFONT, (WPARAM)hFont, TRUE);
        Layout_UpdateHintText(st);

        st->editingPresetIdx = KeyboardLayout_GetCurrentPresetIndex();
        Layout_RefreshPresetCombo(st);
        Layout_LoadDraftFromPreset(hWnd, st, st->editingPresetIdx, false);
        EnableWindow(st->btnDelete, FALSE);
        Layout_SetUnsaved(st, false);
        Layout_UpdateMetaControls(st);

        return 0;
    }

    case WM_SHOWWINDOW:
        if (st)
        {
            if (wParam)
                SetTimer(hWnd, ID_LAYOUT_UI_TIMER, 16, nullptr);
            else
                KillTimer(hWnd, ID_LAYOUT_UI_TIMER);
        }
        return 0;

    case WM_SIZE:
        if (st) Layout_Arrange(hWnd, st);
        InvalidateRect(hWnd, nullptr, FALSE);
        return 0;

    case WM_COMMAND:
        if (!st) return 0;
        if (HIWORD(wParam) == BN_CLICKED)
        {
            switch (LOWORD(wParam))
            {
            case ID_LAYOUT_UNDO: Layout_UndoRedo(hWnd, st, false); return 0;
            case ID_LAYOUT_REDO: Layout_UndoRedo(hWnd, st, true); return 0;
            case ID_LAYOUT_FIT:
                st->view.ready = false; InvalidateRect(hWnd, nullptr, FALSE); return 0;
            case ID_LAYOUT_ACTUAL:
                { float scale, ox, oy; Layout_ComputeTransform(st, st->canvasRc, nullptr, scale, ox, oy);
                  st->view.Zoom((st->canvasRc.right - st->canvasRc.left) / 2.0f,
                      (st->canvasRc.bottom - st->canvasRc.top) / 2.0f, 1.0f / scale);
                  InvalidateRect(hWnd, nullptr, FALSE); return 0; }
            case ID_LAYOUT_PRECISION:
                SetFocus(hWnd); st->history.EndGroup();
                st->preciseY = !st->preciseY;
                if (!st->preciseY && st->selectedIdx >= 0) {
                    auto& key = st->draftKeys[st->selectedIdx];
                    key.row = std::clamp((int)std::lround(KeyboardLayout_KeyY(key) /
                        (double)KEYBOARD_ROW_PITCH_Y), 0, 20);
                    key.y = -1;
                    Layout_SetUnsaved(st, true);
                }
                Layout_UpdateMetaControls(st);
                InvalidateRect(hWnd, &st->canvasRc, FALSE); return 0;
            case ID_LAYOUT_SNAP:
                st->history.EndGroup(); st->snap = !st->snap; Layout_UpdateMetaControls(st); return 0;
            case ID_LAYOUT_DUPLICATE:
                if (st->selectedIdx >= 0 && st->draftKeys.size() < halljoy::ini::kMaxLayoutKeys)
                {
                    auto key = st->draftKeys[st->selectedIdx];
                    auto label = st->draftLabels[st->selectedIdx];
                    key.x = std::clamp(key.x + key.w + 8, 0, 4000);
                    key.hid = 0;
                    st->draftKeys.push_back(key); st->draftLabels.push_back(label);
                    Layout_RebindDraftLabels(st);
                    st->selectedIdx = (int)st->draftKeys.size() - 1;
                    Layout_RefreshSelection(hWnd, st); Layout_SetUnsaved(st, true);
                    InvalidateRect(hWnd, &st->canvasRc, FALSE);
                }
                return 0;
            }
        }
        if (LOWORD(wParam) == ID_LAYOUT_Y &&
            (HIWORD(wParam) == EN_CHANGE || HIWORD(wParam) == EN_KILLFOCUS))
        {
            if (st->selectedIdx >= 0 && (GetFocus() == st->edtY || HIWORD(wParam) == EN_KILLFOCUS))
            {
                int value = 0;
                if (Layout_ReadNumber(st->edtY, st->preciseY ? 0 : 1, st->preciseY ? 4000 : 21, value))
                {
                    auto& key = st->draftKeys[st->selectedIdx];
                    const int y = st->preciseY ? value : halljoy::layout_editor::RowY(key, value - 1);
                    if (KeyboardLayout_KeyY(key) != y)
                    {
                        key.y = st->preciseY || key.y >= 0 ? y : -1;
                        if (!st->preciseY) key.row = value - 1;
                        Layout_SetUnsaved(st, true); InvalidateRect(hWnd, &st->canvasRc, FALSE);
                    }
                }
            }
            return 0;
        }
        if (LOWORD(wParam) == ID_LAYOUT_BRAND && HIWORD(wParam) == CBN_SELCHANGE)
        {
            st->layoutPicker.Browse(st->editingPresetIdx);
            Layout_Arrange(hWnd, st);
            return 0;
        }
        if ((LOWORD(wParam) == ID_LAYOUT_PRESET || LOWORD(wParam) == ID_LAYOUT_VARIANT) && HIWORD(wParam) == CBN_SELCHANGE)
        {
            PremiumCombo::SetDeleteConfirmation(st->cmbPreset, -1);
            PremiumCombo::SetDeleteConfirmation(st->layoutPicker.variant, -1);
            int sel = LOWORD(wParam) == ID_LAYOUT_PRESET ? st->layoutPicker.ChooseModel() : st->layoutPicker.Selected();
            if (sel == LayoutPicker::Create) {
                PremiumCombo::ShowDropDown(st->cmbPreset, true);
                PremiumCombo::BeginInlineEditSelected(st->cmbPreset, false); return 0;
            }
            if (sel >= 0 && sel != st->editingPresetIdx && Layout_ResolveDraft(hWnd, st))
                Layout_LoadDraftFromPreset(hWnd, st, sel, true);
            Layout_RefreshPresetCombo(st);
            return 0;
        }
        if (LOWORD(wParam) == ID_LAYOUT_RESET && HIWORD(wParam) == BN_CLICKED)
        {
            if (!Layout_ResolveDraft(hWnd, st)) return 0;
            Layout_LoadDraftFromPreset(hWnd, st, st->editingPresetIdx, true);
            Layout_RefreshPresetCombo(st);
            return 0;
        }
        if (LOWORD(wParam) == ID_LAYOUT_ADD && HIWORD(wParam) == BN_CLICKED)
        {
            if (st->draftKeys.size() >= halljoy::ini::kMaxLayoutKeys) return 0;
            int n = Layout_DraftCount(st);
            int maxRow = 0;
            for (int i = 0; i < n; ++i)
            {
                KeyDef kk{};
                if (Layout_DraftGet(st, i, kk))
                    maxRow = std::max(maxRow, kk.row);
            }
            st->draftLabels.emplace_back(L"Key");
            KeyDef kd{};
            kd.label = nullptr;
            kd.hid = 0;
            kd.row = std::clamp(maxRow + 1, 0, 20);
            kd.x = 0;
            kd.w = 42;
            kd.h = KEYBOARD_KEY_H;
            st->draftKeys.push_back(kd);
            Layout_RebindDraftLabels(st);
            st->selectedIdx = (int)st->draftKeys.size() - 1;
            Layout_RefreshSelection(hWnd, st);
            Layout_SetUnsaved(st, true);
            InvalidateRect(hWnd, nullptr, FALSE);
            return 0;
        }
        if (LOWORD(wParam) == ID_LAYOUT_BIND_KEY && HIWORD(wParam) == BN_CLICKED)
        {
            if (st->bindArmed)
                Layout_StopBindCapture(hWnd, st, L"Bind cancelled.");
            else
                Layout_StartBindCapture(hWnd, st);
            return 0;
        }
        if (LOWORD(wParam) == ID_LAYOUT_UNIFORM_SPACING && HIWORD(wParam) == BN_CLICKED)
        {
            if (st->uniformSpacingEnabled)
            {
                if (Layout_BakeUniformSpacingIntoDraft(st))
                    Layout_RefreshSelection(hWnd, st);
            }
            st->uniformSpacingEnabled = !st->uniformSpacingEnabled;
            Layout_UpdateUniformSpacingButton(st);
            Layout_UpdateHintText(st);
            Layout_UpdateMetaControls(st);
            Layout_SetUnsaved(st, true);
            InvalidateRect(hWnd, &st->canvasRc, FALSE);
            return 0;
        }
        if (LOWORD(wParam) == ID_LAYOUT_DELETE && HIWORD(wParam) == BN_CLICKED)
        {
            if (st->selectedIdx >= 0 && st->selectedIdx < (int)st->draftKeys.size())
            {
                st->draftKeys.erase(st->draftKeys.begin() + st->selectedIdx);
                if (st->selectedIdx < (int)st->draftLabels.size())
                    st->draftLabels.erase(st->draftLabels.begin() + st->selectedIdx);
                Layout_RebindDraftLabels(st);

                int n = (int)st->draftKeys.size();
                if (st->selectedIdx >= n) st->selectedIdx = n - 1;
                Layout_RefreshSelection(hWnd, st);
                Layout_SetUnsaved(st, true);
                InvalidateRect(hWnd, nullptr, FALSE);
            }
            return 0;
        }
        if (LOWORD(wParam) == ID_LAYOUT_SAVE && HIWORD(wParam) == BN_CLICKED)
        {
            Layout_SaveDraft(hWnd, st);
            return 0;
        }
        if (LOWORD(wParam) == ID_LAYOUT_LABEL_EDIT && HIWORD(wParam) == EN_CHANGE)
        {
            if ((HWND)lParam == st->edtLabel && GetFocus() == st->edtLabel)
                Layout_ApplyLabelFromEdit(hWnd, st);
            return 0;
        }
        if (LOWORD(wParam) == ID_LAYOUT_POS_EDIT && HIWORD(wParam) == EN_KILLFOCUS)
        {
            Layout_ApplyGeometryFromEdits(hWnd, st, true, false, false);
            return 0;
        }
        if (LOWORD(wParam) == ID_LAYOUT_POS_EDIT && HIWORD(wParam) == EN_CHANGE)
        {
            if ((HWND)lParam == st->edtPos && GetFocus() == st->edtPos)
                Layout_ApplyGeometryFromEdits(hWnd, st, true, false, false);
            return 0;
        }
        if ((LOWORD(wParam) == ID_LAYOUT_NOTCH_W || LOWORD(wParam) == ID_LAYOUT_NOTCH_Y) &&
            (HIWORD(wParam) == EN_CHANGE || HIWORD(wParam) == EN_KILLFOCUS))
        {
            if (st->selectedIdx < 0 || st->selectedIdx >= (int)st->draftKeys.size()) return 0;
            auto& key = st->draftKeys[st->selectedIdx];
            if (!key.notchW) return 0;
            const bool width = LOWORD(wParam) == ID_LAYOUT_NOTCH_W;
            HWND edit = width ? st->edtNotchW : st->edtNotchY;
            int value = 0;
            if (GetFocus() == edit && Layout_ReadNumber(edit, 1, (width ? key.w : key.h)-1, value)) {
                int& target = width ? key.notchW : key.notchY;
                if (value != target) {
                    target = value; Layout_SetUnsaved(st, true);
                    InvalidateRect(hWnd, &st->canvasRc, FALSE);
                }
            }
            if (HIWORD(wParam) == EN_KILLFOCUS) { st->history.EndGroup(); Layout_UpdateMetaControls(st); }
            return 0;
        }
        if (LOWORD(wParam) == ID_LAYOUT_WIDTH_EDIT && HIWORD(wParam) == EN_KILLFOCUS)
        {
            Layout_ApplyGeometryFromEdits(hWnd, st, false, true, false);
            return 0;
        }
        if (LOWORD(wParam) == ID_LAYOUT_WIDTH_EDIT && HIWORD(wParam) == EN_CHANGE)
        {
            if ((HWND)lParam == st->edtWidth && GetFocus() == st->edtWidth)
                Layout_ApplyGeometryFromEdits(hWnd, st, false, true, false);
            return 0;
        }
        if (LOWORD(wParam) == ID_LAYOUT_HEIGHT_EDIT && HIWORD(wParam) == EN_KILLFOCUS)
        {
            Layout_ApplyGeometryFromEdits(hWnd, st, false, false, true);
            return 0;
        }
        if (LOWORD(wParam) == ID_LAYOUT_HEIGHT_EDIT && HIWORD(wParam) == EN_CHANGE)
        {
            if ((HWND)lParam == st->edtHeight && GetFocus() == st->edtHeight)
                Layout_ApplyGeometryFromEdits(hWnd, st, false, false, true);
            return 0;
        }
        if (LOWORD(wParam) == ID_LAYOUT_UNIFORM_GAP_EDIT && HIWORD(wParam) == EN_KILLFOCUS)
        {
            Layout_ApplyUniformGapFromEdit(hWnd, st);
            Layout_UpdateMetaControls(st);
            return 0;
        }
        if (LOWORD(wParam) == ID_LAYOUT_UNIFORM_GAP_EDIT && HIWORD(wParam) == EN_CHANGE)
        {
            if ((HWND)lParam == st->edtUniformGap && GetFocus() == st->edtUniformGap)
            {
                Layout_ApplyUniformGapFromEdit(hWnd, st);
                Layout_UpdateMetaControls(st);
            }
            return 0;
        }
        return 0;

    case WM_DRAWITEM:
    {
        const DRAWITEMSTRUCT* dis = (const DRAWITEMSTRUCT*)lParam;
        if (st && dis && dis->CtlType == ODT_BUTTON &&
            ((dis->CtlID == ID_LAYOUT_RESET && st->btnReset == dis->hwndItem) ||
             (dis->CtlID == ID_LAYOUT_ADD && st->btnAdd == dis->hwndItem) ||
             (dis->CtlID == ID_LAYOUT_DELETE && st->btnDelete == dis->hwndItem) ||
             (dis->CtlID == ID_LAYOUT_BIND_KEY && st->btnBindKey == dis->hwndItem) ||
             (dis->CtlID == ID_LAYOUT_SAVE && st->btnSave == dis->hwndItem) ||
             (dis->CtlID == ID_LAYOUT_UNIFORM_SPACING && st->btnUniformSpacing == dis->hwndItem) ||
             (dis->CtlID >= ID_LAYOUT_UNDO && dis->CtlID <= ID_LAYOUT_ACTUAL)))
        {
            // Owner-draw owns every pixel, including rounded corners and disabled
            // alpha blends. Never blend over native button paint or a prior frame.
            FillRect(dis->hDC, &dis->rcItem, UiTheme::Brush_PanelBg());
            Graphics g(dis->hDC);
            g.SetSmoothingMode(SmoothingModeAntiAlias);
            HGDIOBJ previousFont = SelectObject(dis->hDC, GetStockObject(SYSTEM_FONT));
            wchar_t label[128]{}; GetWindowTextW(dis->hwndItem, label, 128);
            const bool enabled = !(dis->itemState & ODS_DISABLED);
            if (dis->CtlID == ID_LAYOUT_SAVE && enabled)
            {
                CustomPage_DrawRoundRect(g, dis->rcItem,
                    (dis->itemState & ODS_SELECTED) ? RGB(30, 65, 91) : RGB(40, 86, 117),
                    UiTheme::Color_Accent(), 5.0f);
                CustomPage_DrawText(dis->hDC, label, dis->rcItem, UiTheme::Color_Text(), DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            }
            else CustomPage_DrawButton(g, dis->hDC, dis->rcItem, label,
                (dis->itemState & ODS_HOTLIGHT) != 0, (dis->itemState & ODS_SELECTED) != 0, enabled);
            if (enabled && (dis->itemState & ODS_FOCUS) && !(dis->itemState & ODS_NOFOCUSRECT)) {
                Pen focus(Gp(UiTheme::Color_Accent(), 190), 2.0f);
                g.DrawLine(&focus, (REAL)dis->rcItem.left + 9, (REAL)dis->rcItem.bottom - 3,
                    (REAL)dis->rcItem.right - 9, (REAL)dis->rcItem.bottom - 3);
            }
            SelectObject(dis->hDC, previousFont);
            return TRUE;
        }
        break;
    }

    case WM_LBUTTONDOWN:
        if (st)
        {
            POINT pt{ (short)LOWORD(lParam), (short)HIWORD(lParam) };
            if (st->dragging || st->panning || st->draggingGuide >= 0) return 0;
            const bool topRuler = PtInRect(&st->topRuler, pt) != 0;
            const bool leftRuler = PtInRect(&st->leftRuler, pt) != 0;
            const int edges = PtInRect(&st->canvasRc, pt) ? Layout_ResizeEdges(st, pt) : 0;
            const int guide = edges ? -1 : Layout_HitGuide(st, pt);
            if (topRuler || leftRuler || guide >= 0) {
                if ((topRuler || leftRuler) && st->guides.size() >= 64) return 0;
                st->guideNew = guide < 0;
                if (st->guideNew) st->guides.push_back({leftRuler, 0});
                st->draggingGuide = st->guideNew ? (int)st->guides.size() - 1 : guide;
                st->guideOriginal = st->guides[st->draggingGuide].coordinate;
                SetFocus(hWnd); SetCapture(hWnd);
                Layout_MoveGuide(hWnd, st, pt); return 0;
            }
            if (PtInRect(&st->canvasRc, pt))
            {
                st->history.EndGroup();
                SetFocus(hWnd);
                int hit = edges ? st->selectedIdx : Layout_HitTestKey(st, pt);
                if (hit < 0)
                {
                    st->selectedIdx = -1; Layout_UpdateMetaControls(st);
                    st->history.Select(-1);
                    InvalidateRect(hWnd, &st->canvasRc, FALSE); return 0;
                }
                if (hit >= 0)
                {
                    st->selectedIdx = hit;
                    st->history.Select(hit);
                    Layout_UpdateMetaControls(st);
                    SetFocus(hWnd);
                    RECT rr = Layout_KeyRectOnCanvas(st, hit, st->canvasRc);
                    st->resizeEdges = edges; st->resizing = edges != 0;
                    st->resizeStart = pt; st->resizeWidth = st->draftKeys[hit].w; st->resizeHeight = st->draftKeys[hit].h;
                    std::vector<int> displayX; Layout_BuildDisplayXMap(st, displayX);
                    st->resizeOrigin = {displayX.empty() ? st->draftKeys[hit].x : displayX[hit],
                        KeyboardLayout_KeyY(st->draftKeys[hit]), st->resizeWidth, st->resizeHeight};
                    st->dragOffsetX = (float)(pt.x - rr.left);
                    st->dragOffsetY = (float)(pt.y - rr.top);
                    st->dragging = true;
                    st->dirty = false;
                    SetCapture(hWnd);
                    InvalidateRect(hWnd, &st->canvasRc, FALSE);
                }
            }
        }
        return 0;

    case WM_MOUSEMOVE:
        if (st && st->draggingGuide >= 0) {
            Layout_MoveGuide(hWnd, st, POINT{(short)LOWORD(lParam), (short)HIWORD(lParam)}); return 0;
        }
        if (st && st->panning)
        {
            POINT point{ (short)LOWORD(lParam), (short)HIWORD(lParam) };
            st->view.x += point.x - st->panPoint.x; st->view.y += point.y - st->panPoint.y;
            st->panPoint = point;
            InvalidateRect(hWnd, nullptr, FALSE); return 0;
        }
        if (st && st->dragging)
        {
            POINT pt{ (short)LOWORD(lParam), (short)HIWORD(lParam) };
            Layout_ApplyDrag(hWnd, st, pt);
        }
        return 0;

    case WM_LBUTTONUP:
        if (st && st->draggingGuide >= 0) {
            POINT point{(short)LOWORD(lParam), (short)HIWORD(lParam)};
            Layout_MoveGuide(hWnd, st, point);
            if (!PtInRect(&st->canvasRc, point)) st->guides.erase(st->guides.begin() + st->draggingGuide);
            st->draggingGuide = -1; ReleaseCapture(); InvalidateRect(hWnd, nullptr, FALSE); return 0;
        }
        if (st && st->dragging)
        {
            st->dragging = false;
            InvalidateRect(hWnd, &st->canvasRc, FALSE);
            ReleaseCapture();
            if (st->dirty)
            {
                Layout_RefreshSelection(hWnd, st);
                Layout_SetUnsaved(st, true);
                st->dirty = false;
            }
        }
        return 0;

    case WM_MBUTTONDOWN:
        if (st && !st->dragging && st->draggingGuide < 0) {
            POINT p{ (short)LOWORD(lParam), (short)HIWORD(lParam) };
            if (PtInRect(&st->canvasRc, p)) {
                float scale, ox, oy; Layout_ComputeTransform(st, st->canvasRc, nullptr, scale, ox, oy);
                st->panning = true; st->panPoint = p; SetCapture(hWnd); SetCursor(LoadCursorW(nullptr, IDC_SIZEALL));
            }
        }
        return 0;
    case WM_MBUTTONUP:
        if (st && st->panning) { st->panning = false; ReleaseCapture(); }
        return 0;
    case WM_MOUSEWHEEL:
        if (st && !st->panning) {
            POINT point{ (short)LOWORD(lParam), (short)HIWORD(lParam) }; ScreenToClient(hWnd, &point);
            if (!PtInRect(&st->canvasRc, point) && !st->dragging && st->draggingGuide < 0) return 0;
            float scale, ox, oy; Layout_ComputeTransform(st, st->canvasRc, nullptr, scale, ox, oy);
            st->view.Zoom((float)(point.x - st->canvasRc.left), (float)(point.y - st->canvasRc.top),
                std::pow(1.15f, GET_WHEEL_DELTA_WPARAM(wParam) / 120.0f));
            if (st->dragging && st->selectedIdx >= 0) {
                // Rebase the active gesture, not the document/history. The next
                // mouse delta uses the new zoom without moving the fixed edge.
                Layout_ComputeTransform(st, st->canvasRc, nullptr, scale, ox, oy);
                const auto& key = st->draftKeys[st->selectedIdx];
                std::vector<int> displayX; Layout_BuildDisplayXMap(st, displayX);
                const int x = displayX.empty() ? key.x : displayX[st->selectedIdx];
                st->resizeOrigin = {x, KeyboardLayout_KeyY(key), key.w, key.h};
                st->resizeStart = point;
                st->dragOffsetX = point.x - ox - (KEYBOARD_MARGIN_X + x) * scale;
                st->dragOffsetY = point.y - oy - (KEYBOARD_MARGIN_Y + KeyboardLayout_KeyY(key)) * scale;
            }
            InvalidateRect(hWnd, nullptr, FALSE);
        }
        return 0;

    case WM_RBUTTONUP:
        if (st && !st->dragging && !st->panning && st->draggingGuide < 0) {
            const int guide = Layout_HitGuide(st, POINT{(short)LOWORD(lParam), (short)HIWORD(lParam)});
            if (guide >= 0) { st->guides.erase(st->guides.begin() + guide); InvalidateRect(hWnd, nullptr, FALSE); }
        }
        return 0;

    case WM_CAPTURECHANGED:
        if (st)
        {
            if (st->draggingGuide >= 0) {
                if (st->guideNew) st->guides.erase(st->guides.begin() + st->draggingGuide);
                else st->guides[st->draggingGuide].coordinate = st->guideOriginal;
                st->draggingGuide = -1; InvalidateRect(hWnd, nullptr, FALSE);
            }
            st->resizing = false;
            st->panning = false;
            st->dragging = false;
            InvalidateRect(hWnd, &st->canvasRc, FALSE);
            if (st->dirty)
            {
                st->dirty = false;
                Layout_RefreshSelection(hWnd, st);
                Layout_SetUnsaved(st, true);
            }
        }
        return 0;

    case WM_GETDLGCODE:
        return DLGC_WANTALLKEYS;

    case WM_TIMER:
        if (!st) return 0;
        if (wParam == ID_LAYOUT_UI_TIMER)
        {
            if (st->bindArmed)
            {
                uint16_t hid = 0, rawM = 0;
                if (BackendUI_ConsumeBindCapture(&hid, &rawM))
                {
                    if (st->selectedIdx >= 0 && st->selectedIdx < (int)st->draftKeys.size())
                    {
                        st->draftKeys[st->selectedIdx].hid = hid;
                        Layout_RefreshSelection(hWnd, st);
                        Layout_SetUnsaved(st, true);
                    }
                    wchar_t s[128]{};
                    swprintf_s(s, L"Key assigned. Press it to test.");
                    Layout_StopBindCapture(hWnd, st, s);
                    InvalidateRect(hWnd, nullptr, FALSE);
                }
                else
                {
                    Layout_UpdateBindStateOnly(st);
                }
            }
            else
            {
                // Update only lightweight status text; repaint preview only when values changed.
                Layout_UpdateBindStateOnly(st);
                uint32_t h = Layout_ComputePreviewHash(st);
                if (h != st->previewHash)
                {
                    st->previewHash = h;
                    InvalidateRect(hWnd, &st->canvasRc, FALSE);
                }
            }
            return 0;
        }
        break;

    case WM_KEYDOWN:
        if (!st) return 0;
        if (st->draggingGuide >= 0) {
            if (wParam == VK_ESCAPE) ReleaseCapture();
            return 0;
        }
        if (st->dragging || st->panning) return 0;
        if (GetFocus() == hWnd && (GetKeyState(VK_CONTROL) & 0x8000))
        {
            if (wParam == 'Z') { Layout_UndoRedo(hWnd, st, (GetKeyState(VK_SHIFT) & 0x8000) != 0); return 0; }
            if (wParam == 'Y') { Layout_UndoRedo(hWnd, st, true); return 0; }
        }
        if (wParam == VK_ESCAPE && st->bindArmed)
        {
            Layout_StopBindCapture(hWnd, st, L"Bind cancelled.");
            return 0;
        }
        if (wParam == VK_RETURN)
        {
            HWND focus = GetFocus();
            if (focus && (focus == st->edtPos || focus == st->edtWidth || focus == st->edtHeight))
            {
                Layout_ApplyGeometryFromEdits(hWnd, st, true, true, true);
                SetFocus(hWnd);
                Layout_UpdateMetaControls(st);
                return 0;
            }
            if (focus && focus == st->edtUniformGap)
            {
                Layout_ApplyUniformGapFromEdit(hWnd, st);
                SetFocus(hWnd);
                Layout_UpdateMetaControls(st);
                return 0;
            }
        }
        if ((GetKeyState(VK_CONTROL) & 0x8000) != 0 && (wParam == 'R' || wParam == 'r'))
        {
            if (!Layout_ResolveDraft(hWnd, st)) return 0;
            Layout_LoadDraftFromPreset(hWnd, st, st->editingPresetIdx, true);
            Layout_RefreshPresetCombo(st);
            return 0;
        }
        if (st->selectedIdx < 0) return 0;
        {
            bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
            int step = shift ? 4 : 1;
            switch (wParam)
            {
            case VK_LEFT:  Layout_NudgeSelectedKey(hWnd, st, 0, -step, 0); return 0;
            case VK_RIGHT: Layout_NudgeSelectedKey(hWnd, st, 0, +step, 0); return 0;
            case VK_UP:    Layout_NudgeSelectedKey(hWnd, st, -1, 0, 0); return 0;
            case VK_DOWN:  Layout_NudgeSelectedKey(hWnd, st, +1, 0, 0); return 0;
            case VK_OEM_4: Layout_NudgeSelectedKey(hWnd, st, 0, 0, -step); return 0; // [
            case VK_OEM_6: Layout_NudgeSelectedKey(hWnd, st, 0, 0, +step); return 0; // ]
            }
        }
        return 0;

    case WM_NCDESTROY:
        if (st)
        {
            KillTimer(hWnd, ID_LAYOUT_UI_TIMER);
            Layout_StopBindCapture(hWnd, st);
            delete st;
            SetWindowLongPtrW(hWnd, GWLP_USERDATA, 0);
        }
        return 0;
    }

    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

// ============================================================================
// Detached layout editor window
// ============================================================================
static HWND g_hLayoutEditorWindow = nullptr;

struct LayoutEditorHostState
{
    HWND hPage = nullptr;
};

bool KeyboardUI_CloseLayoutEditor(bool saveWithoutPrompt)
{
    if (!g_hLayoutEditorWindow || !IsWindow(g_hLayoutEditorWindow)) return true;
    auto* host = (LayoutEditorHostState*)GetWindowLongPtrW(g_hLayoutEditorWindow, GWLP_USERDATA);
    auto* draft = host ? (LayoutPageState*)GetWindowLongPtrW(host->hPage, GWLP_USERDATA) : nullptr;
    if (host && !Layout_ResolveDraft(host->hPage, draft, saveWithoutPrompt))
    {
        if (!saveWithoutPrompt) SetForegroundWindow(g_hLayoutEditorWindow);
        return false;
    }
    DestroyWindow(g_hLayoutEditorWindow);
    return true;
}

static bool LayoutEditor_DeletePreset(int idx, bool keepEditor)
{
    auto* host = g_hLayoutEditorWindow
        ? (LayoutEditorHostState*)GetWindowLongPtrW(g_hLayoutEditorWindow, GWLP_USERDATA) : nullptr;
    auto* draft = host ? (LayoutPageState*)GetWindowLongPtrW(host->hPage, GWLP_USERDATA) : nullptr;
    if (draft && draft->resolvingDraft) return false;
    if (draft && draft->editingPresetIdx == idx)
    {
        if (keepEditor) {
            if (!Layout_ResolveDraft(host->hPage, draft)) return false;
        } else {
            if (!KeyboardUI_CloseLayoutEditor()) return false;
            draft = nullptr;
        }
    }
    if (!KeyboardLayout_DeletePreset(idx)) return false;
    // Deleting another preset must neither close this editor nor shift its draft
    // onto the next catalog entry. All catalog changes occur on this UI thread.
    if (draft)
    {
        if (draft->editingPresetIdx == idx)
            Layout_LoadDraftFromPreset(host->hPage, draft, std::min(idx, KeyboardLayout_GetPresetCount() - 1), true);
        else if (draft->editingPresetIdx > idx) --draft->editingPresetIdx;
        Layout_RefreshPresetCombo(draft);
        Layout_SetUnsaved(draft, true);
    }
    return true;
}

static void LayoutEditor_ApplyDarkFrame(HWND hWnd)
{
    if (!hWnd) return;
    UiTheme::ApplyToTopLevelWindow(hWnd);
    SetWindowPos(hWnd, nullptr, 0, 0, 0, 0,
        SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
    RedrawWindow(hWnd, nullptr, nullptr, RDW_FRAME | RDW_INVALIDATE);
}

static LRESULT CALLBACK LayoutEditorHostProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    auto* st = (LayoutEditorHostState*)GetWindowLongPtrW(hWnd, GWLP_USERDATA);

    switch (msg)
    {
    case WM_CREATE:
    {
        auto* cs = (CREATESTRUCTW*)lParam;
        HINSTANCE hInst = cs ? (HINSTANCE)cs->hInstance : (HINSTANCE)GetWindowLongPtrW(hWnd, GWLP_HINSTANCE);

        st = new LayoutEditorHostState();
        SetWindowLongPtrW(hWnd, GWLP_USERDATA, (LONG_PTR)st);

        st->hPage = CreateWindowW(L"KeyboardSubLayoutPage", L"",
            WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN,
            0, 0, 100, 100, hWnd, nullptr, hInst, nullptr);
        if (!st->hPage) return -1;
        if (cs && cs->lpCreateParams)
        {
            auto* draft = (LayoutPageState*)GetWindowLongPtrW(st->hPage, GWLP_USERDATA);
            Layout_LoadDraftFromPreset(st->hPage, draft, *(const int*)cs->lpCreateParams, true);
            Layout_RefreshPresetCombo(draft);
        }
        LayoutEditor_ApplyDarkFrame(hWnd);
        return 0;
    }

    case WM_SIZE:
        if (st && st->hPage)
        {
            RECT rc{};
            GetClientRect(hWnd, &rc);
            SetWindowPos(st->hPage, nullptr, 0, 0, rc.right - rc.left, rc.bottom - rc.top, SWP_NOZORDER);
        }
        return 0;

    case WM_CLOSE:
        if (!st || Layout_ResolveDraft(st->hPage,
            (LayoutPageState*)GetWindowLongPtrW(st->hPage, GWLP_USERDATA)))
            DestroyWindow(hWnd);
        return 0;

    case WM_GETMINMAXINFO:
        if (auto* bounds = (MINMAXINFO*)lParam)
            bounds->ptMinTrackSize = POINT{ S(hWnd, 820), S(hWnd, 760) };
        return 0;

    case WM_SHOWWINDOW:
    case WM_ACTIVATE:
        if (msg == WM_ACTIVATE && LOWORD(wParam) != WA_INACTIVE && st && st->hPage)
        {
            auto* draft = (LayoutPageState*)GetWindowLongPtrW(st->hPage, GWLP_USERDATA);
            if (draft && !draft->resolvingDraft &&
                !PremiumCombo::GetDroppedState(draft->cmbPreset) &&
                !PremiumCombo::IsEditingItem(draft->cmbPreset) &&
                !draft->layoutPicker.Current())
            {
                Layout_RefreshPresetCombo(draft);
            }
        }
        LayoutEditor_ApplyDarkFrame(hWnd);
        return 0;

    case WM_NCDESTROY:
        g_hLayoutEditorWindow = nullptr;
        if (st)
        {
            delete st;
            SetWindowLongPtrW(hWnd, GWLP_USERDATA, 0);
        }
        return 0;
    }

    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

static void LayoutEditor_OpenWindow(HWND hOwnerPage, int presetIdx)
{
    if (presetIdx < 0) presetIdx = KeyboardLayout_GetCurrentPresetIndex();
    if (g_hLayoutEditorWindow && IsWindow(g_hLayoutEditorWindow))
    {
        ShowWindow(g_hLayoutEditorWindow, SW_SHOWNORMAL);
        SetForegroundWindow(g_hLayoutEditorWindow);
        auto* host = (LayoutEditorHostState*)GetWindowLongPtrW(g_hLayoutEditorWindow, GWLP_USERDATA);
        auto* draft = host ? (LayoutPageState*)GetWindowLongPtrW(host->hPage, GWLP_USERDATA) : nullptr;
        if (draft && draft->editingPresetIdx != presetIdx && Layout_ResolveDraft(host->hPage, draft))
        {
            Layout_LoadDraftFromPreset(host->hPage, draft, presetIdx, true);
            Layout_RefreshPresetCombo(draft);
        }
        return;
    }

    HINSTANCE hInst = (HINSTANCE)GetWindowLongPtrW(hOwnerPage, GWLP_HINSTANCE);
    HWND hOwnerTop = GetAncestor(hOwnerPage, GA_ROOT);

    static bool childReg = false;
    if (!childReg)
    {
        WNDCLASSW wc{};
        wc.lpfnWndProc = KeyboardSubpages_LayoutPageProc;
        wc.hInstance = hInst;
        wc.lpszClassName = L"KeyboardSubLayoutPage";
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.hbrBackground = nullptr;
        RegisterClassW(&wc);
        childReg = true;
    }

    static bool hostReg = false;
    if (!hostReg)
    {
        WNDCLASSW wc{};
        wc.lpfnWndProc = LayoutEditorHostProc;
        wc.hInstance = hInst;
        wc.lpszClassName = L"KeyboardLayoutEditorHost";
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.hbrBackground = UiTheme::Brush_PanelBg();
        wc.hIcon = (HICON)LoadImageW(hInst, MAKEINTRESOURCEW(IDI_HALLJOY), IMAGE_ICON, 0, 0, LR_DEFAULTSIZE);
        RegisterClassW(&wc);
        hostReg = true;
    }

    int w = S(hOwnerPage, 1180);
    int h = S(hOwnerPage, 760);
    int x = CW_USEDEFAULT;
    int y = CW_USEDEFAULT;

    g_hLayoutEditorWindow = CreateWindowExW(
        WS_EX_APPWINDOW,
        L"KeyboardLayoutEditorHost",
        L"HallJoy - Layout Editor",
        WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        x, y, w, h,
        hOwnerTop, nullptr, hInst, &presetIdx);

    if (g_hLayoutEditorWindow)
    {
        HICON hBig = (HICON)LoadImageW(hInst, MAKEINTRESOURCEW(IDI_HALLJOY), IMAGE_ICON, 0, 0, LR_DEFAULTSIZE);
        HICON hSmall = (HICON)LoadImageW(hInst, MAKEINTRESOURCEW(IDI_SMALL), IMAGE_ICON, 0, 0, LR_DEFAULTSIZE);
        if (hBig) SendMessageW(g_hLayoutEditorWindow, WM_SETICON, ICON_BIG, (LPARAM)hBig);
        if (hSmall) SendMessageW(g_hLayoutEditorWindow, WM_SETICON, ICON_SMALL, (LPARAM)hSmall);
        LayoutEditor_ApplyDarkFrame(g_hLayoutEditorWindow);
    }
}

// ============================================================================
// Premium slider + value chip
// ============================================================================
#if defined(HALLJOY_ANALOG_SIMULATOR)
bool KeyboardSubpages_TestLayoutEditor()
{
    if (AppPaths_Mode() != AppDataMode::SimulatorOverride) return false;
    if (!KeyboardLayout_TestFirstRunSelection()) return false;
    if (!BackendUI_TestTrackedUnion()) return false;
    HDESK previous = GetThreadDesktop(GetCurrentThreadId());
    wchar_t name[64]{}; swprintf_s(name, L"HallJoyLayoutTest-%lu", GetCurrentProcessId());
    HDESK desktop = CreateDesktopW(name, nullptr, nullptr, 0, GENERIC_ALL, nullptr);
    if (!desktop) return false;
    if (!SetThreadDesktop(desktop)) { CloseDesktop(desktop); return false; }
    HWND owner = CreateWindowW(L"STATIC", L"", WS_OVERLAPPEDWINDOW,
        0, 0, 900, 700, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
    bool ok = owner != nullptr;
    if (owner)
    {
        LayoutEditor_OpenWindow(owner, 0);
        HWND hostWindow = g_hLayoutEditorWindow;
        auto* host = (LayoutEditorHostState*)GetWindowLongPtrW(hostWindow, GWLP_USERDATA);
        auto* st = host ? (LayoutPageState*)GetWindowLongPtrW(host->hPage, GWLP_USERDATA) : nullptr;
        ok &= st != nullptr;
        if (st)
        {
            const HWND page = host->hPage;
            const size_t count = st->draftKeys.size();
            // Use the production message admission policy on real controls.
            // hostWindow stands in for the main root; its owner is a separate
            // root, proving independent editor windows are not globally muted.
            SetFocus(st->cmbPreset);
            const int modelBefore = PremiumCombo::GetCurSel(st->cmbPreset);
            for (HWND control : {st->cmbPreset,st->layoutPicker.brand,st->btnSave,st->btnSnap,page}) {
                for (UINT message : {WM_KEYDOWN,WM_KEYUP,WM_CHAR,WM_SYSKEYDOWN,WM_SYSKEYUP,WM_SYSCHAR}) {
                    for (WPARAM key : {WPARAM(VK_RETURN),WPARAM(VK_SPACE),WPARAM(VK_DOWN),WPARAM(VK_F4),WPARAM('S'),WPARAM('Z'),WPARAM(VK_DELETE)}) {
                        MSG event{}; event.hwnd=control; event.message=message; event.wParam=key;
                        ok &= !halljoy::main_input::Allow(event,hostWindow);
                        if (halljoy::main_input::Allow(event,hostWindow)) DispatchMessageW(&event);
                    }
                }
            }
            ok &= !PremiumCombo::GetDroppedState(st->cmbPreset) && PremiumCombo::GetCurSel(st->cmbPreset)==modelBefore;
            PremiumCombo::ShowDropDown(st->cmbPreset,true);
            HWND popup = nullptr;
            // Find only this root's popup; desktop z-order is not an ownership contract.
            for (HWND candidate=GetTopWindow(nullptr);candidate;candidate=GetWindow(candidate,GW_HWNDNEXT)) {
                wchar_t cls[64]{};GetClassNameW(candidate,cls,64);
                if (wcscmp(cls,L"PremiumCombo_Popup")==0 && GetWindow(candidate,GW_OWNER)==hostWindow) {popup=candidate;break;}
            }
            MSG popupKey{};popupKey.hwnd=popup;popupKey.message=WM_KEYDOWN;popupKey.wParam=VK_RETURN;
            ok &= popup && !halljoy::main_input::Allow(popupKey,hostWindow);
            PremiumCombo::ShowDropDown(st->cmbPreset,false);
            MSG editorKey{};editorKey.hwnd=page;editorKey.message=WM_KEYDOWN;editorKey.wParam='Z';
            ok &= halljoy::main_input::Allow(editorKey,owner);
            if (!ok) throw std::runtime_error("main window keyboard admission checks failed");
            // Exercise the actual shape edit controls and Win32 hit region,
            // on the private test desktop, without screen capture.
            const auto originalKey = st->draftKeys[0];
            st->draftKeys[0] = {L"Enter",40,0,0,66,86,0,12,40};
            st->selectedIdx = 0; st->history.Reset(Layout_CaptureDraft(st));
            Layout_UpdateMetaControls(st);
            ok &= st->shapeControlsVisible && IsWindowVisible(st->edtNotchW);
            SetFocus(st->edtNotchW);
            MSG typing{};typing.hwnd=st->edtNotchW;typing.message=WM_CHAR;typing.wParam='1';
            ok &= halljoy::main_input::Allow(typing,hostWindow);
            SetFocus(st->edtNotchW); SetWindowTextW(st->edtNotchW, L"13");
            ok &= st->draftKeys[0].notchW == 13 && st->history.CanUndo();
            SetWindowTextW(st->edtNotchW, L"66");
            ok &= st->draftKeys[0].notchW == 13;
            SetFocus(page); Layout_UndoRedo(page, st, false);
            ok &= st->draftKeys[0].notchW == 12;
            HWND shapeWindow = CreateWindowW(L"BUTTON", L"", WS_CHILD,0,0,66,86,page,nullptr,GetModuleHandleW(nullptr),nullptr);
            extern bool KeyboardUI_TestCompoundButtonPaint();
            extern bool KeyboardUI_TestPausePreview();
            if (!KeyboardUI_TestPausePreview()) throw std::runtime_error("pause preview production events failed");
            if (!KeyboardUI_TestCompoundButtonPaint()) throw std::runtime_error("native compound button paint overwrote notch");
            SetWindowLongPtrW(shapeWindow,GWLP_USERDATA,40); // Real Enter: exercise impact/selection animations too.
            HRGN region = CreateRectRgn(0,0,0,0);
            // Exact native progress-fill contour: all boundaries, including
            // the inner elbow, retain the same inset as rectangular keys.
            RECT innerBounds{0,0,66,86}; POINT innerNotch{12,40};
            const int inset = KeyShape_InnerInset(innerBounds,innerNotch);
            InflateRect(&innerBounds,-inset,-inset);
            innerNotch.y -= 2*inset;
            const auto innerPoints = KeyShape_Points(innerBounds,innerNotch);
            HRGN fillRegion = CreatePolygonRgn(innerPoints.data(),(int)innerPoints.size(),WINDING);
            ok &= inset==3 && PtInRegion(fillRegion,4,36) && !PtInRegion(fillRegion,4,38);
            ok &= !PtInRegion(fillRegion,13,60) && PtInRegion(fillRegion,15,60);
            for(int depth=0;depth<=80;++depth) {
                HRGN progress = CreateRectRgn(3,3,63,3+depth);
                CombineRgn(progress,progress,fillRegion,RGN_AND);
                ok &= !PtInRegion(progress,13,60) && !PtInRegion(progress,4,38);
                ok &= (PtInRegion(progress,16,60)!=FALSE) == (depth>57);
                DeleteObject(progress);
            }
            DeleteObject(fillRegion);
            for (int scale : {1,2,4,1}) {
                SetWindowPos(shapeWindow,nullptr,0,0,66*scale,86*scale,SWP_NOZORDER|SWP_NOACTIVATE);
                KeyShape_Set(shapeWindow,st->draftKeys[0],66*scale,86*scale);
                ok &= GetWindowRgn(shapeWindow,region) != ERROR;
                ok &= PtInRegion(region,5*scale,20*scale) && PtInRegion(region,20*scale,60*scale);
                ok &= !PtInRegion(region,5*scale,60*scale);
            }
            // Owner-draw can target an unclipped backing DC. The renderer must
            // preserve the neighbour's pixels itself, not rely on HWND clipping.
            HDC shapeScreen=GetDC(page), shapeDC=CreateCompatibleDC(shapeScreen);
            HBITMAP shapeBitmap=CreateCompatibleBitmap(shapeScreen,80,100);
            HGDIOBJ shapeOld=SelectObject(shapeDC,shapeBitmap);
            ReleaseDC(page,shapeScreen);
            HBRUSH neighbour=CreateSolidBrush(RGB(217,23,171));
            for (bool selected : {false,true}) for (float depth : {0.0f,0.25f,0.5f,1.0f}) {
                RECT target{0,0,80,100};FillRect(shapeDC,&target,neighbour);
                DRAWITEMSTRUCT draw{};draw.hwndItem=shapeWindow;draw.hDC=shapeDC;
                draw.rcItem={5,7,71,93};draw.itemState=selected?ODS_SELECTED:0;
                KeyboardRender_DrawKey(&draw,40,selected,depth);
                for(int y=40;y<86;++y) for(int x=0;x<12;++x)
                    ok &= GetPixel(shapeDC,5+x,7+y)==RGB(217,23,171);
                ok &= GetPixel(shapeDC,25,25)!=RGB(217,23,171);
                if (!selected) {
                    ok &= GetPixel(shapeDC,5+13,7+60)==UiTheme::Color_ControlBg();
                    ok &= GetPixel(shapeDC,5+5,7+38)==UiTheme::Color_ControlBg();
                }
            }
            DeleteObject(neighbour);SelectObject(shapeDC,shapeOld);DeleteObject(shapeBitmap);DeleteDC(shapeDC);
            if (!ok) throw std::runtime_error("compound compositor overwrote notch pixels");
            // Match main preview's actual order: shape first, deferred resize
            // second; switch rectangular/compound repeatedly on the same HWND.
            for (int scale : {4,1,2,1}) {
                KeyShape_Set(shapeWindow,originalKey,66*scale,86*scale);
                KeyShape_Set(shapeWindow,st->draftKeys[0],66*scale,86*scale);
                SetWindowPos(shapeWindow,nullptr,0,0,66*scale,86*scale,SWP_NOZORDER|SWP_NOACTIVATE|SWP_NOREDRAW);
                ok &= GetWindowRgn(shapeWindow,region) != ERROR;
                ok &= !PtInRegion(region,5*scale,60*scale);
            }
            KeyShape_Set(shapeWindow,originalKey,66,86);
            ok &= !KeyShape_Get(shapeWindow).x && GetWindowRgn(shapeWindow,region) == ERROR;
            DeleteObject(region); DestroyWindow(shapeWindow);
            st->draftKeys[0] = originalKey; st->selectedIdx = -1;
            st->history.Reset(Layout_CaptureDraft(st));
            Layout_SetUnsaved(st,false); Layout_UpdateMetaControls(st);
            if (!ok) throw std::runtime_error("compound key editor/region checks failed");
            ok &= FindWindowExW(page, nullptr, L"LISTBOX", nullptr) == nullptr && !st->history.CanUndo();
            SendMessageW(page, WM_COMMAND, MAKEWPARAM(ID_LAYOUT_ADD, BN_CLICKED), 0);
            SendMessageW(page, WM_COMMAND, MAKEWPARAM(ID_LAYOUT_UNDO, BN_CLICKED), 0);
            ok &= st->draftKeys.size() == count && !st->hasUnsaved;
            SendMessageW(page, WM_COMMAND, MAKEWPARAM(ID_LAYOUT_REDO, BN_CLICKED), 0);
            ok &= st->draftKeys.size() == count + 1 && st->hasUnsaved;
            Layout_UndoRedo(page, st, false);
            st->selectedIdx = 0; st->history.Select(0); st->preciseY = true;
            Layout_UpdateMetaControls(st);
            const int initialY = KeyboardLayout_KeyY(st->draftKeys[0]);
            SetFocus(st->edtY);
            SetWindowTextW(st->edtY, std::to_wstring(initialY + 23).c_str());
            ok &= KeyboardLayout_KeyY(st->draftKeys[0]) == initialY + 23 && st->hasUnsaved;
            const int originalRow = st->draftKeys[0].row;
            SendMessageW(page, WM_COMMAND, MAKEWPARAM(ID_LAYOUT_PRECISION, BN_CLICKED), 0);
            const int snappedRow = std::clamp((int)std::lround((initialY + 23) / (double)KEYBOARD_ROW_PITCH_Y), 0, 20);
            ok &= !st->preciseY && st->draftKeys[0].row == snappedRow && st->draftKeys[0].y == -1;
            ok &= KeyboardLayout_KeyY(st->draftKeys[0]) == snappedRow * KEYBOARD_ROW_PITCH_Y;
            Layout_UndoRedo(page, st, false);
            ok &= st->draftKeys[0].row == originalRow && KeyboardLayout_KeyY(st->draftKeys[0]) == initialY + 23;
            SetFocus(st->edtY);
            SetWindowTextW(st->edtY, L"12junk");
            ok &= KeyboardLayout_KeyY(st->draftKeys[0]) == initialY + 23;
            Layout_UndoRedo(page, st, false);
            ok &= KeyboardLayout_KeyY(st->draftKeys[0]) == initialY && !st->hasUnsaved;
            st->view.ready = true; st->view.scale = 1; st->view.x = 0; st->view.y = 0;
            st->selectedIdx = 0; st->history.Select(0); st->preciseY = true;
            const int initialX = st->draftKeys[0].x;
            RECT keyRect = Layout_KeyRectOnCanvas(st, 0, st->canvasRc);
            st->dragOffsetX = 0; st->dragOffsetY = 0; st->dragging = true;
            Layout_ApplyDrag(page, st, POINT{keyRect.left + 1, keyRect.top + 1});
            ValidateRect(page, nullptr);
            SendMessageW(page, WM_CAPTURECHANGED, 0, 0);
            ok &= GetUpdateRect(page, nullptr, FALSE) != FALSE;
            ok &= st->draftKeys[0].x == initialX + 1 && KeyboardLayout_KeyY(st->draftKeys[0]) == initialY + 1;
            ok &= st->view.scale == 1 && st->view.x == 0 && st->view.y == 0;
            Layout_UndoRedo(page, st, false);
            ok &= !st->hasUnsaved && st->draftKeys[0].x == initialX;
            const int originalWidth = st->draftKeys[0].w, originalHeight = st->draftKeys[0].h;
            st->selectedIdx = 0; st->history.Select(0);
            keyRect = Layout_KeyRectOnCanvas(st, 0, st->canvasRc);
            const POINT corner{keyRect.right - 2, keyRect.bottom - 2};
            SendMessageW(page, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(corner.x, corner.y));
            SendMessageW(page, WM_MOUSEMOVE, MK_LBUTTON, MAKELPARAM(corner.x + 1, corner.y + 1));
            ValidateRect(page, nullptr);
            SendMessageW(page, WM_LBUTTONUP, 0, MAKELPARAM(corner.x + 1, corner.y + 1));
            ok &= GetUpdateRect(page, nullptr, FALSE) != FALSE && !st->dragging && !st->resizing;
            ok &= st->draftKeys[0].w == originalWidth + 1 && st->draftKeys[0].h == originalHeight + 1;
            Layout_UndoRedo(page, st, false);
            ok &= st->draftKeys[0].w == originalWidth && st->draftKeys[0].h == originalHeight && !st->hasUnsaved;
            const auto dimensions = Layout_CaptureDraft(st);
            using namespace halljoy::layout_editor;
            const int edgeCases[] = {Left, Right, Top, Bottom, Left | Top, Right | Top, Left | Bottom, Right | Bottom};
            for (int edges : edgeCases) {
                st->selectedIdx = 0; st->history.Select(0); st->preciseY = true;
                const RECT r = Layout_KeyRectOnCanvas(st, 0, st->canvasRc);
                POINT p{(r.left + r.right) / 2, (r.top + r.bottom) / 2};
                if (edges & Left) p.x = r.left + 2;
                if (edges & Right) p.x = r.right - 2;
                if (edges & Top) p.y = r.top + 2;
                if (edges & Bottom) p.y = r.bottom - 2;
                ok &= Layout_ResizeEdges(st, p) == edges;
                const auto keyBefore = st->draftKeys[0];
                const auto expected = Resize({keyBefore.x, KeyboardLayout_KeyY(keyBefore), keyBefore.w, keyBefore.h}, edges, 1, 1, true);
                SendMessageW(page, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(p.x, p.y));
                SendMessageW(page, WM_MOUSEMOVE, MK_LBUTTON, MAKELPARAM(p.x + 1, p.y + 1));
                SendMessageW(page, WM_LBUTTONUP, 0, MAKELPARAM(p.x + 1, p.y + 1));
                const auto& after = st->draftKeys[0];
                ok &= after.x == expected.x && KeyboardLayout_KeyY(after) == expected.y && after.w == expected.w && after.h == expected.h;
                Layout_UndoRedo(page, st, false);
                ok &= dimensions.Same(Layout_CaptureDraft(st));
            }
            st->preciseY = false;
            const RECT topKey = Layout_KeyRectOnCanvas(st, 0, st->canvasRc);
            ok &= Layout_ResizeEdges(st, POINT{(topKey.left + topKey.right) / 2, topKey.top + 2}) == 0;
            // Guide creation/movement/deletion is editor-only and never dirties the preset.
            const POINT rulerStart{st->topRuler.left + 40, st->topRuler.top + 8};
            const POINT guideEnd{st->canvasRc.left + 200, st->canvasRc.top + KEYBOARD_MARGIN_Y + 200};
            SendMessageW(page, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(rulerStart.x, rulerStart.y));
            SendMessageW(page, WM_MOUSEMOVE, MK_LBUTTON, MAKELPARAM(guideEnd.x, guideEnd.y));
            SendMessageW(page, WM_LBUTTONUP, 0, MAKELPARAM(guideEnd.x, guideEnd.y));
            ok &= st->guides.size() == 1 && !st->guides[0].vertical && st->guides[0].coordinate == 200;
            ok &= dimensions.Same(Layout_CaptureDraft(st)) && !st->hasUnsaved;
            SendMessageW(page, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(guideEnd.x, guideEnd.y));
            SendMessageW(page, WM_MOUSEMOVE, MK_LBUTTON, MAKELPARAM(guideEnd.x, guideEnd.y + 10));
            SendMessageW(page, WM_KEYDOWN, VK_ESCAPE, 0);
            ok &= st->draggingGuide == -1 && st->guides[0].coordinate == 200;
            SendMessageW(page, WM_RBUTTONUP, 0, MAKELPARAM(guideEnd.x, guideEnd.y));
            ok &= st->guides.empty();
            SendMessageW(page, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(rulerStart.x, rulerStart.y));
            SendMessageW(page, WM_KEYDOWN, VK_ESCAPE, 0);
            ok &= st->guides.empty() && dimensions.Same(Layout_CaptureDraft(st));
            SendMessageW(page, WM_MBUTTONDOWN, MK_MBUTTON, MAKELPARAM(st->canvasRc.left + 20, st->canvasRc.top + 20));
            SendMessageW(page, WM_MOUSEMOVE, MK_MBUTTON, MAKELPARAM(st->canvasRc.left + 37, st->canvasRc.top + 29));
            SendMessageW(page, WM_MBUTTONUP, 0, 0);
            ok &= st->view.x == 17 && st->view.y == 9 && dimensions.Same(Layout_CaptureDraft(st));
            // Wheel during a captured resize/move must change only the camera,
            if (!ok) throw std::runtime_error("editor pre-wheel checks failed");
            // preserve capture, and keep the next delta at the new model scale.
            auto wheelAt = [&](POINT client, short delta) {
                POINT screen = client; ClientToScreen(page, &screen);
                SendMessageW(page, WM_MOUSEWHEEL, MAKEWPARAM(MK_LBUTTON, delta), MAKELPARAM(screen.x, screen.y));
            };
            for (bool resize : {true, false}) {
                st->view.scale = 1; st->view.x = 0; st->view.y = 0;
                st->selectedIdx = 0; st->history.Select(0); st->preciseY = true;
                const RECT r = Layout_KeyRectOnCanvas(st, 0, st->canvasRc);
                const POINT p = resize ? POINT{r.right - 2, r.bottom - 2}
                    : POINT{(r.left + r.right) / 2, (r.top + r.bottom) / 2};
                SendMessageW(page, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(p.x, p.y));
                wheelAt(p, 120);
                ok &= st->view.scale > 1 && st->dragging && GetCapture() == page && dimensions.Same(Layout_CaptureDraft(st));
                SendMessageW(page, WM_MOUSEMOVE, MK_LBUTTON, MAKELPARAM(p.x, p.y));
                ok &= dimensions.Same(Layout_CaptureDraft(st));
                SendMessageW(page, WM_MOUSEMOVE, MK_LBUTTON, MAKELPARAM(p.x + 2, p.y + 2));
                const auto moved = Layout_CaptureDraft(st);
                wheelAt(POINT{p.x + 2, p.y + 2}, -120);
                ok &= moved.Same(Layout_CaptureDraft(st)) && GetCapture() == page;
                SendMessageW(page, WM_LBUTTONUP, 0, MAKELPARAM(p.x + 2, p.y + 2));
                Layout_UndoRedo(page, st, false);
                ok &= dimensions.Same(Layout_CaptureDraft(st)) && !st->hasUnsaved;
            }
            if (!ok) throw std::runtime_error("editor captured wheel checks failed");
            st->view.scale = 1; st->view.x = 0; st->view.y = 0;
            SendMessageW(page, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(rulerStart.x, rulerStart.y));
            SendMessageW(page, WM_MOUSEMOVE, MK_LBUTTON, MAKELPARAM(guideEnd.x, guideEnd.y));
            wheelAt(guideEnd, 120);
            ok &= st->view.scale > 1 && st->draggingGuide >= 0 && GetCapture() == page;
            SendMessageW(page, WM_MOUSEMOVE, MK_LBUTTON, MAKELPARAM(guideEnd.x, guideEnd.y));
            ok &= st->guides.size() == 1 && st->guides[0].coordinate == 200 && dimensions.Same(Layout_CaptureDraft(st));
            SendMessageW(page, WM_KEYDOWN, VK_ESCAPE, 0);
            ok &= st->guides.empty();
            // Offscreen code-only regression: every owner-drawn corner must
            if (!ok) throw std::runtime_error("editor guide wheel checks failed");
            // overwrite the native background, including disabled/focused states.
            HDC screenDC = GetDC(page), testDC = CreateCompatibleDC(screenDC);
            HBITMAP bitmap = CreateCompatibleBitmap(screenDC, 132, 32);
            HGDIOBJ oldBitmap = SelectObject(testDC, bitmap);
            ReleaseDC(page, screenDC);
            HBRUSH sentinel = CreateSolidBrush(RGB(255, 0, 255));
            for (UINT state : {0u, (UINT)ODS_DISABLED, (UINT)ODS_FOCUS, (UINT)ODS_SELECTED}) {
                DRAWITEMSTRUCT draw{}; draw.CtlType = ODT_BUTTON; draw.CtlID = ID_LAYOUT_UNDO;
                draw.itemAction = ODA_DRAWENTIRE; draw.itemState = state;
                draw.hwndItem = st->btnUndo; draw.hDC = testDC; draw.rcItem = {0, 0, 132, 32};
                FillRect(testDC, &draw.rcItem, sentinel);
                SendMessageW(page, WM_DRAWITEM, ID_LAYOUT_UNDO, (LPARAM)&draw);
                COLORREF corners[4]{}; int cornerIndex = 0;
                for (int x : {0, 131}) for (int y : {0, 31}) {
                    const COLORREF color = GetPixel(testDC, x, y);
                    ok &= color != CLR_INVALID && color != RGB(255, 0, 255);
                    corners[cornerIndex++] = color;
                }
                const COLORREF first = GetPixel(testDC, 15, 8);
                FillRect(testDC, &draw.rcItem, (HBRUSH)GetStockObject(WHITE_BRUSH));
                SendMessageW(page, WM_DRAWITEM, ID_LAYOUT_UNDO, (LPARAM)&draw);
                ok &= first == GetPixel(testDC, 15, 8);
                cornerIndex = 0;
                for (int x : {0, 131}) for (int y : {0, 31})
                    ok &= corners[cornerIndex++] == GetPixel(testDC, x, y);
            }
            DeleteObject(sentinel); SelectObject(testDC, oldBitmap); DeleteObject(bitmap); DeleteDC(testDC);
            if (!ok) throw std::runtime_error("editor owner-draw coverage checks failed");
            RECT snapRect{}; GetClientRect(st->btnSnap, &snapRect);
            const LPARAM clickPoint = MAKELPARAM(snapRect.right / 2, snapRect.bottom / 2);
            for (int click = 0; click < 8; ++click) {
                const bool beforeClick = st->snap;
                SendMessageW(st->btnSnap, click % 2 ? WM_LBUTTONDBLCLK : WM_LBUTTONDOWN, MK_LBUTTON, clickPoint);
                ok &= st->snap == beforeClick; // No action before release.
                SendMessageW(st->btnSnap, WM_LBUTTONUP, 0, clickPoint);
                ok &= st->snap != beforeClick;
            }
            const bool beforeCancel = st->snap;
            SendMessageW(st->btnSnap, WM_LBUTTONDBLCLK, MK_LBUTTON, clickPoint);
            SendMessageW(st->btnSnap, WM_CANCELMODE, 0, 0);
            SendMessageW(st->btnSnap, WM_LBUTTONUP, 0, clickPoint);
            ok &= st->snap == beforeCancel;
            if (!ok) throw std::runtime_error("editor repeated native button clicks failed");
            st->preciseY = false;
            SendMessageW(page, WM_COMMAND, MAKEWPARAM(ID_LAYOUT_ADD, BN_CLICKED), 0);
            ok &= st->hasUnsaved && st->draftKeys.size() == count + 1;
            g_layoutTestDecision = IDCANCEL;
            SendMessageW(hostWindow, WM_CLOSE, 0, 0);
            ok &= IsWindow(hostWindow) && st->hasUnsaved;
            PremiumCombo::SetCurSel(st->cmbPreset, st->layoutPicker.Row(1), false);
            SendMessageW(page, WM_COMMAND, MAKEWPARAM(ID_LAYOUT_PRESET, CBN_SELCHANGE), (LPARAM)st->cmbPreset);
            ok &= st->editingPresetIdx == 0 && st->layoutPicker.Selected() == 0;
            SendMessageW(page, WM_COMMAND, MAKEWPARAM(ID_LAYOUT_RESET, BN_CLICKED), 0);
            ok &= st->draftKeys.size() == count + 1;
            // Save failures at every transaction stage must retain the actual UI draft.
            g_layoutTestDecision = IDYES;
            const auto before = KeyboardLayout_GetSnapshot();
            for (auto stage : { HallJoyPersistence::SaveStage::Prepare, HallJoyPersistence::SaveStage::Write,
                HallJoyPersistence::SaveStage::Flush, HallJoyPersistence::SaveStage::Validate,
                HallJoyPersistence::SaveStage::Replace })
            {
                IniUtil_TestSetFailureStage(stage);
                const bool closed = KeyboardUI_CloseLayoutEditor();
                IniUtil_TestSetFailureStage(HallJoyPersistence::SaveStage::None);
                ok &= !closed && IsWindow(hostWindow) && st->hasUnsaved;
                ok &= KeyboardLayout_GetSnapshot() == before;
            }
            // Discard followed by a preset change does not save the old draft.
            g_layoutTestDecision = IDNO;
            PremiumCombo::SetCurSel(st->cmbPreset, st->layoutPicker.Row(1), false);
            SendMessageW(page, WM_COMMAND, MAKEWPARAM(ID_LAYOUT_PRESET, CBN_SELCHANGE), (LPARAM)st->cmbPreset);
            ok &= st->editingPresetIdx == 1 && !st->hasUnsaved;
            std::vector<KeyDef> saved; std::vector<std::wstring> labels;
            KeyboardLayout_GetPresetSnapshot(0, saved, labels, nullptr, nullptr);
            ok &= saved.size() == count;
            // Interrupted dragging still marks the changed geometry as unsaved.
            st->draftKeys[0].x += 1; st->dirty = true; st->dragging = true;
            SendMessageW(page, WM_CAPTURECHANGED, 0, 0);
            ok &= st->hasUnsaved && !st->dirty && !st->dragging;
            st->draftKeys[0].x -= 1;
            Layout_SetUnsaved(st, true);
            ok &= !st->hasUnsaved;
            st->resolvingDraft = true;
            ok &= !KeyboardUI_CloseLayoutEditor();
            st->resolvingDraft = false;
            st->selectedIdx = 0;
            Layout_UpdateMetaControls(st);
            SetFocus(st->edtLabel);
            SetWindowTextW(st->edtLabel, L"Test label");
            ok &= st->draftLabels[0] == L"Test label" && st->hasUnsaved;
            g_layoutTestDecision = IDYES;
            ok &= KeyboardUI_CloseLayoutEditor();
            ok &= !IsWindow(hostWindow);
            KeyboardLayout_GetPresetSnapshot(1, saved, labels, nullptr, nullptr);
            ok &= labels[0] == L"Test label";
            LayoutEditor_OpenWindow(owner, 0);
            hostWindow = g_hLayoutEditorWindow;
            host = (LayoutEditorHostState*)GetWindowLongPtrW(hostWindow, GWLP_USERDATA);
            if (host) SendMessageW(host->hPage, WM_COMMAND, MAKEWPARAM(ID_LAYOUT_ADD, BN_CLICKED), 0);
            g_layoutTestDecision = IDNO;
            ok &= KeyboardUI_CloseLayoutEditor() && !IsWindow(hostWindow);
            KeyboardLayout_GetPresetSnapshot(0, saved, labels, nullptr, nullptr);
            ok &= saved.size() == count;
        }
        int first = -1, second = -1;
        if (KeyboardLayout_CreatePreset(L"Editor Delete A", &first) &&
            KeyboardLayout_CreatePreset(L"Editor Delete B", &second))
        {
            LayoutEditor_OpenWindow(owner, second);
            const HWND window = g_hLayoutEditorWindow;
            auto* currentHost = (LayoutEditorHostState*)GetWindowLongPtrW(window, GWLP_USERDATA);
            auto* draft = currentHost ? (LayoutPageState*)GetWindowLongPtrW(currentHost->hPage, GWLP_USERDATA) : nullptr;
            if (draft)
            {
                SendMessageW(currentHost->hPage, WM_COMMAND, MAKEWPARAM(ID_LAYOUT_ADD, BN_CLICKED), 0);
                g_layoutTestDecision = IDCANCEL;
                ok &= LayoutEditor_DeletePreset(first) && IsWindow(window) && draft->hasUnsaved;
                ok &= draft->editingPresetIdx == second - 1 &&
                    std::wstring(KeyboardLayout_GetPresetName(draft->editingPresetIdx)) == L"Editor Delete B";
                ok &= !LayoutEditor_DeletePreset(second - 1) && IsWindow(window);
                g_layoutTestDecision = IDNO;
                ok &= LayoutEditor_DeletePreset(second - 1) && !IsWindow(window);
            }
            else ok = false;
        }
        else ok = false;
        // Manage the shared catalog through the real editor combo, without
        if (!ok) throw std::runtime_error("editor pre-catalog checks failed");
        // changing the independently selected main preview layout.
        KeyboardLayout_SetPresetIndex(1);
        const int mainBeforeCreate = KeyboardLayout_GetCurrentPresetIndex();
        const int catalogBefore = KeyboardLayout_GetPresetCount();
        LayoutEditor_OpenWindow(owner, 0);
        auto* catalogHost = (LayoutEditorHostState*)GetWindowLongPtrW(g_hLayoutEditorWindow, GWLP_USERDATA);
        auto* catalogDraft = catalogHost ? (LayoutPageState*)GetWindowLongPtrW(catalogHost->hPage, GWLP_USERDATA) : nullptr;
        if (catalogDraft) {
            const HWND page = catalogHost->hPage;
            size_t brandModels=0;
            for (int i=0;i<KeyboardLayout_GetPresetCount();++i)
                if (KeyboardLayout_GetPresetBrand(i)==L"DrunkDeer") ++brandModels;
            size_t represented=0;
            for (const auto& group : catalogDraft->layoutPicker.groups) represented+=group.members.size();
            ok &= represented == brandModels+1; // Every concrete layout plus Create.
            PremiumCombo::SetCurSel(catalogDraft->cmbPreset, catalogDraft->layoutPicker.Row(LayoutPicker::Create), false);
            SendMessageW(page, WM_COMMAND, MAKEWPARAM(ID_LAYOUT_PRESET, CBN_SELCHANGE), (LPARAM)catalogDraft->cmbPreset);
            const HWND edit = GetFocus(); wchar_t editClass[32]{}; GetClassNameW(edit, editClass, 32);
            if (wcscmp(editClass, L"Edit") == 0 || wcscmp(editClass, L"EDIT") == 0) {
                SetWindowTextW(edit, L"Editor Combo Created");
                SendMessageW(edit, WM_KEYDOWN, VK_RETURN, 0);
                MSG commit{};
                while (PeekMessageW(&commit, page, PremiumCombo::MsgItemTextCommit(), PremiumCombo::MsgItemTextCommit(), PM_REMOVE))
                    DispatchMessageW(&commit);
                ok &= KeyboardLayout_GetPresetCount() == catalogBefore + 1 && catalogDraft->editingPresetIdx == catalogBefore;
                ok &= KeyboardLayout_GetCurrentPresetIndex() == mainBeforeCreate;
                std::vector<KeyDef> source, copy; std::vector<std::wstring> sourceLabels, copyLabels;
                KeyboardLayout_GetPresetSnapshot(0, source, sourceLabels, nullptr, nullptr);
                KeyboardLayout_GetPresetSnapshot(catalogBefore, copy, copyLabels, nullptr, nullptr);
                ok &= sourceLabels == copyLabels && source.size() == copy.size();
                if (!ok) throw std::runtime_error("editor combo create/source checks failed");
                SendMessageW(page, WM_COMMAND, MAKEWPARAM(ID_LAYOUT_ADD, BN_CLICKED), 0);
                const int createdRow = catalogDraft->layoutPicker.Row(catalogBefore);
                ok &= createdRow >= 0 && createdRow != catalogBefore;
                ok &= KeyboardLayout_GetPresetBrand(catalogBefore) == L"DrunkDeer";
                const WPARAM deletion = MAKEWPARAM(createdRow, (WORD)PremiumCombo::ItemButtonKind::Delete);
                const WPARAM confirmation = MAKEWPARAM(createdRow, (WORD)PremiumCombo::ItemButtonKind::ConfirmDelete);
                g_layoutTestDecision = IDCANCEL;
                PremiumCombo::ShowDropDown(catalogDraft->cmbPreset, true);
                SendMessageW(page, PremiumCombo::MsgItemButton(), deletion, (LPARAM)catalogDraft->cmbPreset);
                ok &= PremiumCombo::GetDeleteConfirmation(catalogDraft->cmbPreset) == createdRow;
                auto* comboState = PremiumComboInternal::Get(catalogDraft->cmbPreset);
                for (auto action : {PremiumCombo::ItemButtonKind::ConfirmDelete, PremiumCombo::ItemButtonKind::CancelDelete}) {
                    const RECT button = PremiumComboInternal::GetPopupItemButtonRect(comboState, createdRow, action);
                    ok &= button.right > button.left && button.bottom > button.top;
                    POINT point{(button.left + button.right) / 2, (button.top + button.bottom) / 2};
                    ClientToScreen(comboState->hwndPopup, &point);
                    int hit = -1; PremiumCombo::ItemButtonKind kind{}; bool inside = false;
                    ok &= PremiumComboInternal::HitTestPopupItemButtonFromScreen(comboState, point, hit, kind, inside);
                    ok &= inside && hit == createdRow && kind == action;
                }
                PremiumCombo::ShowDropDown(catalogDraft->cmbPreset, false);
                ok &= PremiumCombo::GetDeleteConfirmation(catalogDraft->cmbPreset) == -1;
                SendMessageW(page, PremiumCombo::MsgItemButton(), confirmation, (LPARAM)catalogDraft->cmbPreset);
                ok &= KeyboardLayout_GetPresetCount() == catalogBefore + 1;
                SendMessageW(page, PremiumCombo::MsgItemButton(), deletion, (LPARAM)catalogDraft->cmbPreset);
                SendMessageW(page, PremiumCombo::MsgItemButton(), confirmation, (LPARAM)catalogDraft->cmbPreset);
                ok &= KeyboardLayout_GetPresetCount() == catalogBefore + 1 && catalogDraft->hasUnsaved;
                if (!ok) throw std::runtime_error("editor combo delete cancellation failed");
                g_layoutTestDecision = IDNO;
                SendMessageW(page, PremiumCombo::MsgItemButton(), deletion, (LPARAM)catalogDraft->cmbPreset);
                SendMessageW(page, PremiumCombo::MsgItemButton(), confirmation, (LPARAM)catalogDraft->cmbPreset);
                ok &= KeyboardLayout_GetPresetCount() == catalogBefore && IsWindow(page) && !catalogDraft->hasUnsaved;
                ok &= KeyboardLayout_GetCurrentPresetIndex() == mainBeforeCreate;
                if (!ok) throw std::runtime_error("editor combo delete completion failed");
            } else throw std::runtime_error("editor combo inline edit did not focus");
        } else ok = false;
        g_layoutTestDecision = IDNO;
        KeyboardUI_CloseLayoutEditor();
        g_layoutTestDecision = 0;
        DestroyWindow(owner);
    }
    ok &= SetThreadDesktop(previous) != FALSE;
    CloseDesktop(desktop);
    return ok;
}
#endif

static Color Gp(COLORREF c, BYTE a) { return Color(a, GetRValue(c), GetGValue(c), GetBValue(c)); }

struct PremiumSliderState
{
    int minV = 1;
    int maxV = 20;
    int posV = 5;
    bool dragging = false;
};

static int PremiumSlider_Clamp(const PremiumSliderState* st, int v)
{
    if (!st) return v;
    return std::clamp(v, st->minV, st->maxV);
}

static float PremiumSlider_ValueToT(const PremiumSliderState* st)
{
    if (!st) return 0.0f;
    int den = (st->maxV - st->minV);
    if (den <= 0) return 0.0f;
    return (float)(st->posV - st->minV) / (float)den;
}

static int PremiumSlider_XToValue(const PremiumSliderState* st, int x, int w, int pad)
{
    if (!st) return 0;
    int usable = w - pad * 2;
    if (usable <= 1) return st->minV;

    float t = (float)(x - pad) / (float)usable;
    t = std::clamp(t, 0.0f, 1.0f);

    float v = (float)st->minV + t * (float)(st->maxV - st->minV);
    int iv = (int)lroundf(v);
    return PremiumSlider_Clamp(st, iv);
}

static void PremiumSlider_Notify(HWND hWnd, int code)
{
    HWND parent = GetParent(hWnd);
    if (!parent) return;
    PostMessageW(parent, WM_HSCROLL, (WPARAM)code, (LPARAM)hWnd);
}

static void AddRoundRectPath(GraphicsPath& path, const RectF& r, float rad)
{
    float rr = std::clamp(rad, 0.0f, std::min(r.Width, r.Height) * 0.5f);
    float d = rr * 2.0f;
    RectF arc(r.X, r.Y, d, d);

    path.StartFigure();
    path.AddArc(arc, 180, 90);
    arc.X = r.GetRight() - d; path.AddArc(arc, 270, 90);
    arc.Y = r.GetBottom() - d; path.AddArc(arc, 0, 90);
    arc.X = r.X; path.AddArc(arc, 90, 90);
    path.CloseFigure();
}

static void PremiumSlider_Paint(HWND hWnd, HDC hdc)
{
    PremiumSliderState* st = (PremiumSliderState*)GetWindowLongPtrW(hWnd, GWLP_USERDATA);
    RECT rc{};
    GetClientRect(hWnd, &rc);

    int w = rc.right - rc.left;
    int h = rc.bottom - rc.top;
    if (w <= 2 || h <= 2) return;

    Graphics g(hdc);
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    g.SetPixelOffsetMode(PixelOffsetModeHighQuality);

    SolidBrush bg(Gp(UiTheme::Color_PanelBg()));
    g.FillRectangle(&bg, (REAL)0, (REAL)0, (REAL)w, (REAL)h);

    int pad = std::clamp(h / 3, 8, 14);
    int trackH = std::clamp(h / 5, 6, 10);
    int cy = h / 2;

    RectF track((REAL)pad, (REAL)(cy - trackH / 2), (REAL)(w - pad * 2), (REAL)trackH);
    float rr = track.Height * 0.5f;

    {
        SolidBrush br(Gp(RGB(55, 55, 55)));
        GraphicsPath p;
        AddRoundRectPath(p, track, rr);
        g.FillPath(&br, &p);

        Pen border(Gp(UiTheme::Color_Border()), 1.0f);
        g.DrawPath(&border, &p);
    }

    float t = PremiumSlider_ValueToT(st);

    RectF fill = track;
    fill.Width = std::max(0.0f, track.Width * t);

    if (fill.Width > 0.5f)
    {
        Color accent = Gp(UiTheme::Color_Accent());
        Color accent2(
            255,
            (BYTE)std::min(255, (int)accent.GetR() + 18),
            (BYTE)std::min(255, (int)accent.GetG() + 18),
            (BYTE)std::min(255, (int)accent.GetB() + 18));

        LinearGradientBrush grad(fill, accent2, accent, LinearGradientModeVertical);

        GraphicsPath p;
        AddRoundRectPath(p, fill, rr);
        g.FillPath(&grad, &p);
    }

    float knobX = track.X + track.Width * t;
    float knobR = std::clamp((float)h * 0.22f, 7.0f, 12.0f);

    SolidBrush knobFill(Gp(RGB(235, 235, 235)));
    Pen knobBorder(Gp(RGB(15, 15, 15), 220), 1.5f);

    RectF knob(knobX - knobR, (REAL)cy - knobR, knobR * 2.0f, knobR * 2.0f);
    g.FillEllipse(&knobFill, knob);
    g.DrawEllipse(&knobBorder, knob);

    if (st && st->dragging)
    {
        Pen ring(Gp(UiTheme::Color_Accent(), 230), 2.5f);
        g.DrawEllipse(&ring, RectF(knob.X - 2.0f, knob.Y - 2.0f, knob.Width + 4.0f, knob.Height + 4.0f));
    }
}

static LRESULT CALLBACK PremiumSliderProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    PremiumSliderState* st = (PremiumSliderState*)GetWindowLongPtrW(hWnd, GWLP_USERDATA);

    switch (msg)
    {
    case WM_NCCREATE:
        return TRUE;

    case WM_CREATE:
    {
        st = new PremiumSliderState();
        st->posV = (int)Settings_GetPollingMs();
        SetWindowLongPtrW(hWnd, GWLP_USERDATA, (LONG_PTR)st);
        return 0;
    }

    case WM_NCDESTROY:
        if (st)
        {
            delete st;
            SetWindowLongPtrW(hWnd, GWLP_USERDATA, 0);
        }
        return 0;

    case WM_ERASEBKGND:
        return 1;

    case WM_PAINT:
    {
        PAINTSTRUCT ps{};
        HDC hdc = BeginPaint(hWnd, &ps);
        PremiumSlider_Paint(hWnd, hdc);
        EndPaint(hWnd, &ps);
        return 0;
    }

    case WM_LBUTTONDOWN:
    {
        if (!st) break;
        SetFocus(hWnd);
        SetCapture(hWnd);
        st->dragging = true;

        RECT rc{};
        GetClientRect(hWnd, &rc);
        int w = rc.right - rc.left;
        int h = rc.bottom - rc.top;
        int pad = std::clamp(h / 3, 8, 14);

        int x = (short)LOWORD(lParam);
        int nv = PremiumSlider_XToValue(st, x, w, pad);
        if (nv != st->posV)
        {
            st->posV = nv;
            InvalidateRect(hWnd, nullptr, FALSE);
            PremiumSlider_Notify(hWnd, SB_THUMBTRACK);
        }
        else
        {
            InvalidateRect(hWnd, nullptr, FALSE);
        }
        return 0;
    }

    case WM_MOUSEMOVE:
    {
        if (!st || !st->dragging) break;

        RECT rc{};
        GetClientRect(hWnd, &rc);
        int w = rc.right - rc.left;
        int h = rc.bottom - rc.top;
        int pad = std::clamp(h / 3, 8, 14);

        int x = (short)LOWORD(lParam);
        int nv = PremiumSlider_XToValue(st, x, w, pad);
        if (nv != st->posV)
        {
            st->posV = nv;
            InvalidateRect(hWnd, nullptr, FALSE);
            PremiumSlider_Notify(hWnd, SB_THUMBTRACK);
        }
        return 0;
    }

    case WM_LBUTTONUP:
    {
        if (!st) break;
        if (st->dragging)
        {
            st->dragging = false;
            ReleaseCapture();
            InvalidateRect(hWnd, nullptr, FALSE);
            PremiumSlider_Notify(hWnd, SB_ENDSCROLL);
            PremiumSlider_Notify(hWnd, SB_THUMBPOSITION);
        }
        return 0;
    }

    case WM_KEYDOWN:
    {
        if (!st) break;

        if (wParam == VK_LEFT || wParam == VK_DOWN) st->posV -= 1;
        else if (wParam == VK_RIGHT || wParam == VK_UP) st->posV += 1;
        else break;

        st->posV = PremiumSlider_Clamp(st, st->posV);
        InvalidateRect(hWnd, nullptr, FALSE);
        PremiumSlider_Notify(hWnd, SB_THUMBPOSITION);
        return 0;
    }

    case TBM_SETRANGE:
    {
        if (!st) break;
        int minV = (int)LOWORD(lParam);
        int maxV = (int)HIWORD(lParam);
        if (minV > maxV) std::swap(minV, maxV);
        st->minV = minV;
        st->maxV = maxV;
        st->posV = PremiumSlider_Clamp(st, st->posV);
        InvalidateRect(hWnd, nullptr, FALSE);
        return 0;
    }

    case TBM_SETPOS:
    {
        if (!st) break;
        st->posV = PremiumSlider_Clamp(st, (int)lParam);
        InvalidateRect(hWnd, nullptr, FALSE);
        return 0;
    }

    case TBM_GETPOS:
    {
        if (!st) break;
        return (LRESULT)st->posV;
    }

    case WM_GETDLGCODE:
        return DLGC_WANTARROWS;
    }

    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

static HWND PremiumSlider_Create(HWND parent, HINSTANCE hInst, int x, int y, int w, int h, int id)
{
    static bool reg = false;
    if (!reg)
    {
        WNDCLASSW wc{};
        wc.lpfnWndProc = PremiumSliderProc;
        wc.hInstance = hInst;
        wc.lpszClassName = L"PremiumSlider";
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.hbrBackground = nullptr;
        RegisterClassW(&wc);
        reg = true;
    }

    return CreateWindowW(L"PremiumSlider", L"",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP,
        x, y, w, h,
        parent, (HMENU)(INT_PTR)id, hInst, nullptr);
}

// ---------------- Premium value chip ----------------
struct PremiumChipState
{
    wchar_t text[64]{};
};

static void PremiumChip_Paint(HWND hWnd, HDC hdc)
{
    PremiumChipState* st = (PremiumChipState*)GetWindowLongPtrW(hWnd, GWLP_USERDATA);

    RECT rc{};
    GetClientRect(hWnd, &rc);
    int w = rc.right - rc.left;
    int h = rc.bottom - rc.top;
    if (w <= 2 || h <= 2) return;

    Graphics g(hdc);
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    g.SetPixelOffsetMode(PixelOffsetModeHighQuality);
    g.SetTextRenderingHint(TextRenderingHintAntiAliasGridFit);

    SolidBrush bg(Gp(UiTheme::Color_PanelBg()));
    g.FillRectangle(&bg, 0, 0, w, h);

    RectF r(0.0f, 0.0f, (REAL)w, (REAL)h);
    r.Inflate(-1.0f, -1.0f);
    float rad = std::clamp(r.Height * 0.40f, 6.0f, 14.0f);

    GraphicsPath p;
    AddRoundRectPath(p, r, rad);

    SolidBrush fill(Gp(UiTheme::Color_ControlBg()));
    g.FillPath(&fill, &p);

    Pen border(Gp(UiTheme::Color_Border()), 1.0f);
    g.DrawPath(&border, &p);

    const wchar_t* txt = (st && st->text[0]) ? st->text : L"";
    FontFamily ff(L"Segoe UI");
    StringFormat fmt;
    fmt.SetAlignment(StringAlignmentCenter);
    fmt.SetLineAlignment(StringAlignmentCenter);
    fmt.SetFormatFlags(StringFormatFlagsNoWrap);

    float em = std::clamp(r.Height * 0.52f, 11.0f, 16.0f);
    Font font(&ff, em, FontStyleBold, UnitPixel);

    SolidBrush tbr(Gp(UiTheme::Color_Text()));
    g.DrawString(txt, -1, &font, r, &fmt, &tbr);
}

static LRESULT CALLBACK PremiumChipProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    PremiumChipState* st = (PremiumChipState*)GetWindowLongPtrW(hWnd, GWLP_USERDATA);

    switch (msg)
    {
    case WM_NCCREATE: return TRUE;

    case WM_CREATE:
        st = new PremiumChipState();
        SetWindowLongPtrW(hWnd, GWLP_USERDATA, (LONG_PTR)st);
        return 0;

    case WM_NCDESTROY:
        if (st) { delete st; SetWindowLongPtrW(hWnd, GWLP_USERDATA, 0); }
        return 0;

    case WM_ERASEBKGND:
        return 1;

    case WM_SETTEXT:
        if (st)
        {
            const wchar_t* s = (const wchar_t*)lParam;
            if (!s) s = L"";
            wcsncpy_s(st->text, s, _TRUNCATE);
            InvalidateRect(hWnd, nullptr, FALSE);
        }
        return TRUE;

    case WM_PAINT:
    {
        PAINTSTRUCT ps{};
        HDC hdc = BeginPaint(hWnd, &ps);
        PremiumChip_Paint(hWnd, hdc);
        EndPaint(hWnd, &ps);
        return 0;
    }
    }

    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

static HWND PremiumChip_Create(HWND parent, HINSTANCE hInst, int x, int y, int w, int h, int id)
{
    static bool reg = false;
    if (!reg)
    {
        WNDCLASSW wc{};
        wc.lpfnWndProc = PremiumChipProc;
        wc.hInstance = hInst;
        wc.lpszClassName = L"PremiumValueChip";
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.hbrBackground = nullptr;
        RegisterClassW(&wc);
        reg = true;
    }

    return CreateWindowW(L"PremiumValueChip", L"",
        WS_CHILD | WS_VISIBLE,
        x, y, w, h,
        parent, (HMENU)(INT_PTR)id, hInst, nullptr);
}

// ============================================================================
// Global settings page
// ============================================================================
struct GlobalSettingsPageState
{
    HWND lblGlobalProfile = nullptr;
    HWND cmbGlobalProfile = nullptr;

    HWND lblLayout = nullptr;
    HWND cmbLayout = nullptr;
    LayoutPicker layoutPicker;
    HWND btnLayoutEditor = nullptr;

    HWND lblPoll = nullptr;
    HWND sldPoll = nullptr;
    HWND chipPoll = nullptr;

    HWND lblUiRefresh = nullptr;
    HWND sldUiRefresh = nullptr;
    HWND chipUiRefresh = nullptr;

    HWND lblFactoryReset = nullptr;
    HWND btnFactoryReset = nullptr;
    HWND lblFactoryResetHint = nullptr;

    CustomPageSurface surface;
    CustomPageScrollController scroll;
    int scrollY = 0;
    int contentHeight = 0;
    RECT rcGlobalProfile{};
    RECT rcLayout{};
    RECT rcLayoutBrand{}, rcLayoutTitle{}, rcLayoutVariant{};
    RECT rcLayoutEditor{};
    RECT rcPollSlider{};
    RECT rcPollChip{};
    RECT rcUiRefreshSlider{};
    RECT rcUiRefreshChip{};
    RECT rcEngineRuntime{};
    RECT rcDiagnosticLogging{}, rcHallJoyFolder{}, rcLoggingError{};
    RECT rcCommunity{}, rcDiscord{};
    DWORD loggingError = ERROR_SUCCESS;
    bool pausePulseTimer = false;
    ULONGLONG pausePulseEpoch = 0;
    RECT rcFactoryReset{};
    int hotId = 0;
    int pressedId = 0;
    int dragId = 0;

    int   pendingDeleteIdx = -1;
    DWORD pendingDeleteTick = 0;
    bool  pendingDeleteIsGlobalProfile = false;
    HWND hToast = nullptr;
    std::wstring toastText;
    ULONGLONG toastHideAt = 0;
};

static constexpr int GLOB_ID_POLL_SLIDER = 7601;
static constexpr int GLOB_ID_UIREFRESH_SLIDER = 7602;
static constexpr int GLOB_ID_LAYOUT_COMBO = 7603;
static constexpr int GLOB_ID_LAYOUT_EDITOR = 7604;
static constexpr int GLOB_ID_LAYOUT_BRAND = 7690;
static constexpr int GLOB_ID_LAYOUT_VARIANT = 7691;
static constexpr int GLOB_ID_GLOBAL_PROFILE_COMBO = 7606;
static constexpr int GLOB_ID_GLOBAL_PROFILE_SAVE = 7610;
static constexpr int GLOB_ID_ENGINE_RUNTIME = 7611;
static constexpr int GLOB_ID_DIAGNOSTIC_LOGGING = 7612;
static constexpr int GLOB_ID_HALLJOY_FOLDER = 7613;
static constexpr int GLOB_ID_DISCORD = 7614;
static constexpr int GLOB_ID_FACTORY_RESET = 7607;

static const wchar_t* Global_EngineRuntimeButtonText(halljoy::runtime_command::State state)
{
    using halljoy::runtime_command::State;
    switch (state)
    {
    case State::Active: return L"Pause HallJoy";
    case State::Paused: return L"Resume HallJoy";
    case State::PauseFaulted: return L"Restart required";
    case State::PauseRequested:
    case State::Neutralizing:
    case State::StoppingProviders:
    case State::ReleasingLeases: return L"Pausing…";
    case State::ResumeRequested:
    case State::Enumerating:
    case State::ProvingCapabilities:
    case State::PublishingNeutralGeneration: return L"Resuming…";
    }
    return L"Engine state unavailable";
}

static bool Global_EngineRuntimeButtonEnabled()
{
    using halljoy::runtime_command::State;
    const auto state = halljoy::engine_runtime::EngineRuntimeOwner_Snapshot().state;
    return state == State::Active || state == State::Paused;
}

static void Global_DrawActionButton(const DRAWITEMSTRUCT* dis, bool danger = false)
{
    if (!dis) return;

    const int width = dis->rcItem.right - dis->rcItem.left;
    const int height = dis->rcItem.bottom - dis->rcItem.top;
    if (width <= 1 || height <= 1) return;

    auto draw = [&](HDC hdc, const RECT& rc)
    {
        const bool disabled = (dis->itemState & ODS_DISABLED) != 0;
        const bool pressed = (dis->itemState & ODS_SELECTED) != 0;
        const bool hot = (dis->itemState & ODS_HOTLIGHT) != 0;

        COLORREF bg = danger ? RGB(108, 35, 43) : UiTheme::Color_ControlBg();
        if (disabled)
            bg = danger ? RGB(69, 40, 44) : RGB(35, 35, 37);
        else if (pressed)
            bg = danger ? RGB(82, 28, 34) : RGB(42, 42, 44);
        else if (hot)
            bg = danger ? RGB(128, 41, 50) : RGB(40, 40, 42);

        FillRect(hdc, &rc, UiTheme::Brush_PanelBg());

        Graphics g(hdc);
        g.SetSmoothingMode(SmoothingModeAntiAlias);
        g.SetPixelOffsetMode(PixelOffsetModeHighQuality);
        RectF bounds(0.75f, 0.75f, (REAL)(rc.right - rc.left) - 1.5f,
            (REAL)(rc.bottom - rc.top) - 1.5f);
        const float radius = (float)std::clamp(S(dis->hwndItem, 7), 4, 12);
        GraphicsPath path;
        AddRoundRectPath(path, bounds, radius);
        SolidBrush fill(Gp(bg));
        g.FillPath(&fill, &path);

        const COLORREF border = danger ? RGB(222, 78, 91) : UiTheme::Color_Border();
        Pen outline(Gp(disabled ? UiTheme::Color_Border() : border), 1.25f);
        outline.SetLineJoin(LineJoinRound);
        g.DrawPath(&outline, &path);

        if ((dis->itemState & ODS_FOCUS) != 0 && !disabled)
        {
            RectF focus = bounds;
            focus.Inflate(-3.0f, -3.0f);
            GraphicsPath focusPath;
            AddRoundRectPath(focusPath, focus, std::max(3.0f, radius - 2.0f));
            Pen focusPen(Gp(danger ? RGB(255, 171, 180) : UiTheme::Color_Accent()), 2.0f);
            focusPen.SetLineJoin(LineJoinRound);
            g.DrawPath(&focusPen, &focusPath);
        }

        wchar_t text[128]{};
        GetWindowTextW(dis->hwndItem, text, (int)_countof(text));
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, disabled ? UiTheme::Color_TextMuted() :
            (danger ? RGB(255, 238, 240) : UiTheme::Color_Text()));
        DrawTextW(hdc, text, -1, const_cast<RECT*>(&rc),
            DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    };

    HDC memDC = CreateCompatibleDC(dis->hDC);
    HBITMAP bitmap = memDC ? CreateCompatibleBitmap(dis->hDC, width, height) : nullptr;
    if (!memDC || !bitmap)
    {
        if (bitmap) DeleteObject(bitmap);
        if (memDC) DeleteDC(memDC);
        draw(dis->hDC, dis->rcItem);
        return;
    }

    HGDIOBJ oldBitmap = SelectObject(memDC, bitmap);
    RECT local{ 0, 0, width, height };
    draw(memDC, local);
    BitBlt(dis->hDC, dis->rcItem.left, dis->rcItem.top, width, height,
        memDC, 0, 0, SRCCOPY);
    SelectObject(memDC, oldBitmap);
    DeleteObject(bitmap);
    DeleteDC(memDC);
}

static void Global_RequestSave(HWND hWnd)
{
    HWND root = ResolveAppMainWindow(hWnd);
    if (root) PostMessageW(root, WM_APP_REQUEST_SAVE, 0, 0);
}

static void Global_RequestApplyTiming(HWND hWnd)
{
    HWND root = ResolveAppMainWindow(hWnd);
    if (root) PostMessageW(root, WM_APP_APPLY_TIMING, 0, 0);
}

static void Global_NotifyMainPage(HWND hWnd)
{
    HWND tab = GetParent(hWnd);
    HWND page = tab ? GetParent(tab) : nullptr;
    if (page) PostMessageW(page, WM_APP_KEYBOARD_LAYOUT_CHANGED, 0, 0);
}

static void GlobalToast_EnsureWindow(HWND hPage, GlobalSettingsPageState* st)
{
    if (!st || st->hToast) return;

    static bool reg = false;
    if (!reg)
    {
        WNDCLASSW wc{};
        wc.lpfnWndProc = [](HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) -> LRESULT
            {
                auto* stLocal = (GlobalSettingsPageState*)GetWindowLongPtrW(hWnd, GWLP_USERDATA);
                switch (msg)
                {
                case WM_NCCREATE: return TRUE;
                case WM_CREATE:
                {
                    auto* cs = (CREATESTRUCTW*)lParam;
                    stLocal = (GlobalSettingsPageState*)cs->lpCreateParams;
                    SetWindowLongPtrW(hWnd, GWLP_USERDATA, (LONG_PTR)stLocal);
                    SetLayeredWindowAttributes(hWnd, 0, 235, LWA_ALPHA);
                    return 0;
                }
                case WM_ERASEBKGND: return 1;
                case WM_PAINT:
                {
                    PAINTSTRUCT ps{};
                    HDC hdc = BeginPaint(hWnd, &ps);
                    RECT rc{};
                    GetClientRect(hWnd, &rc);
                    int w = rc.right - rc.left;
                    int h = rc.bottom - rc.top;

                    Graphics g(hdc);
                    g.SetSmoothingMode(SmoothingModeAntiAlias);
                    g.SetPixelOffsetMode(PixelOffsetModeHighQuality);
                    g.SetCompositingQuality(CompositingQualityHighQuality);
                    g.SetTextRenderingHint(TextRenderingHintAntiAliasGridFit);

                    RectF r(0.0f, 0.0f, (REAL)w, (REAL)h);
                    r.Inflate(-1.0f, -1.0f);

                    float rad = std::clamp(r.Height * 0.35f, 8.0f, 14.0f);
                    GraphicsPath p;
                    AddRoundRectPath(p, r, rad);

                    SolidBrush brFill(Color(245, 34, 34, 34));
                    g.FillPath(&brFill, &p);

                    Pen pen(Color(255, 255, 90, 90), 2.0f);
                    pen.SetLineJoin(LineJoinRound);
                    g.DrawPath(&pen, &p);

                    std::wstring text = (stLocal ? stLocal->toastText : L"");
                    if (!text.empty())
                    {
                        FontFamily ff(L"Segoe UI");
                        float em = std::clamp(r.Height * 0.36f, 11.0f, 14.0f);
                        Font font(&ff, em, FontStyleRegular, UnitPixel);
                        StringFormat fmt;
                        fmt.SetAlignment(StringAlignmentNear);
                        fmt.SetLineAlignment(StringAlignmentCenter);
                        fmt.SetTrimming(StringTrimmingEllipsisCharacter);
                        fmt.SetFormatFlags(StringFormatFlagsNoWrap);
                        RectF tr = r;
                        tr.Inflate(-10.0f, 0.0f);
                        SolidBrush txtBr(Gp(UiTheme::Color_Text(), 255));
                        g.DrawString(text.c_str(), -1, &font, tr, &fmt, &txtBr);
                    }

                    EndPaint(hWnd, &ps);
                    return 0;
                }
                case WM_NCDESTROY:
                    SetWindowLongPtrW(hWnd, GWLP_USERDATA, 0);
                    return 0;
                }
                return DefWindowProcW(hWnd, msg, wParam, lParam);
            };

        wc.hInstance = (HINSTANCE)GetWindowLongPtrW(hPage, GWLP_HINSTANCE);
        wc.lpszClassName = L"DD_LayoutDeleteToast";
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.hbrBackground = nullptr;
        RegisterClassW(&wc);
        reg = true;
    }

    HWND ownerTop = GetAncestor(hPage, GA_ROOT);
    st->hToast = CreateWindowExW(
        WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_NOACTIVATE | WS_EX_LAYERED,
        L"DD_LayoutDeleteToast",
        L"",
        WS_POPUP,
        0, 0, 10, 10,
        ownerTop, nullptr, (HINSTANCE)GetWindowLongPtrW(hPage, GWLP_HINSTANCE),
        st);
    if (st->hToast)
        ShowWindow(st->hToast, SW_HIDE);
}

static void GlobalToast_Hide(HWND hPage, GlobalSettingsPageState* st)
{
    if (!st) return;
    st->toastHideAt = 0;
    if (hPage) KillTimer(hPage, TOAST_TIMER_ID);
    if (st->hToast) ShowWindow(st->hToast, SW_HIDE);
}

static void GlobalToast_ShowNearCursor(HWND hPage, GlobalSettingsPageState* st, const wchar_t* text)
{
    if (!st || !hPage) return;
    GlobalToast_EnsureWindow(hPage, st);
    if (!st->hToast) return;

    st->toastText = (text ? text : L"");

    HFONT font = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
    HDC hdc = GetDC(hPage);
    HGDIOBJ oldF = SelectObject(hdc, font);
    RECT calc{ 0,0,0,0 };
    DrawTextW(hdc, st->toastText.c_str(), (int)st->toastText.size(), &calc,
        DT_CALCRECT | DT_SINGLELINE | DT_NOPREFIX);
    SelectObject(hdc, oldF);
    ReleaseDC(hPage, hdc);

    int padX = S(hPage, 16);
    int padY = S(hPage, 10);
    int textW = (int)(calc.right - calc.left);
    int textH = (int)(calc.bottom - calc.top);
    int w = std::clamp(textW + padX * 2, S(hPage, 220), S(hPage, 520));
    int h = std::max(S(hPage, 34), textH + padY * 2);

    POINT pt{};
    GetCursorPos(&pt);
    int x = pt.x + S(hPage, 14);
    int y = pt.y + S(hPage, 18);

    HMONITOR mon = MonitorFromPoint(pt, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi{};
    mi.cbSize = sizeof(mi);
    if (GetMonitorInfoW(mon, &mi))
    {
        RECT wa = mi.rcWork;
        if (x + w > wa.right) x = wa.right - w;
        if (y + h > wa.bottom) y = wa.bottom - h;
        if (x < wa.left) x = wa.left;
        if (y < wa.top) y = wa.top;
    }

    SetWindowPos(st->hToast, HWND_TOPMOST, x, y, w, h, SWP_NOACTIVATE | SWP_SHOWWINDOW);
    InvalidateRect(st->hToast, nullptr, TRUE);
    st->toastHideAt = GetTickCount64() + TOAST_SHOW_MS;
    SetTimer(hPage, TOAST_TIMER_ID, 30, nullptr);
}

static void GlobalDeleteConfirm_Clear(HWND hPage, GlobalSettingsPageState* st)
{
    if (!st) return;
    st->pendingDeleteIdx = -1;
    st->pendingDeleteTick = 0;
    st->pendingDeleteIsGlobalProfile = false;
    GlobalToast_Hide(hPage, st);
}

static void Global_Layout(HWND hWnd, GlobalSettingsPageState* st);

static void Global_RefreshLayoutCombo(GlobalSettingsPageState* st)
{
    if (!st || !st->cmbLayout) return;

    const bool hadVariants = st->layoutPicker.HasVariants();
    st->layoutPicker.Refresh(KeyboardLayout_GetCurrentPresetIndex());
    if (hadVariants != st->layoutPicker.HasVariants()) Global_Layout(GetParent(st->cmbLayout), st);
}

static void Global_UpdateUi(GlobalSettingsPageState* st);

static void Global_UpdateProfileSaveIcon(GlobalSettingsPageState* st)
{
    if (!st || !st->cmbGlobalProfile) return;
    PremiumCombo::SetExtraIcon(
        st->cmbGlobalProfile,
        GlobalProfiles_IsDirty()
            ? PremiumCombo::ExtraIconKind::Save
            : PremiumCombo::ExtraIconKind::None);
}

static void Global_RefreshGlobalProfileCombo(GlobalSettingsPageState* st)
{
    if (!st || !st->cmbGlobalProfile) return;

    PremiumCombo::Clear(st->cmbGlobalProfile);

    std::vector<std::wstring> names;
    GlobalProfiles_List(names);
    for (size_t i = 0; i < names.size(); ++i)
    {
        int idx = PremiumCombo::AddString(st->cmbGlobalProfile, names[i].c_str());
        if (i > 0) // non-default profiles can be deleted
            PremiumCombo::SetItemButtonKind(st->cmbGlobalProfile, idx, PremiumCombo::ItemButtonKind::Delete);
    }

    int createIdx = PremiumCombo::AddString(st->cmbGlobalProfile, L"+ Create New Profile...");
    (void)createIdx;
    PremiumCombo::SetDropMaxVisible(st->cmbGlobalProfile, 10);

    const std::wstring& active = GlobalProfiles_GetActiveName();
    int activeIdx = 0;
    for (size_t i = 0; i < names.size(); ++i)
    {
        if (FileNamePolicy_Equivalent(names[i], active))
        {
            activeIdx = (int)i;
            break;
        }
    }
    PremiumCombo::SetCurSel(st->cmbGlobalProfile, activeIdx, false);
    Global_UpdateProfileSaveIcon(st);
}

static bool Global_ApplyActiveGlobalProfile(GlobalSettingsPageState* st, HWND hWnd, const std::wstring& name)
{
    if (!st) return false;
    if (!GlobalProfiles_Switch(name)) {
        MessageBoxW(hWnd, L"Could not switch profile. Check that its settings and bindings are complete and readable, and that your profiles can be saved.",
            L"Profiles", MB_ICONWARNING);
        Global_UpdateUi(st);
        return false;
    }

    // Apply runtime timing/backend state from loaded profile.
    RealtimeLoop_SetIntervalMs(Settings_GetPollingMs());
    Backend_SetVirtualGamepadCount(Settings_GetVirtualGamepadCount());
    Backend_SetVirtualGamepadsEnabled(Settings_GetVirtualGamepadsEnabled());

    if (st->sldPoll)
        SendMessageW(st->sldPoll, TBM_SETPOS, TRUE, (LPARAM)std::clamp(Settings_GetPollingMs(), 1u, 20u));
    if (st->sldUiRefresh)
        SendMessageW(st->sldUiRefresh, TBM_SETPOS, TRUE, (LPARAM)std::clamp(Settings_GetUIRefreshMs(), 1u, 200u));

    GlobalProfiles_SetDirty(false);
    Global_UpdateProfileSaveIcon(st);

    if (g_hPageConfig && IsWindow(g_hPageConfig))
        PostMessageW(g_hPageConfig, WM_APP_CONFIG_PROFILE_APPLIED, 0, 0);

    Global_NotifyMainPage(hWnd);
    Global_RequestApplyTiming(hWnd);
    Global_RequestSave(hWnd);
    Global_UpdateUi(st);
    return true;
}

static void Global_UpdateUi(GlobalSettingsPageState* st)
{
    if (!st) return;

    if (st->cmbGlobalProfile)
    {
        int count = PremiumCombo::GetCount(st->cmbGlobalProfile);
        int sel = PremiumCombo::GetCurSel(st->cmbGlobalProfile);
        bool selIsCreateRow = (count > 0 && sel == count - 1);
        if (!selIsCreateRow)
        {
            const std::wstring& active = GlobalProfiles_GetActiveName();
            int activeIdx = -1;
            for (int i = 0; i < count - 1; ++i)
            {
                wchar_t item[260]{};
                PremiumCombo::GetLBText(st->cmbGlobalProfile, i, item, (int)_countof(item));
                if (FileNamePolicy_Equivalent(item, active))
                {
                    activeIdx = i;
                    break;
                }
            }
            if (activeIdx >= 0 && sel != activeIdx)
                PremiumCombo::SetCurSel(st->cmbGlobalProfile, activeIdx, false);
        }
    }

    if (st->cmbLayout)
    {
        Global_RefreshLayoutCombo(st);
    }

    if (st->chipPoll)
    {
        wchar_t b[32]{};
        swprintf_s(b, L"%u ms", (unsigned)Settings_GetPollingMs());
        SetWindowTextW(st->chipPoll, b);
    }

    if (st->chipUiRefresh)
    {
        wchar_t b[32]{};
        swprintf_s(b, L"%u ms", (unsigned)Settings_GetUIRefreshMs());
        SetWindowTextW(st->chipUiRefresh, b);
    }

    Global_UpdateProfileSaveIcon(st);

}

static bool Layout_NudgeSelectedKey(HWND hWnd, LayoutPageState* st, int dRow, int dX, int dW)
{
    if (!st || st->selectedIdx < 0) return false;
    if (st->selectedIdx >= (int)st->draftKeys.size()) return false;

    KeyDef& k = st->draftKeys[st->selectedIdx];
    if (dX && st->uniformSpacingEnabled)
    {
        Layout_BakeUniformSpacingIntoDraft(st); st->uniformSpacingEnabled = false;
        Layout_UpdateUniformSpacingButton(st);
    }
    int nextRow = std::clamp(k.row + dRow, 0, 20);
    int nextY = dRow ? (st->preciseY ? std::clamp(KeyboardLayout_KeyY(k) + dRow, 0, 4000) : halljoy::layout_editor::RowY(k, nextRow)) : KeyboardLayout_KeyY(k);
    int nextX = std::clamp(k.x + dX, 0, 4000);
    int nextW = std::clamp(k.w + dW, KEYBOARD_KEY_MIN_DIM, KEYBOARD_KEY_MAX_DIM);
    if (nextY == KeyboardLayout_KeyY(k) && nextX == k.x && nextW == k.w)
        return false;
    if (dRow) { k.y = st->preciseY || k.y >= 0 ? nextY : -1; if (!st->preciseY) k.row = nextRow; }
    k.x = nextX;
    k.w = nextW;

    Layout_RefreshSelection(hWnd, st);
    Layout_SetUnsaved(st, true);
    InvalidateRect(hWnd, nullptr, FALSE);
    return true;
}

static void Global_Layout(HWND hWnd, GlobalSettingsPageState* st);

static int Global_GetMaxScroll(HWND hWnd, GlobalSettingsPageState* st)
{
    if (!st) return 0;
    CustomPageSurface_SetState(&st->surface, st->scrollY, st->contentHeight);
    return CustomPageSurface_GetMaxScroll(hWnd, &st->surface);
}

static void Global_SetScrollY(HWND hWnd, GlobalSettingsPageState* st, int scrollY)
{
    if (!st) return;
    CustomPageSurface_SetState(&st->surface, st->scrollY, st->contentHeight);
    CustomPageSurface_SetScrollY(hWnd, &st->surface, scrollY);
    if (st->scrollY == st->surface.scrollY)
        return;

    st->scrollY = st->surface.scrollY;
}

static void Global_Layout(HWND hWnd, GlobalSettingsPageState* st)
{
    if (!st) return;

    RECT rc{};
    GetClientRect(hWnd, &rc);

    int margin = S(hWnd, 16);
    int scrollbarReserve = S(hWnd, 8) + S(hWnd, 7) * 2;
    int x = margin;
    int y = margin;

    int chipW = S(hWnd, 86);
    int gap = S(hWnd, 10);
    int comboVisibleH = S(hWnd, 26);
    int sliderH = S(hWnd, 34);
    int chipH = sliderH;
    int labelH = S(hWnd, 18);
    int rowGap = S(hWnd, 18);

    int sliderW = (rc.right - rc.left) - margin * 2 - scrollbarReserve - chipW - gap;
    sliderW = std::max(S(hWnd, 180), sliderW);
    // All geometry is retained in content coordinates. The compatibility
    // PremiumCombo HWNDs stay hidden until their popup is explicitly opened.
    // Keep the community entry first and compact at every window width.
    const int communityRight = std::min(x + sliderW + gap + chipW, x + S(hWnd, 480));
    const bool compactCommunity = communityRight - x < S(hWnd, 440);
    st->rcCommunity = RECT{ x, y, communityRight, y + S(hWnd, compactCommunity ? 112 : 80) };
    const int discordX = compactCommunity ? x + S(hWnd, 14) : communityRight - S(hWnd, 146);
    const int discordY = y + S(hWnd, compactCommunity ? 68 : 24);
    st->rcDiscord = RECT{ discordX, discordY, discordX + S(hWnd, 132), discordY + S(hWnd, 32) };
    y = st->rcCommunity.bottom + S(hWnd, 24);
    y += labelH + S(hWnd, 6);
    st->rcGlobalProfile = RECT{ x, y, x + sliderW + gap + chipW, y + comboVisibleH };
    y += comboVisibleH + rowGap;
    y += labelH + S(hWnd, 6);
    st->rcLayoutTitle = RECT{ x, y, x + sliderW + gap + chipW, y + labelH };
    y += labelH + S(hWnd, 10);
    const int brandW = std::min(S(hWnd, 160), (sliderW + chipW) / 2);
    st->rcLayoutBrand = RECT{ x, y, x + brandW, y + comboVisibleH };
    const int variantW = st->layoutPicker.HasVariants() ? S(hWnd, 96) + gap : 0;
    st->rcLayout = RECT{ x + brandW + gap, y, x + sliderW + gap + chipW - variantW, y + comboVisibleH };
    st->rcLayoutVariant = variantW ? RECT{ st->rcLayout.right + gap, y, x + sliderW + gap + chipW, y + comboVisibleH } : RECT{};
    y += comboVisibleH + rowGap;
    st->rcLayoutEditor = RECT{ x, y, x + S(hWnd, 210), y + S(hWnd, 28) };
    y += S(hWnd, 28) + S(hWnd, 14);
    st->rcHallJoyFolder = RECT{ x, y, x + S(hWnd, 210), y + S(hWnd, 28) };
    y += S(hWnd, 28) + S(hWnd, 14);
    y += labelH + S(hWnd, 6);
    st->rcPollSlider = RECT{ x, y, x + sliderW, y + sliderH };
    st->rcPollChip = RECT{ x + sliderW + gap, y, x + sliderW + gap + chipW, y + chipH };
    y += sliderH + rowGap;
    y += labelH + S(hWnd, 6);
    st->rcUiRefreshSlider = RECT{ x, y, x + sliderW, y + sliderH };
    st->rcUiRefreshChip = RECT{ x + sliderW + gap, y, x + sliderW + gap + chipW, y + chipH };
    y += sliderH + S(hWnd, 18);
    st->rcEngineRuntime = RECT{ x, y, x + S(hWnd, 210), y + S(hWnd, 32) };
    y += S(hWnd, 32) + S(hWnd, 58);
    st->rcDiagnosticLogging = RECT{ x, y, x + S(hWnd, 280), y + S(hWnd, 28) };
    y += S(hWnd, 34);
    st->loggingError = SupportLog_LastError();
    st->rcLoggingError = RECT{ x, y, st->rcUiRefreshChip.right, y + S(hWnd, 44) };
    if (st->loggingError != ERROR_SUCCESS) y += S(hWnd, 52);
    y += S(hWnd, 10);
    y += labelH + S(hWnd, 7);
    st->rcFactoryReset = RECT{ x, y, x + S(hWnd, 210), y + S(hWnd, 30) };
    y += S(hWnd, 30) + S(hWnd, 7);
    y += S(hWnd, 34) + margin;

    const int previousScroll = st->scrollY;
    st->contentHeight = y;
    CustomPageSurface_SetState(&st->surface, st->scrollY, st->contentHeight);
    CustomPageSurface_SetContentHeight(hWnd, &st->surface, st->contentHeight);
    st->scrollY = st->surface.scrollY;
    if (st->scrollY != previousScroll)
    {
        Global_Layout(hWnd, st);
        return;
    }
    CustomPageSurface_MarkDirty(hWnd, &st->surface);
}

static constexpr UINT_PTR GLOBAL_PAUSE_PULSE_TIMER = 0x7611;

static RECT Global_PulseRect(HWND hWnd, GlobalSettingsPageState* st)
{
    RECT rc = st->rcEngineRuntime;
    OffsetRect(&rc, 0, -st->scrollY);
    InflateRect(&rc, S(hWnd, 7), S(hWnd, 7));
    return rc;
}

static void Global_UpdatePulse(HWND hWnd, GlobalSettingsPageState* st)
{
    if (!st) return;
    RECT client{}, intersection{};
    GetClientRect(hWnd, &client);
    RECT glow = Global_PulseRect(hWnd, st);
    const bool animate = IsWindowVisible(hWnd) && !IsIconic(GetAncestor(hWnd, GA_ROOT)) &&
        IntersectRect(&intersection, &client, &glow) &&
        halljoy::engine_runtime::EngineRuntimeOwner_Snapshot().state == halljoy::runtime_command::State::Paused;
    if (animate && !st->pausePulseTimer)
    {
        st->pausePulseEpoch = GetTickCount64();
        st->pausePulseTimer = SetTimer(hWnd, GLOBAL_PAUSE_PULSE_TIMER, 33, nullptr) != 0;
    }
    else if (!animate && st->pausePulseTimer)
    {
        KillTimer(hWnd, GLOBAL_PAUSE_PULSE_TIMER);
        st->pausePulseTimer = false;
    }
}

static void Global_DrawPulse(HWND hWnd, HDC hdc, GlobalSettingsPageState* st)
{
    if (!st->pausePulseTimer) return;
    // Overlay only: restore the cached page first, never rebuild it per frame.
    Gdiplus::Graphics g(hdc);
    g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    RECT button = st->rcEngineRuntime;
    OffsetRect(&button, 0, -st->scrollY);
    g.ExcludeClip(Gdiplus::Rect(button.left, button.top,
        button.right - button.left, button.bottom - button.top));
    const double phase = (GetTickCount64() - st->pausePulseEpoch) % 2400 / 2400.0;
    const double strength = 0.5 - 0.5 * std::cos(phase * 6.283185307179586);
    for (int spread = 6; spread >= 1; --spread)
    {
        RECT halo = button;
        InflateRect(&halo, S(hWnd, spread), S(hWnd, spread));
        CustomPage_DrawRoundRect(g, halo, RGB(222, 167, 82), RGB(222, 167, 82),
            (float)S(hWnd, 4 + spread), (BYTE)(2 + strength * (spread > 3 ? 9 : 17)));
    }
}

static void Global_RenderContent(HWND hWnd, HDC hdc, const RECT&, void* user)
{
    auto* st = (GlobalSettingsPageState*)user;
    if (!st) return;
    Graphics g(hdc);
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    g.SetPixelOffsetMode(PixelOffsetModeHighQuality);

    const int labelH = S(hWnd, 18);
    auto labelAbove = [&](const RECT& control, const wchar_t* text)
    {
        RECT r{ control.left, control.top - S(hWnd, 24), control.right, control.top - S(hWnd, 6) };
        CustomPage_DrawText(hdc, text, r, UiTheme::Color_Text(),
            DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    };
    auto button = [&](int id, const RECT& r, const wchar_t* text)
    {
        CustomPage_DrawButton(g, hdc, r, text, st->hotId == id,
            st->pressedId == id, true);
    };

    labelAbove(st->rcGlobalProfile,
        GlobalProfiles_IsDirty() ? L"Global profile - unsaved" : L"Global profile");
    PremiumCombo::PaintRetainedFace(st->cmbGlobalProfile, hdc, st->rcGlobalProfile,
        st->hotId == GLOB_ID_GLOBAL_PROFILE_COMBO || st->hotId == GLOB_ID_GLOBAL_PROFILE_SAVE);
    labelAbove(st->rcLayoutTitle, L"Layout");
    labelAbove(st->rcLayoutBrand, L"Brand");
    labelAbove(st->rcLayout, L"Model");
    PremiumCombo::PaintRetainedFace(st->layoutPicker.brand, hdc, st->rcLayoutBrand,
        st->hotId == GLOB_ID_LAYOUT_BRAND);
    PremiumCombo::PaintRetainedFace(st->cmbLayout, hdc, st->rcLayout,
        st->hotId == GLOB_ID_LAYOUT_COMBO);
    if (st->layoutPicker.HasVariants()) {
        labelAbove(st->rcLayoutVariant, L"Variant");
        PremiumCombo::PaintRetainedFace(st->layoutPicker.variant, hdc, st->rcLayoutVariant,
            st->hotId == GLOB_ID_LAYOUT_VARIANT);
    }
    button(GLOB_ID_LAYOUT_EDITOR, st->rcLayoutEditor, L"Layout editor");

    labelAbove(st->rcPollSlider, L"Polling rate");
    CustomPage_DrawSlider(g, hWnd, st->rcPollSlider, 1, 20,
        (int)std::clamp(Settings_GetPollingMs(), 1u, 20u));
    wchar_t value[32]{};
    swprintf_s(value, L"%u ms", (unsigned)Settings_GetPollingMs());
    CustomPage_DrawChip(g, hdc, st->rcPollChip, value);

    labelAbove(st->rcUiRefreshSlider, L"UI refresh interval");
    CustomPage_DrawSlider(g, hWnd, st->rcUiRefreshSlider, 1, 200,
        (int)std::clamp(Settings_GetUIRefreshMs(), 1u, 200u));
    swprintf_s(value, L"%u ms", (unsigned)Settings_GetUIRefreshMs());
    CustomPage_DrawChip(g, hdc, st->rcUiRefreshChip, value);

    // One snapshot for text, colour and enablement. The complete image lives
    // in the retained page cache; the glow has no animation or timer.
    const auto engineState = halljoy::engine_runtime::EngineRuntimeOwner_Snapshot().state;
    using halljoy::runtime_command::State;
    const bool paused = engineState == State::Paused;
    const bool faulted = engineState == State::PauseFaulted;
    const bool enabled = paused || engineState == State::Active;
    const bool hot = enabled && st->hotId == GLOB_ID_ENGINE_RUNTIME;
    const bool pressed = enabled && st->pressedId == GLOB_ID_ENGINE_RUNTIME;
    if (paused || faulted)
    {
        const COLORREF accent = faulted ? RGB(222, 104, 114) : RGB(222, 167, 82);
        if (paused)
            for (int spread = 6; spread >= 1; --spread)
            {
                RECT halo = st->rcEngineRuntime;
                InflateRect(&halo, S(hWnd, spread), S(hWnd, spread));
                CustomPage_DrawRoundRect(g, halo, accent, accent,
                    (float)S(hWnd, 4 + spread), (BYTE)(spread > 3 ? 5 : 9));
            }
        const COLORREF fill = faulted ? RGB(67, 35, 39) :
            (pressed ? RGB(68, 49, 28) : hot ? RGB(88, 65, 35) : RGB(74, 55, 32));
        CustomPage_DrawRoundRect(g, st->rcEngineRuntime, fill, accent, (float)S(hWnd, 4));
        CustomPage_DrawText(hdc, Global_EngineRuntimeButtonText(engineState), st->rcEngineRuntime,
            faulted ? RGB(245, 180, 186) : RGB(250, 219, 166),
            DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        RECT status = st->rcEngineRuntime;
        status.left = status.right + S(hWnd, 14);
        status.right = status.left + S(hWnd, 100);
        CustomPage_DrawText(hdc, paused ? L"Paused" : L"Needs restart", status, accent,
            DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    }
    else
        CustomPage_DrawButton(g, hdc, st->rcEngineRuntime, Global_EngineRuntimeButtonText(engineState),
            hot, pressed, enabled);

    RECT pauseHint{ st->rcEngineRuntime.left, st->rcEngineRuntime.bottom + S(hWnd, 8),
        st->rcUiRefreshChip.right, st->rcEngineRuntime.bottom + S(hWnd, 48) };
    CustomPage_DrawText(hdc, L"Pause releases your keyboard so you can use its web configurator without a device-access conflict.",
        pauseHint, UiTheme::Color_TextMuted(), DT_LEFT | DT_TOP | DT_WORDBREAK | DT_NOPREFIX);

    CustomPage_DrawCheckbox(g, hdc, hWnd, st->rcDiagnosticLogging, L"Enable logging",
        Settings_GetDiagnosticLogging(), true);
    CustomPage_DrawButton(g, hdc, st->rcHallJoyFolder, L"Open HallJoy folder",
        st->hotId == GLOB_ID_HALLJOY_FOLDER, st->pressedId == GLOB_ID_HALLJOY_FOLDER, true);
    if (st->loggingError != ERROR_SUCCESS) {
        wchar_t errorText[128]{};
        swprintf_s(errorText, L"Could not write the support log. Windows error: %lu", st->loggingError);
        CustomPage_DrawText(hdc, errorText, st->rcLoggingError, RGB(222,104,114), DT_LEFT | DT_WORDBREAK);
    }

    // Retained card: no child-window repaint races, timers or network activity.
    CustomPage_DrawRoundRect(g, st->rcCommunity, UiTheme::Color_PanelBg(), RGB(66, 73, 96),
        (float)S(hWnd, 7));
    const bool stackedCommunity = st->rcDiscord.top >= st->rcCommunity.top + S(hWnd, 60);
    const int communityTextRight = stackedCommunity ? st->rcCommunity.right - S(hWnd, 14)
        : st->rcDiscord.left - S(hWnd, 18);
    // Measure with the same selected font and wrapping rules used for painting.
    // Centre the pair as one block, not two independently padded rectangles.
    RECT communityTitle{ st->rcCommunity.left + S(hWnd, 14), 0, communityTextRight, 0 };
    RECT communityBody = communityTitle;
    DrawTextW(hdc, L"HallJoy on Discord", -1, &communityTitle,
        DT_LEFT | DT_SINGLELINE | DT_CALCRECT);
    DrawTextW(hdc, L"Get help, share feedback and follow updates.", -1, &communityBody,
        DT_LEFT | DT_WORDBREAK | DT_CALCRECT);
    const int communityTextTop = st->rcCommunity.top + S(hWnd, 12);
    const int communityTextBottom = stackedCommunity ? st->rcDiscord.top - S(hWnd, 10)
        : st->rcCommunity.bottom - S(hWnd, 12);
    const int communityGap = S(hWnd, 6);
    const int communityTitleHeight = communityTitle.bottom - communityTitle.top;
    const int communityBodyHeight = std::min<int>(communityBody.bottom - communityBody.top,
        std::max(0, communityTextBottom - communityTextTop - communityTitleHeight - communityGap));
    const int communityBlockHeight = communityTitleHeight + communityGap + communityBodyHeight;
    communityTitle.top = communityTextTop + std::max(0,
        (communityTextBottom - communityTextTop - communityBlockHeight) / 2);
    communityTitle.bottom = communityTitle.top + communityTitleHeight;
    communityTitle.right = communityTextRight;
    communityBody = RECT{ communityTitle.left, communityTitle.bottom + communityGap,
        communityTextRight, communityTitle.bottom + communityGap + communityBodyHeight };
    CustomPage_DrawText(hdc, L"HallJoy on Discord", communityTitle, UiTheme::Color_Text(),
        DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    CustomPage_DrawText(hdc, L"Get help, share feedback and follow updates.", communityBody,
        UiTheme::Color_TextMuted(), DT_LEFT | DT_WORDBREAK | DT_END_ELLIPSIS);
    const bool discordPressed = st->pressedId == GLOB_ID_DISCORD;
    const bool discordHot = st->hotId == GLOB_ID_DISCORD;
    CustomPage_DrawRoundRect(g, st->rcDiscord,
        discordPressed ? RGB(60, 67, 132) : discordHot ? RGB(85, 94, 181) : RGB(70, 78, 154),
        RGB(111, 122, 214), (float)S(hWnd, 4));
    CustomPage_DrawText(hdc, L"Join Discord", st->rcDiscord, RGB(245, 246, 255),
        DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    RECT factoryLabel{ st->rcFactoryReset.left, st->rcFactoryReset.top - S(hWnd, 25),
        st->rcFactoryReset.right, st->rcFactoryReset.top - S(hWnd, 7) };
    CustomPage_DrawText(hdc, L"Factory reset", factoryLabel, UiTheme::Color_Text(),
        DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    CustomPage_DrawRoundRect(g, st->rcFactoryReset, RGB(108, 35, 43), RGB(222, 78, 91),
        (float)S(hWnd, 7));
    CustomPage_DrawText(hdc, L"Reset All Settings", st->rcFactoryReset, RGB(255, 238, 240),
        DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    RECT factoryHint{ st->rcFactoryReset.left, st->rcFactoryReset.bottom + S(hWnd, 7),
        st->rcUiRefreshChip.right, st->rcFactoryReset.bottom + S(hWnd, 41) };
    CustomPage_DrawText(hdc,
        L"Backs up and resets settings, bindings, profiles, layouts and curve presets.",
        factoryHint, UiTheme::Color_TextMuted(), DT_LEFT | DT_TOP | DT_WORDBREAK | DT_END_ELLIPSIS);
    (void)labelH;
}

static int Global_HitTest(GlobalSettingsPageState* st, POINT clientPoint)
{
    if (!st) return 0;
    POINT pt = CustomPageSurface_ClientToContent(&st->surface, clientPoint);
    RECT saveIcon{};
    if (PremiumCombo::GetRetainedExtraIconRect(
        st->cmbGlobalProfile, st->rcGlobalProfile, &saveIcon) && PtInRect(&saveIcon, pt))
        return GLOB_ID_GLOBAL_PROFILE_SAVE;
    const std::pair<int, RECT*> hits[] = {
        { GLOB_ID_GLOBAL_PROFILE_COMBO, &st->rcGlobalProfile },
        { GLOB_ID_LAYOUT_COMBO, &st->rcLayout },
        { GLOB_ID_LAYOUT_BRAND, &st->rcLayoutBrand },
        { GLOB_ID_LAYOUT_VARIANT, &st->rcLayoutVariant },
        { GLOB_ID_LAYOUT_EDITOR, &st->rcLayoutEditor },
        { GLOB_ID_POLL_SLIDER, &st->rcPollSlider },
        { GLOB_ID_UIREFRESH_SLIDER, &st->rcUiRefreshSlider },
        { GLOB_ID_ENGINE_RUNTIME, &st->rcEngineRuntime },
        { GLOB_ID_DIAGNOSTIC_LOGGING, &st->rcDiagnosticLogging },
        { GLOB_ID_HALLJOY_FOLDER, &st->rcHallJoyFolder },
        { GLOB_ID_DISCORD, &st->rcDiscord },
        { GLOB_ID_FACTORY_RESET, &st->rcFactoryReset }
    };
    for (const auto& hit : hits)
        if (PtInRect(hit.second, pt)) return hit.first;
    return 0;
}

static void Global_CloseComboAnchors(GlobalSettingsPageState* st)
{
    if (!st) return;
    HWND combos[] = { st->cmbGlobalProfile, st->cmbLayout, st->layoutPicker.brand, st->layoutPicker.variant };
    for (HWND combo : combos)
    {
        if (!combo) continue;
        PremiumCombo::ShowDropDown(combo, false);
        ShowWindow(combo, SW_HIDE);
    }
}

static void Global_OpenComboAnchor(HWND hWnd, GlobalSettingsPageState* st, HWND combo, const RECT& contentRect)
{
    if (!st || !combo) return;
    Global_CloseComboAnchors(st);
    RECT view = CustomPageSurface_ContentToClient(&st->surface, contentRect);
    SetWindowPos(combo, HWND_TOP, view.left, view.top,
        std::max(1L, view.right - view.left), std::max(1L, view.bottom - view.top),
        SWP_NOACTIVATE | SWP_SHOWWINDOW);
    SetFocus(combo);
    PremiumCombo::ShowDropDown(combo, true);
}

static void Global_SetSliderFromClient(HWND hWnd, GlobalSettingsPageState* st, int id, int clientX)
{
    if (!st) return;
    if (id == GLOB_ID_POLL_SLIDER)
    {
        const int value = CustomPage_SliderValueFromPoint(hWnd, st->rcPollSlider, 1, 20, clientX);
        Settings_SetPollingMs((UINT)value);
        RealtimeLoop_SetIntervalMs(Settings_GetPollingMs());
    }
    else if (id == GLOB_ID_UIREFRESH_SLIDER)
    {
        const int value = CustomPage_SliderValueFromPoint(hWnd, st->rcUiRefreshSlider, 1, 200, clientX);
        Settings_SetUIRefreshMs((UINT)value);
    }
    else return;
    GlobalProfiles_SetDirty(true);
    Global_RequestApplyTiming(hWnd);
    Global_RequestSave(hWnd);
    CustomPageSurface_MarkDirty(hWnd, &st->surface);
}

LRESULT CALLBACK KeyboardSubpages_GlobalSettingsPageProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (msg == WM_APP_KEYBOARD_LAYOUT_CHANGED) {
        auto* state = (GlobalSettingsPageState*)GetWindowLongPtrW(hWnd, GWLP_USERDATA);
        if (state) {
            Global_RefreshLayoutCombo(state);
            CustomPageSurface_MarkDirty(hWnd, &state->surface);
        }
        return 0;
    }
    auto* st = (GlobalSettingsPageState*)GetWindowLongPtrW(hWnd, GWLP_USERDATA);

    if (msg == PremiumCombo::MsgDropStateChanged())
    {
        if (st && !wParam)
        {
            ShowWindow((HWND)lParam, SW_HIDE);
            CustomPageSurface_MarkDirty(hWnd, &st->surface);
        }
        return 0;
    }

    if (msg == PremiumCombo::MsgItemTextCommit())
    {
        if (!st)
            return 0;

        HWND hCombo = (HWND)lParam;
        if (!hCombo)
            return 0;

        GlobalDeleteConfirm_Clear(hWnd, st);

        int idx = (int)LOWORD(wParam);
        PremiumCombo::ItemButtonKind kind = (PremiumCombo::ItemButtonKind)(int)HIWORD(wParam);
        if (kind != PremiumCombo::ItemButtonKind::Rename)
            return 0;

        wchar_t nameBuf[260]{};
        PremiumCombo::ConsumeCommittedText(hCombo, nameBuf, 260);

        if (hCombo == st->cmbLayout) return 0; // Selection-only control.

        if (hCombo == st->cmbGlobalProfile)
        {
            int count = PremiumCombo::GetCount(st->cmbGlobalProfile);
            if (count <= 0 || idx != count - 1)
                return 0; // only create row supports inline edit here

            std::wstring newName = GlobalProfiles_SanitizeName(nameBuf);
            if (newName.empty() || GlobalProfiles_IsDefault(newName))
            {
                MessageBoxW(hWnd, L"Profile name cannot be empty.", L"Profiles", MB_ICONWARNING);
                return 0;
            }

            std::vector<std::wstring> names;
            GlobalProfiles_List(names);
            for (const auto& n : names)
            {
                if (FileNamePolicy_Equivalent(n, newName))
                {
                    MessageBoxW(hWnd, L"Profile with this name already exists.", L"Profiles", MB_ICONWARNING);
                    return 0;
                }
            }

            if (GetFileAttributesW(GlobalProfiles_GetBindingsPath(newName).c_str()) != INVALID_FILE_ATTRIBUTES) {
                MessageBoxW(hWnd, L"A bindings file with this profile name already exists. Choose another name.",
                    L"Profiles", MB_ICONWARNING);
                return 0;
            }
            const std::wstring prevName = GlobalProfiles_GetActiveName();
            if (!GlobalProfiles_Save(prevName))
            {
                Global_UpdateUi(st);
                return 0;
            }

            // New profile starts as a full copy of current runtime state.
            if (!GlobalProfiles_Save(newName))
            {
                Global_UpdateUi(st);
                return 0;
            }

            GlobalProfiles_SetActiveName(newName);
            if (!GlobalProfiles_SaveActiveToSettingsIni(AppPaths_SettingsIni().c_str()))
            {
                GlobalProfiles_SetActiveName(prevName);
                GlobalProfiles_Delete(newName);
                Global_UpdateUi(st);
                return 0;
            }
            GlobalProfiles_SetDirty(false);
            Global_RefreshGlobalProfileCombo(st);
            PremiumCombo::ShowDropDown(st->cmbGlobalProfile, false);
            Global_RequestSave(hWnd);
            Global_UpdateUi(st);
            return 0;
        }

        return 0;
    }

    if (msg == PremiumCombo::MsgItemButton())
    {
        if (!st)
            return 0;

        HWND hCombo = (HWND)lParam;
        if (!hCombo)
            return 0;

        int idx = (int)LOWORD(wParam);
        PremiumCombo::ItemButtonKind kind = (PremiumCombo::ItemButtonKind)(int)HIWORD(wParam);
        if (kind != PremiumCombo::ItemButtonKind::Delete)
        {
            GlobalDeleteConfirm_Clear(hWnd, st);
            return 0;
        }

        if (hCombo == st->cmbLayout) return 0; // Selection-only control.

        if (hCombo == st->cmbGlobalProfile)
        {
            int count = PremiumCombo::GetCount(st->cmbGlobalProfile);
            int createRow = count - 1;
            if (idx <= 0 || idx >= createRow)
                return 0; // don't delete default or create row

            wchar_t nameBuf[260]{};
            PremiumCombo::GetLBText(st->cmbGlobalProfile, idx, nameBuf, (int)_countof(nameBuf));
            std::wstring name = nameBuf;
            if (name.empty() || GlobalProfiles_IsDefault(name))
                return 0;

            DWORD now = GetTickCount();
            if (st->pendingDeleteIdx == idx && st->pendingDeleteIsGlobalProfile &&
                (now - st->pendingDeleteTick) <= TOAST_SHOW_MS)
            {
                GlobalDeleteConfirm_Clear(hWnd, st);

                // If deleting active profile, switch to default first.
                if (FileNamePolicy_Equivalent(GlobalProfiles_GetActiveName(), name) &&
                    !Global_ApplyActiveGlobalProfile(st, hWnd, L"Default")) return 0;

                if (GlobalProfiles_Delete(name))
                {
                    Global_RefreshGlobalProfileCombo(st);
                    Global_RequestSave(hWnd);
                    Global_UpdateUi(st);
                }
                return 0;
            }

            st->pendingDeleteIdx = idx;
            st->pendingDeleteTick = now;
            st->pendingDeleteIsGlobalProfile = true;
            GlobalToast_ShowNearCursor(hWnd, st, L"Click again to confirm delete");
            return 0;
        }

        return 0;
    }

    if (msg == PremiumCombo::MsgExtraIcon())
    {
        if (!st || (HWND)lParam != st->cmbGlobalProfile)
            return 0;

        if (!GlobalProfiles_Save(GlobalProfiles_GetActiveName()))
            return 0;

        GlobalProfiles_SetDirty(false);
        Global_UpdateProfileSaveIcon(st);
        Global_RequestSave(hWnd);
        CustomPageSurface_MarkDirty(hWnd, &st->surface);
        return 0;
    }

    if (msg == WM_APP_ENGINE_RUNTIME_STATE_CHANGED)
    {
        Global_UpdatePulse(hWnd, st);
        if (st) CustomPageSurface_MarkDirty(hWnd, &st->surface);
        return 0;
    }

    if (msg == WM_APP_GLOBAL_PROFILE_DIRTY)
    {
        if (st)
        {
            Global_UpdateProfileSaveIcon(st);
            CustomPageSurface_MarkDirty(hWnd, &st->surface);
        }
        return 0;
    }

    switch (msg)
    {
    case WM_ERASEBKGND:
        return 1;

    case WM_PAINT:
    {
        PAINTSTRUCT ps{};
        HDC memDC = nullptr;
        HBITMAP bmp = nullptr;
        HGDIOBJ oldBmp = nullptr;
        BeginDoubleBufferPaint(hWnd, ps, memDC, bmp, oldBmp, true);
        if (st)
        {
            st->surface.scrollY = st->scrollY;
            if (st->loggingError != SupportLog_LastError())
                Global_Layout(hWnd, st);
            st->surface.contentHeight = st->contentHeight;
            CustomPageSurface_Present(hWnd, memDC, &st->surface,
                Global_RenderContent, st, st->scroll.draggingThumb);
            Global_UpdatePulse(hWnd, st);
            Global_DrawPulse(hWnd, memDC, st);
        }
        EndDoubleBufferPaint(hWnd, ps, memDC, bmp, oldBmp);
        return 0;
    }

    case WM_CTLCOLORSTATIC:
    {
        HDC hdc = (HDC)wParam;
        SetBkMode(hdc, TRANSPARENT);

        HWND hCtl = (HWND)lParam;
        if (st && hCtl && st->lblFactoryResetHint && hCtl == st->lblFactoryResetHint)
            SetTextColor(hdc, UiTheme::Color_TextMuted());
        else
            SetTextColor(hdc, UiTheme::Color_Text());

        return (LRESULT)UiTheme::Brush_PanelBg();
    }

    case WM_CREATE:
    {
        HINSTANCE hInst = (HINSTANCE)GetWindowLongPtrW(hWnd, GWLP_HINSTANCE);
        HFONT hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);

        st = new GlobalSettingsPageState();
        SetWindowLongPtrW(hWnd, GWLP_USERDATA, (LONG_PTR)st);

        st->lblGlobalProfile = CreateWindowW(L"STATIC", L"Global profile",
            WS_CHILD | WS_VISIBLE, 0, 0, 10, 10, hWnd, nullptr, hInst, nullptr);
        SendMessageW(st->lblGlobalProfile, WM_SETFONT, (WPARAM)hFont, TRUE);

        st->cmbGlobalProfile = PremiumCombo::Create(hWnd, hInst,
            0, 0, 10, 10, GLOB_ID_GLOBAL_PROFILE_COMBO,
            WS_CHILD | WS_VISIBLE | WS_TABSTOP);
        PremiumCombo::SetFont(st->cmbGlobalProfile, hFont, true);
        Global_RefreshGlobalProfileCombo(st);

        st->lblLayout = CreateWindowW(L"STATIC", L"Keyboard layout",
            WS_CHILD | WS_VISIBLE, 0, 0, 10, 10, hWnd, nullptr, hInst, nullptr);
        SendMessageW(st->lblLayout, WM_SETFONT, (WPARAM)hFont, TRUE);

        st->cmbLayout = PremiumCombo::Create(hWnd, hInst,
            0, 0, 10, 10, GLOB_ID_LAYOUT_COMBO,
            WS_CHILD | WS_VISIBLE | WS_TABSTOP);
        PremiumCombo::SetFont(st->cmbLayout, hFont, true);
        st->layoutPicker.model = st->cmbLayout;
        st->layoutPicker.brand = PremiumCombo::Create(hWnd, hInst,
            0, 0, 10, 10, GLOB_ID_LAYOUT_BRAND, WS_CHILD | WS_TABSTOP);
        PremiumCombo::SetFont(st->layoutPicker.brand, hFont, false);
        PremiumCombo::SetDropMaxVisible(st->layoutPicker.brand, 10);
        st->layoutPicker.variant = PremiumCombo::Create(hWnd, hInst,
            0, 0, 10, 10, GLOB_ID_LAYOUT_VARIANT, WS_CHILD | WS_TABSTOP);
        PremiumCombo::SetFont(st->layoutPicker.variant, hFont, false);
        Global_RefreshLayoutCombo(st);

        st->btnLayoutEditor = CreateWindowW(L"BUTTON", L"Layout editor",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
            0, 0, 10, 10, hWnd, (HMENU)(INT_PTR)GLOB_ID_LAYOUT_EDITOR, hInst, nullptr);
        SendMessageW(st->btnLayoutEditor, WM_SETFONT, (WPARAM)hFont, TRUE);

        st->lblPoll = CreateWindowW(L"STATIC", L"Polling rate",
            WS_CHILD | WS_VISIBLE, 0, 0, 10, 10, hWnd, nullptr, hInst, nullptr);
        SendMessageW(st->lblPoll, WM_SETFONT, (WPARAM)hFont, TRUE);

        st->sldPoll = PremiumSlider_Create(hWnd, hInst, 0, 0, 10, 10, GLOB_ID_POLL_SLIDER);
        SendMessageW(st->sldPoll, TBM_SETRANGE, TRUE, MAKELONG(1, 20));
        SendMessageW(st->sldPoll, TBM_SETPOS, TRUE, (LPARAM)std::clamp(Settings_GetPollingMs(), 1u, 20u));

        st->chipPoll = PremiumChip_Create(hWnd, hInst, 0, 0, 10, 10, GLOB_ID_POLL_SLIDER + 100);
        SendMessageW(st->chipPoll, WM_SETFONT, (WPARAM)hFont, TRUE);

        st->lblUiRefresh = CreateWindowW(L"STATIC", L"UI refresh interval",
            WS_CHILD | WS_VISIBLE, 0, 0, 10, 10, hWnd, nullptr, hInst, nullptr);
        SendMessageW(st->lblUiRefresh, WM_SETFONT, (WPARAM)hFont, TRUE);

        st->sldUiRefresh = PremiumSlider_Create(hWnd, hInst, 0, 0, 10, 10, GLOB_ID_UIREFRESH_SLIDER);
        SendMessageW(st->sldUiRefresh, TBM_SETRANGE, TRUE, MAKELONG(1, 200));
        SendMessageW(st->sldUiRefresh, TBM_SETPOS, TRUE, (LPARAM)std::clamp(Settings_GetUIRefreshMs(), 1u, 200u));

        st->chipUiRefresh = PremiumChip_Create(hWnd, hInst, 0, 0, 10, 10, GLOB_ID_UIREFRESH_SLIDER + 100);
        SendMessageW(st->chipUiRefresh, WM_SETFONT, (WPARAM)hFont, TRUE);

        st->lblFactoryReset = CreateWindowW(L"STATIC", L"Factory reset",
            WS_CHILD | WS_VISIBLE, 0, 0, 10, 10, hWnd, nullptr, hInst, nullptr);
        SendMessageW(st->lblFactoryReset, WM_SETFONT, (WPARAM)hFont, TRUE);

        st->btnFactoryReset = CreateWindowW(L"BUTTON", L"Reset All Settings",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
            0, 0, 10, 10, hWnd, (HMENU)(INT_PTR)GLOB_ID_FACTORY_RESET, hInst, nullptr);
        SendMessageW(st->btnFactoryReset, WM_SETFONT, (WPARAM)hFont, TRUE);

        st->lblFactoryResetHint = CreateWindowW(L"STATIC",
            L"Backs up and resets settings, bindings, profiles, layouts and curve presets.",
            WS_CHILD | WS_VISIBLE | SS_LEFT, 0, 0, 10, 10, hWnd, nullptr, hInst, nullptr);
        SendMessageW(st->lblFactoryResetHint, WM_SETFONT, (WPARAM)hFont, TRUE);

        // Retain the two PremiumCombo instances only as popup/keyboard
        // controllers. Every scrolled pixel, including their closed faces, is
        // rendered by the page surface.
        HWND compatibilityChildren[] = {
            st->lblGlobalProfile, st->cmbGlobalProfile, st->lblLayout, st->cmbLayout,
            st->btnLayoutEditor, st->lblPoll, st->sldPoll,
            st->chipPoll, st->lblUiRefresh, st->sldUiRefresh, st->chipUiRefresh,
            st->lblFactoryReset, st->btnFactoryReset, st->lblFactoryResetHint
        };
        for (HWND child : compatibilityChildren)
            if (child) ShowWindow(child, SW_HIDE);

        Global_UpdateUi(st);
        Global_Layout(hWnd, st);
        return 0;
    }

    case WM_SIZE:
        Global_Layout(hWnd, st);
        return 0;

    case WM_LBUTTONDOWN:
        if (st)
        {
            CustomPageSurface_SetState(&st->surface, st->scrollY, st->contentHeight);
            POINT pt{ (short)LOWORD(lParam), (short)HIWORD(lParam) };
            Global_CloseComboAnchors(st);
            auto scrollResult = CustomPageSurface_HandleScrollMessage(hWnd, &st->surface,
                &st->scroll, msg, wParam, lParam, S(hWnd, 54));
            if (scrollResult != CustomPageScrollResult::NotHandled)
            {
                st->scrollY = st->surface.scrollY;
                return 0;
            }
            st->pressedId = Global_HitTest(st, pt);
            st->dragId = (st->pressedId == GLOB_ID_POLL_SLIDER ||
                st->pressedId == GLOB_ID_UIREFRESH_SLIDER) ? st->pressedId : 0;
            if (st->pressedId)
            {
                SetCapture(hWnd);
                if (st->dragId)
                    Global_SetSliderFromClient(hWnd, st, st->dragId, pt.x);
                CustomPageSurface_MarkDirty(hWnd, &st->surface);
            }
            return 0;
        }
        break;

    case WM_MOUSEMOVE:
        if (st)
        {
            POINT pt{ (short)LOWORD(lParam), (short)HIWORD(lParam) };
            auto scrollResult = CustomPageSurface_HandleScrollMessage(hWnd, &st->surface,
                &st->scroll, msg, wParam, lParam, S(hWnd, 54));
            if (scrollResult != CustomPageScrollResult::NotHandled)
            {
                st->scrollY = st->surface.scrollY;
                return 0;
            }
            if (st->dragId)
            {
                Global_SetSliderFromClient(hWnd, st, st->dragId, pt.x);
                return 0;
            }
            const int hot = Global_HitTest(st, pt);
            if (hot != st->hotId)
            {
                st->hotId = hot;
                CustomPageSurface_MarkDirty(hWnd, &st->surface);
            }
            return 0;
        }
        break;

    case WM_LBUTTONUP:
        if (st)
        {
            const bool wasScroll = st->scroll.draggingThumb;
            CustomPageSurface_HandleScrollMessage(hWnd, &st->surface,
                &st->scroll, msg, wParam, lParam, S(hWnd, 54));
            POINT pt{ (short)LOWORD(lParam), (short)HIWORD(lParam) };
            const int pressed = st->pressedId;
            const bool wasSlider = st->dragId != 0;
            st->pressedId = 0;
            st->dragId = 0;
            if (GetCapture() == hWnd)
                ReleaseCapture();
            if (!wasScroll && !wasSlider && pressed && Global_HitTest(st, pt) == pressed)
            {
                if (pressed == GLOB_ID_GLOBAL_PROFILE_COMBO)
                    Global_OpenComboAnchor(hWnd, st, st->cmbGlobalProfile, st->rcGlobalProfile);
                else if (pressed == GLOB_ID_GLOBAL_PROFILE_SAVE)
                {
                    WPARAM wp = MAKEWPARAM((UINT)PremiumCombo::ExtraIconKind::Save,
                        (UINT)GLOB_ID_GLOBAL_PROFILE_COMBO);
                    PostMessageW(hWnd, PremiumCombo::MsgExtraIcon(), wp,
                        (LPARAM)st->cmbGlobalProfile);
                }
                else if (pressed == GLOB_ID_LAYOUT_COMBO)
                    Global_OpenComboAnchor(hWnd, st, st->cmbLayout, st->rcLayout);
                else if (pressed == GLOB_ID_LAYOUT_BRAND)
                    Global_OpenComboAnchor(hWnd, st, st->layoutPicker.brand, st->rcLayoutBrand);
                else if (pressed == GLOB_ID_LAYOUT_VARIANT)
                    Global_OpenComboAnchor(hWnd, st, st->layoutPicker.variant, st->rcLayoutVariant);
                else
                    PostMessageW(hWnd, WM_COMMAND, MAKEWPARAM(pressed, BN_CLICKED), 0);
            }
            CustomPageSurface_MarkDirty(hWnd, &st->surface);
            return 0;
        }
        break;

    case WM_CAPTURECHANGED:
    case WM_CANCELMODE:
        if (st)
        {
            CustomPageSurface_HandleScrollMessage(hWnd, &st->surface,
                &st->scroll, msg, wParam, lParam, S(hWnd, 54));
            st->pressedId = 0;
            st->dragId = 0;
            CustomPageSurface_MarkDirty(hWnd, &st->surface);
        }
        return 0;

    case WM_MOUSEWHEEL:
        if (st)
        {
            Global_CloseComboAnchors(st);
            CustomPageSurface_HandleScrollMessage(hWnd, &st->surface,
                &st->scroll, msg, wParam, lParam, S(hWnd, 54));
            st->scrollY = st->surface.scrollY;
            return 0;
        }
        break;

    case WM_VSCROLL:
        if (st)
        {
            RECT rc{};
            GetClientRect(hWnd, &rc);
            int page = std::max(1, (int)(rc.bottom - rc.top) - S(hWnd, 48));
            int line = std::max(1, S(hWnd, 40));
            int next = st->scrollY;
            switch (LOWORD(wParam))
            {
            case SB_TOP:        next = 0; break;
            case SB_BOTTOM:     next = Global_GetMaxScroll(hWnd, st); break;
            case SB_LINEUP:     next -= line; break;
            case SB_LINEDOWN:   next += line; break;
            case SB_PAGEUP:     next -= page; break;
            case SB_PAGEDOWN:   next += page; break;
            default: break;
            }
            Global_SetScrollY(hWnd, st, next);
            return 0;
        }
        break;

    case WM_SHOWWINDOW:
        Global_UpdatePulse(hWnd, st);
        if (!wParam && st)
        {
            KillTimer(hWnd, GLOBAL_PAUSE_PULSE_TIMER);
            st->pausePulseTimer = false;
        }
        break;

    case WM_TIMER:
        if (st && wParam == GLOBAL_PAUSE_PULSE_TIMER)
        {
            Global_UpdatePulse(hWnd, st);
            if (st->pausePulseTimer)
            {
                RECT rc = Global_PulseRect(hWnd, st);
                InvalidateRect(hWnd, &rc, FALSE);
            }
            return 0;
        }
        if (st && wParam == TOAST_TIMER_ID)
        {
            const ULONGLONG now = GetTickCount64();
            if (st->toastHideAt != 0 && now >= st->toastHideAt)
                GlobalToast_Hide(hWnd, st);
            return 0;
        }
        break;

    case WM_HSCROLL:
    {
        if (!st) return 0;

        if ((HWND)lParam == st->sldPoll)
        {
            int v = (int)SendMessageW(st->sldPoll, TBM_GETPOS, 0, 0);
            v = std::clamp(v, 1, 20);
            Settings_SetPollingMs((UINT)v);
            RealtimeLoop_SetIntervalMs(Settings_GetPollingMs());
            GlobalProfiles_SetDirty(true);
            Global_UpdateProfileSaveIcon(st);
            Global_UpdateUi(st);
            Global_RequestApplyTiming(hWnd);
            Global_RequestSave(hWnd);
            return 0;
        }

        if ((HWND)lParam == st->sldUiRefresh)
        {
            int v = (int)SendMessageW(st->sldUiRefresh, TBM_GETPOS, 0, 0);
            v = std::clamp(v, 1, 200);
            Settings_SetUIRefreshMs((UINT)v);
            GlobalProfiles_SetDirty(true);
            Global_UpdateProfileSaveIcon(st);
            Global_UpdateUi(st);
            Global_RequestApplyTiming(hWnd);
            Global_RequestSave(hWnd);
            return 0;
        }

        return 0;
    }

    case WM_DRAWITEM:
    {
        const DRAWITEMSTRUCT* dis = (const DRAWITEMSTRUCT*)lParam;
        if (st && dis && dis->CtlType == ODT_BUTTON &&
            ((dis->CtlID == GLOB_ID_LAYOUT_EDITOR && st->btnLayoutEditor == dis->hwndItem) ||
             (dis->CtlID == GLOB_ID_FACTORY_RESET && st->btnFactoryReset == dis->hwndItem)))
        {
            Global_DrawActionButton(dis, dis->CtlID == GLOB_ID_FACTORY_RESET);
            return TRUE;
        }
        break;
    }

    case WM_COMMAND:
        if (!st) return 0;

        if (LOWORD(wParam) == (UINT)GLOB_ID_GLOBAL_PROFILE_COMBO && HIWORD(wParam) == CBN_SELCHANGE)
        {
            GlobalDeleteConfirm_Clear(hWnd, st);

            int sel = PremiumCombo::GetCurSel(st->cmbGlobalProfile);
            int count = PremiumCombo::GetCount(st->cmbGlobalProfile);
            bool selIsCreateRow = (count > 0 && sel == count - 1);
            if (selIsCreateRow)
            {
                PremiumCombo::ShowDropDown(st->cmbGlobalProfile, true);
                PremiumCombo::BeginInlineEditSelected(st->cmbGlobalProfile, false);
                return 0;
            }

            if (sel >= 0 && sel < count - 1)
            {
                wchar_t nameBuf[260]{};
                PremiumCombo::GetLBText(st->cmbGlobalProfile, sel, nameBuf, (int)_countof(nameBuf));
                Global_ApplyActiveGlobalProfile(st, hWnd, nameBuf);
                PremiumCombo::ShowDropDown(st->cmbGlobalProfile, false);
                ShowWindow(st->cmbGlobalProfile, SW_HIDE);
                CustomPageSurface_MarkDirty(hWnd, &st->surface);
            }
            return 0;
        }

        if (LOWORD(wParam) == GLOB_ID_LAYOUT_BRAND && HIWORD(wParam) == CBN_SELCHANGE)
        {
            st->layoutPicker.Browse(KeyboardLayout_GetCurrentPresetIndex());
            Global_Layout(hWnd, st);
            Global_CloseComboAnchors(st);
            CustomPageSurface_MarkDirty(hWnd, &st->surface);
            return 0;
        }
        if ((LOWORD(wParam) == GLOB_ID_LAYOUT_COMBO || LOWORD(wParam) == GLOB_ID_LAYOUT_VARIANT) && HIWORD(wParam) == CBN_SELCHANGE)
        {
            GlobalDeleteConfirm_Clear(hWnd, st);
            int sel = LOWORD(wParam) == GLOB_ID_LAYOUT_COMBO ? st->layoutPicker.ChooseModel() : st->layoutPicker.Selected();
            if (sel >= 0 && sel < KeyboardLayout_GetPresetCount() && sel != KeyboardLayout_GetCurrentPresetIndex())
            {
                KeyboardLayout_SetPresetIndex(sel);
                Global_NotifyMainPage(hWnd);
                Global_RequestSave(hWnd);
            }
            st->layoutPicker.Refresh(KeyboardLayout_GetCurrentPresetIndex(), true);
            Global_CloseComboAnchors(st);
            Global_Layout(hWnd, st);
            CustomPageSurface_MarkDirty(hWnd, &st->surface);
            return 0;
        }

        if (LOWORD(wParam) == (UINT)GLOB_ID_LAYOUT_EDITOR && HIWORD(wParam) == BN_CLICKED)
        {
            GlobalDeleteConfirm_Clear(hWnd, st);
            LayoutEditor_OpenWindow(hWnd);
            return 0;
        }

        if (LOWORD(wParam) == GLOB_ID_DIAGNOSTIC_LOGGING && HIWORD(wParam) == BN_CLICKED)
        {
            Settings_SetDiagnosticLogging(!Settings_GetDiagnosticLogging());
            Global_RequestSave(hWnd);
            CustomPageSurface_MarkDirty(hWnd, &st->surface);
            return 0;
        }
        if (LOWORD(wParam) == GLOB_ID_HALLJOY_FOLDER && HIWORD(wParam) == BN_CLICKED)
        {
            const auto directory = AppPaths_DataRoot();
            if (!directory.empty()) {
                CreateDirectoryW(directory.c_str(), nullptr);
                ShellExecuteW(hWnd, L"open", directory.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
            }
            return 0;
        }
        if (LOWORD(wParam) == GLOB_ID_DISCORD && HIWORD(wParam) == BN_CLICKED)
        {
            if (reinterpret_cast<INT_PTR>(ShellExecuteW(hWnd, L"open", kDiscordInviteUrl,
                nullptr, nullptr, SW_SHOWNORMAL)) <= 32) {
                const std::wstring error = std::wstring(L"Could not open Discord. Please try again or use the invite:\n") + kDiscordInviteUrl;
                MessageBoxW(hWnd, error.c_str(),
                    L"HallJoy", MB_OK | MB_ICONWARNING);
            }
            return 0;
        }
        if (LOWORD(wParam) == (UINT)GLOB_ID_ENGINE_RUNTIME && HIWORD(wParam) == BN_CLICKED)
        {
            if (!Global_EngineRuntimeButtonEnabled())
                return 0;
            HWND root = ResolveAppMainWindow(hWnd);
            if (root)
                PostMessageW(root, WM_APP_ENGINE_RUNTIME_TOGGLE, 0, 0);
            return 0;
        }

        if (LOWORD(wParam) == (UINT)GLOB_ID_FACTORY_RESET && HIWORD(wParam) == BN_CLICKED)
        {
            GlobalDeleteConfirm_Clear(hWnd, st);
            const int answer = MessageBoxW(
                hWnd,
                L"Reset all HallJoy settings to defaults?\n\n"
                L"HallJoy will safely back up and reset:\n"
                L"  - application and Input Overlay settings\n"
                L"  - all keyboard and gamepad bindings\n"
                L"  - global profiles and custom layouts\n"
                L"  - curve presets\n\n"
                L"HallJoy will then restart automatically. Your backup will be kept in the HallJoy data folder.",
                L"Reset all HallJoy settings",
                MB_ICONWARNING | MB_YESNO | MB_DEFBUTTON2);
            if (answer != IDYES)
                return 0;

            DWORD error = ERROR_SUCCESS;
            if (!KeyboardUI_CloseLayoutEditor()) return 0;
            if (!FactoryReset_Request(&error))
            {
                wchar_t message[256]{};
                swprintf_s(message,
                    L"HallJoy could not prepare the factory reset. No settings were changed.\n\nWindows error: %lu",
                    static_cast<unsigned long>(error));
                MessageBoxW(hWnd, message, L"HallJoy factory reset", MB_ICONERROR | MB_OK);
                return 0;
            }

            HWND root = ResolveAppMainWindow(hWnd);
            if (root)
                PostMessageW(root, WM_APP_FACTORY_RESET_RESTART, 0, 0);
            return 0;
        }

        return 0;

    case WM_NCDESTROY:
        if (st)
        {
            if (st->scroll.draggingThumb && GetCapture() == hWnd)
                ReleaseCapture();
            GlobalDeleteConfirm_Clear(hWnd, st);
            if (st->hToast && IsWindow(st->hToast))
                DestroyWindow(st->hToast);
            st->hToast = nullptr;
            KillTimer(hWnd, GLOBAL_PAUSE_PULSE_TIMER);
            CustomPageSurface_Destroy(&st->surface);
            delete st;
            SetWindowLongPtrW(hWnd, GWLP_USERDATA, 0);
        }
        return 0;
    }

    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

// ============================================================================
#if defined(HALLJOY_ANALOG_SIMULATOR)
bool KeyboardSubpages_TestLayoutPicker()
{
    if (AppPaths_Mode() != AppDataMode::SimulatorOverride) return false;
    HDESK previous = GetThreadDesktop(GetCurrentThreadId());
    wchar_t name[80]{}; swprintf_s(name, L"HallJoyPickerTest-%lu", GetCurrentProcessId());
    HDESK desktop = CreateDesktopW(name, nullptr, nullptr, 0, GENERIC_ALL, nullptr);
    if (!desktop) return false;
    if (!SetThreadDesktop(desktop)) { CloseDesktop(desktop); return false; }
    const int saved = KeyboardLayout_GetCurrentPresetIndex();
    KeyboardLayout_SetPresetIndex(0);
    WNDCLASSW wc{}; wc.lpfnWndProc = KeyboardSubpages_GlobalSettingsPageProc;
    wc.hInstance = GetModuleHandleW(nullptr); wc.lpszClassName = L"HallJoyPickerTestPage";
    RegisterClassW(&wc);
    HWND owner = CreateWindowW(L"STATIC", L"", WS_OVERLAPPEDWINDOW,
        0, 0, 900, 800, nullptr, nullptr, wc.hInstance, nullptr);
    HWND page = CreateWindowW(wc.lpszClassName, L"", WS_CHILD,
        0, 0, 850, 750, owner, nullptr, wc.hInstance, nullptr);
    auto* st = (GlobalSettingsPageState*)GetWindowLongPtrW(page, GWLP_USERDATA);
    bool ok = st && owner;
    if (st) {
        auto& picker = st->layoutPicker;
        ok &= picker.Selected() == 0 && picker.browsing == L"DrunkDeer";
        const auto snapshot = KeyboardLayout_GetSnapshot();
        auto browse = [&](const wchar_t* brand) {
            auto it = std::find(picker.brands.begin(), picker.brands.end(), brand);
            PremiumCombo::SetCurSel(picker.brand, (int)(it - picker.brands.begin()), false);
            SendMessageW(page, WM_COMMAND, MAKEWPARAM(GLOB_ID_LAYOUT_BRAND, CBN_SELCHANGE), (LPARAM)picker.brand);
        };
        ok &= picker.brands.front() == L"All";
        browse(L"All");
        size_t represented=0;
        for (const auto& group : picker.groups) represented+=group.members.size();
        ok &= represented == (size_t)KeyboardLayout_GetPresetCount();
        ok &= picker.groups.size() < represented;
        ok &= KeyboardLayout_GetSnapshot() == snapshot;
        for (int i = 0; i < KeyboardLayout_GetPresetCount(); ++i) {
            wchar_t label[512]{};
            PremiumCombo::GetLBText(picker.model, picker.Row(i), label, (int)std::size(label));
            ok &= std::wstring(label) == LayoutPicker::ModelName(i,true);
        }
        PremiumCombo::SetCurSel(picker.model, picker.Row(4), false);
        SendMessageW(page, WM_COMMAND, MAKEWPARAM(GLOB_ID_LAYOUT_COMBO, CBN_SELCHANGE), (LPARAM)picker.model);
        Global_UpdateUi(st);
        ok &= KeyboardLayout_GetCurrentPresetIndex() == 4 && picker.browsing == L"All" && picker.Selected() == 4;
        picker.Refresh(4, true);
        ok &= picker.browsing == L"All" && std::wstring(picker.CreationBrand()) == L"Custom";
        KeyboardLayout_SetPresetIndex(0);
        Global_UpdateUi(st);
        browse(L"Keychron");
        Global_UpdateUi(st); // A normal settings refresh must not undo browsing.
        ok &= picker.browsing == L"Keychron" && picker.Selected() == LayoutPicker::Invalid;
        ok &= KeyboardLayout_GetCurrentPresetIndex() == 0;
        size_t keychronCount = 0;
        for (int i = 0; i < KeyboardLayout_GetPresetCount(); ++i)
            if (KeyboardLayout_GetPresetBrand(i) == L"Keychron") ++keychronCount;
        represented=0;
        for (const auto& group : picker.groups) represented+=group.members.size();
        ok &= represented == keychronCount && picker.Row(0) < 0 && !picker.HasVariants();
        ok &= LayoutPicker::ModelName(picker.PresetAt(0), false) == L"K2 HE";
        ok &= LayoutPicker::VariantName(picker.PresetAt(0)) == L"JIS"; // ANSI/ISO belong to the shared K2/K3 model.
        for (size_t row=1;row<picker.presets.size();++row)
            ok &= !halljoy::layout_sort::Less(picker.groups[row].label, picker.groups[row-1].label);
        const int keychron = picker.PresetAt(0);
        // Exercise the production scrollbar against an actual popup on this
        // private desktop; never move the user's real cursor.
        PremiumCombo::ShowDropDown(picker.model, true);
        auto* combo = PremiumComboInternal::Get(picker.model);
        if (combo && combo->hwndPopup) {
            using namespace PremiumComboInternal;
            SetWindowPos(combo->hwndPopup, nullptr, 0, 0, 300, 200, SWP_NOZORDER | SWP_NOACTIVATE);
            ScrollGeometry g;
            ok &= GetScrollGeometry(combo, g);
            const int selected = combo->curSel;
            auto screen = [&](int x, int y) { POINT p{x,y}; ClientToScreen(combo->hwndPopup, &p); return p; };
            POINT grab = screen(g.thumb.left, g.thumb.top+2);
            bool inside = false;
            ok &= HitTestPopupIndexFromScreen(combo, grab, inside) == -1 && inside;
            ok &= ScrollMouseDown(combo, grab) && combo->scrollDragging;
            ScrollMouseMove(combo, screen(-100, 10000));
            ok &= combo->scrollTop == GetMaxScrollTop(combo) && combo->curSel == selected && combo->dropped;
            ScrollMouseMove(combo, screen(1000, -10000));
            ok &= combo->scrollTop == 0;
            SendMessageW(picker.model, WM_LBUTTONUP, 0, 0);
            ok &= !combo->scrollDragging && combo->dropped && combo->curSel == selected;
            ok &= ScrollMouseDown(combo, screen(g.track.left, g.track.bottom-1));
            ok &= combo->scrollTop > 0 && !combo->scrollDragging && combo->curSel == selected;
            GetScrollGeometry(combo, g);
            ok &= ScrollMouseDown(combo, screen(g.thumb.left, g.thumb.top+2));
            SendMessageW(picker.model, WM_CANCELMODE, 0, 0);
            ok &= !combo->scrollDragging && !combo->dropped && combo->curSel == selected;
        } else ok = false;
        PremiumCombo::ShowDropDown(picker.model, false);
        PremiumCombo::SetCurSel(picker.model, 0, false);
        SendMessageW(page, WM_COMMAND, MAKEWPARAM(GLOB_ID_LAYOUT_COMBO, CBN_SELCHANGE), (LPARAM)picker.model);
        ok &= KeyboardLayout_GetCurrentPresetIndex() == keychron && keychron != 0;
        browse(L"Lemokey");
        ok &= picker.presets.size()==1 && !picker.HasVariants();
        PremiumCombo::SetCurSel(picker.model,0,false);
        SendMessageW(page,WM_COMMAND,MAKEWPARAM(GLOB_ID_LAYOUT_COMBO,CBN_SELCHANGE),(LPARAM)picker.model);
        ok &= picker.HasVariants() && picker.variants.size()==2;
        ok &= st->rcLayout.right < st->rcLayoutVariant.left;
        for (int row=0;row<2;++row) {
            ok &= LayoutPicker::VariantName(picker.VariantAt(row)) == (row==0 ? L"ANSI" : L"ISO");
            PremiumCombo::SetCurSel(picker.variant,row,false);
            SendMessageW(page,WM_COMMAND,MAKEWPARAM(GLOB_ID_LAYOUT_VARIANT,CBN_SELCHANGE),(LPARAM)picker.variant);
            ok &= KeyboardLayout_Count()==(row==0 ? 81 : 82);
            bool enter=false;
            for (int i=0;i<KeyboardLayout_Count();++i) {
                const auto& key=KeyboardLayout_Data()[i];
                if (key.hid==40) { enter=true; ok &= (key.notchW>0)==(row==1); }
            }
            ok &= enter;
        }
        browse(L"DrunkDeer");
        ok &= picker.presets.size()==5;
        struct DrunkDeerExpected { const wchar_t* name; int count; bool compound; };
        for (const auto& expected : {DrunkDeerExpected{L"A75 ANSI",82,false},
            {L"A75 Pro",82,false},{L"A75 ISO",83,true},{L"G60 ANSI",61,false},
            {L"G65 ANSI",68,false},{L"G75 ANSI",84,false},{L"G75 JIS",86,true}}) {
            int target=-1;
            for (int i=0;i<KeyboardLayout_GetPresetCount();++i)
                if (KeyboardLayout_GetPresetBrand(i)==L"DrunkDeer" && KeyboardLayout_GetPresetModel(i)==expected.name) target=i;
            ok &= target>=0 && picker.Row(target)>=0;
            PremiumCombo::SetCurSel(picker.model,picker.Row(target),false);
            SendMessageW(page,WM_COMMAND,MAKEWPARAM(GLOB_ID_LAYOUT_COMBO,CBN_SELCHANGE),(LPARAM)picker.model);
            const auto variant=std::find(picker.variants.begin(),picker.variants.end(),target);
            ok &= variant!=picker.variants.end();
            PremiumCombo::SetCurSel(picker.variant,(int)(variant-picker.variants.begin()),false);
            SendMessageW(page,WM_COMMAND,MAKEWPARAM(GLOB_ID_LAYOUT_VARIANT,CBN_SELCHANGE),(LPARAM)picker.variant);
            ok &= picker.Selected()==target && KeyboardLayout_Count()==expected.count;
            bool found=false;
            for (int i=0;i<KeyboardLayout_Count();++i) {
                const auto& key=KeyboardLayout_Data()[i];
                if (key.hid==40) { found=true; ok &= (key.notchW>0)==expected.compound; }
            }
            ok &= found;
        }
        KeyboardLayout_SetPresetIndex(keychron);
        Global_UpdateUi(st);
        browse(L"Custom");
        ok &= picker.Selected() == LayoutPicker::Invalid;
        ok &= KeyboardLayout_GetCurrentPresetIndex() == keychron;
        KeyboardLayout_SetPresetIndex(2);
        SendMessageW(page, WM_APP_KEYBOARD_LAYOUT_CHANGED, 0, 0);
        ok &= picker.browsing == L"Other" && picker.Selected() == 2;
        ok &= st->rcLayoutBrand.right < st->rcLayout.left && st->rcLayoutBrand.top == st->rcLayout.top;
        const int count = KeyboardLayout_GetPresetCount();
        int transient = -1;
        ok &= KeyboardLayout_CreatePreset(L"Picker Transient", &transient, 0, false);
        ok &= transient >= 0 && KeyboardLayout_DeletePreset(transient);
        ok &= KeyboardLayout_GetPresetCount() == count && !picker.Current();
        ok &= picker.Selected() == LayoutPicker::Invalid; // Same count, stale row map.
        Global_UpdateUi(st);
        ok &= picker.Current() && picker.Selected() == 2;
        // Every concrete variant is reachable, round-trips through refresh and
        // never offers another model's geometry. All remains the selected view.
        browse(L"All");
        for (int target=0;target<KeyboardLayout_GetPresetCount();++target) {
            PremiumCombo::SetCurSel(picker.model,picker.Row(target),false);
            SendMessageW(page,WM_COMMAND,MAKEWPARAM(GLOB_ID_LAYOUT_COMBO,CBN_SELCHANGE),(LPARAM)picker.model);
            const auto it=std::find(picker.variants.begin(),picker.variants.end(),target);
            ok &= it!=picker.variants.end();
            PremiumCombo::SetCurSel(picker.variant,(int)(it-picker.variants.begin()),false);
            SendMessageW(page,WM_COMMAND,MAKEWPARAM(GLOB_ID_LAYOUT_VARIANT,CBN_SELCHANGE),(LPARAM)picker.variant);
            picker.Refresh(target,true);
            ok &= picker.Selected()==target && KeyboardLayout_GetCurrentPresetIndex()==target && picker.browsing==L"All";
            ok &= picker.HasVariants()==(picker.groups[picker.Row(target)].members.size()>1);
            for (int member : picker.variants)
                ok &= KeyboardLayout_GetPresetBrand(member)==KeyboardLayout_GetPresetBrand(target) &&
                    LayoutPicker::ModelName(member,false)==LayoutPicker::ModelName(target,false);
        }
        // Exercise variant-specific editor cancellation and two-step deletion
        // on disposable catalog entries, never on the user's actual layouts.
        int ansi=-1,iso=-1;
        ok &= KeyboardLayout_CreatePreset(L"Variant Test ANSI",&ansi,0,false,L"DrunkDeer");
        ok &= KeyboardLayout_CreatePreset(L"Variant Test ISO",&iso,0,false,L"DrunkDeer");
        LayoutEditor_OpenWindow(owner,ansi);
        auto* host=(LayoutEditorHostState*)GetWindowLongPtrW(g_hLayoutEditorWindow,GWLP_USERDATA);
        auto* draft=host ? (LayoutPageState*)GetWindowLongPtrW(host->hPage,GWLP_USERDATA) : nullptr;
        if (draft) {
            const HWND editor=host->hPage;
            ok &= draft->layoutPicker.HasVariants() && draft->layoutPicker.Row(ansi)==draft->layoutPicker.Row(iso);
            SendMessageW(editor,WM_COMMAND,MAKEWPARAM(ID_LAYOUT_ADD,BN_CLICKED),0);
            g_layoutTestDecision=IDCANCEL;
            PremiumCombo::SetCurSel(draft->layoutPicker.variant,1,false);
            SendMessageW(editor,WM_COMMAND,MAKEWPARAM(ID_LAYOUT_VARIANT,CBN_SELCHANGE),(LPARAM)draft->layoutPicker.variant);
            ok &= draft->hasUnsaved && draft->editingPresetIdx==ansi && draft->layoutPicker.Selected()==ansi;
            g_layoutTestDecision=IDNO;
            PremiumCombo::SetCurSel(draft->layoutPicker.variant,1,false);
            SendMessageW(editor,WM_COMMAND,MAKEWPARAM(ID_LAYOUT_VARIANT,CBN_SELCHANGE),(LPARAM)draft->layoutPicker.variant);
            ok &= !draft->hasUnsaved && draft->editingPresetIdx==iso && draft->layoutPicker.Selected()==iso;
            const HWND variants=draft->layoutPicker.variant;
            const int beforeDelete=KeyboardLayout_GetPresetCount();
            SendMessageW(editor,PremiumCombo::MsgItemButton(),MAKEWPARAM(1,(int)PremiumCombo::ItemButtonKind::Delete),(LPARAM)variants);
            ok &= KeyboardLayout_GetPresetCount()==beforeDelete && PremiumCombo::GetDeleteConfirmation(variants)==1;
            SendMessageW(editor,PremiumCombo::MsgItemButton(),MAKEWPARAM(1,(int)PremiumCombo::ItemButtonKind::ConfirmDelete),(LPARAM)variants);
            ok &= KeyboardLayout_GetPresetCount()==beforeDelete-1;
            ok &= std::wstring(KeyboardLayout_GetPresetName(ansi))==L"Variant Test ANSI";
            if (IsWindow(g_hLayoutEditorWindow)) ok &= KeyboardUI_CloseLayoutEditor();
        } else ok=false;
        g_layoutTestDecision=0;
        ok &= KeyboardLayout_DeletePreset(ansi);
    }
    if (owner) DestroyWindow(owner);
    UnregisterClassW(wc.lpszClassName, wc.hInstance);
    KeyboardLayout_SetPresetIndex(saved);
    ok &= SetThreadDesktop(previous) != FALSE;
    CloseDesktop(desktop);
    return ok;
}
#endif

// Mouse settings page
// Disabled in keyboard_page_main.cpp: this part currently works poorly.
// Retained as a foundation for future repair; do not remove the implementation.
// ============================================================================
struct MouseSettingsPageState
{
    HWND lblEnable = nullptr;
    HWND lblBetaNote = nullptr;
    HWND btnEnable = nullptr;
    HWND btnBlockMouse = nullptr;
    HWND lblTarget = nullptr;
    HWND cmbTarget = nullptr;
    HWND lblSensitivity = nullptr;
    HWND sldSensitivity = nullptr;
    HWND chipSensitivity = nullptr;
    HWND lblAggressiveness = nullptr;
    HWND sldAggressiveness = nullptr;
    HWND chipAggressiveness = nullptr;
    HWND lblMaxOffset = nullptr;
    HWND sldMaxOffset = nullptr;
    HWND chipMaxOffset = nullptr;
    HWND lblFollowSpeed = nullptr;
    HWND sldFollowSpeed = nullptr;
    HWND chipFollowSpeed = nullptr;
    HWND lblAsiStatus = nullptr;
    HWND lblHint = nullptr;
    RECT visRc{};
    RECT visDynRc{};
    CustomPageSurface surface;
    int scrollY = 0;
    int contentHeight = 0;
    bool scrollDrag = false;
    int  scrollDragGrabOffsetY = 0;
    int  scrollDragThumbHeight = 0;
    int  scrollDragMax = 0;
};

static constexpr int MOUSE_ID_ENABLE_BUTTON = 7801;
static constexpr int MOUSE_ID_TARGET_COMBO = 7802;
static constexpr int MOUSE_ID_SENS_SLIDER = 7803;
static constexpr int MOUSE_ID_BLOCK_MOUSE_INPUT = 7804;
static constexpr int MOUSE_ID_AGGR_SLIDER = 7807;
static constexpr int MOUSE_ID_MAX_OFFSET_SLIDER = 7808;
static constexpr int MOUSE_ID_FOLLOW_SPEED_SLIDER = 7809;
static constexpr UINT_PTR MOUSE_STATUS_TIMER_ID = 7805;
static constexpr UINT_PTR MOUSE_VIS_TIMER_ID = 7806;
static constexpr int MOUSE_VIS_FIXED_W = 420;
static constexpr int MOUSE_VIS_FIXED_H = 240;

static void Mouse_RequestSave(HWND hWnd)
{
    HWND root = ResolveAppMainWindow(hWnd);
    if (root) PostMessageW(root, WM_APP_REQUEST_SAVE, 0, 0);
}

static void Mouse_MarkGlobalProfileDirty()
{
    if (g_hPageGlobal && IsWindow(g_hPageGlobal))
        PostMessageW(g_hPageGlobal, WM_APP_GLOBAL_PROFILE_DIRTY, 0, 0);
}

static void Mouse_UpdateUi(MouseSettingsPageState* st)
{
    if (!st) return;
    if (st->btnEnable)
    {
        SetWindowTextW(
            st->btnEnable,
            Settings_GetMouseToStickEnabled() ? L"Mouse to Stick: ON" : L"Mouse to Stick: OFF");
    }
    if (st->btnBlockMouse)
    {
        SetWindowTextW(
            st->btnBlockMouse,
            Settings_GetBlockMouseInput() ? L"Block Mouse Input: ON" : L"Block Mouse Input: OFF");
    }
    if (st->chipSensitivity)
    {
        wchar_t b[32]{};
        swprintf_s(b, L"%d%%", (int)std::lround(Settings_GetMouseToStickSensitivity() * 100.0f));
        SetWindowTextW(st->chipSensitivity, b);
    }
    if (st->chipAggressiveness)
    {
        wchar_t b[32]{};
        swprintf_s(b, L"%.2fx", Settings_GetMouseToStickAggressiveness());
        SetWindowTextW(st->chipAggressiveness, b);
    }
    if (st->chipMaxOffset)
    {
        wchar_t b[32]{};
        swprintf_s(b, L"%.2fx", Settings_GetMouseToStickMaxOffset());
        SetWindowTextW(st->chipMaxOffset, b);
    }
    if (st->chipFollowSpeed)
    {
        wchar_t b[32]{};
        swprintf_s(b, L"%.2fx", Settings_GetMouseToStickFollowSpeed());
        SetWindowTextW(st->chipFollowSpeed, b);
    }
    if (st->lblAsiStatus)
    {
        SetWindowTextW(
            st->lblAsiStatus,
            MouseIpc_IsAsiConnected()
                ? L"ASI bridge: Connected"
                : L"ASI bridge: Not detected");
    }
}

static int Mouse_ScrollbarWidthPx(HWND hWnd) { return S(hWnd, 12); }
static int Mouse_ScrollbarMarginPx(HWND hWnd) { return S(hWnd, 8); }

static int Mouse_GetViewportHeight(HWND hWnd)
{
    RECT rc{};
    GetClientRect(hWnd, &rc);
    return std::max(0, (int)rc.bottom - (int)rc.top);
}

static RECT Mouse_GetScrollTrackRect(HWND hWnd)
{
    return CustomPageSurface_GetScrollTrackRect(hWnd);
}

static int Mouse_GetMaxScroll(HWND hWnd, MouseSettingsPageState* st)
{
    if (!st) return 0;
    CustomPageSurface_SetState(&st->surface, st->scrollY, st->contentHeight);
    return CustomPageSurface_GetMaxScroll(hWnd, &st->surface);
}

static RECT Mouse_GetScrollThumbRect(HWND hWnd, MouseSettingsPageState* st)
{
    RECT tr = CustomPageSurface_GetScrollTrackRect(hWnd);
    if (!st) return tr;
    CustomPageSurface_SetState(&st->surface, st->scrollY, st->contentHeight);
    return CustomPageSurface_GetScrollThumbRect(hWnd, &st->surface);
}

static void Mouse_OffsetAllChildren(HWND hWnd, int dy)
{
    if (dy == 0) return;
    int count = 0;
    for (HWND c = GetWindow(hWnd, GW_CHILD); c; c = GetWindow(c, GW_HWNDNEXT))
        ++count;
    if (count <= 0) return;

    HDWP hdwp = BeginDeferWindowPos(count);
    for (HWND c = GetWindow(hWnd, GW_CHILD); c; c = GetWindow(c, GW_HWNDNEXT))
    {
        RECT r{};
        if (!GetWindowRect(c, &r)) continue;
        MapWindowPoints(nullptr, hWnd, (LPPOINT)&r, 2);
        if (hdwp)
        {
            hdwp = DeferWindowPos(hdwp, c, nullptr, r.left, r.top + dy, 0, 0,
                SWP_NOZORDER | SWP_NOSIZE | SWP_NOACTIVATE);
        }
        else
        {
            SetWindowPos(c, nullptr, r.left, r.top + dy, 0, 0,
                SWP_NOZORDER | SWP_NOSIZE | SWP_NOACTIVATE);
        }
    }
    if (hdwp) EndDeferWindowPos(hdwp);
}

static void Mouse_RequestFullRepaint(HWND hWnd)
{
    RedrawWindow(hWnd, nullptr, nullptr,
        RDW_INVALIDATE | RDW_NOERASE | RDW_ALLCHILDREN);
}

static void Mouse_SetScrollY(HWND hWnd, MouseSettingsPageState* st, int newScrollY)
{
    if (!st) return;
    int maxScroll = Mouse_GetMaxScroll(hWnd, st);
    int target = std::clamp(newScrollY, 0, maxScroll);
    if (target != st->scrollY)
    {
        int dy = st->scrollY - target;
        Mouse_OffsetAllChildren(hWnd, dy);
        st->scrollY = target;
        CustomPageSurface_SetState(&st->surface, st->scrollY, st->contentHeight);
    }
    Mouse_RequestFullRepaint(hWnd);
}

static RECT Mouse_ContentRectToClient(const RECT& contentRc, int scrollY)
{
    RECT r = contentRc;
    OffsetRect(&r, 0, -scrollY);
    return r;
}

static void Mouse_InvalidateDynamicVisual(HWND hWnd, MouseSettingsPageState* st)
{
    if (!st) return;
    if (st->visDynRc.right <= st->visDynRc.left || st->visDynRc.bottom <= st->visDynRc.top) return;
    RECT clientRc = Mouse_ContentRectToClient(st->visDynRc, st->scrollY);
    RECT wndRc{};
    GetClientRect(hWnd, &wndRc);
    RECT paintRc{};
    if (!IntersectRect(&paintRc, &clientRc, &wndRc))
        return;
    RedrawWindow(hWnd, &paintRc, nullptr, RDW_INVALIDATE | RDW_NOERASE | RDW_UPDATENOW);
}

static void DrawMouseScrollbar(HWND hWnd, HDC hdc, MouseSettingsPageState* st)
{
    if (!st) return;
    CustomPageSurface_SetState(&st->surface, st->scrollY, st->contentHeight);
    CustomPageSurface_DrawScrollbar(hWnd, hdc, &st->surface, st->scrollDrag);
}

static void Mouse_DrawVisualization(HWND hWnd, HDC hdc, MouseSettingsPageState* st)
{
    if (!st) return;
    RECT rc = st->visRc;
    if (rc.right <= rc.left || rc.bottom <= rc.top) return;

    Graphics g(hdc);
    g.SetSmoothingMode(SmoothingModeHighQuality);
    g.SetPixelOffsetMode(PixelOffsetModeHighQuality);
    g.SetTextRenderingHint(TextRenderingHintAntiAliasGridFit);

    RectF panel((float)rc.left, (float)rc.top, (float)(rc.right - rc.left), (float)(rc.bottom - rc.top));
    const float pad = (float)S(hWnd, 10);
    const float titleH = (float)S(hWnd, 20);

    {
        GraphicsPath p;
        AddRoundRectPath(p, panel, (float)S(hWnd, 10));
        SolidBrush bg(Gp(UiTheme::Color_ControlBg(), 255));
        Pen br(Gp(UiTheme::Color_Border(), 255), 1.0f);
        g.FillPath(&bg, &p);
        g.DrawPath(&br, &p);
    }

    RectF titleR(panel.X + pad, panel.Y + pad * 0.4f, panel.Width - pad * 2.0f, titleH);
    {
        FontFamily ff(L"Segoe UI");
        Font font(&ff, std::clamp((float)S(hWnd, 11), 9.0f, 14.0f), FontStyleRegular, UnitPixel);
        StringFormat fmt;
        fmt.SetAlignment(StringAlignmentNear);
        fmt.SetLineAlignment(StringAlignmentCenter);
        SolidBrush tb(Gp(UiTheme::Color_TextMuted()));
        g.DrawString(L"Mouse -> Stick Visualizer", -1, &font, titleR, &fmt, &tb);
    }

    RectF plotR(
        panel.X + pad,
        panel.Y + pad + titleH + (float)S(hWnd, 2),
        panel.Width - pad * 2.0f,
        panel.Height - (pad * 2.0f + titleH + (float)S(hWnd, 42)));
    if (plotR.Width < 40.0f || plotR.Height < 40.0f) return;

    float d = std::min(plotR.Width, plotR.Height);
    RectF sq(plotR.X + (plotR.Width - d) * 0.5f, plotR.Y + (plotR.Height - d) * 0.5f, d, d);
    float cx = sq.X + sq.Width * 0.5f;
    float cy = sq.Y + sq.Height * 0.5f;
    float r = std::max<float>(12.0f, (float)(sq.Width * 0.5f - 4.0f));

    Pen gridPen(Gp(UiTheme::Color_Border(), 150), 1.0f);
    g.DrawLine(&gridPen, (INT)std::lround(cx - r), (INT)std::lround(cy), (INT)std::lround(cx + r), (INT)std::lround(cy));
    g.DrawLine(&gridPen, (INT)std::lround(cx), (INT)std::lround(cy - r), (INT)std::lround(cx), (INT)std::lround(cy + r));
    g.DrawRectangle(&gridPen, cx - r, cy - r, r * 2.0f, r * 2.0f);

    BackendMouseStickDebug dbg{};
    Backend_GetMouseStickDebug(&dbg);

    float errX = dbg.targetX - dbg.followerX;
    float errY = dbg.targetY - dbg.followerY;
    float radius = std::max<float>(1.0f, dbg.radius);
    float nx = std::clamp(errX / radius, -1.2f, 1.2f);
    float ny = std::clamp(errY / radius, -1.2f, 1.2f);

    float tx = cx + nx * r;
    float ty = cy - ny * r;
    float ox = cx + std::clamp(dbg.outputX, -1.0f, 1.0f) * r;
    float oy = cy - std::clamp(dbg.outputY, -1.0f, 1.0f) * r;

    Pen toTarget(Gp(UiTheme::Color_Accent(), 140), 1.2f);
    g.DrawLine(&toTarget, cx, cy, tx, ty);
    Pen toOut(Gp(RGB(90, 180, 255), 210), 1.8f);
    g.DrawLine(&toOut, cx, cy, ox, oy);

    SolidBrush anchorBrush(Gp(UiTheme::Color_Text(), 240));
    g.FillEllipse(&anchorBrush, cx - 3.0f, cy - 3.0f, 6.0f, 6.0f);
    SolidBrush targetBrush(Gp(UiTheme::Color_Accent(), 235));
    g.FillEllipse(&targetBrush, tx - 4.0f, ty - 4.0f, 8.0f, 8.0f);
    SolidBrush outBrush(Gp(RGB(90, 180, 255), 235));
    g.FillEllipse(&outBrush, ox - 4.0f, oy - 4.0f, 8.0f, 8.0f);

    wchar_t l1[160]{};
    wchar_t l2[160]{};
    swprintf_s(l1, L"Target err: X %.1f  Y %.1f   Radius: %.1f", errX, errY, radius);
    swprintf_s(l2, L"Stick out: X %.2f  Y %.2f   Input: %s",
        dbg.outputX, dbg.outputY, dbg.usingRawInput ? L"RAW" : L"Cursor");

    RECT tr1{
        (LONG)std::lround(panel.X + pad),
        (LONG)std::lround(panel.GetBottom() - pad - S(hWnd, 32)),
        (LONG)std::lround(panel.GetRight() - pad),
        (LONG)std::lround(panel.GetBottom() - pad - S(hWnd, 16))
    };
    RECT tr2{
        (LONG)std::lround(panel.X + pad),
        (LONG)std::lround(panel.GetBottom() - pad - S(hWnd, 16)),
        (LONG)std::lround(panel.GetRight() - pad),
        (LONG)std::lround(panel.GetBottom() - pad)
    };
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, UiTheme::Color_TextMuted());
    DrawTextW(hdc, l1, -1, &tr1, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    DrawTextW(hdc, l2, -1, &tr2, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
}

enum class MouseCustomKind
{
    Label,
    Hint,
    Button,
    Slider,
    Chip,
    Visual
};

struct MouseCustomItem
{
    int id = 0;
    MouseCustomKind kind = MouseCustomKind::Label;
    RECT rc{};
    std::wstring text;
    int minV = 0;
    int maxV = 100;
    int value = 0;
    bool enabled = true;
};

struct MouseCustomState
{
    std::vector<MouseCustomItem> items;
    CustomPageSurface surface;
    CustomPageScrollController scroll;
    int scrollY = 0;
    int contentHeight = 0;
    int hotId = 0;
    int pressedId = 0;
    int dragId = 0;
    bool scrollDrag = false;
    int scrollDragGrabOffsetY = 0;
    int scrollDragThumbHeight = 0;
    int scrollDragMax = 0;
    RECT visRc{};
    RECT visDynRc{};
};

static void MouseCustom_RequestSave(HWND hWnd)
{
    GlobalProfiles_SetDirty(true);
    Mouse_MarkGlobalProfileDirty();
    Mouse_RequestSave(hWnd);
}

static int MouseCustom_SliderValue(int id)
{
    switch (id)
    {
    case MOUSE_ID_SENS_SLIDER:
        return std::clamp((int)std::lround(Settings_GetMouseToStickSensitivity() * 100.0f), 10, 800);
    case MOUSE_ID_AGGR_SLIDER:
        return std::clamp((int)std::lround(Settings_GetMouseToStickAggressiveness() * 100.0f), 20, 300);
    case MOUSE_ID_MAX_OFFSET_SLIDER:
        return std::clamp((int)std::lround(Settings_GetMouseToStickMaxOffset() * 100.0f), 0, 600);
    case MOUSE_ID_FOLLOW_SPEED_SLIDER:
        return std::clamp((int)std::lround(Settings_GetMouseToStickFollowSpeed() * 100.0f), 20, 300);
    default:
        return 0;
    }
}

static std::wstring MouseCustom_SliderText(int id)
{
    wchar_t b[32]{};
    switch (id)
    {
    case MOUSE_ID_SENS_SLIDER:
        swprintf_s(b, L"%d%%", MouseCustom_SliderValue(id));
        break;
    case MOUSE_ID_AGGR_SLIDER:
    case MOUSE_ID_MAX_OFFSET_SLIDER:
    case MOUSE_ID_FOLLOW_SPEED_SLIDER:
        swprintf_s(b, L"%.2fx", (double)MouseCustom_SliderValue(id) / 100.0);
        break;
    default:
        b[0] = 0;
        break;
    }
    return b;
}

static void MouseCustom_AddItem(MouseCustomState* st, int id, MouseCustomKind kind, RECT rc, const std::wstring& text = L"")
{
    if (!st) return;
    MouseCustomItem it{};
    it.id = id;
    it.kind = kind;
    it.rc = rc;
    it.text = text;
    st->items.push_back(std::move(it));
}

static void MouseCustom_AddSliderRow(HWND hWnd, MouseCustomState* st, int& y, int x, int w, const wchar_t* label, int id, int minV, int maxV)
{
    int labelH = S(hWnd, 18);
    int sliderH = S(hWnd, 34);
    int gap = S(hWnd, 10);
    int chipW = S(hWnd, 86);
    int sliderW = std::max(S(hWnd, 160), w - chipW - gap);

    MouseCustom_AddItem(st, id + 1000, MouseCustomKind::Label, RECT{ x, y, x + w, y + labelH }, label);
    y += labelH + S(hWnd, 6);

    MouseCustom_AddItem(st, id, MouseCustomKind::Slider, RECT{ x, y, x + sliderW, y + sliderH });
    st->items.back().minV = minV;
    st->items.back().maxV = maxV;
    st->items.back().value = MouseCustom_SliderValue(id);

    MouseCustom_AddItem(st, id + 100, MouseCustomKind::Chip, RECT{ x + sliderW + gap, y - S(hWnd, 1), x + sliderW + gap + chipW, y + sliderH + S(hWnd, 1) }, MouseCustom_SliderText(id));
    y += sliderH + S(hWnd, 10);
}

static void MouseCustom_RebuildLayout(HWND hWnd, MouseCustomState* st)
{
    if (!st) return;
    st->items.clear();

    RECT rc{};
    GetClientRect(hWnd, &rc);
    int margin = S(hWnd, 16);
    int sbReserve = S(hWnd, 8) + S(hWnd, 7) * 2;
    int x = margin;
    int y = margin;
    int w = std::max(S(hWnd, 260), (int)(rc.right - rc.left) - margin * 2 - sbReserve);
    int labelH = S(hWnd, 18);
    int rowGap = S(hWnd, 14);
    int btnH = S(hWnd, 30);
    int btnW = std::min(S(hWnd, 220), w);

    MouseCustom_AddItem(st, 1, MouseCustomKind::Label, RECT{ x, y, x + w, y + labelH }, L"Mouse to stick");
    y += labelH + S(hWnd, 6);
    MouseCustom_AddItem(st, 2, MouseCustomKind::Hint, RECT{ x, y, x + w, y + S(hWnd, 34) },
        L"BETA: this feature is experimental, and I am not happy with how it works yet.");
    y += S(hWnd, 34) + S(hWnd, 8);

    MouseCustom_AddItem(st, MOUSE_ID_ENABLE_BUTTON, MouseCustomKind::Button, RECT{ x, y, x + btnW, y + btnH },
        Settings_GetMouseToStickEnabled() ? L"Mouse to Stick: ON" : L"Mouse to Stick: OFF");
    y += btnH + S(hWnd, 8);

    MouseCustom_AddItem(st, MOUSE_ID_BLOCK_MOUSE_INPUT, MouseCustomKind::Button, RECT{ x, y, x + btnW, y + btnH },
        Settings_GetBlockMouseInput() ? L"Block Mouse Input: ON" : L"Block Mouse Input: OFF");
    y += btnH + rowGap;

    MouseCustom_AddItem(st, 3, MouseCustomKind::Label, RECT{ x, y, x + w, y + labelH }, L"Target stick");
    y += labelH + S(hWnd, 6);
    MouseCustom_AddItem(st, MOUSE_ID_TARGET_COMBO, MouseCustomKind::Button, RECT{ x, y, x + std::min(S(hWnd, 260), w), y + S(hWnd, 28) },
        Settings_GetMouseToStickTarget() == 1 ? L"Right Stick" : L"Left Stick");
    y += S(hWnd, 28) + rowGap;

    MouseCustom_AddSliderRow(hWnd, st, y, x, w, L"Sensitivity", MOUSE_ID_SENS_SLIDER, 10, 800);
    MouseCustom_AddSliderRow(hWnd, st, y, x, w, L"Aggressiveness", MOUSE_ID_AGGR_SLIDER, 20, 300);
    MouseCustom_AddSliderRow(hWnd, st, y, x, w, L"Max offset from center", MOUSE_ID_MAX_OFFSET_SLIDER, 0, 600);
    MouseCustom_AddSliderRow(hWnd, st, y, x, w, L"Follower speed", MOUSE_ID_FOLLOW_SPEED_SLIDER, 20, 300);

    MouseCustom_AddItem(st, 4, MouseCustomKind::Hint, RECT{ x, y, x + w, y + S(hWnd, 36) },
        L"Maps raw mouse movement to one gamepad stick. Useful for games that block mouse + gamepad together.");
    y += S(hWnd, 36) + S(hWnd, 8);

    MouseCustom_AddItem(st, 5, MouseCustomKind::Label, RECT{ x, y, x + w, y + S(hWnd, 20) },
        MouseIpc_IsAsiConnected() ? L"ASI bridge: Connected" : L"ASI bridge: Not detected");
    y += S(hWnd, 20) + S(hWnd, 8);

    int visW = std::min(S(hWnd, MOUSE_VIS_FIXED_W), w);
    int visH = S(hWnd, MOUSE_VIS_FIXED_H);
    st->visRc = RECT{ x, y, x + visW, y + visH };
    int visPad = S(hWnd, 10);
    int titleH = S(hWnd, 20);
    st->visDynRc = st->visRc;
    st->visDynRc.left += visPad;
    st->visDynRc.right -= visPad;
    st->visDynRc.top += visPad + titleH + S(hWnd, 2);
    st->visDynRc.bottom -= visPad;
    MouseCustom_AddItem(st, 6, MouseCustomKind::Visual, st->visRc);
    y += visH + margin;

    st->contentHeight = y;
    st->surface.scrollY = st->scrollY;
    CustomPageSurface_SetContentHeight(hWnd, &st->surface, y);
    st->scrollY = st->surface.scrollY;
}

static MouseCustomItem* MouseCustom_HitTest(MouseCustomState* st, POINT pt)
{
    if (!st) return nullptr;
    pt.y += st->scrollY;
    for (auto it = st->items.rbegin(); it != st->items.rend(); ++it)
    {
        if (PtInRect(&it->rc, pt))
            return &(*it);
    }
    return nullptr;
}

static RECT MouseCustom_ToView(const RECT& rc, int scrollY)
{
    RECT r = rc;
    OffsetRect(&r, 0, -scrollY);
    return r;
}

static void MouseCustom_DrawSlider(HWND hWnd, Graphics& g, const MouseCustomItem& it, const RECT& rc)
{
    CustomPage_DrawSlider(g, hWnd, rc, it.minV, it.maxV, it.value);
}

static void MouseCustom_DrawText(HDC hdc, const std::wstring& text, RECT rc, COLORREF color, UINT fmt)
{
    CustomPage_DrawText(hdc, text, rc, color, fmt);
}

static void MouseCustom_DrawItem(HWND hWnd, HDC hdc, Graphics& g, MouseCustomState* st, const MouseCustomItem& it, int scrollY, const RECT& clipClient)
{
    RECT rc = MouseCustom_ToView(it.rc, scrollY);
    RECT clip{};
    if (!IntersectRect(&clip, &rc, &clipClient))
        return;
    bool hot = st && st->hotId == it.id && it.enabled;
    bool pressed = st && st->pressedId == it.id && it.enabled;
    COLORREF text = it.enabled ? UiTheme::Color_Text() : UiTheme::Color_TextMuted();
    switch (it.kind)
    {
    case MouseCustomKind::Label:
        MouseCustom_DrawText(hdc, it.text, rc, text, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        break;
    case MouseCustomKind::Hint:
        MouseCustom_DrawText(hdc, it.text, rc, UiTheme::Color_TextMuted(), DT_LEFT | DT_TOP | DT_WORDBREAK | DT_END_ELLIPSIS);
        break;
    case MouseCustomKind::Button:
        CustomPage_DrawButton(g, hdc, rc, it.text, hot, pressed, it.enabled);
        break;
    case MouseCustomKind::Slider:
        MouseCustom_DrawSlider(hWnd, g, it, rc);
        break;
    case MouseCustomKind::Chip:
        CustomPage_DrawChip(g, hdc, rc, it.text, it.enabled);
        break;
    case MouseCustomKind::Visual:
    {
        MouseSettingsPageState tmp{};
        tmp.visRc = rc;
        RECT dyn = st ? st->visDynRc : it.rc;
        tmp.visDynRc = MouseCustom_ToView(dyn, scrollY);
        Mouse_DrawVisualization(hWnd, hdc, &tmp);
        break;
    }
    }
}

static void MouseCustom_RenderCacheContent(HWND hWnd, HDC hdc, const RECT& full, void* user)
{
    auto* st = (MouseCustomState*)user;
    if (!st) return;
    HGDIOBJ oldFont = SelectObject(hdc, GetStockObject(DEFAULT_GUI_FONT));
    Graphics g(hdc);
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    g.SetPixelOffsetMode(PixelOffsetModeHighQuality);
    g.SetTextRenderingHint(TextRenderingHintClearTypeGridFit);
    for (const auto& it : st->items)
        MouseCustom_DrawItem(hWnd, hdc, g, st, it, 0, full);
    SelectObject(hdc, oldFont);
}

static void MouseCustom_SetSliderFromPoint(HWND hWnd, MouseCustomState* st, MouseCustomItem* it, int x)
{
    if (!st || !it) return;
    RECT rc = it->rc;
    int h = rc.bottom - rc.top;
    int pad = std::clamp(h / 3, 8, 14);
    int left = rc.left + pad;
    int right = rc.right - pad;
    double t = (double)(x - left) / (double)std::max(1, right - left);
    int v = it->minV + (int)std::lround(std::clamp(t, 0.0, 1.0) * (double)(it->maxV - it->minV));
    switch (it->id)
    {
    case MOUSE_ID_SENS_SLIDER: Settings_SetMouseToStickSensitivity((float)v / 100.0f); break;
    case MOUSE_ID_AGGR_SLIDER: Settings_SetMouseToStickAggressiveness((float)v / 100.0f); break;
    case MOUSE_ID_MAX_OFFSET_SLIDER: Settings_SetMouseToStickMaxOffset((float)v / 100.0f); break;
    case MOUSE_ID_FOLLOW_SPEED_SLIDER: Settings_SetMouseToStickFollowSpeed((float)v / 100.0f); break;
    default: break;
    }
    MouseCustom_RequestSave(hWnd);
    MouseCustom_RebuildLayout(hWnd, st);
    CustomPageSurface_MarkDirty(hWnd, &st->surface);
}

static void MouseCustom_Activate(HWND hWnd, MouseCustomState* st, int id)
{
    if (!st) return;
    switch (id)
    {
    case MOUSE_ID_ENABLE_BUTTON:
        Settings_SetMouseToStickEnabled(!Settings_GetMouseToStickEnabled());
        break;
    case MOUSE_ID_BLOCK_MOUSE_INPUT:
        Settings_SetBlockMouseInput(!Settings_GetBlockMouseInput());
        break;
    case MOUSE_ID_TARGET_COMBO:
        Settings_SetMouseToStickTarget(Settings_GetMouseToStickTarget() == 1 ? 0 : 1);
        break;
    default:
        return;
    }
    MouseCustom_RequestSave(hWnd);
    MouseCustom_RebuildLayout(hWnd, st);
    CustomPageSurface_MarkDirty(hWnd, &st->surface);
}

static LRESULT MouseCustom_PageProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    auto* st = (MouseCustomState*)GetWindowLongPtrW(hWnd, GWLP_USERDATA);
    switch (msg)
    {
    case WM_ERASEBKGND:
        return 1;
    case WM_CREATE:
        st = new MouseCustomState();
        SetWindowLongPtrW(hWnd, GWLP_USERDATA, (LONG_PTR)st);
        MouseCustom_RebuildLayout(hWnd, st);
        SetTimer(hWnd, MOUSE_STATUS_TIMER_ID, 500, nullptr);
        SetTimer(hWnd, MOUSE_VIS_TIMER_ID, std::clamp(Settings_GetUIRefreshMs(), 8u, 33u), nullptr);
        return 0;
    case WM_NCDESTROY:
        if (st)
        {
            CustomPageSurface_Destroy(&st->surface);
            delete st;
            SetWindowLongPtrW(hWnd, GWLP_USERDATA, 0);
        }
        return 0;
    case WM_SIZE:
        MouseCustom_RebuildLayout(hWnd, st);
        InvalidateRect(hWnd, nullptr, FALSE);
        return 0;
    case WM_SHOWWINDOW:
        if (wParam && st)
        {
            MouseCustom_RebuildLayout(hWnd, st);
            CustomPageSurface_MarkDirty(hWnd, &st->surface);
        }
        return 0;
    case WM_PAINT:
    {
        PAINTSTRUCT ps{};
        HDC memDC = nullptr;
        HBITMAP bmp = nullptr;
        HGDIOBJ oldBmp = nullptr;
        BeginDoubleBufferPaint(hWnd, ps, memDC, bmp, oldBmp);
        if (st)
        {
            st->surface.contentHeight = st->contentHeight;
            st->surface.scrollY = st->scrollY;
            CustomPageSurface_Present(hWnd, memDC, &st->surface,
                MouseCustom_RenderCacheContent, st, st->scroll.draggingThumb);
        }
        EndDoubleBufferPaint(hWnd, ps, memDC, bmp, oldBmp);
        return 0;
    }
    case WM_TIMER:
        if (st && (wParam == MOUSE_STATUS_TIMER_ID || wParam == MOUSE_VIS_TIMER_ID))
        {
            MouseCustom_RebuildLayout(hWnd, st);
            CustomPageSurface_MarkDirty(hWnd, &st->surface);
            return 0;
        }
        break;
    case WM_MOUSEWHEEL:
        if (st)
        {
            st->surface.scrollY = st->scrollY;
            st->surface.contentHeight = st->contentHeight;
            CustomPageSurface_HandleScrollMessage(hWnd, &st->surface, &st->scroll,
                msg, wParam, lParam, S(hWnd, 44));
            st->scrollY = st->surface.scrollY;
            return 0;
        }
        break;
    case WM_LBUTTONDOWN:
        if (st)
        {
            SetFocus(hWnd);
            POINT pt{ (short)LOWORD(lParam), (short)HIWORD(lParam) };
            st->surface.scrollY = st->scrollY;
            st->surface.contentHeight = st->contentHeight;
            if (CustomPageSurface_HandleScrollMessage(hWnd, &st->surface, &st->scroll,
                msg, wParam, lParam, S(hWnd, 44)) != CustomPageScrollResult::NotHandled)
            {
                st->scrollY = st->surface.scrollY;
                return 0;
            }
            MouseCustomItem* hit = MouseCustom_HitTest(st, pt);
            if (hit && hit->enabled)
            {
                st->pressedId = hit->id;
                if (hit->kind == MouseCustomKind::Slider)
                {
                    st->dragId = hit->id;
                    POINT cp = pt;
                    cp.y += st->scrollY;
                    MouseCustom_SetSliderFromPoint(hWnd, st, hit, cp.x);
                }
                SetCapture(hWnd);
                CustomPageSurface_MarkDirty(hWnd, &st->surface);
            }
            return 0;
        }
        break;
    case WM_MOUSEMOVE:
        if (st)
        {
            POINT pt{ (short)LOWORD(lParam), (short)HIWORD(lParam) };
            if (st->scroll.draggingThumb)
            {
                CustomPageSurface_HandleScrollMessage(hWnd, &st->surface, &st->scroll,
                    msg, wParam, lParam, S(hWnd, 44));
                st->scrollY = st->surface.scrollY;
                return 0;
            }
            if (st->dragId)
            {
                MouseCustomItem* it = nullptr;
                for (auto& item : st->items)
                    if (item.id == st->dragId) { it = &item; break; }
                if (it)
                {
                    POINT cp = pt;
                    cp.y += st->scrollY;
                    MouseCustom_SetSliderFromPoint(hWnd, st, it, cp.x);
                }
                return 0;
            }
            MouseCustomItem* hot = MouseCustom_HitTest(st, pt);
            int hotId = hot ? hot->id : 0;
            if (hotId != st->hotId)
            {
                st->hotId = hotId;
                CustomPageSurface_MarkDirty(hWnd, &st->surface);
            }
            return 0;
        }
        break;
    case WM_LBUTTONUP:
        if (st)
        {
            POINT pt{ (short)LOWORD(lParam), (short)HIWORD(lParam) };
            int pressed = st->pressedId;
            bool wasDrag = st->dragId != 0 || st->scroll.draggingThumb;
            st->pressedId = 0;
            st->dragId = 0;
            CustomPageSurface_HandleScrollMessage(hWnd, &st->surface, &st->scroll,
                msg, wParam, lParam, S(hWnd, 44));
            if (GetCapture() == hWnd)
                ReleaseCapture();
            MouseCustomItem* hit = MouseCustom_HitTest(st, pt);
            if (!wasDrag && hit && hit->id == pressed && hit->enabled)
                MouseCustom_Activate(hWnd, st, hit->id);
            else
                CustomPageSurface_MarkDirty(hWnd, &st->surface);
            return 0;
        }
        break;
    case WM_CAPTURECHANGED:
        if (st)
        {
            st->pressedId = 0;
            st->dragId = 0;
            CustomPageSurface_HandleScrollMessage(hWnd, &st->surface, &st->scroll,
                msg, wParam, lParam, S(hWnd, 44));
            CustomPageSurface_MarkDirty(hWnd, &st->surface);
        }
        return 0;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

static void Mouse_Layout(HWND hWnd, MouseSettingsPageState* st)
{
    if (!st) return;
    RECT rc{};
    GetClientRect(hWnd, &rc);

    int margin = S(hWnd, 16);
    int sbReserve = Mouse_ScrollbarWidthPx(hWnd) + Mouse_ScrollbarMarginPx(hWnd) * 2;
    int x = margin;
    int y = margin;
    int chipW = S(hWnd, 86);
    int gap = S(hWnd, 10);
    int comboVisibleH = S(hWnd, 26);
    int sliderH = S(hWnd, 34);
    int chipH = sliderH;
    int labelH = S(hWnd, 18);
    int rowGap = S(hWnd, 18);

    int sliderW = (rc.right - rc.left) - margin * 2 - chipW - gap - sbReserve;
    sliderW = std::max(S(hWnd, 180), sliderW);

    if (st->lblEnable)
        SetWindowPos(st->lblEnable, nullptr, x, y, sliderW + gap + chipW, labelH, SWP_NOZORDER);
    y += labelH + S(hWnd, 6);

    if (st->lblBetaNote)
        SetWindowPos(st->lblBetaNote, nullptr, x, y, sliderW + gap + chipW, S(hWnd, 34), SWP_NOZORDER);
    y += S(hWnd, 34) + S(hWnd, 8);

    if (st->btnEnable)
        SetWindowPos(st->btnEnable, nullptr, x, y, S(hWnd, 220), S(hWnd, 30), SWP_NOZORDER);
    y += S(hWnd, 30) + S(hWnd, 8);

    if (st->btnBlockMouse)
        SetWindowPos(st->btnBlockMouse, nullptr, x, y, S(hWnd, 220), S(hWnd, 30), SWP_NOZORDER);
    y += S(hWnd, 30) + rowGap;

    if (st->lblTarget)
        SetWindowPos(st->lblTarget, nullptr, x, y, sliderW + gap + chipW, labelH, SWP_NOZORDER);
    y += labelH + S(hWnd, 6);

    if (st->cmbTarget)
        SetWindowPos(st->cmbTarget, nullptr, x, y, sliderW + gap + chipW, comboVisibleH, SWP_NOZORDER);
    y += comboVisibleH + rowGap;

    if (st->lblSensitivity)
        SetWindowPos(st->lblSensitivity, nullptr, x, y, sliderW + gap + chipW, labelH, SWP_NOZORDER);
    y += labelH + S(hWnd, 6);

    if (st->sldSensitivity)
        SetWindowPos(st->sldSensitivity, nullptr, x, y, sliderW, sliderH, SWP_NOZORDER);
    if (st->chipSensitivity)
        SetWindowPos(st->chipSensitivity, nullptr, x + sliderW + gap, y, chipW, chipH, SWP_NOZORDER);
    y += sliderH + S(hWnd, 10);

    if (st->lblAggressiveness)
        SetWindowPos(st->lblAggressiveness, nullptr, x, y, sliderW + gap + chipW, labelH, SWP_NOZORDER);
    y += labelH + S(hWnd, 6);

    if (st->sldAggressiveness)
        SetWindowPos(st->sldAggressiveness, nullptr, x, y, sliderW, sliderH, SWP_NOZORDER);
    if (st->chipAggressiveness)
        SetWindowPos(st->chipAggressiveness, nullptr, x + sliderW + gap, y, chipW, chipH, SWP_NOZORDER);
    y += sliderH + S(hWnd, 10);

    if (st->lblMaxOffset)
        SetWindowPos(st->lblMaxOffset, nullptr, x, y, sliderW + gap + chipW, labelH, SWP_NOZORDER);
    y += labelH + S(hWnd, 6);

    if (st->sldMaxOffset)
        SetWindowPos(st->sldMaxOffset, nullptr, x, y, sliderW, sliderH, SWP_NOZORDER);
    if (st->chipMaxOffset)
        SetWindowPos(st->chipMaxOffset, nullptr, x + sliderW + gap, y, chipW, chipH, SWP_NOZORDER);
    y += sliderH + S(hWnd, 10);

    if (st->lblFollowSpeed)
        SetWindowPos(st->lblFollowSpeed, nullptr, x, y, sliderW + gap + chipW, labelH, SWP_NOZORDER);
    y += labelH + S(hWnd, 6);

    if (st->sldFollowSpeed)
        SetWindowPos(st->sldFollowSpeed, nullptr, x, y, sliderW, sliderH, SWP_NOZORDER);
    if (st->chipFollowSpeed)
        SetWindowPos(st->chipFollowSpeed, nullptr, x + sliderW + gap, y, chipW, chipH, SWP_NOZORDER);
    y += sliderH + S(hWnd, 14);

    if (st->lblHint)
        SetWindowPos(st->lblHint, nullptr, x, y, std::max<int>(S(hWnd, 120), (int)(sliderW + gap + chipW)), S(hWnd, 36), SWP_NOZORDER);
    y += S(hWnd, 36) + S(hWnd, 8);

    if (st->lblAsiStatus)
        SetWindowPos(st->lblAsiStatus, nullptr, x, y, std::max<int>(S(hWnd, 120), (int)(sliderW + gap + chipW)), S(hWnd, 20), SWP_NOZORDER);
    y += S(hWnd, 20) + S(hWnd, 8);

    int visW = S(hWnd, MOUSE_VIS_FIXED_W);
    int visH = S(hWnd, MOUSE_VIS_FIXED_H);
    st->visRc = RECT{ x, y, x + visW, y + visH };
    int maxRight = rc.right - margin - sbReserve;
    if (st->visRc.right > maxRight)
        st->visRc.right = std::max<int>((int)st->visRc.left + S(hWnd, 180), maxRight);

    int visPad = S(hWnd, 10);
    int titleH = S(hWnd, 20);
    st->visDynRc = st->visRc;
    st->visDynRc.left += visPad;
    st->visDynRc.right -= visPad;
    st->visDynRc.top += visPad + titleH + S(hWnd, 2);
    st->visDynRc.bottom -= visPad;
    if (st->visDynRc.right < st->visDynRc.left) st->visDynRc.right = st->visDynRc.left;
    if (st->visDynRc.bottom < st->visDynRc.top) st->visDynRc.bottom = st->visDynRc.top;

    st->contentHeight = st->visRc.bottom + margin;
    CustomPageSurface_SetState(&st->surface, st->scrollY, st->contentHeight);
}

LRESULT CALLBACK KeyboardSubpages_MouseSettingsPageProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    return MouseCustom_PageProc(hWnd, msg, wParam, lParam);

    auto* st = (MouseSettingsPageState*)GetWindowLongPtrW(hWnd, GWLP_USERDATA);

    switch (msg)
    {
    case WM_ERASEBKGND:
        return 1;

    case WM_PAINT:
    {
        PAINTSTRUCT ps{};
        HDC memDC = nullptr;
        HBITMAP bmp = nullptr;
        HGDIOBJ oldBmp = nullptr;
        BeginDoubleBufferPaint(hWnd, ps, memDC, bmp, oldBmp);

        SaveDC(memDC);
        if (st && st->scrollY != 0)
            SetViewportOrgEx(memDC, 0, -st->scrollY, nullptr);
        Mouse_DrawVisualization(hWnd, memDC, st);
        RestoreDC(memDC, -1);

        DrawMouseScrollbar(hWnd, memDC, st);
        EndDoubleBufferPaint(hWnd, ps, memDC, bmp, oldBmp);
        return 0;
    }

    case WM_CTLCOLORSTATIC:
    {
        HDC hdc = (HDC)wParam;
        SetBkMode(hdc, TRANSPARENT);
        HWND hCtl = (HWND)lParam;
        if (st && hCtl && ((st->lblHint && hCtl == st->lblHint) ||
            (st->lblBetaNote && hCtl == st->lblBetaNote)))
            SetTextColor(hdc, UiTheme::Color_TextMuted());
        else
            SetTextColor(hdc, UiTheme::Color_Text());
        return (LRESULT)UiTheme::Brush_PanelBg();
    }

    case WM_CREATE:
    {
        HINSTANCE hInst = (HINSTANCE)GetWindowLongPtrW(hWnd, GWLP_HINSTANCE);
        HFONT hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);

        st = new MouseSettingsPageState();
        SetWindowLongPtrW(hWnd, GWLP_USERDATA, (LONG_PTR)st);

        st->lblEnable = CreateWindowW(L"STATIC", L"Mouse to stick",
            WS_CHILD | WS_VISIBLE, 0, 0, 10, 10, hWnd, nullptr, hInst, nullptr);
        SendMessageW(st->lblEnable, WM_SETFONT, (WPARAM)hFont, TRUE);

        st->lblBetaNote = CreateWindowW(
            L"STATIC",
            L"BETA: this feature is experimental, and I am not happy with how it works yet.",
            WS_CHILD | WS_VISIBLE,
            0, 0, 10, 10, hWnd, nullptr, hInst, nullptr);
        SendMessageW(st->lblBetaNote, WM_SETFONT, (WPARAM)hFont, TRUE);

        st->btnEnable = CreateWindowW(L"BUTTON", L"",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
            0, 0, 10, 10, hWnd, (HMENU)(INT_PTR)MOUSE_ID_ENABLE_BUTTON, hInst, nullptr);
        SendMessageW(st->btnEnable, WM_SETFONT, (WPARAM)hFont, TRUE);

        st->btnBlockMouse = CreateWindowW(L"BUTTON", L"",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
            0, 0, 10, 10, hWnd, (HMENU)(INT_PTR)MOUSE_ID_BLOCK_MOUSE_INPUT, hInst, nullptr);
        SendMessageW(st->btnBlockMouse, WM_SETFONT, (WPARAM)hFont, TRUE);

        st->lblTarget = CreateWindowW(L"STATIC", L"Target stick",
            WS_CHILD | WS_VISIBLE, 0, 0, 10, 10, hWnd, nullptr, hInst, nullptr);
        SendMessageW(st->lblTarget, WM_SETFONT, (WPARAM)hFont, TRUE);

        st->cmbTarget = PremiumCombo::Create(hWnd, hInst,
            0, 0, 10, 10, MOUSE_ID_TARGET_COMBO,
            WS_CHILD | WS_VISIBLE | WS_TABSTOP);
        PremiumCombo::SetFont(st->cmbTarget, hFont, true);
        PremiumCombo::AddString(st->cmbTarget, L"Left Stick");
        PremiumCombo::AddString(st->cmbTarget, L"Right Stick");
        PremiumCombo::SetCurSel(st->cmbTarget, std::clamp(Settings_GetMouseToStickTarget(), 0, 1), false);

        st->lblSensitivity = CreateWindowW(L"STATIC", L"Sensitivity",
            WS_CHILD | WS_VISIBLE, 0, 0, 10, 10, hWnd, nullptr, hInst, nullptr);
        SendMessageW(st->lblSensitivity, WM_SETFONT, (WPARAM)hFont, TRUE);

        st->sldSensitivity = PremiumSlider_Create(hWnd, hInst, 0, 0, 10, 10, MOUSE_ID_SENS_SLIDER);
        SendMessageW(st->sldSensitivity, TBM_SETRANGE, TRUE, MAKELONG(10, 800));
        SendMessageW(
            st->sldSensitivity,
            TBM_SETPOS,
            TRUE,
            (LPARAM)std::clamp((int)std::lround(Settings_GetMouseToStickSensitivity() * 100.0f), 10, 800));

        st->chipSensitivity = PremiumChip_Create(hWnd, hInst, 0, 0, 10, 10, MOUSE_ID_SENS_SLIDER + 100);
        SendMessageW(st->chipSensitivity, WM_SETFONT, (WPARAM)hFont, TRUE);

        st->lblAggressiveness = CreateWindowW(L"STATIC", L"Aggressiveness",
            WS_CHILD | WS_VISIBLE, 0, 0, 10, 10, hWnd, nullptr, hInst, nullptr);
        SendMessageW(st->lblAggressiveness, WM_SETFONT, (WPARAM)hFont, TRUE);

        st->sldAggressiveness = PremiumSlider_Create(hWnd, hInst, 0, 0, 10, 10, MOUSE_ID_AGGR_SLIDER);
        SendMessageW(st->sldAggressiveness, TBM_SETRANGE, TRUE, MAKELONG(20, 300));
        SendMessageW(
            st->sldAggressiveness,
            TBM_SETPOS,
            TRUE,
            (LPARAM)std::clamp((int)std::lround(Settings_GetMouseToStickAggressiveness() * 100.0f), 20, 300));

        st->chipAggressiveness = PremiumChip_Create(hWnd, hInst, 0, 0, 10, 10, MOUSE_ID_AGGR_SLIDER + 100);
        SendMessageW(st->chipAggressiveness, WM_SETFONT, (WPARAM)hFont, TRUE);

        st->lblMaxOffset = CreateWindowW(L"STATIC", L"Max offset from center",
            WS_CHILD | WS_VISIBLE, 0, 0, 10, 10, hWnd, nullptr, hInst, nullptr);
        SendMessageW(st->lblMaxOffset, WM_SETFONT, (WPARAM)hFont, TRUE);

        st->sldMaxOffset = PremiumSlider_Create(hWnd, hInst, 0, 0, 10, 10, MOUSE_ID_MAX_OFFSET_SLIDER);
        SendMessageW(st->sldMaxOffset, TBM_SETRANGE, TRUE, MAKELONG(0, 600));
        SendMessageW(
            st->sldMaxOffset,
            TBM_SETPOS,
            TRUE,
            (LPARAM)std::clamp((int)std::lround(Settings_GetMouseToStickMaxOffset() * 100.0f), 0, 600));

        st->chipMaxOffset = PremiumChip_Create(hWnd, hInst, 0, 0, 10, 10, MOUSE_ID_MAX_OFFSET_SLIDER + 100);
        SendMessageW(st->chipMaxOffset, WM_SETFONT, (WPARAM)hFont, TRUE);

        st->lblFollowSpeed = CreateWindowW(L"STATIC", L"Follower speed",
            WS_CHILD | WS_VISIBLE, 0, 0, 10, 10, hWnd, nullptr, hInst, nullptr);
        SendMessageW(st->lblFollowSpeed, WM_SETFONT, (WPARAM)hFont, TRUE);

        st->sldFollowSpeed = PremiumSlider_Create(hWnd, hInst, 0, 0, 10, 10, MOUSE_ID_FOLLOW_SPEED_SLIDER);
        SendMessageW(st->sldFollowSpeed, TBM_SETRANGE, TRUE, MAKELONG(20, 300));
        SendMessageW(
            st->sldFollowSpeed,
            TBM_SETPOS,
            TRUE,
            (LPARAM)std::clamp((int)std::lround(Settings_GetMouseToStickFollowSpeed() * 100.0f), 20, 300));

        st->chipFollowSpeed = PremiumChip_Create(hWnd, hInst, 0, 0, 10, 10, MOUSE_ID_FOLLOW_SPEED_SLIDER + 100);
        SendMessageW(st->chipFollowSpeed, WM_SETFONT, (WPARAM)hFont, TRUE);

        st->lblHint = CreateWindowW(
            L"STATIC",
            L"Maps raw mouse movement to one gamepad stick. Useful for games that block mouse + gamepad together.",
            WS_CHILD | WS_VISIBLE,
            0, 0, 10, 10, hWnd, nullptr, hInst, nullptr);
        SendMessageW(st->lblHint, WM_SETFONT, (WPARAM)hFont, TRUE);

        st->lblAsiStatus = CreateWindowW(
            L"STATIC",
            L"",
            WS_CHILD | WS_VISIBLE,
            0, 0, 10, 10, hWnd, nullptr, hInst, nullptr);
        SendMessageW(st->lblAsiStatus, WM_SETFONT, (WPARAM)hFont, TRUE);

        Mouse_UpdateUi(st);
        Mouse_Layout(hWnd, st);
        SetTimer(hWnd, MOUSE_STATUS_TIMER_ID, 500, nullptr);
        SetTimer(hWnd, MOUSE_VIS_TIMER_ID, std::clamp(Settings_GetUIRefreshMs(), 8u, 33u), nullptr);
        return 0;
    }

    case WM_SIZE:
        if (st)
        {
            int keepScroll = st->scrollY;
            if (keepScroll != 0)
            {
                Mouse_OffsetAllChildren(hWnd, keepScroll);
                st->scrollY = 0;
            }
            Mouse_Layout(hWnd, st);
            Mouse_SetScrollY(hWnd, st, keepScroll);
        }
        else
        {
            Mouse_Layout(hWnd, st);
        }
        return 0;

    case WM_LBUTTONDOWN:
        if (st)
        {
            POINT pt{ (short)LOWORD(lParam), (short)HIWORD(lParam) };
            RECT thumb = Mouse_GetScrollThumbRect(hWnd, st);
            RECT track = Mouse_GetScrollTrackRect(hWnd);
            int maxScroll = Mouse_GetMaxScroll(hWnd, st);

            if (maxScroll > 0 && PtInRect(&thumb, pt))
            {
                st->scrollDrag = true;
                st->scrollDragGrabOffsetY = pt.y - thumb.top;
                st->scrollDragThumbHeight = std::max(1, (int)thumb.bottom - (int)thumb.top);
                st->scrollDragMax = maxScroll;
                SetCapture(hWnd);
                InvalidateRect(hWnd, nullptr, FALSE);
                return 0;
            }

            if (maxScroll > 0 && PtInRect(&track, pt))
            {
                int page = std::max(1, Mouse_GetViewportHeight(hWnd) - S(hWnd, 48));
                if (pt.y < thumb.top)
                    Mouse_SetScrollY(hWnd, st, st->scrollY - page);
                else if (pt.y >= thumb.bottom)
                    Mouse_SetScrollY(hWnd, st, st->scrollY + page);
                return 0;
            }
        }
        break;

    case WM_MOUSEMOVE:
        if (st && st->scrollDrag)
        {
            POINT pt{ (short)LOWORD(lParam), (short)HIWORD(lParam) };
            RECT track = Mouse_GetScrollTrackRect(hWnd);
            int trackH = std::max(1, (int)track.bottom - (int)track.top);
            int thumbH = std::max(1, st->scrollDragThumbHeight);
            int travel = std::max(1, trackH - thumbH);
            int maxScroll = std::max(1, st->scrollDragMax);

            int topWanted = pt.y - st->scrollDragGrabOffsetY;
            int topMin = track.top;
            int topMax = track.bottom - thumbH;
            if (topMax < topMin) topMax = topMin;
            int top = std::clamp(topWanted, topMin, topMax);
            double t = (double)(top - topMin) / (double)travel;
            int target = (int)std::lround(t * (double)maxScroll);
            Mouse_SetScrollY(hWnd, st, target);
            return 0;
        }
        break;

    case WM_LBUTTONUP:
        if (st && st->scrollDrag)
        {
            st->scrollDrag = false;
            if (GetCapture() == hWnd)
                ReleaseCapture();
            InvalidateRect(hWnd, nullptr, FALSE);
            return 0;
        }
        break;

    case WM_CAPTURECHANGED:
        if (st && st->scrollDrag)
        {
            st->scrollDrag = false;
            InvalidateRect(hWnd, nullptr, FALSE);
        }
        return 0;

    case WM_MOUSEWHEEL:
        if (st)
        {
            int delta = GET_WHEEL_DELTA_WPARAM(wParam);
            if (delta != 0)
            {
                int step = S(hWnd, 44);
                int next = st->scrollY - ((delta / WHEEL_DELTA) * step);
                Mouse_SetScrollY(hWnd, st, next);
            }
            return 0;
        }
        break;

    case WM_VSCROLL:
        if (st)
        {
            int page = std::max(1, Mouse_GetViewportHeight(hWnd) - S(hWnd, 48));
            int line = std::max(1, S(hWnd, 40));
            int next = st->scrollY;
            switch (LOWORD(wParam))
            {
            case SB_TOP:           next = 0; break;
            case SB_BOTTOM:        next = Mouse_GetMaxScroll(hWnd, st); break;
            case SB_LINEUP:        next -= line; break;
            case SB_LINEDOWN:      next += line; break;
            case SB_PAGEUP:        next -= page; break;
            case SB_PAGEDOWN:      next += page; break;
            case SB_THUMBPOSITION:
            case SB_THUMBTRACK:    next = HIWORD(wParam); break;
            default: break;
            }
            Mouse_SetScrollY(hWnd, st, next);
            return 0;
        }
        break;

    case WM_HSCROLL:
        if (st && (HWND)lParam == st->sldSensitivity)
        {
            int v = (int)SendMessageW(st->sldSensitivity, TBM_GETPOS, 0, 0);
            v = std::clamp(v, 10, 800);
            Settings_SetMouseToStickSensitivity((float)v / 100.0f);
            GlobalProfiles_SetDirty(true);
            Mouse_MarkGlobalProfileDirty();
            Mouse_UpdateUi(st);
            Mouse_RequestSave(hWnd);
            Mouse_InvalidateDynamicVisual(hWnd, st);
            return 0;
        }
        if (st && (HWND)lParam == st->sldAggressiveness)
        {
            int v = (int)SendMessageW(st->sldAggressiveness, TBM_GETPOS, 0, 0);
            v = std::clamp(v, 20, 300);
            Settings_SetMouseToStickAggressiveness((float)v / 100.0f);
            GlobalProfiles_SetDirty(true);
            Mouse_MarkGlobalProfileDirty();
            Mouse_UpdateUi(st);
            Mouse_RequestSave(hWnd);
            Mouse_InvalidateDynamicVisual(hWnd, st);
            return 0;
        }
        if (st && (HWND)lParam == st->sldMaxOffset)
        {
            int v = (int)SendMessageW(st->sldMaxOffset, TBM_GETPOS, 0, 0);
            v = std::clamp(v, 0, 600);
            Settings_SetMouseToStickMaxOffset((float)v / 100.0f);
            GlobalProfiles_SetDirty(true);
            Mouse_MarkGlobalProfileDirty();
            Mouse_UpdateUi(st);
            Mouse_RequestSave(hWnd);
            Mouse_InvalidateDynamicVisual(hWnd, st);
            return 0;
        }
        if (st && (HWND)lParam == st->sldFollowSpeed)
        {
            int v = (int)SendMessageW(st->sldFollowSpeed, TBM_GETPOS, 0, 0);
            v = std::clamp(v, 20, 300);
            Settings_SetMouseToStickFollowSpeed((float)v / 100.0f);
            GlobalProfiles_SetDirty(true);
            Mouse_MarkGlobalProfileDirty();
            Mouse_UpdateUi(st);
            Mouse_RequestSave(hWnd);
            Mouse_InvalidateDynamicVisual(hWnd, st);
            return 0;
        }
        return 0;

    case WM_TIMER:
        if (st && wParam == MOUSE_STATUS_TIMER_ID)
        {
            Mouse_UpdateUi(st);
            return 0;
        }
        if (st && wParam == MOUSE_VIS_TIMER_ID)
        {
            if (!IsWindowVisible(hWnd))
                return 0;
            Mouse_InvalidateDynamicVisual(hWnd, st);
            return 0;
        }
        return 0;

    case WM_DRAWITEM:
    {
        const DRAWITEMSTRUCT* dis = (const DRAWITEMSTRUCT*)lParam;
        if (st && dis && dis->CtlType == ODT_BUTTON &&
            ((dis->CtlID == MOUSE_ID_ENABLE_BUTTON && st->btnEnable == dis->hwndItem) ||
             (dis->CtlID == MOUSE_ID_BLOCK_MOUSE_INPUT && st->btnBlockMouse == dis->hwndItem)))
        {
            Global_DrawActionButton(dis);
            return TRUE;
        }
        break;
    }

    case WM_COMMAND:
        if (!st) return 0;

        if (LOWORD(wParam) == (UINT)MOUSE_ID_ENABLE_BUTTON && HIWORD(wParam) == BN_CLICKED)
        {
            Settings_SetMouseToStickEnabled(!Settings_GetMouseToStickEnabled());
            GlobalProfiles_SetDirty(true);
            Mouse_MarkGlobalProfileDirty();
            Mouse_UpdateUi(st);
            Mouse_RequestSave(hWnd);
            InvalidateRect(st->btnEnable, nullptr, FALSE);
            return 0;
        }

        if (LOWORD(wParam) == (UINT)MOUSE_ID_BLOCK_MOUSE_INPUT && HIWORD(wParam) == BN_CLICKED)
        {
            Settings_SetBlockMouseInput(!Settings_GetBlockMouseInput());
            GlobalProfiles_SetDirty(true);
            Mouse_MarkGlobalProfileDirty();
            Mouse_UpdateUi(st);
            Mouse_RequestSave(hWnd);
            InvalidateRect(st->btnBlockMouse, nullptr, FALSE);
            return 0;
        }

        if (LOWORD(wParam) == (UINT)MOUSE_ID_TARGET_COMBO && HIWORD(wParam) == CBN_SELCHANGE)
        {
            int sel = PremiumCombo::GetCurSel(st->cmbTarget);
            Settings_SetMouseToStickTarget(std::clamp(sel, 0, 1));
            GlobalProfiles_SetDirty(true);
            Mouse_MarkGlobalProfileDirty();
            Mouse_RequestSave(hWnd);
            return 0;
        }

        return 0;

    case WM_NCDESTROY:
        if (st)
        {
            if (st->scrollDrag && GetCapture() == hWnd)
                ReleaseCapture();
            KillTimer(hWnd, MOUSE_STATUS_TIMER_ID);
            KillTimer(hWnd, MOUSE_VIS_TIMER_ID);
            delete st;
            SetWindowLongPtrW(hWnd, GWLP_USERDATA, 0);
        }
        return 0;
    }

    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

// ============================================================================
// Snappy Joystick Toggle (premium owner-draw)
// ============================================================================
static constexpr const wchar_t* SNAPPY_TOGGLE_ANIM_PROP = L"DD_SnappyToggleAnimPtr";

static KspToggleAnimState* SnappyToggle_Get(HWND hBtn)
{
    return (KspToggleAnimState*)GetPropW(hBtn, SNAPPY_TOGGLE_ANIM_PROP);
}

static void SnappyToggle_Free(HWND hBtn)
{
    if (auto* st = SnappyToggle_Get(hBtn))
    {
        RemovePropW(hBtn, SNAPPY_TOGGLE_ANIM_PROP);
        delete st;
    }
}

static float SnappyClamp01(float v) { return std::clamp(v, 0.0f, 1.0f); }

static bool SnappyToggle_HitTestSwitchOnly(HWND hBtn, POINT ptClient)
{
    RECT rc{};
    GetClientRect(hBtn, &rc);
    int w = rc.right - rc.left;
    int h = rc.bottom - rc.top;
    if (w <= 0 || h <= 0) return false;

    float sw = std::clamp((float)h * 1.55f, 36.0f, 54.0f);
    float sh = std::clamp((float)h * 0.78f, 18.0f, 28.0f);
    float sy = ((float)h - sh) * 0.5f;

    RECT r{};
    r.left = 0;
    r.right = (int)std::lround(sw);
    r.top = (int)std::lround(sy);
    r.bottom = (int)std::lround(sy + sh);

    return (ptClient.x >= r.left && ptClient.x < r.right && ptClient.y >= r.top && ptClient.y < r.bottom);
}

static void SnappyToggle_StartAnim(HWND hBtn, bool checked, bool animate)
{
    auto* st = SnappyToggle_Get(hBtn);
    if (!st)
    {
        st = new KspToggleAnimState();
        SetPropW(hBtn, SNAPPY_TOGGLE_ANIM_PROP, (HANDLE)st);
    }

    float target = checked ? 1.0f : 0.0f;

    if (!st->initialized || !animate)
    {
        st->initialized = true;
        st->checked = checked;
        st->t = target;
        st->from = target;
        st->to = target;
        st->running = false;
        st->startTick = GetTickCount();
        InvalidateRect(hBtn, nullptr, FALSE);
        return;
    }

    st->checked = checked;
    st->from = st->t;
    st->to = target;
    st->startTick = GetTickCount();
    st->durationMs = 140;
    st->running = true;

    SetTimer(hBtn, 1, 15, nullptr);
    InvalidateRect(hBtn, nullptr, FALSE);
}

static void SnappyToggle_Tick(HWND hBtn)
{
    auto* st = SnappyToggle_Get(hBtn);
    if (!st || !st->running) { KillTimer(hBtn, 1); return; }

    DWORD now = GetTickCount();
    DWORD dt = now - st->startTick;
    float x = (st->durationMs > 0) ? (float)dt / (float)st->durationMs : 1.0f;
    x = SnappyClamp01(x);

    // smoothstep
    float s = x * x * (3.0f - 2.0f * x);
    st->t = st->from + (st->to - st->from) * s;

    if (x >= 1.0f - 1e-4f)
    {
        st->t = st->to;
        st->running = false;
        KillTimer(hBtn, 1);
    }

    InvalidateRect(hBtn, nullptr, FALSE);
}

static LRESULT CALLBACK SnappyToggle_SubclassProc(HWND hBtn, UINT msg, WPARAM wParam, LPARAM lParam,
    UINT_PTR, DWORD_PTR)
{
    switch (msg)
    {
    case WM_LBUTTONDOWN:
    case WM_LBUTTONDBLCLK:
    {
        POINT pt{ (short)LOWORD(lParam), (short)HIWORD(lParam) };
        bool onSwitch = SnappyToggle_HitTestSwitchOnly(hBtn, pt);
        SnappyDebugLog(L"WM_LBUTTONDOWN", hBtn, onSwitch ? 1 : 0, (int)wParam);
        if (!onSwitch) { SetFocus(hBtn); return 0; }
        break;
    }

    case WM_SETCURSOR:
    {
        POINT pt{};
        GetCursorPos(&pt);
        ScreenToClient(hBtn, &pt);
        if (SnappyToggle_HitTestSwitchOnly(hBtn, pt)) { SetCursor(LoadCursorW(nullptr, IDC_HAND)); return TRUE; }
        break;
    }

    case WM_TIMER:
        if (wParam == 1) { SnappyToggle_Tick(hBtn); return 0; }
        break;

    case WM_NCDESTROY:
        SnappyDebugLog(L"WM_NCDESTROY", hBtn);
        KillTimer(hBtn, 1);
        SnappyToggle_Free(hBtn);
        RemoveWindowSubclass(hBtn, SnappyToggle_SubclassProc, 1);
        break;
    }
    return DefSubclassProc(hBtn, msg, wParam, lParam);
}

static void DrawSnappyToggleOwnerDraw_Impl(const DRAWITEMSTRUCT* dis)
{
    const bool disabled = (dis->itemState & ODS_DISABLED) != 0;

    bool checked = (SendMessageW(dis->hwndItem, BM_GETCHECK, 0, 0) == BST_CHECKED);

    float t = checked ? 1.0f : 0.0f;
    if (auto* st = SnappyToggle_Get(dis->hwndItem))
        if (st->initialized) t = std::clamp(st->t, 0.0f, 1.0f);

    Gdiplus::Graphics g(dis->hDC);
    g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHighQuality);
    g.SetTextRenderingHint(Gdiplus::TextRenderingHintAntiAliasGridFit);

    Gdiplus::RectF bounds(
        (float)dis->rcItem.left, (float)dis->rcItem.top,
        (float)(dis->rcItem.right - dis->rcItem.left),
        (float)(dis->rcItem.bottom - dis->rcItem.top));

    // background
    {
        Gdiplus::SolidBrush bg(Gp(UiTheme::Color_PanelBg()));
        g.FillRectangle(&bg, bounds);
    }

    float h = bounds.Height;
    float sw = std::clamp(h * 1.55f, 36.0f, 54.0f);
    float sh = std::clamp(h * 0.78f, 18.0f, 28.0f);
    float sx = bounds.X;
    float sy = bounds.Y + (bounds.Height - sh) * 0.5f;

    Gdiplus::RectF track(sx, sy, sw, sh);
    float rr = sh * 0.5f;

    Gdiplus::Color onC = disabled ? Gp(UiTheme::Color_Border()) : Gp(UiTheme::Color_Accent());
    Gdiplus::Color offC = Gp(RGB(70, 70, 70));
    auto lerpC = [&](const Gdiplus::Color& a, const Gdiplus::Color& b, float tt)
        {
            tt = std::clamp(tt, 0.0f, 1.0f);
            auto L = [&](BYTE aa, BYTE bb) -> BYTE { return (BYTE)std::clamp((int)lroundf(aa + (bb - aa) * tt), 0, 255); };
            return Gdiplus::Color(L(a.GetA(), b.GetA()), L(a.GetR(), b.GetR()), L(a.GetG(), b.GetG()), L(a.GetB(), b.GetB()));
        };

    {
        Gdiplus::SolidBrush br(lerpC(offC, onC, t));
        Gdiplus::GraphicsPath p;
        AddRoundRectPath(p, track, rr);
        g.FillPath(&br, &p);
    }

    float thumbD = sh - 4.0f;
    float thumbX0 = track.X + 2.0f;
    float thumbX1 = track.GetRight() - 2.0f - thumbD;
    float thumbX = thumbX0 + (thumbX1 - thumbX0) * t;

    {
        Gdiplus::RectF thumb(thumbX, track.Y + 2.0f, thumbD, thumbD);
        Gdiplus::SolidBrush brThumb(disabled ? Gp(UiTheme::Color_TextMuted()) : Gp(RGB(240, 240, 240)));
        g.FillEllipse(&brThumb, thumb);
    }

    // label
    {
        const wchar_t* label = L"Snap Stick";
        const int ctrlId = GetDlgCtrlID(dis->hwndItem);
        if (ctrlId == ID_LAST_KEY_PRIORITY)
            label = L"Last Key Priority";
        else if (ctrlId == ID_BLOCK_BOUND_KEYS)
            label = L"Block Bound Keys";
        Gdiplus::RectF textR(track.GetRight() + 10.0f, bounds.Y,
            bounds.GetRight() - (track.GetRight() + 10.0f), bounds.Height);

        FontFamily ff(L"Segoe UI");
        Gdiplus::StringFormat fmt;
        fmt.SetFormatFlags(Gdiplus::StringFormatFlagsNoWrap);
        fmt.SetTrimming(Gdiplus::StringTrimmingEllipsisCharacter);
        fmt.SetAlignment(Gdiplus::StringAlignmentNear);
        fmt.SetLineAlignment(Gdiplus::StringAlignmentCenter);

        float em = std::clamp(bounds.Height * 0.46f, 11.0f, 16.0f);
        Gdiplus::Font font(&ff, em, Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);

        Gdiplus::SolidBrush br(disabled ? Gp(UiTheme::Color_TextMuted()) : Gp(UiTheme::Color_Text()));
        g.DrawString(label, -1, &font, textR, &fmt, &br);
    }
}

static void DrawSnappyToggleOwnerDraw(const DRAWITEMSTRUCT* dis)
{
    if (!dis) return;

    RECT rc = dis->rcItem;
    int w = rc.right - rc.left;
    int h = rc.bottom - rc.top;
    if (w <= 2 || h <= 2)
    {
        DrawSnappyToggleOwnerDraw_Impl(dis);
        return;
    }

    HDC memDC = CreateCompatibleDC(dis->hDC);
    if (!memDC)
    {
        DrawSnappyToggleOwnerDraw_Impl(dis);
        return;
    }

    HBITMAP bmp = CreateCompatibleBitmap(dis->hDC, w, h);
    if (!bmp)
    {
        DeleteDC(memDC);
        DrawSnappyToggleOwnerDraw_Impl(dis);
        return;
    }

    HGDIOBJ oldBmp = SelectObject(memDC, bmp);
    DRAWITEMSTRUCT di = *dis;
    di.hDC = memDC;
    di.rcItem = RECT{ 0, 0, w, h };

    DrawSnappyToggleOwnerDraw_Impl(&di);
    BitBlt(dis->hDC, rc.left, rc.top, w, h, memDC, 0, 0, SRCCOPY);

    SelectObject(memDC, oldBmp);
    DeleteObject(bmp);
    DeleteDC(memDC);
}

// ============================================================================
// Config page
// ============================================================================
struct ConfigPageState
{
    HWND cmbSparkPollMode = nullptr;
    HWND cmbSparkRows = nullptr;
    HWND chkSnappy = nullptr;
    HWND chkLastKeyPriority = nullptr;
    HWND lblLastKeyPrioritySensitivity = nullptr;
    HWND sldLastKeyPrioritySensitivity = nullptr;
    HWND chipLastKeyPrioritySensitivity = nullptr;
    HWND chkBlockBoundKeys = nullptr;
    bool blockShortcutCapturing = false;
    std::wstring blockShortcutError;
    HWND btnAnalogSelfTest = nullptr;
    HWND lblAnalogSelfTest = nullptr;

    // status label for presets
    HWND lblProfileStatus = nullptr;

    // --- Delete confirmation (two-click) ---
    int   pendingDeleteIdx = -1;
    DWORD pendingDeleteTick = 0;

    // --- Premium toast (popup hint) ---
    HWND hToast = nullptr;
    std::wstring toastText;
    ULONGLONG toastHideAt = 0;

    // vertical scroll state for Configuration page
    CustomPageSurface surface;
    CustomPageScrollController scroll;
    int scrollY = 0;
    int contentHeight = 0;
    bool customControls = true;
    bool sparkControlsVisible = false;
    int hotCustomId = 0;
    int pressedCustomId = 0;
    int dragCustomId = 0;
    std::wstring profileStatusText;

    // analog self-test state
    bool selfTestRunning = false;
    DWORD selfTestStartedAt = 0;
    uint32_t selfTestStartKeySeq = 0;
    uint16_t selfTestPeakRawMilli = 0;
    uint16_t selfTestPeakOutMilli = 0;
    uint16_t selfTestPeakFullMilli = 0;
    uint16_t selfTestPeakFullDevMilli = 0;
    int selfTestDeviceCount = 0;
    int selfTestMode = 0;
};

static void LayoutConfigControls(HWND hWnd, ConfigPageState* st);
static void Config_OffsetAllChildren(HWND hWnd, int dy);
static void Config_SetScrollY(HWND hWnd, ConfigPageState* st, int newScrollY);
static void Config_SetCustomChildrenVisible(ConfigPageState* st, bool visible);
static void Config_MarkSurfaceDirty(HWND hWnd, ConfigPageState* st);
static bool Config_UpdateSparkCombos(HWND hWnd, ConfigPageState* st);
static void DrawCpWeightHintIfNeeded(HWND hWnd, HDC hdc);

static int Config_ScrollbarWidthPx(HWND hWnd) { return S(hWnd, 12); }
static int Config_ScrollbarMarginPx(HWND hWnd) { return S(hWnd, 8); }

static int Config_LkpSensitivityToSlider(float v01)
{
    // Stored value is retrigger threshold (0.02..0.95), where lower threshold
    // means "more sensitive". UI slider is inverted to show intuitive sensitivity.
    const float lo = 0.02f;
    const float hi = 0.95f;
    float th = std::clamp(v01, lo, hi);
    float t = (hi - th) / (hi - lo); // 0..1
    int pct = 1 + (int)lroundf(t * 99.0f);
    return std::clamp(pct, 1, 100);
}

static float Config_SliderToLkpSensitivity(int sliderPos)
{
    const float lo = 0.02f;
    const float hi = 0.95f;
    int pct = std::clamp(sliderPos, 1, 100);
    float t = (float)(pct - 1) / 99.0f;   // 0..1
    return hi - t * (hi - lo);       // inverted
}

static void Config_UpdateLkpSensitivityUi(ConfigPageState* st)
{
    if (!st) return;

    int sliderPos = Config_LkpSensitivityToSlider(Settings_GetLastKeyPrioritySensitivity());
    if (st->sldLastKeyPrioritySensitivity && IsWindow(st->sldLastKeyPrioritySensitivity))
    {
        int cur = (int)SendMessageW(st->sldLastKeyPrioritySensitivity, TBM_GETPOS, 0, 0);
        if (cur != sliderPos)
            SendMessageW(st->sldLastKeyPrioritySensitivity, TBM_SETPOS, TRUE, (LPARAM)sliderPos);
        EnableWindow(st->sldLastKeyPrioritySensitivity, Settings_GetLastKeyPriority() ? TRUE : FALSE);
    }
    if (st->lblLastKeyPrioritySensitivity && IsWindow(st->lblLastKeyPrioritySensitivity))
    {
        EnableWindow(st->lblLastKeyPrioritySensitivity, Settings_GetLastKeyPriority() ? TRUE : FALSE);
    }

    if (st->chipLastKeyPrioritySensitivity && IsWindow(st->chipLastKeyPrioritySensitivity))
    {
        wchar_t b[32]{};
        swprintf_s(b, L"%d%%", sliderPos);
        SetWindowTextW(st->chipLastKeyPrioritySensitivity, b);
        EnableWindow(st->chipLastKeyPrioritySensitivity, Settings_GetLastKeyPriority() ? TRUE : FALSE);
    }
}

static void Config_RefreshFromCurrentSettings(HWND hWnd, ConfigPageState* st)
{
    if (!st) return;
    if (!Settings_GetBlockBoundKeys()) {
        st->blockShortcutCapturing = false;
        App_SetBlockKeysHotkeyCapture(false);
        st->hotCustomId = st->pressedCustomId = 0;
    }

    if (st->chkSnappy && IsWindow(st->chkSnappy))
    {
        bool on = Settings_GetSnappyJoystick();
        SendMessageW(st->chkSnappy, BM_SETCHECK, on ? BST_CHECKED : BST_UNCHECKED, 0);
        SnappyToggle_StartAnim(st->chkSnappy, on, false);
    }
    if (st->chkLastKeyPriority && IsWindow(st->chkLastKeyPriority))
    {
        bool on = Settings_GetLastKeyPriority();
        SendMessageW(st->chkLastKeyPriority, BM_SETCHECK, on ? BST_CHECKED : BST_UNCHECKED, 0);
        SnappyToggle_StartAnim(st->chkLastKeyPriority, on, false);
    }
    if (st->chkBlockBoundKeys && IsWindow(st->chkBlockBoundKeys))
    {
        bool on = Settings_GetBlockBoundKeys();
        SendMessageW(st->chkBlockBoundKeys, BM_SETCHECK, on ? BST_CHECKED : BST_UNCHECKED, 0);
        SnappyToggle_StartAnim(st->chkBlockBoundKeys, on, false);
    }
    Config_UpdateLkpSensitivityUi(st);

    // Refresh curve preset combo, override/invert/mode toggles and graph state.
    KeySettingsPanel_HandleCommand(hWnd, 9999, 0);
    KeySettingsPanel_SetSelectedHid(KeyboardUI_Internal_GetSelectedHid());
    Config_MarkSurfaceDirty(hWnd, st);
}

static void RequestSave(HWND hWnd)
{
    GlobalProfiles_SetDirty(true);
    if (g_hPageGlobal && IsWindow(g_hPageGlobal))
        PostMessageW(g_hPageGlobal, WM_APP_GLOBAL_PROFILE_DIRTY, 0, 0);

    HWND root = GetAncestor(hWnd, GA_ROOT);
    if (root) PostMessageW(root, WM_APP_REQUEST_SAVE, 0, 0);
}

static void SetProfileStatus(ConfigPageState* st, const wchar_t* text)
{
    if (!st) return;
    st->profileStatusText = text ? text : L"";
    if (st->lblProfileStatus)
        SetWindowTextW(st->lblProfileStatus, text ? text : L"");
}

static const wchar_t* Config_KeycodeModeName(int mode)
{
    switch (mode)
    {
    case 0: return L"HID";
    case 1: return L"ScanCode1";
    case 2: return L"VirtualKey";
    case 3: return L"VirtualKeyTranslate";
    default: return L"Unknown";
    }
}

static void Config_SetSelfTestText(ConfigPageState* st, const wchar_t* text)
{
    if (!st || !st->lblAnalogSelfTest || !IsWindow(st->lblAnalogSelfTest)) return;
    SetWindowTextW(st->lblAnalogSelfTest, text ? text : L"");
}

static void Config_StartSelfTest(HWND hWnd, ConfigPageState* st)
{
    if (!st) return;
    BackendAnalogTelemetry t{};
    Backend_GetAnalogTelemetry(&t);

    st->selfTestRunning = true;
    st->selfTestStartedAt = GetTickCount();
    st->selfTestStartKeySeq = t.keyboardEventSeq;
    st->selfTestPeakRawMilli = 0;
    st->selfTestPeakOutMilli = 0;
    st->selfTestPeakFullMilli = 0;
    st->selfTestPeakFullDevMilli = 0;
    st->selfTestDeviceCount = t.deviceCount;
    st->selfTestMode = t.keycodeMode;

    if (st->btnAnalogSelfTest && IsWindow(st->btnAnalogSelfTest))
    {
        SetWindowTextW(st->btnAnalogSelfTest, L"Stop Self-Test");
        InvalidateRect(st->btnAnalogSelfTest, nullptr, FALSE);
    }
    Config_SetSelfTestText(st, L"Self-test running: press and hold several analog keys for 3 seconds...");
    SetTimer(hWnd, ANALOG_SELF_TEST_TIMER_ID, 80, nullptr);
}

static void Config_FinishSelfTest(HWND hWnd, ConfigPageState* st)
{
    if (!st) return;
    st->selfTestRunning = false;
    KillTimer(hWnd, ANALOG_SELF_TEST_TIMER_ID);
    if (st->btnAnalogSelfTest && IsWindow(st->btnAnalogSelfTest))
    {
        SetWindowTextW(st->btnAnalogSelfTest, L"Run Analog Self-Test");
        InvalidateRect(st->btnAnalogSelfTest, nullptr, FALSE);
    }

    BackendAnalogTelemetry t{};
    Backend_GetAnalogTelemetry(&t);

    uint32_t keyDelta = (t.keyboardEventSeq >= st->selfTestStartKeySeq)
        ? (t.keyboardEventSeq - st->selfTestStartKeySeq)
        : 0;

    uint16_t peakRaw = std::max(st->selfTestPeakRawMilli, t.trackedMaxRawMilli);
    uint16_t peakOut = std::max(st->selfTestPeakOutMilli, t.trackedMaxOutMilli);
    uint16_t peakFull = std::max(st->selfTestPeakFullMilli, t.fullBufferMaxMilli);
    uint16_t peakDev = std::max(st->selfTestPeakFullDevMilli, t.fullBufferDeviceBestMaxMilli);
    uint16_t peakAny = std::max(std::max(peakRaw, peakOut), std::max(peakFull, peakDev));

    wchar_t msg[512]{};
    if (!t.sdkInitialised)
    {
        swprintf_s(msg, L"Self-test: SDK is not initialized.");
    }
    else if (std::max(st->selfTestDeviceCount, t.deviceCount) <= 0)
    {
        swprintf_s(msg, L"Self-test: no analog keyboard detected by SDK.");
    }
    else if (keyDelta == 0)
    {
        swprintf_s(msg, L"Self-test: no key presses detected during test.");
    }
    else if (peakAny <= 2)
    {
        if (t.lastAnalogError < 0)
        {
            swprintf_s(msg,
                L"Self-test: device detected, but analog stream is zero (SDK err %d). Reinstall Universal Analog Plugin + Wooting SDK.",
                t.lastAnalogError);
        }
        else
        {
            swprintf_s(msg,
                L"Self-test: keyboard is detected, but analog stream is zero. Reinstall/repair Universal Analog Plugin + Wooting SDK.");
        }
    }
    else
    {
        swprintf_s(msg,
            L"Self-test OK: analog data detected (peak %.1f%%, mode %s).",
            (double)peakAny / 10.0,
            Config_KeycodeModeName(t.keycodeMode));
    }

    Config_SetSelfTestText(st, msg);
}

static void LayoutConfigControls(HWND hWnd, ConfigPageState* st)
{
    if (!st) return;

    int margin = S(hWnd, 12);
    int totalW = S(hWnd, 416);

    int x = margin;
    int y = S(hWnd, 310);
    int yAfter = y;

    // Snappy toggle
    if (st->chkSnappy)
    {
        int toggleH = S(hWnd, 26);
        SetWindowPos(st->chkSnappy, nullptr, x, yAfter,
            totalW, toggleH, SWP_NOZORDER);

        yAfter += toggleH + S(hWnd, 10);
    }

    if (st->chkLastKeyPriority)
    {
        int toggleH = S(hWnd, 26);
        int gap = S(hWnd, 8);
        int gapAfterToggle = S(hWnd, 14);
        int labelW = S(hWnd, 72);
        int chipW = S(hWnd, 68);
        int sliderW = S(hWnd, 96);
        int rightW = labelW + gap + sliderW + gap + chipW;
        int toggleW = std::max(S(hWnd, 140), totalW - rightW - gapAfterToggle);

        SetWindowPos(st->chkLastKeyPriority, nullptr, x, yAfter,
            toggleW, toggleH, SWP_NOZORDER);

        if (st->lblLastKeyPrioritySensitivity)
            SetWindowPos(st->lblLastKeyPrioritySensitivity, nullptr, x + toggleW + gapAfterToggle, yAfter,
                labelW, toggleH, SWP_NOZORDER);
        if (st->sldLastKeyPrioritySensitivity)
            SetWindowPos(st->sldLastKeyPrioritySensitivity, nullptr, x + toggleW + gapAfterToggle + labelW + gap, yAfter,
                sliderW, toggleH, SWP_NOZORDER);
        if (st->chipLastKeyPrioritySensitivity)
            SetWindowPos(st->chipLastKeyPrioritySensitivity, nullptr, x + toggleW + gapAfterToggle + labelW + gap + sliderW + gap, yAfter,
                chipW, toggleH, SWP_NOZORDER);

        yAfter += toggleH + S(hWnd, 10);
    }

    if (st->chkBlockBoundKeys)
    {
        int toggleH = S(hWnd, 26);
        SetWindowPos(st->chkBlockBoundKeys, nullptr, x, yAfter,
            totalW, toggleH, SWP_NOZORDER);

        yAfter += toggleH + S(hWnd, 10);
    }

    if (st->btnAnalogSelfTest)
    {
        int bh = S(hWnd, 28);
        int bw = S(hWnd, 220);
        SetWindowPos(st->btnAnalogSelfTest, nullptr, x, yAfter,
            bw, bh, SWP_NOZORDER);
        yAfter += bh + S(hWnd, 8);
    }

    if (st->lblAnalogSelfTest)
    {
        int lh = S(hWnd, 34);
        SetWindowPos(st->lblAnalogSelfTest, nullptr, x, yAfter,
            totalW, lh, SWP_NOZORDER);
        yAfter += lh + S(hWnd, 8);
    }

    if (st->lblProfileStatus)
    {
        SetWindowPos(st->lblProfileStatus, nullptr, x, yAfter,
            (int)std::max(10, (int)totalW), S(hWnd, 18), SWP_NOZORDER);
    }
}

static RECT Config_Rect(int x, int y, int w, int h)
{
    return RECT{ x, y, x + w, y + h };
}

static RECT Config_ToViewRect(RECT rc, ConfigPageState* st)
{
    if (st && st->scrollY != 0)
        OffsetRect(&rc, 0, -st->scrollY);
    return rc;
}

static RECT Config_CustomToggleRect(HWND hWnd, int ordinal)
{
    int x = S(hWnd, 12);
    int y = S(hWnd, 310);
    int h = S(hWnd, 26);
    int gap = S(hWnd, 10);
    ordinal = std::max(0, ordinal);
    int rowY = y + (h + gap) * ordinal;
    int rowW = (ordinal == 1) ? S(hWnd, 180) : S(hWnd, 416);
    return Config_Rect(x, rowY, rowW, h);
}

static RECT Config_CustomLkpLabelRect(HWND hWnd)
{
    RECT row = Config_CustomToggleRect(hWnd, 1);
    return Config_Rect(row.right + S(hWnd, 14), row.top, S(hWnd, 72), row.bottom - row.top);
}

static RECT Config_CustomLkpSliderRect(HWND hWnd)
{
    RECT label = Config_CustomLkpLabelRect(hWnd);
    return Config_Rect(label.right + S(hWnd, 8), label.top, S(hWnd, 96), label.bottom - label.top);
}

static RECT Config_CustomLkpChipRect(HWND hWnd)
{
    RECT slider = Config_CustomLkpSliderRect(hWnd);
    return Config_Rect(slider.right + S(hWnd, 8), slider.top, S(hWnd, 68), slider.bottom - slider.top);
}

static RECT Config_BlockAllowRect(HWND hWnd)
{
    const auto row = Config_CustomToggleRect(hWnd, 2);
    return Config_Rect(S(hWnd, 36), row.bottom + S(hWnd, 12), S(hWnd, 440), S(hWnd, 26));
}

static RECT Config_BlockShortcutRect(HWND hWnd)
{
    const auto row = Config_BlockAllowRect(hWnd);
    return Config_Rect(S(hWnd, 154), row.bottom + S(hWnd, 8), S(hWnd, 210), S(hWnd, 30));
}

static RECT Config_BlockShortcutClearRect(HWND hWnd)
{
    const auto row = Config_BlockShortcutRect(hWnd);
    return Config_Rect(row.right + S(hWnd, 8), row.top, S(hWnd, 70), row.bottom - row.top);
}

static int Config_BlockOptionsSpace(HWND hWnd)
{
    if (!Settings_GetBlockBoundKeys()) return 0;
    auto* st = reinterpret_cast<ConfigPageState*>(GetWindowLongPtrW(hWnd, GWLP_USERDATA));
    const bool error = (st && !st->blockShortcutError.empty()) || App_BlockKeysHotkeyError() != 0;
    return S(hWnd, 88) + (error ? S(hWnd, 34) : 0);
}

static std::wstring Config_BlockShortcutText()
{
    const UINT chord = Settings_GetBlockKeysHotkey();
    if (!chord) return L"Set shortcut";
    std::wstring text;
    const UINT mods = chord >> 8;
    if (mods & MOD_CONTROL) text += L"Ctrl + ";
    if (mods & MOD_ALT) text += L"Alt + ";
    if (mods & MOD_SHIFT) text += L"Shift + ";
    if (mods & MOD_WIN) text += L"Win + ";
    const UINT scan = MapVirtualKeyW(chord & 255, MAPVK_VK_TO_VSC_EX);
    wchar_t key[64]{};
    LONG flags = (scan & 255) << 16;
    if ((scan & 0xff00) == 0xe000) flags |= 1 << 24;
    if (!GetKeyNameTextW(flags, key, _countof(key))) swprintf_s(key, L"Key %u", chord & 255);
    return text + key;
}

static RECT Config_PrivilegeWarningRect(HWND hWnd)
{
    RECT client{};
    GetClientRect(hWnd, &client);
    RECT rc = Config_CustomToggleRect(hWnd, 2);
    rc.top = rc.bottom + Config_BlockOptionsSpace(hWnd) + S(hWnd, 6);
    rc.left = S(hWnd, 12);
    rc.right = (std::max)(rc.left + S(hWnd, 80), client.right - S(hWnd, 28));
    // Measure only when the wrapping width/DPI changes, not on graph repaints.
    static int measuredWidth = -1, measuredScale = -1, measuredHeight = 0;
    const int width = rc.right - rc.left, scale = S(hWnd, 18);
    if (width != measuredWidth || scale != measuredScale) {
        HDC dc = GetDC(hWnd);
        if (!dc) { rc.bottom = rc.top + S(hWnd, 72); return rc; }
        HGDIOBJ old = SelectObject(dc, GetStockObject(SYSTEM_FONT));
        RECT measured = rc;
        DrawTextW(dc, halljoy::input_privilege::kText, -1, &measured, DT_LEFT | DT_WORDBREAK | DT_NOPREFIX | DT_CALCRECT);
        SelectObject(dc, old);
        ReleaseDC(hWnd, dc);
        measuredWidth = width; measuredScale = scale;
        measuredHeight = (std::max)(scale, static_cast<int>(measured.bottom - measured.top));
    }
    rc.bottom = rc.top + measuredHeight;
    return rc;
}

static int Config_PrivilegeWarningSpace(HWND hWnd)
{
    if (!halljoy::input_privilege::detector.warning) return 0;
    const RECT rc = Config_PrivilegeWarningRect(hWnd);
    return rc.bottom - rc.top + S(hWnd, 12);
}

static RECT Config_CustomSparkModeLabelRect(HWND hWnd)
{
    int y = Config_CustomToggleRect(hWnd, 2).bottom + S(hWnd, 10) + Config_BlockOptionsSpace(hWnd) + Config_PrivilegeWarningSpace(hWnd);
    return Config_Rect(S(hWnd, 12), y, S(hWnd, 112), S(hWnd, 28));
}

static RECT Config_CustomSparkModeRect(HWND hWnd)
{
    RECT label = Config_CustomSparkModeLabelRect(hWnd);
    return Config_Rect(label.right + S(hWnd, 8), label.top, S(hWnd, 188), label.bottom - label.top);
}

static RECT Config_CustomSparkRowsLabelRect(HWND hWnd)
{
    RECT mode = Config_CustomSparkModeRect(hWnd);
    return Config_Rect(mode.right + S(hWnd, 16), mode.top, S(hWnd, 72), mode.bottom - mode.top);
}

static RECT Config_CustomSparkRowsRect(HWND hWnd)
{
    RECT label = Config_CustomSparkRowsLabelRect(hWnd);
    return Config_Rect(label.right + S(hWnd, 8), label.top, S(hWnd, 116), label.bottom - label.top);
}

static std::wstring Config_Utf8ToWide(const char* text)
{
    if (!text || !*text)
        return L"";
    const int needed = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text, -1, nullptr, 0);
    if (needed <= 1)
    {
        std::wstring fallback;
        while (*text)
            fallback.push_back((wchar_t)(unsigned char)*text++);
        return fallback;
    }
    std::wstring result((size_t)needed, L'\0');
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text, -1, result.data(), needed);
    result.resize((size_t)needed - 1u);
    return result;
}

static std::vector<std::wstring> BuildAnalogDiagnosticsLines(
    const BackendAnalogTelemetry& t)
{
    std::vector<std::wstring> lines;
    wchar_t line[768]{};

    if (halljoy::keyboard_support::GetStatusSnapshot().analogSourceConnected)
        lines.emplace_back(L"Analog keyboard: connected and visible to HallJoy.");
    else
        lines.emplace_back(L"Analog keyboard: not currently detected. If you have an analog keyboard, ask for support in Discord.");

    if (t.mad68Present)
    {
        const wchar_t* state = t.mad68Connected
            ? (t.mad68Full ? L"full native" : (t.mad68EmergencyWasd ? L"validating (W/A/S/D safety publication)" : L"connected, validating"))
            : L"detected, waiting for native stream";
        _snwprintf_s(line, _countof(line), _TRUNCATE,
            L"MADLIONS native A0: VID 373B / PID %04X | firmware %04X | %ls | coverage %u/68 | published %u",
            (unsigned)t.mad68ProductId, (unsigned)t.mad68FirmwareVersion, state,
            (unsigned)t.mad68Coverage, (unsigned)t.mad68PublishedKeys);
        lines.emplace_back(line);
        _snwprintf_s(line, _countof(line), _TRUNCATE,
            L"  Analog scale: device raw 0..1600 | HallJoy normalized output %u levels | native A0 stream",
            (unsigned)t.analogOutputLevels);
        lines.emplace_back(line);
    }

    if (t.hex80Present)
    {
        const wchar_t* state = t.hex80Connected ? L"native polling active" : L"detected, waiting for validated polling";
        _snwprintf_s(line, _countof(line), _TRUNCATE,
            L"ATK x QK Hex80: VID %04X / PID %04X | firmware %04X | %ls | mapped %u/104 slots",
            (unsigned)t.hex80VendorId, (unsigned)t.hex80ProductId,
            (unsigned)t.hex80FirmwareVersion, state, (unsigned)t.hex80MappedKeys);
        lines.emplace_back(line);
        if (t.hex80Connected)
        {
            _snwprintf_s(line, _countof(line), _TRUNCATE,
                L"  Native 0x96 polling: chunks %.1f Hz | full matrix %.1f Hz | matrix interval avg/max %u/%u us",
                (double)t.hex80ChunkHz10 / 10.0, (double)t.hex80MatrixHz10 / 10.0,
                (unsigned)t.hex80AvgMatrixIntervalUs, (unsigned)t.hex80MaxMatrixIntervalUs);
            lines.emplace_back(line);
            _snwprintf_s(line, _countof(line), _TRUNCATE,
                L"  USB transaction avg/max %u/%u us | packet age %u ms | polls ok/fail/total %llu/%llu/%llu",
                (unsigned)t.hex80AvgTransactionUs, (unsigned)t.hex80MaxTransactionUs,
                (unsigned)t.hex80LastPacketAgeMs,
                (unsigned long long)t.hex80PollSuccess,
                (unsigned long long)t.hex80PollFail,
                (unsigned long long)t.hex80PollAttempts);
            lines.emplace_back(line);
            _snwprintf_s(line, _countof(line), _TRUNCATE,
                L"  Analog scale: travel 0..%u (%u nominal levels) -> HallJoy %u levels | observed %u keys | active %u",
                (unsigned)t.hex80TravelMax, (unsigned)t.hex80TravelMax + 1u,
                (unsigned)t.analogOutputLevels, (unsigned)t.hex80ObservedKeys,
                (unsigned)t.hex80ActiveKeys);
            lines.emplace_back(line);
            _snwprintf_s(line, _countof(line), _TRUNCATE,
                L"  HID reports in/out %u/%u bytes | matrix cycles %llu | protocol reads 4 slots/request",
                (unsigned)t.hex80InputReportBytes, (unsigned)t.hex80OutputReportBytes,
                (unsigned long long)t.hex80MatrixCycles);
            lines.emplace_back(line);
        }
    }

    if (t.addressedPresent)
    {
        const wchar_t* state = t.addressedConnected ? L"native polling active" : L"validated, waiting for fresh responses";
        _snwprintf_s(line, _countof(line), _TRUNCATE,
            L"Addressed Analog 09/94/02: VID %04X / PID %04X | %ls | mapped %u keys | active %u",
            (unsigned)t.addressedVendorId, (unsigned)t.addressedProductId, state,
            (unsigned)t.addressedMappedKeys, (unsigned)t.addressedActiveKeys);
        lines.emplace_back(line);
        _snwprintf_s(line, _countof(line), _TRUNCATE,
            L"  FF60:0061 addressed polling: ok/fail/total %llu/%llu/%llu | response age %u ms | HID in/out %u/%u bytes",
            (unsigned long long)t.addressedPollSuccess,
            (unsigned long long)t.addressedPollFail,
            (unsigned long long)t.addressedPollAttempts,
            (unsigned)t.addressedLastResponseAgeMs,
            (unsigned)t.addressedInputReportBytes,
            (unsigned)t.addressedOutputReportBytes);
        lines.emplace_back(line);
        _snwprintf_s(line, _countof(line), _TRUNCATE,
            L"  Up to 9 requested keys per packet | dynamic 0x83 map with QBZ-compatible fallback | HallJoy output %u levels",
            (unsigned)t.analogOutputLevels);
        lines.emplace_back(line);
    }

    if (t.sparkConnected)
    {
        _snwprintf_s(line, _countof(line), _TRUNCATE,
            L"SparkLink: VID %04X / PID %04X | rows %d/%d | route %.1f Hz | matrix %.1f Hz",
            (unsigned)t.sparkVendorId, (unsigned)t.sparkProductId,
            t.sparkActiveRows, t.sparkRows,
            (double)t.sparkRouteHz10 / 10.0, (double)t.sparkMatrixHz10 / 10.0);
        lines.emplace_back(line);
        _snwprintf_s(line, _countof(line), _TRUNCATE,
            L"HE data: avg interval %u us | max %u us | last age %u ms | max row age %u ms | ok %u / fail %u",
            (unsigned)t.sparkAvgIntervalUs, (unsigned)t.sparkMaxIntervalUs,
            (unsigned)t.sparkLastRouteAgeMs, (unsigned)t.sparkMaxRowAgeMs,
            (unsigned)t.sparkRouteOk, (unsigned)t.sparkRouteFail);
        lines.emplace_back(line);
        _snwprintf_s(line, _countof(line), _TRUNCATE,
            L"Last route: row %u, %s | HallJoy polling interval: %u ms",
            (unsigned)t.sparkLastRouteRow, t.sparkLastRouteOk ? L"ok" : L"failed",
            (unsigned)Settings_GetPollingMs());
        lines.emplace_back(line);
        _snwprintf_s(line, _countof(line), _TRUNCATE,
            L"SparkLink debug: tx avg/max %u/%u us | HE mode %u | row limit %u",
            (unsigned)t.sparkAvgRouteTxUs, (unsigned)t.sparkMaxRouteTxUs,
            (unsigned)t.sparkPollMode, (unsigned)t.sparkRowLimit);
        lines.emplace_back(line);
    }

    // Generic catalog output makes a newly registered protocol visible in
    // Configuration without adding another device-specific UI branch.
    for (int i = 0; i < t.nativeProtocolCount && i < kBackendMaxNativeProtocols; ++i)
    {
        const auto& native = t.nativeProtocols[i];
        const bool knownDetailed = std::strcmp(native.id, "mad68-a0") == 0 ||
            std::strcmp(native.id, "hex80-0x96") == 0 ||
            std::strcmp(native.id, "addressed-099402") == 0 ||
            std::strcmp(native.id, "sparklink") == 0 ||
            std::strcmp(native.id, "sayo-depth") == 0;
        if (knownDetailed || (!native.present && !native.connected))
            continue;
        _snwprintf_s(line, _countof(line), _TRUNCATE,
            L"Native protocol %S: %ls | VID/PID %04X:%04X | %ls | mapped %u | active %u | %.1f Hz",
            native.id, native.name,
            (unsigned)native.vendorId, (unsigned)native.productId,
            native.connected ? L"connected" : L"detected",
            (unsigned)native.mappedKeys, (unsigned)native.activeKeys,
            (double)native.updateHz10 / 10.0);
        lines.emplace_back(line);
        _snwprintf_s(line, _countof(line), _TRUNCATE,
            L"  %ls | usage %04X:%04X | HID in/out %u/%u | update age %u ms",
            native.status,
            (unsigned)native.usagePage, (unsigned)native.usage,
            (unsigned)native.inputReportBytes, (unsigned)native.outputReportBytes,
            (unsigned)native.lastUpdateAgeMs);
        lines.emplace_back(line);
    }

    const auto isNativeDuplicate = [&t](const BackendAnalogDeviceTelemetry& d) {
        for (int i = 0; i < t.nativeProtocolCount && i < kBackendMaxNativeProtocols; ++i)
        {
            const auto& native = t.nativeProtocols[i];
            if (native.connected && native.vendorId == d.vendorId && native.productId == d.productId)
                return true;
        }
        return false;
    };
    bool hasVisiblePluginDevice = false;
    for (int i = 0; i < t.pluginDeviceCount && i < kBackendMaxAnalogDevices; ++i)
    {
        if (t.pluginDevices[i].present && !isNativeDuplicate(t.pluginDevices[i]))
        {
            hasVisiblePluginDevice = true;
            break;
        }
    }

    if (t.pluginHostAvailable && hasVisiblePluginDevice)
    {
        _snwprintf_s(line, _countof(line), _TRUNCATE,
            L"Analog host: %ls | event-driven snapshot reads %.1f Hz | successful %.1f Hz | publish age %u ms | HallJoy realtime %u ms",
            t.pluginHostReady ? L"ready" : L"not ready",
            (double)t.pluginHostPollHz10 / 10.0,
            (double)t.pluginHostSuccessfulPollHz10 / 10.0,
            (unsigned)t.pluginHostLastPublishAgeMs,
            (unsigned)Settings_GetPollingMs());
        lines.emplace_back(line);
        _snwprintf_s(line, _countof(line), _TRUNCATE,
            L"  Host counters: successful/total %llu/%llu | restarts %d | invalid snapshots %d",
            (unsigned long long)t.pluginHostSuccessfulPolls,
            (unsigned long long)t.pluginHostTotalPolls,
            t.pluginHostRestartCount,
            t.pluginHostInvalidSnapshots);
        lines.emplace_back(line);
        _snwprintf_s(line, _countof(line), _TRUNCATE,
            L"  Host errors: plugin %d | transport %d | status %d",
            t.pluginHostLastError, t.pluginHostTransportError, t.pluginHostStatus);
        lines.emplace_back(line);

        for (int i = 0; i < t.pluginDeviceCount && i < kBackendMaxAnalogDevices; ++i)
        {
            const auto& d = t.pluginDevices[i];
            if (!d.present)
                continue;
            if (isNativeDuplicate(d))
                continue;
            std::wstring deviceName = Config_Utf8ToWide(d.name);
            const std::wstring manufacturer = Config_Utf8ToWide(d.manufacturer);
            if (deviceName.empty()) deviceName = manufacturer;
            if (deviceName.empty()) deviceName = L"Analog keyboard";

            const wchar_t* transport = L"unknown transport";
            const wchar_t* rateLabel = L"device updates";
            if ((d.flags & BackendAnalogDeviceFlag_SynchronousHallJoyPoll) != 0)
            {
                transport = L"synchronous HID polling";
                rateLabel = L"completed polls";
            }
            else if ((d.flags & BackendAnalogDeviceFlag_PolledTransport) != 0)
            {
                if ((d.flags & BackendAnalogDeviceFlag_DeadlinePacedWorker) != 0)
                    transport = L"deadline-paced background HID polling";
                else if ((d.flags & BackendAnalogDeviceFlag_UnthrottledWorker) != 0)
                    transport = L"unthrottled background HID polling (diagnostic)";
                else
                    transport = L"background HID polling";
                rateLabel = L"completed polls";
            }
            else if ((d.flags & BackendAnalogDeviceFlag_StreamTransport) != 0)
            {
                transport = L"HID report stream";
                rateLabel = L"received reports";
            }

            if (d.usage != 0)
            {
                _snwprintf_s(line, _countof(line), _TRUNCATE,
                    L"%ls: VID %04X / PID %04X | usage %04X:%04X | %ls | %ls",
                    deviceName.c_str(), (unsigned)d.vendorId, (unsigned)d.productId,
                    (unsigned)d.usagePage, (unsigned)d.usage, transport,
                    d.bluetooth ? L"Bluetooth" : L"USB");
            }
            else
            {
                _snwprintf_s(line, _countof(line), _TRUNCATE,
                    L"%ls: VID %04X / PID %04X | usage page %04X | %ls | %ls",
                    deviceName.c_str(), (unsigned)d.vendorId, (unsigned)d.productId,
                    (unsigned)d.usagePage, transport, d.bluetooth ? L"Bluetooth" : L"USB");
            }
            lines.emplace_back(line);
            _snwprintf_s(line, _countof(line), _TRUNCATE,
                L"  Device ID %016llX | identity %ls",
                (unsigned long long)d.deviceId,
                (d.flags & BackendAnalogDeviceFlag_DuplicateSafeId) != 0
                    ? L"stable HID interface path"
                    : L"enumeration fallback (path unavailable)");
            lines.emplace_back(line);
            _snwprintf_s(line, _countof(line), _TRUNCATE,
                L"  Measured %ls %.1f Hz | interval avg/max %u/%u us | age %u ms | updates %llu | active keys %u",
                rateLabel, (double)d.updateHz10 / 10.0,
                (unsigned)d.averageUpdateIntervalUs, (unsigned)d.maximumUpdateIntervalUs,
                (unsigned)d.lastUpdateAgeMs, (unsigned long long)d.updateCount,
                (unsigned)d.activeKeys);
            lines.emplace_back(line);

            if (d.rows != 0 && d.columns != 0)
            {
                _snwprintf_s(line, _countof(line), _TRUNCATE,
                    L"  Parser layout %ux%u (%u slots) | per-row polling unavailable; one aggregate backend rate is measured",
                    (unsigned)d.rows, (unsigned)d.columns,
                    (unsigned)(d.layoutKeySlots != 0 ? d.layoutKeySlots : d.rows * d.columns));
            }
            else if (d.layoutKeySlots != 0)
            {
                _snwprintf_s(line, _countof(line), _TRUNCATE,
                    L"  Parser layout %u slots | row/column topology and per-row polling unavailable",
                    (unsigned)d.layoutKeySlots);
            }
            else
            {
                wcscpy_s(line, L"  Matrix topology and per-row polling unavailable from this device protocol");
            }
            lines.emplace_back(line);

            if (d.nominalRawLevels != 0)
            {
                _snwprintf_s(line, _countof(line), _TRUNCATE,
                    L"  Analog scale: parser %u levels/key | HallJoy normalized output %u levels",
                    (unsigned)d.nominalRawLevels, (unsigned)t.analogOutputLevels);
            }
            else
            {
                _snwprintf_s(line, _countof(line), _TRUNCATE,
                    L"  Analog scale: parser does not expose source resolution | HallJoy normalized output %u levels",
                    (unsigned)t.analogOutputLevels);
            }
            lines.emplace_back(line);

            _snwprintf_s(line, _countof(line), _TRUNCATE,
                L"  HID reports in/out/feature %u/%u/%u bytes | distinct plugin output values observed %u",
                (unsigned)d.inputReportBytes, (unsigned)d.outputReportBytes,
                (unsigned)d.featureReportBytes, (unsigned)d.observedDistinctLevels);
            lines.emplace_back(line);

            if (d.observedKeys != 0)
            {
                _snwprintf_s(line, _countof(line), _TRUNCATE,
                    L"  Observed positions/key min/avg/max %u/%.1f/%u across %u keys (session, 12-bit measurement)",
                    (unsigned)d.observedLevelsPerKeyMin,
                    (double)d.observedLevelsPerKeyAverage10 / 10.0,
                    (unsigned)d.observedLevelsPerKeyMax,
                    (unsigned)d.observedKeys);
            }
            else
            {
                wcscpy_s(line, L"  Observed positions/key: move several keys through full travel to collect session statistics");
            }
            lines.emplace_back(line);
        }
    }

    if (t.sayoConnected)
    {
        _snwprintf_s(line, _countof(line), _TRUNCATE,
            L"SayoDevice: VID %04X / PID %04X | readers %d | writable %u",
            (unsigned)t.sayoVendorId, (unsigned)t.sayoProductId,
            t.sayoReaders, (unsigned)t.sayoWriteCapableReaders);
        lines.emplace_back(line);
        _snwprintf_s(line, _countof(line), _TRUNCATE,
            L"  Measured completed depth responses %.1f Hz | interval avg/max %u/%u us | depth/report age %u/%u ms",
            (double)t.sayoDepthHz10 / 10.0,
            (unsigned)t.sayoAvgDepthIntervalUs, (unsigned)t.sayoMaxDepthIntervalUs,
            (unsigned)t.sayoLastDepthAgeMs, (unsigned)t.sayoLastPacketAgeMs);
        lines.emplace_back(line);
        lines.emplace_back(L"  Matrix topology and per-row polling unavailable from the Sayo O3C protocol");
        if (t.sayoObservedKeys != 0)
        {
            _snwprintf_s(line, _countof(line), _TRUNCATE,
                L"  Analog scale: source %u levels/key | observed exact positions/key %u/%.1f/%u across %u keys | output %u",
                (unsigned)t.sayoDepthRawLevels,
                (unsigned)t.sayoObservedPositionsMin,
                (double)t.sayoObservedPositionsAverage10 / 10.0,
                (unsigned)t.sayoObservedPositionsMax,
                (unsigned)t.sayoObservedKeys,
                (unsigned)t.analogOutputLevels);
        }
        else
        {
            _snwprintf_s(line, _countof(line), _TRUNCATE,
                L"  Analog scale: source %u levels/key | move keys to measure positions | mapped keys %u | output %u",
                (unsigned)t.sayoDepthRawLevels, (unsigned)t.sayoMappedKeys,
                (unsigned)t.analogOutputLevels);
        }
        lines.emplace_back(line);
        _snwprintf_s(line, _countof(line), _TRUNCATE,
            L"  Poll writes ok/fail/total %llu/%llu/%llu | depth packets %llu | HID reports in/out %u/%u bytes",
            (unsigned long long)t.sayoPollSuccess,
            (unsigned long long)t.sayoPollFail,
            (unsigned long long)t.sayoPollAttempts,
            (unsigned long long)t.sayoDepthPackets,
            (unsigned)t.sayoInputReportBytes, (unsigned)t.sayoOutputReportBytes);
        lines.emplace_back(line);
    }

    if (lines.empty())
        lines.emplace_back(L"Analog input: no analog source connected");
    return lines;
}

static RECT Config_CustomStatusRect(HWND hWnd, ConfigPageState* st)
{
    int y = Config_CustomToggleRect(hWnd, 2).bottom + S(hWnd, 10) + Config_BlockOptionsSpace(hWnd) + Config_PrivilegeWarningSpace(hWnd);
    BackendAnalogTelemetry t{};
    Backend_GetAnalogTelemetry(&t);
    if (t.sparkConnected)
        y += S(hWnd, 28) + S(hWnd, 10);
    return Config_Rect(S(hWnd, 12), y, S(hWnd, 720), S(hWnd, 18));
}

static void Config_DrawCustomToggle(HWND hWnd, HDC hdc, Gdiplus::Graphics& g, const RECT& rc, const std::wstring& text, bool checked, bool enabled)
{
    CustomPage_DrawCheckbox(g, hdc, hWnd, rc, text, checked, enabled);
}

static void Config_DrawCustomCombo(HWND hWnd, HDC hdc, Gdiplus::Graphics& g,
    HWND combo, const RECT& rc, const std::wstring& text, bool enabled)
{
    if (combo)
    {
        PremiumCombo::PaintRetainedFace(combo, hdc, rc, false);
        return;
    }
    CustomPage_DrawRoundRect(g, rc, UiTheme::Color_ControlBg(), enabled ? UiTheme::Color_Border() : RGB(52, 52, 52), 4.0f, enabled ? 255 : 150);
    RECT trc = rc;
    trc.left += S(hWnd, 8);
    trc.right -= S(hWnd, 26);
    CustomPage_DrawText(hdc, text, trc, enabled ? UiTheme::Color_Text() : UiTheme::Color_TextMuted(), DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    int cx = rc.right - S(hWnd, 14);
    int cy = (rc.top + rc.bottom) / 2;
    POINT pts[3]{ { cx - S(hWnd, 4), cy - S(hWnd, 2) }, { cx, cy + S(hWnd, 2) }, { cx + S(hWnd, 4), cy - S(hWnd, 2) } };
    HPEN pen = CreatePen(PS_SOLID, S(hWnd, 2), enabled ? UiTheme::Color_TextMuted() : RGB(80, 80, 80));
    HGDIOBJ old = SelectObject(hdc, pen);
    Polyline(hdc, pts, 3);
    SelectObject(hdc, old);
    DeleteObject(pen);
}

static void Config_DrawCustomControls(HWND hWnd, HDC hdc, ConfigPageState* st)
{
    if (!st || !st->customControls) return;
    Gdiplus::Graphics g(hdc);
    g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHighQuality);

    Config_DrawCustomToggle(hWnd, hdc, g, Config_ToViewRect(Config_CustomToggleRect(hWnd, 0), st), L"Snap Stick", Settings_GetSnappyJoystick(), true);
    Config_DrawCustomToggle(hWnd, hdc, g, Config_ToViewRect(Config_CustomToggleRect(hWnd, 1), st), L"Last Key Priority", Settings_GetLastKeyPriority(), true);

    RECT lr = Config_ToViewRect(Config_CustomLkpLabelRect(hWnd), st);
    CustomPage_DrawText(hdc, L"Sensivity", lr, Settings_GetLastKeyPriority() ? UiTheme::Color_Text() : UiTheme::Color_TextMuted(), DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    int lkp = Config_LkpSensitivityToSlider(Settings_GetLastKeyPrioritySensitivity());
    CustomPage_DrawSlider(g, hWnd, Config_ToViewRect(Config_CustomLkpSliderRect(hWnd), st), 1, 100, lkp);
    wchar_t chip[32]{};
    swprintf_s(chip, L"%d%%", lkp);
    CustomPage_DrawChip(g, hdc, Config_ToViewRect(Config_CustomLkpChipRect(hWnd), st), chip, Settings_GetLastKeyPriority());

    Config_DrawCustomToggle(hWnd, hdc, g, Config_ToViewRect(Config_CustomToggleRect(hWnd, 2), st), L"Block Bound Keys", Settings_GetBlockBoundKeys(), true);
    if (Settings_GetBlockBoundKeys()) {
    // Inset children and a quiet connecting rail express ownership without
    // introducing another heading/font or a permanently empty container.
    RECT group = Config_BlockAllowRect(hWnd);
    group.left = S(hWnd, 20);
    group.bottom = Config_CustomToggleRect(hWnd, 2).bottom + Config_BlockOptionsSpace(hWnd) - S(hWnd, 8);
    group = Config_ToViewRect(group, st);
    Gdiplus::Pen rail(Gp(UiTheme::Color_Border()), static_cast<Gdiplus::REAL>(S(hWnd, 2)));
    g.DrawLine(&rail, static_cast<INT>(group.left), static_cast<INT>(group.top),
        static_cast<INT>(group.left), static_cast<INT>(group.bottom));
    Config_DrawCustomToggle(hWnd, hdc, g, Config_ToViewRect(Config_BlockAllowRect(hWnd), st),
        L"Keep Alt and Tab unblocked", Settings_GetBlockKeysAllowAltTab(), true);
    RECT shortcut = Config_BlockShortcutRect(hWnd);
    RECT label = shortcut; label.left = S(hWnd, 36); label.right = shortcut.left - S(hWnd, 8);
    CustomPage_DrawText(hdc, L"Toggle shortcut", Config_ToViewRect(label, st), UiTheme::Color_TextMuted(), DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    CustomPage_DrawButton(g, hdc, Config_ToViewRect(shortcut, st),
        st->blockShortcutCapturing ? L"Press shortcut..." : Config_BlockShortcutText(),
        st->blockShortcutCapturing || st->hotCustomId == ID_BLOCK_KEYS_SHORTCUT,
        st->pressedCustomId == ID_BLOCK_KEYS_SHORTCUT, true);
    CustomPage_DrawButton(g, hdc, Config_ToViewRect(Config_BlockShortcutClearRect(hWnd), st), L"Clear",
        st->hotCustomId == ID_BLOCK_KEYS_CLEAR_SHORTCUT, st->pressedCustomId == ID_BLOCK_KEYS_CLEAR_SHORTCUT,
        Settings_GetBlockKeysHotkey() != 0);
    if (!st->blockShortcutError.empty() || App_BlockKeysHotkeyError()) {
        RECT error{S(hWnd, 36), shortcut.bottom + S(hWnd, 6), S(hWnd, 720), shortcut.bottom + S(hWnd, 34)};
        CustomPage_DrawText(hdc, st->blockShortcutError.empty() ? L"Shortcut unavailable. Click to reassign it." : st->blockShortcutError,
            Config_ToViewRect(error, st), RGB(235, 185, 100), DT_LEFT | DT_WORDBREAK);
    }
    }
    if (halljoy::input_privilege::detector.warning)
        CustomPage_DrawText(hdc, halljoy::input_privilege::kText,
            Config_ToViewRect(Config_PrivilegeWarningRect(hWnd), st), RGB(235, 185, 100),
            DT_LEFT | DT_WORDBREAK | DT_NOPREFIX);

    BackendAnalogTelemetry t{};
    Backend_GetAnalogTelemetry(&t);
    if (t.sparkConnected)
    {
        CustomPage_DrawText(hdc, L"HE poll mode", Config_ToViewRect(Config_CustomSparkModeLabelRect(hWnd), st), UiTheme::Color_Text(), DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        CustomPage_DrawText(hdc, L"Rows", Config_ToViewRect(Config_CustomSparkRowsLabelRect(hWnd), st), UiTheme::Color_Text(), DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        const wchar_t* mode = L"Safe";
        if (Settings_GetSparkPollMode() == SettingsSparkPollMode_FastYield) mode = L"Fast yield";
        else if (Settings_GetSparkPollMode() == SettingsSparkPollMode_MaxBurst) mode = L"Max burst";
        wchar_t rows[24]{};
        UINT rowLimit = Settings_GetSparkRowLimit();
        if (rowLimit == 0) wcscpy_s(rows, L"Auto");
        else swprintf_s(rows, L"%u row%ls", (unsigned)rowLimit, rowLimit == 1 ? L"" : L"s");
        Config_DrawCustomCombo(hWnd, hdc, g, st->cmbSparkPollMode,
            Config_ToViewRect(Config_CustomSparkModeRect(hWnd), st), mode, true);
        Config_DrawCustomCombo(hWnd, hdc, g, st->cmbSparkRows,
            Config_ToViewRect(Config_CustomSparkRowsRect(hWnd), st), rows, true);
    }
}

static void Config_DrawLiveStatus(HWND hWnd, HDC hdc, ConfigPageState* st)
{
    if (!st || !st->customControls) return;
    RECT row = Config_ToViewRect(Config_CustomStatusRect(hWnd, st), st);
    row.bottom = row.top + S(hWnd, 18);
    if (!st->profileStatusText.empty())
        CustomPage_DrawText(hdc, st->profileStatusText, row, UiTheme::Color_TextMuted(),
            DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
}

static bool Config_UpdateSparkCombos(HWND hWnd, ConfigPageState* st)
{
    if (!st) return false;
    BackendAnalogTelemetry t{};
    Backend_GetAnalogTelemetry(&t);
    const bool visible = st->customControls && t.sparkConnected;
    const bool changed = visible != st->sparkControlsVisible;
    st->sparkControlsVisible = visible;

    HWND combos[] = { st->cmbSparkPollMode, st->cmbSparkRows };
    for (HWND combo : combos)
        if (combo && (!visible || changed))
        {
            PremiumCombo::ShowDropDown(combo, false);
            ShowWindow(combo, SW_HIDE);
            EnableWindow(combo, visible ? TRUE : FALSE);
        }
    if (st->cmbSparkPollMode && PremiumCombo::GetCurSel(st->cmbSparkPollMode) !=
        (int)std::clamp<UINT>(Settings_GetSparkPollMode(), 0u, 2u))
        PremiumCombo::SetCurSel(st->cmbSparkPollMode,
            (int)std::clamp<UINT>(Settings_GetSparkPollMode(), 0u, 2u), false);
    if (st->cmbSparkRows && PremiumCombo::GetCurSel(st->cmbSparkRows) !=
        (int)std::clamp<UINT>(Settings_GetSparkRowLimit(), 0u, 8u))
        PremiumCombo::SetCurSel(st->cmbSparkRows,
            (int)std::clamp<UINT>(Settings_GetSparkRowLimit(), 0u, 8u), false);
    return changed;
}

static void Config_SetCustomChildrenVisible(ConfigPageState* st, bool visible)
{
    if (!st) return;
    int cmd = visible ? SW_SHOWNA : SW_HIDE;
    HWND controls[] = {
        st->chkSnappy, st->chkLastKeyPriority, st->lblLastKeyPrioritySensitivity,
        st->sldLastKeyPrioritySensitivity, st->chipLastKeyPrioritySensitivity,
        st->chkBlockBoundKeys,
        st->btnAnalogSelfTest, st->lblAnalogSelfTest, st->lblProfileStatus
    };
    for (HWND h : controls)
    {
        if (h && IsWindow(h))
        {
            EnableWindow(h, visible ? TRUE : FALSE);
            ShowWindow(h, cmd);
            if (!visible)
            {
                SetWindowPos(h, nullptr, -32000, -32000, 1, 1,
                    SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOOWNERZORDER);
            }
        }
    }
}

static void Config_RenderCacheContent(HWND hWnd, HDC hdc, const RECT& full, void* user)
{
    auto* st = (ConfigPageState*)user;
    if (!st) return;

    int oldScroll = st->scrollY;
    st->scrollY = 0;
    SetPropW(hWnd, CONFIG_SCROLLY_PROP, (HANDLE)(INT_PTR)0);

    RECT rc = full;
    KeySettingsPanel_DrawGraphRetainedContent(hdc, rc);
    DrawCpWeightHintIfNeeded(hWnd, hdc);
    KeySettingsPanel_DrawControls(hWnd, hdc);
    Config_DrawCustomControls(hWnd, hdc, st);

    st->scrollY = oldScroll;
    SetPropW(hWnd, CONFIG_SCROLLY_PROP, (HANDLE)(INT_PTR)oldScroll);
}

static void Config_MarkSurfaceDirty(HWND hWnd, ConfigPageState* st)
{
    if (!st) return;
    CustomPageSurface_MarkDirty(hWnd, &st->surface);
}

static void Config_ClosePopupAnchors(ConfigPageState* st)
{
    KeySettingsPanel_CloseCustomPopups();
    if (!st) return;
    HWND combos[] = { st->cmbSparkPollMode, st->cmbSparkRows };
    for (HWND combo : combos)
    {
        if (!combo) continue;
        PremiumCombo::ShowDropDown(combo, false);
        ShowWindow(combo, SW_HIDE);
    }
}

static void Config_OpenPopupAnchor(HWND hWnd, ConfigPageState* st, HWND combo, RECT contentRect)
{
    if (!st || !combo) return;
    Config_ClosePopupAnchors(st);
    RECT view = Config_ToViewRect(contentRect, st);
    SetWindowPos(combo, HWND_TOP, view.left, view.top,
        std::max(1L, view.right - view.left), std::max(1L, view.bottom - view.top),
        SWP_NOACTIVATE | SWP_SHOWWINDOW);
    SetFocus(combo);
    PremiumCombo::ShowDropDown(combo, true);
}

static bool Config_HandleCustomControlsMouse(HWND hWnd, ConfigPageState* st, UINT msg, WPARAM wParam, LPARAM contentLParam)
{
    if (!st || !st->customControls) return false;
    POINT pt{ (short)LOWORD(contentLParam), (short)HIWORD(contentLParam) };

    auto hit = [&](const RECT& rc) { return pt.x >= rc.left && pt.x < rc.right && pt.y >= rc.top && pt.y < rc.bottom; };
    int hitId = 0;
    if (hit(Config_CustomToggleRect(hWnd, 0))) hitId = ID_SNAPPY;
    else if (hit(Config_CustomToggleRect(hWnd, 1))) hitId = ID_LAST_KEY_PRIORITY;
    else if (hit(Config_CustomToggleRect(hWnd, 2))) hitId = ID_BLOCK_BOUND_KEYS;
    else if (Settings_GetBlockBoundKeys() && hit(Config_BlockAllowRect(hWnd))) hitId = ID_BLOCK_KEYS_ALLOW_ALT_TAB;
    else if (Settings_GetBlockBoundKeys() && hit(Config_BlockShortcutRect(hWnd))) hitId = ID_BLOCK_KEYS_SHORTCUT;
    else if (Settings_GetBlockBoundKeys() && Settings_GetBlockKeysHotkey() && hit(Config_BlockShortcutClearRect(hWnd))) hitId = ID_BLOCK_KEYS_CLEAR_SHORTCUT;
    else if (st->sparkControlsVisible && hit(Config_CustomSparkModeRect(hWnd))) hitId = ID_SPARK_POLL_MODE;
    else if (st->sparkControlsVisible && hit(Config_CustomSparkRowsRect(hWnd))) hitId = ID_SPARK_ROW_LIMIT;
    else if (hit(Config_CustomLkpSliderRect(hWnd))) hitId = ID_LAST_KEY_PRIORITY_SENS_SLIDER;

    if (msg == WM_MOUSEMOVE)
    {
        if (st->dragCustomId == ID_LAST_KEY_PRIORITY_SENS_SLIDER)
        {
            int v = CustomPage_SliderValueFromPoint(hWnd, Config_CustomLkpSliderRect(hWnd), 1, 100, pt.x);
            Settings_SetLastKeyPrioritySensitivity(Config_SliderToLkpSensitivity(v));
            Config_UpdateLkpSensitivityUi(st);
            RequestSave(hWnd);
            Config_MarkSurfaceDirty(hWnd, st);
            return true;
        }
        if (st->hotCustomId != hitId)
        {
            st->hotCustomId = hitId;
            Config_MarkSurfaceDirty(hWnd, st);
        }
        if (hitId)
            SetCursor(LoadCursorW(nullptr, IDC_HAND));
        return hitId != 0;
    }

    if (msg == WM_LBUTTONDOWN)
    {
        if (hitId)
        {
            st->pressedCustomId = hitId;
            if (hitId == ID_LAST_KEY_PRIORITY_SENS_SLIDER)
                st->dragCustomId = hitId;
            SetFocus(hWnd);
            SetCapture(hWnd);
            Config_MarkSurfaceDirty(hWnd, st);
            return true;
        }
        return false;
    }

    if (msg == WM_LBUTTONUP)
    {
        int pressed = st->pressedCustomId;
        st->pressedCustomId = 0;
        st->dragCustomId = 0;
        if (GetCapture() == hWnd)
            ReleaseCapture();

        if (!pressed || pressed != hitId)
        {
            Config_MarkSurfaceDirty(hWnd, st);
            return pressed != 0;
        }

        if (pressed == ID_LAST_KEY_PRIORITY_SENS_SLIDER)
        {
            int v = CustomPage_SliderValueFromPoint(hWnd, Config_CustomLkpSliderRect(hWnd), 1, 100, pt.x);
            Settings_SetLastKeyPrioritySensitivity(Config_SliderToLkpSensitivity(v));
            Config_UpdateLkpSensitivityUi(st);
            RequestSave(hWnd);
        }
        else
        {
            if (pressed == ID_SPARK_POLL_MODE)
                Config_OpenPopupAnchor(hWnd, st, st->cmbSparkPollMode, Config_CustomSparkModeRect(hWnd));
            else if (pressed == ID_SPARK_ROW_LIMIT)
                Config_OpenPopupAnchor(hWnd, st, st->cmbSparkRows, Config_CustomSparkRowsRect(hWnd));
            else
                PostMessageW(hWnd, WM_COMMAND, MAKEWPARAM((UINT)pressed, BN_CLICKED), 0);
        }
        Config_MarkSurfaceDirty(hWnd, st);
        return true;
    }

    (void)wParam;
    return false;
}

static void Config_OffsetAllChildren(HWND hWnd, int dy)
{
    if (dy == 0) return;

    int count = 0;
    for (HWND child = GetWindow(hWnd, GW_CHILD); child; child = GetWindow(child, GW_HWNDNEXT))
        ++count;
    if (count <= 0) return;

    HDWP hdwp = BeginDeferWindowPos(count);
    for (HWND child = GetWindow(hWnd, GW_CHILD); child; child = GetWindow(child, GW_HWNDNEXT))
    {
        RECT rc{};
        if (!GetWindowRect(child, &rc)) continue;
        MapWindowPoints(nullptr, hWnd, (LPPOINT)&rc, 2);
        if (hdwp)
        {
            hdwp = DeferWindowPos(hdwp, child, nullptr,
                rc.left, rc.top + dy, 0, 0,
                SWP_NOZORDER | SWP_NOSIZE | SWP_NOACTIVATE);
        }
        else
        {
            SetWindowPos(child, nullptr,
                rc.left, rc.top + dy, 0, 0,
                SWP_NOZORDER | SWP_NOSIZE | SWP_NOACTIVATE);
        }
    }

    if (hdwp) EndDeferWindowPos(hdwp);
}

static void Config_RequestFullRepaint(HWND hWnd)
{
    RedrawWindow(hWnd, nullptr, nullptr,
        RDW_INVALIDATE | RDW_NOERASE | RDW_NOCHILDREN | RDW_UPDATENOW);
}

static int Config_GetViewportHeight(HWND hWnd)
{
    RECT rc{};
    GetClientRect(hWnd, &rc);
    int h = (int)(rc.bottom - rc.top);
    return std::max(0, h);
}

static int Config_ComputeBaseContentBottom(HWND hWnd)
{
    int margin = S(hWnd, 12);

    // Graph + CP hint region (painted on parent, not child controls)
    int graphBottom = S(hWnd, 86) + S(hWnd, 160) + margin;
    int cpHintBottom = S(hWnd, 286) + S(hWnd, 20) + margin;

    // Explicit controls below graph
    int y = S(hWnd, 310);
    int bottom = y
        + (S(hWnd, 26) + S(hWnd, 10)) // Snap Stick
        + (S(hWnd, 26) + S(hWnd, 10)) // Last Key Priority + sensitivity slider
        + (S(hWnd, 26) + S(hWnd, 10)) // Block Bound Keys
        + (S(hWnd, 26) + S(hWnd, 10)) // Spark missed HID diagnostics
        + (S(hWnd, 26) + S(hWnd, 10)); // Spark telemetry diagnostics

    BackendAnalogTelemetry t{};
    Backend_GetAnalogTelemetry(&t);
    if (t.sparkConnected)
        bottom += (S(hWnd, 28) + S(hWnd, 10)); // SparkLink HE poll controls

    if (kShowAnalogSelfTestControls)
    {
        bottom += (S(hWnd, 28) + S(hWnd, 8)); // Self-test button
        bottom += (S(hWnd, 34) + S(hWnd, 8)); // Self-test result
    }

    RECT telemetryRect = Config_CustomStatusRect(hWnd, nullptr);
    bottom = (std::max)(bottom, (int)telemetryRect.bottom + margin);

    return std::max(bottom, std::max(graphBottom, cpHintBottom));
}

static int Config_RecalcContentHeight(HWND hWnd, ConfigPageState* st)
{
    if (!st) return 0;

    int bottom = Config_ComputeBaseContentBottom(hWnd);
    int margin = S(hWnd, 12);

    struct EnumCtx
    {
        HWND parent = nullptr;
        int bottom = 0;
        int margin = 0;
        int scrollY = 0;
    } ctx;
    ctx.parent = hWnd;
    ctx.bottom = bottom;
    ctx.margin = margin;
    ctx.scrollY = st->scrollY;

    EnumChildWindows(hWnd,
        [](HWND child, LPARAM lp) -> BOOL
        {
            auto* c = (EnumCtx*)lp;
            if (!c || !c->parent) return TRUE;
            if (!IsWindowVisible(child)) return TRUE;

            RECT rc{};
            if (!GetWindowRect(child, &rc)) return TRUE;
            MapWindowPoints(nullptr, c->parent, (LPPOINT)&rc, 2);
            int childBottom = (int)rc.bottom + c->margin + c->scrollY;
            c->bottom = std::max(c->bottom, childBottom);
            return TRUE;
        },
        (LPARAM)&ctx);

    st->contentHeight = std::max(0, ctx.bottom);
    CustomPageSurface_SetState(&st->surface, st->scrollY, st->contentHeight);
    return st->contentHeight;
}

static int Config_GetMaxScroll(HWND hWnd, ConfigPageState* st)
{
    if (!st) return 0;
    int contentH = Config_RecalcContentHeight(hWnd, st);
    CustomPageSurface_SetState(&st->surface, st->scrollY, contentH);
    return CustomPageSurface_GetMaxScroll(hWnd, &st->surface);
}

static RECT Config_GetScrollTrackRect(HWND hWnd)
{
    return CustomPageSurface_GetScrollTrackRect(hWnd);
}

static RECT Config_GetScrollThumbRect(HWND hWnd, ConfigPageState* st)
{
    RECT tr = Config_GetScrollTrackRect(hWnd);
    if (!st) return tr;
    Config_RecalcContentHeight(hWnd, st);
    CustomPageSurface_SetState(&st->surface, st->scrollY, st->contentHeight);
    return CustomPageSurface_GetScrollThumbRect(hWnd, &st->surface);
}

static void Config_SetScrollY(HWND hWnd, ConfigPageState* st, int newScrollY)
{
    if (!st) return;

    int maxScroll = Config_GetMaxScroll(hWnd, st);

    int target = std::clamp(newScrollY, 0, maxScroll);
    if (target != st->scrollY)
    {
        if (st->customControls)
        {
            CustomPageSurface_SetState(&st->surface, st->scrollY, st->contentHeight);
            CustomPageSurface_SetScrollY(hWnd, &st->surface, target);
            st->scrollY = st->surface.scrollY;
        }
        else
        {
            int dy = st->scrollY - target;
            Config_OffsetAllChildren(hWnd, dy);
            st->scrollY = target;
            CustomPageSurface_SetState(&st->surface, st->scrollY, st->contentHeight);
        }
        SetPropW(hWnd, CONFIG_SCROLLY_PROP, (HANDLE)(INT_PTR)st->scrollY);
    }
    if (st->customControls)
    {
        KeySettingsPanel_UpdateCustomControlsLayout(hWnd);
        Config_UpdateSparkCombos(hWnd, st);
    }
    else
        Config_RequestFullRepaint(hWnd);
}

static CustomPageScrollResult Config_HandleSharedScroll(
    HWND hWnd, ConfigPageState* st, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (!st) return CustomPageScrollResult::NotHandled;
    Config_RecalcContentHeight(hWnd, st);
    CustomPageSurface_SetState(&st->surface, st->scrollY, st->contentHeight);
    CustomPageScrollResult result = CustomPageSurface_HandleScrollMessage(
        hWnd, &st->surface, &st->scroll, msg, wParam, lParam, S(hWnd, 54));
    if (result == CustomPageScrollResult::OffsetChanged)
    {
        st->scrollY = st->surface.scrollY;
        SetPropW(hWnd, CONFIG_SCROLLY_PROP, (HANDLE)(INT_PTR)st->scrollY);
    }
    return result;
}

static LPARAM Config_AdjustClientMouseLParamForScroll(ConfigPageState* st, LPARAM lParam)
{
    if (!st || st->scrollY == 0) return lParam;
    int x = (short)LOWORD(lParam);
    int y = (short)HIWORD(lParam);
    y += st->scrollY;
    return MAKELPARAM((short)x, (short)y);
}

static LPARAM Config_AdjustWheelLParamForScroll(HWND hWnd, ConfigPageState* st, LPARAM lParam)
{
    if (!st || st->scrollY == 0) return lParam;

    POINT pt{ (short)LOWORD(lParam), (short)HIWORD(lParam) };
    ScreenToClient(hWnd, &pt);
    pt.y += st->scrollY;
    ClientToScreen(hWnd, &pt);
    return MAKELPARAM((short)pt.x, (short)pt.y);
}

static void DrawConfigScrollbar(HWND hWnd, HDC hdc, ConfigPageState* st)
{
    if (!st) return;
    Config_RecalcContentHeight(hWnd, st);
    CustomPageSurface_SetState(&st->surface, st->scrollY, st->contentHeight);
    CustomPageSurface_DrawScrollbar(hWnd, hdc, &st->surface, st->scroll.draggingThumb);
}

static std::wstring SanitizePresetNameForFile(const std::wstring& in)
{
    std::wstring s = FileNamePolicy_NormalizeStem(in);
    if (s.size() >= 4)
    {
        const wchar_t* tail = s.c_str() + (s.size() - 4);
        if (_wcsicmp(tail, L".ini") == 0)
            s.resize(s.size() - 4);
    }
    return FileNamePolicy_NormalizeStem(s);
}

// ============================================================================
// Premium toast (small popup hint) for delete confirmation
// ============================================================================
static void Toast_EnsureWindow(HWND hPage, ConfigPageState* st)
{
    if (!st || st->hToast) return;

    static bool reg = false;
    if (!reg)
    {
        WNDCLASSW wc{};
        wc.lpfnWndProc = [](HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) -> LRESULT
            {
                auto* stLocal = (ConfigPageState*)GetWindowLongPtrW(hWnd, GWLP_USERDATA);

                switch (msg)
                {
                case WM_NCCREATE:
                    return TRUE;

                case WM_CREATE:
                {
                    auto* cs = (CREATESTRUCTW*)lParam;
                    stLocal = (ConfigPageState*)cs->lpCreateParams;
                    SetWindowLongPtrW(hWnd, GWLP_USERDATA, (LONG_PTR)stLocal);

                    // layered alpha
                    SetLayeredWindowAttributes(hWnd, 0, 235, LWA_ALPHA);
                    return 0;
                }

                case WM_ERASEBKGND:
                    return 1;

                case WM_PAINT:
                {
                    PAINTSTRUCT ps{};
                    HDC hdc = BeginPaint(hWnd, &ps);

                    RECT rc{};
                    GetClientRect(hWnd, &rc);
                    int w = rc.right - rc.left;
                    int h = rc.bottom - rc.top;

                    Graphics g(hdc);
                    g.SetSmoothingMode(SmoothingModeAntiAlias);
                    g.SetPixelOffsetMode(PixelOffsetModeHighQuality);
                    g.SetCompositingQuality(CompositingQualityHighQuality);
                    g.SetTextRenderingHint(TextRenderingHintAntiAliasGridFit);

                    RectF r(0.0f, 0.0f, (REAL)w, (REAL)h);
                    r.Inflate(-1.0f, -1.0f);

                    float rad = std::clamp(r.Height * 0.35f, 8.0f, 14.0f);

                    GraphicsPath p;
                    AddRoundRectPath(p, r, rad);

                    // Fill
                    Color fill(245, 34, 34, 34); // slightly translucent
                    SolidBrush brFill(fill);
                    g.FillPath(&brFill, &p);

                    // Border (soft red)
                    Color border(255, 255, 90, 90);
                    Pen pen(border, 2.0f);
                    pen.SetLineJoin(LineJoinRound);
                    g.DrawPath(&pen, &p);

                    // Text
                    std::wstring text = (stLocal ? stLocal->toastText : L"");
                    if (!text.empty())
                    {
                        FontFamily ff(L"Segoe UI");
                        float em = std::clamp(r.Height * 0.36f, 11.0f, 14.0f);
                        Font font(&ff, em, FontStyleRegular, UnitPixel);

                        StringFormat fmt;
                        fmt.SetAlignment(StringAlignmentNear);
                        fmt.SetLineAlignment(StringAlignmentCenter);
                        fmt.SetTrimming(StringTrimmingEllipsisCharacter);
                        fmt.SetFormatFlags(StringFormatFlagsNoWrap);

                        RectF tr = r;
                        tr.Inflate(-10.0f, 0.0f);

                        SolidBrush brTxt(Gp(UiTheme::Color_Text(), 255));
                        g.DrawString(text.c_str(), -1, &font, tr, &fmt, &brTxt);
                    }

                    EndPaint(hWnd, &ps);
                    return 0;
                }

                case WM_NCDESTROY:
                    SetWindowLongPtrW(hWnd, GWLP_USERDATA, 0);
                    return 0;
                }

                return DefWindowProcW(hWnd, msg, wParam, lParam);
            };

        wc.hInstance = (HINSTANCE)GetWindowLongPtrW(hPage, GWLP_HINSTANCE);
        wc.lpszClassName = L"DD_PresetToast";
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.hbrBackground = nullptr;
        RegisterClassW(&wc);
        reg = true;
    }

    HWND ownerTop = GetAncestor(hPage, GA_ROOT);

    st->hToast = CreateWindowExW(
        WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_NOACTIVATE | WS_EX_LAYERED,
        L"DD_PresetToast",
        L"",
        WS_POPUP,
        0, 0, 10, 10,
        ownerTop, nullptr, (HINSTANCE)GetWindowLongPtrW(hPage, GWLP_HINSTANCE),
        st);

    if (st->hToast)
        ShowWindow(st->hToast, SW_HIDE);
}

static void Toast_Hide(HWND hPage, ConfigPageState* st)
{
    if (!st) return;
    st->toastHideAt = 0;
    if (hPage) KillTimer(hPage, TOAST_TIMER_ID);
    if (st->hToast) ShowWindow(st->hToast, SW_HIDE);
}

static void Toast_ShowNearCursor(HWND hPage, ConfigPageState* st, const wchar_t* text)
{
    if (!st || !hPage) return;

    Toast_EnsureWindow(hPage, st);
    if (!st->hToast) return;

    st->toastText = (text ? text : L"");

    // Measure text using Win32 DrawText for good sizing
    HFONT font = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
    HDC hdc = GetDC(hPage);
    HGDIOBJ oldF = SelectObject(hdc, font);

    RECT calc{ 0,0,0,0 };
    DrawTextW(hdc, st->toastText.c_str(), (int)st->toastText.size(), &calc,
        DT_CALCRECT | DT_SINGLELINE | DT_NOPREFIX);

    SelectObject(hdc, oldF);
    ReleaseDC(hPage, hdc);

    int padX = S(hPage, 16);
    int padY = S(hPage, 10);

    // FIX: RECT uses LONG; std::max needs same types
    int textW = (int)(calc.right - calc.left);
    int textH = (int)(calc.bottom - calc.top);

    int w = textW + padX * 2;
    int h = std::max(S(hPage, 34), textH + padY * 2);

    w = std::clamp(w, S(hPage, 220), S(hPage, 520));

    POINT pt{};
    GetCursorPos(&pt);

    int x = pt.x + S(hPage, 14);
    int y = pt.y + S(hPage, 18);

    // Clamp to monitor work area
    HMONITOR mon = MonitorFromPoint(pt, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi{};
    mi.cbSize = sizeof(mi);
    if (GetMonitorInfoW(mon, &mi))
    {
        RECT wa = mi.rcWork;
        if (x + w > wa.right) x = wa.right - w;
        if (y + h > wa.bottom) y = wa.bottom - h;
        if (x < wa.left) x = wa.left;
        if (y < wa.top) y = wa.top;
    }

    SetWindowPos(st->hToast, HWND_TOPMOST, x, y, w, h,
        SWP_NOACTIVATE | SWP_SHOWWINDOW);

    InvalidateRect(st->hToast, nullptr, TRUE);

    st->toastHideAt = GetTickCount64() + TOAST_SHOW_MS;
    SetTimer(hPage, TOAST_TIMER_ID, 30, nullptr);
}

static void DeleteConfirm_Clear(HWND hPage, ConfigPageState* st)
{
    if (!st) return;
    st->pendingDeleteIdx = -1;
    st->pendingDeleteTick = 0;
    Toast_Hide(hPage, st);
}

// Draw hint for CP weights
static void DrawCpWeightHintIfNeeded(HWND hWnd, HDC hdc)
{
    float w01 = 0.0f;
    KeySettingsPanel_DragHint hint = KeySettingsPanel_GetDragHint(&w01);
    if (hint == KeySettingsPanel_DragHint::None)
        return;

    RECT rcClient{};
    GetClientRect(hWnd, &rcClient);

    int x = S(hWnd, 12);
    int y = S(hWnd, 286);
    auto* st = (ConfigPageState*)GetWindowLongPtrW(hWnd, GWLP_USERDATA);
    if (st)
        y -= st->scrollY;

    Graphics g(hdc);
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    g.SetPixelOffsetMode(PixelOffsetModeHighQuality);
    g.SetTextRenderingHint(TextRenderingHintAntiAliasGridFit);

    const Color orange(255, 255, 170, 0);
    const Color orangeBorder(255, 210, 135, 0);

    float iconD = (float)S(hWnd, 16);
    RectF icon((float)x, (float)y, iconD, iconD);

    {
        SolidBrush br(orange);
        g.FillEllipse(&br, icon);
        Pen pen(orangeBorder, 1.0f);
        g.DrawEllipse(&pen, icon);
    }

    {
        FontFamily ff(L"Segoe UI");
        float em = std::clamp(iconD * 0.78f, 10.0f, 14.0f);
        Font font(&ff, em, FontStyleBold, UnitPixel);

        StringFormat fmt;
        fmt.SetAlignment(StringAlignmentCenter);
        fmt.SetLineAlignment(StringAlignmentCenter);
        fmt.SetFormatFlags(StringFormatFlagsNoWrap);

        SolidBrush txt(Color(255, 20, 20, 20));
        g.DrawString(L"!", -1, &font, icon, &fmt, &txt);
    }

    int pct = (int)std::lround(std::clamp(w01, 0.0f, 1.0f) * 100.0f);

    wchar_t msg2[256]{};
    swprintf_s(msg2, L"Use mouse wheel to change weight (%d%%).", pct);

    {
        FontFamily ff(L"Segoe UI");
        float em = (float)S(hWnd, 13);
        em = std::clamp(em, 11.0f, 14.0f);
        Font font(&ff, em, FontStyleRegular, UnitPixel);

        StringFormat fmt;
        fmt.SetAlignment(StringAlignmentNear);
        fmt.SetLineAlignment(StringAlignmentCenter);
        fmt.SetTrimming(StringTrimmingEllipsisCharacter);
        fmt.SetFormatFlags(StringFormatFlagsNoWrap);

        RectF tr(icon.GetRight() + 8.0f, (REAL)y - 1.0f,
            (REAL)(rcClient.right - rcClient.left) - (icon.GetRight() + 8.0f) - (REAL)S(hWnd, 12),
            iconD + 2.0f);

        SolidBrush txt(Gp(UiTheme::Color_TextMuted(), 255));
        g.DrawString(msg2, -1, &font, tr, &fmt, &txt);
    }

    (void)hint;
}

static HWND GetPresetCombo(HWND hWnd)
{
    return GetDlgItem(hWnd, KSP_ID_PROFILE);
}

static void SelectActivePresetInCombo(HWND hWnd)
{
    HWND hCombo = GetPresetCombo(hWnd);
    if (!hCombo) return;

    std::vector<KeyboardProfiles::ProfileInfo> list;
    int activeIdx = KeyboardProfiles::RefreshList(list); // active preset index among presets

    if (activeIdx >= 0)
    {
        // Indices match: KeySettingsPanel adds "+ Create..." after preset items.
        PremiumCombo::SetCurSel(hCombo, activeIdx, false);
        PremiumCombo::SetExtraIcon(hCombo, PremiumCombo::ExtraIconKind::None);
    }
}

static void DoBeginInlineCreate(HWND hWnd, ConfigPageState* st)
{
    HWND hCombo = GetPresetCombo(hWnd);
    if (!hCombo) return;

    int count = PremiumCombo::GetCount(hCombo);
    if (count <= 0) return;

    int idx = count - 1;

    PremiumCombo::ShowDropDown(hCombo, true);
    PremiumCombo::SetCurSel(hCombo, idx, false);
    PremiumCombo::BeginInlineEdit(hCombo, idx, false);

    SetProfileStatus(st, L"Type a name and press Enter to create a new preset.");
}

static bool DeletePreset_NoPopup_ConfigPage(HWND hWnd, ConfigPageState* st, int idx, bool requireShift)
{
    std::vector<KeyboardProfiles::ProfileInfo> list;
    KeyboardProfiles::RefreshList(list);

    if (idx < 0 || idx >= (int)list.size())
        return false;

    bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
    if (requireShift && !shift)
    {
        SetProfileStatus(st, L"Hold Shift to delete.");
        return false;
    }

    if (KeyboardProfiles::DeletePreset(list[idx].path))
    {
        SetProfileStatus(st, L"Preset deleted.");
        KeySettingsPanel_HandleCommand(hWnd, 9999, 0);
        RequestSave(hWnd);
        return true;
    }

    SetProfileStatus(st, L"ERROR: Failed to delete preset.");
    KeySettingsPanel_HandleCommand(hWnd, 9999, 0);
    return false;
}

static void Config_EndBlockShortcutCapture(HWND hWnd, ConfigPageState* st)
{
    if (!st) return;
    st->blockShortcutCapturing = false;
    App_SetBlockKeysHotkeyCapture(false);
    Config_MarkSurfaceDirty(hWnd, st);
}

static void Config_CommitBlockShortcut(HWND hWnd, ConfigPageState* st, UINT chord)
{
    if (!st) return;
    Config_EndBlockShortcutCapture(hWnd, st);
    const DWORD error = App_SetBlockKeysHotkey(chord);
    if (error) {
        st->blockShortcutError = error == ERROR_HOTKEY_ALREADY_REGISTERED
            ? L"Shortcut is already in use. Choose another."
            : L"This shortcut is unavailable. Choose another.";
    } else {
        st->blockShortcutError.clear();
        HWND root = GetAncestor(hWnd, GA_ROOT);
        if (root) PostMessageW(root, WM_APP_REQUEST_SAVE, 0, 0);
    }
    Config_RecalcContentHeight(hWnd, st);
    Config_MarkSurfaceDirty(hWnd, st);
}

LRESULT CALLBACK KeyboardSubpages_ConfigPageProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    auto* st = (ConfigPageState*)GetWindowLongPtrW(hWnd, GWLP_USERDATA);
    if (msg == halljoy::main_input::QueryExplicitInput())
        return st && st->blockShortcutCapturing ? 1 : 0;
    if (st && st->blockShortcutCapturing && !Settings_GetBlockBoundKeys())
        Config_EndBlockShortcutCapture(hWnd, st);
    if (msg == WM_APP_BLOCK_KEYS_CHANGED) {
        if (st) {
            Config_RefreshFromCurrentSettings(hWnd, st);
            Config_RecalcContentHeight(hWnd, st);
            Config_MarkSurfaceDirty(hWnd, st);
        }
        return 0;
    }
    if (msg == WM_APP_BLOCK_KEYS_CANCEL_CAPTURE) {
        if (st && st->blockShortcutCapturing) Config_EndBlockShortcutCapture(hWnd, st);
        return 0;
    }
    if (msg == WM_APP_BLOCK_KEYS_CAPTURED) {
        if (st && st->blockShortcutCapturing)
            Config_CommitBlockShortcut(hWnd, st, (LOWORD(lParam) << 8) | HIWORD(lParam));
        return 0;
    }
    if (st && st->blockShortcutCapturing) {
        if (msg == WM_GETDLGCODE) return DLGC_WANTALLKEYS;
        if (msg == WM_KILLFOCUS || msg == WM_DESTROY || (msg == WM_SHOWWINDOW && !wParam))
            Config_EndBlockShortcutCapture(hWnd, st);
        if (msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN) {
            if (wParam == VK_ESCAPE) { Config_EndBlockShortcutCapture(hWnd, st); return 0; }
            if ((lParam & (1LL << 30)) != 0) return 0; // Auto-repeat is not a new assignment.
            UINT mods = 0;
            if (GetKeyState(VK_CONTROL) & 0x8000) mods |= MOD_CONTROL;
            if (GetKeyState(VK_MENU) & 0x8000) mods |= MOD_ALT;
            if (GetKeyState(VK_SHIFT) & 0x8000) mods |= MOD_SHIFT;
            if ((GetKeyState(VK_LWIN) | GetKeyState(VK_RWIN)) & 0x8000) mods |= MOD_WIN;
            const UINT chord = (mods << 8) | static_cast<UINT>(wParam);
            if (halljoy::block_keys::ValidShortcut(chord)) Config_CommitBlockShortcut(hWnd, st, chord);
            return 0;
        }
        if (msg == WM_KEYUP || msg == WM_SYSKEYUP) return 0;
    }
    if (msg == WM_COMMAND && HIWORD(wParam) == BN_CLICKED && st) {
        // Ignore queued clicks belonging to controls that have just collapsed.
        if (!Settings_GetBlockBoundKeys() &&
            (LOWORD(wParam) == ID_BLOCK_KEYS_ALLOW_ALT_TAB || LOWORD(wParam) == ID_BLOCK_KEYS_SHORTCUT ||
             LOWORD(wParam) == ID_BLOCK_KEYS_CLEAR_SHORTCUT)) return 0;
        if (LOWORD(wParam) == ID_BLOCK_KEYS_ALLOW_ALT_TAB) {
            Settings_SetBlockKeysAllowAltTab(!Settings_GetBlockKeysAllowAltTab());
            HWND root = GetAncestor(hWnd, GA_ROOT);
            if (root) PostMessageW(root, WM_APP_REQUEST_SAVE, 0, 0);
            Config_MarkSurfaceDirty(hWnd, st);
            return 0;
        }
        if (LOWORD(wParam) == ID_BLOCK_KEYS_SHORTCUT) {
            Config_ClosePopupAnchors(st);
            SetFocus(hWnd);
            st->blockShortcutError.clear();
            st->blockShortcutCapturing = true;
            App_SetBlockKeysHotkeyCapture(true);
            Config_RecalcContentHeight(hWnd, st);
            Config_MarkSurfaceDirty(hWnd, st);
            return 0;
        }
        if (LOWORD(wParam) == ID_BLOCK_KEYS_CLEAR_SHORTCUT) {
            Config_CommitBlockShortcut(hWnd, st, 0);
            return 0;
        }
    }
    if (msg == halljoy::input_privilege::kChangedMessage)
    {
        if (st) {
            Config_RecalcContentHeight(hWnd, st);
            Config_MarkSurfaceDirty(hWnd, st);
        }
        return 0;
    }

    if (msg == PremiumCombo::MsgDropStateChanged())
    {
        if (st && !wParam)
        {
            ShowWindow((HWND)lParam, SW_HIDE);
            Config_MarkSurfaceDirty(hWnd, st);
        }
        return 0;
    }
    static UiPaintAuditCounter paintAudit(L"configuration");

    if (msg == WM_APP_CONFIG_PROFILE_APPLIED)
    {
        if (st)
            Config_RefreshFromCurrentSettings(hWnd, st);
        if (st)
            Config_RecalcContentHeight(hWnd, st);
        if (st)
            Config_MarkSurfaceDirty(hWnd, st);
        return 0;
    }

    if (msg == WM_APP_CONFIG_MARK_SURFACE_DIRTY)
    {
        if (st && st->customControls)
            Config_MarkSurfaceDirty(hWnd, st);
        return 0;
    }

    if (msg == WM_TIMER)
    {
        // 1) toast auto-hide
        if (wParam == TOAST_TIMER_ID && st)
        {
            const ULONGLONG now = GetTickCount64();
            if (st->toastHideAt != 0 && now >= st->toastHideAt)
                Toast_Hide(hWnd, st);
            return 0;
        }

        // 2) analog self-test sampler
        if (wParam == ANALOG_SELF_TEST_TIMER_ID && st && st->selfTestRunning)
        {
            BackendAnalogTelemetry t{};
            Backend_GetAnalogTelemetry(&t);
            st->selfTestPeakRawMilli = std::max(st->selfTestPeakRawMilli, t.trackedMaxRawMilli);
            st->selfTestPeakOutMilli = std::max(st->selfTestPeakOutMilli, t.trackedMaxOutMilli);
            st->selfTestPeakFullMilli = std::max(st->selfTestPeakFullMilli, t.fullBufferMaxMilli);
            st->selfTestPeakFullDevMilli = std::max(st->selfTestPeakFullDevMilli, t.fullBufferDeviceBestMaxMilli);
            st->selfTestMode = t.keycodeMode;

            DWORD now = GetTickCount();
            if (now - st->selfTestStartedAt >= 3000)
                Config_FinishSelfTest(hWnd, st);
            return 0;
        }

        // Forward other timers to KeySettings panel (morph etc.)
        KeySettingsPanel_HandleTimer(hWnd, wParam);
        if (st && st->customControls)
            Config_MarkSurfaceDirty(hWnd, st);
        else if (st && st->scrollY != 0)
            Config_RequestFullRepaint(hWnd);
        return 0;
    }

    if (msg == WM_APP_PROFILE_BEGIN_CREATE)
    {
        DoBeginInlineCreate(hWnd, st);
        Config_MarkSurfaceDirty(hWnd, st);
        return 0;
    }

    // Inline text commit from PremiumCombo (Rename + Create New)
    if (msg == PremiumCombo::MsgItemTextCommit())
    {
        if (st) DeleteConfirm_Clear(hWnd, st);

        int idx = (int)LOWORD(wParam);
        PremiumCombo::ItemButtonKind kind = (PremiumCombo::ItemButtonKind)(int)HIWORD(wParam);
        HWND hCombo = (HWND)lParam;

        if (kind != PremiumCombo::ItemButtonKind::Rename || !hCombo)
            return 0;

        wchar_t newNameBuf[260]{};
        PremiumCombo::ConsumeCommittedText(hCombo, newNameBuf, 260);

        std::wstring raw = newNameBuf;
        std::wstring safe = SanitizePresetNameForFile(raw);

        std::vector<KeyboardProfiles::ProfileInfo> list;
        KeyboardProfiles::RefreshList(list);

        // Current curve shown on screen (the whole point of presets)
        KeyDeadzone curCurve = Ksp_GetVisualCurve();

        // Case A: commit came from the last row => create new preset
        if (idx == (int)list.size())
        {
            if (safe.empty())
            {
                SetProfileStatus(st, L"");
                KeySettingsPanel_HandleCommand(hWnd, 9999, 0);
                return 0;
            }

            if (KeyboardProfiles::CreatePreset(safe, curCurve))
            {
                SetProfileStatus(st, L"Preset created.");

                // refresh list in KeySettingsPanel
                KeySettingsPanel_HandleCommand(hWnd, 9999, 0);

                // force UI selection to the now-active preset (newly created)
                SelectActivePresetInCombo(hWnd);

                // optional: close dropdown after creation (feels premium)
                HWND hCombo2 = GetPresetCombo(hWnd);
                if (hCombo2) PremiumCombo::ShowDropDown(hCombo2, false);

                RequestSave(hWnd);
            }
            else
            {
                SetProfileStatus(st, L"ERROR: Failed to create preset.");
                KeySettingsPanel_HandleCommand(hWnd, 9999, 0);
            }
            return 0;
        }

        // Case B: rename existing preset
        if (idx < 0 || idx >= (int)list.size())
        {
            SetProfileStatus(st, L"Rename failed.");
            KeySettingsPanel_HandleCommand(hWnd, 9999, 0);
            return 0;
        }

        const auto& p = list[idx];

        if (safe.empty())
        {
            SetProfileStatus(st, L"");
            KeySettingsPanel_HandleCommand(hWnd, 9999, 0);
            return 0;
        }

        if (FileNamePolicy_Equivalent(safe, p.name))
        {
            SetProfileStatus(st, L"");
            KeySettingsPanel_HandleCommand(hWnd, 9999, 0);
            return 0;
        }

        fs::path oldPath = fs::path(p.path);
        fs::path dir = oldPath.parent_path();
        std::wstring newPathText;
        if (!FileNamePolicy_BuildChildPath(dir.wstring(), safe, L".ini", newPathText))
        {
            SetProfileStatus(st, L"Rename failed: invalid name.");
            KeySettingsPanel_HandleCommand(hWnd, 9999, 0);
            return 0;
        }

        fs::path newPath = newPathText;

        for (int listIndex = 0; listIndex < (int)list.size(); ++listIndex)
        {
            if (listIndex != idx && FileNamePolicy_Equivalent(list[listIndex].name, safe))
            {
                SetProfileStatus(st, L"Rename failed: name already exists.");
                KeySettingsPanel_HandleCommand(hWnd, 9999, 0);
                return 0;
            }
        }

        if (GetFileAttributesW(newPath.wstring().c_str()) != INVALID_FILE_ATTRIBUTES)
        {
            SetProfileStatus(st, L"Rename failed: name already exists.");
            KeySettingsPanel_HandleCommand(hWnd, 9999, 0);
            return 0;
        }

        std::wstring active = KeyboardProfiles::GetActiveProfileName();

        // If renaming active preset:
        // Save current curve into new file (this also updates "active" state inside module),
        // then delete the old file.
        if (!active.empty() && FileNamePolicy_Equivalent(active, p.name))
        {
            if (!KeyboardProfiles::SavePreset(newPath.wstring(), curCurve))
            {
                // newPath was verified absent above. SavePreset can commit the
                // curve and then fail while committing the active-name state.
                DeleteFileW(newPath.wstring().c_str());
                SetProfileStatus(st, L"Rename failed: could not save new preset.");
                KeySettingsPanel_HandleCommand(hWnd, 9999, 0);
                return 0;
            }

            if (!DeleteFileW(oldPath.wstring().c_str()))
            {
                SetProfileStatus(st, L"Rename incomplete: new preset saved, but old preset could not be deleted.");
                KeySettingsPanel_HandleCommand(hWnd, 9999, 0);
                return 0;
            }

            SetProfileStatus(st, L"Preset renamed.");
            KeySettingsPanel_HandleCommand(hWnd, 9999, 0);
            RequestSave(hWnd);
            return 0;
        }

        // Non-active preset: simple file rename
        BOOL ok = MoveFileExW(oldPath.wstring().c_str(), newPath.wstring().c_str(),
            MOVEFILE_COPY_ALLOWED | MOVEFILE_WRITE_THROUGH);

        if (!ok)
        {
            SetProfileStatus(st, L"Rename failed: file rename error.");
            KeySettingsPanel_HandleCommand(hWnd, 9999, 0);
            return 0;
        }

        SetProfileStatus(st, L"Preset renamed.");
        KeySettingsPanel_HandleCommand(hWnd, 9999, 0);
        return 0;
    }

    // Item button clicks (Delete gets delivered here; Rename starts inline edit internally)
    if (msg == PremiumCombo::MsgItemButton())
    {
        int idx = (int)LOWORD(wParam);
        PremiumCombo::ItemButtonKind kind = (PremiumCombo::ItemButtonKind)(int)HIWORD(wParam);

        if (kind == PremiumCombo::ItemButtonKind::Delete)
        {
            if (!st) return 0;

            bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;

            // Shift => instant delete (no confirmation)
            if (shift)
            {
                DeleteConfirm_Clear(hWnd, st);
                DeletePreset_NoPopup_ConfigPage(hWnd, st, idx, false);
                return 0;
            }

            DWORD now = GetTickCount();

            // Second click within window => delete
            if (st->pendingDeleteIdx == idx && (now - st->pendingDeleteTick) <= TOAST_SHOW_MS)
            {
                DeleteConfirm_Clear(hWnd, st);
                DeletePreset_NoPopup_ConfigPage(hWnd, st, idx, false);
                return 0;
            }

            // First click => arm confirmation + show premium toast
            st->pendingDeleteIdx = idx;
            st->pendingDeleteTick = now;

            Toast_ShowNearCursor(hWnd, st, L"Click again to confirm delete");
            SetProfileStatus(st, L""); // don't spam status bar
            return 0;
        }

        // any other button cancels pending delete
        if (st) DeleteConfirm_Clear(hWnd, st);
        return 0;
    }

    // Extra icon click (save dirty preset)
    if (msg == PremiumCombo::MsgExtraIcon())
    {
        if (st) DeleteConfirm_Clear(hWnd, st);

        HWND hCombo = (HWND)lParam;

        std::vector<KeyboardProfiles::ProfileInfo> list;
        KeyboardProfiles::RefreshList(list);

        int sel = -1;
        int count = 0;
        if (hCombo)
        {
            sel = PremiumCombo::GetCurSel(hCombo);
            count = PremiumCombo::GetCount(hCombo);
        }

        auto refreshUi = [&]()
            {
                KeySettingsPanel_HandleCommand(hWnd, 9999, 0);
                RequestSave(hWnd);
            };

        bool selIsCreateNew = (count > 0 && sel == (count - 1));

        if (list.empty() || selIsCreateNew || sel < 0)
        {
            PostMessageW(hWnd, WM_APP_PROFILE_BEGIN_CREATE, 0, 0);
            return 0;
        }

        if (sel >= 0 && sel < (int)list.size())
        {
            const auto& p = list[sel];

            // Save CURRENT curve (visual state) into selected preset
            KeyDeadzone curCurve = Ksp_GetVisualCurve();

            if (KeyboardProfiles::SavePreset(p.path, curCurve))
            {
                std::wstring ok = L"Preset saved: " + p.name;
                SetProfileStatus(st, ok.c_str());
                refreshUi();
            }
            else
            {
                std::wstring err = L"ERROR: Failed to save preset: " + p.name;
                SetProfileStatus(st, err.c_str());
                refreshUi();
            }
            return 0;
        }

        PostMessageW(hWnd, WM_APP_PROFILE_BEGIN_CREATE, 0, 0);
        return 0;
    }

    switch (msg)
    {
    case WM_ERASEBKGND:
        paintAudit.Event(WM_ERASEBKGND);
        return 1;

    case WM_APP_CONFIG_TELEMETRY_REFRESH:
        if (st)
        {
            if (Config_UpdateSparkCombos(hWnd, st))
            {
                UiAuditTraceInvalidation(L"configuration", L"spark_visibility_changed");
                Config_RecalcContentHeight(hWnd, st);
                Config_MarkSurfaceDirty(hWnd, st);
            }
            else
            {
                RECT status = Config_ToViewRect(Config_CustomStatusRect(hWnd, st), st);
                UiAuditTraceInvalidation(L"configuration", L"telemetry_hash_changed", &status);
                InvalidateRect(hWnd, &status, FALSE);
            }
        }
        return 0;

    case WM_PAINT:
    {
        uint64_t paintStart = CustomPageSurface_QpcNow();
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);
        paintAudit.Event(WM_PAINT, &ps.rcPaint);
        RECT rc{};
        GetClientRect(hWnd, &rc);

        HDC memDC = CreateCompatibleDC(hdc);
        HBITMAP bmp = CreateCompatibleBitmap(hdc, std::max(1, (int)(rc.right - rc.left)), std::max(1, (int)(rc.bottom - rc.top)));
        HGDIOBJ oldBmp = SelectObject(memDC, bmp);
        FillRect(memDC, &rc, UiTheme::Brush_PanelBg());

        if (st && st->customControls)
        {
            Config_RecalcContentHeight(hWnd, st);
            st->surface.scrollY = st->scrollY;
            CustomPageSurface_SetState(&st->surface, st->scrollY, st->contentHeight);
            CustomPageSurface_Present(hWnd, memDC, &st->surface,
                Config_RenderCacheContent, st, st->scroll.draggingThumb);
            st->scrollY = st->surface.scrollY;
            // Dynamic telemetry must be composed after the retained page has
            // been presented. Otherwise graph-only invalidation just copies
            // the old marker from the page cache until an unrelated click.
            KeySettingsPanel_DrawGraphViewportOverlay(memDC, rc);
            Config_DrawLiveStatus(hWnd, memDC, st);
        }
        else
        {
            KeySettingsPanel_DrawGraph(memDC, rc);
            DrawCpWeightHintIfNeeded(hWnd, memDC);
            KeySettingsPanel_DrawControls(hWnd, memDC);
            Config_DrawCustomControls(hWnd, memDC, st);
            DrawConfigScrollbar(hWnd, memDC, st);
        }

        const RECT& dirty = ps.rcPaint;
        if (dirty.right > dirty.left && dirty.bottom > dirty.top)
            BitBlt(hdc, dirty.left, dirty.top, dirty.right - dirty.left, dirty.bottom - dirty.top,
                memDC, dirty.left, dirty.top, SRCCOPY);
        SelectObject(memDC, oldBmp);
        DeleteObject(bmp);
        DeleteDC(memDC);
        EndPaint(hWnd, &ps);
        if (st && st->customControls && st->surface.scrollSampleStartMs != 0)
        {
            CustomPageSurface_BeginPaintSample(&st->surface, paintStart);
            CustomPageSurface_MaybeLogScrollPerf(&st->surface, L"ui.config.scroll");
        }
        return 0;
    }

    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLORBTN:
    {
        HDC hdc = (HDC)wParam;
        SetBkMode(hdc, TRANSPARENT);

        HWND hCtl = (HWND)lParam;
        if (st && hCtl)
        {
            if (st->lblProfileStatus && hCtl == st->lblProfileStatus)
            {
                SetTextColor(hdc, UiTheme::Color_TextMuted());
            }
            else if (st->lblAnalogSelfTest && hCtl == st->lblAnalogSelfTest)
            {
                SetTextColor(hdc, UiTheme::Color_TextMuted());
            }
            else
            {
                SetTextColor(hdc, UiTheme::Color_Text());
            }
        }
        else
        {
            SetTextColor(hdc, UiTheme::Color_Text());
        }

        return (LRESULT)UiTheme::Brush_PanelBg();
    }

    case WM_CREATE:
    {
        HINSTANCE hInst = (HINSTANCE)GetWindowLongPtrW(hWnd, GWLP_HINSTANCE);
        HFONT hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);

        st = new ConfigPageState();
        SetWindowLongPtrW(hWnd, GWLP_USERDATA, (LONG_PTR)st);
        SetPropW(hWnd, CONFIG_SCROLLY_PROP, (HANDLE)(INT_PTR)0);

        KeySettingsPanel_Create(hWnd, hInst);
        KeySettingsPanel_EnableCustomControls(true);
        KeySettingsPanel_SetSelectedHid(KeyboardUI_Internal_GetSelectedHid());

        st->lblProfileStatus = CreateWindowW(L"STATIC", L"",
            WS_CHILD | WS_VISIBLE,
            0, 0, 10, 10,
            hWnd, nullptr, hInst, nullptr);
        SendMessageW(st->lblProfileStatus, WM_SETFONT, (WPARAM)hFont, TRUE);

        st->cmbSparkPollMode = PremiumCombo::Create(hWnd, hInst, 0, 0, 10, 10,
            ID_SPARK_POLL_MODE, WS_CHILD | WS_TABSTOP);
        PremiumCombo::SetFont(st->cmbSparkPollMode, hFont, true);
        PremiumCombo::AddString(st->cmbSparkPollMode, L"Safe");
        PremiumCombo::AddString(st->cmbSparkPollMode, L"Fast yield");
        PremiumCombo::AddString(st->cmbSparkPollMode, L"Max burst");
        PremiumCombo::SetDropMaxVisible(st->cmbSparkPollMode, 3);

        st->cmbSparkRows = PremiumCombo::Create(hWnd, hInst, 0, 0, 10, 10,
            ID_SPARK_ROW_LIMIT, WS_CHILD | WS_TABSTOP);
        PremiumCombo::SetFont(st->cmbSparkRows, hFont, true);
        PremiumCombo::AddString(st->cmbSparkRows, L"Auto");
        for (int rows = 1; rows <= 8; ++rows)
        {
            wchar_t text[24]{};
            swprintf_s(text, L"%d row%ls", rows, rows == 1 ? L"" : L"s");
            PremiumCombo::AddString(st->cmbSparkRows, text);
        }
        PremiumCombo::SetDropMaxVisible(st->cmbSparkRows, 9);

        st->chkSnappy = CreateWindowW(L"BUTTON", L"Snap Stick",
            WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX | BS_OWNERDRAW,
            0, 0, 10, 10,
            hWnd, (HMENU)(INT_PTR)ID_SNAPPY, hInst, nullptr);
        SendMessageW(st->chkSnappy, WM_SETFONT, (WPARAM)hFont, TRUE);

        st->chkBlockBoundKeys = CreateWindowW(L"BUTTON", L"Block Bound Keys",
            WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX | BS_OWNERDRAW,
            0, 0, 10, 10,
            hWnd, (HMENU)(INT_PTR)ID_BLOCK_BOUND_KEYS, hInst, nullptr);
        SendMessageW(st->chkBlockBoundKeys, WM_SETFONT, (WPARAM)hFont, TRUE);

        st->chkLastKeyPriority = CreateWindowW(L"BUTTON", L"Last Key Priority",
            WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX | BS_OWNERDRAW,
            0, 0, 10, 10,
            hWnd, (HMENU)(INT_PTR)ID_LAST_KEY_PRIORITY, hInst, nullptr);
        SendMessageW(st->chkLastKeyPriority, WM_SETFONT, (WPARAM)hFont, TRUE);

        st->lblLastKeyPrioritySensitivity = CreateWindowW(L"STATIC", L"Sensivity",
            WS_CHILD | WS_VISIBLE | SS_LEFT | SS_CENTERIMAGE,
            0, 0, 10, 10,
            hWnd, nullptr, hInst, nullptr);
        SendMessageW(st->lblLastKeyPrioritySensitivity, WM_SETFONT, (WPARAM)hFont, TRUE);

        st->sldLastKeyPrioritySensitivity = PremiumSlider_Create(
            hWnd, hInst, 0, 0, 10, 10, ID_LAST_KEY_PRIORITY_SENS_SLIDER);
        SendMessageW(st->sldLastKeyPrioritySensitivity, TBM_SETRANGE, TRUE, MAKELONG(1, 100));
        SendMessageW(st->sldLastKeyPrioritySensitivity, TBM_SETPOS, TRUE,
            (LPARAM)Config_LkpSensitivityToSlider(Settings_GetLastKeyPrioritySensitivity()));

        st->chipLastKeyPrioritySensitivity = PremiumChip_Create(
            hWnd, hInst, 0, 0, 10, 10, ID_LAST_KEY_PRIORITY_SENS_CHIP);
        SendMessageW(st->chipLastKeyPrioritySensitivity, WM_SETFONT, (WPARAM)hFont, TRUE);

        if (kShowAnalogSelfTestControls)
        {
            st->btnAnalogSelfTest = CreateWindowW(L"BUTTON", L"Run Analog Self-Test",
                WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
                0, 0, 10, 10,
                hWnd, (HMENU)(INT_PTR)ID_ANALOG_SELF_TEST, hInst, nullptr);
            SendMessageW(st->btnAnalogSelfTest, WM_SETFONT, (WPARAM)hFont, TRUE);

            st->lblAnalogSelfTest = CreateWindowW(L"STATIC",
                L"Self-test checks SDK, plugin, and analog stream health.",
                WS_CHILD | WS_VISIBLE | SS_LEFT,
                0, 0, 10, 10,
                hWnd, nullptr, hInst, nullptr);
            SendMessageW(st->lblAnalogSelfTest, WM_SETFONT, (WPARAM)hFont, TRUE);
        }

        // initial state
        SendMessageW(st->chkSnappy, BM_SETCHECK, Settings_GetSnappyJoystick() ? BST_CHECKED : BST_UNCHECKED, 0);
        SendMessageW(st->chkLastKeyPriority, BM_SETCHECK, Settings_GetLastKeyPriority() ? BST_CHECKED : BST_UNCHECKED, 0);
        SendMessageW(st->chkBlockBoundKeys, BM_SETCHECK, Settings_GetBlockBoundKeys() ? BST_CHECKED : BST_UNCHECKED, 0);
        SnappyDebugLog(L"WM_CREATE_INIT", st->chkSnappy);

        // anim init (snap, no boot-animation)
        SetWindowSubclass(st->chkSnappy, SnappyToggle_SubclassProc, 1, 0);
        SnappyToggle_StartAnim(st->chkSnappy, Settings_GetSnappyJoystick(), false);
        SetWindowSubclass(st->chkLastKeyPriority, SnappyToggle_SubclassProc, 1, 0);
        SnappyToggle_StartAnim(st->chkLastKeyPriority, Settings_GetLastKeyPriority(), false);
        SetWindowSubclass(st->chkBlockBoundKeys, SnappyToggle_SubclassProc, 1, 0);
        SnappyToggle_StartAnim(st->chkBlockBoundKeys, Settings_GetBlockBoundKeys(), false);
        Config_UpdateLkpSensitivityUi(st);
        Config_RefreshFromCurrentSettings(hWnd, st);

        LayoutConfigControls(hWnd, st);
        Config_SetCustomChildrenVisible(st, !st->customControls);
        Config_SetScrollY(hWnd, st, 0);
        Config_UpdateSparkCombos(hWnd, st);
        Config_MarkSurfaceDirty(hWnd, st);

        SetProfileStatus(st, L"");
        return 0;
    }

    case WM_SIZE:
        if (st)
        {
            int keepScroll = st->scrollY;
            if (keepScroll != 0 && !st->customControls)
            {
                // normalize current child coordinates back to "content space"
                Config_OffsetAllChildren(hWnd, keepScroll);
                st->scrollY = 0;
            }

            LayoutConfigControls(hWnd, st);
            Config_SetCustomChildrenVisible(st, !st->customControls);
            Config_SetScrollY(hWnd, st, keepScroll);
            Config_MarkSurfaceDirty(hWnd, st);
        }
        else
        {
            LayoutConfigControls(hWnd, st);
        }
        break;

    case WM_HSCROLL:
    {
        if (st && (HWND)lParam == st->sldLastKeyPrioritySensitivity)
        {
            int sv = (int)SendMessageW(st->sldLastKeyPrioritySensitivity, TBM_GETPOS, 0, 0);
            sv = std::clamp(sv, 1, 100);
            Settings_SetLastKeyPrioritySensitivity(Config_SliderToLkpSensitivity(sv));
            Config_UpdateLkpSensitivityUi(st);
            RequestSave(hWnd);
            Config_MarkSurfaceDirty(hWnd, st);
            return 0;
        }
        break;
    }

    case WM_MEASUREITEM:
    {
        MEASUREITEMSTRUCT* mis = (MEASUREITEMSTRUCT*)lParam;
        if (KeySettingsPanel_HandleMeasureItem(mis))
            return TRUE;
        break;
    }

    case WM_DRAWITEM:
    {
        const DRAWITEMSTRUCT* dis = (const DRAWITEMSTRUCT*)lParam;

        // 1) KeySettings panel controls
        if (KeySettingsPanel_HandleDrawItem(dis))
            return TRUE;

        // 2) Config toggles
        if (st && dis && dis->CtlType == ODT_BUTTON &&
            ((dis->CtlID == ID_SNAPPY && st->chkSnappy == dis->hwndItem) ||
             (dis->CtlID == ID_LAST_KEY_PRIORITY && st->chkLastKeyPriority == dis->hwndItem) ||
             (dis->CtlID == ID_BLOCK_BOUND_KEYS && st->chkBlockBoundKeys == dis->hwndItem)))
        {
            DrawSnappyToggleOwnerDraw(dis);
            return TRUE;
        }
        if (st && dis && dis->CtlType == ODT_BUTTON &&
            (dis->CtlID == ID_ANALOG_SELF_TEST && st->btnAnalogSelfTest == dis->hwndItem))
        {
            Layout_DrawFlatButton(dis);
            return TRUE;
        }
        break;
    }

    case WM_LBUTTONDOWN:
    {
        if (st)
        {
            Config_ClosePopupAnchors(st);
            if (Config_HandleSharedScroll(hWnd, st, msg, wParam, lParam) !=
                CustomPageScrollResult::NotHandled)
            {
                Config_MarkSurfaceDirty(hWnd, st);
                return 0;
            }
        }

        LPARAM lpAdj = Config_AdjustClientMouseLParamForScroll(st, lParam);
        if (Config_HandleCustomControlsMouse(hWnd, st, WM_LBUTTONDOWN, wParam, lpAdj))
            return 0;
        if (KeySettingsPanel_HandleCustomControlsMouse(hWnd, WM_LBUTTONDOWN, wParam, lpAdj))
        {
            Config_MarkSurfaceDirty(hWnd, st);
            return 0;
        }
        if (KeySettingsPanel_HandleMouse(hWnd, WM_LBUTTONDOWN, wParam, lpAdj))
        {
            Config_MarkSurfaceDirty(hWnd, st);
            return 0;
        }
        break;
    }

    case WM_MOUSEMOVE:
    {
        if (st && st->scroll.draggingThumb)
        {
            Config_HandleSharedScroll(hWnd, st, msg, wParam, lParam);
            return 0;
        }

        LPARAM lpAdj = Config_AdjustClientMouseLParamForScroll(st, lParam);
        if (Config_HandleCustomControlsMouse(hWnd, st, WM_MOUSEMOVE, wParam, lpAdj))
            return 0;
        if (KeySettingsPanel_HandleCustomControlsMouse(hWnd, WM_MOUSEMOVE, wParam, lpAdj))
        {
            Config_MarkSurfaceDirty(hWnd, st);
            return 0;
        }
        if (KeySettingsPanel_HandleMouse(hWnd, WM_MOUSEMOVE, wParam, lpAdj))
        {
            Config_MarkSurfaceDirty(hWnd, st);
            return 0;
        }
        break;
    }

    case WM_LBUTTONUP:
    {
        if (st && st->scroll.draggingThumb)
        {
            Config_HandleSharedScroll(hWnd, st, msg, wParam, lParam);
            Config_MarkSurfaceDirty(hWnd, st);
            return 0;
        }

        LPARAM lpAdj = Config_AdjustClientMouseLParamForScroll(st, lParam);
        if (Config_HandleCustomControlsMouse(hWnd, st, WM_LBUTTONUP, wParam, lpAdj))
            return 0;
        if (KeySettingsPanel_HandleCustomControlsMouse(hWnd, WM_LBUTTONUP, wParam, lpAdj))
        {
            Config_MarkSurfaceDirty(hWnd, st);
            return 0;
        }
        if (KeySettingsPanel_HandleMouse(hWnd, WM_LBUTTONUP, wParam, lpAdj))
        {
            Config_MarkSurfaceDirty(hWnd, st);
            return 0;
        }
        break;
    }

    case WM_MOUSEWHEEL:
    {
        LPARAM lpAdj = Config_AdjustWheelLParamForScroll(hWnd, st, lParam);
        if (KeySettingsPanel_HandleMouse(hWnd, msg, wParam, lpAdj))
        {
            Config_MarkSurfaceDirty(hWnd, st);
            return 0;
        }

        if (st)
        {
            Config_ClosePopupAnchors(st);
            Config_HandleSharedScroll(hWnd, st, msg, wParam, lParam);
            return 0;
        }
        break;
    }

    case WM_CAPTURECHANGED:
    {
        if (st)
        {
            Config_HandleSharedScroll(hWnd, st, msg, wParam, lParam);
            st->pressedCustomId = 0;
            st->dragCustomId = 0;
            Config_MarkSurfaceDirty(hWnd, st);
        }
        return 0;
    }

    case WM_SETCURSOR:
    {
        if (!st) break;
        if ((HWND)wParam != hWnd) break;

        POINT pt{};
        GetCursorPos(&pt);
        ScreenToClient(hWnd, &pt);

        RECT thumb = Config_GetScrollThumbRect(hWnd, st);
        RECT track = Config_GetScrollTrackRect(hWnd);
        int maxScroll = Config_GetMaxScroll(hWnd, st);

        if (maxScroll > 0 && (PtInRect(&thumb, pt) || PtInRect(&track, pt)))
        {
            SetCursor(LoadCursorW(nullptr, IDC_HAND));
            return TRUE;
        }
        break;
    }

    case WM_GETDLGCODE:
        return DLGC_WANTALLKEYS;

    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
    {
        // Profile actions are mouse-only. Explicit shortcut capture is handled
        // above, before the normal page switch; focused EDITs own text input.
        return 0;
    }

    case WM_COMMAND:
    {
        if (st) DeleteConfirm_Clear(hWnd, st);

        if (st && HIWORD(wParam) == CBN_SELCHANGE &&
            LOWORD(wParam) == (UINT)ID_SPARK_POLL_MODE && (HWND)lParam == st->cmbSparkPollMode)
        {
            Settings_SetSparkPollMode((UINT)std::clamp(PremiumCombo::GetCurSel(st->cmbSparkPollMode), 0, 2));
            RequestSave(hWnd);
            PremiumCombo::ShowDropDown(st->cmbSparkPollMode, false);
            ShowWindow(st->cmbSparkPollMode, SW_HIDE);
            Config_MarkSurfaceDirty(hWnd, st);
            return 0;
        }
        if (st && HIWORD(wParam) == CBN_SELCHANGE &&
            LOWORD(wParam) == (UINT)ID_SPARK_ROW_LIMIT && (HWND)lParam == st->cmbSparkRows)
        {
            Settings_SetSparkRowLimit((UINT)std::clamp(PremiumCombo::GetCurSel(st->cmbSparkRows), 0, 8));
            RequestSave(hWnd);
            PremiumCombo::ShowDropDown(st->cmbSparkRows, false);
            ShowWindow(st->cmbSparkRows, SW_HIDE);
            Config_MarkSurfaceDirty(hWnd, st);
            return 0;
        }

        if (LOWORD(wParam) == (UINT)ID_ANALOG_SELF_TEST && HIWORD(wParam) == BN_CLICKED && st)
        {
            if (!st->selfTestRunning)
                Config_StartSelfTest(hWnd, st);
            else
                Config_FinishSelfTest(hWnd, st);
            Config_MarkSurfaceDirty(hWnd, st);
            return 0;
        }

        // Snappy toggle
        if (LOWORD(wParam) == (UINT)ID_SNAPPY && HIWORD(wParam) == BN_CLICKED && st && st->chkSnappy)
        {
            SnappyDebugLog(L"WM_COMMAND_BN_CLICKED_BEFORE", st->chkSnappy, (int)LOWORD(wParam), (int)HIWORD(wParam));
            bool on = !Settings_GetSnappyJoystick();
            SendMessageW(st->chkSnappy, BM_SETCHECK, on ? BST_CHECKED : BST_UNCHECKED, 0);

            Settings_SetSnappyJoystick(on);
            SnappyToggle_StartAnim(st->chkSnappy, on, true);
            SnappyDebugLog(L"WM_COMMAND_BN_CLICKED_AFTER", st->chkSnappy, on ? 1 : 0, 0);

            RequestSave(hWnd);
            Config_MarkSurfaceDirty(hWnd, st);
            return 0;
        }

        if (LOWORD(wParam) == (UINT)ID_BLOCK_BOUND_KEYS && HIWORD(wParam) == BN_CLICKED && st && st->chkBlockBoundKeys)
        {
            bool on = !Settings_GetBlockBoundKeys();
            SendMessageW(st->chkBlockBoundKeys, BM_SETCHECK, on ? BST_CHECKED : BST_UNCHECKED, 0);
            Settings_SetBlockBoundKeys(on);
            if (!on) {
                Config_EndBlockShortcutCapture(hWnd, st);
                st->hotCustomId = st->pressedCustomId = 0;
            }
            SnappyToggle_StartAnim(st->chkBlockBoundKeys, on, true);
            RequestSave(hWnd);
            Config_RecalcContentHeight(hWnd, st);
            Config_MarkSurfaceDirty(hWnd, st);
            return 0;
        }

        if (LOWORD(wParam) == (UINT)ID_LAST_KEY_PRIORITY && HIWORD(wParam) == BN_CLICKED && st && st->chkLastKeyPriority)
        {
            bool on = !Settings_GetLastKeyPriority();
            SendMessageW(st->chkLastKeyPriority, BM_SETCHECK, on ? BST_CHECKED : BST_UNCHECKED, 0);
            Settings_SetLastKeyPriority(on);
            SnappyToggle_StartAnim(st->chkLastKeyPriority, on, true);
            Config_UpdateLkpSensitivityUi(st);
            RequestSave(hWnd);
            Config_MarkSurfaceDirty(hWnd, st);
            return 0;
        }

        // Preset selection:
        // If user selected "+ Create New..." row, start inline create (deferred).
        if (LOWORD(wParam) == (UINT)KSP_ID_PROFILE && HIWORD(wParam) == CBN_SELCHANGE)
        {
            HWND hCombo = (HWND)lParam;
            if (hCombo)
            {
                int sel = PremiumCombo::GetCurSel(hCombo);
                int cnt = PremiumCombo::GetCount(hCombo);
                if (cnt > 0 && sel == cnt - 1)
                {
                    PostMessageW(hWnd, WM_APP_PROFILE_BEGIN_CREATE, 0, 0);
                    return 0;
                }
            }
            // else: fallthrough to KeySettingsPanel_HandleCommand to apply preset
        }

        if (KeySettingsPanel_HandleCommand(hWnd, wParam, lParam))
        {
            KeySettingsPanel_CloseCustomPopups();
            if (st && st->customControls)
                Config_MarkSurfaceDirty(hWnd, st);
            else if (st && st->scrollY != 0)
                Config_RequestFullRepaint(hWnd);
            return 0;
        }

        return 0;
    }

    case WM_NCDESTROY:
        // FIX: free cached GDI resources used by graph renderer
        KeySettingsPanel_Shutdown();

        RemovePropW(hWnd, CONFIG_SCROLLY_PROP);

        if (st)
        {
            KillTimer(hWnd, ANALOG_SELF_TEST_TIMER_ID);
            st->selfTestRunning = false;
            Toast_Hide(hWnd, st);
            if (st->hToast)
            {
                DestroyWindow(st->hToast);
                st->hToast = nullptr;
            }

            if (st->chkSnappy && IsWindow(st->chkSnappy))
            {
                // subclass will free state on WM_NCDESTROY of the control, but best-effort safety:
                SnappyToggle_Free(st->chkSnappy);
            }
            if (st->chkBlockBoundKeys && IsWindow(st->chkBlockBoundKeys))
            {
                SnappyToggle_Free(st->chkBlockBoundKeys);
            }
            if (st->chkLastKeyPriority && IsWindow(st->chkLastKeyPriority))
            {
                SnappyToggle_Free(st->chkLastKeyPriority);
            }
            CustomPageSurface_Destroy(&st->surface);
            delete st;
            SetWindowLongPtrW(hWnd, GWLP_USERDATA, 0);
        }
        return 0;
    }

    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

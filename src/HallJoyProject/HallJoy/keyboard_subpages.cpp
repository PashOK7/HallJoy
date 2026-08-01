// keyboard_subpages.cpp
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <commctrl.h>
#include <string>
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
#include "keyboard_ui_state.h"
#include "keyboard_keysettings_panel.h"
#include "keyboard_keysettings_panel_internal.h"

#include "backend.h"
#include "gamepad_render.h"
#include "ui_theme.h"
#include "settings.h"
#include "realtime_loop.h"
#include "win_util.h"
#include "keyboard_profiles.h"
#include "premium_combo.h"
#include "keyboard_layout.h"
#include "settings_ini.h"
#include "profile_ini.h"
#include "app_paths.h"
#include "global_profiles.h"
#include "mouse_ipc.h"
#include "overlay_server.h"
#include "debug_log.h"
#include "custom_page_surface.h"
#include "custom_page_controls.h"

using namespace Gdiplus;
namespace fs = std::filesystem;

static constexpr UINT WM_APP_REQUEST_SAVE = WM_APP + 1;
static constexpr UINT WM_APP_APPLY_TIMING = WM_APP + 2;
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
#if defined(_DEBUG)
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
static void BeginDoubleBufferPaint(HWND hWnd, PAINTSTRUCT& ps, HDC& outMemDC, HBITMAP& outBmp, HGDIOBJ& outOldBmp)
{
    HDC hdc = BeginPaint(hWnd, &ps);
    RECT rc{};
    GetClientRect(hWnd, &rc);
    outMemDC = CreateCompatibleDC(hdc);
    outBmp = CreateCompatibleBitmap(hdc, rc.right - rc.left, rc.bottom - rc.top);
    outOldBmp = SelectObject(outMemDC, outBmp);
    FillRect(outMemDC, &rc, UiTheme::Brush_PanelBg());
}

static void EndDoubleBufferPaint(HWND hWnd, PAINTSTRUCT& ps, HDC memDC, HBITMAP bmp, HGDIOBJ oldBmp)
{
    HDC hdc = ps.hdc;
    RECT rc{};
    GetClientRect(hWnd, &rc);
    BitBlt(hdc, 0, 0, rc.right - rc.left, rc.bottom - rc.top, memDC, 0, 0, SRCCOPY);
    SelectObject(memDC, oldBmp);
    DeleteObject(bmp);
    DeleteDC(memDC);
    EndPaint(hWnd, &ps);
}

// ============================================================================
// Gamepad Tester page (DPI-scaled)
// ============================================================================
LRESULT CALLBACK KeyboardSubpages_TesterPageProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_ERASEBKGND: return 1;

    case WM_PAINT:
    {
        PAINTSTRUCT ps{};
        HDC memDC = nullptr;
        HBITMAP bmp = nullptr;
        HGDIOBJ oldBmp = nullptr;
        BeginDoubleBufferPaint(hWnd, ps, memDC, bmp, oldBmp);

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
        bool showAnalogInfo = analog.sdkInitialised || analog.sparkConnected || analog.sayoConnected ||
            analog.mad68Present || analog.hex80Present || analog.addressedPresent ||
            analog.nativeProtocolCount > 0;
        int analogInfoH = showAnalogInfo ? S(hWnd, 72) : 0;
        int availW = std::max(1, clientW - margin * 2 - cardGap * (cols - 1));
        int availH = std::max(1, clientH - margin * 2 - analogInfoH - (showAnalogInfo ? cardGap : 0) - cardGap * (rows - 1));
        int cardW = std::max(1, availW / cols);
        int cardH = std::max(1, availH / rows);

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
            RECT info{ margin, margin, clientW - margin, margin + analogInfoH };
            FillRect(memDC, &info, UiTheme::Brush_ControlBg());
            Rectangle(memDC, info.left, info.top, info.right, info.bottom);

            int x = info.left + S(hWnd, 10);
            int y = info.top + S(hWnd, 8);
            int lineH = S(hWnd, 16);
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

        for (int pad = 0; pad < padCount; ++pad)
        {
            int col = pad % cols;
            int row = pad / cols;
            int left = margin + col * (cardW + cardGap);
            int top = margin + analogInfoH + (showAnalogInfo ? cardGap : 0) + row * (cardH + cardGap);

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

        SelectObject(memDC, oldFont);
        SelectObject(memDC, oldBrushGlobal);
        SelectObject(memDC, oldPenGlobal);
        DeleteObject(cardPen);
        EndDoubleBufferPaint(hWnd, ps, memDC, bmp, oldBmp);
        return 0;
    }
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

static void OverlayPage_SetClipboardText(HWND hWnd, const std::wstring& text)
{
    if (!OpenClipboard(hWnd))
        return;
    EmptyClipboard();
    size_t bytes = (text.size() + 1u) * sizeof(wchar_t);
    HGLOBAL mem = GlobalAlloc(GMEM_MOVEABLE, bytes);
    if (mem)
    {
        void* dst = GlobalLock(mem);
        if (dst)
        {
            memcpy(dst, text.c_str(), bytes);
            GlobalUnlock(mem);
            SetClipboardData(CF_UNICODETEXT, mem);
            mem = nullptr;
        }
    }
    if (mem)
        GlobalFree(mem);
    CloseClipboard();
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
    Checkbox,
    Slider,
    Chip,
    Edit,
    Hue,
    ColorPreview,
    Hint
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
    int scrollY = 0;
    int contentHeight = 0;
    int hotId = 0;
    int pressedId = 0;
    int focusId = 0;
    int dragId = 0;
    int colorDragMode = 0; // 1 = saturation/value square, 2 = hue strip
    bool scrollDrag = false;
    int scrollDragGrabOffsetY = 0;
    int scrollDragThumbHeight = 0;
    int scrollDragMax = 0;
    std::wstring portText;
    std::wstring hexText;
    std::wstring labelHexText;
    OverlayColorBitmapCache indicatorColorCache;
    OverlayColorBitmapCache labelColorCache;
    OverlayColorUiState indicatorColorUi;
    OverlayColorUiState labelColorUi;
};

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

static uint16_t OverlayCustom_GetPort(OverlayCustomState* st)
{
    if (!st) return OverlayServer_GetConfiguredPort();
    wchar_t* end = nullptr;
    unsigned long port = wcstoul(st->portText.c_str(), &end, 10);
    if (port < 1 || port > 65535)
        return OverlayServer_GetConfiguredPort();
    return (uint16_t)port;
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

    bool compactTop = w < portW + gap + directionW + gap + depthW;
    OverlayCustom_AddItem(st, 2, OverlayCustomKind::Label, RECT{ x, y, x + portW, y + labelH }, L"Port");
    if (!compactTop)
    {
        OverlayCustom_AddItem(st, 3, OverlayCustomKind::Label, RECT{ x + portW + gap, y, x + portW + gap + directionW, y + labelH }, L"Fill direction");
        OverlayCustom_AddItem(st, 4, OverlayCustomKind::Label, RECT{ x + portW + gap + directionW + gap, y, x + portW + gap + directionW + gap + depthW, y + labelH }, L"Depth display");
        y += labelH + S(hWnd, 6);
        OverlayCustom_AddItem(st, OVERLAY_ID_PORT, OverlayCustomKind::Edit, RECT{ x, y, x + portW, y + editH }, st->portText);
        st->items.back().enabled = !OverlayServer_IsRunning();
        OverlayCustom_AddItem(st, OVERLAY_ID_DIRECTION, OverlayCustomKind::Button, RECT{ x + portW + gap, y, x + portW + gap + directionW, y + editH },
            OverlayServer_GetFillDirection() == OverlayFillDirection::TopDown ? L"Top to bottom" : L"Bottom to top");
        OverlayCustom_AddItem(st, OVERLAY_ID_DEPTH_SOURCE, OverlayCustomKind::Button, RECT{ x + portW + gap + directionW + gap, y, x + portW + gap + directionW + gap + depthW, y + editH },
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
        OverlayCustom_AddItem(st, OVERLAY_ID_DIRECTION, OverlayCustomKind::Button, RECT{ x, y, x + directionW, y + editH },
            OverlayServer_GetFillDirection() == OverlayFillDirection::TopDown ? L"Top to bottom" : L"Bottom to top");
        y += editH + S(hWnd, 8);
        OverlayCustom_AddItem(st, 4, OverlayCustomKind::Label, RECT{ x, y, x + depthW, y + labelH }, L"Depth display");
        y += labelH + S(hWnd, 6);
        OverlayCustom_AddItem(st, OVERLAY_ID_DEPTH_SOURCE, OverlayCustomKind::Button, RECT{ x, y, x + depthW, y + editH },
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
    OverlayCustom_AddItem(st, OVERLAY_ID_LABEL_FONT, OverlayCustomKind::Button, RECT{ x, y, x + styleBtnW, y + editH }, OverlayCustom_LabelFontName(OverlayServer_GetLabelFontIndex()));
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

    OverlayCustom_AddItem(st, 9, OverlayCustomKind::Label, RECT{ x, y, x + w, y + labelH }, L"OBS browser source URL");
    y += labelH + S(hWnd, 6);
    OverlayCustom_AddItem(st, 10, OverlayCustomKind::Label, RECT{ x, y, x + w, y + valueH }, OverlayCustom_BuildUrl(st));
    y += valueH + rowGap;

    OverlayCustom_AddItem(st, 11, OverlayCustomKind::Label, RECT{ x, y, x + w, y + labelH }, L"Server status");
    y += labelH + S(hWnd, 6);
    std::wstring status = OverlayServer_IsRunning() ? L"Running" : L"Stopped";
    std::wstring err = OverlayServer_GetLastError();
    if (!err.empty()) status += L" (" + err + L")";
    OverlayCustom_AddItem(st, 12, OverlayCustomKind::Label, RECT{ x, y, x + w, y + valueH }, status);
    y += valueH + rowGap;

    if (w >= btnW * 3 + gap * 2)
    {
        OverlayCustom_AddItem(st, OVERLAY_ID_TOGGLE, OverlayCustomKind::Button, RECT{ x, y, x + btnW, y + btnH }, OverlayServer_IsRunning() ? L"Stop server" : L"Start server");
        OverlayCustom_AddItem(st, OVERLAY_ID_OPEN, OverlayCustomKind::Button, RECT{ x + btnW + gap, y, x + btnW * 2 + gap, y + btnH }, L"Open");
        st->items.back().enabled = OverlayServer_IsRunning();
        OverlayCustom_AddItem(st, OVERLAY_ID_COPY, OverlayCustomKind::Button, RECT{ x + (btnW + gap) * 2, y, x + (btnW + gap) * 2 + btnW, y + btnH }, L"Copy URL");
        y += btnH + rowGap;
    }
    else
    {
        int narrowW = std::min(w, S(hWnd, 220));
        OverlayCustom_AddItem(st, OVERLAY_ID_TOGGLE, OverlayCustomKind::Button, RECT{ x, y, x + narrowW, y + btnH }, OverlayServer_IsRunning() ? L"Stop server" : L"Start server");
        y += btnH + S(hWnd, 8);
        OverlayCustom_AddItem(st, OVERLAY_ID_OPEN, OverlayCustomKind::Button, RECT{ x, y, x + narrowW, y + btnH }, L"Open");
        st->items.back().enabled = OverlayServer_IsRunning();
        y += btnH + S(hWnd, 8);
        OverlayCustom_AddItem(st, OVERLAY_ID_COPY, OverlayCustomKind::Button, RECT{ x, y, x + narrowW, y + btnH }, L"Copy URL");
        y += btnH + rowGap;
    }

    OverlayCustom_AddItem(st, 13, OverlayCustomKind::Hint, RECT{ x, y, x + w, y + S(hWnd, 52) },
        L"Add the URL to OBS as a Browser Source. The page renders the current HallJoy keyboard layout and HE analog depth.");
    y += S(hWnd, 52) + margin;

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
    case OverlayCustomKind::Label:
        OverlayCustom_DrawText(hdc, it.text, rc, text, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        break;
    case OverlayCustomKind::Hint:
        OverlayCustom_DrawText(hdc, it.text, rc, UiTheme::Color_TextMuted(), DT_LEFT | DT_TOP | DT_WORDBREAK | DT_END_ELLIPSIS);
        break;
    case OverlayCustomKind::Button:
        CustomPage_DrawButton(g, hdc, rc, it.text, hot, pressed, it.enabled);
        break;
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
        OverlayCustom_DrawRoundRect(g, rc, UiTheme::Color_ControlBg(), focused ? UiTheme::Color_Accent() : UiTheme::Color_Border(), 4.0f, it.enabled ? 255 : 145);
        {
            RECT trc = rc;
            InflateRect(&trc, -S(hWnd, 8), 0);
            const std::wstring& editText = (it.id == OVERLAY_ID_PORT) ? st->portText :
                ((it.id == OVERLAY_ID_LABEL_COLOR_HEX) ? st->labelHexText : st->hexText);
            OverlayCustom_DrawText(hdc, editText, trc, text, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
            if (focused && it.enabled)
            {
                SIZE sz{};
                const std::wstring& s = editText;
                GetTextExtentPoint32W(hdc, s.c_str(), (int)s.size(), &sz);
                int cx = std::min(trc.left + sz.cx + 1, trc.right - 2);
                HPEN caret = CreatePen(PS_SOLID, 1, UiTheme::Color_Text());
                HGDIOBJ old = SelectObject(hdc, caret);
                MoveToEx(hdc, cx, rc.top + S(hWnd, 6), nullptr);
                LineTo(hdc, cx, rc.bottom - S(hWnd, 6));
                SelectObject(hdc, old);
                DeleteObject(caret);
            }
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

    HGDIOBJ oldFont = SelectObject(hdc, GetStockObject(DEFAULT_GUI_FONT));
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
    CustomPageSurface_DrawScrollbar(hWnd, hdc, &st->surface, st->scrollDrag);
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
            OverlayServer_Start(OverlayCustom_GetPort(st));
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
        OverlayPage_SetClipboardText(hWnd, OverlayCustom_BuildUrl(st));
        break;
    case OVERLAY_ID_DIRECTION:
        OverlayServer_SetFillDirection(OverlayServer_GetFillDirection() == OverlayFillDirection::TopDown
            ? OverlayFillDirection::BottomUp
            : OverlayFillDirection::TopDown);
        break;
    case OVERLAY_ID_DEPTH_SOURCE:
        OverlayServer_SetUseRawDepth(!OverlayServer_GetUseRawDepth());
        break;
    case OVERLAY_ID_LABEL_FONT:
        OverlayServer_SetLabelFontIndex((OverlayServer_GetLabelFontIndex() + 1) % 13);
        break;
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

static LRESULT OverlayCustom_PageProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    auto* st = (OverlayCustomState*)GetWindowLongPtrW(hWnd, GWLP_USERDATA);
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
        OverlayCustom_RebuildLayout(hWnd, st);
        return 0;
    }

    case WM_NCDESTROY:
        OverlayCustom_DestroyCache(st);
        delete st;
        SetWindowLongPtrW(hWnd, GWLP_USERDATA, 0);
        return 0;

    case WM_SIZE:
        OverlayCustom_RebuildLayout(hWnd, st);
        InvalidateRect(hWnd, nullptr, FALSE);
        return 0;

    case WM_SHOWWINDOW:
        if (wParam && st)
        {
            st->hexText = OverlayPage_FormatHex(OverlayServer_GetAccentColor());
            st->labelHexText = OverlayPage_FormatHex(OverlayServer_GetLabelColor());
            OverlayCustom_RebuildLayout(hWnd, st);
            InvalidateRect(hWnd, nullptr, FALSE);
        }
        return 0;

    case WM_PAINT:
    {
        uint64_t paintStart = CustomPageSurface_QpcNow();
        PAINTSTRUCT ps{};
        HDC memDC = nullptr;
        HBITMAP bmp = nullptr;
        HGDIOBJ oldBmp = nullptr;
        BeginDoubleBufferPaint(hWnd, ps, memDC, bmp, oldBmp);
        RECT rc{};
        GetClientRect(hWnd, &rc);
        FillRect(memDC, &rc, UiTheme::Brush_PanelBg());
        Graphics g(memDC);
        g.SetSmoothingMode(SmoothingModeAntiAlias);
        g.SetPixelOffsetMode(PixelOffsetModeHighQuality);
        g.SetTextRenderingHint(TextRenderingHintClearTypeGridFit);
        if (st)
        {
            if (OverlayCustom_RenderContentCache(hWnd, memDC, st))
            {
                HDC cacheDC = CreateCompatibleDC(memDC);
                if (cacheDC)
                {
                    HGDIOBJ old = SelectObject(cacheDC, st->surface.contentCache);
                    int copyW = std::min((int)(rc.right - rc.left), st->surface.cacheWidth);
                    int copyH = std::min((int)(rc.bottom - rc.top), std::max(0, st->surface.cacheHeight - st->scrollY));
                    if (copyW > 0 && copyH > 0)
                        BitBlt(memDC, 0, 0, copyW, copyH, cacheDC, 0, st->scrollY, SRCCOPY);
                    SelectObject(cacheDC, old);
                    DeleteDC(cacheDC);
                }
            }
            else
            {
                HGDIOBJ oldFont = SelectObject(memDC, GetStockObject(DEFAULT_GUI_FONT));
                for (const auto& it : st->items)
                    OverlayCustom_DrawItem(hWnd, memDC, g, st, it);
                SelectObject(memDC, oldFont);
            }
            OverlayCustom_DrawScrollbar(hWnd, memDC, g, st);
        }
        EndDoubleBufferPaint(hWnd, ps, memDC, bmp, oldBmp);
        if (st && st->surface.scrollSampleStartMs != 0)
        {
            CustomPageSurface_BeginPaintSample(&st->surface, paintStart);
            CustomPageSurface_MaybeLogScrollPerf(&st->surface, L"ui.overlay.scroll");
        }
        return 0;
    }

    case WM_SETCURSOR:
        if (LOWORD(lParam) == HTCLIENT)
        {
            SetCursor(LoadCursorW(nullptr, IDC_ARROW));
            return TRUE;
        }
        break;

    case WM_MOUSEMOVE:
    {
        if (!st) break;
        POINT pt{ (short)LOWORD(lParam), (short)HIWORD(lParam) };
        if (st->scrollDrag)
        {
            RECT track = OverlayCustom_GetScrollTrackRect(hWnd);
            int thumbH = std::max(1, st->scrollDragThumbHeight);
            int travel = std::max(1, (int)(track.bottom - track.top) - thumbH);
            int maxScroll = std::max(1, st->scrollDragMax);
            int topWanted = pt.y - st->scrollDragGrabOffsetY;
            int topClamped = std::clamp(topWanted, (int)track.top, (int)track.bottom - thumbH);
            double t = (double)(topClamped - track.top) / (double)travel;
            OverlayCustom_SetScrollY(hWnd, st, (int)std::lround(t * (double)maxScroll));
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

    case WM_LBUTTONDOWN:
    {
        if (!st) break;
        SetFocus(hWnd);
        POINT pt{ (short)LOWORD(lParam), (short)HIWORD(lParam) };
        RECT thumb = OverlayCustom_GetScrollThumbRect(hWnd, st);
        RECT track = OverlayCustom_GetScrollTrackRect(hWnd);
        int maxScroll = OverlayCustom_GetMaxScroll(hWnd, st);
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
            OverlayCustom_SetScrollY(hWnd, st, pt.y < thumb.top ? st->scrollY - page : st->scrollY + page);
            return 0;
        }

        OverlayCustomItem* hit = OverlayCustom_HitTest(st, pt);
        st->focusId = 0;
        if (hit && hit->enabled)
        {
            st->pressedId = hit->id;
            if (hit->kind == OverlayCustomKind::Edit)
                st->focusId = hit->id;
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
        bool wasDrag = st->dragId != 0 || st->scrollDrag;
        st->pressedId = 0;
        st->dragId = 0;
        st->colorDragMode = 0;
        st->scrollDrag = false;
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
            st->pressedId = 0;
            st->dragId = 0;
            st->colorDragMode = 0;
            st->scrollDrag = false;
            OverlayCustom_MarkCacheDirty(hWnd, st);
        }
        return 0;

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
            OverlayCustom_SetScrollY(hWnd, st, st->scrollY - notches * step);
            return 0;
        }
        break;

    case WM_CHAR:
        if (st && (st->focusId == OVERLAY_ID_PORT || st->focusId == OVERLAY_ID_COLOR_HEX || st->focusId == OVERLAY_ID_LABEL_COLOR_HEX))
        {
            std::wstring& s = (st->focusId == OVERLAY_ID_PORT) ? st->portText :
                ((st->focusId == OVERLAY_ID_LABEL_COLOR_HEX) ? st->labelHexText : st->hexText);
            if (wParam == VK_BACK)
            {
                if (!s.empty())
                    s.pop_back();
            }
            else if (wParam == 22) // Ctrl+V
            {
                if (OpenClipboard(hWnd))
                {
                    HANDLE h = GetClipboardData(CF_UNICODETEXT);
                    const wchar_t* txt = h ? (const wchar_t*)GlobalLock(h) : nullptr;
                    if (txt)
                    {
                        for (const wchar_t* p = txt; *p; ++p)
                        {
                            wchar_t ch = *p;
                            if (st->focusId == OVERLAY_ID_PORT)
                            {
                                if (iswdigit(ch) && s.size() < 5) s.push_back(ch);
                            }
                            else
                            {
                                if ((iswxdigit(ch) || ch == L'#') && s.size() < 7) s.push_back(towupper(ch));
                            }
                        }
                        GlobalUnlock(h);
                    }
                    CloseClipboard();
                }
            }
            else if (wParam >= 32)
            {
                wchar_t ch = (wchar_t)wParam;
                if (st->focusId == OVERLAY_ID_PORT)
                {
                    if (iswdigit(ch) && s.size() < 5) s.push_back(ch);
                    OverlayServer_SetConfiguredPort(OverlayCustom_GetPort(st));
                }
                else
                {
                    if ((iswxdigit(ch) || ch == L'#') && s.size() < 7)
                        s.push_back(towupper(ch));
                    uint32_t color = 0;
                    if (OverlayPage_ParseHex(s, &color))
                    {
                        if (st->focusId == OVERLAY_ID_LABEL_COLOR_HEX)
                        {
                            OverlayServer_SetLabelColor(color);
                            OverlayCustom_SyncColorUiFromRgb(st->labelColorUi, color);
                        }
                        else
                        {
                            OverlayServer_SetAccentColor(color);
                            OverlayCustom_SyncColorUiFromRgb(st->indicatorColorUi, color);
                        }
                    }
                }
            }
            if (st->focusId == OVERLAY_ID_PORT)
                OverlayServer_SetConfiguredPort(OverlayCustom_GetPort(st));
            else
            {
                uint32_t color = 0;
                if (OverlayPage_ParseHex(s, &color))
                {
                    if (st->focusId == OVERLAY_ID_LABEL_COLOR_HEX)
                    {
                        OverlayServer_SetLabelColor(color);
                        OverlayCustom_SyncColorUiFromRgb(st->labelColorUi, color);
                    }
                    else
                    {
                        OverlayServer_SetAccentColor(color);
                        OverlayCustom_SyncColorUiFromRgb(st->indicatorColorUi, color);
                    }
                }
            }
            OverlayCustom_RebuildLayout(hWnd, st);
            OverlayCustom_RequestSave(hWnd);
            InvalidateRect(hWnd, nullptr, FALSE);
            return 0;
        }
        break;

    case WM_KEYDOWN:
        if (st && wParam == VK_ESCAPE)
        {
            st->focusId = 0;
            OverlayCustom_MarkCacheDirty(hWnd, st);
            return 0;
        }
        break;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

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
struct LayoutPageState
{
    HWND lblPreset = nullptr;
    HWND cmbPreset = nullptr;
    HWND btnAdd = nullptr;
    HWND btnDelete = nullptr;
    HWND btnReset = nullptr;
    HWND btnSave = nullptr;
    HWND btnUniformSpacing = nullptr;
    HWND lblUniformGap = nullptr;
    HWND edtUniformGap = nullptr;
    HWND lblLabel = nullptr;
    HWND edtLabel = nullptr;
    HWND btnApplyLabel = nullptr;
    HWND btnBindKey = nullptr;
    HWND lblPos = nullptr;
    HWND edtPos = nullptr;
    HWND lblWidth = nullptr;
    HWND edtWidth = nullptr;
    HWND lblHeight = nullptr;
    HWND edtHeight = nullptr;
    HWND lblBindState = nullptr;
    HWND lblKeys = nullptr;
    HWND lstKeys = nullptr;
    HWND lblHint = nullptr;

    int selectedIdx = -1;
    bool dragging = false;
    bool dirty = false;
    bool hasUnsaved = false;
    bool bindArmed = false;
    int dragOffsetX = 0;
    int dragOffsetY = 0;
    RECT canvasRc{};
    int editingPresetIdx = 0;
    std::vector<KeyDef> draftKeys;
    std::vector<std::wstring> draftLabels;
    uint32_t previewHash = 0;
    bool uniformSpacingEnabled = false;
    int uniformSpacingGap = 8;
};

static constexpr int ID_LAYOUT_PRESET = 8111;
static constexpr int ID_LAYOUT_RESET = 8112;
static constexpr int ID_LAYOUT_KEYS = 8113;
static constexpr int ID_LAYOUT_SAVE = 8114;
static constexpr int ID_LAYOUT_ADD = 8115;
static constexpr int ID_LAYOUT_DELETE = 8116;
static constexpr int ID_LAYOUT_LABEL_EDIT = 8117;
static constexpr int ID_LAYOUT_LABEL_APPLY = 8118;
static constexpr int ID_LAYOUT_BIND_KEY = 8119;
static constexpr int ID_LAYOUT_UNIFORM_SPACING = 8120;
static constexpr int ID_LAYOUT_POS_EDIT = 8122;
static constexpr int ID_LAYOUT_WIDTH_EDIT = 8123;
static constexpr int ID_LAYOUT_UNIFORM_GAP_EDIT = 8124;
static constexpr int ID_LAYOUT_HEIGHT_EDIT = 8125;
static constexpr UINT_PTR ID_LAYOUT_UI_TIMER = 8121;

static bool Layout_NudgeSelectedKey(HWND hWnd, LayoutPageState* st, int dRow, int dX, int dW);
static void Layout_SetUnsaved(LayoutPageState* st, bool on);
static void Layout_RefreshKeyList(HWND hWnd, LayoutPageState* st);
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

static void Layout_SetWindowTextIfChanged(HWND hWnd, const wchar_t* text)
{
    if (!hWnd || !IsWindow(hWnd)) return;
    const wchar_t* target = text ? text : L"";
    wchar_t cur[256]{};
    GetWindowTextW(hWnd, cur, (int)(sizeof(cur) / sizeof(cur[0])));
    if (wcscmp(cur, target) != 0)
        SetWindowTextW(hWnd, target);
}

static void Layout_UpdateUniformSpacingButton(LayoutPageState* st)
{
    if (!st || !st->btnUniformSpacing) return;
    Layout_SetWindowTextIfChanged(
        st->btnUniformSpacing,
        st->uniformSpacingEnabled ? L"Uniform Spacing: ON" : L"Uniform Spacing: OFF");
}

static void Layout_UpdateHintText(LayoutPageState* st)
{
    if (!st || !st->lblHint) return;
    if (st->uniformSpacingEnabled)
    {
        Layout_SetWindowTextIfChanged(st->lblHint,
            L"Select a key and drag to move it.\nMouse wheel changes width (Shift = x4).\nUniform Spacing: drag reorders keys in row. Click Save Changes to apply.");
    }
    else
    {
        Layout_SetWindowTextIfChanged(st->lblHint,
            L"Select a key and drag to move it.\nMouse wheel changes width (Shift = x4).\nClick Save Changes to apply.");
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

    wchar_t b[32]{};
    GetWindowTextW(st->edtUniformGap, b, (int)(sizeof(b) / sizeof(b[0])));
    int v = 0;
    if (swscanf_s(b, L"%d", &v) != 1)
        v = st->uniformSpacingGap;
    v = std::clamp(v, 0, 120);

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
    Layout_RefreshKeyList(hWnd, st);
    st->previewHash = Layout_ComputePreviewHash(st);
    if (clearUnsaved)
        Layout_SetUnsaved(st, false);
    InvalidateRect(hWnd, nullptr, FALSE);
    return true;
}

static void Layout_SetUnsaved(LayoutPageState* st, bool on)
{
    if (!st) return;
    st->hasUnsaved = on;
    if (st->btnSave && IsWindow(st->btnSave))
        EnableWindow(st->btnSave, on ? TRUE : FALSE);
    if (st->btnDelete && IsWindow(st->btnDelete))
        EnableWindow(st->btnDelete, st->selectedIdx >= 0 ? TRUE : FALSE);
}

static void Layout_UpdateMetaControls(LayoutPageState* st)
{
    if (!st) return;
    bool hasSel = (st->selectedIdx >= 0);
    bool labelHasFocus = (st->edtLabel && GetFocus() == st->edtLabel);
    bool posHasFocus = (st->edtPos && GetFocus() == st->edtPos);
    bool widthHasFocus = (st->edtWidth && GetFocus() == st->edtWidth);
    bool heightHasFocus = (st->edtHeight && GetFocus() == st->edtHeight);
    bool uniformGapHasFocus = (st->edtUniformGap && GetFocus() == st->edtUniformGap);

    if (st->btnDelete && IsWindow(st->btnDelete))
        EnableWindow(st->btnDelete, hasSel ? TRUE : FALSE);
    if (st->btnApplyLabel && IsWindow(st->btnApplyLabel))
        EnableWindow(st->btnApplyLabel, hasSel ? TRUE : FALSE);
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
        if (st->edtLabel && !labelHasFocus)
            Layout_SetWindowTextIfChanged(st->edtLabel, (k.label && k.label[0]) ? k.label : L"");
        if (st->edtPos && !posHasFocus)
        {
            wchar_t b[32]{};
            swprintf_s(b, L"%d", k.x);
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
            swprintf_s(s, L"HID: %u  Raw: %u", (unsigned)k.hid, (unsigned)BackendUI_GetRawMilli(k.hid));
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
    swprintf_s(s, L"HID: %u  Raw: %u", (unsigned)k.hid, (unsigned)BackendUI_GetRawMilli(k.hid));
    Layout_SetWindowTextIfChanged(st->lblBindState, s);
}

static uint32_t Layout_ComputePreviewHash(LayoutPageState* st)
{
    if (!st) return 0;
    uint32_t h = 2166136261u;
    for (const KeyDef& k : st->draftKeys)
    {
        if (k.hid == 0 || k.hid >= 256)
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

    st->draftLabels[st->selectedIdx] = (txt[0] ? txt : L"Key");
    Layout_RebindDraftLabels(st);
    Layout_RefreshKeyList(hWnd, st);
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
        wchar_t b[32]{};
        GetWindowTextW(st->edtPos, b, (int)(sizeof(b) / sizeof(b[0])));
        int v = 0;
        if (swscanf_s(b, L"%d", &v) == 1)
        {
            v = std::clamp(v, 0, 4000);
            if (k.x != v)
            {
                k.x = v;
                changed = true;
            }
        }
    }

    if (applyWidth && st->edtWidth)
    {
        wchar_t b[32]{};
        GetWindowTextW(st->edtWidth, b, (int)(sizeof(b) / sizeof(b[0])));
        int v = 0;
        if (swscanf_s(b, L"%d", &v) == 1)
        {
            v = std::clamp(v, KEYBOARD_KEY_MIN_DIM, KEYBOARD_KEY_MAX_DIM);
            if (k.w != v)
            {
                k.w = v;
                changed = true;
            }
        }
    }

    if (applyHeight && st->edtHeight)
    {
        wchar_t b[32]{};
        GetWindowTextW(st->edtHeight, b, (int)(sizeof(b) / sizeof(b[0])));
        int v = 0;
        if (swscanf_s(b, L"%d", &v) == 1)
        {
            v = std::clamp(v, KEYBOARD_KEY_MIN_DIM, KEYBOARD_KEY_MAX_DIM);
            if (k.h != v)
            {
                k.h = v;
                changed = true;
            }
        }
    }

    if (changed)
    {
        Layout_RefreshKeyList(hWnd, st);
        Layout_SetUnsaved(st, true);
        InvalidateRect(hWnd, &st->canvasRc, FALSE);
    }
    Layout_UpdateMetaControls(st);
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

static void Layout_ComputeCanvasRect(HWND hWnd, LayoutPageState* st)
{
    RECT rc{};
    GetClientRect(hWnd, &rc);

    int margin = S(hWnd, 12);
    int leftW = S(hWnd, 300);
    int topY = S(hWnd, 56);

    st->canvasRc.left = margin + leftW + S(hWnd, 12);
    st->canvasRc.top = topY;
    st->canvasRc.right = rc.right - margin;
    st->canvasRc.bottom = rc.bottom - margin;
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

    int maxX = 1;
    int maxBottom = KEYBOARD_KEY_H;
    for (size_t i = 0; i < st->draftKeys.size(); ++i)
    {
        const KeyDef& k = st->draftKeys[i];
        int x = k.x;
        if (pDisplayX && i < pDisplayX->size())
            x = (*pDisplayX)[i];
        maxX = std::max(maxX, x + k.w);
        maxBottom = std::max(maxBottom, k.row * KEYBOARD_ROW_PITCH_Y + std::max(KEYBOARD_KEY_MIN_DIM, k.h));
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
    int y = (int)std::lround(oy + (KEYBOARD_MARGIN_Y + k.row * KEYBOARD_ROW_PITCH_Y) * scale);
    int w = std::max(10, (int)std::lround(k.w * scale));
    int h = std::max(10, (int)std::lround(std::max(KEYBOARD_KEY_MIN_DIM, k.h) * scale));
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
        if (PtInRect(&r, pt)) return i;
    }
    return -1;
}

static void Layout_RefreshKeyList(HWND hWnd, LayoutPageState* st)
{
    if (!st || !st->lstKeys) return;
    SendMessageW(st->lstKeys, LB_RESETCONTENT, 0, 0);

    int n = Layout_DraftCount(st);
    for (int i = 0; i < n; ++i)
    {
        KeyDef k{};
        if (!Layout_DraftGet(st, i, k)) continue;

        wchar_t line[256]{};
        swprintf_s(line, L"%2d. %-7ls HID:%3u  row:%d x:%d w:%d h:%d", i + 1,
            (k.label ? k.label : L""), (unsigned)k.hid, k.row, k.x, k.w, std::max(KEYBOARD_KEY_MIN_DIM, k.h));
        SendMessageW(st->lstKeys, LB_ADDSTRING, 0, (LPARAM)line);
    }

    if (st->selectedIdx >= n) st->selectedIdx = -1;
    if (st->selectedIdx >= 0)
        SendMessageW(st->lstKeys, LB_SETCURSEL, (WPARAM)st->selectedIdx, 0);
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

    Graphics g(hdc);
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    g.SetPixelOffsetMode(PixelOffsetModeHighQuality);
    g.SetTextRenderingHint(TextRenderingHintAntiAliasGridFit);

    RectF canvas((REAL)st->canvasRc.left, (REAL)st->canvasRc.top,
        (REAL)(st->canvasRc.right - st->canvasRc.left), (REAL)(st->canvasRc.bottom - st->canvasRc.top));

    SolidBrush bg(Gp(RGB(28, 28, 30)));
    g.FillRectangle(&bg, canvas);

    // subtle grid helps alignment while dragging keys
    {
        Pen rowPen(Gp(RGB(70, 70, 76), 110), 1.0f);
        Pen colPen(Gp(RGB(56, 56, 60), 80), 1.0f);
        const int step = std::max(8, S(hWnd, 12));
        int x0 = (int)canvas.X;
        int y0 = (int)canvas.Y;
        int x1 = (int)canvas.GetRight();
        int y1 = (int)canvas.GetBottom();

        for (int y = y0; y <= y1; y += step)
            g.DrawLine(&rowPen, (REAL)x0, (REAL)y, (REAL)x1, (REAL)y);
        for (int x = x0; x <= x1; x += step)
            g.DrawLine(&colPen, (REAL)x, (REAL)y0, (REAL)x, (REAL)y1);
    }

    Pen border(Gp(UiTheme::Color_Border()), 1.0f);
    g.DrawRectangle(&border, canvas);

    std::vector<int> displayX;
    Layout_BuildDisplayXMap(st, displayX);
    const std::vector<int>* pDisplayX = displayX.empty() ? nullptr : &displayX;
    float scale = 1.0f, ox = 0.0f, oy = 0.0f;
    Layout_ComputeTransform(st, st->canvasRc, pDisplayX, scale, ox, oy);

    int n = Layout_DraftCount(st);
    for (int i = 0; i < n; ++i)
    {
        KeyDef k{};
        if (!Layout_DraftGet(st, i, k)) continue;

        RECT rr = Layout_KeyRectOnCanvasFast(st, i, pDisplayX, scale, ox, oy);
        RectF r((REAL)rr.left, (REAL)rr.top, (REAL)(rr.right - rr.left), (REAL)(rr.bottom - rr.top));
        r.Inflate(-1.0f, -1.0f);

        bool sel = (i == st->selectedIdx);
        SolidBrush fill(sel ? Gp(UiTheme::Color_Accent(), 210) : Gp(RGB(48, 48, 52), 230));
        g.FillRectangle(&fill, r);

        // Live analog preview for bound HID keys (helps verify bind immediately).
        if (k.hid > 0 && k.hid < 256)
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
        g.DrawRectangle(&keyBorder, r);

        if (k.label && k.label[0])
        {
            FontFamily ff(L"Segoe UI");
            float em = std::clamp(r.Height * 0.36f, 9.0f, 13.0f);
            Font font(&ff, em, FontStyleRegular, UnitPixel);
            StringFormat fmt;
            fmt.SetAlignment(StringAlignmentCenter);
            fmt.SetLineAlignment(StringAlignmentCenter);
            fmt.SetFormatFlags(StringFormatFlagsNoWrap);
            SolidBrush txt(sel ? Gp(RGB(12, 12, 12)) : Gp(UiTheme::Color_Text()));
            g.DrawString(k.label, -1, &font, r, &fmt, &txt);
        }
    }
}

static void Layout_ApplyDrag(HWND hWnd, LayoutPageState* st, POINT ptClient)
{
    if (!st || st->selectedIdx < 0) return;

    KeyDef k{};
    if (!Layout_DraftGet(st, st->selectedIdx, k)) return;

    std::vector<int> displayX;
    const std::vector<int>* pDisplayX = nullptr;
    if (st->uniformSpacingEnabled)
    {
        Layout_BuildDisplayXMap(st, displayX);
        pDisplayX = displayX.empty() ? nullptr : &displayX;
    }

    float scale = 1.0f, ox = 0.0f, oy = 0.0f;
    Layout_ComputeTransform(st, st->canvasRc, pDisplayX, scale, ox, oy);
    if (scale <= 0.0001f) return;

    int left = ptClient.x - st->dragOffsetX;
    int top = ptClient.y - st->dragOffsetY;

    int targetDisplayX = (int)std::lround(((float)left - ox) / scale) - KEYBOARD_MARGIN_X;
    float rowPitch = (float)KEYBOARD_ROW_PITCH_Y * scale;
    int modelRow = (int)std::lround((((float)top - oy) - (float)KEYBOARD_MARGIN_Y * scale) / std::max(1.0f, rowPitch));

    KeyDef& edit = st->draftKeys[st->selectedIdx];
    int nextRow = std::clamp(modelRow, 0, 20);
    bool changed = false;

    if (st->uniformSpacingEnabled)
    {
        auto keyDisplayX = [&](int idx) -> int
        {
            if (pDisplayX && idx >= 0 && (size_t)idx < pDisplayX->size())
                return (*pDisplayX)[(size_t)idx];
            if (idx >= 0 && (size_t)idx < st->draftKeys.size())
                return st->draftKeys[(size_t)idx].x;
            return 0;
        };

        auto buildRowOrder = [&](int row) -> std::vector<int>
        {
            std::vector<int> ids;
            ids.reserve(st->draftKeys.size());
            for (int i = 0; i < (int)st->draftKeys.size(); ++i)
            {
                if (st->draftKeys[(size_t)i].row == row)
                    ids.push_back(i);
            }
            std::sort(ids.begin(), ids.end(), [&](int a, int b)
            {
                int ax = keyDisplayX(a);
                int bx = keyDisplayX(b);
                if (ax != bx) return ax < bx;
                return a < b;
            });
            return ids;
        };

        auto rowBaseX = [&](int row) -> int
        {
            int base = INT_MAX;
            for (int i = 0; i < (int)st->draftKeys.size(); ++i)
            {
                if (st->draftKeys[(size_t)i].row == row)
                    base = std::min(base, keyDisplayX(i));
            }
            if (base == INT_MAX)
                base = std::clamp(targetDisplayX, 0, 4000);
            return base;
        };

        auto insertionPos = [&](const std::vector<int>& ids) -> int
        {
            int ins = (int)ids.size();
            for (int i = 0; i < (int)ids.size(); ++i)
            {
                int id = ids[(size_t)i];
                int center = keyDisplayX(id) + st->draftKeys[(size_t)id].w / 2;
                if (targetDisplayX < center)
                {
                    ins = i;
                    break;
                }
            }
            return ins;
        };

        auto applyRowOrder = [&](int row, const std::vector<int>& order, int baseX)
        {
            int gap = std::max(0, st->uniformSpacingGap);
            int x = std::clamp(baseX, 0, 4000);
            for (int id : order)
            {
                KeyDef& kk = st->draftKeys[(size_t)id];
                kk.row = row;
                kk.x = std::clamp(x, 0, 4000);
                x += kk.w + gap;
            }
        };

        int rowFrom = edit.row;
        int rowTo = nextRow;
        std::vector<int> fromOrder = buildRowOrder(rowFrom);

        if (rowTo == rowFrom)
        {
            int oldPos = -1;
            for (int i = 0; i < (int)fromOrder.size(); ++i)
            {
                if (fromOrder[(size_t)i] == st->selectedIdx)
                {
                    oldPos = i;
                    break;
                }
            }

            if (oldPos >= 0)
            {
                std::vector<int> movable = fromOrder;
                movable.erase(movable.begin() + oldPos);

                int ins = insertionPos(movable);
                ins = std::clamp(ins, 0, (int)movable.size());
                movable.insert(movable.begin() + ins, st->selectedIdx);

                if (movable != fromOrder)
                {
                    applyRowOrder(rowFrom, movable, rowBaseX(rowFrom));
                    changed = true;
                }
            }
        }
        else
        {
            std::vector<int> toOrder = buildRowOrder(rowTo);

            std::vector<int> fromWithout = fromOrder;
            fromWithout.erase(std::remove(fromWithout.begin(), fromWithout.end(), st->selectedIdx), fromWithout.end());

            int ins = insertionPos(toOrder);
            ins = std::clamp(ins, 0, (int)toOrder.size());
            toOrder.insert(toOrder.begin() + ins, st->selectedIdx);

            if (!fromWithout.empty())
                applyRowOrder(rowFrom, fromWithout, rowBaseX(rowFrom));
            applyRowOrder(rowTo, toOrder, rowBaseX(rowTo));
            changed = true;
        }
    }
    else
    {
        int nextX = std::clamp(targetDisplayX, 0, 4000);
        if (edit.x != nextX)
        {
            edit.x = nextX;
            changed = true;
        }
    }

    if (!st->uniformSpacingEnabled && edit.row != nextRow)
    {
        edit.row = nextRow;
        changed = true;
    }

    if (changed)
    {
        st->dirty = true;
        Layout_UpdateMetaControls(st);
        InvalidateRect(hWnd, &st->canvasRc, FALSE);
    }
}

LRESULT CALLBACK KeyboardSubpages_LayoutPageProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    auto* st = (LayoutPageState*)GetWindowLongPtrW(hWnd, GWLP_USERDATA);

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
        HFONT hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);

        st = new LayoutPageState();
        SetWindowLongPtrW(hWnd, GWLP_USERDATA, (LONG_PTR)st);

        st->lblPreset = CreateWindowW(L"STATIC", L"Keyboard model", WS_CHILD | WS_VISIBLE,
            0, 0, 10, 10, hWnd, nullptr, hInst, nullptr);
        SendMessageW(st->lblPreset, WM_SETFONT, (WPARAM)hFont, TRUE);

        st->cmbPreset = PremiumCombo::Create(hWnd, hInst, 0, 0, 10, 10, ID_LAYOUT_PRESET,
            WS_CHILD | WS_VISIBLE | WS_TABSTOP);
        PremiumCombo::SetFont(st->cmbPreset, hFont, true);
        PremiumCombo::SetDropMaxVisible(st->cmbPreset, 8);

        st->btnReset = CreateWindowW(L"BUTTON", L"Reset To Preset", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
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

        st->btnApplyLabel = CreateWindowW(L"BUTTON", L"Apply Label", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
            0, 0, 10, 10, hWnd, (HMENU)(INT_PTR)ID_LAYOUT_LABEL_APPLY, hInst, nullptr);
        SendMessageW(st->btnApplyLabel, WM_SETFONT, (WPARAM)hFont, TRUE);

        st->btnBindKey = CreateWindowW(L"BUTTON", L"Bind Physical Key", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
            0, 0, 10, 10, hWnd, (HMENU)(INT_PTR)ID_LAYOUT_BIND_KEY, hInst, nullptr);
        SendMessageW(st->btnBindKey, WM_SETFONT, (WPARAM)hFont, TRUE);

        st->lblPos = CreateWindowW(L"STATIC", L"Position", WS_CHILD | WS_VISIBLE,
            0, 0, 10, 10, hWnd, nullptr, hInst, nullptr);
        SendMessageW(st->lblPos, WM_SETFONT, (WPARAM)hFont, TRUE);

        st->edtPos = CreateWindowW(L"EDIT", L"",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_BORDER | ES_AUTOHSCROLL | ES_NUMBER,
            0, 0, 10, 10, hWnd, (HMENU)(INT_PTR)ID_LAYOUT_POS_EDIT, hInst, nullptr);
        SendMessageW(st->edtPos, WM_SETFONT, (WPARAM)hFont, TRUE);

        st->lblWidth = CreateWindowW(L"STATIC", L"Width", WS_CHILD | WS_VISIBLE,
            0, 0, 10, 10, hWnd, nullptr, hInst, nullptr);
        SendMessageW(st->lblWidth, WM_SETFONT, (WPARAM)hFont, TRUE);

        st->edtWidth = CreateWindowW(L"EDIT", L"",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_BORDER | ES_AUTOHSCROLL | ES_NUMBER,
            0, 0, 10, 10, hWnd, (HMENU)(INT_PTR)ID_LAYOUT_WIDTH_EDIT, hInst, nullptr);
        SendMessageW(st->edtWidth, WM_SETFONT, (WPARAM)hFont, TRUE);

        st->lblHeight = CreateWindowW(L"STATIC", L"Height", WS_CHILD | WS_VISIBLE,
            0, 0, 10, 10, hWnd, nullptr, hInst, nullptr);
        SendMessageW(st->lblHeight, WM_SETFONT, (WPARAM)hFont, TRUE);

        st->edtHeight = CreateWindowW(L"EDIT", L"",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_BORDER | ES_AUTOHSCROLL | ES_NUMBER,
            0, 0, 10, 10, hWnd, (HMENU)(INT_PTR)ID_LAYOUT_HEIGHT_EDIT, hInst, nullptr);
        SendMessageW(st->edtHeight, WM_SETFONT, (WPARAM)hFont, TRUE);

        st->lblBindState = CreateWindowW(L"STATIC", L"", WS_CHILD | WS_VISIBLE,
            0, 0, 10, 10, hWnd, nullptr, hInst, nullptr);
        SendMessageW(st->lblBindState, WM_SETFONT, (WPARAM)hFont, TRUE);

        st->lblKeys = CreateWindowW(L"STATIC", L"Keys", WS_CHILD | WS_VISIBLE,
            0, 0, 10, 10, hWnd, nullptr, hInst, nullptr);
        SendMessageW(st->lblKeys, WM_SETFONT, (WPARAM)hFont, TRUE);

        st->lstKeys = CreateWindowW(L"LISTBOX", L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | LBS_NOTIFY | WS_VSCROLL | WS_BORDER,
            0, 0, 10, 10, hWnd, (HMENU)(INT_PTR)ID_LAYOUT_KEYS, hInst, nullptr);
        SendMessageW(st->lstKeys, WM_SETFONT, (WPARAM)hFont, TRUE);
        UiTheme::ApplyToControl(st->lstKeys);

        st->lblHint = CreateWindowW(L"STATIC",
            L"",
            WS_CHILD | WS_VISIBLE,
            0, 0, 10, 10, hWnd, nullptr, hInst, nullptr);
        SendMessageW(st->lblHint, WM_SETFONT, (WPARAM)hFont, TRUE);
        Layout_UpdateHintText(st);

        int presetCount = KeyboardLayout_GetPresetCount();
        for (int i = 0; i < presetCount; ++i)
            PremiumCombo::AddString(st->cmbPreset, KeyboardLayout_GetPresetName(i));
        st->editingPresetIdx = KeyboardLayout_GetCurrentPresetIndex();
        PremiumCombo::SetCurSel(st->cmbPreset, st->editingPresetIdx, false);
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
        if (st)
        {
            RECT rc{};
            GetClientRect(hWnd, &rc);
            int margin = S(hWnd, 12);
            int leftW = S(hWnd, 300);
            int topY = S(hWnd, 330);

            SetWindowPos(st->lblPreset, nullptr, margin, margin, leftW, S(hWnd, 18), SWP_NOZORDER);
            SetWindowPos(st->cmbPreset, nullptr, margin, margin + S(hWnd, 20), leftW - S(hWnd, 110), S(hWnd, 26), SWP_NOZORDER);
            SetWindowPos(st->btnReset, nullptr, margin + leftW - S(hWnd, 104), margin + S(hWnd, 20), S(hWnd, 104), S(hWnd, 26), SWP_NOZORDER);
            int actionY = margin + S(hWnd, 52);
            int actionH = S(hWnd, 26);
            int actionGap = S(hWnd, 8);
            int addW = S(hWnd, 90);
            int saveW = S(hWnd, 100);
            int addX = margin;
            int saveX = margin + leftW - saveW;
            int delX = addX + addW + actionGap;
            int delW = saveX - actionGap - delX;
            delW = std::max(S(hWnd, 72), delW);
            if (delX + delW > saveX - actionGap)
                delW = std::max(S(hWnd, 60), (saveX - actionGap) - delX);
            if (st->btnAdd)
                SetWindowPos(st->btnAdd, nullptr, addX, actionY, addW, actionH, SWP_NOZORDER);
            if (st->btnDelete)
                SetWindowPos(st->btnDelete, nullptr, delX, actionY, delW, actionH, SWP_NOZORDER);
            if (st->btnSave)
                SetWindowPos(st->btnSave, nullptr, saveX, actionY, saveW, actionH, SWP_NOZORDER);
            if (st->btnUniformSpacing)
                SetWindowPos(st->btnUniformSpacing, nullptr, margin, margin + S(hWnd, 84), leftW, S(hWnd, 24), SWP_NOZORDER);
            if (st->lblUniformGap)
                SetWindowPos(st->lblUniformGap, nullptr, margin, margin + S(hWnd, 112), leftW, S(hWnd, 18), SWP_NOZORDER);
            if (st->edtUniformGap)
                SetWindowPos(st->edtUniformGap, nullptr, margin, margin + S(hWnd, 132), S(hWnd, 96), S(hWnd, 24), SWP_NOZORDER);
            if (st->lblLabel)
                SetWindowPos(st->lblLabel, nullptr, margin, margin + S(hWnd, 162), leftW, S(hWnd, 18), SWP_NOZORDER);
            if (st->edtLabel)
                SetWindowPos(st->edtLabel, nullptr, margin, margin + S(hWnd, 182), leftW - S(hWnd, 114), S(hWnd, 24), SWP_NOZORDER);
            if (st->btnApplyLabel)
                SetWindowPos(st->btnApplyLabel, nullptr, margin + leftW - S(hWnd, 104), margin + S(hWnd, 182), S(hWnd, 104), S(hWnd, 24), SWP_NOZORDER);
            if (st->btnBindKey)
                SetWindowPos(st->btnBindKey, nullptr, margin, margin + S(hWnd, 212), leftW, S(hWnd, 24), SWP_NOZORDER);
            int fieldLabelY = margin + S(hWnd, 239);
            int fieldEditY = margin + S(hWnd, 257);
            int fieldGap = S(hWnd, 8);
            int fieldW = (leftW - fieldGap * 2) / 3;
            if (st->lblPos)
                SetWindowPos(st->lblPos, nullptr, margin, fieldLabelY, fieldW, S(hWnd, 18), SWP_NOZORDER);
            if (st->lblWidth)
                SetWindowPos(st->lblWidth, nullptr, margin + fieldW + fieldGap, fieldLabelY, fieldW, S(hWnd, 18), SWP_NOZORDER);
            if (st->lblHeight)
                SetWindowPos(st->lblHeight, nullptr, margin + (fieldW + fieldGap) * 2, fieldLabelY, fieldW, S(hWnd, 18), SWP_NOZORDER);
            if (st->edtPos)
                SetWindowPos(st->edtPos, nullptr, margin, fieldEditY, fieldW, S(hWnd, 24), SWP_NOZORDER);
            if (st->edtWidth)
                SetWindowPos(st->edtWidth, nullptr, margin + fieldW + fieldGap, fieldEditY, fieldW, S(hWnd, 24), SWP_NOZORDER);
            if (st->edtHeight)
                SetWindowPos(st->edtHeight, nullptr, margin + (fieldW + fieldGap) * 2, fieldEditY, fieldW, S(hWnd, 24), SWP_NOZORDER);
            if (st->lblBindState)
                SetWindowPos(st->lblBindState, nullptr, margin, margin + S(hWnd, 284), leftW, S(hWnd, 18), SWP_NOZORDER);
            if (st->lblKeys)
                SetWindowPos(st->lblKeys, nullptr, margin, margin + S(hWnd, 306), leftW, S(hWnd, 18), SWP_NOZORDER);
            int hintH = S(hWnd, 68);
            int listH = std::max(S(hWnd, 80), (int)rc.bottom - topY - margin - hintH - S(hWnd, 8));
            SetWindowPos(st->lstKeys, nullptr, margin, topY, leftW, listH, SWP_NOZORDER);
            SetWindowPos(st->lblHint, nullptr, margin, rc.bottom - margin - hintH, leftW, hintH, SWP_NOZORDER);

            Layout_ComputeCanvasRect(hWnd, st);
        }
        InvalidateRect(hWnd, nullptr, FALSE);
        return 0;

    case WM_COMMAND:
        if (!st) return 0;
        if (LOWORD(wParam) == ID_LAYOUT_PRESET && HIWORD(wParam) == CBN_SELCHANGE)
        {
            int sel = PremiumCombo::GetCurSel(st->cmbPreset);
            Layout_LoadDraftFromPreset(hWnd, st, sel, true);
            return 0;
        }
        if (LOWORD(wParam) == ID_LAYOUT_RESET && HIWORD(wParam) == BN_CLICKED)
        {
            Layout_LoadDraftFromPreset(hWnd, st, st->editingPresetIdx, true);
            PremiumCombo::SetCurSel(st->cmbPreset, st->editingPresetIdx, false);
            return 0;
        }
        if (LOWORD(wParam) == ID_LAYOUT_ADD && HIWORD(wParam) == BN_CLICKED)
        {
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
            Layout_RefreshKeyList(hWnd, st);
            SendMessageW(st->lstKeys, LB_SETCURSEL, (WPARAM)st->selectedIdx, 0);
            Layout_SetUnsaved(st, true);
            InvalidateRect(hWnd, nullptr, FALSE);
            return 0;
        }
        if (LOWORD(wParam) == ID_LAYOUT_LABEL_APPLY && HIWORD(wParam) == BN_CLICKED)
        {
            Layout_ApplyLabelFromEdit(hWnd, st);
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
                    Layout_RefreshKeyList(hWnd, st);
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
            if (st->selectedIdx < 0)
                st->selectedIdx = (int)SendMessageW(st->lstKeys, LB_GETCURSEL, 0, 0);
            if (st->selectedIdx >= 0 && st->selectedIdx < (int)st->draftKeys.size())
            {
                st->draftKeys.erase(st->draftKeys.begin() + st->selectedIdx);
                if (st->selectedIdx < (int)st->draftLabels.size())
                    st->draftLabels.erase(st->draftLabels.begin() + st->selectedIdx);
                Layout_RebindDraftLabels(st);

                int n = (int)st->draftKeys.size();
                if (st->selectedIdx >= n) st->selectedIdx = n - 1;
                Layout_RefreshKeyList(hWnd, st);
                if (st->selectedIdx >= 0)
                    SendMessageW(st->lstKeys, LB_SETCURSEL, (WPARAM)st->selectedIdx, 0);
                Layout_SetUnsaved(st, true);
                InvalidateRect(hWnd, nullptr, FALSE);
            }
            return 0;
        }
        if (LOWORD(wParam) == ID_LAYOUT_SAVE && HIWORD(wParam) == BN_CLICKED)
        {
            bool wasActive = (st->editingPresetIdx == KeyboardLayout_GetCurrentPresetIndex());
            if (KeyboardLayout_StorePresetSnapshot(st->editingPresetIdx, st->draftKeys, st->draftLabels, true,
                st->uniformSpacingEnabled, st->uniformSpacingGap))
            {
                if (wasActive)
                    Layout_NotifyMainPage(hWnd);
                Layout_RequestSave(hWnd);
                Layout_SetUnsaved(st, false);
            }
            else
            {
                MessageBoxW(hWnd, L"Failed to save layout preset file.", L"Layout Editor", MB_ICONERROR);
            }
            InvalidateRect(hWnd, nullptr, FALSE);
            return 0;
        }
        if (LOWORD(wParam) == ID_LAYOUT_KEYS && HIWORD(wParam) == LBN_SELCHANGE)
        {
            st->selectedIdx = (int)SendMessageW(st->lstKeys, LB_GETCURSEL, 0, 0);
            Layout_UpdateMetaControls(st);
            SetFocus(hWnd);
            InvalidateRect(hWnd, &st->canvasRc, FALSE);
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
             (dis->CtlID == ID_LAYOUT_LABEL_APPLY && st->btnApplyLabel == dis->hwndItem) ||
             (dis->CtlID == ID_LAYOUT_BIND_KEY && st->btnBindKey == dis->hwndItem) ||
             (dis->CtlID == ID_LAYOUT_SAVE && st->btnSave == dis->hwndItem) ||
             (dis->CtlID == ID_LAYOUT_UNIFORM_SPACING && st->btnUniformSpacing == dis->hwndItem)))
        {
            Layout_DrawFlatButton(dis);
            return TRUE;
        }
        break;
    }

    case WM_LBUTTONDOWN:
        if (st)
        {
            POINT pt{ (short)LOWORD(lParam), (short)HIWORD(lParam) };
            if (PtInRect(&st->canvasRc, pt))
            {
                int hit = Layout_HitTestKey(st, pt);
                if (hit >= 0)
                {
                    st->selectedIdx = hit;
                    SendMessageW(st->lstKeys, LB_SETCURSEL, (WPARAM)hit, 0);
                    Layout_UpdateMetaControls(st);
                    SetFocus(hWnd);
                    RECT rr = Layout_KeyRectOnCanvas(st, hit, st->canvasRc);
                    st->dragOffsetX = pt.x - rr.left;
                    st->dragOffsetY = pt.y - rr.top;
                    st->dragging = true;
                    st->dirty = false;
                    SetCapture(hWnd);
                    InvalidateRect(hWnd, &st->canvasRc, FALSE);
                }
            }
        }
        return 0;

    case WM_MOUSEMOVE:
        if (st && st->dragging)
        {
            POINT pt{ (short)LOWORD(lParam), (short)HIWORD(lParam) };
            Layout_ApplyDrag(hWnd, st, pt);
        }
        return 0;

    case WM_LBUTTONUP:
        if (st && st->dragging)
        {
            st->dragging = false;
            ReleaseCapture();
            if (st->dirty)
            {
                Layout_RefreshKeyList(hWnd, st);
                Layout_SetUnsaved(st, true);
                st->dirty = false;
            }
        }
        return 0;

    case WM_MOUSEWHEEL:
        if (st && st->selectedIdx >= 0)
        {
            KeyDef k{};
            if (Layout_DraftGet(st, st->selectedIdx, k))
            {
                int delta = GET_WHEEL_DELTA_WPARAM(wParam);
                bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
                int step = shift ? 4 : 1;
                int newW = k.w + ((delta > 0) ? step : -step);
                int clampedW = std::clamp(newW, KEYBOARD_KEY_MIN_DIM, KEYBOARD_KEY_MAX_DIM);
                if (st->draftKeys[st->selectedIdx].w != clampedW)
                {
                    st->draftKeys[st->selectedIdx].w = clampedW;
                    Layout_RefreshKeyList(hWnd, st);
                    Layout_SetUnsaved(st, true);
                    InvalidateRect(hWnd, &st->canvasRc, FALSE);
                }
            }
            return 0;
        }
        break;

    case WM_CAPTURECHANGED:
        if (st) st->dragging = false;
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
                        Layout_RefreshKeyList(hWnd, st);
                        Layout_SetUnsaved(st, true);
                    }
                    wchar_t s[128]{};
                    swprintf_s(s, L"Bound HID %u (raw %u).", (unsigned)hid, (unsigned)rawM);
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
            Layout_LoadDraftFromPreset(hWnd, st, st->editingPresetIdx, true);
            PremiumCombo::SetCurSel(st->cmbPreset, st->editingPresetIdx, false);
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
            case VK_OEM_4: Layout_NudgeSelectedKey(hWnd, st, 0, 0, -step * 2); return 0; // [
            case VK_OEM_6: Layout_NudgeSelectedKey(hWnd, st, 0, 0, +step * 2); return 0; // ]
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
        DestroyWindow(hWnd);
        return 0;

    case WM_SHOWWINDOW:
    case WM_ACTIVATE:
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

static void LayoutEditor_OpenWindow(HWND hOwnerPage)
{
    if (g_hLayoutEditorWindow && IsWindow(g_hLayoutEditorWindow))
    {
        ShowWindow(g_hLayoutEditorWindow, SW_SHOWNORMAL);
        SetForegroundWindow(g_hLayoutEditorWindow);
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
        hOwnerTop, nullptr, hInst, nullptr);

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
    HWND btnLayoutEditor = nullptr;
    HWND btnOpenLayoutsFolder = nullptr;

    HWND lblPoll = nullptr;
    HWND sldPoll = nullptr;
    HWND chipPoll = nullptr;

    HWND lblUiRefresh = nullptr;
    HWND sldUiRefresh = nullptr;
    HWND chipUiRefresh = nullptr;

    HWND lblHint = nullptr;

    int   pendingDeleteIdx = -1;
    DWORD pendingDeleteTick = 0;
    bool  pendingDeleteIsGlobalProfile = false;
    HWND hToast = nullptr;
    std::wstring toastText;
    DWORD toastHideAt = 0;
};

static constexpr int GLOB_ID_POLL_SLIDER = 7601;
static constexpr int GLOB_ID_UIREFRESH_SLIDER = 7602;
static constexpr int GLOB_ID_LAYOUT_COMBO = 7603;
static constexpr int GLOB_ID_LAYOUT_EDITOR = 7604;
static constexpr int GLOB_ID_LAYOUTS_FOLDER = 7605;
static constexpr int GLOB_ID_GLOBAL_PROFILE_COMBO = 7606;

static void Global_DrawLayoutEditorButton(const DRAWITEMSTRUCT* dis)
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
    DrawTextW(hdc, text, -1, &rc,
        DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

    if (dis->itemState & ODS_FOCUS)
    {
        RECT focus = rc;
        InflateRect(&focus, -4, -4);
        DrawFocusRect(hdc, &focus);
    }
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

static void Global_OpenLayoutsFolder(HWND hWnd)
{
    std::wstring dir = WinUtil_BuildPathNearExe(L"Layouts");
    std::error_code ec;
    fs::create_directories(fs::path(dir), ec);
    ShellExecuteW(hWnd, L"open", dir.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
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
    st->toastHideAt = GetTickCount() + TOAST_SHOW_MS;
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

static void Global_RefreshLayoutCombo(GlobalSettingsPageState* st)
{
    if (!st || !st->cmbLayout) return;

    PremiumCombo::Clear(st->cmbLayout);

    int presetCount = KeyboardLayout_GetPresetCount();
    for (int i = 0; i < presetCount; ++i)
    {
        int idx = PremiumCombo::AddString(st->cmbLayout, KeyboardLayout_GetPresetName(i));
        if (presetCount > 1)
            PremiumCombo::SetItemButtonKind(st->cmbLayout, idx, PremiumCombo::ItemButtonKind::Delete);
    }

    PremiumCombo::AddString(st->cmbLayout, L"+ Create New Layout...");
    PremiumCombo::SetDropMaxVisible(st->cmbLayout, 10);

    int cur = KeyboardLayout_GetCurrentPresetIndex();
    if (presetCount > 0)
        PremiumCombo::SetCurSel(st->cmbLayout, std::clamp(cur, 0, presetCount - 1), false);
    else
        PremiumCombo::SetCurSel(st->cmbLayout, -1, false);
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
        if (_wcsicmp(names[i].c_str(), active.c_str()) == 0)
        {
            activeIdx = (int)i;
            break;
        }
    }
    PremiumCombo::SetCurSel(st->cmbGlobalProfile, activeIdx, false);
    Global_UpdateProfileSaveIcon(st);
}

static void Global_ApplyActiveGlobalProfile(GlobalSettingsPageState* st, HWND hWnd, const std::wstring& name)
{
    if (!st) return;

    std::wstring newName = GlobalProfiles_SanitizeName(name);
    if (newName.empty()) newName = L"Default";
    if (_wcsicmp(newName.c_str(), GlobalProfiles_GetActiveName().c_str()) == 0)
        return;

    // Persist previous profile state before switching away.
    const std::wstring prevName = GlobalProfiles_GetActiveName();
    const bool previousSettingsSaved = SettingsIni_SaveProfile(GlobalProfiles_GetSettingsPath(prevName).c_str());
    const bool previousBindingsSaved = Profile_SaveIni(GlobalProfiles_GetBindingsPath(prevName).c_str());
    if (!previousSettingsSaved || !previousBindingsSaved)
    {
        Global_UpdateUi(st);
        return;
    }

    GlobalProfiles_SetActiveName(newName);
    if (!GlobalProfiles_SaveActiveToSettingsIni(AppPaths_SettingsIni().c_str()))
    {
        GlobalProfiles_SetActiveName(prevName);
        Global_UpdateUi(st);
        return;
    }

    SettingsIni_LoadProfile(GlobalProfiles_GetSettingsPath(newName).c_str());
    Profile_LoadIni(GlobalProfiles_GetBindingsPath(newName).c_str());

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
                if (_wcsicmp(item, active.c_str()) == 0)
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
        int cur = KeyboardLayout_GetCurrentPresetIndex();
        int sel = PremiumCombo::GetCurSel(st->cmbLayout);
        int count = PremiumCombo::GetCount(st->cmbLayout);
        bool selIsCreateRow = (count > 0 && sel == count - 1);
        if (!selIsCreateRow && sel != cur)
            PremiumCombo::SetCurSel(st->cmbLayout, cur, false);
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
    int nextRow = std::clamp(k.row + dRow, 0, 20);
    int nextX = std::clamp(k.x + dX, 0, 4000);
    int nextW = std::clamp(k.w + dW, KEYBOARD_KEY_MIN_DIM, KEYBOARD_KEY_MAX_DIM);
    if (nextRow == k.row && nextX == k.x && nextW == k.w)
        return false;
    k.row = nextRow;
    k.x = nextX;
    k.w = nextW;

    Layout_RefreshKeyList(hWnd, st);
    Layout_SetUnsaved(st, true);
    InvalidateRect(hWnd, nullptr, FALSE);
    return true;
}

static void Global_Layout(HWND hWnd, GlobalSettingsPageState* st)
{
    if (!st) return;

    RECT rc{};
    GetClientRect(hWnd, &rc);

    int margin = S(hWnd, 16);
    int x = margin;
    int y = margin;

    int chipW = S(hWnd, 86);
    int gap = S(hWnd, 10);
    int comboVisibleH = S(hWnd, 26);
    int sliderH = S(hWnd, 34);
    int chipH = sliderH;
    int labelH = S(hWnd, 18);
    int rowGap = S(hWnd, 18);

    int sliderW = (rc.right - rc.left) - margin * 2 - chipW - gap;
    sliderW = std::max(S(hWnd, 180), sliderW);

    if (st->lblGlobalProfile)
        SetWindowPos(st->lblGlobalProfile, nullptr, x, y, sliderW + gap + chipW, labelH, SWP_NOZORDER);
    y += labelH + S(hWnd, 6);

    if (st->cmbGlobalProfile)
        SetWindowPos(st->cmbGlobalProfile, nullptr, x, y, sliderW + gap + chipW, comboVisibleH, SWP_NOZORDER);
    y += comboVisibleH + rowGap;

    if (st->lblLayout)
        SetWindowPos(st->lblLayout, nullptr, x, y, sliderW + gap + chipW, labelH, SWP_NOZORDER);
    y += labelH + S(hWnd, 6);

    if (st->cmbLayout)
        SetWindowPos(st->cmbLayout, nullptr, x, y, sliderW + gap + chipW, comboVisibleH, SWP_NOZORDER);
    y += comboVisibleH + rowGap;

    if (st->btnLayoutEditor)
    {
        int bw = S(hWnd, 210);
        int bh = S(hWnd, 28);
        SetWindowPos(st->btnLayoutEditor, nullptr, x, y, bw, bh, SWP_NOZORDER);
        y += bh + S(hWnd, 14);
    }

    if (st->btnOpenLayoutsFolder)
    {
        int bw = S(hWnd, 210);
        int bh = S(hWnd, 28);
        SetWindowPos(st->btnOpenLayoutsFolder, nullptr, x, y, bw, bh, SWP_NOZORDER);
        y += bh + S(hWnd, 14);
    }

    if (st->lblPoll)
        SetWindowPos(st->lblPoll, nullptr, x, y, sliderW + gap + chipW, labelH, SWP_NOZORDER);
    y += labelH + S(hWnd, 6);

    if (st->sldPoll)
        SetWindowPos(st->sldPoll, nullptr, x, y, sliderW, sliderH, SWP_NOZORDER);
    if (st->chipPoll)
        SetWindowPos(st->chipPoll, nullptr, x + sliderW + gap, y, chipW, chipH, SWP_NOZORDER);
    y += sliderH + rowGap;

    if (st->lblUiRefresh)
        SetWindowPos(st->lblUiRefresh, nullptr, x, y, sliderW + gap + chipW, labelH, SWP_NOZORDER);
    y += labelH + S(hWnd, 6);

    if (st->sldUiRefresh)
        SetWindowPos(st->sldUiRefresh, nullptr, x, y, sliderW, sliderH, SWP_NOZORDER);
    if (st->chipUiRefresh)
        SetWindowPos(st->chipUiRefresh, nullptr, x + sliderW + gap, y, chipW, chipH, SWP_NOZORDER);
    y += sliderH + S(hWnd, 14);

    if (st->lblHint)
        SetWindowPos(st->lblHint, nullptr, x, y, std::max(S(hWnd, 120), sliderW + gap + chipW), S(hWnd, 20), SWP_NOZORDER);
}

LRESULT CALLBACK KeyboardSubpages_GlobalSettingsPageProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    auto* st = (GlobalSettingsPageState*)GetWindowLongPtrW(hWnd, GWLP_USERDATA);

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

        if (hCombo == st->cmbLayout)
        {
            int presetCount = KeyboardLayout_GetPresetCount();
            if (idx != presetCount)
                return 0; // create row is always the last item

            int newIdx = -1;
            if (KeyboardLayout_CreatePreset(nameBuf, &newIdx))
            {
                Global_RefreshLayoutCombo(st);
                if (newIdx >= 0)
                    PremiumCombo::SetCurSel(st->cmbLayout, newIdx, false);
                PremiumCombo::ShowDropDown(st->cmbLayout, false);
                Global_NotifyMainPage(hWnd);
                Global_RequestSave(hWnd);
            }
            else
            {
                MessageBoxW(hWnd, L"Failed to create layout. Name may be empty or already exists.", L"Layouts", MB_ICONWARNING);
                Global_UpdateUi(st);
            }
            return 0;
        }

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
                if (_wcsicmp(n.c_str(), newName.c_str()) == 0)
                {
                    MessageBoxW(hWnd, L"Profile with this name already exists.", L"Profiles", MB_ICONWARNING);
                    return 0;
                }
            }

            const std::wstring prevName = GlobalProfiles_GetActiveName();
            const bool previousSettingsSaved = SettingsIni_SaveProfile(GlobalProfiles_GetSettingsPath(prevName).c_str());
            const bool previousBindingsSaved = Profile_SaveIni(GlobalProfiles_GetBindingsPath(prevName).c_str());
            if (!previousSettingsSaved || !previousBindingsSaved)
            {
                Global_UpdateUi(st);
                return 0;
            }

            // New profile starts as a full copy of current runtime state.
            const bool newSettingsSaved = SettingsIni_SaveProfile(GlobalProfiles_GetSettingsPath(newName).c_str());
            const bool newBindingsSaved = Profile_SaveIni(GlobalProfiles_GetBindingsPath(newName).c_str());
            if (!newSettingsSaved || !newBindingsSaved)
            {
                GlobalProfiles_Delete(newName);
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

        if (hCombo == st->cmbLayout)
        {
            int presetCount = KeyboardLayout_GetPresetCount();
            if (idx < 0 || idx >= presetCount)
                return 0;

            bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
            if (shift)
            {
                GlobalDeleteConfirm_Clear(hWnd, st);
                if (KeyboardLayout_DeletePreset(idx))
                {
                    Global_RefreshLayoutCombo(st);
                    Global_NotifyMainPage(hWnd);
                    Global_RequestSave(hWnd);
                }
                return 0;
            }

            DWORD now = GetTickCount();
            if (st->pendingDeleteIdx == idx && !st->pendingDeleteIsGlobalProfile &&
                (now - st->pendingDeleteTick) <= TOAST_SHOW_MS)
            {
                GlobalDeleteConfirm_Clear(hWnd, st);
                if (KeyboardLayout_DeletePreset(idx))
                {
                    Global_RefreshLayoutCombo(st);
                    Global_NotifyMainPage(hWnd);
                    Global_RequestSave(hWnd);
                }
                return 0;
            }

            st->pendingDeleteIdx = idx;
            st->pendingDeleteTick = now;
            st->pendingDeleteIsGlobalProfile = false;
            GlobalToast_ShowNearCursor(hWnd, st, L"Click again to confirm delete");
            return 0;
        }

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
                if (_wcsicmp(GlobalProfiles_GetActiveName().c_str(), name.c_str()) == 0)
                    Global_ApplyActiveGlobalProfile(st, hWnd, L"Default");

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

        std::wstring settingsPath = AppPaths_ActiveSettingsIni();
        std::wstring bindingsPath = AppPaths_ActiveBindingsIni();
        const bool settingsSaved = SettingsIni_SaveProfile(settingsPath.c_str());
        const bool bindingsSaved = Profile_SaveIni(bindingsPath.c_str());
        if (!settingsSaved || !bindingsSaved)
            return 0;

        GlobalProfiles_SetDirty(false);
        Global_UpdateProfileSaveIcon(st);
        Global_RequestSave(hWnd);
        return 0;
    }

    if (msg == WM_APP_GLOBAL_PROFILE_DIRTY)
    {
        if (st)
            Global_UpdateProfileSaveIcon(st);
        return 0;
    }

    switch (msg)
    {
    case WM_ERASEBKGND:
        return 1;

    case WM_PAINT:
    {
        PAINTSTRUCT ps{};
        HDC hdc = BeginPaint(hWnd, &ps);
        RECT rc{};
        GetClientRect(hWnd, &rc);
        FillRect(hdc, &rc, UiTheme::Brush_PanelBg());
        EndPaint(hWnd, &ps);
        return 0;
    }

    case WM_CTLCOLORSTATIC:
    {
        HDC hdc = (HDC)wParam;
        SetBkMode(hdc, TRANSPARENT);

        HWND hCtl = (HWND)lParam;
        if (st && hCtl && st->lblHint && hCtl == st->lblHint)
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
        Global_RefreshLayoutCombo(st);

        st->btnLayoutEditor = CreateWindowW(L"BUTTON", L"Open Layout Editor Window",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
            0, 0, 10, 10, hWnd, (HMENU)(INT_PTR)GLOB_ID_LAYOUT_EDITOR, hInst, nullptr);
        SendMessageW(st->btnLayoutEditor, WM_SETFONT, (WPARAM)hFont, TRUE);

        st->btnOpenLayoutsFolder = CreateWindowW(L"BUTTON", L"Open Layouts Folder",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
            0, 0, 10, 10, hWnd, (HMENU)(INT_PTR)GLOB_ID_LAYOUTS_FOLDER, hInst, nullptr);
        SendMessageW(st->btnOpenLayoutsFolder, WM_SETFONT, (WPARAM)hFont, TRUE);

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

        st->lblHint = CreateWindowW(L"STATIC",
            L"Changes are applied immediately and saved automatically.",
            WS_CHILD | WS_VISIBLE, 0, 0, 10, 10, hWnd, nullptr, hInst, nullptr);
        SendMessageW(st->lblHint, WM_SETFONT, (WPARAM)hFont, TRUE);

        Global_UpdateUi(st);
        Global_Layout(hWnd, st);
        return 0;
    }

    case WM_SIZE:
        Global_Layout(hWnd, st);
        return 0;

    case WM_TIMER:
        if (st && wParam == TOAST_TIMER_ID)
        {
            DWORD now = GetTickCount();
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
             (dis->CtlID == GLOB_ID_LAYOUTS_FOLDER && st->btnOpenLayoutsFolder == dis->hwndItem)))
        {
            Global_DrawLayoutEditorButton(dis);
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
            }
            return 0;
        }

        if (LOWORD(wParam) == (UINT)GLOB_ID_LAYOUT_COMBO && HIWORD(wParam) == CBN_SELCHANGE)
        {
            GlobalDeleteConfirm_Clear(hWnd, st);
            int sel = PremiumCombo::GetCurSel(st->cmbLayout);
            int count = PremiumCombo::GetCount(st->cmbLayout);
            bool selIsCreateRow = (count > 0 && sel == count - 1);

            if (selIsCreateRow)
            {
                PremiumCombo::ShowDropDown(st->cmbLayout, true);
                PremiumCombo::BeginInlineEditSelected(st->cmbLayout, false);
                return 0;
            }

            if (sel >= 0 && sel != KeyboardLayout_GetCurrentPresetIndex())
            {
                KeyboardLayout_SetPresetIndex(sel);
                Global_NotifyMainPage(hWnd);
                Global_RequestSave(hWnd);
            }
            return 0;
        }

        if (LOWORD(wParam) == (UINT)GLOB_ID_LAYOUT_EDITOR && HIWORD(wParam) == BN_CLICKED)
        {
            GlobalDeleteConfirm_Clear(hWnd, st);
            LayoutEditor_OpenWindow(hWnd);
            return 0;
        }

        if (LOWORD(wParam) == (UINT)GLOB_ID_LAYOUTS_FOLDER && HIWORD(wParam) == BN_CLICKED)
        {
            GlobalDeleteConfirm_Clear(hWnd, st);
            Global_OpenLayoutsFolder(hWnd);
            return 0;
        }

        return 0;

    case WM_NCDESTROY:
        if (st)
        {
            GlobalDeleteConfirm_Clear(hWnd, st);
            if (st->hToast && IsWindow(st->hToast))
                DestroyWindow(st->hToast);
            st->hToast = nullptr;
            delete st;
            SetWindowLongPtrW(hWnd, GWLP_USERDATA, 0);
        }
        return 0;
    }

    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

// ============================================================================
// Mouse settings page
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
        RECT rc{};
        GetClientRect(hWnd, &rc);
        FillRect(memDC, &rc, UiTheme::Brush_PanelBg());
        if (st)
        {
            st->surface.contentHeight = st->contentHeight;
            st->surface.scrollY = st->scrollY;
            if (CustomPageSurface_RenderCache(hWnd, memDC, &st->surface, MouseCustom_RenderCacheContent, st))
            {
                HDC cacheDC = CreateCompatibleDC(memDC);
                if (cacheDC)
                {
                    HGDIOBJ old = SelectObject(cacheDC, st->surface.contentCache);
                    int copyW = std::min((int)(rc.right - rc.left), st->surface.cacheWidth);
                    int copyH = std::min((int)(rc.bottom - rc.top), std::max(0, st->surface.cacheHeight - st->scrollY));
                    if (copyW > 0 && copyH > 0)
                        BitBlt(memDC, 0, 0, copyW, copyH, cacheDC, 0, st->scrollY, SRCCOPY);
                    SelectObject(cacheDC, old);
                    DeleteDC(cacheDC);
                }
            }
            CustomPageSurface_DrawScrollbar(hWnd, memDC, &st->surface, st->scrollDrag);
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
            int delta = GET_WHEEL_DELTA_WPARAM(wParam);
            if (delta != 0)
            {
                int step = S(hWnd, 44);
                st->surface.scrollY = st->scrollY;
                st->surface.contentHeight = st->contentHeight;
                CustomPageSurface_SetScrollY(hWnd, &st->surface, st->scrollY - ((delta / WHEEL_DELTA) * step));
                st->scrollY = st->surface.scrollY;
            }
            return 0;
        }
        break;
    case WM_LBUTTONDOWN:
        if (st)
        {
            SetFocus(hWnd);
            POINT pt{ (short)LOWORD(lParam), (short)HIWORD(lParam) };
            RECT thumb = CustomPageSurface_GetScrollThumbRect(hWnd, &st->surface);
            RECT track = CustomPageSurface_GetScrollTrackRect(hWnd);
            int maxScroll = CustomPageSurface_GetMaxScroll(hWnd, &st->surface);
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
                int page = std::max(1, (int)((track.bottom - track.top) * 0.75));
                CustomPageSurface_SetScrollY(hWnd, &st->surface, pt.y < thumb.top ? st->scrollY - page : st->scrollY + page);
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
            if (st->scrollDrag)
            {
                RECT track = CustomPageSurface_GetScrollTrackRect(hWnd);
                int thumbH = std::max(1, st->scrollDragThumbHeight);
                int travel = std::max(1, (int)(track.bottom - track.top) - thumbH);
                int topWanted = pt.y - st->scrollDragGrabOffsetY;
                int top = std::clamp(topWanted, (int)track.top, (int)track.bottom - thumbH);
                double t = (double)(top - track.top) / (double)travel;
                CustomPageSurface_SetScrollY(hWnd, &st->surface, (int)std::lround(t * (double)std::max(1, st->scrollDragMax)));
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
            bool wasDrag = st->dragId != 0 || st->scrollDrag;
            st->pressedId = 0;
            st->dragId = 0;
            st->scrollDrag = false;
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
            st->scrollDrag = false;
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
            Global_DrawLayoutEditorButton(dis);
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
    HWND chkSnappy = nullptr;
    HWND chkLastKeyPriority = nullptr;
    HWND lblLastKeyPrioritySensitivity = nullptr;
    HWND sldLastKeyPrioritySensitivity = nullptr;
    HWND chipLastKeyPrioritySensitivity = nullptr;
    HWND chkBlockBoundKeys = nullptr;
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
    DWORD toastHideAt = 0;

    // vertical scroll state for Configuration page
    CustomPageSurface surface;
    int scrollY = 0;
    int contentHeight = 0;
    bool scrollDrag = false;
    int  scrollDragStartY = 0;
    int  scrollDragStartScrollY = 0;
    int  scrollDragGrabOffsetY = 0;
    int  scrollDragThumbHeight = 0;
    int  scrollDragMax = 0;
    bool customControls = true;
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

static RECT Config_CustomSparkModeLabelRect(HWND hWnd)
{
    int y = Config_CustomToggleRect(hWnd, 2).bottom + S(hWnd, 10);
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

static std::vector<std::wstring> Config_BuildAnalogTelemetryLines(
    const BackendAnalogTelemetry& t,
    const std::wstring& profileStatus)
{
    std::vector<std::wstring> lines;
    wchar_t line[768]{};

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
                transport = (d.flags & BackendAnalogDeviceFlag_UnthrottledWorker) != 0
                    ? L"unthrottled background HID polling"
                    : L"background HID polling";
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

    // Reserve one stable line for preset/profile notifications so content
    // height does not change when a toast/status message appears.
    lines.emplace_back(profileStatus);
    return lines;
}

static RECT Config_CustomStatusRect(HWND hWnd, ConfigPageState* st)
{
    int y = Config_CustomToggleRect(hWnd, 2).bottom + S(hWnd, 10);
    BackendAnalogTelemetry t{};
    Backend_GetAnalogTelemetry(&t);
    if (t.sparkConnected)
        y += S(hWnd, 28) + S(hWnd, 10);
    const auto lines = Config_BuildAnalogTelemetryLines(t, st ? st->profileStatusText : std::wstring{});
    return Config_Rect(S(hWnd, 12), y, S(hWnd, 720), S(hWnd, std::max(1, (int)lines.size()) * 18));
}

static void Config_DrawCustomToggle(HWND hWnd, HDC hdc, Gdiplus::Graphics& g, const RECT& rc, const std::wstring& text, bool checked, bool enabled)
{
    CustomPage_DrawCheckbox(g, hdc, hWnd, rc, text, checked, enabled);
}

static void Config_DrawCustomCombo(HWND hWnd, HDC hdc, Gdiplus::Graphics& g, const RECT& rc, const std::wstring& text, bool enabled)
{
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

    BackendAnalogTelemetry t{};
    Backend_GetAnalogTelemetry(&t);
    if (t.sparkConnected)
    {
        const wchar_t* mode = L"Safe";
        switch (Settings_GetSparkPollMode())
        {
        case SettingsSparkPollMode_FastYield: mode = L"Fast yield"; break;
        case SettingsSparkPollMode_MaxBurst: mode = L"Max burst"; break;
        default: break;
        }
        wchar_t rows[32]{};
        UINT limit = Settings_GetSparkRowLimit();
        if (limit == 0)
            swprintf_s(rows, L"Auto");
        else
            swprintf_s(rows, L"%u rows", (unsigned)limit);

        CustomPage_DrawText(hdc, L"HE poll mode", Config_ToViewRect(Config_CustomSparkModeLabelRect(hWnd), st), UiTheme::Color_Text(), DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        Config_DrawCustomCombo(hWnd, hdc, g, Config_ToViewRect(Config_CustomSparkModeRect(hWnd), st), mode, true);
        CustomPage_DrawText(hdc, L"Rows", Config_ToViewRect(Config_CustomSparkRowsLabelRect(hWnd), st), UiTheme::Color_Text(), DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        Config_DrawCustomCombo(hWnd, hdc, g, Config_ToViewRect(Config_CustomSparkRowsRect(hWnd), st), rows, true);
    }
    RECT status = Config_ToViewRect(Config_CustomStatusRect(hWnd, st), st);
    const auto telemetryLines = Config_BuildAnalogTelemetryLines(t, st->profileStatusText);
    RECT row = status;
    row.bottom = row.top + S(hWnd, 18);
    for (const auto& telemetryLine : telemetryLines)
    {
        CustomPage_DrawText(hdc, telemetryLine, row, UiTheme::Color_TextMuted(),
            DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        OffsetRect(&row, 0, S(hWnd, 18));
    }
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
    KeySettingsPanel_DrawGraph(hdc, rc);
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

static bool Config_HandleCustomControlsMouse(HWND hWnd, ConfigPageState* st, UINT msg, WPARAM wParam, LPARAM contentLParam)
{
    if (!st || !st->customControls) return false;
    POINT pt{ (short)LOWORD(contentLParam), (short)HIWORD(contentLParam) };

    auto hit = [&](const RECT& rc) { return pt.x >= rc.left && pt.x < rc.right && pt.y >= rc.top && pt.y < rc.bottom; };
    int hitId = 0;
    if (hit(Config_CustomToggleRect(hWnd, 0))) hitId = ID_SNAPPY;
    else if (hit(Config_CustomToggleRect(hWnd, 1))) hitId = ID_LAST_KEY_PRIORITY;
    else if (hit(Config_CustomToggleRect(hWnd, 2))) hitId = ID_BLOCK_BOUND_KEYS;
    else if (hit(Config_CustomSparkModeRect(hWnd))) hitId = ID_SPARK_POLL_MODE;
    else if (hit(Config_CustomSparkRowsRect(hWnd))) hitId = ID_SPARK_ROW_LIMIT;
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
        else if (pressed == ID_SPARK_POLL_MODE)
        {
            UINT cur = Settings_GetSparkPollMode();
            UINT next = (cur >= SettingsSparkPollMode_MaxBurst) ? SettingsSparkPollMode_Safe : (cur + 1u);
            Settings_SetSparkPollMode(next);
            RequestSave(hWnd);
        }
        else if (pressed == ID_SPARK_ROW_LIMIT)
        {
            UINT cur = Settings_GetSparkRowLimit();
            UINT next = (cur >= 8u) ? 0u : (cur + 1u);
            Settings_SetSparkRowLimit(next);
            RequestSave(hWnd);
        }
        else
        {
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
        KeySettingsPanel_UpdateCustomControlsLayout(hWnd);
    else
        Config_RequestFullRepaint(hWnd);
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
    CustomPageSurface_DrawScrollbar(hWnd, hdc, &st->surface, st->scrollDrag);
}

static std::wstring SanitizePresetNameForFile(const std::wstring& in)
{
    std::wstring s = in;

    while (!s.empty() && s.back() == L' ') s.pop_back();
    while (!s.empty() && s.front() == L' ') s.erase(s.begin());

    if (s.size() >= 4)
    {
        const wchar_t* tail = s.c_str() + (s.size() - 4);
        if (_wcsicmp(tail, L".ini") == 0)
            s.resize(s.size() - 4);
    }

    const wchar_t* bad = L"<>:\"/\\|?*";
    for (size_t i = 0; i < s.size(); ++i)
    {
        if (wcschr(bad, s[i]) || s[i] < 32)
            s[i] = L'_';
    }

    while (!s.empty() && (s.back() == L' ' || s.back() == L'.')) s.pop_back();

    return s;
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

    st->toastHideAt = GetTickCount() + TOAST_SHOW_MS;
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

LRESULT CALLBACK KeyboardSubpages_ConfigPageProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    auto* st = (ConfigPageState*)GetWindowLongPtrW(hWnd, GWLP_USERDATA);

    if (msg == WM_APP_CONFIG_PROFILE_APPLIED)
    {
        if (st)
            Config_RefreshFromCurrentSettings(hWnd, st);
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
            DWORD now = GetTickCount();
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

        if (_wcsicmp(safe.c_str(), p.name.c_str()) == 0)
        {
            SetProfileStatus(st, L"");
            KeySettingsPanel_HandleCommand(hWnd, 9999, 0);
            return 0;
        }

        fs::path oldPath = fs::path(p.path);
        fs::path dir = oldPath.parent_path();
        fs::path newPath = dir / (safe + L".ini");

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
        if (!active.empty() && _wcsicmp(active.c_str(), p.name.c_str()) == 0)
        {
            if (!KeyboardProfiles::SavePreset(newPath.wstring(), curCurve))
            {
                SetProfileStatus(st, L"Rename failed: could not save new preset.");
                KeySettingsPanel_HandleCommand(hWnd, 9999, 0);
                return 0;
            }

            DeleteFileW(oldPath.wstring().c_str());

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
    case WM_ERASEBKGND: return 1;

    case WM_PAINT:
    {
        uint64_t paintStart = CustomPageSurface_QpcNow();
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);
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
            if (CustomPageSurface_RenderCache(hWnd, memDC, &st->surface, Config_RenderCacheContent, st))
            {
                HDC cacheDC = CreateCompatibleDC(memDC);
                if (cacheDC)
                {
                    HGDIOBJ old = SelectObject(cacheDC, st->surface.contentCache);
                    int copyW = std::min((int)(rc.right - rc.left), st->surface.cacheWidth);
                    int copyH = std::min((int)(rc.bottom - rc.top), std::max(0, st->surface.cacheHeight - st->surface.scrollY));
                    if (copyW > 0 && copyH > 0)
                        BitBlt(memDC, 0, 0, copyW, copyH, cacheDC, 0, st->surface.scrollY, SRCCOPY);
                    SelectObject(cacheDC, old);
                    DeleteDC(cacheDC);
                }
            }
            st->scrollY = st->surface.scrollY;
            DrawConfigScrollbar(hWnd, memDC, st);
        }
        else
        {
            KeySettingsPanel_DrawGraph(memDC, rc);
            DrawCpWeightHintIfNeeded(hWnd, memDC);
            KeySettingsPanel_DrawControls(hWnd, memDC);
            Config_DrawCustomControls(hWnd, memDC, st);
            DrawConfigScrollbar(hWnd, memDC, st);
        }

        BitBlt(hdc, 0, 0, rc.right - rc.left, rc.bottom - rc.top, memDC, 0, 0, SRCCOPY);
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
            POINT pt{ (short)LOWORD(lParam), (short)HIWORD(lParam) };
            RECT thumb = Config_GetScrollThumbRect(hWnd, st);
            RECT track = Config_GetScrollTrackRect(hWnd);
            int maxScroll = Config_GetMaxScroll(hWnd, st);

            if (maxScroll > 0 && PtInRect(&thumb, pt))
            {
                st->scrollDrag = true;
                st->scrollDragStartY = pt.y;
                st->scrollDragStartScrollY = st->scrollY;
                st->scrollDragGrabOffsetY = pt.y - thumb.top;
                st->scrollDragThumbHeight = std::max(1, (int)thumb.bottom - (int)thumb.top);
                st->scrollDragMax = maxScroll;
                SetCapture(hWnd);
                Config_MarkSurfaceDirty(hWnd, st);
                return 0;
            }

            if (maxScroll > 0 && PtInRect(&track, pt))
            {
                if (pt.y < thumb.top)
                    Config_SetScrollY(hWnd, st, st->scrollY - std::max(1, Config_GetViewportHeight(hWnd) - S(hWnd, 48)));
                else if (pt.y >= thumb.bottom)
                    Config_SetScrollY(hWnd, st, st->scrollY + std::max(1, Config_GetViewportHeight(hWnd) - S(hWnd, 48)));
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
        if (st && st->scrollDrag)
        {
            POINT pt{ (short)LOWORD(lParam), (short)HIWORD(lParam) };
            RECT track = Config_GetScrollTrackRect(hWnd);
            int trackH = std::max(1, (int)track.bottom - (int)track.top);
            int thumbH = std::max(1, st->scrollDragThumbHeight);
            int travel = std::max(1, trackH - thumbH);
            int maxScroll = std::max(1, st->scrollDragMax);

            int topWanted = pt.y - st->scrollDragGrabOffsetY;
            int topMin = (int)track.top;
            int topMax = (int)track.bottom - thumbH;
            if (topMax < topMin) topMax = topMin;
            int top = std::clamp(topWanted, topMin, topMax);
            double t = (double)(top - topMin) / (double)travel;
            int target = (int)std::lround(t * (double)maxScroll);

            Config_SetScrollY(hWnd, st, target);
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
        if (st && st->scrollDrag)
        {
            st->scrollDrag = false;
            if (GetCapture() == hWnd) ReleaseCapture();
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
            int delta = GET_WHEEL_DELTA_WPARAM(wParam);
            int lines = 3;
            SystemParametersInfoW(SPI_GETWHEELSCROLLLINES, 0, &lines, 0);
            if (lines <= 0) lines = 3;

            int linePx = S(hWnd, 18);
            int step = std::max(S(hWnd, 24), lines * linePx);
            int next = st->scrollY - ((delta / WHEEL_DELTA) * step);
            Config_SetScrollY(hWnd, st, next);
            return 0;
        }
        break;
    }

    case WM_CAPTURECHANGED:
    {
        if (st && st->scrollDrag)
        {
            st->scrollDrag = false;
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
        HWND hCombo = GetPresetCombo(hWnd);

        bool ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;

        bool comboOpen = (hCombo && PremiumCombo::GetDroppedState(hCombo));
        bool comboEditing = (hCombo && PremiumCombo::IsEditingItem(hCombo));
        bool allowNonCtrl = comboOpen || comboEditing;

        // Ctrl+S = save preset
        if (ctrl && (wParam == 'S' || wParam == 's'))
        {
            if (hCombo)
            {
                WPARAM wp = MAKEWPARAM((UINT)PremiumCombo::ExtraIconKind::Save, (UINT)KSP_ID_PROFILE);
                PostMessageW(hWnd, PremiumCombo::MsgExtraIcon(), wp, (LPARAM)hCombo);
            }
            return 0;
        }

        // Undo/redo shortcuts
        if (KeySettingsPanel_HandleKey(hWnd, msg, wParam, lParam))
        {
            Config_MarkSurfaceDirty(hWnd, st);
            return 0;
        }

        // Non-ctrl keys: do nothing unless dropdown is open
        if (!ctrl && !allowNonCtrl)
            return 0;

        bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;

        // F2 = rename selected (inline) [only when dropdown open]
        if (wParam == VK_F2)
        {
            if (hCombo)
            {
                int sel = PremiumCombo::GetCurSel(hCombo);
                int cnt = PremiumCombo::GetCount(hCombo);

                if (cnt > 0 && sel == cnt - 1)
                {
                    PostMessageW(hWnd, WM_APP_PROFILE_BEGIN_CREATE, 0, 0);
                }
                else
                {
                    PremiumCombo::BeginInlineEditSelected(hCombo, true);
                    SetProfileStatus(st, L"Type a new name and press Enter.");
                    Config_MarkSurfaceDirty(hWnd, st);
                }
            }
            return 0;
        }

        // Delete = delete selected (require Shift) [only when dropdown open]
        if (wParam == VK_DELETE)
        {
            if (hCombo)
            {
                int sel = PremiumCombo::GetCurSel(hCombo);
                int cnt = PremiumCombo::GetCount(hCombo);
                if (cnt > 0 && sel == cnt - 1)
                    return 0;

                if (!shift)
                {
                    SetProfileStatus(st, L"Hold Shift and press Delete to delete.");
                    Config_MarkSurfaceDirty(hWnd, st);
                    return 0;
                }

                DeletePreset_NoPopup_ConfigPage(hWnd, st, sel, true);
                Config_MarkSurfaceDirty(hWnd, st);
            }
            return 0;
        }

        return 0;
    }

    case WM_COMMAND:
    {
        if (st) DeleteConfirm_Clear(hWnd, st);

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
            SnappyToggle_StartAnim(st->chkBlockBoundKeys, on, true);
            RequestSave(hWnd);
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

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <d2d1.h>
#include <d2d1_1.h>
#include <d2d1effects.h>
#include <commctrl.h>
#include <cstring>
#include <algorithm>
#include <cmath>
#include <vector>

#include "tab_transition.h"
#include "keyboard_canvas.h"
#include "custom_page_surface.h"
#include "ui_theme.h"
#include "perf_trace.h"

#pragma comment(lib, "dxguid.lib")

namespace halljoy::tab_transition {
namespace {

// One captured page: CPU pixels survive a Direct2D device loss; the GPU
// bitmap is created lazily for the current render target.
struct Snapshot {
    int width = 0, height = 0;
    std::vector<std::uint32_t> pixels;
    bool fresh = false; // captured during the running transition
    ID2D1Bitmap* bitmap = nullptr;
    ID2D1RenderTarget* owner = nullptr;
    void ReleaseGpu() { if (bitmap) bitmap->Release(); bitmap = nullptr; owner = nullptr; }
};

// Private to the layer window (the canvas uses a registered message for frames).
constexpr UINT kFinishMessage = WM_APP + 0x3A1;
Host g_host;
HWND g_layer = nullptr;
std::vector<Snapshot> g_snapshots;
Motion g_motion;
LARGE_INTEGER g_start{}, g_frequency{};
bool g_active = false;
int g_target = -1;
float g_indicator = -1.0f;
LARGE_INTEGER g_lastFrame{};

// Motion blur: the strip is drawn into an off-screen GPU target and blurred
// horizontally by the distance the camera travels while a frame is "exposed"
// (half the measured frame time, a 180-degree shutter). Zero speed at both
// ends of the motion means the first and last frames are sharp.
constexpr double kShutter = 0.5;
constexpr float kMaxBlurSigma = 48.0f;
struct BlurPipeline {
    ID2D1RenderTarget* owner = nullptr;
    ID2D1DeviceContext* dc = nullptr;
    ID2D1BitmapRenderTarget* offscreen = nullptr;
    ID2D1Effect* blur = nullptr;
    UINT32 width = 0, height = 0;
    bool unsupported = false;
    void Release() {
        if (blur) blur->Release();
        if (offscreen) offscreen->Release();
        if (dc) dc->Release();
        blur = nullptr; offscreen = nullptr; dc = nullptr; owner = nullptr; width = height = 0;
    }
};
BlurPipeline g_blur;

double Seconds() {
    LARGE_INTEGER now{}; QueryPerformanceCounter(&now);
    return double(now.QuadPart - g_start.QuadPart) / double(g_frequency.QuadPart);
}

bool AnimationsEnabled() {
    BOOL enabled = TRUE;
    if (!SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION, 0, &enabled, 0)) enabled = TRUE;
    return enabled != FALSE;
}

// Page area of the tab control in the layer parent's client coordinates.
RECT PageArea() {
    RECT rc{};
    GetClientRect(g_host.tab, &rc);
    TabCtrl_AdjustRect(g_host.tab, FALSE, &rc);
    MapWindowPoints(g_host.tab, g_host.parent, reinterpret_cast<POINT*>(&rc), 2);
    return rc;
}

int Gap(int width) {
    const UINT dpi = GetDpiForWindow(g_host.parent);
    return std::min(MulDiv(24, dpi ? int(dpi) : 96, 96), std::max(0, width / 8));
}

void ReleaseAll() {
    for (auto& s : g_snapshots) { s.ReleaseGpu(); s.pixels.clear(); s.pixels.shrink_to_fit(); s.width = s.height = 0; s.fresh = false; }
}
// End of a transition: GPU copies go; CPU pixels stay as the cache for the
// pages a later transition passes (they are re-rendered when shown or sized).
void ReleaseGpu() {
    for (auto& s : g_snapshots) { s.ReleaseGpu(); s.fresh = false; }
}

// How a page is captured:
//  Fresh  - always re-rendered, the way it looks now. The shown and the target
//           page (seen sharp at the start and the end). A hidden target is
//           shown for it (it stays shown behind the layer), so its usual
//           on-show refresh runs.
//  Cached - a snapshot of the right size is reused (pages between the two,
//           seen briefly and blurred). Otherwise the hidden page is rendered
//           as it is, without showing it: no on-show relayout, much cheaper.
enum class Mode { Fresh, Cached };

void Capture(int index, Mode mode, bool showForCapture) {
    if (index < 0 || index >= g_host.count) return;
    RECT area = PageArea();
    const int width = std::max<LONG>(1, area.right - area.left), height = std::max<LONG>(1, area.bottom - area.top);
    Snapshot& snap = g_snapshots[static_cast<std::size_t>(index)];
    const bool sized = snap.width == width && snap.height == height && !snap.pixels.empty();
    if (sized && (mode == Mode::Cached || snap.fresh)) return;
    const auto started = halljoy::perf::Now();
    snap.ReleaseGpu();
    snap.width = width; snap.height = height;
    snap.pixels.assign(static_cast<std::size_t>(width) * height, 0);
    HWND page = g_host.page ? g_host.page(index) : nullptr;
    HDC screen = GetDC(g_host.parent);
    HDC dc = CreateCompatibleDC(screen);
    BITMAPINFO bi{};
    bi.bmiHeader.biSize = sizeof(bi.bmiHeader);
    bi.bmiHeader.biWidth = width; bi.bmiHeader.biHeight = -height;
    bi.bmiHeader.biPlanes = 1; bi.bmiHeader.biBitCount = 32; bi.bmiHeader.biCompression = BI_RGB;
    void* bits = nullptr;
    HBITMAP dib = CreateDIBSection(screen, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (dc && dib) {
        HGDIOBJ old = SelectObject(dc, dib);
        RECT all{ 0, 0, width, height };
        FillRect(dc, &all, UiTheme::Brush_PanelBg());
        if (page) {
            if (showForCapture && !(GetWindowLongW(page, GWL_STYLE) & WS_VISIBLE)) ShowWindow(page, SW_SHOWNA);
            CustomPage_PrintTree(page, dc);
        }
        GdiFlush();
        std::memcpy(snap.pixels.data(), bits, snap.pixels.size() * sizeof(std::uint32_t));
        SelectObject(dc, old);
    }
    if (dib) DeleteObject(dib);
    if (dc) DeleteDC(dc);
    ReleaseDC(g_host.parent, screen);
    snap.fresh = mode == Mode::Fresh;
    halljoy::perf::Span("ui.tab_transition.capture", static_cast<std::uint64_t>(index), started);
}

// Pages the camera shows at `pos` (one or two).
void EnsureVisible(double pos) {
    const int a = std::clamp(int(std::floor(pos)), 0, g_host.count - 1);
    const int b = std::clamp(int(std::ceil(pos)), 0, g_host.count - 1);
    Capture(a, a == g_target ? Mode::Fresh : Mode::Cached, a == g_target);
    if (b != a) Capture(b, b == g_target ? Mode::Fresh : Mode::Cached, b == g_target);
}

// After a frame: render at most one missing page ahead of the camera, so it
// is ready before it enters the view (one page per frame, nearest first).
void Prefetch(double pos) {
    const double target = g_motion.x1;
    const int step = target >= pos ? 1 : -1;
    for (int i = step > 0 ? int(std::ceil(pos)) : int(std::floor(pos)); step > 0 ? i <= int(target) : i >= int(target); i += step) {
        if (i < 0 || i >= g_host.count) continue;
        const Snapshot& snap = g_snapshots[static_cast<std::size_t>(i)];
        if (snap.pixels.empty()) { Capture(i, i == g_target ? Mode::Fresh : Mode::Cached, i == g_target); return; }
    }
}

ID2D1Bitmap* Bitmap(Snapshot& snap, ID2D1RenderTarget* rt) {
    if (snap.pixels.empty()) return nullptr;
    if (snap.bitmap && snap.owner == rt) return snap.bitmap;
    snap.ReleaseGpu();
    const auto props = D2D1::BitmapProperties(D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_IGNORE));
    if (SUCCEEDED(rt->CreateBitmap(D2D1::SizeU(UINT32(snap.width), UINT32(snap.height)), snap.pixels.data(),
            UINT32(snap.width * 4), props, &snap.bitmap))) snap.owner = rt;
    return snap.bitmap;
}

void Finish() {
    g_active = false;
    g_indicator = -1.0f;
    const int target = g_target;
    if (g_host.commit && target >= 0) g_host.commit(target);
    if (HWND page = g_host.page ? g_host.page(target) : nullptr)
        RedrawWindow(page, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN);
    // The layer's last frame is the target page at rest, so hiding it shows
    // identical pixels until the page repaints: no flash.
    ShowWindow(g_layer, SW_HIDE);
    ReleaseGpu();
    RECT strip{}; GetClientRect(g_host.tab, &strip);
    InvalidateRect(g_host.tab, &strip, FALSE);
}

void DrawStrip(ID2D1RenderTarget* target, ID2D1RenderTarget* owner, double pos, float width, float stride) {
    const COLORREF bg = UiTheme::Color_PanelBg();
    target->Clear(D2D1::ColorF(GetRValue(bg) / 255.0f, GetGValue(bg) / 255.0f, GetBValue(bg) / 255.0f));
    const int lo = std::max(0, int(std::floor(pos)) - 1), hi = std::min(g_host.count - 1, int(std::ceil(pos)) + 1);
    for (int i = lo; i <= hi; ++i) {
        const float x = float((i - pos) * stride);
        if (x >= width || x + width <= 0) continue;
        Snapshot& snap = g_snapshots[static_cast<std::size_t>(i)];
        // Bitmaps belong to the window target; its compatible off-screen
        // target shares the device, so they draw in either.
        if (ID2D1Bitmap* bitmap = Bitmap(snap, owner)) {
            const D2D1_RECT_F dst = D2D1::RectF(x, 0, x + float(snap.width), float(snap.height));
            target->DrawBitmap(bitmap, dst, 1.0f, D2D1_BITMAP_INTERPOLATION_MODE_LINEAR);
        }
    }
}

bool EnsureBlur(ID2D1RenderTarget* rt, UINT32 width, UINT32 height) {
    if (g_blur.owner != rt) {
        g_blur.Release();
        g_blur.unsupported = false;
        g_blur.owner = rt;
        if (FAILED(rt->QueryInterface(__uuidof(ID2D1DeviceContext), reinterpret_cast<void**>(&g_blur.dc))) ||
            FAILED(g_blur.dc->CreateEffect(CLSID_D2D1DirectionalBlur, &g_blur.blur))) {
            g_blur.unsupported = true; // older systems: transitions stay sharp
            return false;
        }
        g_blur.blur->SetValue(D2D1_DIRECTIONALBLUR_PROP_ANGLE, 0.0f);
        g_blur.blur->SetValue(D2D1_DIRECTIONALBLUR_PROP_BORDER_MODE, D2D1_BORDER_MODE_HARD);
        g_blur.blur->SetValue(D2D1_DIRECTIONALBLUR_PROP_OPTIMIZATION, D2D1_DIRECTIONALBLUR_OPTIMIZATION_BALANCED);
    }
    if (g_blur.unsupported) return false;
    if (!g_blur.offscreen || g_blur.width != width || g_blur.height != height) {
        if (g_blur.offscreen) g_blur.offscreen->Release();
        g_blur.offscreen = nullptr;
        if (FAILED(rt->CreateCompatibleRenderTarget(D2D1::SizeF(float(width), float(height)), &g_blur.offscreen)))
            return false;
        g_blur.width = width; g_blur.height = height;
    }
    return true;
}

bool Paint(HWND layer, ID2D1RenderTarget* rt, void*) {
    if (!g_active) {
        const COLORREF bg = UiTheme::Color_PanelBg();
        rt->Clear(D2D1::ColorF(GetRValue(bg) / 255.0f, GetGValue(bg) / 255.0f, GetBValue(bg) / 255.0f));
        return false;
    }
    const double t = Seconds();
    const double pos = g_motion.Position(t);
    g_indicator = static_cast<float>(pos);
    RECT client{}; GetClientRect(layer, &client);
    const float width = float(client.right);
    const float stride = width + float(Gap(client.right));

    // Measured frame time (first frame: one 60 Hz period), within sane bounds.
    LARGE_INTEGER now{}; QueryPerformanceCounter(&now);
    double frame = g_lastFrame.QuadPart ? double(now.QuadPart - g_lastFrame.QuadPart) / double(g_frequency.QuadPart) : 1.0 / 60;
    g_lastFrame = now;
    frame = std::clamp(frame, 1.0 / 240, 1.0 / 30);
    const double blurLength = std::abs(g_motion.Velocity(t)) * stride * frame * kShutter; // pixels
    // A Gaussian of sigma ~0.35 L looks like a box streak of length L.
    const float sigma = std::min(kMaxBlurSigma, float(blurLength * 0.35));

    EnsureVisible(pos); // normally prefetched already
    bool drawn = false;
    if (sigma >= 0.5f && EnsureBlur(rt, UINT32(std::max<LONG>(1, client.right)), UINT32(std::max<LONG>(1, client.bottom)))) {
        g_blur.offscreen->BeginDraw();
        DrawStrip(g_blur.offscreen, rt, pos, width, stride);
        ID2D1Bitmap* strip = nullptr;
        if (SUCCEEDED(g_blur.offscreen->EndDraw()) && SUCCEEDED(g_blur.offscreen->GetBitmap(&strip))) {
            g_blur.blur->SetInput(0, strip);
            g_blur.blur->SetValue(D2D1_DIRECTIONALBLUR_PROP_STANDARD_DEVIATION, sigma);
            g_blur.dc->DrawImage(g_blur.blur);
            g_blur.blur->SetInput(0, nullptr);
            strip->Release();
            drawn = true;
        }
    }
    if (!drawn) DrawStrip(rt, rt, pos, width, stride);
    Prefetch(pos);

    // The tab indicator follows the camera on the same frame.
    RECT strip{}; GetClientRect(g_host.tab, &strip);
    RECT pageRc = strip; TabCtrl_AdjustRect(g_host.tab, FALSE, &pageRc);
    strip.bottom = pageRc.top;
    RedrawWindow(g_host.tab, &strip, nullptr, RDW_INVALIDATE | RDW_UPDATENOW);
    if (g_motion.Done(t)) {
        // Draw this last frame exactly at rest, then hand over to the page.
        PostMessageW(layer, kFinishMessage, 0, 0);
        return false;
    }
    return true;
}

LRESULT CALLBACK LayerSubclass(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam, UINT_PTR, DWORD_PTR) {
    if (msg == kFinishMessage) { if (g_active) Finish(); return 0; }
    // The page under the moving picture must not receive blind clicks; the
    // tab strip (outside the layer) stays live for quick switching.
    if (msg == WM_NCHITTEST && g_active) return HTCLIENT;
    if (msg == WM_SETCURSOR && g_active) { SetCursor(LoadCursorW(nullptr, IDC_ARROW)); return TRUE; }
    if (msg == WM_NCDESTROY) RemoveWindowSubclass(hwnd, LayerSubclass, 1);
    return DefSubclassProc(hwnd, msg, wParam, lParam);
}

} // namespace

void Initialize(const Host& host) {
    g_host = host;
    QueryPerformanceFrequency(&g_frequency);
    g_snapshots.assign(static_cast<std::size_t>(std::max(0, host.count)), Snapshot{});
    g_layer = keyboard_canvas::Create(host.parent, Paint, nullptr);
    if (g_layer) {
        SetWindowSubclass(g_layer, LayerSubclass, 1, 0);
        ShowWindow(g_layer, SW_HIDE);
    }
}

void Shutdown() {
    g_active = false;
    ReleaseAll();
    g_blur.Release();
    if (g_layer && IsWindow(g_layer)) keyboard_canvas::Destroy(g_layer);
    g_layer = nullptr;
}

bool SwitchTo(int shown, int target) {
    if (!g_layer || target < 0 || target >= g_host.count) return false;
    const HWND root = GetAncestor(g_host.parent, GA_ROOT);
    if (!AnimationsEnabled() || !IsWindowVisible(root) || IsIconic(root)) {
        if (g_active) { g_target = target; Finish(); return true; }
        return false;
    }
    const double t = g_active ? Seconds() : 0.0;
    if (g_active && target == g_target) return true;
    const auto started = halljoy::perf::Now();
    Motion rest;
    rest.x0 = rest.x1 = g_active ? g_motion.Position(t) : double(shown);
    rest.duration = 0;
    const Motion from = g_active ? g_motion : rest;
    const double fromTime = g_active ? t : 0.0;
    const bool starting = !g_active;
    if (starting && shown == target) return false;
    if (starting) Capture(shown, Mode::Fresh, false); // while it is still on screen
    if (!starting && g_target >= 0 && g_target != target) {
        // The previous target was kept shown under the layer; it is no longer needed.
        if (HWND old = g_host.page ? g_host.page(g_target) : nullptr) ShowWindow(old, SW_HIDE);
    }
    g_motion = Retarget(from, fromTime, double(target));
    QueryPerformanceCounter(&g_start);
    g_target = target;
    g_active = true;
    // The tab strip may repaint before the first frame: it must already show
    // the camera's start, not jump to the newly selected tab and back.
    g_indicator = static_cast<float>(g_motion.x0);
    if (starting) {
        // The first frame shows exactly the current page, drawn before the
        // layer becomes visible to the user: nothing below changes visibly.
        const RECT area = PageArea();
        SetWindowPos(g_layer, HWND_TOP, area.left, area.top, area.right - area.left, area.bottom - area.top,
            SWP_NOACTIVATE | SWP_SHOWWINDOW);
        UpdateWindow(g_layer);
    }
    // Behind the layer: the target (sharp at the end), whatever the camera
    // shows now, and the next page in the direction of travel. Pages further
    // along are prefetched one per frame while the camera accelerates.
    Capture(target, Mode::Fresh, true);
    EnsureVisible(g_motion.x0);
    const int next = g_motion.x1 > g_motion.x0 ? int(std::floor(g_motion.x0)) + 1 : int(std::ceil(g_motion.x0)) - 1;
    if (next != target) Capture(next, Mode::Cached, false);
    // Only the target page stays shown (behind the layer) until the handover.
    for (int i = 0; i < g_host.count; ++i)
        if (i != target) if (HWND page = g_host.page ? g_host.page(i) : nullptr) ShowWindow(page, SW_HIDE);
    if (HWND page = g_host.page ? g_host.page(target) : nullptr) ShowWindow(page, SW_SHOWNA);
    halljoy::perf::Span("ui.tab_transition.start", static_cast<std::uint64_t>(target), started);
    // Captures do not eat into the transition time: the camera continues from where it
    // was when the tab was clicked.
    QueryPerformanceCounter(&g_start);
    g_lastFrame.QuadPart = 0;
    keyboard_canvas::RequestFrame(g_layer);
    return true;
}

void InvalidateCache() {
    if (!g_active) ReleaseAll();
}

void JumpTo(int index) {
    if (!g_active) return;
    g_target = index;
    Finish();
}

void OnLayout() {
    if (!g_layer || !g_active) return;
    const RECT area = PageArea();
    SetWindowPos(g_layer, HWND_TOP, area.left, area.top, area.right - area.left, area.bottom - area.top,
        SWP_NOACTIVATE);
    // Pages are already laid out for the new size: capture them again (the
    // rest of the path is prefetched by the frames).
    ReleaseAll();
    Capture(g_target, Mode::Fresh, true);
    EnsureVisible(g_motion.Position(Seconds()));
    keyboard_canvas::RequestFrame(g_layer);
}

bool Active() { return g_active; }
float IndicatorPosition() { return g_active ? g_indicator : -1.0f; }

} // namespace halljoy::tab_transition

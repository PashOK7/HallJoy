#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

#include <algorithm>
#include <atomic>
#include <climits>
#include <charconv>
#include <cstdint>
#include <cwchar>
#include <mutex>
#include <string>
#include <vector>

#include "overlay_server.h"
#include "backend.h"
#include "keyboard_layout.h"
#include "debug_log.h"
#include "stability_trace.h"
#include "worker_exception_barrier.h"

static std::atomic<bool> g_overlayRunning{ false };
static std::atomic<bool> g_overlayWsaStarted{ false };
static std::atomic<bool> g_overlayThreadExited{ true };
static std::atomic<halljoy::worker::WorkerExceptionKind> g_overlayFaultKind{
    halljoy::worker::WorkerExceptionKind::None };
static halljoy::worker::WorkerExceptionRecord g_overlayFaultRecord{};
static std::atomic<uint16_t> g_overlayPort{ 0 };
static std::atomic<uint16_t> g_overlayConfiguredPort{ 8765 };
static std::atomic<int> g_overlayFillDirection{ (int)OverlayFillDirection::TopDown };
static std::atomic<uint32_t> g_overlayEffectFlags{
    OverlayEffect_Smoothing |
    OverlayEffect_Glass |
    OverlayEffect_Bloom |
    OverlayEffect_EdgeSweep |
    OverlayEffect_MicroScale |
    OverlayEffect_LabelContrast |
    OverlayEffect_GlassRimLight
};
static std::atomic<uint32_t> g_overlayAccentColor{ 0x00ff72u };
static std::atomic<int> g_overlaySmoothingStrengthPercent{ 39 };
static std::atomic<int> g_overlayGlassStrengthPercent{ 50 };
static std::atomic<int> g_overlayBloomStrengthPercent{ 33 };
static std::atomic<int> g_overlayEdgeStrengthPercent{ 50 };
static std::atomic<int> g_overlayScaleStrengthPercent{ 50 };
static std::atomic<int> g_overlayLabelStrengthPercent{ 0 };
static std::atomic<int> g_overlayGlassRimStrengthPercent{ 50 };
static std::atomic<int> g_overlayRefreshIntervalMs{ 1 };
static std::atomic<bool> g_overlayAutoStart{ false };
static std::atomic<bool> g_overlayUseRawDepth{ true };
static std::atomic<int> g_overlayLabelFontIndex{ 0 };
static std::atomic<int> g_overlayLabelSizePx{ 13 };
static std::atomic<int> g_overlayLabelShadowPercent{ 50 };
static std::atomic<uint32_t> g_overlayLabelColor{ 0xffffffu };
static HANDLE g_overlayThread = nullptr;
static SOCKET g_overlayListenSocket = INVALID_SOCKET;
static SOCKET g_overlayClientSocket = INVALID_SOCKET;
static std::mutex g_overlaySocketMutex;
static std::mutex g_overlayStateMutex;
static std::wstring g_overlayLastError;

static std::atomic<uint64_t> g_perfStateRequests{ 0 };
static std::atomic<uint64_t> g_perfStateBytes{ 0 };
static std::atomic<uint64_t> g_perfBuildUsTotal{ 0 };
static std::atomic<uint64_t> g_perfBuildUsMax{ 0 };
static std::atomic<uint64_t> g_perfSendUsTotal{ 0 };
static std::atomic<uint64_t> g_perfSendUsMax{ 0 };
static std::atomic<uint64_t> g_perfHttpRequests{ 0 };
static std::atomic<uint64_t> g_perfHttpConnections{ 0 };
static std::atomic<uint64_t> g_perfClientFrames{ 0 };
static std::atomic<uint64_t> g_perfClientFetchUs{ 0 };
static std::atomic<uint64_t> g_perfClientFetches{ 0 };
static std::atomic<uint64_t> g_perfClientRenderUs{ 0 };
static std::atomic<uint64_t> g_perfClientLayoutUs{ 0 };
static std::atomic<uint64_t> g_perfClientSpriteHits{ 0 };
static std::atomic<uint64_t> g_perfClientSpriteMisses{ 0 };
static std::atomic<uint64_t> g_perfClientLabelMisses{ 0 };
static std::atomic<uint64_t> g_perfClientSpriteBuildUs{ 0 };
static std::atomic<uint64_t> g_perfClientLabelBuildUs{ 0 };
static std::atomic<uint64_t> g_perfClientReports{ 0 };
static std::atomic<ULONGLONG> g_perfLastLogMs{ 0 };

static void OverlayPerfLogFile(const wchar_t* line);

static uint64_t OverlayNowUs()
{
    static LARGE_INTEGER freq = [] { LARGE_INTEGER f{}; QueryPerformanceFrequency(&f); return f; }();
    LARGE_INTEGER now{};
    QueryPerformanceCounter(&now);
    return (uint64_t)((now.QuadPart * 1000000ull) / (uint64_t)freq.QuadPart);
}

static void OverlayAtomicMax(std::atomic<uint64_t>& target, uint64_t value)
{
    uint64_t old = target.load(std::memory_order_relaxed);
    while (value > old && !target.compare_exchange_weak(old, value, std::memory_order_relaxed))
    {
    }
}

static void OverlayPerfMaybeLog()
{
    ULONGLONG now = GetTickCount64();
    ULONGLONG last = g_perfLastLogMs.load(std::memory_order_relaxed);
    if (now - last < 5000)
        return;
    if (!g_perfLastLogMs.compare_exchange_strong(last, now, std::memory_order_relaxed))
        return;

    uint64_t stateReq = g_perfStateRequests.exchange(0, std::memory_order_relaxed);
    uint64_t stateBytes = g_perfStateBytes.exchange(0, std::memory_order_relaxed);
    uint64_t buildUs = g_perfBuildUsTotal.exchange(0, std::memory_order_relaxed);
    uint64_t buildMax = g_perfBuildUsMax.exchange(0, std::memory_order_relaxed);
    uint64_t sendUs = g_perfSendUsTotal.exchange(0, std::memory_order_relaxed);
    uint64_t sendMax = g_perfSendUsMax.exchange(0, std::memory_order_relaxed);
    uint64_t httpReq = g_perfHttpRequests.exchange(0, std::memory_order_relaxed);
    uint64_t httpConn = g_perfHttpConnections.exchange(0, std::memory_order_relaxed);
    uint64_t clientFrames = g_perfClientFrames.exchange(0, std::memory_order_relaxed);
    uint64_t clientFetch = g_perfClientFetchUs.exchange(0, std::memory_order_relaxed);
    uint64_t clientFetches = g_perfClientFetches.exchange(0, std::memory_order_relaxed);
    uint64_t clientRender = g_perfClientRenderUs.exchange(0, std::memory_order_relaxed);
    uint64_t clientLayout = g_perfClientLayoutUs.exchange(0, std::memory_order_relaxed);
    uint64_t clientSpriteHits = g_perfClientSpriteHits.exchange(0, std::memory_order_relaxed);
    uint64_t clientSpriteMisses = g_perfClientSpriteMisses.exchange(0, std::memory_order_relaxed);
    uint64_t clientLabelMisses = g_perfClientLabelMisses.exchange(0, std::memory_order_relaxed);
    uint64_t clientSpriteBuild = g_perfClientSpriteBuildUs.exchange(0, std::memory_order_relaxed);
    uint64_t clientLabelBuild = g_perfClientLabelBuildUs.exchange(0, std::memory_order_relaxed);
    uint64_t clientReports = g_perfClientReports.exchange(0, std::memory_order_relaxed);

    double avgBuild = stateReq ? (double)buildUs / (double)stateReq : 0.0;
    double avgSend = stateReq ? (double)sendUs / (double)stateReq : 0.0;
    double avgBytes = stateReq ? (double)stateBytes / (double)stateReq : 0.0;
    double avgFetch = clientFetches ? (double)clientFetch / (double)clientFetches : 0.0;
    double avgRender = clientFrames ? (double)clientRender / (double)clientFrames : 0.0;
    double avgLayout = clientFrames ? (double)clientLayout / (double)clientFrames : 0.0;
    double avgSpriteBuild = clientSpriteMisses ? (double)clientSpriteBuild / (double)clientSpriteMisses : 0.0;
    double avgLabelBuild = clientLabelMisses ? (double)clientLabelBuild / (double)clientLabelMisses : 0.0;

    wchar_t line[768]{};
    swprintf_s(
        line,
        L"[overlay.perf] http_req=%llu conn=%llu state=%llu avg_bytes=%.0f build_us_avg=%.1f build_us_max=%llu send_us_avg=%.1f send_us_max=%llu client_reports=%llu client_frames=%llu client_fetches=%llu client_fetch_us_avg=%.1f client_render_us_avg=%.1f client_layout_us_avg=%.1f sprite_hits=%llu sprite_misses=%llu label_misses=%llu sprite_build_us_avg=%.1f label_build_us_avg=%.1f sprite_build_us_total=%llu label_build_us_total=%llu",
        (unsigned long long)httpReq,
        (unsigned long long)httpConn,
        (unsigned long long)stateReq,
        avgBytes,
        avgBuild,
        (unsigned long long)buildMax,
        avgSend,
        (unsigned long long)sendMax,
        (unsigned long long)clientReports,
        (unsigned long long)clientFrames,
        (unsigned long long)clientFetches,
        avgFetch,
        avgRender,
        avgLayout,
        (unsigned long long)clientSpriteHits,
        (unsigned long long)clientSpriteMisses,
        (unsigned long long)clientLabelMisses,
        avgSpriteBuild,
        avgLabelBuild,
        (unsigned long long)clientSpriteBuild,
        (unsigned long long)clientLabelBuild);
    DebugLog_Write(L"%s", line);
    OverlayPerfLogFile(line);
}

static void AppendUInt(std::string& out, unsigned value)
{
    char buf[16]{};
    auto r = std::to_chars(buf, buf + sizeof(buf), value);
    out.append(buf, r.ptr);
}

static void AppendUInt64(std::string& out, unsigned long long value)
{
    char buf[32]{};
    auto r = std::to_chars(buf, buf + sizeof(buf), value);
    out.append(buf, r.ptr);
}

static std::wstring OverlayPerfLogPath()
{
    std::wstring path;
    std::vector<wchar_t> buf(1024);
    DWORD len = 0;
    for (;;)
    {
        len = GetModuleFileNameW(nullptr, buf.data(), (DWORD)buf.size());
        if (len == 0)
            return L"overlay_perf.log";
        if (len < buf.size())
            break;
        if (buf.size() > 65536)
            return L"overlay_perf.log";
        buf.resize(buf.size() * 2);
    }
    path.assign(buf.data(), len);
    size_t slash = path.find_last_of(L"\\/");
    if (slash != std::wstring::npos)
        path.erase(slash + 1);
    else
        path.clear();
    path += L"overlay_perf.log";
    return path;
}

static void OverlayPerfLogFile(const wchar_t* line)
{
    if (!line || !*line)
        return;
    static const std::wstring path = OverlayPerfLogPath();
    HANDLE h = CreateFileW(path.c_str(), FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE)
        return;

    SYSTEMTIME st{};
    GetLocalTime(&st);
    wchar_t prefix[64]{};
    swprintf_s(prefix, L"%04u-%02u-%02u %02u:%02u:%02u.%03u ",
        st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
    std::wstring full = prefix;
    full += line;
    full += L"\r\n";

    int need = WideCharToMultiByte(CP_UTF8, 0, full.c_str(), (int)full.size(), nullptr, 0, nullptr, nullptr);
    if (need > 0)
    {
        std::string utf8((size_t)need, '\0');
        WideCharToMultiByte(CP_UTF8, 0, full.c_str(), (int)full.size(), utf8.data(), need, nullptr, nullptr);
        DWORD written = 0;
        WriteFile(h, utf8.data(), (DWORD)utf8.size(), &written, nullptr);
    }
    CloseHandle(h);
}

static std::string Utf8FromWide(const wchar_t* w)
{
    if (!w || !*w) return {};

    const size_t charCount = wcslen(w);
    if (charCount > static_cast<size_t>(INT_MAX))
        return {};

    const int inputChars = static_cast<int>(charCount);
    const int need = WideCharToMultiByte(
        CP_UTF8,
        WC_ERR_INVALID_CHARS,
        w,
        inputChars,
        nullptr,
        0,
        nullptr,
        nullptr);
    if (need <= 0) return {};

    std::string out(static_cast<size_t>(need), '\0');
    const int written = WideCharToMultiByte(
        CP_UTF8,
        WC_ERR_INVALID_CHARS,
        w,
        inputChars,
        out.data(),
        need,
        nullptr,
        nullptr);
    if (written != need)
        return {};
    return out;
}

static std::string JsonEscape(const std::string& s)
{
    std::string out;
    out.reserve(s.size() + 8);
    for (unsigned char ch : s)
    {
        switch (ch)
        {
        case '\\': out += "\\\\"; break;
        case '"': out += "\\\""; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        default:
            if (ch < 0x20)
            {
                char b[8]{};
                sprintf_s(b, "\\u%04x", (unsigned)ch);
                out += b;
            }
            else
            {
                out.push_back((char)ch);
            }
            break;
        }
    }
    return out;
}

static void OverlaySetLastError(const wchar_t* msg)
{
    std::lock_guard<std::mutex> lock(g_overlayStateMutex);
    g_overlayLastError = msg ? msg : L"";
}

static std::string OverlayBuildStateJson()
{
    uint64_t beginUs = OverlayNowUs();
    OverlayFillDirection direction = OverlayServer_GetFillDirection();
    uint32_t flags = OverlayServer_GetEffectFlags();
    uint32_t color = OverlayServer_GetAccentColor();
    uint32_t labelColor = OverlayServer_GetLabelColor();
    int refreshMs = OverlayServer_GetRefreshIntervalMs();
    bool useRawDepth = OverlayServer_GetUseRawDepth();
    unsigned cr = (color >> 16) & 0xffu;
    unsigned cg = (color >> 8) & 0xffu;
    unsigned cb = color & 0xffu;
    unsigned lr = (labelColor >> 16) & 0xffu;
    unsigned lg = (labelColor >> 8) & 0xffu;
    unsigned lb = labelColor & 0xffu;

    const auto layout = KeyboardLayout_GetSnapshot();
    const size_t layoutKeyCount = layout ? layout->keys.size() : 0u;

    std::string ss;
    ss.reserve(2048 + layoutKeyCount * 96u);
    ss += "{\"tick\":";
    AppendUInt64(ss, (unsigned long long)GetTickCount64());
    ss += ",\"fillDirection\":\"";
    ss += (direction == OverlayFillDirection::TopDown ? "top-down" : "bottom-up");
    ss += "\",\"accent\":{\"r\":";
    AppendUInt(ss, cr);
    ss += ",\"g\":";
    AppendUInt(ss, cg);
    ss += ",\"b\":";
    AppendUInt(ss, cb);
    ss += "},\"effects\":{\"smoothing\":";
    ss += ((flags & OverlayEffect_Smoothing) ? "true" : "false");
    ss += ",\"glass\":";
    ss += ((flags & OverlayEffect_Glass) ? "true" : "false");
    ss += ",\"bloom\":";
    ss += ((flags & OverlayEffect_Bloom) ? "true" : "false");
    ss += ",\"edgeSweep\":";
    ss += ((flags & OverlayEffect_EdgeSweep) ? "true" : "false");
    ss += ",\"microScale\":";
    ss += ((flags & OverlayEffect_MicroScale) ? "true" : "false");
    ss += ",\"labelContrast\":";
    ss += ((flags & OverlayEffect_LabelContrast) ? "true" : "false");
    ss += ",\"glassRimLight\":";
    ss += ((flags & OverlayEffect_GlassRimLight) ? "true" : "false");
    ss += "},\"strengths\":{\"smoothing\":";
    AppendUInt(ss, (unsigned)OverlayServer_GetEffectStrengthPercent(OverlayEffect_Smoothing));
    ss += ",\"glass\":";
    AppendUInt(ss, (unsigned)OverlayServer_GetEffectStrengthPercent(OverlayEffect_Glass));
    ss += ",\"bloom\":";
    AppendUInt(ss, (unsigned)OverlayServer_GetEffectStrengthPercent(OverlayEffect_Bloom));
    ss += ",\"edgeSweep\":";
    AppendUInt(ss, (unsigned)OverlayServer_GetEffectStrengthPercent(OverlayEffect_EdgeSweep));
    ss += ",\"microScale\":";
    AppendUInt(ss, (unsigned)OverlayServer_GetEffectStrengthPercent(OverlayEffect_MicroScale));
    ss += ",\"labelContrast\":";
    AppendUInt(ss, (unsigned)OverlayServer_GetEffectStrengthPercent(OverlayEffect_LabelContrast));
    ss += ",\"glassRimLight\":";
    AppendUInt(ss, (unsigned)OverlayServer_GetEffectStrengthPercent(OverlayEffect_GlassRimLight));
    ss += "},\"settings\":{\"refreshMs\":";
    AppendUInt(ss, (unsigned)refreshMs);
    ss += ",\"depthSource\":\"";
    ss += (useRawDepth ? "raw" : "output");
    ss += "\",\"useRawDepth\":";
    ss += (useRawDepth ? "true" : "false");
    ss += "},\"labelStyle\":{\"font\":";
    AppendUInt(ss, (unsigned)OverlayServer_GetLabelFontIndex());
    ss += ",\"size\":";
    AppendUInt(ss, (unsigned)OverlayServer_GetLabelSizePx());
    ss += ",\"shadow\":";
    AppendUInt(ss, (unsigned)OverlayServer_GetLabelShadowPercent());
    ss += ",\"color\":{\"r\":";
    AppendUInt(ss, lr);
    ss += ",\"g\":";
    AppendUInt(ss, lg);
    ss += ",\"b\":";
    AppendUInt(ss, lb);
    ss += "}},\"keys\":[";

    static const std::vector<KeyDef> kEmptyKeys;
    const std::vector<KeyDef>& keys = layout ? layout->keys : kEmptyKeys;
    for (size_t i = 0; i < keys.size(); ++i)
    {
        const KeyDef& k = keys[i];
        uint16_t raw = (k.hid < 256) ? BackendUI_GetRawMilli(k.hid) : 0;
        uint16_t out = (k.hid < 256) ? BackendUI_GetAnalogMilli(k.hid) : 0;
        if (i != 0) ss += ',';
        ss += "{\"label\":\"";
        ss += JsonEscape(Utf8FromWide(k.label));
        ss += "\",\"hid\":";
        AppendUInt(ss, (unsigned)k.hid);
        ss += ",\"row\":";
        AppendUInt(ss, (unsigned)k.row);
        ss += ",\"x\":";
        AppendUInt(ss, (unsigned)k.x);
        ss += ",\"w\":";
        AppendUInt(ss, (unsigned)k.w);
        ss += ",\"h\":";
        AppendUInt(ss, (unsigned)k.h);
        ss += ",\"raw\":";
        AppendUInt(ss, (unsigned)raw);
        ss += ",\"out\":";
        AppendUInt(ss, (unsigned)out);
        ss += '}';
    }
    ss += "]}";

    uint64_t durUs = OverlayNowUs() - beginUs;
    g_perfStateRequests.fetch_add(1, std::memory_order_relaxed);
    g_perfStateBytes.fetch_add((uint64_t)ss.size(), std::memory_order_relaxed);
    g_perfBuildUsTotal.fetch_add(durUs, std::memory_order_relaxed);
    OverlayAtomicMax(g_perfBuildUsMax, durUs);
    return ss;
}

static const char* OverlayHtml()
{
    return R"HTML(<!doctype html>
<html>
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>HallJoy Input Overlay</title>
<style>
html,body{margin:0;background:transparent;overflow:hidden}
#c{position:fixed;inset:0;width:100vw;height:100vh;display:block}
</style>
</head>
<body>
<canvas id="c"></canvas>
<script>
const canvas=document.getElementById('c');
const ctx=canvas.getContext('2d',{alpha:true,desynchronized:true});
const displayed=new Map();
let latest=null,layoutKey='',lastRefreshMs=1;
let dpr=1,cw=0,ch=0,boardW=1,boardH=1,scale=1,ox=0,oy=0;
let polling=false;
let perfFrames=0,perfFetches=0,perfFetch=0,perfRender=0,perfLayout=0,lastPerfReport=performance.now();
let perfSpriteHits=0,perfSpriteMisses=0,perfLabelMisses=0,lastRenderTime=performance.now();
let perfSpriteBuild=0,perfLabelBuild=0;
const spriteCache=new Map();
const labelCache=new Map();
let spriteSerial=0;
const SPRITE_CACHE_MAX=4096;
const LABEL_CACHE_MAX=1024;
const SPRITE_PAD=128;
const query=new URLSearchParams(location.search);
const syntheticMode=query.get('synthetic')==='1'||query.get('synthetic')==='true';
const syntheticActive=Math.max(1,Math.min(84,parseInt(query.get('active')||'28',10)||28));
const syntheticHz=Math.max(.1,Math.min(20,parseFloat(query.get('hz')||'2.2')||2.2));
const syntheticChurn=query.get('churn')!=='0';
const syntheticPattern=query.get('pattern')||'wave';
function clamp01(v){return Math.max(0,Math.min(1,v));}
function accent(s){const c=s.accent||{r:73,g:196,b:255};return {r:c.r||0,g:c.g||0,b:c.b||0};}
function rgba(c,a){return 'rgba('+c.r+','+c.g+','+c.b+','+a+')';}
function rgb(c){return 'rgb('+c.r+','+c.g+','+c.b+')';}
function strengthPercent(v,def){return Math.max(0,Math.min(100,v===undefined?def:v));}
function stBloom(stg){return strengthPercent(stg.bloom,50)*0.0408;}
function stEdge(stg){return strengthPercent(stg.edgeSweep,50)*0.0066;}
function stGlass(stg){return strengthPercent(stg.glass,50)/50;}
function stScale(stg){return strengthPercent(stg.microScale,50)*0.0538;}
function stLabel(stg){return strengthPercent(stg.labelContrast,0)*0.0100;}
function stRimLight(stg){return strengthPercent(stg.glassRimLight,50)*3.59/50;}
const labelFonts=[
  '600 {size}px Segoe UI, Arial, sans-serif',
  '600 {size}px Bahnschrift SemiCondensed, Bahnschrift, Segoe UI, sans-serif',
  '800 {size}px Arial Black, Arial, Segoe UI, sans-serif',
  '700 {size}px Impact, Haettenschweiler, Arial Narrow, sans-serif',
  '700 {size}px Trebuchet MS, Segoe UI, Arial, sans-serif',
  '600 {size}px Cascadia Mono, Consolas, monospace',
  '700 {size}px Franklin Gothic Medium, Arial Narrow, Arial, sans-serif',
  '700 {size}px Tahoma, Verdana, Segoe UI, sans-serif',
  '700 {size}px Comic Sans MS, Comic Sans, Segoe UI, cursive',
  '700 {size}px Yu Gothic UI, Meiryo, MS PGothic, Segoe UI, sans-serif',
  '700 {size}px Yu Mincho, MS PMincho, Georgia, serif',
  '700 {size}px MS Gothic, Yu Gothic UI, Consolas, monospace',
  '700 {size}px Papyrus, Gabriola, Comic Sans MS, fantasy'
];
function labelFont(style){
  const size=Math.max(8,Math.min(32,Math.round((style&&style.size)||13)));
  const idx=Math.max(0,Math.min(labelFonts.length-1,Math.round((style&&style.font)||0)));
  return labelFonts[idx].replace('{size}',String(size));
}
function labelColor(style){const c=(style&&style.color)||{r:255,g:255,b:255};return {r:c.r||0,g:c.g||0,b:c.b||0};}
function softGlowGradient(g,c,cx,cy,r0,r1,alpha){
  const gr=g.createRadialGradient(cx,cy,Math.max(0,r0),cx,cy,Math.max(r0+1,r1));
  gr.addColorStop(0,rgba(c,alpha*.34));
  gr.addColorStop(.06,rgba(c,alpha*.33));
  gr.addColorStop(.14,rgba(c,alpha*.29));
  gr.addColorStop(.25,rgba(c,alpha*.22));
  gr.addColorStop(.38,rgba(c,alpha*.145));
  gr.addColorStop(.54,rgba(c,alpha*.080));
  gr.addColorStop(.70,rgba(c,alpha*.038));
  gr.addColorStop(.84,rgba(c,alpha*.014));
  gr.addColorStop(.94,rgba(c,alpha*.003));
  gr.addColorStop(1,rgba(c,0));
  return gr;
}
function resize(){
  const ndpr=Math.max(1,Math.min(2,window.devicePixelRatio||1));
  const nw=Math.max(1,innerWidth),nh=Math.max(1,innerHeight);
  if(ndpr===dpr&&nw===cw&&nh===ch)return false;
  dpr=ndpr;cw=nw;ch=nh;canvas.width=Math.round(cw*dpr);canvas.height=Math.round(ch*dpr);canvas.style.width=cw+'px';canvas.style.height=ch+'px';
  ctx.setTransform(dpr,0,0,dpr,0,0);
  layoutKey='';
  return true;
}
function recalcLayout(keys){
  let maxX=1,maxY=1;
  for(const k of keys){maxX=Math.max(maxX,k.x+k.w);maxY=Math.max(maxY,k.row*46+(k.h||40));}
  boardW=maxX;boardH=maxY;scale=Math.max(.1,Math.min(cw/maxX,ch/maxY)*.96);ox=(cw-boardW*scale)*.5;oy=(ch-boardH*scale)*.5;
}
function syntheticApply(s,now){
  if(!s||!s.keys)return;
  const keys=s.keys,n=keys.length||1,t=now*.001*syntheticHz;
  const center=(t*n*.29)%n,half=syntheticActive*.5;
  for(let i=0;i<n;i++){
    const ring=Math.abs(((i-center+n*.5)%n)-n*.5);
    let v=0;
    if(syntheticPattern==='burst'){
      v=ring<half?1:0;
    }else{
      const local=clamp01(1-ring/Math.max(1,half));
      const wave=.5+.5*Math.sin(t*6.283+i*.31);
      v=local*(syntheticChurn?(.30+.70*wave):1);
    }
    const milli=Math.round(clamp01(v)*1000);
    keys[i].raw=milli;keys[i].out=milli;
  }
  s.tick=Math.round(now);
}
function rr(x,y,w,h,r){
  r=Math.max(0,Math.min(r,w*.5,h*.5));ctx.beginPath();ctx.moveTo(x+r,y);ctx.arcTo(x+w,y,x+w,y+h,r);ctx.arcTo(x+w,y+h,x,y+h,r);ctx.arcTo(x,y+h,x,y,r);ctx.arcTo(x,y,x+w,y,r);ctx.closePath();
}
function rr2(g,x,y,w,h,r){
  r=Math.max(0,Math.min(r,w*.5,h*.5));g.beginPath();g.moveTo(x+r,y);g.arcTo(x+w,y,x+w,y+h,r);g.arcTo(x+w,y+h,x,y+h,r);g.arcTo(x,y+h,x,y,r);g.arcTo(x,y,x+w,y,r);g.closePath();
}
function rrSub(g,x,y,w,h,r){
  r=Math.max(0,Math.min(r,w*.5,h*.5));g.moveTo(x+r,y);g.arcTo(x+w,y,x+w,y+h,r);g.arcTo(x+w,y+h,x,y+h,r);g.arcTo(x,y+h,x,y,r);g.arcTo(x,y,x+w,y,r);g.closePath();
}
function drawGlassMaterial(g,w,h,r,c,vq,glassStrength,eased){
  const amount=Math.max(0,Math.min(2,glassStrength));
  if(amount<=0.001)return;
  const base=Math.min(1,amount);
  const gs=Math.max(0,amount-1);
  g.save();
  rr2(g,0,0,w,h,r);
  g.clip();

  g.globalCompositeOperation='screen';
  let top=g.createLinearGradient(0,0,0,h*.62);
  top.addColorStop(0,'rgba(255,255,255,'+((0.085+0.105*gs)*base)+')');
  top.addColorStop(.22,'rgba(255,255,255,'+((0.028+0.050*gs)*base)+')');
  top.addColorStop(.58,'rgba(255,255,255,0)');
  top.addColorStop(1,'rgba(255,255,255,0)');
  g.fillStyle=top;
  g.fillRect(1,1,w-2,h*.62);

  let lens=g.createLinearGradient(0,h*.10,w,h*.28);
  lens.addColorStop(0,'rgba(255,255,255,0)');
  lens.addColorStop(.18,'rgba(255,255,255,'+((0.028+0.060*gs)*base)+')');
  lens.addColorStop(.52,'rgba(255,255,255,'+((0.040+0.085*gs)*base)+')');
  lens.addColorStop(.84,'rgba(255,255,255,'+((0.016+0.040*gs)*base)+')');
  lens.addColorStop(1,'rgba(255,255,255,0)');
  g.fillStyle=lens;
  g.fillRect(2,2,w-4,h*.30);

  let diagonal=g.createLinearGradient(-w*.25,h*.12,w*1.10,h*.82);
  diagonal.addColorStop(0,'rgba(255,255,255,0)');
  diagonal.addColorStop(.42,'rgba(255,255,255,'+((0.010+0.045*gs)*base)+')');
  diagonal.addColorStop(.50,'rgba(255,255,255,'+((0.030+0.090*gs)*base)+')');
  diagonal.addColorStop(.58,'rgba(255,255,255,'+((0.008+0.030*gs)*base)+')');
  diagonal.addColorStop(1,'rgba(255,255,255,0)');
  g.fillStyle=diagonal;
  g.fillRect(0,0,w,h);

  if(vq>0){
    const ca=vq*(0.10+0.28*gs)*base;
    let caustic=g.createRadialGradient(w*.52,h*(.78-.30*vq),h*.04,w*.52,h*(.78-.30*vq),h*(.34+.12*gs));
    caustic.addColorStop(0,rgba(c,Math.min(.42,ca)));
    caustic.addColorStop(.32,'rgba(255,255,255,'+Math.min(.18,ca*.42)+')');
    caustic.addColorStop(1,'rgba(255,255,255,0)');
    g.fillStyle=caustic;
    g.beginPath();
    g.ellipse(w*.52,h*(.78-.30*vq),w*.35,h*.16,0.08,0,Math.PI*2);
    g.fill();
  }

  g.restore();

  g.save();
  g.globalCompositeOperation='lighter';
  g.lineCap='round';
  g.shadowColor='rgba(255,255,255,'+((0.10+0.22*gs)*base)+')';
  g.shadowBlur=4+9*gs;
  g.lineWidth=1.05+1.25*gs;
  let edge=g.createLinearGradient(0,0,w,h);
  edge.addColorStop(0,'rgba(255,255,255,'+((0.13+0.25*gs)*base)+')');
  edge.addColorStop(.32,'rgba(170,255,235,'+((0.035+0.105*gs)*base)+')');
  edge.addColorStop(.67,'rgba(145,175,255,'+((0.025+0.080*gs)*base)+')');
  edge.addColorStop(1,'rgba(255,255,255,'+((0.035+0.090*gs)*base)+')');
  g.strokeStyle=edge;
  rr2(g,1.2,1.2,w-2.4,h-2.4,Math.max(1,r-1));
  g.stroke();

  g.shadowBlur=0;
  g.lineWidth=.8+1.0*gs;
  g.strokeStyle='rgba(255,255,255,'+((0.075+0.180*gs)*base)+')';
  const topInset=Math.min(10,Math.max(4,w*.035));
  g.beginPath();
  g.moveTo(topInset,2.2);
  g.bezierCurveTo(w*.28,1.0,w*.72,1.0,w-topInset,2.2);
  g.stroke();

  if(vq>0){
    g.strokeStyle=rgba(c,Math.min(.46,(0.09+0.25*gs)*eased*base));
    g.lineWidth=.9+1.6*gs*eased;
    g.beginPath();
    g.moveTo(w*.18,h-2.4);
    g.bezierCurveTo(w*.35,h-1.0,w*.65,h-1.0,w*.82,h-2.4);
    g.stroke();
  }
  g.restore();
}
function getSprite(w,h,v,s,c,fx,stg){
  const rv=Math.round(clamp01(v)*96);
  const bloomStrength=stBloom(stg);
  const edgeStrength=stEdge(stg);
  const glassStrength=stGlass(stg);
  const key=[w,h,rv,s.fillDirection,fx.glass?1:0,fx.bloom?1:0,fx.edgeSweep?1:0,c.r,c.g,c.b,bloomStrength.toFixed(2),edgeStrength.toFixed(2),glassStrength.toFixed(2)].join('|');
  const hit=spriteCache.get(key);
  if(hit){hit.used=++spriteSerial;perfSpriteHits++;return hit.canvas;}
  perfSpriteMisses++;
  const buildStart=performance.now();
  const vq=rv/96,pad=SPRITE_PAD,r=7,sw=Math.ceil(w+pad*2),sh=Math.ceil(h+pad*2);
  const cn=document.createElement('canvas');cn.width=sw;cn.height=sh;
  const g=cn.getContext('2d',{alpha:true});g.translate(pad,pad);
  const eased=vq*vq*(3-2*vq);
  const bloomPower=fx.bloom?clamp01(eased*bloomStrength):0;
  const edgePower=fx.edgeSweep?clamp01(eased*edgeStrength):0;
  g.save();g.shadowColor='rgba(0,0,0,.30)';g.shadowBlur=18;g.shadowOffsetY=6;rr2(g,0,0,w,h,r);
  if(fx.glass&&glassStrength>0.001){
    const base=g.createLinearGradient(0,0,0,h);
    base.addColorStop(0,'rgba(32,39,49,.78)');
    base.addColorStop(.42,'rgba(18,24,32,.76)');
    base.addColorStop(1,'rgba(10,14,20,.80)');
    g.fillStyle=base;
  }else{
    g.fillStyle='rgba(16,20,28,.70)';
  }
  g.fill();g.restore();
  rr2(g,0,0,w,h,r);g.save();g.clip();
  const fh=h*vq,fy=s.fillDirection==='top-down'?0:h-fh;g.fillStyle=rgb(c);g.shadowColor=rgba(c,0.12+0.88*bloomPower);g.shadowBlur=fx.bloom?(4+Math.min(96,bloomStrength*3.2)*bloomPower):0;g.fillRect(0,fy,w,fh);
  if(fx.edgeSweep&&vq>0){g.shadowBlur=8+42*edgePower;g.fillStyle='rgba(255,255,255,'+(0.58+0.42*edgePower)+')';g.fillRect(0,s.fillDirection==='top-down'?fy+fh-3:fy,w,3);}
  if(fx.edgeSweep&&vq>0){const sheen=g.createLinearGradient(0,0,w,0);sheen.addColorStop(0,'rgba(255,255,255,0)');sheen.addColorStop(.42,rgba(c,0.10+0.28*edgePower));sheen.addColorStop(.58,'rgba(255,255,255,'+(0.08+0.34*edgePower)+')');sheen.addColorStop(1,'rgba(255,255,255,0)');g.shadowBlur=0;g.fillStyle=sheen;g.fillRect(2,2,w-4,Math.max(2,h*.14));}
  g.restore();
  if(fx.glass)drawGlassMaterial(g,w,h,r,c,vq,glassStrength,eased);
  rr2(g,.5,.5,w-1,h-1,r);g.lineWidth=1+2.2*edgePower;g.strokeStyle=vq>0&&fx.edgeSweep?rgba(c,0.30+0.70*edgePower):'rgba(255,255,255,.22)';g.shadowColor=rgba(c,0.10+0.78*edgePower);g.shadowBlur=vq>0&&fx.edgeSweep?10+70*edgePower:0;g.stroke();
  if(vq>0&&fx.edgeSweep){rr2(g,2,2,w-4,h-4,Math.max(1,r-2));g.shadowBlur=0;g.lineWidth=.8+4.8*edgePower;g.strokeStyle=rgba(c,0.10+0.70*edgePower);g.stroke();}
  spriteCache.set(key,{canvas:cn,used:++spriteSerial});
  if(spriteCache.size>SPRITE_CACHE_MAX){let oldestK=null,oldest=Infinity;for(const [k,e] of spriteCache){if(e.used<oldest){oldest=e.used;oldestK=k;}}if(oldestK!==null)spriteCache.delete(oldestK);}
  perfSpriteBuild+=(performance.now()-buildStart)*1000;
  return cn;
}
)HTML" R"HTML(
function getLabelSprite(w,h,label,labelPower,enabled,style){
  const lp=enabled?Math.round(clamp01(labelPower)*96):0;
  const lc=labelColor(style),font=labelFont(style),shadow=Math.max(0,Math.min(100,(style&&style.shadow)===undefined?50:style.shadow));
  const key=[w,h,label,lp,enabled?1:0,font,shadow,lc.r,lc.g,lc.b].join('|');
  const hit=labelCache.get(key);
  if(hit){hit.used=++spriteSerial;return hit.canvas;}
  perfLabelMisses++;
  const buildStart=performance.now();
  const pad=10+Math.ceil(18*(enabled?Math.min(1,labelPower):0))+Math.ceil(shadow*.12);
  const cn=document.createElement('canvas');cn.width=Math.ceil(w+pad*2);cn.height=Math.ceil(h+pad*2);
  const g=cn.getContext('2d',{alpha:true});
  g.font=font;g.textAlign='center';g.textBaseline='middle';g.fillStyle=rgb(lc);
  const shadow01=shadow/100;
  if(enabled){g.lineWidth=1.2+5.8*(lp/96);g.strokeStyle='rgba(0,0,0,'+(0.38+0.42*shadow01)+')';g.strokeText(label||'',pad+w*.5,pad+h*.5,Math.max(4,w-8));}
  g.shadowColor='rgba(0,0,0,'+(0.48+0.47*shadow01)+')';g.shadowBlur=(1+15*shadow01)+(enabled?18*(lp/96)*shadow01:0);g.shadowOffsetY=.5+1.8*shadow01;
  g.fillText(label||'',pad+w*.5,pad+h*.5,Math.max(4,w-8));
  labelCache.set(key,{canvas:cn,used:++spriteSerial});
  if(labelCache.size>LABEL_CACHE_MAX){let oldestK=null,oldest=Infinity;for(const [k,e] of labelCache){if(e.used<oldest){oldest=e.used;oldestK=k;}}if(oldestK!==null)labelCache.delete(oldestK);}
  perfLabelBuild+=(performance.now()-buildStart)*1000;
  return cn;
}
function drawGlassNeighborLight(k,c,stg,live){
  const rimStrength=stRimLight(stg);
  if(rimStrength<=.001)return;
  const x=k.x,y=k.row*46,w=k.w,h=k.h||40,r=7;
  const hits=[];
  for(const item of live){
    const o=item[0],ov=item[1];
    if(o===k||ov<=.003)continue;
    const sx=o.x,sh=o.h||40,sy=o.row*46,sw=o.w;
    const overlapX=Math.max(0,Math.min(x+w,sx+sw)-Math.max(x,sx));
    const overlapY=Math.max(0,Math.min(y+h,sy+sh)-Math.max(y,sy));
    const gapX=Math.max(0,Math.max(x,sx)-Math.min(x+w,sx+sw));
    const gapY=Math.max(0,Math.max(y,sy)-Math.min(y+h,sy+sh));
    const gap=Math.hypot(gapX,gapY);
    const reach=12+Math.min(30,Math.max(w,h,o.w,sh)*.24);
    if(gap>reach)continue;
    const e=ov*ov*(3-2*ov),fall=Math.pow(1-gap/reach,1.55);
    let px=x+w*.5,py=y+h*.5,tx=1,ty=0,along=18;
    if(sy>=y+h&&overlapX>0){
      px=(Math.max(x,sx)+Math.min(x+w,sx+sw))*.5;py=y+h-.9;tx=1;ty=0;along=Math.max(14,overlapX*.92);
    }else if(sy+sh<=y&&overlapX>0){
      px=(Math.max(x,sx)+Math.min(x+w,sx+sw))*.5;py=y+.9;tx=1;ty=0;along=Math.max(14,overlapX*.92);
    }else if(sx>=x+w&&overlapY>0){
      px=x+w-.9;py=(Math.max(y,sy)+Math.min(y+h,sy+sh))*.5;tx=0;ty=1;along=Math.max(14,overlapY*.92);
    }else if(sx+sw<=x&&overlapY>0){
      px=x+.9;py=(Math.max(y,sy)+Math.min(y+h,sy+sh))*.5;tx=0;ty=1;along=Math.max(14,overlapY*.92);
    }else{
      const scx=sx+sw*.5,scy=sy+sh*.5;
      const vx=scx-(x+w*.5),vy=scy-(y+h*.5),dist=Math.hypot(vx,vy)||1;
      const dx=vx/dist,dy=vy/dist;
      const bx=dx!==0?(w*.5-.9)/Math.abs(dx):1e9;
      const by=dy!==0?(h*.5-.9)/Math.abs(dy):1e9;
      const t=Math.min(bx,by);
      px=x+w*.5+dx*t;py=y+h*.5+dy*t;
      tx=-dy;ty=dx;along=Math.max(12,Math.min(24,Math.max(w,h)*.34));
    }
    hits.push({px,py,tx,ty,along,p:e*fall,gap});
  }
  if(!hits.length)return;
  hits.sort((a,b)=>b.p-a.p);

  ctx.save();
  ctx.beginPath();
  rrSub(ctx,x+.7,y+.7,w-1.4,h-1.4,r);
  rrSub(ctx,x+3.6,y+3.6,w-7.2,h-7.2,Math.max(1,r-2.6));
  ctx.clip('evenodd');
  ctx.globalCompositeOperation='lighter';
  for(const hit of hits.slice(0,3)){
    const raw=hit.p*Math.max(0,rimStrength);
    if(raw<=.005)continue;
    const shape=1-Math.exp(-raw*.78);
    const boost=Math.min(12,Math.pow(Math.max(0,raw),.72));
    const alphaMul=1+boost*.34;
    const px=hit.px,py=hit.py,tangentX=hit.tx,tangentY=hit.ty;
    const angle=Math.atan2(tangentY,tangentX);
    const band=2.2+3.4*shape+Math.min(5.5,boost*.34);
    const along=Math.min(Math.max(w,h)*.92,hit.along*(0.78+0.45*shape+Math.min(.42,boost*.035)));
    const lx=px,ly=py;

    ctx.save();
    ctx.translate(lx,ly);
    ctx.rotate(angle);
    ctx.scale(along/band,1);
    const rg=ctx.createRadialGradient(0,0,0,0,0,band);
    rg.addColorStop(0,'rgba(255,255,255,'+Math.min(.92,(0.035+0.130*shape)*alphaMul)+')');
    rg.addColorStop(.28,rgba(c,Math.min(.95,(0.030+0.125*shape)*alphaMul)));
    rg.addColorStop(.62,'rgba(255,255,255,'+Math.min(.45,(0.006+0.032*shape)*alphaMul)+')');
    rg.addColorStop(1,'rgba(255,255,255,0)');
    ctx.fillStyle=rg;
    ctx.beginPath();
    ctx.arc(0,0,band,0,Math.PI*2);
    ctx.fill();
    ctx.restore();
  }
  ctx.restore();
}
function drawKey(k,v,s,c,fx,stg,live){
  const x=k.x,y=k.row*46,w=k.w,h=k.h||40;
  const scaleStrength=stScale(stg);
  const labelStrength=stLabel(stg);
  const eased=v*v*(3-2*v);
  const scalePower=clamp01(eased*scaleStrength);
  const labelPower=clamp01(eased*labelStrength);
  ctx.save();
  if(fx.microScale&&v>0){const ms=1+0.024*scalePower;ctx.translate(x+w*.5,y+h*.5);ctx.scale(ms,ms);ctx.translate(-x-w*.5,-y-h*.5);}
  const sp=getSprite(w,h,v,s,c,fx,stg);ctx.drawImage(sp,x-SPRITE_PAD,y-SPRITE_PAD);
  if(fx.glassRimLight)drawGlassNeighborLight(k,c,stg,live);
  const lbl=getLabelSprite(w,h,k.label||'',labelPower,!!fx.labelContrast,s.labelStyle||{});ctx.drawImage(lbl,x-(lbl.width-w)*.5,y-(lbl.height-h)*.5);
  ctx.restore();
}
function drawBloomAura(k,v,c,stg){
  if(v<=0)return;
  const bloomStrength=stBloom(stg);
  if(bloomStrength<=0)return;
  const eased=v*v*(3-2*v);
  const power=clamp01(eased*bloomStrength);
  if(power<=0)return;
  const x=k.x,y=k.row*46,w=k.w,h=k.h||40;
  const tone=Math.min(1,bloomStrength/2.04);
  const radius=h*(1.52+Math.min(2.70,bloomStrength*.55))*(0.84+0.16*power);
  const count=Math.max(1,Math.min(9,Math.ceil(w/(h*.72))));
  const alpha=(0.15+0.38*tone)*power/Math.sqrt(count);
  ctx.save();
  ctx.globalCompositeOperation='lighter';
  for(let i=0;i<count;i++){
    const cx=x+(count===1?w*.5:w*((i+.5)/count));
    const cy=y+h*.55;
    const g=softGlowGradient(ctx,c,cx,cy,0,radius,Math.min(.74,alpha*1.20));
    ctx.fillStyle=g;
    ctx.beginPath();
    ctx.arc(cx,cy,radius,0,Math.PI*2);
    ctx.fill();
  }
  {
    const cx=x+w*.5,cy=y+h*.56;
    const rx=w*.55+radius*.34,ry=h*.30+radius*.30;
    const g=softGlowGradient(ctx,c,cx,cy,0,Math.max(rx,ry),Math.min(.40,alpha*.78));
    ctx.fillStyle=g;
    ctx.save();
    ctx.translate(cx,cy);
    ctx.scale(rx/Math.max(rx,ry),ry/Math.max(rx,ry));
    ctx.beginPath();
    ctx.arc(0,0,Math.max(rx,ry),0,Math.PI*2);
    ctx.fill();
    ctx.restore();
  }
  if(w>h*1.35){
    const cy=y+h*.55,ry=h*.36+radius*.18;
    const steps=Math.max(3,Math.min(11,Math.ceil(w/(h*.55))));
    for(let i=0;i<steps;i++){
      const cx=x+w*((i+.5)/steps);
      const g=softGlowGradient(ctx,c,cx,cy,0,ry*1.75,Math.min(.18,alpha*.30));
      ctx.fillStyle=g;
      ctx.beginPath();
      ctx.ellipse(cx,cy,ry*1.55,ry,0,0,Math.PI*2);
      ctx.fill();
    }
  }
  ctx.restore();
}
function render(){
  const start=performance.now();resize();
  const dt=Math.max(1,Math.min(100,start-lastRenderTime));lastRenderTime=start;
  ctx.clearRect(0,0,cw,ch);
  if(syntheticMode&&latest)syntheticApply(latest,start);
  const s=latest;
  if(s&&s.keys){
    const lk=s.keys.map(k=>k.hid+','+k.row+','+k.x+','+k.w+','+(k.h||40)).join('|');
    let layoutMs=0;if(lk!==layoutKey){const ls=performance.now();layoutKey=lk;recalcLayout(s.keys);layoutMs=performance.now()-ls;perfLayout+=layoutMs*1000;}
    const fx=s.effects||{},stg=s.strengths||{},settings=s.settings||{},c=accent(s);
    const baseResponse=1-(clamp01((stg.smoothing===undefined?35:stg.smoothing)/100)*.82);
    const response=fx.smoothing?1-Math.pow(1-baseResponse,dt/16.6667):1;
    ctx.save();ctx.translate(ox,oy);ctx.scale(scale,scale);
    const live=[],lit=[];
    for(const k of s.keys){
      const id=k.hid+':'+k.row+':'+k.x;const target=clamp01(((settings.useRawDepth?k.raw:k.out)||0)/1000);
      const last=displayed.has(id)?displayed.get(id):target;const v=fx.smoothing?last+(target-last)*response:target;displayed.set(id,Math.abs(v)<0.001?0:v);live.push([k,v]);if(v>.002)lit.push([k,v]);
    }
    if(fx.bloom){for(const item of lit)drawBloomAura(item[0],item[1],c,stg);}
    for(const item of live)drawKey(item[0],item[1],s,c,fx,stg,lit);
    ctx.restore();
  }
  perfFrames++;perfRender+=(performance.now()-start)*1000;const now=performance.now();
  if(now-lastPerfReport>5000){fetch('/client_perf?frames='+perfFrames+'&fetches='+perfFetches+'&fetch_us='+Math.round(perfFetch)+'&render_us='+Math.round(perfRender)+'&layout_us='+Math.round(perfLayout)+'&sprite_hits='+perfSpriteHits+'&sprite_misses='+perfSpriteMisses+'&label_misses='+perfLabelMisses+'&sprite_build_us='+Math.round(perfSpriteBuild)+'&label_build_us='+Math.round(perfLabelBuild),{cache:'no-store'}).catch(()=>{});perfFrames=0;perfFetches=0;perfFetch=0;perfRender=0;perfLayout=0;perfSpriteHits=0;perfSpriteMisses=0;perfLabelMisses=0;perfSpriteBuild=0;perfLabelBuild=0;lastPerfReport=now;}
  requestAnimationFrame(render);
}
async function poll(){
  if(polling)return;polling=true;
  try{
    if(syntheticMode&&latest){polling=false;setTimeout(poll,1000);return;}
    const fetchStart=performance.now();
    latest=await fetch('/state',{cache:'no-store'}).then(r=>r.json());
    if(syntheticMode&&latest&&latest.settings)latest.settings.refreshMs=1;
    lastRefreshMs=Math.max(1,Math.min(250,((latest.settings||{}).refreshMs||1)));
    perfFetch+=(performance.now()-fetchStart)*1000;perfFetches++;
  }catch(e){}
  polling=false;setTimeout(poll,Math.max(1,Math.min(250,lastRefreshMs||1)));
}
addEventListener('resize',()=>{resize();layoutKey='';});
resize();poll();requestAnimationFrame(render);
</script>
</body>
</html>)HTML";
}

static bool OverlaySendAll(SOCKET s, const char* data, int size)
{
    int sent = 0;
    while (sent < size)
    {
        int n = send(s, data + sent, size - sent, 0);
        if (n <= 0)
            return false;
        sent += n;
    }
    return true;
}

static bool OverlaySend(SOCKET s, const std::string& status, const std::string& contentType, const std::string& body, bool keepAlive)
{
    uint64_t beginUs = OverlayNowUs();
    std::string header;
    header.reserve(256);
    header += "HTTP/1.1 ";
    header += status;
    header += "\r\nContent-Type: ";
    header += contentType;
    header += "\r\nContent-Length: ";
    AppendUInt64(header, (unsigned long long)body.size());
    header += "\r\nCache-Control: no-store\r\nAccess-Control-Allow-Origin: *\r\nConnection: ";
    header += keepAlive ? "keep-alive\r\nKeep-Alive: timeout=5, max=256\r\n\r\n" : "close\r\n\r\n";

    bool ok = OverlaySendAll(s, header.data(), (int)header.size());
    if (ok && !body.empty())
        ok = OverlaySendAll(s, body.data(), (int)body.size());

    uint64_t durUs = OverlayNowUs() - beginUs;
    g_perfSendUsTotal.fetch_add(durUs, std::memory_order_relaxed);
    OverlayAtomicMax(g_perfSendUsMax, durUs);
    return ok;
}

static unsigned OverlayQueryUInt(const std::string& query, const char* key)
{
    std::string needle = std::string(key) + "=";
    size_t p = query.find(needle);
    if (p == std::string::npos)
        return 0;
    p += needle.size();
    unsigned value = 0;
    while (p < query.size() && query[p] >= '0' && query[p] <= '9')
    {
        value = value * 10u + (unsigned)(query[p] - '0');
        ++p;
    }
    return value;
}

static void OverlayRecordClientPerf(const std::string& query)
{
    unsigned frames = OverlayQueryUInt(query, "frames");
    if (frames == 0)
        return;
    g_perfClientReports.fetch_add(1, std::memory_order_relaxed);
    g_perfClientFrames.fetch_add(frames, std::memory_order_relaxed);
    g_perfClientFetches.fetch_add(OverlayQueryUInt(query, "fetches"), std::memory_order_relaxed);
    g_perfClientFetchUs.fetch_add(OverlayQueryUInt(query, "fetch_us"), std::memory_order_relaxed);
    g_perfClientRenderUs.fetch_add(OverlayQueryUInt(query, "render_us"), std::memory_order_relaxed);
    g_perfClientLayoutUs.fetch_add(OverlayQueryUInt(query, "layout_us"), std::memory_order_relaxed);
    g_perfClientSpriteHits.fetch_add(OverlayQueryUInt(query, "sprite_hits"), std::memory_order_relaxed);
    g_perfClientSpriteMisses.fetch_add(OverlayQueryUInt(query, "sprite_misses"), std::memory_order_relaxed);
    g_perfClientLabelMisses.fetch_add(OverlayQueryUInt(query, "label_misses"), std::memory_order_relaxed);
    g_perfClientSpriteBuildUs.fetch_add(OverlayQueryUInt(query, "sprite_build_us"), std::memory_order_relaxed);
    g_perfClientLabelBuildUs.fetch_add(OverlayQueryUInt(query, "label_build_us"), std::memory_order_relaxed);
}

static bool OverlayHandleClientRequest(SOCKET client, const std::string& r, bool keepAlive)
{
    std::string path = "/";
    size_t sp1 = r.find(' ');
    if (sp1 != std::string::npos)
    {
        size_t sp2 = r.find(' ', sp1 + 1);
        if (sp2 != std::string::npos)
            path = r.substr(sp1 + 1, sp2 - sp1 - 1);
    }

    std::string query;
    size_t q = path.find('?');
    if (q != std::string::npos)
    {
        query = path.substr(q + 1);
        path.resize(q);
    }

    g_perfHttpRequests.fetch_add(1, std::memory_order_relaxed);
    if (path == "/" || path == "/index.html")
        return OverlaySend(client, "200 OK", "text/html; charset=utf-8", OverlayHtml(), keepAlive);
    else if (path == "/state")
        return OverlaySend(client, "200 OK", "application/json; charset=utf-8", OverlayBuildStateJson(), keepAlive);
    else if (path == "/client_perf")
    {
        OverlayRecordClientPerf(query);
        return OverlaySend(client, "204 No Content", "text/plain; charset=utf-8", "", keepAlive);
    }
    else
        return OverlaySend(client, "404 Not Found", "text/plain; charset=utf-8", "not found", false);
}

static void OverlayHandleClient(SOCKET client)
{
    g_perfHttpConnections.fetch_add(1, std::memory_order_relaxed);
    DWORD timeoutMs = 5000;
    setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeoutMs, sizeof(timeoutMs));

    for (int requestCount = 0; requestCount < 256 && g_overlayRunning.load(std::memory_order_acquire); ++requestCount)
    {
        char req[2048]{};
        int got = recv(client, req, (int)sizeof(req) - 1, 0);
        if (got <= 0)
            break;

        std::string r(req, req + got);
        bool closeRequested = r.find("Connection: close") != std::string::npos ||
            r.find("connection: close") != std::string::npos;
        bool ok = OverlayHandleClientRequest(client, r, !closeRequested);
        OverlayPerfMaybeLog();
        if (!ok || closeRequested)
            break;
    }
}

static DWORD OverlayThreadBody(SOCKET listenSocket)
{
    while (g_overlayRunning.load(std::memory_order_acquire))
    {
        SOCKET client = accept(listenSocket, nullptr, nullptr);
        if (client == INVALID_SOCKET)
        {
            if (!g_overlayRunning.load(std::memory_order_acquire))
                break;
            Sleep(10);
            continue;
        }

        {
            std::lock_guard<std::mutex> lock(g_overlaySocketMutex);
            if (!g_overlayRunning.load(std::memory_order_acquire))
            {
                closesocket(client);
                break;
            }
            g_overlayClientSocket = client;
        }

        OverlayHandleClient(client);

        {
            std::lock_guard<std::mutex> lock(g_overlaySocketMutex);
            if (g_overlayClientSocket == client)
                g_overlayClientSocket = INVALID_SOCKET;
            closesocket(client);
        }
    }
    return 0;
}

static void OverlayThreadOnFault(
    const halljoy::worker::WorkerExceptionRecord& record) noexcept
{
    g_overlayFaultRecord = record;
    g_overlayFaultKind.store(record.kind, std::memory_order_release);
    g_overlayRunning.store(false, std::memory_order_release);
    g_overlayPort.store(0, std::memory_order_release);
    StabilityTrace_WriteCritical(L"ERROR", L"overlay", L"worker.fault",
        L"kind=%u sockets_closed=1", static_cast<unsigned>(record.kind));

    // The worker will not reach its normal per-client close path after stack
    // unwinding. Close only sockets owned by this worker; Stop() still owns WSA
    // teardown and the thread HANDLE.
    try
    {
        std::lock_guard<std::mutex> lock(g_overlaySocketMutex);
        if (g_overlayClientSocket != INVALID_SOCKET)
        {
            shutdown(g_overlayClientSocket, SD_BOTH);
            closesocket(g_overlayClientSocket);
            g_overlayClientSocket = INVALID_SOCKET;
        }
        if (g_overlayListenSocket != INVALID_SOCKET)
        {
            shutdown(g_overlayListenSocket, SD_BOTH);
            closesocket(g_overlayListenSocket);
            g_overlayListenSocket = INVALID_SOCKET;
        }
    }
    catch (...)
    {
    }

    try
    {
        OverlaySetLastError(L"Overlay worker terminated after a C++ exception");
    }
    catch (...)
    {
    }

    OutputDebugStringA("[HallJoy] overlay worker exception: ");
    OutputDebugStringA(record.message);
    OutputDebugStringA("\r\n");
    try
    {
        DebugLog_Write(L"[overlay] worker exception kind=%u",
            static_cast<unsigned>(record.kind));
    }
    catch (...)
    {
    }
}

static void OverlayThreadOnCompletion(
    const halljoy::worker::WorkerExceptionRecord& record) noexcept
{
    g_overlayThreadExited.store(true, std::memory_order_release);
    StabilityTrace_Write(record.kind == halljoy::worker::WorkerExceptionKind::None ? L"INFO" : L"ERROR",
        L"overlay", L"worker.exit", L"fault_kind=%u", static_cast<unsigned>(record.kind));
}

static DWORD WINAPI OverlayThreadProc(LPVOID param) noexcept
{
    const SOCKET listenSocket = static_cast<SOCKET>(reinterpret_cast<UINT_PTR>(param));
    g_overlayThreadExited.store(false, std::memory_order_release);
    StabilityTrace_Write(L"INFO", L"overlay", L"worker.start", L"port=%u",
        static_cast<unsigned>(g_overlayPort.load(std::memory_order_acquire)));
    return static_cast<DWORD>(halljoy::worker::RunWorkerEntryBarrier(
        [listenSocket] { return OverlayThreadBody(listenSocket); },
        OverlayThreadOnFault,
        OverlayThreadOnCompletion,
        0xE0514F45u));
}

bool OverlayServer_Start(uint16_t port)
{
    if (g_overlayRunning.load(std::memory_order_acquire))
        return true;
    if (g_overlayThread)
    {
        OverlaySetLastError(L"Previous overlay worker has not been joined");
        return false;
    }
    if (port == 0)
        port = 8765;
    OverlayServer_SetConfiguredPort(port);

    WSADATA wsa{};
    int wsaRet = WSAStartup(MAKEWORD(2, 2), &wsa);
    if (wsaRet != 0)
    {
        OverlaySetLastError(L"WSAStartup failed");
        StabilityTrace_WriteCritical(L"ERROR", L"overlay", L"start.failed", L"stage=wsa code=%d", wsaRet);
        return false;
    }
    g_overlayWsaStarted.store(true, std::memory_order_release);

    SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == INVALID_SOCKET)
    {
        OverlaySetLastError(L"socket() failed");
        StabilityTrace_WriteCritical(L"ERROR", L"overlay", L"start.failed", L"stage=socket code=%d", WSAGetLastError());
        if (g_overlayWsaStarted.exchange(false, std::memory_order_acq_rel))
            WSACleanup();
        return false;
    }

    BOOL reuse = TRUE;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (const char*)&reuse, sizeof(reuse));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

    if (bind(s, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR)
    {
        const int bindError = WSAGetLastError();
        closesocket(s);
        if (g_overlayWsaStarted.exchange(false, std::memory_order_acq_rel))
            WSACleanup();
        OverlaySetLastError(L"Port is already in use");
        StabilityTrace_Write(L"WARN", L"overlay", L"start.failed", L"stage=bind port=%u code=%d", (unsigned)port, bindError);
        return false;
    }
    if (listen(s, SOMAXCONN) == SOCKET_ERROR)
    {
        const int listenError = WSAGetLastError();
        closesocket(s);
        if (g_overlayWsaStarted.exchange(false, std::memory_order_acq_rel))
            WSACleanup();
        OverlaySetLastError(L"listen() failed");
        StabilityTrace_WriteCritical(L"ERROR", L"overlay", L"start.failed", L"stage=listen code=%d", listenError);
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(g_overlaySocketMutex);
        g_overlayListenSocket = s;
    }
    g_overlayFaultRecord = {};
    g_overlayFaultKind.store(halljoy::worker::WorkerExceptionKind::None, std::memory_order_release);
    g_overlayThreadExited.store(true, std::memory_order_release);
    g_overlayPort.store(port, std::memory_order_release);
    g_overlayRunning.store(true, std::memory_order_release);
    OverlaySetLastError(L"");

    DWORD tid = 0;
    g_overlayThread = CreateThread(nullptr, 0, OverlayThreadProc, (LPVOID)(UINT_PTR)s, 0, &tid);
    if (!g_overlayThread)
    {
        const DWORD threadError = GetLastError();
        g_overlayRunning.store(false, std::memory_order_release);
        SOCKET failedListen = INVALID_SOCKET;
        {
            std::lock_guard<std::mutex> lock(g_overlaySocketMutex);
            failedListen = g_overlayListenSocket;
            g_overlayListenSocket = INVALID_SOCKET;
        }
        if (failedListen != INVALID_SOCKET)
            closesocket(failedListen);
        g_overlayPort.store(0, std::memory_order_release);
        if (g_overlayWsaStarted.exchange(false, std::memory_order_acq_rel))
            WSACleanup();
        OverlaySetLastError(L"CreateThread failed");
        StabilityTrace_WriteCritical(L"ERROR", L"overlay", L"start.failed", L"stage=create_thread win32=%lu", threadError);
        return false;
    }

    StabilityTrace_Write(L"INFO", L"overlay", L"start.ok", L"port=%u", (unsigned)port);
    DebugLog_Write(L"[overlay] server started port=%u", (unsigned)port);
    return true;
}

void OverlayServer_Stop()
{
    const bool wasRunning = g_overlayRunning.exchange(false, std::memory_order_acq_rel);
    StabilityTrace_Write(L"INFO", L"overlay", L"stop.begin", L"was_running=%d has_thread=%d",
        wasRunning ? 1 : 0, g_overlayThread ? 1 : 0);
    if (!wasRunning && !g_overlayThread &&
        !g_overlayWsaStarted.load(std::memory_order_acquire))
    {
        return;
    }

    SOCKET listenSocket = INVALID_SOCKET;
    {
        std::lock_guard<std::mutex> lock(g_overlaySocketMutex);
        listenSocket = g_overlayListenSocket;
        g_overlayListenSocket = INVALID_SOCKET;
        if (g_overlayClientSocket != INVALID_SOCKET)
            shutdown(g_overlayClientSocket, SD_BOTH);
    }
    if (listenSocket != INVALID_SOCKET)
    {
        shutdown(listenSocket, SD_BOTH);
        closesocket(listenSocket);
    }

    if (g_overlayThread)
    {
        DWORD wr = WaitForSingleObject(g_overlayThread, 3000);
        if (wr != WAIT_OBJECT_0)
        {
            const DWORD waitError = wr == WAIT_FAILED ? GetLastError() : ERROR_TIMEOUT;
            OverlaySetLastError(L"Overlay worker join timed out; forcing shutdown-only termination");
            DebugLog_Write(L"[overlay] stop join timeout wait=%lu err=%lu; forcing termination",
                (unsigned long)wr, (unsigned long)waitError);
            StabilityTrace_WriteCritical(L"ERROR", L"overlay", L"stop.timeout",
                L"wait=%lu win32=%lu", (unsigned long)wr, (unsigned long)waitError);
            TerminateThread(g_overlayThread, 0xE0514F56u);
            WaitForSingleObject(g_overlayThread, 1000);
            StabilityTrace_WriteCritical(L"ERROR", L"overlay", L"forced_termination", L"exit_code=0xE0514F56");
        StabilityTrace_WriteCritical(L"ERROR", L"overlay", L"worker.exit", L"fault_kind=forced");
        }
        CloseHandle(g_overlayThread);
        g_overlayThread = nullptr;
    }
    g_overlayPort.store(0, std::memory_order_release);
    if (g_overlayWsaStarted.exchange(false, std::memory_order_acq_rel))
        WSACleanup();
    StabilityTrace_Write(L"INFO", L"overlay", L"stop.end");
    DebugLog_Write(L"[overlay] server stopped");
}

bool OverlayServer_IsRunning()
{
    return g_overlayRunning.load(std::memory_order_acquire);
}

uint16_t OverlayServer_GetPort()
{
    return g_overlayPort.load(std::memory_order_acquire);
}

uint16_t OverlayServer_GetConfiguredPort()
{
    uint16_t port = g_overlayConfiguredPort.load(std::memory_order_acquire);
    return port == 0 ? 8765 : port;
}

void OverlayServer_SetConfiguredPort(uint16_t port)
{
    if (port == 0)
        port = 8765;
    g_overlayConfiguredPort.store(port, std::memory_order_release);
}

std::wstring OverlayServer_GetUrl()
{
    uint16_t port = OverlayServer_IsRunning() ? OverlayServer_GetPort() : OverlayServer_GetConfiguredPort();
    wchar_t buf[96]{};
    swprintf_s(buf, L"http://127.0.0.1:%u/", (unsigned)port);
    return buf;
}

std::wstring OverlayServer_GetLastError()
{
    std::lock_guard<std::mutex> lock(g_overlayStateMutex);
    return g_overlayLastError;
}

OverlayFillDirection OverlayServer_GetFillDirection()
{
    int value = g_overlayFillDirection.load();
    return value == (int)OverlayFillDirection::TopDown
        ? OverlayFillDirection::TopDown
        : OverlayFillDirection::BottomUp;
}

void OverlayServer_SetFillDirection(OverlayFillDirection direction)
{
    g_overlayFillDirection.store(direction == OverlayFillDirection::TopDown
        ? (int)OverlayFillDirection::TopDown
        : (int)OverlayFillDirection::BottomUp);
}

uint32_t OverlayServer_GetEffectFlags()
{
    return g_overlayEffectFlags.load(std::memory_order_acquire);
}

void OverlayServer_SetEffectFlags(uint32_t flags)
{
    uint32_t allowed =
        OverlayEffect_Smoothing |
        OverlayEffect_Glass |
        OverlayEffect_Bloom |
        OverlayEffect_EdgeSweep |
        OverlayEffect_MicroScale |
        OverlayEffect_LabelContrast |
        OverlayEffect_GlassRimLight;
    g_overlayEffectFlags.store(flags & allowed, std::memory_order_release);
}

bool OverlayServer_GetEffectEnabled(uint32_t flag)
{
    return (OverlayServer_GetEffectFlags() & flag) != 0;
}

void OverlayServer_SetEffectEnabled(uint32_t flag, bool enabled)
{
    uint32_t oldFlags = g_overlayEffectFlags.load(std::memory_order_acquire);
    for (;;)
    {
        uint32_t newFlags = enabled ? (oldFlags | flag) : (oldFlags & ~flag);
        if (g_overlayEffectFlags.compare_exchange_weak(oldFlags, newFlags, std::memory_order_acq_rel))
            break;
    }
}

uint32_t OverlayServer_GetAccentColor()
{
    return g_overlayAccentColor.load(std::memory_order_acquire) & 0x00ffffffu;
}

void OverlayServer_SetAccentColor(uint32_t rgb)
{
    g_overlayAccentColor.store(rgb & 0x00ffffffu, std::memory_order_release);
}

static std::atomic<int>* OverlayStrengthAtomic(uint32_t flag)
{
    switch (flag)
    {
    case OverlayEffect_Smoothing: return &g_overlaySmoothingStrengthPercent;
    case OverlayEffect_Glass: return &g_overlayGlassStrengthPercent;
    case OverlayEffect_Bloom: return &g_overlayBloomStrengthPercent;
    case OverlayEffect_EdgeSweep: return &g_overlayEdgeStrengthPercent;
    case OverlayEffect_MicroScale: return &g_overlayScaleStrengthPercent;
    case OverlayEffect_LabelContrast: return &g_overlayLabelStrengthPercent;
    case OverlayEffect_GlassRimLight: return &g_overlayGlassRimStrengthPercent;
    default: return nullptr;
    }
}

int OverlayServer_GetEffectStrengthPercent(uint32_t flag)
{
    std::atomic<int>* value = OverlayStrengthAtomic(flag);
    if (!value) return 100;
    return std::clamp(value->load(std::memory_order_acquire), 0, 100);
}

void OverlayServer_SetEffectStrengthPercent(uint32_t flag, int percent)
{
    std::atomic<int>* value = OverlayStrengthAtomic(flag);
    if (!value) return;
    value->store(std::clamp(percent, 0, 100), std::memory_order_release);
}

int OverlayServer_GetRefreshIntervalMs()
{
    return std::clamp(g_overlayRefreshIntervalMs.load(std::memory_order_acquire), 1, 250);
}

void OverlayServer_SetRefreshIntervalMs(int ms)
{
    g_overlayRefreshIntervalMs.store(std::clamp(ms, 1, 250), std::memory_order_release);
}

bool OverlayServer_GetAutoStart()
{
    return g_overlayAutoStart.load(std::memory_order_acquire);
}

void OverlayServer_SetAutoStart(bool enabled)
{
    g_overlayAutoStart.store(enabled, std::memory_order_release);
}

bool OverlayServer_GetUseRawDepth()
{
    return g_overlayUseRawDepth.load(std::memory_order_acquire);
}

void OverlayServer_SetUseRawDepth(bool enabled)
{
    g_overlayUseRawDepth.store(enabled, std::memory_order_release);
}

int OverlayServer_GetLabelFontIndex()
{
    return std::clamp(g_overlayLabelFontIndex.load(std::memory_order_acquire), 0, 12);
}

void OverlayServer_SetLabelFontIndex(int index)
{
    g_overlayLabelFontIndex.store(std::clamp(index, 0, 12), std::memory_order_release);
}

int OverlayServer_GetLabelSizePx()
{
    return std::clamp(g_overlayLabelSizePx.load(std::memory_order_acquire), 8, 32);
}

void OverlayServer_SetLabelSizePx(int px)
{
    g_overlayLabelSizePx.store(std::clamp(px, 8, 32), std::memory_order_release);
}

int OverlayServer_GetLabelShadowPercent()
{
    return std::clamp(g_overlayLabelShadowPercent.load(std::memory_order_acquire), 0, 100);
}

void OverlayServer_SetLabelShadowPercent(int percent)
{
    g_overlayLabelShadowPercent.store(std::clamp(percent, 0, 100), std::memory_order_release);
}

uint32_t OverlayServer_GetLabelColor()
{
    return g_overlayLabelColor.load(std::memory_order_acquire) & 0x00ffffffu;
}

void OverlayServer_SetLabelColor(uint32_t rgb)
{
    g_overlayLabelColor.store(rgb & 0x00ffffffu, std::memory_order_release);
}

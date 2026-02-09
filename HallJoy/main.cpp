#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <objidl.h>
#include <gdiplus.h>
#include <string>

#include "app.h"
#include "win_util.h"
#include "Resource.h"
#include "debug_log.h"

#pragma comment(lib, "gdiplus.lib")

static HMODULE g_wootingWrapperModule = nullptr;
static constexpr int kDebugLogSchemaVersion = 6;

static bool FileExistsNoDir(const std::wstring& path)
{
    DWORD a = GetFileAttributesW(path.c_str());
    return (a != INVALID_FILE_ATTRIBUTES) && ((a & FILE_ATTRIBUTE_DIRECTORY) == 0);
}

static bool WriteBufferToFile(const std::wstring& path, const void* data, DWORD size)
{
    HANDLE h = CreateFileW(path.c_str(),
        GENERIC_WRITE,
        FILE_SHARE_READ,
        nullptr,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;

    const BYTE* p = (const BYTE*)data;
    DWORD total = 0;
    while (total < size)
    {
        DWORD n = 0;
        DWORD chunk = size - total;
        if (!WriteFile(h, p + total, chunk, &n, nullptr) || n == 0)
        {
            CloseHandle(h);
            return false;
        }
        total += n;
    }

    FlushFileBuffers(h);
    CloseHandle(h);
    return true;
}

static bool ExtractResourceToFile(HINSTANCE hInst, int resId, const std::wstring& dstPath)
{
    HRSRC hRes = FindResourceW(hInst, MAKEINTRESOURCEW(resId), RT_RCDATA);
    if (!hRes) return false;

    DWORD sz = SizeofResource(hInst, hRes);
    if (sz == 0) return false;

    HGLOBAL hData = LoadResource(hInst, hRes);
    if (!hData) return false;

    const void* p = LockResource(hData);
    if (!p) return false;

    std::wstring tmp = dstPath + L".tmp";
    DeleteFileW(tmp.c_str());

    if (!WriteBufferToFile(tmp, p, sz))
    {
        DeleteFileW(tmp.c_str());
        return false;
    }

    if (!MoveFileExW(tmp.c_str(), dstPath.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
    {
        DeleteFileW(tmp.c_str());
        return false;
    }

    return true;
}

static bool EnsureWootingWrapperReady(HINSTANCE hInst)
{
    const std::wstring dllPath = WinUtil_BuildPathNearExe(L"wooting_analog_wrapper.dll");
    DebugLog_Write(L"[wrapper] ensure path=%s", dllPath.c_str());

    if (!FileExistsNoDir(dllPath))
    {
        DebugLog_Write(L"[wrapper] dll not found near exe, extracting from resource");
        if (!ExtractResourceToFile(hInst, IDR_WOOTING_WRAPPER, dllPath))
        {
            DebugLog_Write(L"[wrapper] extract failed err=%lu", GetLastError());
            return false;
        }
        DebugLog_Write(L"[wrapper] extract ok");
    }

    g_wootingWrapperModule = LoadLibraryW(dllPath.c_str());
    DebugLog_Write(L"[wrapper] LoadLibrary result=%p err=%lu", g_wootingWrapperModule, GetLastError());
    return (g_wootingWrapperModule != nullptr);
}

static void InitDpiAwareness()
{
    HMODULE u32 = GetModuleHandleW(L"user32.dll");

    using SetCtxFn = BOOL(WINAPI*)(DPI_AWARENESS_CONTEXT);
    auto setCtx = (SetCtxFn)GetProcAddress(u32, "SetProcessDpiAwarenessContext");
    if (setCtx)
    {
        // system-aware: scales correctly at startup, without needing WM_DPICHANGED relayout
        setCtx(DPI_AWARENESS_CONTEXT_SYSTEM_AWARE);
        return;
    }

    // fallback (older Windows)
    using SetAwareFn = BOOL(WINAPI*)();
    auto setAware = (SetAwareFn)GetProcAddress(u32, "SetProcessDPIAware");
    if (setAware) setAware();
}

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, PWSTR, int nCmdShow)
{
    DebugLog_Init();
    DebugLog_Write(L"[build] log_schema=%d compiled=%S %S", kDebugLogSchemaVersion, __DATE__, __TIME__);
    DebugLog_Write(L"[main] wWinMain start hInst=%p cmdShow=%d", hInst, nCmdShow);

    if (!EnsureWootingWrapperReady(hInst))
    {
        DebugLog_Write(L"[main] wrapper prepare failed");
        MessageBoxW(nullptr,
            L"Failed to prepare wooting_analog_wrapper.dll near the executable.",
            L"HallJoy",
            MB_ICONERROR | MB_OK);
        return 1;
    }
    DebugLog_Write(L"[main] wrapper ready");

    InitDpiAwareness();
    DebugLog_Write(L"[main] dpi awareness configured");

    // Init GDI+ once for the entire application lifetime
    Gdiplus::GdiplusStartupInput gdiInput;
    ULONG_PTR gdiToken = 0;
    Gdiplus::Status gdiStatus = Gdiplus::GdiplusStartup(&gdiToken, &gdiInput, nullptr);
    DebugLog_Write(L"[main] Gdiplus startup status=%d token=%p", (int)gdiStatus, (void*)gdiToken);

    int result = App_Run(hInst, nCmdShow);
    DebugLog_Write(L"[main] App_Run returned=%d", result);

    if (gdiStatus == Gdiplus::Ok && gdiToken != 0)
        Gdiplus::GdiplusShutdown(gdiToken);
    DebugLog_Write(L"[main] exit");

    return result;
}

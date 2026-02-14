#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>
#include <tlhelp32.h>
#include <cstdint>
#include <atomic>
#include <cstdarg>
#include <wchar.h>

static constexpr const wchar_t* kHallJoyMouseIpcName = L"Local\\HallJoy_MouseBridge_v1";
static constexpr uint32_t kHallJoyMouseIpcMagic = 0x484A4D42u; // 'HJMB'
static constexpr uint32_t kHallJoyMouseIpcVersion = 1u;

struct HallJoyMouseIpcShared
{
    uint32_t magic;
    uint32_t version;
    volatile LONG blockMouseWanted;
    volatile LONG blockMouseActive;
    volatile LONG mouseToStickEnabled;
    volatile LONG pauseByRShift;
    volatile LONG heartbeat;
    volatile LONG asiHeartbeat;
    volatile LONG asiAttached;
    volatile LONG reserved1;
};

static HMODULE g_module = nullptr;
static HANDLE g_thread = nullptr;
static std::atomic<bool> g_stop{ false };

static HANDLE g_map = nullptr;
static HallJoyMouseIpcShared* g_ipc = nullptr;

static HWND g_gameWnd = nullptr;
static DWORD g_gameThreadId = 0;
static HHOOK g_msgHook = nullptr;
static bool g_cursorLocked = false;
static POINT g_lockPos{};
static ULONGLONG g_lastStateLogTick = 0;
static bool g_lastBlock = false;
static bool g_lastIpc = false;
static wchar_t g_logPath[MAX_PATH * 2]{};

typedef HRESULT(WINAPI* DirectInput8Create_t)(HINSTANCE, DWORD, REFIID, LPVOID*, LPUNKNOWN);
typedef HRESULT(STDMETHODCALLTYPE* DI8_CreateDevice_t)(void* self, REFGUID rguid, void** outDev, LPUNKNOWN);
typedef HRESULT(STDMETHODCALLTYPE* DIDev_GetDeviceState_t)(void* self, DWORD cbData, LPVOID data);
typedef HRESULT(STDMETHODCALLTYPE* DIDev_GetDeviceData_t)(void* self, DWORD cbObjectData, LPDIDEVICEOBJECTDATA rgdod, LPDWORD pdwInOut, DWORD flags);

static DirectInput8Create_t g_origDirectInput8Create = nullptr;
static DI8_CreateDevice_t g_origDI8CreateDeviceA = nullptr;
static DI8_CreateDevice_t g_origDI8CreateDeviceW = nullptr;
static DIDev_GetDeviceState_t g_origMouseGetDeviceState = nullptr;
static DIDev_GetDeviceData_t g_origMouseGetDeviceData = nullptr;
static bool g_diHookInstalled = false;

static HRESULT STDMETHODCALLTYPE Hook_DIDev_GetDeviceState(void* self, DWORD cbData, LPVOID data);
static HRESULT STDMETHODCALLTYPE Hook_DIDev_GetDeviceData(void* self, DWORD cbObjectData, LPDIDEVICEOBJECTDATA rgdod, LPDWORD pdwInOut, DWORD flags);
static HRESULT STDMETHODCALLTYPE Hook_DI8_CreateDeviceA(void* self, REFGUID rguid, void** outDev, LPUNKNOWN punkOuter);
static HRESULT STDMETHODCALLTYPE Hook_DI8_CreateDeviceW(void* self, REFGUID rguid, void** outDev, LPUNKNOWN punkOuter);
static HRESULT WINAPI Hook_DirectInput8Create(HINSTANCE hinst, DWORD ver, REFIID riid, LPVOID* outObj, LPUNKNOWN punkOuter);

static void InitLogPath()
{
    if (g_logPath[0] != 0)
        return;

    wchar_t modPath[MAX_PATH * 2]{};
    if (!GetModuleFileNameW(g_module, modPath, (DWORD)_countof(modPath)))
    {
        lstrcpyW(g_logPath, L"HallJoyASI.log");
        return;
    }

    wchar_t* slash = wcsrchr(modPath, L'\\');
    if (!slash)
        slash = wcsrchr(modPath, L'/');
    if (slash)
        *(slash + 1) = 0;

    lstrcpyW(g_logPath, modPath);
    lstrcatW(g_logPath, L"HallJoyASI.log");
}

static void AppendLogLineToFile(const wchar_t* line)
{
    InitLogPath();
    HANDLE h = CreateFileW(
        g_logPath,
        FILE_APPEND_DATA,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr,
        OPEN_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);
    if (h == INVALID_HANDLE_VALUE)
        return;

    int need = WideCharToMultiByte(CP_UTF8, 0, line, -1, nullptr, 0, nullptr, nullptr);
    if (need > 1)
    {
        char* buf = (char*)HeapAlloc(GetProcessHeap(), 0, (SIZE_T)need);
        if (buf)
        {
            if (WideCharToMultiByte(CP_UTF8, 0, line, -1, buf, need, nullptr, nullptr) > 0)
            {
                DWORD written = 0;
                // -1 to skip trailing '\0'
                WriteFile(h, buf, (DWORD)(need - 1), &written, nullptr);
            }
            HeapFree(GetProcessHeap(), 0, buf);
        }
    }
    CloseHandle(h);
}

static void Log(const wchar_t* fmt, ...)
{
    wchar_t msg[512]{};
    va_list ap;
    va_start(ap, fmt);
    _vsnwprintf_s(msg, _countof(msg), _TRUNCATE, fmt, ap);
    va_end(ap);

    wchar_t out[640]{};
    swprintf_s(out, L"[HallJoyASI] %s\n", msg);
    OutputDebugStringW(out);
    AppendLogLineToFile(out);
}

static bool IpcOpen()
{
    if (g_ipc) return true;
    g_map = OpenFileMappingW(FILE_MAP_READ | FILE_MAP_WRITE, FALSE, kHallJoyMouseIpcName);
    if (!g_map)
    {
        Log(L"IpcOpen: OpenFileMapping failed err=%lu", GetLastError());
        return false;
    }
    void* v = MapViewOfFile(g_map, FILE_MAP_READ | FILE_MAP_WRITE, 0, 0, sizeof(HallJoyMouseIpcShared));
    if (!v)
    {
        Log(L"IpcOpen: MapViewOfFile failed err=%lu", GetLastError());
        CloseHandle(g_map);
        g_map = nullptr;
        return false;
    }
    g_ipc = (HallJoyMouseIpcShared*)v;
    if (g_ipc->magic != kHallJoyMouseIpcMagic || g_ipc->version != kHallJoyMouseIpcVersion)
    {
        Log(L"IpcOpen: bad header magic=0x%08X version=%u", g_ipc->magic, g_ipc->version);
        UnmapViewOfFile(g_ipc);
        g_ipc = nullptr;
        CloseHandle(g_map);
        g_map = nullptr;
        return false;
    }
    g_ipc->asiAttached = 1;
    Log(L"IpcOpen: connected");
    return true;
}

static void IpcClose()
{
    if (g_ipc)
        g_ipc->asiAttached = 0;
    if (g_ipc || g_map)
        Log(L"IpcClose");
    if (g_ipc)
    {
        UnmapViewOfFile(g_ipc);
        g_ipc = nullptr;
    }
    if (g_map)
    {
        CloseHandle(g_map);
        g_map = nullptr;
    }
}

static bool IsBlockWanted()
{
    if (!g_ipc) return false;
    const bool wanted = (g_ipc->blockMouseWanted != 0);
    const bool pause = (g_ipc->pauseByRShift != 0);
    return wanted && !pause;
}

static bool PatchIATInModule(HMODULE moduleBase, const char* importDll, const char* funcName, void* hookFn, void** outOrig)
{
    if (!moduleBase || !importDll || !funcName || !hookFn) return false;

    auto* dos = (IMAGE_DOS_HEADER*)moduleBase;
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return false;
    auto* nt = (IMAGE_NT_HEADERS*)((BYTE*)moduleBase + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) return false;

    auto& dir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    if (!dir.VirtualAddress || !dir.Size) return false;

    auto* imp = (IMAGE_IMPORT_DESCRIPTOR*)((BYTE*)moduleBase + dir.VirtualAddress);
    for (; imp->Name; ++imp)
    {
        const char* dllName = (const char*)((BYTE*)moduleBase + imp->Name);
        if (!dllName) continue;
        if (_stricmp(dllName, importDll) != 0) continue;

        auto* firstThunk = (IMAGE_THUNK_DATA*)((BYTE*)moduleBase + imp->FirstThunk);
        auto* origThunk = imp->OriginalFirstThunk
            ? (IMAGE_THUNK_DATA*)((BYTE*)moduleBase + imp->OriginalFirstThunk)
            : firstThunk;

        for (; origThunk->u1.AddressOfData; ++origThunk, ++firstThunk)
        {
            if (IMAGE_SNAP_BY_ORDINAL(origThunk->u1.Ordinal))
                continue;
            auto* ibn = (IMAGE_IMPORT_BY_NAME*)((BYTE*)moduleBase + origThunk->u1.AddressOfData);
            if (!ibn || !ibn->Name) continue;
            if (strcmp((const char*)ibn->Name, funcName) != 0)
                continue;

            FARPROC* pfn = (FARPROC*)&firstThunk->u1.Function;
            DWORD oldProt = 0;
            if (!VirtualProtect(pfn, sizeof(void*), PAGE_EXECUTE_READWRITE, &oldProt))
                return false;

            if (outOrig && !*outOrig)
                *outOrig = (void*)(*pfn);

            *pfn = (FARPROC)hookFn;
            DWORD tmp = 0;
            VirtualProtect(pfn, sizeof(void*), oldProt, &tmp);
            FlushInstructionCache(GetCurrentProcess(), pfn, sizeof(void*));
            return true;
        }
    }
    return false;
}

static bool PatchIATAllModules(const char* importDll, const char* funcName, void* hookFn, void** outOrig)
{
    DWORD pid = GetCurrentProcessId();
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
    if (snap == INVALID_HANDLE_VALUE)
        return false;

    MODULEENTRY32 me{};
    me.dwSize = sizeof(me);
    bool patchedAny = false;
    if (Module32First(snap, &me))
    {
        do
        {
            HMODULE mod = (HMODULE)me.hModule;
            if (PatchIATInModule(mod, importDll, funcName, hookFn, outOrig))
                patchedAny = true;
        } while (Module32Next(snap, &me));
    }
    CloseHandle(snap);
    return patchedAny;
}

static bool PatchVtableEntry(void** vtbl, size_t index, void* hookFn, void** outOrig)
{
    if (!vtbl || !hookFn) return false;
    void** slot = &vtbl[index];
    DWORD oldProt = 0;
    if (!VirtualProtect(slot, sizeof(void*), PAGE_EXECUTE_READWRITE, &oldProt))
        return false;
    if (outOrig && !*outOrig)
        *outOrig = *slot;
    *slot = hookFn;
    DWORD tmp = 0;
    VirtualProtect(slot, sizeof(void*), oldProt, &tmp);
    FlushInstructionCache(GetCurrentProcess(), slot, sizeof(void*));
    return true;
}

static void HookMouseDeviceVtable(void* dev)
{
    if (!dev) return;
    void** vtbl = *(void***)dev;
    if (!vtbl) return;

    // IDirectInputDevice8::GetDeviceState index = 9, GetDeviceData index = 10
    bool ok1 = PatchVtableEntry(vtbl, 9, (void*)&Hook_DIDev_GetDeviceState, (void**)&g_origMouseGetDeviceState);
    bool ok2 = PatchVtableEntry(vtbl, 10, (void*)&Hook_DIDev_GetDeviceData, (void**)&g_origMouseGetDeviceData);
    Log(L"HookMouseDeviceVtable: dev=%p state=%d data=%d", dev, ok1 ? 1 : 0, ok2 ? 1 : 0);
}

static HRESULT STDMETHODCALLTYPE Hook_DIDev_GetDeviceState(void* self, DWORD cbData, LPVOID data)
{
    if (IsBlockWanted())
    {
        if (data && cbData > 0)
            ZeroMemory(data, cbData);
        return DI_OK;
    }
    if (g_origMouseGetDeviceState)
        return g_origMouseGetDeviceState(self, cbData, data);
    return E_FAIL;
}

static HRESULT STDMETHODCALLTYPE Hook_DIDev_GetDeviceData(void* self, DWORD cbObjectData, LPDIDEVICEOBJECTDATA rgdod, LPDWORD pdwInOut, DWORD flags)
{
    if (IsBlockWanted())
    {
        if (rgdod && pdwInOut && *pdwInOut > 0 && cbObjectData > 0)
            ZeroMemory(rgdod, (*pdwInOut) * cbObjectData);
        if (pdwInOut) *pdwInOut = 0;
        return DI_OK;
    }
    if (g_origMouseGetDeviceData)
        return g_origMouseGetDeviceData(self, cbObjectData, rgdod, pdwInOut, flags);
    return E_FAIL;
}

static HRESULT STDMETHODCALLTYPE Hook_DI8_CreateDeviceA(void* self, REFGUID rguid, void** outDev, LPUNKNOWN punkOuter)
{
    if (!g_origDI8CreateDeviceA)
        return E_FAIL;
    HRESULT hr = g_origDI8CreateDeviceA(self, rguid, outDev, punkOuter);
    if (SUCCEEDED(hr) && outDev && *outDev && IsEqualGUID(rguid, GUID_SysMouse))
    {
        Log(L"DI8A CreateDevice: mouse dev=%p", *outDev);
        HookMouseDeviceVtable(*outDev);
    }
    return hr;
}

static HRESULT STDMETHODCALLTYPE Hook_DI8_CreateDeviceW(void* self, REFGUID rguid, void** outDev, LPUNKNOWN punkOuter)
{
    if (!g_origDI8CreateDeviceW)
        return E_FAIL;
    HRESULT hr = g_origDI8CreateDeviceW(self, rguid, outDev, punkOuter);
    if (SUCCEEDED(hr) && outDev && *outDev && IsEqualGUID(rguid, GUID_SysMouse))
    {
        Log(L"DI8W CreateDevice: mouse dev=%p", *outDev);
        HookMouseDeviceVtable(*outDev);
    }
    return hr;
}

static void HookDirectInput8Interface(LPVOID obj, REFIID riid)
{
    if (!obj) return;
    void** vtbl = *(void***)obj;
    if (!vtbl) return;

    if (IsEqualIID(riid, IID_IDirectInput8A))
    {
        bool ok = PatchVtableEntry(vtbl, 3, (void*)&Hook_DI8_CreateDeviceA, (void**)&g_origDI8CreateDeviceA);
        Log(L"HookDirectInput8Interface: IID_IDirectInput8A patch=%d", ok ? 1 : 0);
    }
    else if (IsEqualIID(riid, IID_IDirectInput8W))
    {
        bool ok = PatchVtableEntry(vtbl, 3, (void*)&Hook_DI8_CreateDeviceW, (void**)&g_origDI8CreateDeviceW);
        Log(L"HookDirectInput8Interface: IID_IDirectInput8W patch=%d", ok ? 1 : 0);
    }
}

static HRESULT WINAPI Hook_DirectInput8Create(HINSTANCE hinst, DWORD ver, REFIID riid, LPVOID* outObj, LPUNKNOWN punkOuter)
{
    if (!g_origDirectInput8Create)
        return E_FAIL;
    HRESULT hr = g_origDirectInput8Create(hinst, ver, riid, outObj, punkOuter);
    Log(L"DirectInput8Create: hr=0x%08lX riid=%08lX", (unsigned long)hr, (unsigned long)riid.Data1);
    if (SUCCEEDED(hr) && outObj && *outObj)
        HookDirectInput8Interface(*outObj, riid);
    return hr;
}

static void InstallDirectInputHookIfNeeded()
{
    if (g_diHookInstalled)
        return;

    bool patched = PatchIATAllModules(
        "dinput8.dll",
        "DirectInput8Create",
        (void*)&Hook_DirectInput8Create,
        (void**)&g_origDirectInput8Create);

    if (patched)
    {
        g_diHookInstalled = true;
        Log(L"InstallDirectInputHook: patched=1 orig=%p", (void*)g_origDirectInput8Create);
    }
    else
    {
        Log(L"InstallDirectInputHook: patched=0 (not imported yet?)");
    }
}

static BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam)
{
    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    if (pid != GetCurrentProcessId())
        return TRUE;

    if (!IsWindowVisible(hwnd))
        return TRUE;
    if (GetWindow(hwnd, GW_OWNER))
        return TRUE;

    HWND* out = (HWND*)lParam;
    *out = hwnd;
    return FALSE;
}

static HWND FindGameWindow()
{
    HWND w = nullptr;
    EnumWindows(EnumWindowsProc, (LPARAM)&w);
    if (w)
    {
        wchar_t cls[128]{};
        GetClassNameW(w, cls, (int)_countof(cls));
        Log(L"FindGameWindow: hwnd=%p class=%s", w, cls);
    }
    return w;
}

static void ReleaseCursorLock()
{
    if (g_cursorLocked)
    {
        ClipCursor(nullptr);
        g_cursorLocked = false;
        Log(L"Cursor lock OFF");
    }
}

static void EnsureCursorLock(HWND wnd)
{
    if (g_cursorLocked || !wnd) return;

    RECT rc{};
    if (!GetClientRect(wnd, &rc)) return;
    POINT p{ (rc.left + rc.right) / 2, (rc.top + rc.bottom) / 2 };
    ClientToScreen(wnd, &p);
    g_lockPos = p;

    RECT clip{ p.x, p.y, p.x + 1, p.y + 1 };
    ClipCursor(&clip);
    SetCursorPos(p.x, p.y);
    g_cursorLocked = true;
    Log(L"Cursor lock ON at %ld,%ld", (long)p.x, (long)p.y);
}

static LRESULT CALLBACK GameMsgHookProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    if (nCode >= 0 && lParam)
    {
        MSG* m = (MSG*)lParam;
        const bool block = IsBlockWanted();
        if (block)
        {
            switch (m->message)
            {
            case WM_MOUSEMOVE:
            case WM_LBUTTONDOWN:
            case WM_LBUTTONUP:
            case WM_LBUTTONDBLCLK:
            case WM_RBUTTONDOWN:
            case WM_RBUTTONUP:
            case WM_RBUTTONDBLCLK:
            case WM_MBUTTONDOWN:
            case WM_MBUTTONUP:
            case WM_MBUTTONDBLCLK:
            case WM_XBUTTONDOWN:
            case WM_XBUTTONUP:
            case WM_XBUTTONDBLCLK:
            case WM_MOUSEWHEEL:
            case WM_MOUSEHWHEEL:
            case WM_INPUT:
                // Drop message before game's message loop handles it.
                m->message = WM_NULL;
                m->wParam = 0;
                m->lParam = 0;
                break;
            default:
                break;
            }
        }
    }
    return CallNextHookEx(g_msgHook, nCode, wParam, lParam);
}

static void InstallMsgHookIfNeeded()
{
    if (g_msgHook)
        return;

    HWND wnd = FindGameWindow();
    if (!wnd) return;
    g_gameWnd = wnd;
    g_gameThreadId = GetWindowThreadProcessId(wnd, nullptr);
    if (g_gameThreadId == 0) return;

    g_msgHook = SetWindowsHookExW(WH_GETMESSAGE, GameMsgHookProc, g_module, g_gameThreadId);
    if (g_msgHook)
        Log(L"Message hook installed hwnd=%p tid=%lu", wnd, (unsigned long)g_gameThreadId);
    else
        Log(L"Message hook failed hwnd=%p tid=%lu err=%lu", wnd, (unsigned long)g_gameThreadId, GetLastError());
}

static void UninstallMsgHook()
{
    ReleaseCursorLock();
    if (g_msgHook)
    {
        UnhookWindowsHookEx(g_msgHook);
        g_msgHook = nullptr;
        Log(L"Message hook removed");
    }
    g_gameWnd = nullptr;
    g_gameThreadId = 0;
}

static DWORD WINAPI WorkerThread(LPVOID)
{
    Log(L"Worker thread start");
    InstallDirectInputHookIfNeeded();
    Sleep(300);
    Log(L"Worker thread probe begin");
    while (!g_stop.load(std::memory_order_relaxed))
    {
        __try
        {
            InstallDirectInputHookIfNeeded();

            if (!g_ipc)
                IpcOpen();

            if (g_ipc)
                InterlockedIncrement((volatile LONG*)&g_ipc->asiHeartbeat);

            InstallMsgHookIfNeeded();

            bool block = IsBlockWanted();
            if (block && g_gameWnd)
                EnsureCursorLock(g_gameWnd);
            bool hasIpc = (g_ipc != nullptr);
            ULONGLONG now = GetTickCount64();
            if (block != g_lastBlock || hasIpc != g_lastIpc || (now - g_lastStateLogTick) >= 1000)
            {
                g_lastBlock = block;
                g_lastIpc = hasIpc;
                g_lastStateLogTick = now;
                Log(L"state: ipc=%d block=%d attached=%d hb=%ld",
                    hasIpc ? 1 : 0,
                    block ? 1 : 0,
                    (g_ipc ? (int)g_ipc->asiAttached : 0),
                    (g_ipc ? g_ipc->heartbeat : 0));
            }

            if (!block)
                ReleaseCursorLock();
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            DWORD code = GetExceptionCode();
            Log(L"EXCEPTION in worker: 0x%08lX", code);
            break;
        }

        Sleep(5);
    }

    Log(L"Worker thread stop");
    UninstallMsgHook();
    IpcClose();
    return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        g_module = hModule;
        DisableThreadLibraryCalls(hModule);
        g_stop.store(false, std::memory_order_relaxed);
        Log(L"DLL_PROCESS_ATTACH module=%p", hModule);
        g_thread = CreateThread(nullptr, 0, WorkerThread, nullptr, 0, nullptr);
        if (!g_thread)
            Log(L"CreateThread failed err=%lu", GetLastError());
    }
    else if (reason == DLL_PROCESS_DETACH)
    {
        Log(L"DLL_PROCESS_DETACH");
        g_stop.store(true, std::memory_order_relaxed);
        if (g_thread)
        {
            WaitForSingleObject(g_thread, 500);
            CloseHandle(g_thread);
            g_thread = nullptr;
        }
        UninstallMsgHook();
        IpcClose();
        g_module = nullptr;
    }
    return TRUE;
}

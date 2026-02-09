#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <string>
#include <vector>
#include <cstdarg>
#include <cwchar>

#include "debug_log.h"

static SRWLOCK g_logLock = SRWLOCK_INIT;
static std::wstring g_logPath;
static bool g_logReady = false;

static std::wstring BuildPathNearExe(const wchar_t* fileName)
{
    std::vector<wchar_t> buf(1024);
    DWORD len = 0;
    for (;;)
    {
        len = GetModuleFileNameW(nullptr, buf.data(), (DWORD)buf.size());
        if (len == 0) return std::wstring(fileName ? fileName : L"log.txt");
        if (len < buf.size()) break;
        if (buf.size() > 65536) return std::wstring(fileName ? fileName : L"log.txt");
        buf.resize(buf.size() * 2);
    }

    std::wstring p(buf.data(), len);
    size_t slash = p.find_last_of(L"\\/");
    if (slash != std::wstring::npos) p.erase(slash + 1);
    else p.clear();
    p += (fileName ? fileName : L"log.txt");
    return p;
}

static std::string WideToUtf8(const wchar_t* ws)
{
    if (!ws) return {};
    int wlen = (int)wcslen(ws);
    if (wlen <= 0) return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, ws, wlen, nullptr, 0, nullptr, nullptr);
    if (n <= 0) return {};
    std::string out((size_t)n, '\0');
    WideCharToMultiByte(CP_UTF8, 0, ws, wlen, out.data(), n, nullptr, nullptr);
    return out;
}

static void WriteUtf8Line(const wchar_t* line)
{
    if (!line || !*line || g_logPath.empty()) return;

    HANDLE h = CreateFileW(
        g_logPath.c_str(),
        FILE_APPEND_DATA,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr,
        OPEN_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);
    if (h == INVALID_HANDLE_VALUE) return;

    std::string utf8 = WideToUtf8(line);
    if (!utf8.empty())
    {
        DWORD written = 0;
        WriteFile(h, utf8.data(), (DWORD)utf8.size(), &written, nullptr);
    }

    static const char nl[] = "\r\n";
    DWORD w2 = 0;
    WriteFile(h, nl, (DWORD)sizeof(nl) - 1, &w2, nullptr);

    CloseHandle(h);
}

void DebugLog_Init()
{
    AcquireSRWLockExclusive(&g_logLock);

    g_logPath = BuildPathNearExe(L"log.txt");
    g_logReady = false;

    HANDLE h = CreateFileW(
        g_logPath.c_str(),
        GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);
    if (h != INVALID_HANDLE_VALUE)
    {
        static const unsigned char utf8Bom[3] = { 0xEF, 0xBB, 0xBF };
        DWORD w = 0;
        WriteFile(h, utf8Bom, 3, &w, nullptr);
        CloseHandle(h);
        g_logReady = true;
    }

    ReleaseSRWLockExclusive(&g_logLock);

    SYSTEMTIME st{};
    GetLocalTime(&st);
    DebugLog_Write(
        L"[log.init] %04u-%02u-%02u %02u:%02u:%02u.%03u pid=%lu path=%s",
        st.wYear, st.wMonth, st.wDay,
        st.wHour, st.wMinute, st.wSecond, st.wMilliseconds,
        GetCurrentProcessId(),
        g_logPath.c_str());
}

void DebugLog_Write(const wchar_t* fmt, ...)
{
    if (!fmt || !*fmt) return;

    AcquireSRWLockShared(&g_logLock);
    const bool ready = g_logReady;
    ReleaseSRWLockShared(&g_logLock);
    if (!ready) return;

    wchar_t msg[2048]{};
    va_list ap;
    va_start(ap, fmt);
    _vsnwprintf_s(msg, _countof(msg), _TRUNCATE, fmt, ap);
    va_end(ap);

    SYSTEMTIME st{};
    GetLocalTime(&st);

    wchar_t line[2300]{};
    _snwprintf_s(
        line, _countof(line), _TRUNCATE,
        L"[%02u:%02u:%02u.%03u][t%lu] %s",
        st.wHour, st.wMinute, st.wSecond, st.wMilliseconds,
        GetCurrentThreadId(),
        msg);

    AcquireSRWLockExclusive(&g_logLock);
    WriteUtf8Line(line);
    ReleaseSRWLockExclusive(&g_logLock);
}

const wchar_t* DebugLog_Path()
{
    AcquireSRWLockShared(&g_logLock);
    const wchar_t* p = g_logPath.c_str();
    ReleaseSRWLockShared(&g_logLock);
    return p;
}

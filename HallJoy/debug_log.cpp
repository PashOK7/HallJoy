#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <string>
#include <vector>
#include <utility>
#include <cstdarg>
#include <cwchar>
#include <atomic>

#include "debug_log.h"

static SRWLOCK g_logLock = SRWLOCK_INIT;
static std::wstring g_logPath;
static HANDLE g_logFile = INVALID_HANDLE_VALUE;
static HANDLE g_writerThread = nullptr;
static HANDLE g_writeEvent = nullptr; // manual-reset
static std::vector<std::wstring> g_pendingLines;
static std::atomic<bool> g_logReady{ false };
static std::atomic<bool> g_stopWriter{ false };

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

static void WriteUtf8Line(HANDLE hFile, const wchar_t* line)
{
    if (!line || !*line || hFile == INVALID_HANDLE_VALUE) return;

    std::string utf8 = WideToUtf8(line);
    if (!utf8.empty())
    {
        DWORD written = 0;
        WriteFile(hFile, utf8.data(), (DWORD)utf8.size(), &written, nullptr);
    }

    static const char nl[] = "\r\n";
    DWORD w2 = 0;
    WriteFile(hFile, nl, (DWORD)sizeof(nl) - 1, &w2, nullptr);
}

static DWORD WINAPI DebugLogWriterThreadProc(LPVOID)
{
    for (;;)
    {
        if (!g_writeEvent)
            return 0;

        WaitForSingleObject(g_writeEvent, INFINITE);

        for (;;)
        {
            std::vector<std::wstring> batch;
            HANDLE hFile = INVALID_HANDLE_VALUE;

            AcquireSRWLockExclusive(&g_logLock);
            if (!g_pendingLines.empty())
                batch.swap(g_pendingLines);
            hFile = g_logFile;
            if (g_pendingLines.empty() && g_writeEvent)
                ResetEvent(g_writeEvent);
            ReleaseSRWLockExclusive(&g_logLock);

            if (!batch.empty())
            {
                for (const auto& line : batch)
                    WriteUtf8Line(hFile, line.c_str());
            }

            if (batch.empty())
                break;
        }

        if (g_stopWriter.load(std::memory_order_relaxed))
            return 0;
    }
}

void DebugLog_Init()
{
#if defined(NDEBUG)
    return;
#endif

    AcquireSRWLockExclusive(&g_logLock);

    if (g_logReady.load(std::memory_order_relaxed))
    {
        ReleaseSRWLockExclusive(&g_logLock);
        return;
    }

    g_logPath = BuildPathNearExe(L"log.txt");
    g_pendingLines.clear();
    g_stopWriter.store(false, std::memory_order_relaxed);

    g_logFile = CreateFileW(
        g_logPath.c_str(),
        GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);
    if (g_logFile != INVALID_HANDLE_VALUE)
    {
        static const unsigned char utf8Bom[3] = { 0xEF, 0xBB, 0xBF };
        DWORD w = 0;
        WriteFile(g_logFile, utf8Bom, 3, &w, nullptr);
    }

    if (g_logFile != INVALID_HANDLE_VALUE)
    {
        g_writeEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        if (g_writeEvent)
        {
            g_writerThread = CreateThread(nullptr, 0, DebugLogWriterThreadProc, nullptr, 0, nullptr);
        }
    }

    if (g_logFile == INVALID_HANDLE_VALUE || !g_writeEvent || !g_writerThread)
    {
        if (g_writerThread)
        {
            CloseHandle(g_writerThread);
            g_writerThread = nullptr;
        }
        if (g_writeEvent)
        {
            CloseHandle(g_writeEvent);
            g_writeEvent = nullptr;
        }
        if (g_logFile != INVALID_HANDLE_VALUE)
        {
            CloseHandle(g_logFile);
            g_logFile = INVALID_HANDLE_VALUE;
        }
        g_logReady.store(false, std::memory_order_relaxed);
    }
    else
    {
        g_logReady.store(true, std::memory_order_relaxed);
    }

    ReleaseSRWLockExclusive(&g_logLock);

    if (!g_logReady.load(std::memory_order_relaxed))
        return;

    SYSTEMTIME st{};
    GetLocalTime(&st);
    DebugLog_Write(
        L"[log.init] %04u-%02u-%02u %02u:%02u:%02u.%03u pid=%lu path=%s",
        st.wYear, st.wMonth, st.wDay,
        st.wHour, st.wMinute, st.wSecond, st.wMilliseconds,
        GetCurrentProcessId(),
        g_logPath.c_str());
}

void DebugLog_Shutdown()
{
#if defined(NDEBUG)
    return;
#endif

    HANDLE threadToJoin = nullptr;
    HANDLE fileToClose = INVALID_HANDLE_VALUE;
    HANDLE eventToClose = nullptr;

    AcquireSRWLockExclusive(&g_logLock);
    if (!g_logReady.load(std::memory_order_relaxed))
    {
        ReleaseSRWLockExclusive(&g_logLock);
        return;
    }

    g_stopWriter.store(true, std::memory_order_release);
    if (g_writeEvent)
        SetEvent(g_writeEvent);
    threadToJoin = g_writerThread;
    g_writerThread = nullptr;
    ReleaseSRWLockExclusive(&g_logLock);

    if (threadToJoin)
    {
        WaitForSingleObject(threadToJoin, INFINITE);
        CloseHandle(threadToJoin);
    }

    AcquireSRWLockExclusive(&g_logLock);
    fileToClose = g_logFile;
    g_logFile = INVALID_HANDLE_VALUE;
    eventToClose = g_writeEvent;
    g_writeEvent = nullptr;
    g_pendingLines.clear();
    g_logReady.store(false, std::memory_order_release);
    ReleaseSRWLockExclusive(&g_logLock);

    if (eventToClose)
        CloseHandle(eventToClose);
    if (fileToClose != INVALID_HANDLE_VALUE)
        CloseHandle(fileToClose);
}

void DebugLog_Write(const wchar_t* fmt, ...)
{
#if defined(NDEBUG)
    (void)fmt;
    return;
#endif

    if (!fmt || !*fmt) return;

    const bool ready = g_logReady.load(std::memory_order_acquire);
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
    if (g_logReady.load(std::memory_order_relaxed))
    {
        g_pendingLines.emplace_back(line);
        if (g_writeEvent)
            SetEvent(g_writeEvent);
    }
    ReleaseSRWLockExclusive(&g_logLock);
}

const wchar_t* DebugLog_Path()
{
#if defined(NDEBUG)
    return L"";
#endif

    AcquireSRWLockShared(&g_logLock);
    const wchar_t* p = g_logPath.c_str();
    ReleaseSRWLockShared(&g_logLock);
    return p;
}

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <tlhelp32.h>

#include <string>
#include <vector>
#include <utility>
#include <cstdarg>
#include <cwchar>
#include <cstdint>
#include <cstdlib>
#include <atomic>
#include <algorithm>

#include "debug_log.h"
#include "worker_exception_barrier.h"
#if defined(HALLJOY_MAD68PR_NATIVE)
#include "mad68pr_backend.h"
#endif

static SRWLOCK g_logLock = SRWLOCK_INIT;
static std::wstring g_logPath;
static HANDLE g_logFile = INVALID_HANDLE_VALUE;
static HANDLE g_writerThread = nullptr;
static HANDLE g_writeEvent = nullptr; // manual-reset
static std::vector<std::wstring> g_pendingLines;
static std::atomic<bool> g_logReady{ false };
static std::atomic<bool> g_stopWriter{ false };
static std::atomic<bool> g_writerExited{ true };
static std::atomic<halljoy::worker::WorkerExceptionKind> g_writerFaultKind{
    halljoy::worker::WorkerExceptionKind::None };
static halljoy::worker::WorkerExceptionRecord g_writerFaultRecord{};
static thread_local wchar_t g_lastCheckpoint[384] = L"startup";
static std::atomic<bool> g_watchdogStarted{ false };
static HANDLE g_watchdogNormalExitEvent = nullptr;
static PVOID g_vectoredExceptionHandler = nullptr;
static std::atomic<bool> g_vectoredFatalReportWritten{ false };
static constexpr DWORD kBufferedFlushIntervalMs = 250;

#if defined(NDEBUG) && !defined(HALLJOY_DIAGNOSTIC)
#define HALLJOY_LOG_DISABLED 1
#else
#define HALLJOY_LOG_DISABLED 0
#endif

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

static bool ParseDiagnosticWatchPid(DWORD& pidOut)
{
#if !defined(HALLJOY_DIAGNOSTIC) && !defined(HALLJOY_MAD68PR_NATIVE)
    (void)pidOut;
    return false;
#else
    const wchar_t* marker = wcsstr(GetCommandLineW(), L"--diagnostic-watch=");
    if (!marker) return false;
    marker += wcslen(L"--diagnostic-watch=");
    wchar_t* end = nullptr;
    unsigned long value = wcstoul(marker, &end, 10);
    if (end == marker || value == 0 || value > 0xFFFFFFFFul)
        return false;
    pidOut = (DWORD)value;
    return true;
#endif
}

static std::wstring WatchEventName(const wchar_t* kind, DWORD pid)
{
    wchar_t name[160]{};
    _snwprintf_s(
        name, _countof(name), _TRUNCATE,
        L"Local\\HallJoyDiagnostic%s_%lu",
        kind ? kind : L"Watch",
        pid);
    return name;
}

static const wchar_t* ExitCodeHint(DWORD code)
{
    switch (code)
    {
    case 0x00000000u: return L"success";
    case 0x00000003u: return L"abort_or_fatal_runtime";
    case 0x40000015u: return L"fatal_app_exit";
    case 0xC0000005u: return L"access_violation";
    case 0xC000001Du: return L"illegal_instruction";
    case 0xC0000094u: return L"integer_divide_by_zero";
    case 0xC00000FDu: return L"stack_overflow";
    case 0xC0000374u: return L"heap_corruption";
    case 0xC0000409u: return L"fast_fail_or_stack_buffer_overrun";
    default: return L"unknown";
    }
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

static void ReplaceAllInsensitive(std::wstring& text, const std::wstring& needle, const wchar_t* replacement)
{
    if (needle.empty() || !replacement) return;

    size_t pos = 0;
    while (pos < text.size())
    {
        size_t found = std::wstring::npos;
        for (size_t i = pos; i + needle.size() <= text.size(); ++i)
        {
            if (_wcsnicmp(text.c_str() + i, needle.c_str(), needle.size()) == 0)
            {
                found = i;
                break;
            }
        }
        if (found == std::wstring::npos) break;
        text.replace(found, needle.size(), replacement);
        pos = found + wcslen(replacement);
    }
}

static std::wstring SanitizeDiagnosticText(const wchar_t* text)
{
    std::wstring out = text ? text : L"";
#if defined(HALLJOY_DIAGNOSTIC)
    const std::wstring appDir = BuildPathNearExe(L"");
    if (!appDir.empty())
        ReplaceAllInsensitive(out, appDir, L"<APPDIR>\\");

    wchar_t profile[32768]{};
    DWORD profileLen = GetEnvironmentVariableW(L"USERPROFILE", profile, (DWORD)_countof(profile));
    if (profileLen > 0 && profileLen < _countof(profile))
        ReplaceAllInsensitive(out, profile, L"<USERPROFILE>");

    wchar_t temp[32768]{};
    DWORD tempLen = GetTempPathW((DWORD)_countof(temp), temp);
    if (tempLen > 0 && tempLen < _countof(temp))
        ReplaceAllInsensitive(out, temp, L"<TEMP>\\");
#endif
    return out;
}

static void WriteUtf8Line(HANDLE hFile, const wchar_t* line)
{
    if (!line || !*line || hFile == INVALID_HANDLE_VALUE) return;

    std::wstring safeLine = SanitizeDiagnosticText(line);
    std::string utf8 = WideToUtf8(safeLine.c_str());
    if (!utf8.empty())
    {
        DWORD written = 0;
        WriteFile(hFile, utf8.data(), (DWORD)utf8.size(), &written, nullptr);
    }

    static const char nl[] = "\r\n";
    DWORD w2 = 0;
    WriteFile(hFile, nl, (DWORD)sizeof(nl) - 1, &w2, nullptr);
}

static DWORD DebugLogWriterThreadBody()
{
    bool dirty = false;
    ULONGLONG lastFlushMs = GetTickCount64();
    std::vector<std::wstring> batch;
    batch.reserve(256);

    for (;;)
    {
        HANDLE writeEvent = nullptr;
        AcquireSRWLockShared(&g_logLock);
        writeEvent = g_writeEvent;
        ReleaseSRWLockShared(&g_logLock);
        if (!writeEvent)
            return 0;

        const DWORD waitMs = dirty ? kBufferedFlushIntervalMs : INFINITE;
        const DWORD waitResult = WaitForSingleObject(writeEvent, waitMs);
        if (waitResult == WAIT_FAILED)
            return 0;

        for (;;)
        {
            HANDLE hFile = INVALID_HANDLE_VALUE;

            batch.clear();
            AcquireSRWLockExclusive(&g_logLock);
            if (!g_pendingLines.empty())
                batch.swap(g_pendingLines);
            hFile = g_logFile;
            if (g_pendingLines.empty() && g_writeEvent)
                ResetEvent(g_writeEvent);
            ReleaseSRWLockExclusive(&g_logLock);

            if (batch.empty())
                break;

            // Never hold the queue/state lock during filesystem I/O. Producers,
            // including the realtime thread, can enqueue while this worker writes.
            for (const auto& line : batch)
                WriteUtf8Line(hFile, line.c_str());
            dirty = true;
        }

        const bool stopping = g_stopWriter.load(std::memory_order_acquire);
        const ULONGLONG nowMs = GetTickCount64();
        if (dirty && (stopping || waitResult == WAIT_TIMEOUT || nowMs - lastFlushMs >= kBufferedFlushIntervalMs))
        {
            HANDLE hFile = INVALID_HANDLE_VALUE;
            AcquireSRWLockShared(&g_logLock);
            hFile = g_logFile;
            ReleaseSRWLockShared(&g_logLock);
            if (hFile != INVALID_HANDLE_VALUE)
                FlushFileBuffers(hFile);
            dirty = false;
            lastFlushMs = nowMs;
        }

        if (stopping)
            return 0;
    }
}

static void DebugLogWriterOnFault(
    const halljoy::worker::WorkerExceptionRecord& record) noexcept
{
    g_writerFaultRecord = record;
    g_writerFaultKind.store(record.kind, std::memory_order_release);

    // Close the producer gate immediately. Otherwise producers would continue
    // growing an unconsumed queue after the writer has terminated.
    g_logReady.store(false, std::memory_order_release);
    g_stopWriter.store(true, std::memory_order_release);
    HANDLE writeEvent = g_writeEvent;
    if (writeEvent)
        SetEvent(writeEvent);

    OutputDebugStringA("[HallJoy] debug writer exception: ");
    OutputDebugStringA(record.message);
    OutputDebugStringA("\r\n");
}

static void DebugLogWriterOnCompletion(
    const halljoy::worker::WorkerExceptionRecord&) noexcept
{
    g_writerExited.store(true, std::memory_order_release);
}

static DWORD WINAPI DebugLogWriterThreadProc(LPVOID) noexcept
{
    g_writerExited.store(false, std::memory_order_release);
    return static_cast<DWORD>(halljoy::worker::RunWorkerEntryBarrier(
        [] { return DebugLogWriterThreadBody(); },
        DebugLogWriterOnFault,
        DebugLogWriterOnCompletion,
        0xE0514C45u));
}

void DebugLog_Init()
{
#if HALLJOY_LOG_DISABLED
    return;
#endif

#if defined(HALLJOY_DIAGNOSTIC)
    // Prevent a report from an earlier run being mistaken for the current one.
    DeleteFileW(BuildPathNearExe(L"HallJoyDiagnosticCrash.txt").c_str());
    DeleteFileW(BuildPathNearExe(L"HallJoyDiagnosticVectored.txt").c_str());
    DeleteFileW(BuildPathNearExe(L"HallJoyDiagnosticExit.txt").c_str());
#endif

    AcquireSRWLockExclusive(&g_logLock);

    if (g_logReady.load(std::memory_order_relaxed))
    {
        ReleaseSRWLockExclusive(&g_logLock);
        return;
    }
    if (g_writerThread || g_writeEvent || g_logFile != INVALID_HANDLE_VALUE)
    {
        // A failed writer generation must be reaped by DebugLog_Shutdown before
        // a new generation can replace any of its HANDLEs or file state.
        ReleaseSRWLockExclusive(&g_logLock);
        return;
    }

#if defined(HALLJOY_DIAGNOSTIC)
    g_logPath = BuildPathNearExe(L"HallJoyDiagnostic.log");
#else
    g_logPath = BuildPathNearExe(L"log.txt");
#endif
    g_pendingLines.clear();
    g_stopWriter.store(false, std::memory_order_relaxed);
    g_writerExited.store(true, std::memory_order_relaxed);
    g_writerFaultRecord = {};
    g_writerFaultKind.store(halljoy::worker::WorkerExceptionKind::None, std::memory_order_relaxed);

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
#if defined(HALLJOY_DIAGNOSTIC) || defined(HALLJOY_MAD68PR_NATIVE)
    // The quiet native watchdog remains active in production so an unexpected
    // process exit can still restore A9. Signalling normal shutdown must happen
    // even when ordinary file logging is compiled out.
    if (g_watchdogNormalExitEvent)
    {
        SetEvent(g_watchdogNormalExitEvent);
        CloseHandle(g_watchdogNormalExitEvent);
        g_watchdogNormalExitEvent = nullptr;
    }
#endif

#if HALLJOY_LOG_DISABLED
    return;
#endif

    HANDLE threadToJoin = nullptr;
    HANDLE fileToClose = INVALID_HANDLE_VALUE;
    HANDLE eventToClose = nullptr;

    AcquireSRWLockExclusive(&g_logLock);
    const bool ownsResources = g_writerThread || g_writeEvent || g_logFile != INVALID_HANDLE_VALUE;
    if (!g_logReady.load(std::memory_order_relaxed) && !ownsResources)
    {
        ReleaseSRWLockExclusive(&g_logLock);
        return;
    }

    // Stop accepting new normal log lines before the writer drains the queue.
    // Crash handlers use separate files and remain independent of this state.
    g_logReady.store(false, std::memory_order_release);
    g_stopWriter.store(true, std::memory_order_release);
    if (g_writeEvent)
        SetEvent(g_writeEvent);
    threadToJoin = g_writerThread;
    g_writerThread = nullptr;
    ReleaseSRWLockExclusive(&g_logLock);

    if (threadToJoin)
    {
        DWORD wr = WaitForSingleObject(threadToJoin, 3000);
        if (wr != WAIT_OBJECT_0)
        {
            // Final process-exit fallback. A blocked filesystem write must not
            // keep the watchdog, analogue host, or HallJoy process alive forever.
            TerminateThread(threadToJoin, 0xE0514C47u);
            WaitForSingleObject(threadToJoin, 1000);
        }
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
#if HALLJOY_LOG_DISABLED
    (void)fmt;
    return;
#endif

    if (!fmt || !*fmt) return;
    if (!g_logReady.load(std::memory_order_acquire)) return;

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

    // Normal diagnostic logging is always asynchronous. Fatal handlers write
    // their own dedicated crash reports synchronously, so the realtime path
    // never has to wait for WriteFile or FlushFileBuffers.
    AcquireSRWLockExclusive(&g_logLock);
    if (g_logReady.load(std::memory_order_relaxed))
    {
        g_pendingLines.emplace_back(line);
        if (g_writeEvent)
            SetEvent(g_writeEvent);
    }
    ReleaseSRWLockExclusive(&g_logLock);
}

void DebugLog_WriteBuffered(const wchar_t* fmt, ...)
{
#if HALLJOY_LOG_DISABLED
    (void)fmt;
    return;
#endif

    if (!fmt || !*fmt) return;
    if (!g_logReady.load(std::memory_order_acquire)) return;

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
#if HALLJOY_LOG_DISABLED
    return L"";
#endif

    AcquireSRWLockShared(&g_logLock);
    const wchar_t* p = g_logPath.c_str();
    ReleaseSRWLockShared(&g_logLock);
    return p;
}

bool DebugLog_IsDiagnosticBuild()
{
#if defined(HALLJOY_DIAGNOSTIC)
    return true;
#else
    return false;
#endif
}

void DebugLog_SetCheckpoint(const wchar_t* fmt, ...)
{
#if !defined(HALLJOY_DIAGNOSTIC)
    (void)fmt;
    return;
#else
    if (!fmt || !*fmt) return;

    va_list ap;
    va_start(ap, fmt);
    _vsnwprintf_s(g_lastCheckpoint, _countof(g_lastCheckpoint), _TRUNCATE, fmt, ap);
    va_end(ap);
#endif
}

static std::wstring ModuleNameAndOffset(const void* address)
{
    HMODULE module = nullptr;
    if (!address ||
        !GetModuleHandleExW(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            (LPCWSTR)address,
            &module))
    {
        return L"<unknown>";
    }

    wchar_t path[32768]{};
    DWORD len = GetModuleFileNameW(module, path, (DWORD)_countof(path));
    const wchar_t* name = path;
    if (len > 0 && len < _countof(path))
    {
        const wchar_t* slash = wcsrchr(path, L'\\');
        if (slash) name = slash + 1;
    }

    wchar_t text[512]{};
    uintptr_t offset = (uintptr_t)address - (uintptr_t)module;
    _snwprintf_s(text, _countof(text), _TRUNCATE, L"%s+0x%llX", name, (unsigned long long)offset);
    return text;
}

static bool IsFatalDiagnosticException(DWORD code)
{
    switch (code)
    {
    case EXCEPTION_ACCESS_VIOLATION:
    case EXCEPTION_IN_PAGE_ERROR:
    case EXCEPTION_ILLEGAL_INSTRUCTION:
    case EXCEPTION_STACK_OVERFLOW:
    case 0xC0000374u: // heap corruption
    case 0xC0000409u: // fail-fast / stack buffer overrun
        return true;
    default:
        return false;
    }
}


static void WriteDiagnosticContext(HANDLE file, EXCEPTION_POINTERS* ep)
{
    if (file == INVALID_HANDLE_VALUE || !ep || !ep->ContextRecord)
        return;

    wchar_t line[1200]{};
#if defined(_M_X64) || defined(__x86_64__)
    const CONTEXT& c = *ep->ContextRecord;
    _snwprintf_s(line, _countof(line), _TRUNCATE,
        L"RIP=%s RSP=0x%016llX RBP=0x%016llX EFLAGS=0x%08lX",
        ModuleNameAndOffset(reinterpret_cast<const void*>(static_cast<uintptr_t>(c.Rip))).c_str(),
        (unsigned long long)c.Rsp, (unsigned long long)c.Rbp, c.EFlags);
    WriteUtf8Line(file, line);
    _snwprintf_s(line, _countof(line), _TRUNCATE,
        L"RAX=%016llX RBX=%016llX RCX=%016llX RDX=%016llX RSI=%016llX RDI=%016llX",
        c.Rax, c.Rbx, c.Rcx, c.Rdx, c.Rsi, c.Rdi);
    WriteUtf8Line(file, line);
    _snwprintf_s(line, _countof(line), _TRUNCATE,
        L"R8=%016llX R9=%016llX R10=%016llX R11=%016llX R12=%016llX R13=%016llX R14=%016llX R15=%016llX",
        c.R8, c.R9, c.R10, c.R11, c.R12, c.R13, c.R14, c.R15);
    WriteUtf8Line(file, line);

    uintptr_t words[48]{};
    SIZE_T bytesRead = 0;
    if (ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<const void*>(static_cast<uintptr_t>(c.Rsp)),
        words, sizeof(words), &bytesRead))
    {
        WriteUtf8Line(file, L"stack_qwords:");
        const size_t count = std::min<size_t>(_countof(words), bytesRead / sizeof(words[0]));
        for (size_t i = 0; i < count; ++i)
        {
            _snwprintf_s(line, _countof(line), _TRUNCATE,
                L"  rsp+0x%03llX = 0x%016llX  %s",
                (unsigned long long)(i * sizeof(uintptr_t)),
                (unsigned long long)words[i],
                ModuleNameAndOffset(reinterpret_cast<const void*>(words[i])).c_str());
            WriteUtf8Line(file, line);
        }
    }
#else
    WriteUtf8Line(file, L"register_capture=unsupported_non_x64_build");
#endif
}

static LONG CALLBACK DiagnosticVectoredExceptionHandler(EXCEPTION_POINTERS* ep)
{
#if !defined(HALLJOY_DIAGNOSTIC)
    (void)ep;
    return EXCEPTION_CONTINUE_SEARCH;
#else
    const DWORD code = (ep && ep->ExceptionRecord) ? ep->ExceptionRecord->ExceptionCode : 0;
    if (!IsFatalDiagnosticException(code))
        return EXCEPTION_CONTINUE_SEARCH;

    bool expected = false;
    if (!g_vectoredFatalReportWritten.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
        return EXCEPTION_CONTINUE_SEARCH;

    const void* address = (ep && ep->ExceptionRecord) ? ep->ExceptionRecord->ExceptionAddress : nullptr;
    const std::wstring moduleAndOffset = ModuleNameAndOffset(address);
    const std::wstring reportPath = BuildPathNearExe(L"HallJoyDiagnosticVectored.txt");
    HANDLE file = CreateFileW(
        reportPath.c_str(), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH, nullptr);
    if (file != INVALID_HANDLE_VALUE)
    {
        WriteUtf8Line(file, L"HallJoy diagnostic vectored exception report");
        WriteUtf8Line(file, L"stage=first_chance_before_unhandled_filter");

        SYSTEMTIME st{};
        GetLocalTime(&st);
        wchar_t line[1024]{};
        _snwprintf_s(line, _countof(line), _TRUNCATE,
            L"time=%04u-%02u-%02u %02u:%02u:%02u.%03u",
            st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
        WriteUtf8Line(file, line);
        _snwprintf_s(line, _countof(line), _TRUNCATE, L"exception=0x%08lX", code);
        WriteUtf8Line(file, line);
        _snwprintf_s(line, _countof(line), _TRUNCATE, L"fault=%s", moduleAndOffset.c_str());
        WriteUtf8Line(file, line);
        _snwprintf_s(line, _countof(line), _TRUNCATE, L"thread=%lu", GetCurrentThreadId());
        WriteUtf8Line(file, line);
        _snwprintf_s(line, _countof(line), _TRUNCATE, L"checkpoint=%s", g_lastCheckpoint);
        WriteUtf8Line(file, line);
        WriteDiagnosticContext(file, ep);

        if (ep && ep->ExceptionRecord &&
            (code == EXCEPTION_ACCESS_VIOLATION || code == EXCEPTION_IN_PAGE_ERROR) &&
            ep->ExceptionRecord->NumberParameters >= 2)
        {
            const ULONG_PTR operation = ep->ExceptionRecord->ExceptionInformation[0];
            const ULONG_PTR target = ep->ExceptionRecord->ExceptionInformation[1];
            const wchar_t* operationName = operation == 0 ? L"read" : (operation == 1 ? L"write" : (operation == 8 ? L"execute" : L"unknown"));
            _snwprintf_s(line, _countof(line), _TRUNCATE, L"memory_operation=%s", operationName);
            WriteUtf8Line(file, line);
            _snwprintf_s(line, _countof(line), _TRUNCATE, L"memory_address=0x%llX", (unsigned long long)target);
            WriteUtf8Line(file, line);
        }

        WriteUtf8Line(file, L"log=HallJoyDiagnostic.log");
        WriteUtf8Line(file, L"note=this report is written at first chance; SetUnhandledExceptionFilter is not guaranteed to run after stack corruption, fail-fast, filter replacement, or direct process termination");
        WriteUtf8Line(file, L"privacy=no minidump; no username, command line, hardware inventory, or absolute user path is intentionally collected");
        FlushFileBuffers(file);
        CloseHandle(file);
    }
    return EXCEPTION_CONTINUE_SEARCH;
#endif
}

static LONG WINAPI DiagnosticUnhandledExceptionFilter(EXCEPTION_POINTERS* ep)
{
#if !defined(HALLJOY_DIAGNOSTIC)
    (void)ep;
    return EXCEPTION_CONTINUE_SEARCH;
#else
    const DWORD code = (ep && ep->ExceptionRecord) ? ep->ExceptionRecord->ExceptionCode : 0;
    const void* address = (ep && ep->ExceptionRecord) ? ep->ExceptionRecord->ExceptionAddress : nullptr;
    const std::wstring moduleAndOffset = ModuleNameAndOffset(address);
    const std::wstring crashPath = BuildPathNearExe(L"HallJoyDiagnosticCrash.txt");

    HANDLE file = CreateFileW(
        crashPath.c_str(),
        GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH,
        nullptr);
    if (file != INVALID_HANDLE_VALUE)
    {
        WriteUtf8Line(file, L"HallJoy diagnostic crash report");

        SYSTEMTIME st{};
        GetLocalTime(&st);
        wchar_t line[1024]{};
        _snwprintf_s(
            line, _countof(line), _TRUNCATE,
            L"time=%04u-%02u-%02u %02u:%02u:%02u.%03u",
            st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
        WriteUtf8Line(file, line);

        _snwprintf_s(line, _countof(line), _TRUNCATE, L"exception=0x%08lX", code);
        WriteUtf8Line(file, line);
        _snwprintf_s(line, _countof(line), _TRUNCATE, L"fault=%s", moduleAndOffset.c_str());
        WriteUtf8Line(file, line);
        _snwprintf_s(line, _countof(line), _TRUNCATE, L"thread=%lu", GetCurrentThreadId());
        WriteUtf8Line(file, line);
        _snwprintf_s(line, _countof(line), _TRUNCATE, L"checkpoint=%s", g_lastCheckpoint);
        WriteUtf8Line(file, line);
        WriteDiagnosticContext(file, ep);
        WriteUtf8Line(file, L"source=SetUnhandledExceptionFilter");
        WriteUtf8Line(file, L"log=HallJoyDiagnostic.log");
        WriteUtf8Line(file, L"privacy=no minidump; no username, hardware inventory, or absolute user path is intentionally collected");
        FlushFileBuffers(file);
        CloseHandle(file);
    }
    return EXCEPTION_EXECUTE_HANDLER;
#endif
}

void DebugLog_InstallCrashHandler()
{
#if defined(HALLJOY_DIAGNOSTIC)
    if (!g_vectoredExceptionHandler)
        g_vectoredExceptionHandler = AddVectoredExceptionHandler(1, DiagnosticVectoredExceptionHandler);
    LPTOP_LEVEL_EXCEPTION_FILTER previous = SetUnhandledExceptionFilter(DiagnosticUnhandledExceptionFilter);
    DebugLog_Write(L"[diagnostic] crash handlers installed vectored=%p previous_uef=%p current_uef=%p; support log is privacy-sanitized",
        g_vectoredExceptionHandler, previous, DiagnosticUnhandledExceptionFilter);
#endif
}


static void WriteSyntheticCrashReportIfMissing(DWORD watchedPid, DWORD exitCode, bool normalExit)
{
#if defined(HALLJOY_DIAGNOSTIC)
    if (normalExit || exitCode == 0)
        return;
    const std::wstring crashPath = BuildPathNearExe(L"HallJoyDiagnosticCrash.txt");
    if (GetFileAttributesW(crashPath.c_str()) != INVALID_FILE_ATTRIBUTES)
        return;

    HANDLE file = CreateFileW(crashPath.c_str(), GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, CREATE_NEW,
        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH, nullptr);
    if (file == INVALID_HANDLE_VALUE)
        return;

    WriteUtf8Line(file, L"HallJoy diagnostic crash report");
    WriteUtf8Line(file, L"source=exit_watchdog_synthetic");
    WriteUtf8Line(file, L"reason=the in-process unhandled-exception filter did not run or could not complete");
    SYSTEMTIME st{};
    GetLocalTime(&st);
    wchar_t line[768]{};
    _snwprintf_s(line, _countof(line), _TRUNCATE,
        L"time=%04u-%02u-%02u %02u:%02u:%02u.%03u",
        st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
    WriteUtf8Line(file, line);
    _snwprintf_s(line, _countof(line), _TRUNCATE, L"watched_pid=%lu", watchedPid);
    WriteUtf8Line(file, line);
    _snwprintf_s(line, _countof(line), _TRUNCATE, L"exit_code=0x%08lX", exitCode);
    WriteUtf8Line(file, line);
    _snwprintf_s(line, _countof(line), _TRUNCATE, L"exit_hint=%s", ExitCodeHint(exitCode));
    WriteUtf8Line(file, line);
    const bool vectoredExists = GetFileAttributesW(BuildPathNearExe(L"HallJoyDiagnosticVectored.txt").c_str()) != INVALID_FILE_ATTRIBUTES;
    _snwprintf_s(line, _countof(line), _TRUNCATE, L"vectored_report_present=%d", vectoredExists ? 1 : 0);
    WriteUtf8Line(file, line);
    WriteUtf8Line(file, L"details=see HallJoyDiagnosticVectored.txt for first-chance registers/stack and HallJoyDiagnosticExit.txt for final exit state");
    WriteUtf8Line(file, L"privacy=no username, command line, hardware inventory, or absolute user path is intentionally collected");
    FlushFileBuffers(file);
    CloseHandle(file);
#endif
}

bool DebugLog_TryRunExitWatchdogCommand()
{
#if !defined(HALLJOY_DIAGNOSTIC) && !defined(HALLJOY_MAD68PR_NATIVE)
    return false;
#else
    DWORD watchedPid = 0;
    if (!ParseDiagnosticWatchPid(watchedPid))
        return false;

    const std::wstring readyName = WatchEventName(L"WatchReady", watchedPid);
    const std::wstring normalName = WatchEventName(L"NormalExit", watchedPid);
    HANDLE readyEvent = OpenEventW(EVENT_MODIFY_STATE, FALSE, readyName.c_str());
    HANDLE normalEvent = OpenEventW(SYNCHRONIZE, FALSE, normalName.c_str());
    HANDLE process = OpenProcess(SYNCHRONIZE | PROCESS_QUERY_LIMITED_INFORMATION, FALSE, watchedPid);

    if (readyEvent)
    {
        SetEvent(readyEvent);
        CloseHandle(readyEvent);
    }

    DWORD waitResult = WAIT_FAILED;
    DWORD exitCode = 0xFFFFFFFFu;
    DWORD openError = ERROR_SUCCESS;
    if (process)
    {
        waitResult = WaitForSingleObject(process, INFINITE);
        if (!GetExitCodeProcess(process, &exitCode))
            exitCode = 0xFFFFFFFFu;
        CloseHandle(process);
    }
    else
    {
        openError = GetLastError();
    }

    const bool normalExit = normalEvent && WaitForSingleObject(normalEvent, 0) == WAIT_OBJECT_0;
    if (normalEvent)
        CloseHandle(normalEvent);

    bool mad68EmergencyRestoreAttempted = false;
    bool mad68EmergencyRestoreSent = false;
#if defined(HALLJOY_MAD68PR_NATIVE)
    // The watchdog is a separate process and reaches this point only after the
    // HallJoy process has terminated. It can therefore close the narrow crash
    // window between A8 and the parent process's normal A9 cleanup without
    // racing the live backend. A9 is idempotent and the backend hard-blocks all
    // other firmware opcodes.
    mad68EmergencyRestoreAttempted = true;
    mad68EmergencyRestoreSent = Mad68ProR_EmergencyRestoreInputOnce();
#endif

#if defined(HALLJOY_DIAGNOSTIC)
    const std::wstring path = BuildPathNearExe(L"HallJoyDiagnosticExit.txt");
    HANDLE file = CreateFileW(
        path.c_str(),
        GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH,
        nullptr);
    if (file != INVALID_HANDLE_VALUE)
    {
        WriteUtf8Line(file, L"HallJoy diagnostic process exit report");

        SYSTEMTIME st{};
        GetLocalTime(&st);
        wchar_t line[512]{};
        _snwprintf_s(
            line, _countof(line), _TRUNCATE,
            L"time=%04u-%02u-%02u %02u:%02u:%02u.%03u",
            st.wYear, st.wMonth, st.wDay,
            st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
        WriteUtf8Line(file, line);

        _snwprintf_s(line, _countof(line), _TRUNCATE, L"watched_pid=%lu", watchedPid);
        WriteUtf8Line(file, line);
        _snwprintf_s(line, _countof(line), _TRUNCATE, L"open_error=%lu", openError);
        WriteUtf8Line(file, line);
        _snwprintf_s(line, _countof(line), _TRUNCATE, L"wait_result=0x%08lX", waitResult);
        WriteUtf8Line(file, line);
        _snwprintf_s(line, _countof(line), _TRUNCATE, L"exit_code=0x%08lX", exitCode);
        WriteUtf8Line(file, line);
        _snwprintf_s(line, _countof(line), _TRUNCATE, L"exit_hint=%s", ExitCodeHint(exitCode));
        WriteUtf8Line(file, line);
        _snwprintf_s(line, _countof(line), _TRUNCATE, L"normal_shutdown_marker=%d", normalExit ? 1 : 0);
        WriteUtf8Line(file, line);
        _snwprintf_s(line, _countof(line), _TRUNCATE,
            L"mad68pr_emergency_A9_attempted=%d", mad68EmergencyRestoreAttempted ? 1 : 0);
        WriteUtf8Line(file, line);
        _snwprintf_s(line, _countof(line), _TRUNCATE,
            L"mad68pr_emergency_A9_write_sent=%d", mad68EmergencyRestoreSent ? 1 : 0);
        WriteUtf8Line(file, line);
        WriteUtf8Line(file, L"privacy=no command line, username, absolute user path, or memory dump is collected");
        FlushFileBuffers(file);
        CloseHandle(file);
    }

    WriteSyntheticCrashReportIfMissing(watchedPid, exitCode, normalExit);
#else
    // Production watchdog is intentionally silent. Its only responsibility is
    // the narrow, idempotent A9 recovery after the parent process exits.
    (void)openError;
    (void)waitResult;
    (void)exitCode;
    (void)normalExit;
    (void)mad68EmergencyRestoreAttempted;
    (void)mad68EmergencyRestoreSent;
#endif
    return true;
#endif
}

void DebugLog_StartExitWatchdog()
{
#if defined(HALLJOY_DIAGNOSTIC) || defined(HALLJOY_MAD68PR_NATIVE)
    bool expected = false;
    if (!g_watchdogStarted.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
        return;

    const DWORD pid = GetCurrentProcessId();
    const std::wstring readyName = WatchEventName(L"WatchReady", pid);
    const std::wstring normalName = WatchEventName(L"NormalExit", pid);
    HANDLE readyEvent = CreateEventW(nullptr, TRUE, FALSE, readyName.c_str());
    g_watchdogNormalExitEvent = CreateEventW(nullptr, TRUE, FALSE, normalName.c_str());

    wchar_t exePath[32768]{};
    DWORD exeLen = GetModuleFileNameW(nullptr, exePath, (DWORD)_countof(exePath));
    if (exeLen == 0 || exeLen >= _countof(exePath))
    {
        DebugLog_Write(L"[diagnostic.watchdog] exe path failed err=%lu", GetLastError());
        if (readyEvent) CloseHandle(readyEvent);
        return;
    }

    wchar_t commandLine[33080]{};
    _snwprintf_s(
        commandLine, _countof(commandLine), _TRUNCATE,
        L"\"%s\" --diagnostic-watch=%lu",
        exePath,
        pid);

    STARTUPINFOW si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};
    BOOL created = CreateProcessW(
        exePath,
        commandLine,
        nullptr,
        nullptr,
        FALSE,
        CREATE_NO_WINDOW | CREATE_NEW_PROCESS_GROUP,
        nullptr,
        nullptr,
        &si,
        &pi);
    DWORD createError = created ? ERROR_SUCCESS : GetLastError();
    if (created)
    {
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
    }

    DWORD readyWait = WAIT_FAILED;
    if (created && readyEvent)
        readyWait = WaitForSingleObject(readyEvent, 2000);
    if (readyEvent)
        CloseHandle(readyEvent);

    DebugLog_Write(
        L"[diagnostic.watchdog] start created=%d err=%lu ready=0x%08lX pid=%lu",
        created ? 1 : 0,
        createError,
        readyWait,
        pid);
#endif
}

static bool ContainsInsensitive(const wchar_t* text, const wchar_t* needle)
{
    if (!text || !needle || !*needle) return false;
    const size_t textLen = wcslen(text);
    const size_t needleLen = wcslen(needle);
    if (needleLen > textLen) return false;
    for (size_t i = 0; i + needleLen <= textLen; ++i)
    {
        if (_wcsnicmp(text + i, needle, needleLen) == 0)
            return true;
    }
    return false;
}

void DebugLog_LogAnalogModules()
{
#if defined(HALLJOY_DIAGNOSTIC)
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, GetCurrentProcessId());
    if (snapshot == INVALID_HANDLE_VALUE)
    {
        DebugLog_Write(L"[diagnostic.modules] snapshot failed err=%lu", GetLastError());
        return;
    }

    MODULEENTRY32W entry{};
    entry.dwSize = sizeof(entry);
    int matched = 0;
    if (Module32FirstW(snapshot, &entry))
    {
        do
        {
            const wchar_t* name = entry.szModule;
            if (!ContainsInsensitive(name, L"wooting") &&
                !ContainsInsensitive(name, L"analog") &&
                !ContainsInsensitive(name, L"madlion") &&
                !ContainsInsensitive(name, L"plugin") &&
                !ContainsInsensitive(name, L"abiv"))
            {
                continue;
            }

            LARGE_INTEGER fileSize{};
            FILETIME writeTime{};
            HANDLE file = CreateFileW(
                entry.szExePath,
                FILE_READ_ATTRIBUTES,
                FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                nullptr,
                OPEN_EXISTING,
                FILE_ATTRIBUTE_NORMAL,
                nullptr);
            if (file != INVALID_HANDLE_VALUE)
            {
                GetFileSizeEx(file, &fileSize);
                GetFileTime(file, nullptr, nullptr, &writeTime);
                CloseHandle(file);
            }

            SYSTEMTIME utc{};
            FileTimeToSystemTime(&writeTime, &utc);
            DebugLog_Write(
                L"[diagnostic.module] name=%s base=%p image_bytes=%lu file_bytes=%llu write_utc=%04u-%02u-%02uT%02u:%02u:%02uZ",
                name,
                entry.modBaseAddr,
                entry.modBaseSize,
                (unsigned long long)fileSize.QuadPart,
                utc.wYear, utc.wMonth, utc.wDay,
                utc.wHour, utc.wMinute, utc.wSecond);
            ++matched;
        } while (Module32NextW(snapshot, &entry));
    }
    DWORD enumError = GetLastError();
    CloseHandle(snapshot);
    DebugLog_Write(L"[diagnostic.modules] matched=%d enum_end=%lu", matched, enumError);
#endif
}


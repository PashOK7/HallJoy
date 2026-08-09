#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <atomic>
#include <cstddef>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cwchar>

#define HALLJOY_STABILITY_TRACE_IMPLEMENTATION 1
#include "stability_trace.h"

namespace
{
#if defined(HALLJOY_STABILITY_TRACE)
constexpr std::uint64_t kTraceSchema = 1;
#if defined(HALLJOY_AULA_AGGRESSIVE_TRACE)
constexpr std::uint64_t kMaxTraceBytes = 64u * 1024u * 1024u;
#else
constexpr std::uint64_t kMaxTraceBytes = 1024u * 1024u;
#endif
constexpr std::uint64_t kTraceCapReserveBytes = 256u;
constexpr wchar_t kTraceStage[] = L"S02V1";

SRWLOCK g_traceLock = SRWLOCK_INIT;
HANDLE g_traceFile = INVALID_HANDLE_VALUE;
HANDLE g_traceMapping = nullptr;
unsigned char* g_traceView = nullptr;
std::atomic<bool> g_traceEnabled{ false };
std::atomic<std::uint64_t> g_traceSequence{ 0 };
std::uint64_t g_traceBytes = 0;
ULONGLONG g_traceStartMs = 0;
bool g_traceCapped = false;
wchar_t g_tracePath[32768]{};

bool BuildPathNearExe(const wchar_t* fileName, wchar_t* out, std::size_t outCount) noexcept
{
    if (!fileName || !out || outCount < 8)
        return false;

    DWORD length = GetModuleFileNameW(nullptr, out, static_cast<DWORD>(outCount));
    if (length == 0 || length >= outCount)
        return false;

    wchar_t* slash = wcsrchr(out, L'\\');
    wchar_t* slash2 = wcsrchr(out, L'/');
    if (!slash || (slash2 && slash2 > slash))
        slash = slash2;
    if (slash)
        slash[1] = L'\0';
    else
        out[0] = L'\0';

    return wcscat_s(out, outCount, fileName) == 0;
}

void SanitizeText(wchar_t* value) noexcept
{
    if (!value)
        return;
    for (wchar_t* p = value; *p; ++p)
    {
        if (*p == L'\r' || *p == L'\n' || *p == L'\t')
            *p = L' ';
    }
}

bool AppendBytesLocked(const char* bytes, std::size_t length) noexcept
{
    if (!bytes || length == 0 || !g_traceView)
        return false;
    if (g_traceBytes + length > kMaxTraceBytes)
        return false;
    std::memcpy(g_traceView + g_traceBytes, bytes, length);
    g_traceBytes += length;
    return true;
}

bool WriteRawLocked(const wchar_t* line) noexcept
{
    if (!line || !g_traceView)
        return false;

    const int wideLength = static_cast<int>(wcslen(line));
    if (wideLength <= 0)
        return true;

    char utf8[4096]{};
    int utf8Length = WideCharToMultiByte(
        CP_UTF8, 0, line, wideLength, utf8, static_cast<int>(sizeof(utf8) - 3), nullptr, nullptr);
    if (utf8Length <= 0)
        return false;

    utf8[utf8Length++] = '\r';
    utf8[utf8Length++] = '\n';

    if (g_traceBytes + static_cast<std::uint64_t>(utf8Length) >
        kMaxTraceBytes - kTraceCapReserveBytes)
    {
        if (!g_traceCapped)
        {
            const std::uint64_t cappedSequence =
                g_traceSequence.fetch_add(1, std::memory_order_relaxed) + 1;
            char capped[224]{};
            const int cappedLength = _snprintf_s(
                capped, sizeof(capped), _TRUNCATE,
                "[seq=%llu][level=ERROR][component=trace][event=trace.capped] "
                "max_bytes=%llu\r\n",
                static_cast<unsigned long long>(cappedSequence),
                static_cast<unsigned long long>(kMaxTraceBytes));
            if (cappedLength > 0)
                AppendBytesLocked(capped, static_cast<std::size_t>(cappedLength));
            g_traceCapped = true;
        }
        g_traceEnabled.store(false, std::memory_order_release);
        return false;
    }

    return AppendBytesLocked(utf8, static_cast<std::size_t>(utf8Length));
}

void WriteFormatted(const wchar_t* level, const wchar_t* component,
    const wchar_t* event, const wchar_t* fieldsFormat, va_list args) noexcept
{
    if (!g_traceEnabled.load(std::memory_order_acquire) || !level || !component || !event)
        return;

    wchar_t fields[1536]{};
    if (fieldsFormat && *fieldsFormat)
        _vsnwprintf_s(fields, _countof(fields), _TRUNCATE, fieldsFormat, args);
    SanitizeText(fields);

    SYSTEMTIME now{};
    GetLocalTime(&now);
    const ULONGLONG elapsed = GetTickCount64() - g_traceStartMs;
    const DWORD processId = GetCurrentProcessId();
    const DWORD threadId = GetCurrentThreadId();

    AcquireSRWLockExclusive(&g_traceLock);
    if (g_traceEnabled.load(std::memory_order_relaxed))
    {
        // The sequence is assigned under the same lock as the append. This
        // guarantees that file order and sequence order cannot diverge when
        // several worker boundaries emit events concurrently.
        const std::uint64_t sequence =
            g_traceSequence.fetch_add(1, std::memory_order_relaxed) + 1;
        wchar_t line[2304]{};
        _snwprintf_s(
            line, _countof(line), _TRUNCATE,
            L"[%04u-%02u-%02uT%02u:%02u:%02u.%03u]"
            L"[elapsed_ms=%llu][seq=%llu][pid=%lu][tid=%lu]"
            L"[level=%s][component=%s][event=%s]%s%s",
            now.wYear, now.wMonth, now.wDay,
            now.wHour, now.wMinute, now.wSecond, now.wMilliseconds,
            static_cast<unsigned long long>(elapsed),
            static_cast<unsigned long long>(sequence),
            processId, threadId,
            level, component, event,
            fields[0] ? L" " : L"", fields);
        WriteRawLocked(line);
    }
    ReleaseSRWLockExclusive(&g_traceLock);
}
#endif
}

void StabilityTrace_Init() noexcept
{
#if !defined(HALLJOY_STABILITY_TRACE)
    return;
#else
    AcquireSRWLockExclusive(&g_traceLock);
    if (g_traceFile != INVALID_HANDLE_VALUE || g_traceMapping || g_traceView)
    {
        ReleaseSRWLockExclusive(&g_traceLock);
        return;
    }

#if defined(HALLJOY_AULA_AGGRESSIVE_TRACE)
    if (!BuildPathNearExe(L"HallJoy.log", g_tracePath, _countof(g_tracePath)))
    {
        ReleaseSRWLockExclusive(&g_traceLock);
        return;
    }
#else
    wchar_t previousPath[32768]{};
    if (!BuildPathNearExe(L"HallJoyStabilityTrace.log", g_tracePath, _countof(g_tracePath)) ||
        !BuildPathNearExe(L"HallJoyStabilityTrace.previous.log", previousPath, _countof(previousPath)))
    {
        ReleaseSRWLockExclusive(&g_traceLock);
        return;
    }

    DeleteFileW(previousPath);
    MoveFileExW(g_tracePath, previousPath, MOVEFILE_REPLACE_EXISTING);
#endif
    g_traceFile = CreateFileW(
        g_tracePath,
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);
    if (g_traceFile != INVALID_HANDLE_VALUE)
    {
        g_traceMapping = CreateFileMappingW(
            g_traceFile, nullptr, PAGE_READWRITE,
            0, static_cast<DWORD>(kMaxTraceBytes), nullptr);
        if (g_traceMapping)
            g_traceView = static_cast<unsigned char*>(MapViewOfFile(
                g_traceMapping, FILE_MAP_WRITE, 0, 0, static_cast<SIZE_T>(kMaxTraceBytes)));
    }

    if (g_traceFile == INVALID_HANDLE_VALUE || !g_traceMapping || !g_traceView)
    {
        if (g_traceView) UnmapViewOfFile(g_traceView);
        if (g_traceMapping) CloseHandle(g_traceMapping);
        if (g_traceFile != INVALID_HANDLE_VALUE) CloseHandle(g_traceFile);
        g_traceView = nullptr;
        g_traceMapping = nullptr;
        g_traceFile = INVALID_HANDLE_VALUE;
        g_tracePath[0] = L'\0';
        ReleaseSRWLockExclusive(&g_traceLock);
        return;
    }

    static constexpr unsigned char bom[3] = { 0xEF, 0xBB, 0xBF };
    std::memcpy(g_traceView, bom, sizeof(bom));
    g_traceBytes = sizeof(bom);
    g_traceCapped = false;
    g_traceStartMs = GetTickCount64();
    g_traceSequence.store(0, std::memory_order_relaxed);
    g_traceEnabled.store(true, std::memory_order_release);
    ReleaseSRWLockExclusive(&g_traceLock);

    StabilityTrace_WriteCritical(
        L"INFO", L"main", L"session.start",
        L"schema=%llu stage=%s compiled_date=%S compiled_time=%S",
        static_cast<unsigned long long>(kTraceSchema), kTraceStage, __DATE__, __TIME__);
#endif
}

void StabilityTrace_Shutdown(int exitCode) noexcept
{
#if !defined(HALLJOY_STABILITY_TRACE)
    (void)exitCode;
    return;
#else
    if (g_traceEnabled.load(std::memory_order_acquire))
        StabilityTrace_WriteCritical(L"INFO", L"main", L"session.end", L"exit_code=%d", exitCode);

    AcquireSRWLockExclusive(&g_traceLock);
    g_traceEnabled.store(false, std::memory_order_release);
    unsigned char* view = g_traceView;
    HANDLE mapping = g_traceMapping;
    HANDLE file = g_traceFile;
    const std::uint64_t finalBytes = g_traceBytes;
    g_traceView = nullptr;
    g_traceMapping = nullptr;
    g_traceFile = INVALID_HANDLE_VALUE;

    if (view)
    {
        // Unmapping and closing the mapped file hands dirty pages to the cache
        // manager. Explicit FlushViewOfFile/FlushFileBuffers made a diagnostic
        // shutdown depend on storage/AV latency and has physically stalled past
        // the 12-second process watchdog even after session.end was appended.
        UnmapViewOfFile(view);
    }
    if (mapping)
        CloseHandle(mapping);
    if (file != INVALID_HANDLE_VALUE)
    {
        LARGE_INTEGER end{};
        end.QuadPart = static_cast<LONGLONG>(finalBytes);
        if (SetFilePointerEx(file, end, nullptr, FILE_BEGIN))
            SetEndOfFile(file);
        CloseHandle(file);
    }
    ReleaseSRWLockExclusive(&g_traceLock);
#endif
}

void StabilityTrace_Write(const wchar_t* level, const wchar_t* component,
    const wchar_t* event, const wchar_t* fieldsFormat, ...) noexcept
{
#if !defined(HALLJOY_STABILITY_TRACE)
    (void)level; (void)component; (void)event; (void)fieldsFormat;
#else
    va_list args;
    va_start(args, fieldsFormat);
    WriteFormatted(level, component, event, fieldsFormat, args);
    va_end(args);
#endif
}

void StabilityTrace_WriteCritical(const wchar_t* level, const wchar_t* component,
    const wchar_t* event, const wchar_t* fieldsFormat, ...) noexcept
{
#if !defined(HALLJOY_STABILITY_TRACE)
    (void)level; (void)component; (void)event; (void)fieldsFormat;
#else
    va_list args;
    va_start(args, fieldsFormat);
    WriteFormatted(level, component, event, fieldsFormat, args);
    va_end(args);
#endif
}

void StabilityTrace_AppendPlain(const wchar_t* line) noexcept
{
#if !defined(HALLJOY_STABILITY_TRACE)
    (void)line;
#else
    if (!line || !*line || !g_traceEnabled.load(std::memory_order_acquire))
        return;
    AcquireSRWLockExclusive(&g_traceLock);
    if (g_traceEnabled.load(std::memory_order_relaxed))
        WriteRawLocked(line);
    ReleaseSRWLockExclusive(&g_traceLock);
#endif
}

bool StabilityTrace_IsEnabled() noexcept
{
#if defined(HALLJOY_STABILITY_TRACE)
    return g_traceEnabled.load(std::memory_order_acquire);
#else
    return false;
#endif
}

const wchar_t* StabilityTrace_Path() noexcept
{
#if defined(HALLJOY_STABILITY_TRACE)
    return g_tracePath;
#else
    return L"";
#endif
}

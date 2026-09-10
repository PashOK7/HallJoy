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
#if defined(HALLJOY_DRUNKDEER_DIAGNOSTIC)
constexpr std::uint64_t kTraceSchema = 2;
#else
constexpr std::uint64_t kTraceSchema = 1;
#if defined(HALLJOY_AULA_AGGRESSIVE_TRACE) || defined(HALLJOY_IROK_ND75_DIAGNOSTIC)
constexpr std::uint64_t kMaxTraceBytes = 64u * 1024u * 1024u;
#else
constexpr std::uint64_t kMaxTraceBytes = 1024u * 1024u;
#endif
constexpr std::uint64_t kTraceCapReserveBytes = 256u;
#endif
constexpr wchar_t kTraceStage[] = L"S02V1";

SRWLOCK g_traceLock = SRWLOCK_INIT;
HANDLE g_traceFile = INVALID_HANDLE_VALUE;
HANDLE g_traceMapping = nullptr;
unsigned char* g_traceView = nullptr;
std::atomic<bool> g_traceEnabled{ false };
std::atomic<std::uint64_t> g_traceSequence{ 0 };
std::uint64_t g_traceBytes = 0;
std::uint64_t g_traceCapacityBytes = 0;
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
#if defined(HALLJOY_DIAGNOSTIC)
    struct PrivateTokens
    {
        wchar_t userProfile[32768]{};
        wchar_t applicationDirectory[32768]{};
        wchar_t temporaryDirectory[32768]{};
    };
    static const PrivateTokens tokens = [] {
        PrivateTokens result{};
        GetEnvironmentVariableW(L"USERPROFILE", result.userProfile,
            static_cast<DWORD>(_countof(result.userProfile)));
        const DWORD applicationLength = GetModuleFileNameW(
            nullptr, result.applicationDirectory,
            static_cast<DWORD>(_countof(result.applicationDirectory)));
        if (applicationLength > 0 &&
            applicationLength < _countof(result.applicationDirectory))
        {
            wchar_t* slash = wcsrchr(result.applicationDirectory, L'\\');
            wchar_t* forwardSlash = wcsrchr(result.applicationDirectory, L'/');
            if (!slash || (forwardSlash && forwardSlash > slash))
                slash = forwardSlash;
            if (slash)
                slash[1] = L'\0';
            else
                result.applicationDirectory[0] = L'\0';
        }
        else
        {
            result.applicationDirectory[0] = L'\0';
        }
        const DWORD temporaryLength = GetTempPathW(
            static_cast<DWORD>(_countof(result.temporaryDirectory)),
            result.temporaryDirectory);
        if (temporaryLength == 0 ||
            temporaryLength >= _countof(result.temporaryDirectory))
            result.temporaryDirectory[0] = L'\0';
        return result;
    }();
    const auto redact = [value](const wchar_t* token) noexcept {
        const std::size_t length = token ? wcslen(token) : 0;
        if (length < 2) return;
        for (wchar_t* position = value; *position; ++position)
        {
            if (_wcsnicmp(position, token, length) != 0) continue;
            for (std::size_t index = 0; index < length; ++index)
                position[index] = L'*';
            position += length - 1;
        }
    };
    redact(tokens.userProfile);
    redact(tokens.applicationDirectory);
    redact(tokens.temporaryDirectory);

    // A SetupAPI/Raw Input interface path contains a stable per-machine device
    // instance. The exact 6x21 diagnostic records a one-way path hash where
    // correlation is required, so raw \\?\ / \\.\ paths are never needed in the
    // user-returned support file. Mask at the final sink so a future debug call
    // cannot accidentally bypass the privacy contract.
    for (wchar_t* position = value; *position; ++position)
    {
        if (position[0] != L'\\' || position[1] != L'\\' ||
            (position[2] != L'?' && position[2] != L'.') ||
            position[3] != L'\\')
            continue;
        wchar_t* end = position;
        while (*end && *end != L' ' && *end != L'"' &&
            *end != L']' && *end != L')')
        {
            *end = L'*';
            ++end;
        }
        if (!*end)
            break;
        position = end;
    }
#endif
}

bool RemapTraceLocked(std::uint64_t capacity) noexcept
{
    if (g_traceFile == INVALID_HANDLE_VALUE || capacity == 0 ||
        capacity > static_cast<std::uint64_t>(SIZE_MAX))
        return false;

    if (g_traceView)
    {
        UnmapViewOfFile(g_traceView);
        g_traceView = nullptr;
    }
    if (g_traceMapping)
    {
        CloseHandle(g_traceMapping);
        g_traceMapping = nullptr;
    }

    g_traceMapping = CreateFileMappingW(
        g_traceFile, nullptr, PAGE_READWRITE,
        static_cast<DWORD>(capacity >> 32),
        static_cast<DWORD>(capacity & 0xffffffffull), nullptr);
    if (!g_traceMapping) return false;
    g_traceView = static_cast<unsigned char*>(MapViewOfFile(
        g_traceMapping, FILE_MAP_WRITE, 0, 0, static_cast<SIZE_T>(capacity)));
    if (!g_traceView)
    {
        CloseHandle(g_traceMapping);
        g_traceMapping = nullptr;
        return false;
    }
    g_traceCapacityBytes = capacity;
    return true;
}

bool AppendBytesLocked(const char* bytes, std::size_t length) noexcept
{
    if (!bytes || length == 0)
        return false;
#if defined(HALLJOY_DRUNKDEER_DIAGNOSTIC)
    if (g_traceFile == INVALID_HANDLE_VALUE ||
        length > static_cast<std::size_t>(MAXDWORD) ||
        length > UINT64_MAX - g_traceBytes)
        return false;
    LARGE_INTEGER position{};
    position.QuadPart = static_cast<LONGLONG>(g_traceBytes);
    if (!SetFilePointerEx(g_traceFile, position, nullptr, FILE_BEGIN))
        return false;
    DWORD written = 0;
    if (!WriteFile(g_traceFile, bytes, static_cast<DWORD>(length),
            &written, nullptr) || written != length)
        return false;
    g_traceBytes += length;
    return true;
#else
    if (!g_traceView) return false;
    if (g_traceBytes + length > kMaxTraceBytes)
        return false;
    std::memcpy(g_traceView + g_traceBytes, bytes, length);
    g_traceBytes += length;
    return true;
#endif
}

bool WriteRawLocked(const wchar_t* line) noexcept
{
    if (!line
#if defined(HALLJOY_DRUNKDEER_DIAGNOSTIC)
        || g_traceFile == INVALID_HANDLE_VALUE
#else
        || !g_traceView
#endif
        )
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

#if !defined(HALLJOY_DRUNKDEER_DIAGNOSTIC)
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
#endif

    return AppendBytesLocked(utf8, static_cast<std::size_t>(utf8Length));
}

void WriteFormatted(const wchar_t* level, const wchar_t* component,
    const wchar_t* event, const wchar_t* fieldsFormat, va_list args,
    bool flush) noexcept
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
        const bool written = WriteRawLocked(line);
#if defined(HALLJOY_DRUNKDEER_DIAGNOSTIC)
        if (written && flush && g_traceFile != INVALID_HANDLE_VALUE)
            (void)FlushFileBuffers(g_traceFile);
#else
        (void)written;
        (void)flush;
#endif
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
#if defined(HALLJOY_DRUNKDEER_DIAGNOSTIC)
        // This diagnostic is an evidence recorder, not a high-rate production
        // trace. Direct append keeps the on-disk length authoritative after an
        // abnormal process exit and avoids a preallocated NUL tail.
#else
        (void)RemapTraceLocked(kMaxTraceBytes);
#endif
    }

    if (g_traceFile == INVALID_HANDLE_VALUE
#if !defined(HALLJOY_DRUNKDEER_DIAGNOSTIC)
        || !g_traceMapping || !g_traceView
#endif
        )
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
    g_traceBytes = 0;
    if (!AppendBytesLocked(reinterpret_cast<const char*>(bom), sizeof(bom)))
    {
        if (g_traceView) UnmapViewOfFile(g_traceView);
        if (g_traceMapping) CloseHandle(g_traceMapping);
        CloseHandle(g_traceFile);
        g_traceView = nullptr;
        g_traceMapping = nullptr;
        g_traceFile = INVALID_HANDLE_VALUE;
        g_tracePath[0] = L'\0';
        ReleaseSRWLockExclusive(&g_traceLock);
        return;
    }
    g_traceCapped = false;
    g_traceStartMs = GetTickCount64();
    g_traceSequence.store(0, std::memory_order_relaxed);
    g_traceEnabled.store(true, std::memory_order_release);
    ReleaseSRWLockExclusive(&g_traceLock);

    StabilityTrace_WriteCritical(
        L"INFO", L"main", L"session.start",
        L"schema=%llu stage=%s compiled_date=%S compiled_time=%S",
        static_cast<unsigned long long>(kTraceSchema), kTraceStage, __DATE__, __TIME__);
#if defined(HALLJOY_DRUNKDEER_DIAGNOSTIC)
    StabilityTrace_WriteCritical(
        L"INFO", L"trace", L"trace.growth_policy",
        L"hard_cap=0 storage=direct_append critical_flush=1 preallocation=0 volume_control=event_aggregation privacy_redaction=userprofile");
#endif
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
#if !defined(HALLJOY_DRUNKDEER_DIAGNOSTIC)
    const std::uint64_t finalBytes = g_traceBytes;
#endif
    g_traceView = nullptr;
    g_traceMapping = nullptr;
    g_traceFile = INVALID_HANDLE_VALUE;
    g_traceCapacityBytes = 0;

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
#if defined(HALLJOY_DRUNKDEER_DIAGNOSTIC)
        (void)FlushFileBuffers(file);
#else
        LARGE_INTEGER end{};
        end.QuadPart = static_cast<LONGLONG>(finalBytes);
        if (SetFilePointerEx(file, end, nullptr, FILE_BEGIN))
            SetEndOfFile(file);
#endif
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
    WriteFormatted(level, component, event, fieldsFormat, args, false);
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
    WriteFormatted(level, component, event, fieldsFormat, args, true);
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
#if defined(HALLJOY_DRUNKDEER_DIAGNOSTIC)
    {
        wchar_t safeLine[4096]{};
        wcsncpy_s(safeLine, line, _TRUNCATE);
        SanitizeText(safeLine);
        WriteRawLocked(safeLine);
    }
#else
        WriteRawLocked(line);
#endif
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

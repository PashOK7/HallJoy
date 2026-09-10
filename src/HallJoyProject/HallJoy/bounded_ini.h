#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
#include <algorithm>
#include <limits>

namespace halljoy::ini {
inline constexpr std::uint64_t kMaxFileBytes = 16u * 1024u * 1024u;
inline constexpr std::size_t kMaxLayoutKeys = 4096;
// Hold the file against replacement/writes while the Win32 INI reader stages it.
class ReadFile {
    HANDLE handle_ = INVALID_HANDLE_VALUE;
public:
    explicit ReadFile(const wchar_t* path) {
        if (!path || !*path) return;
        handle_ = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, nullptr,
            OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
        LARGE_INTEGER size{};
        BY_HANDLE_FILE_INFORMATION information{};
        if (handle_ != INVALID_HANDLE_VALUE &&
            (!GetFileInformationByHandle(handle_, &information) ||
             (information.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)) != 0 ||
             !GetFileSizeEx(handle_, &size) || size.QuadPart <= 0 ||
             static_cast<std::uint64_t>(size.QuadPart) > kMaxFileBytes)) {
            CloseHandle(handle_); handle_ = INVALID_HANDLE_VALUE;
        }
    }
    ~ReadFile() { if (handle_ != INVALID_HANDLE_VALUE) CloseHandle(handle_); }
    ReadFile(const ReadFile&) = delete;
    ReadFile& operator=(const ReadFile&) = delete;
    explicit operator bool() const { return handle_ != INVALID_HANDLE_VALUE; }
};
inline bool Read(const wchar_t* path, const wchar_t* section,
    const wchar_t* key, std::wstring& out, std::size_t maximum = 65536) {
    // maximum is a buffer capacity including the terminator. Keep a spare
    // character to distinguish a complete value from Win32's truncated result.
    if (maximum < 2 || maximum > (std::numeric_limits<DWORD>::max)()) return false;
    for (std::size_t capacity = std::min<std::size_t>(256, maximum);;) {
        std::vector<wchar_t> buffer(capacity);
        const DWORD n = GetPrivateProfileStringW(section, key, L"", buffer.data(),
            static_cast<DWORD>(capacity), path);
        if (n < capacity - 1) { out.assign(buffer.data(), n); return true; }
        if (capacity == maximum) break;
        capacity += (std::min)(capacity, maximum - capacity);
    }
    return false; // Never parse a truncated token.
}
inline bool Unsigned(std::wstring_view text, std::uint32_t maximum,
    std::uint32_t& out) {
    while (!text.empty() && (text.front() == L' ' || text.front() == L'\t')) text.remove_prefix(1);
    while (!text.empty() && (text.back() == L' ' || text.back() == L'\t')) text.remove_suffix(1);
    unsigned base = 10;
    if (text.size() > 2 && text[0] == L'0' && (text[1] == L'x' || text[1] == L'X')) {
        base = 16; text.remove_prefix(2);
    }
    if (text.empty()) return false;
    std::uint32_t value = 0;
    for (wchar_t c : text) {
        const unsigned d = c >= L'0' && c <= L'9' ? c - L'0' :
            c >= L'a' && c <= L'f' ? c - L'a' + 10 :
            c >= L'A' && c <= L'F' ? c - L'A' + 10 : 99;
        if (d >= base || d > maximum || value > (maximum - d) / base) return false;
        value = value * base + d;
    }
    out = value; return true;
}
inline bool Signed(std::wstring_view text, std::int32_t minimum,
    std::int32_t maximum, std::int32_t& out) {
    while (!text.empty() && (text.front() == L' ' || text.front() == L'\t')) text.remove_prefix(1);
    while (!text.empty() && (text.back() == L' ' || text.back() == L'\t')) text.remove_suffix(1);
    if (text.empty() || minimum > maximum) return false;
    bool negative = false;
    if (text.front() == L'-' || text.front() == L'+') {
        negative = text.front() == L'-';
        text.remove_prefix(1);
    }
    if (text.empty()) return false;
    std::uint64_t magnitude = 0;
    const std::uint64_t limit = negative
        ? (minimum < 0 ? static_cast<std::uint64_t>(-static_cast<std::int64_t>(minimum)) : 0)
        : (maximum > 0 ? static_cast<std::uint64_t>(maximum) : 0);
    for (wchar_t c : text) {
        if (c < L'0' || c > L'9') return false;
        const unsigned digit = static_cast<unsigned>(c - L'0');
        if (digit > limit || magnitude > (limit - digit) / 10u) return false;
        magnitude = magnitude * 10u + digit;
    }
    const std::int64_t value = negative
        ? -static_cast<std::int64_t>(magnitude) : static_cast<std::int64_t>(magnitude);
    if (value < minimum || value > maximum) return false;
    out = static_cast<std::int32_t>(value);
    return true;
}
inline bool ReadUnsigned(const wchar_t* path, const wchar_t* section,
    const wchar_t* key, std::uint32_t maximum, std::uint32_t defaultValue,
    std::uint32_t& out) {
    std::wstring text;
    if (!Read(path, section, key, text, 128)) return false;
    if (text.empty()) { out = defaultValue; return true; }
    return Unsigned(text, maximum, out);
}
inline bool ReadSigned(const wchar_t* path, const wchar_t* section,
    const wchar_t* key, std::int32_t minimum, std::int32_t maximum,
    std::int32_t defaultValue, std::int32_t& out) {
    std::wstring text;
    if (!Read(path, section, key, text, 128)) return false;
    if (text.empty()) { out = defaultValue; return true; }
    return Signed(text, minimum, maximum, out);
}
inline bool HasBundle(const wchar_t* path) {
    wchar_t marker[32]{};
    // Presence, including an unknown version, is authoritative: never fall back.
    return GetPrivateProfileStringW(L"HallJoyProfile", L"BundleVersion", L"",
        marker, 32, path) != 0;
}
}

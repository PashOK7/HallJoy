#pragma once
#include "bounded_ini.h"
#include <map>

namespace halljoy::layout_storage {
// Win32 retains legacy ANSI/UTF-16 parsing and case-insensitive INI semantics.
// One section read replaces hundreds of per-key profile API calls. The caller
// holds ini::ReadFile throughout acquisition to prevent a mixed file generation.
class Section {
    struct Less {
        bool operator()(const std::wstring& a, const std::wstring& b) const {
            return _wcsicmp(a.c_str(), b.c_str()) < 0;
        }
    };
    std::map<std::wstring, std::wstring, Less> values_;
public:
    bool Load(const wchar_t* path, const wchar_t* section) {
        values_.clear();
        constexpr size_t maximum = static_cast<size_t>(ini::kMaxFileBytes) + 2;
        std::vector<wchar_t> buffer(4096);
        for (;;) {
            const DWORD size = GetPrivateProfileSectionW(section, buffer.data(),
                static_cast<DWORD>(buffer.size()), path);
            if (size < buffer.size() - 2) break;
            if (buffer.size() == maximum) return false;
            buffer.resize((std::min)(buffer.size() * 2, maximum));
        }
        for (const wchar_t* entry = buffer.data(); *entry; entry += wcslen(entry) + 1) {
            const wchar_t* equals = wcschr(entry, L'=');
            if (!equals) continue;
            std::wstring key(entry, equals);
            while (!key.empty() && (key.back() == L' ' || key.back() == L'\t')) key.pop_back();
            std::wstring value(equals + 1);
            const auto first = value.find_first_not_of(L" \t");
            value = first == std::wstring::npos ? L"" : value.substr(first);
            const auto last = value.find_last_not_of(L" \t");
            if (last != std::wstring::npos) value.resize(last + 1);
            if (value.size() >= 2 && (value.front() == L'\"' || value.front() == L'\'') && value.back() == value.front())
                value = value.substr(1, value.size() - 2);
            values_.emplace(std::move(key), std::move(value)); // First duplicate wins, like Win32.
        }
        return true;
    }
    const std::wstring& Get(const wchar_t* key) const {
        static const std::wstring empty;
        const auto found = values_.find(key);
        return found == values_.end() ? empty : found->second;
    }
    bool Integer(const wchar_t* key, int minimum, int maximum, int fallback, int& out) const {
        const auto& text = Get(key);
        if (text.empty()) { out = fallback; return true; }
        std::int32_t value = 0;
        if (!ini::Signed(text, minimum, maximum, value)) return false;
        out = value;
        return true;
    }
};

// The transaction owns this already-created temporary file. Commit and durable
// flush still belong to IniUtil_SaveAtomic; no destination is written in place.
inline bool WriteUtf16(const wchar_t* path, const std::wstring& text, DWORD* error) {
    if ((text.size() + 1) * sizeof(wchar_t) > ini::kMaxFileBytes) {
        if (error) *error = ERROR_FILE_TOO_LARGE;
        return false;
    }
    const std::wstring encoded = std::wstring(1, static_cast<wchar_t>(0xFEFF)) + text;
    HANDLE file = CreateFileW(path, GENERIC_WRITE, 0, nullptr, OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
    if (file == INVALID_HANDLE_VALUE) { if (error) *error = GetLastError(); return false; }
    DWORD written = 0;
    const DWORD bytes = static_cast<DWORD>(encoded.size() * sizeof(wchar_t));
    const bool ok = WriteFile(file, encoded.data(), bytes, &written, nullptr) && written == bytes && SetEndOfFile(file);
    const DWORD failure = ok ? ERROR_SUCCESS : GetLastError();
    const bool closed = CloseHandle(file) != FALSE;
    if (error) *error = !ok ? (failure ? failure : ERROR_WRITE_FAULT) : closed ? ERROR_SUCCESS : GetLastError();
    return ok && closed;
}
}

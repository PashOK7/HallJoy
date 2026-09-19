#pragma once
#include "layout_ini_section.h"

namespace halljoy::ini {
// A transaction-local writer for newly generated documents. Existing-file
// updates use Win32 directly. Only the owning thread and exact temporary path
// can contribute; callers must Finish before validation/atomic replacement.
class WriteBatch {
    struct Less { bool operator()(const std::wstring& a, const std::wstring& b) const {
        return _wcsicmp(a.c_str(), b.c_str()) < 0;
    }};
    using Values = std::map<std::wstring, std::wstring, Less>;
    std::map<std::wstring, Values, Less> sections_;
    std::wstring path_;
    WriteBatch* previous_;
    inline static thread_local WriteBatch* current_ = nullptr;
public:
    explicit WriteBatch(const wchar_t* path) : path_(path), previous_(current_) { current_ = this; }
    ~WriteBatch() { current_ = previous_; }
    WriteBatch(const WriteBatch&) = delete;
    WriteBatch& operator=(const WriteBatch&) = delete;
    static BOOL Put(const wchar_t* section, const wchar_t* key, const wchar_t* value, const wchar_t* path) {
        if (!current_) return WritePrivateProfileStringW(section, key, value, path);
        if (!path || current_->path_ != path || !section || !*section) {
            SetLastError(ERROR_INVALID_PARAMETER); return FALSE;
        }
        auto& sections = current_->sections_;
        if (!key) { sections.erase(section); return TRUE; }
        if (!value) {
            auto found = sections.find(section);
            if (found != sections.end()) found->second.erase(key);
            return TRUE;
        }
        // Values are emitted as one quoted INI line, never extra sections.
        if (wcspbrk(section, L"\r\n[]") || wcspbrk(key, L"\r\n=") || wcspbrk(value, L"\r\n")) {
            SetLastError(ERROR_INVALID_DATA); return FALSE;
        }
        sections[section][key] = value;
        return TRUE;
    }
    bool Finish(DWORD* error) const {
        std::wstring document;
        for (const auto& section : sections_) {
            document += L"[" + section.first + L"]\r\n";
            for (const auto& value : section.second) {
                document += value.first + L"=\"" + value.second + L"\"\r\n";
                if (document.size() > kMaxFileBytes / sizeof(wchar_t)) {
                    if (error) *error = ERROR_FILE_TOO_LARGE;
                    return false;
                }
            }
            document += L"\r\n";
        }
        return layout_storage::WriteUtf16(path_.c_str(), document, error);
    }
};
}

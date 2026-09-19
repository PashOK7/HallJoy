#pragma once
#include <cwchar>
#include <initializer_list>
#include <cwctype>

namespace halljoy::public_diagnostic {
// Drop the remainder of a field list at a private field. Values can contain
// spaces, so splitting them at whitespace would leak partial names or paths.
inline void Redact(wchar_t* text) noexcept {
    if (!text) return;
    for (auto p=text; *p; ++p) {
        if (iswalpha(p[0]) && p[1]==L':' && (p[2]==L'\\' || p[2]==L'/')) {
            p[0]=L'*'; p[1]=L'\0'; return;
        }
        if (p!=text && p[-1]!=L' ') continue;
        auto end=p;
        while (iswalnum(*end) || *end==L'_') ++end;
        if (*end!=L'=') continue;
        const auto length=static_cast<std::size_t>(end-p);
        bool sensitive=false;
        for (const auto token : {L"path",L"root",L"legacy",L"source",L"backup",
             L"directory",L"active_profile",L"profile",L"name",L"serial",L"container",
             L"checkpoint",L"hid",L"scan",L"vkey",L"text",L"chunk0",L"data"})
            if (length==std::wcslen(token) && std::wcsncmp(p,token,length)==0) sensitive=true;
        if (length>=5 && std::wcsncmp(end-5,L"_hash",5)==0) sensitive=true;
        if (length>=5 && std::wcsncmp(end-5,L"_path",5)==0) sensitive=true;
        if (sensitive) {
            if (end[1]) { end[1]=L'*'; end[2]=L'\0'; }
            return;
        }
    }
}
}

#pragma once
#include <cwctype>
namespace halljoy::mini60 {
// USB product string of the AULA MINI 60 HE PRO ("MINI 60 HE PRO" in HidD_GetProductString and in
// the bus-reported name). Read without sending any command. The Red Square Alumix 68 shares
// 0C45:80A2 but does not use this name. Case-insensitive; "MINI 60" or "MINI60" followed by
// "HE PRO" with any spacing.
inline bool IsMini60ProName(const wchar_t* name) noexcept {
    if(!name)return false;
    wchar_t folded[128]{};
    unsigned n=0;
    for(const wchar_t* c=name;*c && n<127;++c)
        if(!std::iswspace(static_cast<wint_t>(*c)))folded[n++]=static_cast<wchar_t>(std::towupper(static_cast<wint_t>(*c)));
    folded[n]=0;
    const auto contains=[&](const wchar_t* needle){
        for(unsigned i=0;folded[i];++i){
            unsigned j=0;while(needle[j] && folded[i+j]==needle[j])++j;
            if(!needle[j])return true;
        }
        return false;
    };
    return contains(L"MINI60HEPRO");
}
}

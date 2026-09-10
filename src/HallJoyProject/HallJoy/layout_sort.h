#pragma once
#include <string_view>

namespace halljoy::layout_sort {
constexpr wchar_t Fold(wchar_t c) { return c>=L'A' && c<=L'Z' ? c+32 : c; }
constexpr bool Digit(wchar_t c) { return c>=L'0' && c<=L'9'; }
inline int Natural(std::wstring_view a,std::wstring_view b) {
    size_t i=0,j=0;
    while(i<a.size() && j<b.size()) {
        if(Digit(a[i]) && Digit(b[j])) {
            size_t ae=i,be=j;
            while(ae<a.size() && Digit(a[ae])) ++ae;
            while(be<b.size() && Digit(b[be])) ++be;
            while(i<ae && a[i]==L'0') ++i;
            while(j<be && b[j]==L'0') ++j;
            if(ae-i!=be-j) return ae-i<be-j ? -1 : 1;
            while(i<ae) { if(a[i]!=b[j]) return a[i]<b[j] ? -1 : 1; ++i; ++j; }
            i=ae;j=be; // No integer conversion/overflow, including user names.
        } else {
            if(Fold(a[i])!=Fold(b[j])) return Fold(a[i])<Fold(b[j]) ? -1 : 1;
            ++i;++j;
        }
    }
    return i==a.size() ? (j==b.size()?0:-1) : 1;
}
inline int Region(std::wstring_view& name) {
    constexpr std::wstring_view suffixes[]={L" ANSI",L" ISO",L" JIS"};
    for(int i=0;i<3;++i) {
        auto suffix=suffixes[i];
        if(name.size()>=suffix.size() && Natural(name.substr(name.size()-suffix.size()),suffix)==0) {
            name.remove_suffix(suffix.size());return i+1;
        }
    }
    return 0;
}
inline bool Less(std::wstring_view a,std::wstring_view b) {
    const int ar=Region(a),br=Region(b);
    const int base=Natural(a,b);
    return base ? base<0 : ar<br;
}
}

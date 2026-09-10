#include "../HallJoy/layout_sort.h"
#include <algorithm>
#include <cassert>
#include <iostream>
#include <string>
#include <vector>
int main() {
    using halljoy::layout_sort::Less;
    std::vector<std::wstring> values={L"Q12 HE ANSI",L"K10 HE ISO",L"Q1 HE 8K ISO",L"K2 HE JIS",L"Q1 HE JIS",L"K3 HE ANSI",L"K2 HE ANSI",L"Q1 HE 8K ANSI",L"K2 HE ISO",L"Q1 HE ANSI"};
    std::stable_sort(values.begin(),values.end(),Less);
    const std::vector<std::wstring> expected={L"K2 HE ANSI",L"K2 HE ISO",L"K2 HE JIS",L"K3 HE ANSI",L"K10 HE ISO",L"Q1 HE ANSI",L"Q1 HE JIS",L"Q1 HE 8K ANSI",L"Q1 HE 8K ISO",L"Q12 HE ANSI"};
    assert(values==expected);
    assert(!Less(L"K02 HE ANSI",L"k2 he ansi") && !Less(L"k2 he ansi",L"K02 HE ANSI"));
    assert(Less(L"Custom 99999999999999999999",L"Custom 100000000000000000000"));
    assert(Less(L"DrunkDeer A75",L"Keychron K2 HE ANSI"));
    values.insert(values.end(),{L"",L"K02 HE ANSI",L"k2 he ansi",L"Other",L"0",L"00"});
    for(const auto& a:values) for(const auto& b:values) for(const auto& c:values) {
        assert(!Less(a,a));assert(!(Less(a,b)&&Less(b,a)));
        if(Less(a,b)&&Less(b,c)) assert(Less(a,c));
    }
    std::cout<<"LAYOUT_NATURAL_SORT=PASS\n";
}

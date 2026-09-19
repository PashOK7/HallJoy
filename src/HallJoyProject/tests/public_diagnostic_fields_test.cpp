#include "public_diagnostic_fields.h"
#include <cassert>
#include <string>
#include <iostream>
int main() {
    for (const auto text:{L"error=0 active_profile=Secret Name later=Private",
        L"path=Z:\\Private Folder\\file.txt next=Private",L"instance_hash=Private",
        L"checkpoint=Private",L"hid=Private",L"root=/private/name",L"Z:\\Private"}) {
        std::wstring s=text;halljoy::public_diagnostic::Redact(s.data());
        assert(!wcsstr(s.c_str(),L"Private") && !wcsstr(s.c_str(),L"Secret"));
    }
    wchar_t emptyValue[]=L"path=";halljoy::public_diagnostic::Redact(emptyValue);
    assert(wcscmp(emptyValue,L"path=")==0);
    wchar_t aggregate[]=L"publications=500 active=2 failures=0";
    halljoy::public_diagnostic::Redact(aggregate);
    assert(wcscmp(aggregate,L"publications=500 active=2 failures=0")==0);
    halljoy::public_diagnostic::Redact(nullptr);
    std::cout<<"PUBLIC_DIAGNOSTIC_FIELDS=PASS\n";
}

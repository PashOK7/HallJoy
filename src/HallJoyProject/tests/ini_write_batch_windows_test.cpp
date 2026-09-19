#include "ini_write_batch.h"
#include <cassert>
#include <filesystem>
#include <iostream>
#include <thread>
int main() {
    wchar_t root[MAX_PATH]{}, path[MAX_PATH]{}, other[MAX_PATH]{};
    assert(GetTempPathW(MAX_PATH, root));
    assert(GetTempFileNameW(root,L"HJB",0,path));
    assert(GetTempFileNameW(root,L"HJB",0,other));
    using halljoy::ini::WriteBatch;
    DWORD error=0;
    {
        WriteBatch batch(path);
        assert(WriteBatch::Put(L"Section",L"Key",L"old",path));
        assert(WriteBatch::Put(L"section",L"KEY",L"  Unicode \x0416 \"quote\"  ",path));
        assert(WriteBatch::Put(L"Section",L"Delete",L"gone",path));
        assert(WriteBatch::Put(L"Section",L"Delete",nullptr,path));
        assert(WriteBatch::Put(L"Remove",L"Key",L"gone",path));
        assert(WriteBatch::Put(L"Remove",nullptr,nullptr,path));
        assert(!WriteBatch::Put(L"Section",L"Bad",L"line\nbreak",path));
        assert(!WriteBatch::Put(L"Section",L"Key",L"wrong path",other));
        assert(std::filesystem::file_size(path)==0);
        {
            WriteBatch nested(other);
            assert(WriteBatch::Put(L"Nested",L"Key",L"value",other));
            assert(nested.Finish(&error));
        }
        bool threadOk=false;
        std::thread worker([&] { threadOk=WriteBatch::Put(L"Direct",L"Key",L"thread",other)!=FALSE; });
        worker.join(); assert(threadOk);
        assert(WriteBatch::Put(L"Section",L"Empty",L"",path));
        assert(batch.Finish(&error));
    }
    wchar_t value[128]{};
    GetPrivateProfileStringW(L"SECTION",L"key",L"missing",value,128,path);
    assert(std::wstring(value)==L"  Unicode \x0416 \"quote\"  ");
    for (const auto* key:{L"Delete",L"Bad"}) {
        GetPrivateProfileStringW(L"Section",key,L"missing",value,128,path);
        assert(std::wstring(value)==L"missing");
    }
    GetPrivateProfileStringW(L"Direct",L"Key",L"missing",value,128,other);
    assert(std::wstring(value)==L"thread");
    assert(WriteBatch::Put(L"Section",L"Direct",L"restored",path));
    assert(DeleteFileW(path));assert(DeleteFileW(other));
    std::cout << "INI_WRITE_BATCH_WINDOWS_TEST=PASS scope=1 thread_isolation=1 unicode=1 deletion=1 fallback=1\n";
}

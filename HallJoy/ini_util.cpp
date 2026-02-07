#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "ini_util.h"

void IniUtil_Flush(const wchar_t* path)
{
    if (!path) return;
    WritePrivateProfileStringW(nullptr, nullptr, nullptr, path);
}

bool IniUtil_AtomicReplace(const wchar_t* tmpPath, const wchar_t* dstPath)
{
    if (!tmpPath || !dstPath) return false;

    IniUtil_Flush(tmpPath);

    BOOL ok = MoveFileExW(tmpPath, dstPath, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
    if (!ok)
    {
        DeleteFileW(tmpPath);
        return false;
    }
    return true;
}
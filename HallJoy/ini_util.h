#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <string>

// Forces WritePrivateProfile* buffers to be flushed to disk for this INI file.
void IniUtil_Flush(const wchar_t* path);

// Atomically replace destination INI file with tmp file.
// - Flushes tmp
// - MoveFileEx(REPLACE_EXISTING | WRITE_THROUGH)
// - Deletes tmp on failure
bool IniUtil_AtomicReplace(const wchar_t* tmpPath, const wchar_t* dstPath);
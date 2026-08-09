#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <string>

#include "transactional_file_store.h"

using IniUtilWriteCallback = bool (*)(const wchar_t* temporaryPath, void* context, DWORD* errorOut);
using IniUtilValidateCallback = bool (*)(const wchar_t* temporaryPath, void* context, DWORD* errorOut);

// Forces WritePrivateProfile* buffers to be flushed to disk for this INI file.
bool IniUtil_Flush(const wchar_t* path);

// Writes to a unique same-directory temporary file, physically flushes it,
// validates it by reading it back, then atomically replaces the destination.
HallJoyPersistence::SaveResult IniUtil_SaveAtomic(
    const wchar_t* destinationPath,
    IniUtilWriteCallback writer,
    IniUtilValidateCallback validator,
    void* context);

// Emits a critical trace and a single actionable UI error per process.
void IniUtil_ReportSaveFailure(
    const wchar_t* dataKind,
    const wchar_t* destinationPath,
    const HallJoyPersistence::SaveResult& result);

const wchar_t* IniUtil_SaveStageName(HallJoyPersistence::SaveStage stage) noexcept;

// Copies destination contents into an already-created transaction temp file.
// A missing destination is treated as an empty new file.
bool IniUtil_CopyExistingForUpdate(
    const wchar_t* destinationPath,
    const wchar_t* temporaryPath,
    DWORD* errorOut);

// Atomically replace destination INI file with tmp file.
// - Flushes tmp
// - MoveFileEx(REPLACE_EXISTING | WRITE_THROUGH)
// - Deletes tmp on failure
bool IniUtil_AtomicReplace(const wchar_t* tmpPath, const wchar_t* dstPath);

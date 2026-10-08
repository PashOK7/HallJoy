#pragma once
#include <windows.h>

bool Profile_SaveIni(const wchar_t* path);
bool Profile_LoadIni(const wchar_t* path);
#include "bindings.h"
bool Profile_PrepareIni(const wchar_t* path, BindingsSnapshot& out);
bool Profile_WriteBindingsSections(const wchar_t* path);
// Writes all bindings sections of a snapshot plus the bundle marker into an
// existing file (export/import staging). Does not read or change live bindings.
bool Profile_WriteBindingsSnapshot(const wchar_t* path, const BindingsSnapshot& snapshot);

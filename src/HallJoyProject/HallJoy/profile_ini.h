#pragma once
#include <windows.h>

bool Profile_SaveIni(const wchar_t* path);
bool Profile_LoadIni(const wchar_t* path);
#include "bindings.h"
bool Profile_PrepareIni(const wchar_t* path, BindingsSnapshot& out);
bool Profile_WriteBindingsSections(const wchar_t* path);

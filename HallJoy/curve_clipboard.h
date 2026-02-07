// curve_clipboard.h
#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <string>
#include <cstdint>

#include "key_settings.h"

// Clipboard helper for copying/pasting curve settings.
//
// Stored as CF_UNICODETEXT with a small tagged text format, so it's easy to debug
// and robust across app versions.
//
// This module is intentionally UI-agnostic: it only knows how to serialize/parse
// KeyDeadzone and interact with Win32 clipboard.

namespace CurveClipboard
{
    // Copies curve to clipboard. Returns true on success.
    bool Copy(HWND owner, const KeyDeadzone& ks);

    // Tries to read curve from clipboard into outKs. Returns true on success.
    bool Paste(HWND owner, KeyDeadzone& outKs);
}
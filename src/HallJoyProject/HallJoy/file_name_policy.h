#pragma once

#include <string>

// Windows-safe canonical filename policy shared by global profiles, keyboard
// layouts and curve presets. Returned stems never contain a path separator,
// trailing dot/space, a reserved DOS device basename, or more than 80 UTF-16
// code units. User-visible text is normalized to NFC.
std::wstring FileNamePolicy_NormalizeStem(const std::wstring& input);
std::wstring FileNamePolicy_CanonicalKey(const std::wstring& input);
bool FileNamePolicy_Equivalent(const std::wstring& left, const std::wstring& right);

// Builds one direct child path and rejects extension/path aliasing.
bool FileNamePolicy_BuildChildPath(
    const std::wstring& root,
    const std::wstring& stem,
    const std::wstring& extension,
    std::wstring& outPath);

// Chooses a collision-free direct child. Collisions are compared after NFC and
// invariant case folding, then resolved with a bounded " (n)" suffix.
bool FileNamePolicy_MakeUniqueChildPath(
    const std::wstring& root,
    const std::wstring& requestedStem,
    const std::wstring& extension,
    std::wstring& outStem,
    std::wstring& outPath);

constexpr std::size_t FileNamePolicy_MaxStemLength() noexcept { return 80; }

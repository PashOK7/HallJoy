#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <algorithm>
#include <cwctype>
#include <filesystem>
#include <string>
#include <unordered_set>

#include "file_name_policy.h"

#pragma comment(lib, "Normaliz.lib")

namespace fs = std::filesystem;

namespace
{
    std::wstring NormalizeNfc(const std::wstring& input)
    {
        if (input.empty()) return {};
        const int required = NormalizeString(
            NormalizationC,
            input.data(),
            static_cast<int>(input.size()),
            nullptr,
            0);
        if (required <= 0)
            return input;

        std::wstring normalized(static_cast<std::size_t>(required), L'\0');
        const int written = NormalizeString(
            NormalizationC,
            input.data(),
            static_cast<int>(input.size()),
            normalized.data(),
            required);
        if (written <= 0)
            return input;
        normalized.resize(static_cast<std::size_t>(written));
        return normalized;
    }

    std::wstring InvariantLower(const std::wstring& input)
    {
        if (input.empty()) return {};
        const int required = LCMapStringEx(
            LOCALE_NAME_INVARIANT,
            LCMAP_LOWERCASE | LCMAP_LINGUISTIC_CASING,
            input.data(),
            static_cast<int>(input.size()),
            nullptr,
            0,
            nullptr,
            nullptr,
            0);
        if (required <= 0)
            return input;

        std::wstring lowered(static_cast<std::size_t>(required), L'\0');
        const int written = LCMapStringEx(
            LOCALE_NAME_INVARIANT,
            LCMAP_LOWERCASE | LCMAP_LINGUISTIC_CASING,
            input.data(),
            static_cast<int>(input.size()),
            lowered.data(),
            required,
            nullptr,
            nullptr,
            0);
        if (written <= 0)
            return input;
        lowered.resize(static_cast<std::size_t>(written));
        return lowered;
    }

    bool IsReservedDeviceBase(const std::wstring& stem)
    {
        std::wstring base = stem.substr(0, stem.find(L'.'));
        base = InvariantLower(base);
        if (base == L"con" || base == L"prn" || base == L"aux" ||
            base == L"nul" || base == L"clock$")
            return true;
        if (base.size() == 4 &&
            (base.rfind(L"com", 0) == 0 || base.rfind(L"lpt", 0) == 0) &&
            base[3] >= L'1' && base[3] <= L'9')
            return true;
        return false;
    }

    std::wstring FullPath(const std::wstring& path)
    {
        if (path.empty()) return {};
        DWORD required = GetFullPathNameW(path.c_str(), 0, nullptr, nullptr);
        if (required == 0) return {};
        std::wstring full(static_cast<std::size_t>(required), L'\0');
        DWORD written = GetFullPathNameW(path.c_str(), required, full.data(), nullptr);
        if (written == 0 || written >= required) return {};
        full.resize(static_cast<std::size_t>(written));
        return full;
    }

    bool IsDirectChild(const std::wstring& root, const std::wstring& candidate)
    {
        std::wstring fullRoot = FullPath(root);
        std::wstring fullCandidate = FullPath(candidate);
        if (fullRoot.empty() || fullCandidate.empty()) return false;
        while (!fullRoot.empty() && (fullRoot.back() == L'\\' || fullRoot.back() == L'/'))
            fullRoot.pop_back();
        fullRoot.push_back(L'\\');
        if (fullCandidate.size() <= fullRoot.size()) return false;
        return CompareStringOrdinal(
            fullRoot.c_str(),
            static_cast<int>(fullRoot.size()),
            fullCandidate.c_str(),
            static_cast<int>(fullRoot.size()),
            TRUE) == CSTR_EQUAL &&
            fullCandidate.find_first_of(L"\\/", fullRoot.size()) == std::wstring::npos;
    }

    bool ExtensionIsSafe(const std::wstring& extension)
    {
        return !extension.empty() && extension.front() == L'.' &&
            extension.find_first_of(L"\\/") == std::wstring::npos &&
            extension.find(L"..") == std::wstring::npos;
    }

    std::wstring FitSuffix(const std::wstring& base, const std::wstring& suffix)
    {
        constexpr std::size_t maxLength = FileNamePolicy_MaxStemLength();
        if (suffix.size() >= maxLength) return {};
        std::wstring fitted = base.substr(0, maxLength - suffix.size());
        while (!fitted.empty() && (fitted.back() == L' ' || fitted.back() == L'.'))
            fitted.pop_back();
        fitted += suffix;
        return fitted;
    }
}

std::wstring FileNamePolicy_NormalizeStem(const std::wstring& input)
{
    std::wstring stem = NormalizeNfc(input);
    while (!stem.empty() && iswspace(stem.front())) stem.erase(stem.begin());
    while (!stem.empty() && (iswspace(stem.back()) || stem.back() == L'.')) stem.pop_back();

    constexpr const wchar_t* invalid = L"<>:\"/\\|?*";
    for (wchar_t& ch : stem)
    {
        if (ch < 32 || wcschr(invalid, ch))
            ch = L'_';
    }
    while (!stem.empty() && (iswspace(stem.back()) || stem.back() == L'.')) stem.pop_back();
    if (stem.size() > FileNamePolicy_MaxStemLength())
    {
        stem.resize(FileNamePolicy_MaxStemLength());
        if (!stem.empty() && stem.back() >= 0xD800 && stem.back() <= 0xDBFF)
            stem.pop_back();
    }
    while (!stem.empty() && (iswspace(stem.back()) || stem.back() == L'.')) stem.pop_back();
    if (stem.empty() || stem == L"." || stem == L"..")
        return {};
    if (IsReservedDeviceBase(stem))
    {
        if (stem.size() == FileNamePolicy_MaxStemLength()) stem.pop_back();
        stem.insert(stem.begin(), L'_');
    }
    return stem;
}

std::wstring FileNamePolicy_CanonicalKey(const std::wstring& input)
{
    return InvariantLower(FileNamePolicy_NormalizeStem(input));
}

bool FileNamePolicy_Equivalent(const std::wstring& left, const std::wstring& right)
{
    const std::wstring leftKey = FileNamePolicy_CanonicalKey(left);
    const std::wstring rightKey = FileNamePolicy_CanonicalKey(right);
    return !leftKey.empty() && leftKey == rightKey;
}

bool FileNamePolicy_BuildChildPath(
    const std::wstring& root,
    const std::wstring& stem,
    const std::wstring& extension,
    std::wstring& outPath)
{
    outPath.clear();
    const std::wstring normalized = FileNamePolicy_NormalizeStem(stem);
    if (root.empty() || normalized.empty() || !ExtensionIsSafe(extension))
        return false;
    const std::wstring candidate = (fs::path(root) / (normalized + extension)).wstring();
    if (!IsDirectChild(root, candidate))
        return false;
    outPath = candidate;
    return true;
}

bool FileNamePolicy_MakeUniqueChildPath(
    const std::wstring& root,
    const std::wstring& requestedStem,
    const std::wstring& extension,
    std::wstring& outStem,
    std::wstring& outPath)
{
    outStem.clear();
    outPath.clear();
    const std::wstring base = FileNamePolicy_NormalizeStem(requestedStem);
    if (base.empty() || !ExtensionIsSafe(extension)) return false;

    std::unordered_set<std::wstring> existingKeys;
    std::error_code ec;
    for (const auto& entry : fs::directory_iterator(fs::path(root), ec))
    {
        if (ec) return false;
        if (!entry.is_regular_file(ec) || ec) continue;
        if (_wcsicmp(entry.path().extension().c_str(), extension.c_str()) != 0) continue;
        existingKeys.insert(FileNamePolicy_CanonicalKey(entry.path().stem().wstring()));
    }

    for (unsigned index = 0; index < 10000; ++index)
    {
        const std::wstring suffix = index == 0 ? L"" : L" (" + std::to_wstring(index) + L")";
        const std::wstring candidateStem = FitSuffix(base, suffix);
        const std::wstring key = FileNamePolicy_CanonicalKey(candidateStem);
        std::wstring candidatePath;
        if (key.empty() || existingKeys.contains(key) ||
            !FileNamePolicy_BuildChildPath(root, candidateStem, extension, candidatePath))
            continue;
        if (GetFileAttributesW(candidatePath.c_str()) == INVALID_FILE_ATTRIBUTES)
        {
            outStem = candidateStem;
            outPath = candidatePath;
            return true;
        }
    }
    return false;
}

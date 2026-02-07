// keyboard_profiles.cpp
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <shlobj.h>     // SHGetKnownFolderPath
#pragma comment(lib, "ole32.lib")

#include <string>
#include <vector>
#include <algorithm>
#include <filesystem>

#include "keyboard_profiles.h"
#include "ini_util.h"
#include "win_util.h"

namespace fs = std::filesystem;

// ----------------------------------------------------------------------------
// Module state (UI state: which preset is considered "active" + dirty flag)
// ----------------------------------------------------------------------------
static std::wstring g_activeName;
static std::wstring g_activePath;
static bool g_dirty = false;

// Persisted UI state load flag
static bool g_stateLoaded = false;

// ----------------------------------------------------------------------------
// Small helpers
// ----------------------------------------------------------------------------
static std::wstring SanitizeFileName(const std::wstring& name)
{
    std::wstring s = name;

    // trim spaces
    while (!s.empty() && s.front() == L' ') s.erase(s.begin());
    while (!s.empty() && s.back() == L' ') s.pop_back();

    const wchar_t* bad = L"<>:\"/\\|?*";
    for (size_t i = 0; i < s.size(); ++i)
    {
        if (wcschr(bad, s[i]) || s[i] < 32)
            s[i] = L'_';
    }

    // no trailing dot/space
    while (!s.empty() && (s.back() == L'.' || s.back() == L' ')) s.pop_back();

    if (s.empty()) s = L"New Preset";
    return s;
}

static bool EnsureDirExists(const std::wstring& dir)
{
    std::error_code ec;
    if (fs::exists(dir, ec))
        return true;
    return fs::create_directories(dir, ec);
}

static bool TestDirWritable(const std::wstring& dir)
{
    if (dir.empty()) return false;

    std::wstring tmp = dir;
    if (!tmp.empty() && tmp.back() != L'\\' && tmp.back() != L'/')
        tmp += L'\\';
    tmp += L"~dd_curvepreset_write_test.tmp";

    HANDLE h = CreateFileW(tmp.c_str(),
        GENERIC_WRITE,
        FILE_SHARE_READ,
        nullptr,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_TEMPORARY | FILE_ATTRIBUTE_HIDDEN,
        nullptr);

    if (h == INVALID_HANDLE_VALUE)
        return false;

    const char data[4] = { 'T','E','S','T' };
    DWORD written = 0;
    BOOL ok = WriteFile(h, data, (DWORD)sizeof(data), &written, nullptr);
    CloseHandle(h);

    DeleteFileW(tmp.c_str());
    return ok && (written == sizeof(data));
}

static std::wstring TryGetLocalAppDataPresetsDir()
{
    PWSTR p = nullptr;
    if (FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppData, KF_FLAG_DEFAULT, nullptr, &p)) || !p)
        return L"";

    std::wstring base = p;
    CoTaskMemFree(p);

    // New folder (no backward compatibility required)
    return base + L"\\HallJoy\\CurvePresets";
}

// FIX: cache presets dir to avoid repeated write-test file creation on every call
static const std::wstring& GetPresetsDir()
{
    static std::wstring cached;
    static bool inited = false;
    if (inited) return cached;

    // 1) Prefer near exe if writable
    {
        std::wstring dirExe = WinUtil_BuildPathNearExe(L"CurvePresets");
        if (EnsureDirExists(dirExe) && TestDirWritable(dirExe))
        {
            cached = dirExe;
            inited = true;
            return cached;
        }
    }

    // 2) Fallback to LocalAppData
    {
        std::wstring dirApp = TryGetLocalAppDataPresetsDir();
        if (!dirApp.empty())
        {
            if (EnsureDirExists(dirApp) && TestDirWritable(dirApp))
            {
                cached = dirApp;
                inited = true;
                return cached;
            }
        }
    }

    // Last resort (may be read-only, but predictable)
    cached = WinUtil_BuildPathNearExe(L"CurvePresets");
    EnsureDirExists(cached);
    inited = true;
    return cached;
}

// Persist UI state in a tiny INI inside presets dir
static const std::wstring& GetStateIniPath()
{
    static std::wstring p;
    static bool inited = false;
    if (inited) return p;

    p = GetPresetsDir();
    if (!p.empty() && p.back() != L'\\' && p.back() != L'/')
        p += L'\\';
    p += L"_preset_state.ini";

    inited = true;
    return p;
}

static void LoadStateOnce()
{
    if (g_stateLoaded) return;
    g_stateLoaded = true;

    const std::wstring& stPath = GetStateIniPath();

    wchar_t buf[260]{};
    GetPrivateProfileStringW(L"UI", L"ActiveName", L"", buf, 260, stPath.c_str());
    g_activeName = buf;

    // We don't persist dirty; always start clean (UI will compute it anyway)
    g_dirty = false;
}

static void SaveState()
{
    const std::wstring& stPath = GetStateIniPath();

    // Ensure dir exists (should already, but safe)
    EnsureDirExists(GetPresetsDir());

    WritePrivateProfileStringW(L"UI", L"ActiveName", g_activeName.c_str(), stPath.c_str());
    IniUtil_Flush(stPath.c_str());
}

static float ClampF(float v, float lo, float hi)
{
    return (v < lo) ? lo : (v > hi ? hi : v);
}

static int ClampI(int v, int lo, int hi)
{
    return (v < lo) ? lo : (v > hi ? hi : v);
}

static float ReadM01(const wchar_t* sec, const wchar_t* key, int defM, const wchar_t* path)
{
    int m = GetPrivateProfileIntW(sec, key, defM, path);
    m = ClampI(m, 0, 1000);
    return (float)m / 1000.0f;
}

static int ReadI(const wchar_t* sec, const wchar_t* key, int defV, const wchar_t* path)
{
    return GetPrivateProfileIntW(sec, key, defV, path);
}

static void WriteI(const wchar_t* sec, const wchar_t* key, int v, const wchar_t* path)
{
    wchar_t buf[64]{};
    swprintf_s(buf, L"%d", v);
    WritePrivateProfileStringW(sec, key, buf, path);
}

static void WriteM01(const wchar_t* sec, const wchar_t* key, float v01, const wchar_t* path)
{
    int m = (int)lroundf(ClampF(v01, 0.0f, 1.0f) * 1000.0f);
    WriteI(sec, key, m, path);
}

static KeyDeadzone NormalizePreset(KeyDeadzone ks)
{
    ks.useUnique = true; // preset represents a standalone curve definition

    ks.curveMode = (ks.curveMode == 0) ? 0 : 1;

    ks.low = ClampF(ks.low, 0.0f, 0.99f);
    ks.high = ClampF(ks.high, 0.01f, 1.0f);
    if (ks.high < ks.low + 0.01f)
        ks.high = ClampF(ks.low + 0.01f, 0.01f, 1.0f);

    ks.antiDeadzone = ClampF(ks.antiDeadzone, 0.0f, 0.99f);
    ks.outputCap = ClampF(ks.outputCap, 0.01f, 1.0f);
    if (ks.outputCap < ks.antiDeadzone + 0.01f)
        ks.outputCap = ClampF(ks.antiDeadzone + 0.01f, 0.01f, 1.0f);

    ks.cp1_x = ClampF(ks.cp1_x, 0.0f, 1.0f);
    ks.cp1_y = ClampF(ks.cp1_y, 0.0f, 1.0f);
    ks.cp2_x = ClampF(ks.cp2_x, 0.0f, 1.0f);
    ks.cp2_y = ClampF(ks.cp2_y, 0.0f, 1.0f);

    ks.cp1_w = ClampF(ks.cp1_w, 0.0f, 1.0f);
    ks.cp2_w = ClampF(ks.cp2_w, 0.0f, 1.0f);

    // enforce monotonic-ish X order like editor/backend
    const float minGap = 0.01f;
    ks.cp1_x = ClampF(ks.cp1_x, ks.low + minGap, ks.high - minGap);
    ks.cp2_x = ClampF(ks.cp2_x, ks.cp1_x + minGap, ks.high - minGap);

    return ks;
}

// Internal load that NEVER touches module active/dirty state (safe for comparisons)
static bool LoadPresetFile_NoState(const std::wstring& path, KeyDeadzone& outKs)
{
    if (GetFileAttributesW(path.c_str()) == INVALID_FILE_ATTRIBUTES)
        return false;

    // defaults from struct
    KeyDeadzone ks{};

    // Read milli-values to avoid locale float issues
    ks.low = ReadM01(L"Curve", L"Low", (int)lroundf(ks.low * 1000.0f), path.c_str());
    ks.high = ReadM01(L"Curve", L"High", (int)lroundf(ks.high * 1000.0f), path.c_str());
    ks.antiDeadzone = ReadM01(L"Curve", L"AntiDeadzone", (int)lroundf(ks.antiDeadzone * 1000.0f), path.c_str());
    ks.outputCap = ReadM01(L"Curve", L"OutputCap", (int)lroundf(ks.outputCap * 1000.0f), path.c_str());

    ks.cp1_x = ReadM01(L"Curve", L"Cp1X", (int)lroundf(ks.cp1_x * 1000.0f), path.c_str());
    ks.cp1_y = ReadM01(L"Curve", L"Cp1Y", (int)lroundf(ks.cp1_y * 1000.0f), path.c_str());
    ks.cp2_x = ReadM01(L"Curve", L"Cp2X", (int)lroundf(ks.cp2_x * 1000.0f), path.c_str());
    ks.cp2_y = ReadM01(L"Curve", L"Cp2Y", (int)lroundf(ks.cp2_y * 1000.0f), path.c_str());

    ks.cp1_w = ReadM01(L"Curve", L"Cp1W", (int)lroundf(ks.cp1_w * 1000.0f), path.c_str());
    ks.cp2_w = ReadM01(L"Curve", L"Cp2W", (int)lroundf(ks.cp2_w * 1000.0f), path.c_str());

    ks.curveMode = (uint8_t)(ReadI(L"Curve", L"Mode", (int)ks.curveMode, path.c_str()) == 0 ? 0 : 1);
    ks.invert = (ReadI(L"Curve", L"Invert", ks.invert ? 1 : 0, path.c_str()) != 0);

    ks = NormalizePreset(ks);
    outKs = ks;
    return true;
}

// ----------------------------------------------------------------------------
// Public API (now: curve presets only)
// ----------------------------------------------------------------------------
namespace KeyboardProfiles
{
    int RefreshList(std::vector<ProfileInfo>& outList)
    {
        LoadStateOnce();

        outList.clear();

        std::wstring dir = GetPresetsDir();
        std::wstring search = dir + L"\\*.ini";

        WIN32_FIND_DATAW fd{};
        HANDLE hFind = FindFirstFileW(search.c_str(), &fd);
        if (hFind != INVALID_HANDLE_VALUE)
        {
            do
            {
                if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;

                std::wstring fname = fd.cFileName;

                // Skip our internal state file
                if (_wcsicmp(fname.c_str(), L"_preset_state.ini") == 0)
                    continue;

                fs::path p(fname);

                ProfileInfo pi;
                pi.name = p.stem().wstring();
                pi.path = dir + L"\\" + fname;
                outList.push_back(std::move(pi));

            } while (FindNextFileW(hFind, &fd));
            FindClose(hFind);
        }

        std::sort(outList.begin(), outList.end(), [](const ProfileInfo& a, const ProfileInfo& b) {
            return _wcsicmp(a.name.c_str(), b.name.c_str()) < 0;
            });

        int activeIdx = -1;
        if (!g_activeName.empty())
        {
            for (int i = 0; i < (int)outList.size(); ++i)
            {
                if (_wcsicmp(outList[i].name.c_str(), g_activeName.c_str()) == 0)
                {
                    activeIdx = i;
                    break;
                }
            }
        }

        // IMPORTANT: keep active path in sync with active name (path is not persisted).
        if (activeIdx >= 0)
        {
            g_activePath = outList[activeIdx].path;
        }
        else
        {
            // active preset no longer exists on disk -> clear active state to avoid stale UI
            if (!g_activeName.empty())
            {
                g_activeName.clear();
                g_activePath.clear();
                SaveState();
            }
        }

        return activeIdx;
    }

    bool LoadPreset(const std::wstring& path, KeyDeadzone& outKs)
    {
        // FIX: no side effects (does NOT change active/dirty).
        return LoadPresetFile_NoState(path, outKs);
    }

    bool SavePreset(const std::wstring& path, const KeyDeadzone& inKs)
    {
        LoadStateOnce();

        if (path.empty()) return false;

        // Write to tmp, then atomic replace
        std::wstring tmp = path + L".tmp";
        DeleteFileW(tmp.c_str());

        KeyDeadzone ks = NormalizePreset(inKs);

        // clear section (no compatibility needed, keep file clean)
        WritePrivateProfileStringW(L"Curve", nullptr, nullptr, tmp.c_str());

        WriteM01(L"Curve", L"Low", ks.low, tmp.c_str());
        WriteM01(L"Curve", L"High", ks.high, tmp.c_str());
        WriteM01(L"Curve", L"AntiDeadzone", ks.antiDeadzone, tmp.c_str());
        WriteM01(L"Curve", L"OutputCap", ks.outputCap, tmp.c_str());

        WriteM01(L"Curve", L"Cp1X", ks.cp1_x, tmp.c_str());
        WriteM01(L"Curve", L"Cp1Y", ks.cp1_y, tmp.c_str());
        WriteM01(L"Curve", L"Cp2X", ks.cp2_x, tmp.c_str());
        WriteM01(L"Curve", L"Cp2Y", ks.cp2_y, tmp.c_str());

        WriteM01(L"Curve", L"Cp1W", ks.cp1_w, tmp.c_str());
        WriteM01(L"Curve", L"Cp2W", ks.cp2_w, tmp.c_str());

        WriteI(L"Curve", L"Mode", (int)(ks.curveMode == 0 ? 0 : 1), tmp.c_str());
        WriteI(L"Curve", L"Invert", ks.invert ? 1 : 0, tmp.c_str());

        if (!IniUtil_AtomicReplace(tmp.c_str(), path.c_str()))
        {
            DeleteFileW(tmp.c_str());
            return false;
        }

        // Update "active preset" state (saving means "this preset is now current")
        fs::path pp(path);
        g_activeName = pp.stem().wstring();
        g_activePath = path;
        g_dirty = false;
        SaveState();

        return true;
    }

    bool CreatePreset(const std::wstring& name, const KeyDeadzone& ks)
    {
        LoadStateOnce();

        std::wstring safeName = SanitizeFileName(name);

        std::wstring dir = GetPresetsDir();
        EnsureDirExists(dir);

        std::wstring path = dir + L"\\" + safeName + L".ini";

        int counter = 1;
        while (GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES)
        {
            path = dir + L"\\" + safeName + L" (" + std::to_wstring(counter++) + L").ini";
        }

        return SavePreset(path, ks);
    }

    bool DeletePreset(const std::wstring& path)
    {
        LoadStateOnce();

        if (GetFileAttributesW(path.c_str()) == INVALID_FILE_ATTRIBUTES)
            return false;

        if (!DeleteFileW(path.c_str()))
            return false;

        // Determine whether this is the active preset:
        // - Prefer path compare if we have it
        // - Fallback to name compare (stem) because g_activePath is not persisted
        bool isActive = false;

        if (!g_activePath.empty() && _wcsicmp(g_activePath.c_str(), path.c_str()) == 0)
        {
            isActive = true;
        }
        else if (!g_activeName.empty())
        {
            fs::path pp(path);
            std::wstring name = pp.stem().wstring();
            if (_wcsicmp(name.c_str(), g_activeName.c_str()) == 0)
                isActive = true;
        }

        if (isActive)
        {
            g_activeName.clear();
            g_activePath.clear();
            g_dirty = true;
            SaveState();
        }

        return true;
    }

    std::wstring GetActiveProfileName()
    {
        LoadStateOnce();
        return g_activeName;
    }

    void SetActiveProfileName(const std::wstring& name)
    {
        LoadStateOnce();

        g_activeName = name;

        // IMPORTANT: path is not persisted; force re-resolve on next RefreshList()
        g_activePath.clear();

        SaveState();
    }

    void SetDirty(bool dirty)
    {
        LoadStateOnce();
        g_dirty = dirty;
        // dirty isn't persisted; no SaveState() here
    }

    bool IsDirty()
    {
        LoadStateOnce();
        return g_dirty;
    }
}
// keyboard_profiles.cpp
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <string>
#include <vector>
#include <algorithm>
#include <filesystem>

#include "keyboard_profiles.h"
#include "app_paths.h"
#include "file_name_policy.h"
#include "ini_util.h"

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
static bool EnsureDirExists(const std::wstring& dir)
{
    std::error_code ec;
    if (fs::exists(dir, ec))
        return true;
    return fs::create_directories(dir, ec);
}

static const std::wstring& GetPresetsDir()
{
    return AppPaths_CurvePresetsDir();
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
    g_activeName = FileNamePolicy_NormalizeStem(buf);

    // We don't persist dirty; always start clean (UI will compute it anyway)
    g_dirty = false;
}

namespace
{
    struct CurveStateSaveContext
    {
        const wchar_t* destinationPath = nullptr;
        const wchar_t* activeName = nullptr;
    };

    bool CurveStateTransactionWrite(const wchar_t* temporaryPath, void* rawContext, DWORD* errorOut)
    {
        auto* context = static_cast<CurveStateSaveContext*>(rawContext);
        if (!IniUtil_CopyExistingForUpdate(context->destinationPath, temporaryPath, errorOut))
            return false;

        bool ok = WritePrivateProfileStringW(L"HallJoyPersistence", L"SchemaVersion", L"1", temporaryPath) != FALSE;
        ok &= WritePrivateProfileStringW(L"HallJoyPersistence", L"Kind", L"CurveState", temporaryPath) != FALSE;
        ok &= WritePrivateProfileStringW(L"UI", L"ActiveName", context->activeName, temporaryPath) != FALSE;
        if (!ok && errorOut)
        {
            const DWORD error = GetLastError();
            *errorOut = error != ERROR_SUCCESS ? error : ERROR_WRITE_FAULT;
        }
        return ok;
    }

    bool CurveStateTransactionValidate(const wchar_t* temporaryPath, void* rawContext, DWORD* errorOut)
    {
        auto* context = static_cast<CurveStateSaveContext*>(rawContext);
        wchar_t schema[32]{};
        wchar_t kind[32]{};
        wchar_t active[260]{};
        GetPrivateProfileStringW(L"HallJoyPersistence", L"SchemaVersion", L"{missing}", schema, (DWORD)_countof(schema), temporaryPath);
        GetPrivateProfileStringW(L"HallJoyPersistence", L"Kind", L"{missing}", kind, (DWORD)_countof(kind), temporaryPath);
        GetPrivateProfileStringW(L"UI", L"ActiveName", L"{missing}", active, (DWORD)_countof(active), temporaryPath);
        const bool ok = wcscmp(schema, L"1") == 0 &&
            wcscmp(kind, L"CurveState") == 0 &&
            wcscmp(active, context->activeName) == 0;
        if (!ok && errorOut) *errorOut = ERROR_INVALID_DATA;
        return ok;
    }

    bool SaveStateToPath(const std::wstring& path, const std::wstring& activeName)
    {
        if (path.empty()) return false;
        CurveStateSaveContext context{ path.c_str(), activeName.c_str() };
        const auto result = IniUtil_SaveAtomic(
            path.c_str(),
            CurveStateTransactionWrite,
            CurveStateTransactionValidate,
            &context);
        if (!result.Succeeded())
        {
            IniUtil_ReportSaveFailure(L"curve state", path.c_str(), result);
            return false;
        }
        return true;
    }
}

static bool SaveState()
{
    const std::wstring& stPath = GetStateIniPath();

    // Ensure dir exists (should already, but safe)
    if (!EnsureDirExists(GetPresetsDir()))
        return false;

    return SaveStateToPath(stPath, g_activeName);
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

static bool WriteI(const wchar_t* sec, const wchar_t* key, int v, const wchar_t* path)
{
    wchar_t buf[64]{};
    swprintf_s(buf, L"%d", v);
    return WritePrivateProfileStringW(sec, key, buf, path) != FALSE;
}

static bool WriteM01(const wchar_t* sec, const wchar_t* key, float v01, const wchar_t* path)
{
    int m = (int)lroundf(ClampF(v01, 0.0f, 1.0f) * 1000.0f);
    return WriteI(sec, key, m, path);
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

namespace
{
    struct CurvePresetSaveContext
    {
        KeyDeadzone curve{};
    };

    bool CurvePresetTransactionWrite(const wchar_t* temporaryPath, void* rawContext, DWORD* errorOut)
    {
        auto* context = static_cast<CurvePresetSaveContext*>(rawContext);
        const KeyDeadzone& ks = context->curve;
        bool ok = WritePrivateProfileStringW(L"HallJoyPersistence", L"SchemaVersion", L"1", temporaryPath) != FALSE;
        ok &= WritePrivateProfileStringW(L"HallJoyPersistence", L"Kind", L"CurvePreset", temporaryPath) != FALSE;
        ok &= WritePrivateProfileStringW(L"Curve", nullptr, nullptr, temporaryPath) != FALSE;
        ok &= WriteM01(L"Curve", L"Low", ks.low, temporaryPath);
        ok &= WriteM01(L"Curve", L"High", ks.high, temporaryPath);
        ok &= WriteM01(L"Curve", L"AntiDeadzone", ks.antiDeadzone, temporaryPath);
        ok &= WriteM01(L"Curve", L"OutputCap", ks.outputCap, temporaryPath);
        ok &= WriteM01(L"Curve", L"Cp1X", ks.cp1_x, temporaryPath);
        ok &= WriteM01(L"Curve", L"Cp1Y", ks.cp1_y, temporaryPath);
        ok &= WriteM01(L"Curve", L"Cp2X", ks.cp2_x, temporaryPath);
        ok &= WriteM01(L"Curve", L"Cp2Y", ks.cp2_y, temporaryPath);
        ok &= WriteM01(L"Curve", L"Cp1W", ks.cp1_w, temporaryPath);
        ok &= WriteM01(L"Curve", L"Cp2W", ks.cp2_w, temporaryPath);
        ok &= WriteI(L"Curve", L"Mode", ks.curveMode == 0 ? 0 : 1, temporaryPath);
        ok &= WriteI(L"Curve", L"Invert", ks.invert ? 1 : 0, temporaryPath);
        if (!ok && errorOut)
        {
            const DWORD error = GetLastError();
            *errorOut = error != ERROR_SUCCESS ? error : ERROR_WRITE_FAULT;
        }
        return ok;
    }

    bool ReadIniValueEquals(const wchar_t* path, const wchar_t* section, const wchar_t* key, const std::wstring& expected)
    {
        wchar_t value[128]{};
        GetPrivateProfileStringW(section, key, L"{missing}", value, (DWORD)_countof(value), path);
        return expected == value;
    }

    bool CurvePresetTransactionValidate(const wchar_t* temporaryPath, void* rawContext, DWORD* errorOut)
    {
        auto* context = static_cast<CurvePresetSaveContext*>(rawContext);
        const KeyDeadzone& ks = context->curve;
        auto milli = [](float value)
        {
            return std::to_wstring((int)lroundf(ClampF(value, 0.0f, 1.0f) * 1000.0f));
        };

        bool ok = ReadIniValueEquals(temporaryPath, L"HallJoyPersistence", L"SchemaVersion", L"1") &&
            ReadIniValueEquals(temporaryPath, L"HallJoyPersistence", L"Kind", L"CurvePreset");
        ok &= ReadIniValueEquals(temporaryPath, L"Curve", L"Low", milli(ks.low));
        ok &= ReadIniValueEquals(temporaryPath, L"Curve", L"High", milli(ks.high));
        ok &= ReadIniValueEquals(temporaryPath, L"Curve", L"AntiDeadzone", milli(ks.antiDeadzone));
        ok &= ReadIniValueEquals(temporaryPath, L"Curve", L"OutputCap", milli(ks.outputCap));
        ok &= ReadIniValueEquals(temporaryPath, L"Curve", L"Cp1X", milli(ks.cp1_x));
        ok &= ReadIniValueEquals(temporaryPath, L"Curve", L"Cp1Y", milli(ks.cp1_y));
        ok &= ReadIniValueEquals(temporaryPath, L"Curve", L"Cp2X", milli(ks.cp2_x));
        ok &= ReadIniValueEquals(temporaryPath, L"Curve", L"Cp2Y", milli(ks.cp2_y));
        ok &= ReadIniValueEquals(temporaryPath, L"Curve", L"Cp1W", milli(ks.cp1_w));
        ok &= ReadIniValueEquals(temporaryPath, L"Curve", L"Cp2W", milli(ks.cp2_w));
        ok &= ReadIniValueEquals(temporaryPath, L"Curve", L"Mode", std::to_wstring(ks.curveMode == 0 ? 0 : 1));
        ok &= ReadIniValueEquals(temporaryPath, L"Curve", L"Invert", std::to_wstring(ks.invert ? 1 : 0));
        if (!ok && errorOut) *errorOut = ERROR_INVALID_DATA;
        return ok;
    }
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
            const std::wstring leftKey = FileNamePolicy_CanonicalKey(a.name);
            const std::wstring rightKey = FileNamePolicy_CanonicalKey(b.name);
            if (leftKey != rightKey) return leftKey < rightKey;
            return _wcsicmp(a.path.c_str(), b.path.c_str()) < 0;
            });
        outList.erase(std::unique(outList.begin(), outList.end(), [](const ProfileInfo& a, const ProfileInfo& b) {
            return FileNamePolicy_Equivalent(a.name, b.name);
            }), outList.end());

        int activeIdx = -1;
        if (!g_activeName.empty())
        {
            for (int i = 0; i < (int)outList.size(); ++i)
            {
                if (FileNamePolicy_Equivalent(outList[i].name, g_activeName))
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

        CurvePresetSaveContext context{ NormalizePreset(inKs) };
        const auto result = IniUtil_SaveAtomic(
            path.c_str(),
            CurvePresetTransactionWrite,
            CurvePresetTransactionValidate,
            &context);
        if (!result.Succeeded())
        {
            IniUtil_ReportSaveFailure(L"curve preset", path.c_str(), result);
            return false;
        }

        // Update "active preset" state (saving means "this preset is now current")
        const std::wstring previousName = g_activeName;
        const std::wstring previousPath = g_activePath;
        const bool previousDirty = g_dirty;
        fs::path pp(path);
        g_activeName = FileNamePolicy_NormalizeStem(pp.stem().wstring());
        g_activePath = path;
        g_dirty = false;
        if (!SaveState())
        {
            g_activeName = previousName;
            g_activePath = previousPath;
            g_dirty = previousDirty;
            return false;
        }

        return true;
    }

    bool CreatePreset(const std::wstring& name, const KeyDeadzone& ks)
    {
        LoadStateOnce();

        std::wstring safeName = FileNamePolicy_NormalizeStem(name);
        if (safeName.empty()) return false;

        std::wstring dir = GetPresetsDir();
        if (!EnsureDirExists(dir)) return false;

        std::wstring path;
        if (!FileNamePolicy_MakeUniqueChildPath(dir, safeName, L".ini", safeName, path))
            return false;

        if (SavePreset(path, ks))
            return true;
        DeleteFileW(path.c_str());
        return false;
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
            if (FileNamePolicy_Equivalent(name, g_activeName))
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

    bool SetActiveProfileName(const std::wstring& name)
    {
        LoadStateOnce();

        const std::wstring normalizedName = name.empty() ? L"" : FileNamePolicy_NormalizeStem(name);
        if (!name.empty() && normalizedName.empty()) return false;

        if ((g_activeName.empty() && normalizedName.empty()) || FileNamePolicy_Equivalent(g_activeName, normalizedName))
            return true;

        const std::wstring previousName = g_activeName;
        const std::wstring previousPath = g_activePath;
        g_activeName = normalizedName;

        // IMPORTANT: path is not persisted; force re-resolve on next RefreshList()
        g_activePath.clear();

        if (!SaveState())
        {
            g_activeName = previousName;
            g_activePath = previousPath;
            return false;
        }
        return true;
    }

#if defined(HALLJOY_ANALOG_SIMULATOR)
    bool TestSaveStateToPath(const std::wstring& path, const std::wstring& activeName)
    {
        return SaveStateToPath(path, activeName);
    }
#endif

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

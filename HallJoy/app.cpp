// app.cpp
#ifndef _WIN32_IE
#define _WIN32_IE 0x0600
#endif

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <dbt.h>
#include <commctrl.h>
#include <shellapi.h>
#include <urlmon.h>
#include <winhttp.h>
#include <hidusage.h>

#include <string>
#include <vector>
#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <regex>
#include <cwctype>
#include <limits>
#include <atomic>

#include "app.h"
#include "Resource.h"
#include "backend.h"
#include "bindings.h"
#include "keyboard_ui.h"
#include "settings.h"
#include "settings_ini.h"
#include "global_profiles.h"
#include "realtime_loop.h"
#include "win_util.h"
#include "app_paths.h"
#include "ui_theme.h"
#include "debug_log.h"
#include "mouse_ipc.h"
#include "mouse_bind_codes.h"

#pragma comment(lib, "Comctl32.lib")
#pragma comment(lib, "Urlmon.lib")
#pragma comment(lib, "Winhttp.lib")

static constexpr UINT WM_APP_REQUEST_SAVE = WM_APP + 1;
static constexpr UINT WM_APP_APPLY_TIMING = WM_APP + 2;
static constexpr UINT WM_APP_KEYBOARD_LAYOUT_CHANGED = WM_APP + 260;

// UI refresh timer
static const UINT_PTR UI_TIMER_ID = 2;

// Debounced settings save timer
static const UINT_PTR SETTINGS_SAVE_TIMER_ID = 3;
static const UINT SETTINGS_SAVE_TIMER_MS = 350;

static HWND g_hPageMain = nullptr;
static HWND g_hMainWnd = nullptr;
static HHOOK g_hKeyboardHook = nullptr;
static HHOOK g_hMouseHook = nullptr;
static bool g_backendReady = false;
static bool g_digitalFallbackWarnShown = false;
static std::atomic<bool> g_mouseBlockPauseByRShift{ false };
static bool g_mouseCursorLocked = false;
static POINT g_mouseCursorLockPos{};

static void SaveSettingsByActiveGlobalProfile()
{
    const std::wstring& active = GlobalProfiles_GetActiveName();
    if (GlobalProfiles_IsDefault(active))
    {
        SettingsIni_Save(AppPaths_SettingsIni().c_str());
        return;
    }

    // IMPORTANT:
    // When non-default profile is active, do NOT overwrite base settings.ini with
    // runtime values from that profile, otherwise "Default" profile gets polluted.
    // Keep only active profile marker in base file.
    GlobalProfiles_SaveActiveToSettingsIni(AppPaths_SettingsIni().c_str());

    // Active profile stores all runtime settings except layout/window.
    std::wstring profileSettingsPath = AppPaths_ActiveSettingsIni();
    SettingsIni_SaveProfile(profileSettingsPath.c_str());
}

static bool IsWindowRectVisibleOnAnyScreen(int x, int y, int w, int h)
{
    RECT r{ x, y, x + w, y + h };
    RECT vr{};
    vr.left = GetSystemMetrics(SM_XVIRTUALSCREEN);
    vr.top = GetSystemMetrics(SM_YVIRTUALSCREEN);
    vr.right = vr.left + GetSystemMetrics(SM_CXVIRTUALSCREEN);
    vr.bottom = vr.top + GetSystemMetrics(SM_CYVIRTUALSCREEN);
    RECT inter{};
    return IntersectRect(&inter, &r, &vr) != FALSE;
}

static std::wstring Utf8ToWide(const std::string& s)
{
    if (s.empty()) return {};
    int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
    if (n <= 0) return {};
    std::wstring out((size_t)n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), out.data(), n);
    return out;
}

static std::string WideToUtf8(const std::wstring& s)
{
    if (s.empty()) return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0, nullptr, nullptr);
    if (n <= 0) return {};
    std::string out((size_t)n, '\0');
    WideCharToMultiByte(CP_UTF8, 0, s.data(), (int)s.size(), out.data(), n, nullptr, nullptr);
    return out;
}

static std::wstring ToLowerCopy(std::wstring s)
{
    std::transform(s.begin(), s.end(), s.begin(), [](wchar_t c) { return (wchar_t)towlower(c); });
    return s;
}

static bool EndsWithNoCase(const std::wstring& s, const std::wstring& suffix)
{
    if (suffix.size() > s.size()) return false;
    std::wstring ls = ToLowerCopy(s);
    std::wstring lf = ToLowerCopy(suffix);
    return ls.compare(ls.size() - lf.size(), lf.size(), lf) == 0;
}

static std::wstring FileNameFromUrl(const std::wstring& url)
{
    size_t slash = url.find_last_of(L'/');
    size_t start = (slash == std::wstring::npos) ? 0 : (slash + 1);
    size_t end = url.find_first_of(L"?#", start);
    if (end == std::wstring::npos) end = url.size();
    if (start >= end) return L"download.bin";
    return url.substr(start, end - start);
}

static std::wstring JsonUnescapeBasic(std::wstring s)
{
    size_t p = 0;
    while ((p = s.find(L"\\/", p)) != std::wstring::npos)
    {
        s.replace(p, 2, L"/");
    }
    p = 0;
    while ((p = s.find(L"\\u0026", p)) != std::wstring::npos)
    {
        s.replace(p, 6, L"&");
    }
    p = 0;
    while ((p = s.find(L"\\u003d", p)) != std::wstring::npos)
    {
        s.replace(p, 6, L"=");
    }
    return s;
}

static bool HttpGetUtf8(const std::wstring& url, std::string& outBody)
{
    outBody.clear();
    DebugLog_Write(L"[deps.http] GET %s", url.c_str());

    URL_COMPONENTSW uc{};
    uc.dwStructSize = sizeof(uc);
    uc.dwSchemeLength = (DWORD)-1;
    uc.dwHostNameLength = (DWORD)-1;
    uc.dwUrlPathLength = (DWORD)-1;
    uc.dwExtraInfoLength = (DWORD)-1;
    if (!WinHttpCrackUrl(url.c_str(), 0, 0, &uc))
    {
        DebugLog_Write(L"[deps.http] WinHttpCrackUrl failed err=%lu", GetLastError());
        return false;
    }

    std::wstring host(uc.lpszHostName, uc.dwHostNameLength);
    std::wstring path = (uc.dwUrlPathLength > 0) ? std::wstring(uc.lpszUrlPath, uc.dwUrlPathLength) : L"/";
    if (uc.dwExtraInfoLength > 0)
        path.append(uc.lpszExtraInfo, uc.dwExtraInfoLength);

    HINTERNET hSession = WinHttpOpen(L"HallJoy/1.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS,
        0);
    if (!hSession) { DebugLog_Write(L"[deps.http] WinHttpOpen failed err=%lu", GetLastError()); return false; }

    HINTERNET hConnect = WinHttpConnect(hSession, host.c_str(), uc.nPort, 0);
    if (!hConnect)
    {
        DebugLog_Write(L"[deps.http] WinHttpConnect failed err=%lu", GetLastError());
        WinHttpCloseHandle(hSession);
        return false;
    }

    DWORD flags = (uc.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET hReq = WinHttpOpenRequest(hConnect, L"GET", path.c_str(), nullptr,
        WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
    if (!hReq)
    {
        DebugLog_Write(L"[deps.http] WinHttpOpenRequest failed err=%lu", GetLastError());
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return false;
    }

    const wchar_t* headers = L"User-Agent: HallJoy\r\nAccept: application/vnd.github+json\r\n";
    bool ok = WinHttpSendRequest(hReq, headers, (DWORD)-1L, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) != FALSE;
    if (ok) ok = WinHttpReceiveResponse(hReq, nullptr) != FALSE;

    DWORD statusCode = 0;
    DWORD statusSize = sizeof(statusCode);
    if (ok)
    {
        ok = WinHttpQueryHeaders(hReq,
            WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
            WINHTTP_HEADER_NAME_BY_INDEX,
            &statusCode,
            &statusSize,
            WINHTTP_NO_HEADER_INDEX) != FALSE;
    }
    if (ok && statusCode >= 400)
    {
        DebugLog_Write(L"[deps.http] status=%lu", statusCode);
        ok = false;
    }

    while (ok)
    {
        DWORD avail = 0;
        if (!WinHttpQueryDataAvailable(hReq, &avail))
        {
            ok = false;
            break;
        }
        if (avail == 0) break;

        size_t oldSize = outBody.size();
        outBody.resize(oldSize + (size_t)avail);
        DWORD got = 0;
        if (!WinHttpReadData(hReq, outBody.data() + oldSize, avail, &got))
        {
            ok = false;
            break;
        }
        outBody.resize(oldSize + (size_t)got);
    }

    WinHttpCloseHandle(hReq);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    DebugLog_Write(L"[deps.http] done ok=%d bytes=%u", ok ? 1 : 0, (unsigned)outBody.size());
    return ok;
}

static std::vector<std::wstring> ExtractBrowserDownloadUrls(const std::string& jsonUtf8)
{
    std::vector<std::wstring> out;
    static const std::regex re("\"browser_download_url\"\\s*:\\s*\"([^\"]+)\"");
    std::sregex_iterator it(jsonUtf8.begin(), jsonUtf8.end(), re);
    std::sregex_iterator end;
    for (; it != end; ++it)
    {
        std::wstring url = Utf8ToWide((*it)[1].str());
        if (!url.empty())
            out.push_back(JsonUnescapeBasic(url));
    }
    return out;
}

static bool ContainsAllTokens(const std::wstring& lowerText, const std::vector<std::wstring>& tokens)
{
    for (const auto& t : tokens)
    {
        if (t.empty()) continue;
        if (lowerText.find(ToLowerCopy(t)) == std::wstring::npos)
            return false;
    }
    return true;
}

static int ScoreAssetName(const std::wstring& lowerName, const std::vector<std::wstring>& preferredTokens)
{
    int score = 0;
    for (const auto& t : preferredTokens)
    {
        if (!t.empty() && lowerName.find(ToLowerCopy(t)) != std::wstring::npos)
            score += 4;
    }
    if (EndsWithNoCase(lowerName, L".exe")) score += 2;
    if (EndsWithNoCase(lowerName, L".msi")) score += 1;
    return score;
}

static std::wstring SelectBestAssetUrl(
    const std::vector<std::wstring>& urls,
    const std::vector<std::wstring>& requiredTokens,
    const std::vector<std::wstring>& preferredTokens,
    const std::vector<std::wstring>& allowedExtensions)
{
    int bestScore = -1;
    std::wstring best;

    for (const auto& url : urls)
    {
        std::wstring name = ToLowerCopy(FileNameFromUrl(url));
        bool extOk = allowedExtensions.empty();
        for (const auto& ext : allowedExtensions)
        {
            if (EndsWithNoCase(name, ext))
            {
                extOk = true;
                break;
            }
        }
        if (!extOk) continue;
        if (!ContainsAllTokens(name, requiredTokens)) continue;

        int score = ScoreAssetName(name, preferredTokens);
        if (score > bestScore)
        {
            bestScore = score;
            best = url;
        }
    }

    if (!best.empty()) return best;

    // Fallback: first file with matching extension
    for (const auto& url : urls)
    {
        std::wstring name = ToLowerCopy(FileNameFromUrl(url));
        for (const auto& ext : allowedExtensions)
        {
            if (EndsWithNoCase(name, ext))
                return url;
        }
    }

    return {};
}

static bool ResolveLatestAssetUrl(
    const std::vector<std::wstring>& apiUrls,
    const std::vector<std::wstring>& requiredTokens,
    const std::vector<std::wstring>& preferredTokens,
    const std::vector<std::wstring>& allowedExtensions,
    std::wstring& outUrl)
{
    outUrl.clear();
    for (const auto& api : apiUrls)
    {
        std::string body;
        if (!HttpGetUtf8(api, body))
            continue;
        auto urls = ExtractBrowserDownloadUrls(body);
        if (urls.empty())
            continue;
        std::wstring pick = SelectBestAssetUrl(urls, requiredTokens, preferredTokens, allowedExtensions);
        if (!pick.empty())
        {
            outUrl = pick;
            return true;
        }
    }
    return false;
}

static std::wstring BuildTempInstallerPath(const std::wstring& fileName)
{
    namespace fs = std::filesystem;
    try
    {
        fs::path dir = fs::temp_directory_path() / L"HallJoy" / L"deps";
        std::error_code ec;
        fs::create_directories(dir, ec);
        fs::path p = dir / (fileName.empty() ? L"download.bin" : fileName);
        return p.wstring();
    }
    catch (...)
    {
        wchar_t tmp[MAX_PATH]{};
        DWORD n = GetTempPathW((DWORD)_countof(tmp), tmp);
        std::wstring base = (n > 0 && n < _countof(tmp)) ? std::wstring(tmp) : L".\\";
        if (!base.empty() && base.back() != L'\\' && base.back() != L'/')
            base.push_back(L'\\');
        return base + (fileName.empty() ? L"download.bin" : fileName);
    }
}

static bool DownloadUrlToFilePath(const std::wstring& url, const std::wstring& filePath)
{
    HRESULT hr = URLDownloadToFileW(nullptr, url.c_str(), filePath.c_str(), 0, nullptr);
    return SUCCEEDED(hr);
}

static bool DownloadLatestAssetToTemp(
    const std::vector<std::wstring>& apiUrls,
    const std::vector<std::wstring>& requiredTokens,
    const std::vector<std::wstring>& preferredTokens,
    const std::vector<std::wstring>& allowedExtensions,
    std::wstring& outFilePath)
{
    outFilePath.clear();
    std::wstring assetUrl;
    if (!ResolveLatestAssetUrl(apiUrls, requiredTokens, preferredTokens, allowedExtensions, assetUrl))
    {
        DebugLog_Write(L"[deps] failed resolve latest asset url");
        return false;
    }

    std::wstring fileName = FileNameFromUrl(assetUrl);
    std::wstring dst = BuildTempInstallerPath(fileName);
    DebugLog_Write(L"[deps] download asset=%s -> %s", assetUrl.c_str(), dst.c_str());
    if (!DownloadUrlToFilePath(assetUrl, dst))
    {
        DebugLog_Write(L"[deps] URLDownloadToFile failed");
        return false;
    }
    outFilePath = dst;
    return true;
}

static std::wstring QuoteForCmdArg(const std::wstring& s)
{
    std::wstring out = L"\"";
    for (wchar_t c : s)
    {
        if (c == L'"') out += L"\\\"";
        else out += c;
    }
    out += L"\"";
    return out;
}

static bool RunCommandElevatedAndWait(HWND hwnd, const std::wstring& file, const std::wstring& params)
{
    DebugLog_Write(L"[deps.exec] runas file=%s params=%s", file.c_str(), params.c_str());
    SHELLEXECUTEINFOW sei{};
    sei.cbSize = sizeof(sei);
    sei.fMask = SEE_MASK_NOCLOSEPROCESS;
    sei.hwnd = hwnd;
    sei.lpVerb = L"runas";
    sei.lpFile = file.c_str();
    sei.lpParameters = params.empty() ? nullptr : params.c_str();
    sei.nShow = SW_SHOWNORMAL;

    if (!ShellExecuteExW(&sei))
    {
        DebugLog_Write(L"[deps.exec] ShellExecuteEx failed err=%lu", GetLastError());
        return false;
    }

    if (sei.hProcess)
    {
        WaitForSingleObject(sei.hProcess, INFINITE);
        DWORD code = 1;
        GetExitCodeProcess(sei.hProcess, &code);
        CloseHandle(sei.hProcess);
        DebugLog_Write(L"[deps.exec] exit_code=%lu", code);
        return (code == 0 || code == 3010 || code == 1641);
    }
    return true;
}

static bool RunInstallerElevatedAndWait(HWND hwnd, const std::wstring& installerPath)
{
    if (EndsWithNoCase(installerPath, L".msi"))
    {
        std::wstring params = L"/i " + QuoteForCmdArg(installerPath);
        return RunCommandElevatedAndWait(hwnd, L"msiexec.exe", params);
    }
    return RunCommandElevatedAndWait(hwnd, installerPath, L"");
}

static std::wstring QuoteForPowerShellSingle(const std::wstring& s)
{
    std::wstring out = L"'";
    for (wchar_t c : s)
    {
        if (c == L'\'') out += L"''";
        else out += c;
    }
    out += L"'";
    return out;
}

static bool WriteTextFileUtf8(const std::wstring& path, const std::wstring& text)
{
    std::string utf8 = WideToUtf8(text);
    HANDLE h = CreateFileW(path.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE)
        return false;

    DWORD written = 0;
    bool ok = WriteFile(h, utf8.data(), (DWORD)utf8.size(), &written, nullptr) != FALSE;
    CloseHandle(h);
    return ok && written == (DWORD)utf8.size();
}

static bool RunPowerShellScriptElevatedAndWait(HWND hwnd, const std::wstring& scriptText)
{
    std::wstring scriptPath = BuildTempInstallerPath(L"halljoy_install_uap.ps1");
    if (!WriteTextFileUtf8(scriptPath, scriptText))
        return false;

    std::wstring params = L"-NoProfile -ExecutionPolicy Bypass -File " + QuoteForCmdArg(scriptPath);
    return RunCommandElevatedAndWait(hwnd, L"powershell.exe", params);
}

static bool InstallUniversalAnalogPluginFromZip(HWND hwnd, const std::wstring& zipPath)
{
    std::wstring extractDir = BuildTempInstallerPath(L"uap_extract");
    std::wstring dstDir = L"C:\\Program Files\\WootingAnalogPlugins";

    std::wstring script;
    script += L"$ErrorActionPreference='Stop'\n";
    script += L"$zip=" + QuoteForPowerShellSingle(zipPath) + L"\n";
    script += L"$extract=" + QuoteForPowerShellSingle(extractDir) + L"\n";
    script += L"$dst=" + QuoteForPowerShellSingle(dstDir) + L"\n";
    script += L"if (Test-Path -LiteralPath $extract) { Remove-Item -LiteralPath $extract -Recurse -Force }\n";
    script += L"Expand-Archive -LiteralPath $zip -DestinationPath $extract -Force\n";
    script += L"New-Item -ItemType Directory -Path $dst -Force | Out-Null\n";
    script += L"$srcClassic = Join-Path $extract 'universal-analog-plugin'\n";
    script += L"$srcWooting = Join-Path $extract 'universal-analog-plugin-with-wooting-device-support'\n";
    script += L"if (Test-Path -LiteralPath $srcClassic) {\n";
    script += L"  Copy-Item -LiteralPath $srcClassic -Destination $dst -Recurse -Force\n";
    script += L"} elseif (Test-Path -LiteralPath $srcWooting) {\n";
    script += L"  Copy-Item -LiteralPath $srcWooting -Destination $dst -Recurse -Force\n";
    script += L"} else {\n";
    script += L"  throw 'No supported plugin folders found in Windows.zip'\n";
    script += L"}\n";

    return RunPowerShellScriptElevatedAndWait(hwnd, script);
}

static std::wstring BuildIssuesText(uint32_t issues)
{
    std::wstring t;
    if (issues & BackendInitIssue_VigemBusMissing)
        t += L"- ViGEm Bus is missing.\n";
    if (issues & BackendInitIssue_WootingSdkMissing)
        t += L"- Wooting Analog SDK is missing.\n";
    if (issues & BackendInitIssue_WootingIncompatible)
        t += L"- Wooting Analog SDK version is incompatible.\n";
    if (issues & BackendInitIssue_WootingNoPlugins)
        t += L"- No Wooting analog plugins are installed.\n";
    if (issues & BackendInitIssue_Unknown)
        t += L"- Unknown backend initialization issue.\n";
    if (t.empty())
        t = L"- Unknown backend initialization issue.\n";
    return t;
}

enum class DependencyInstallResult
{
    Skipped = 0,   // not needed or user canceled
    Installed = 1, // installers ran successfully
    Failed = 2,    // attempted but failed
};

static DependencyInstallResult TryInstallMissingDependencies(HWND hwnd, uint32_t issues)
{
    DebugLog_Write(L"[deps] begin install flow issues=0x%08X", issues);
    const bool needVigem = (issues & BackendInitIssue_VigemBusMissing) != 0;
    const bool needWootingSdk = (issues & (BackendInitIssue_WootingSdkMissing |
                                           BackendInitIssue_WootingIncompatible |
                                           BackendInitIssue_WootingNoPlugins)) != 0;
    const bool suggestUap = (issues & (BackendInitIssue_WootingSdkMissing |
                                       BackendInitIssue_WootingIncompatible |
                                       BackendInitIssue_WootingNoPlugins)) != 0;

    if (!needVigem && !needWootingSdk)
        return DependencyInstallResult::Skipped;

    std::wstring prompt = L"Missing dependencies detected:\n\n";
    prompt += BuildIssuesText(issues);
    prompt += L"\nDownload and run the latest installers from GitHub now?";
    if (MessageBoxW(hwnd, prompt.c_str(), L"HallJoy", MB_ICONQUESTION | MB_YESNO) != IDYES)
    {
        DebugLog_Write(L"[deps] user declined install");
        return DependencyInstallResult::Skipped;
    }

    if (needVigem)
    {
        std::wstring installerPath;
        if (!DownloadLatestAssetToTemp(
            { L"https://api.github.com/repos/ViGEm/ViGEmBus/releases/latest",
              L"https://api.github.com/repos/nefarius/ViGEmBus/releases/latest" },
            { L"vigem", L"bus" },
            { L"x64", L"setup", L"installer" },
            { L".exe", L".msi" },
            installerPath))
        {
            MessageBoxW(hwnd, L"Failed to download latest ViGEm Bus installer from GitHub.", L"HallJoy", MB_ICONERROR);
            DebugLog_Write(L"[deps] ViGEm installer download failed");
            return DependencyInstallResult::Failed;
        }

        if (!RunInstallerElevatedAndWait(hwnd, installerPath))
        {
            MessageBoxW(hwnd, L"ViGEm Bus installation did not complete successfully.", L"HallJoy", MB_ICONERROR);
            DebugLog_Write(L"[deps] ViGEm installer failed");
            return DependencyInstallResult::Failed;
        }
    }

    if (needWootingSdk)
    {
        std::wstring installerPath;
        if (!DownloadLatestAssetToTemp(
            { L"https://api.github.com/repos/WootingKb/wooting-analog-sdk/releases/latest" },
            { L"wooting", L"analog", L"sdk" },
            { L"x86_64", L"windows", L"msi" },
            { L".msi", L".exe" },
            installerPath))
        {
            MessageBoxW(hwnd, L"Failed to download latest Wooting Analog SDK installer from GitHub.", L"HallJoy", MB_ICONERROR);
            DebugLog_Write(L"[deps] Wooting SDK installer download failed");
            return DependencyInstallResult::Failed;
        }

        if (!RunInstallerElevatedAndWait(hwnd, installerPath))
        {
            MessageBoxW(hwnd, L"Wooting Analog SDK installation did not complete successfully.", L"HallJoy", MB_ICONERROR);
            DebugLog_Write(L"[deps] Wooting SDK installer failed");
            return DependencyInstallResult::Failed;
        }
    }

    if (suggestUap)
    {
        const int wantUap = MessageBoxW(
            hwnd,
            L"Install optional Universal Analog Plugin for broader HE keyboard support?",
            L"HallJoy",
            MB_ICONQUESTION | MB_YESNO);
        if (wantUap == IDYES)
        {
            std::wstring zipPath;
            if (!DownloadLatestAssetToTemp(
                { L"https://api.github.com/repos/AnalogSense/universal-analog-plugin/releases/latest" },
                { L"windows" },
                { L"windows", L"zip" },
                { L".zip" },
                zipPath))
            {
                MessageBoxW(hwnd, L"Failed to download Universal Analog Plugin (Windows.zip).", L"HallJoy", MB_ICONWARNING);
            }
            else if (!InstallUniversalAnalogPluginFromZip(hwnd, zipPath))
            {
                MessageBoxW(hwnd, L"Universal Analog Plugin installation failed. You can install it manually later.", L"HallJoy", MB_ICONWARNING);
            }
        }
    }

    return DependencyInstallResult::Installed;
}

static bool RelaunchSelf()
{
    wchar_t exePath[MAX_PATH]{};
    DWORD n = GetModuleFileNameW(nullptr, exePath, (DWORD)_countof(exePath));
    if (n == 0 || n >= _countof(exePath))
    {
        DebugLog_Write(L"[relaunch] GetModuleFileName failed err=%lu", GetLastError());
        return false;
    }

    std::wstring workDir(exePath);
    size_t slash = workDir.find_last_of(L"\\/");
    if (slash != std::wstring::npos)
        workDir.resize(slash);

    std::wstring cmdLine = L"\"";
    cmdLine += exePath;
    cmdLine += L"\"";
    std::vector<wchar_t> mutableCmd(cmdLine.begin(), cmdLine.end());
    mutableCmd.push_back(L'\0');

    STARTUPINFOW si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};
    if (CreateProcessW(
        exePath,
        mutableCmd.data(),
        nullptr,
        nullptr,
        FALSE,
        0,
        nullptr,
        workDir.empty() ? nullptr : workDir.c_str(),
        &si,
        &pi))
    {
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
        DebugLog_Write(L"[relaunch] CreateProcess success exe=%s", exePath);
        return true;
    }
    DebugLog_Write(L"[relaunch] CreateProcess failed err=%lu", GetLastError());

    HINSTANCE h = ShellExecuteW(nullptr, L"open", exePath, nullptr, workDir.empty() ? nullptr : workDir.c_str(), SW_SHOWNORMAL);
    DebugLog_Write(L"[relaunch] ShellExecute result=%p", h);
    return ((INT_PTR)h > 32);
}

static bool IsOwnForegroundWindow()
{
    HWND fg = GetForegroundWindow();
    if (!fg) return false;

    HWND root = GetAncestor(fg, GA_ROOT);
    if (!root) root = fg;

    wchar_t cls[128]{};
    GetClassNameW(root, cls, (int)_countof(cls));
    return (_wcsicmp(cls, L"WootingVigemGui") == 0 ||
        _wcsicmp(cls, L"KeyboardLayoutEditorHost") == 0);
}

static bool IsMouseBlockingActiveNow()
{
    if (!Settings_GetBlockMouseInput()) return false;
    if (!Settings_GetMouseToStickEnabled()) return false;
    if (IsOwnForegroundWindow()) return false;
    if (g_mouseBlockPauseByRShift.load(std::memory_order_relaxed)) return false;
    return true;
}

static void PublishMouseIpcState()
{
    bool mts = Settings_GetMouseToStickEnabled();
    bool blockWanted = Settings_GetBlockMouseInput() && mts;
    bool active = IsMouseBlockingActiveNow();
    bool pause = g_mouseBlockPauseByRShift.load(std::memory_order_relaxed);
    MouseIpc_PublishState(blockWanted, active, mts, pause);
}

static void UpdateMouseCursorLockState(bool blockNow)
{
    if (!blockNow)
    {
        g_mouseCursorLocked = false;
        return;
    }

    if (!g_mouseCursorLocked)
    {
        GetCursorPos(&g_mouseCursorLockPos);
        g_mouseCursorLocked = true;
    }
}

static uint16_t HidFromKeyboardScanCode(DWORD scanCode, bool extended, DWORD vkCode)
{
    switch (scanCode & 0xFFu)
    {
    case 0x01: return 41; // Esc
    case 0x02: return 30; // 1
    case 0x03: return 31; // 2
    case 0x04: return 32; // 3
    case 0x05: return 33; // 4
    case 0x06: return 34; // 5
    case 0x07: return 35; // 6
    case 0x08: return 36; // 7
    case 0x09: return 37; // 8
    case 0x0A: return 38; // 9
    case 0x0B: return 39; // 0
    case 0x0C: return 45; // -
    case 0x0D: return 46; // =
    case 0x0E: return 42; // Backspace
    case 0x0F: return 43; // Tab
    case 0x10: return 20; // Q
    case 0x11: return 26; // W
    case 0x12: return 8;  // E
    case 0x13: return 21; // R
    case 0x14: return 23; // T
    case 0x15: return 28; // Y
    case 0x16: return 24; // U
    case 0x17: return 12; // I
    case 0x18: return 18; // O
    case 0x19: return 19; // P
    case 0x1A: return 47; // [
    case 0x1B: return 48; // ]
    case 0x1C: return extended ? 88 : 40; // Enter / Numpad Enter
    case 0x1D: return extended ? 228 : 224; // RCtrl / LCtrl
    case 0x1E: return 4;  // A
    case 0x1F: return 22; // S
    case 0x20: return 7;  // D
    case 0x21: return 9;  // F
    case 0x22: return 10; // G
    case 0x23: return 11; // H
    case 0x24: return 13; // J
    case 0x25: return 14; // K
    case 0x26: return 15; // L
    case 0x27: return 51; // ;
    case 0x28: return 52; // '
    case 0x29: return 53; // `
    case 0x2A: return 225; // LShift
    case 0x2B: return 49; // Backslash
    case 0x2C: return 29; // Z
    case 0x2D: return 27; // X
    case 0x2E: return 6;  // C
    case 0x2F: return 25; // V
    case 0x30: return 5;  // B
    case 0x31: return 17; // N
    case 0x32: return 16; // M
    case 0x33: return 54; // ,
    case 0x34: return 55; // .
    case 0x35: return extended ? 84 : 56; // Numpad / or /
    case 0x36: return 229; // RShift
    case 0x37: return extended ? 70 : 85; // PrintScreen / Numpad *
    case 0x38: return extended ? 230 : 226; // RAlt / LAlt
    case 0x39: return 44; // Space
    case 0x3A: return 57; // CapsLock
    case 0x3B: return 58; // F1
    case 0x3C: return 59; // F2
    case 0x3D: return 60; // F3
    case 0x3E: return 61; // F4
    case 0x3F: return 62; // F5
    case 0x40: return 63; // F6
    case 0x41: return 64; // F7
    case 0x42: return 65; // F8
    case 0x43: return 66; // F9
    case 0x44: return 67; // F10
    case 0x45: return 83; // NumLock
    case 0x46: return 71; // ScrollLock
    case 0x47: return extended ? 74 : 95; // Home / Numpad 7
    case 0x48: return extended ? 82 : 96; // Up / Numpad 8
    case 0x49: return extended ? 75 : 97; // PgUp / Numpad 9
    case 0x4A: return 86; // Numpad -
    case 0x4B: return extended ? 80 : 92; // Left / Numpad 4
    case 0x4C: return 93; // Numpad 5
    case 0x4D: return extended ? 79 : 94; // Right / Numpad 6
    case 0x4E: return 87; // Numpad +
    case 0x4F: return extended ? 77 : 89; // End / Numpad 1
    case 0x50: return extended ? 81 : 90; // Down / Numpad 2
    case 0x51: return extended ? 78 : 91; // PgDn / Numpad 3
    case 0x52: return extended ? 73 : 98; // Insert / Numpad 0
    case 0x53: return extended ? 76 : 99; // Delete / Numpad .
    case 0x57: return 68; // F11
    case 0x58: return 69; // F12
    case 0x5B: return 227; // LWin
    case 0x5C: return 231; // RWin
    case 0x5D: return 101; // Menu/App
    default:
        break;
    }

    // Fallback for rare events with zero/unknown scan code.
    switch (vkCode)
    {
    case 'A': return 4; case 'B': return 5; case 'C': return 6; case 'D': return 7; case 'E': return 8;
    case 'F': return 9; case 'G': return 10; case 'H': return 11; case 'I': return 12; case 'J': return 13;
    case 'K': return 14; case 'L': return 15; case 'M': return 16; case 'N': return 17; case 'O': return 18;
    case 'P': return 19; case 'Q': return 20; case 'R': return 21; case 'S': return 22; case 'T': return 23;
    case 'U': return 24; case 'V': return 25; case 'W': return 26; case 'X': return 27; case 'Y': return 28;
    case 'Z': return 29;
    case '1': return 30; case '2': return 31; case '3': return 32; case '4': return 33; case '5': return 34;
    case '6': return 35; case '7': return 36; case '8': return 37; case '9': return 38; case '0': return 39;
    case VK_SPACE: return 44;
    case VK_TAB: return 43;
    case VK_RETURN: return extended ? 88 : 40;
    case VK_BACK: return 42;
    case VK_ESCAPE: return 41;
    case VK_LEFT: return 80;
    case VK_RIGHT: return 79;
    case VK_UP: return 82;
    case VK_DOWN: return 81;
    case VK_HOME: return 74;
    case VK_END: return 77;
    case VK_PRIOR: return 75;
    case VK_NEXT: return 78;
    case VK_INSERT: return 73;
    case VK_DELETE: return 76;
    default:
        return 0;
    }
}

static LRESULT CALLBACK KeyboardBlockHookProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    if (nCode == HC_ACTION && lParam)
    {
        if (wParam == WM_KEYDOWN || wParam == WM_KEYUP || wParam == WM_SYSKEYDOWN || wParam == WM_SYSKEYUP)
        {
            const KBDLLHOOKSTRUCT* k = (const KBDLLHOOKSTRUCT*)lParam;
            const bool ext = (k->flags & LLKHF_EXTENDED) != 0;
            const bool isDown = (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN);
            uint16_t hid = HidFromKeyboardScanCode(k->scanCode, ext, k->vkCode);
            Backend_NotifyKeyboardEvent(
                hid,
                (uint16_t)(k->scanCode & 0xFFFFu),
                (uint16_t)(k->vkCode & 0xFFFFu),
                isDown,
                (k->flags & LLKHF_INJECTED) != 0);

            if (hid == 229)
            {
                g_mouseBlockPauseByRShift.store(isDown, std::memory_order_relaxed);
                PublishMouseIpcState();
            }

            if (isDown && k->vkCode == VK_DELETE)
            {
                const bool ctrlDown = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
                const bool altDown = (GetAsyncKeyState(VK_MENU) & 0x8000) != 0;
                if (ctrlDown && altDown && Settings_GetMouseToStickEnabled())
                {
                    Settings_SetMouseToStickEnabled(false);
                    DebugLog_Write(L"[app] Ctrl+Alt+Del detected: Mouse->Stick disabled");
                    PublishMouseIpcState();
                    if (g_hMainWnd && IsWindow(g_hMainWnd))
                        PostMessageW(g_hMainWnd, WM_APP_REQUEST_SAVE, 0, 0);
                }
            }

            if (Settings_GetBlockBoundKeys() && (k->flags & LLKHF_INJECTED) == 0 && !IsOwnForegroundWindow())
            {
                // Right Shift must always be able to pause mouse blocking, even if bound.
                if (hid == 229 && Settings_GetBlockMouseInput() && Settings_GetMouseToStickEnabled())
                    return CallNextHookEx(g_hKeyboardHook, nCode, wParam, lParam);

                if (hid != 0 && Bindings_IsHidBound(hid))
                    return 1; // swallow key event
            }
        }
    }
    return CallNextHookEx(g_hKeyboardHook, nCode, wParam, lParam);
}

static LRESULT CALLBACK MouseBlockHookProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    if (nCode == HC_ACTION && lParam)
    {
        const MSLLHOOKSTRUCT* m = (const MSLLHOOKSTRUCT*)lParam;
        if ((m->flags & LLMHF_INJECTED) == 0)
        {
            switch (wParam)
            {
            case WM_LBUTTONDOWN: Backend_SetMouseBindButtonState(kMouseBindHidLButton, true); break;
            case WM_LBUTTONUP: Backend_SetMouseBindButtonState(kMouseBindHidLButton, false); break;
            case WM_RBUTTONDOWN: Backend_SetMouseBindButtonState(kMouseBindHidRButton, true); break;
            case WM_RBUTTONUP: Backend_SetMouseBindButtonState(kMouseBindHidRButton, false); break;
            case WM_MBUTTONDOWN: Backend_SetMouseBindButtonState(kMouseBindHidMButton, true); break;
            case WM_MBUTTONUP: Backend_SetMouseBindButtonState(kMouseBindHidMButton, false); break;
            case WM_XBUTTONDOWN:
            {
                WORD xb = HIWORD(m->mouseData);
                if (xb == XBUTTON1) Backend_SetMouseBindButtonState(kMouseBindHidX1, true);
                else if (xb == XBUTTON2) Backend_SetMouseBindButtonState(kMouseBindHidX2, true);
                break;
            }
            case WM_XBUTTONUP:
            {
                WORD xb = HIWORD(m->mouseData);
                if (xb == XBUTTON1) Backend_SetMouseBindButtonState(kMouseBindHidX1, false);
                else if (xb == XBUTTON2) Backend_SetMouseBindButtonState(kMouseBindHidX2, false);
                break;
            }
            case WM_MOUSEWHEEL:
            {
                short d = GET_WHEEL_DELTA_WPARAM(m->mouseData);
                if (d > 0) Backend_PulseMouseBindWheel(kMouseBindHidWheelUp);
                else if (d < 0) Backend_PulseMouseBindWheel(kMouseBindHidWheelDown);
                break;
            }
            default:
                break;
            }
        }

        bool blockNow = IsMouseBlockingActiveNow();
        UpdateMouseCursorLockState(blockNow);

        if ((m->flags & LLMHF_INJECTED) == 0 && blockNow)
        {
            switch (wParam)
            {
            case WM_MOUSEMOVE:
                // Keep cursor physically frozen while blocked.
                SetCursorPos(g_mouseCursorLockPos.x, g_mouseCursorLockPos.y);
                return 1;
            case WM_LBUTTONDOWN:
            case WM_LBUTTONUP:
            case WM_LBUTTONDBLCLK:
            case WM_RBUTTONDOWN:
            case WM_RBUTTONUP:
            case WM_RBUTTONDBLCLK:
            case WM_MBUTTONDOWN:
            case WM_MBUTTONUP:
            case WM_MBUTTONDBLCLK:
            case WM_XBUTTONDOWN:
            case WM_XBUTTONUP:
            case WM_XBUTTONDBLCLK:
            case WM_MOUSEWHEEL:
            case WM_MOUSEHWHEEL:
                return 1;
            default:
                break;
            }
        }
    }
    return CallNextHookEx(g_hMouseHook, nCode, wParam, lParam);
}

static void RequestSettingsSave(HWND hMainWnd)
{
    SetTimer(hMainWnd, SETTINGS_SAVE_TIMER_ID, SETTINGS_SAVE_TIMER_MS, nullptr);
}

static void ApplyTimingSettings(HWND hMainWnd)
{
    UINT pollMs = std::clamp(Settings_GetPollingMs(), 1u, 20u);
    RealtimeLoop_SetIntervalMs(pollMs);

    UINT uiMs = std::clamp(Settings_GetUIRefreshMs(), 1u, 200u);
    SetTimer(hMainWnd, UI_TIMER_ID, uiMs, nullptr);
}

static void ResizeChildren(HWND hwnd)
{
    if (!g_hPageMain) return;

    RECT rc{};
    GetClientRect(hwnd, &rc);
    int w = rc.right - rc.left;
    int h = rc.bottom - rc.top;

    SetWindowPos(g_hPageMain, nullptr, 0, 0, w, h, SWP_NOZORDER);
}

static LRESULT CALLBACK MainWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_ERASEBKGND:
        return 1;

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rc{};
        GetClientRect(hwnd, &rc);
        FillRect(hdc, &rc, UiTheme::Brush_WindowBg());
        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_CREATE:
    {
        DebugLog_Write(L"[app] WM_CREATE");
        g_mouseBlockPauseByRShift.store(false, std::memory_order_relaxed);
        g_mouseCursorLocked = false;
        UiTheme::ApplyToTopLevelWindow(hwnd);

        HINSTANCE hInst = (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE);

        // Create the main keyboard UI page directly (no top-level tabs anymore)
        g_hPageMain = KeyboardUI_CreatePage(hwnd, hInst);
        if (!g_hPageMain)
        {
            DebugLog_Write(L"[app] KeyboardUI_CreatePage failed");
            MessageBoxW(hwnd, L"Failed to create main UI page.", L"Error", MB_ICONERROR);
            return -1; // abort window creation
        }

        ResizeChildren(hwnd);
        ShowWindow(g_hPageMain, SW_SHOW);

        g_backendReady = Backend_Init();
        if (!g_backendReady)
        {
            uint32_t issues = Backend_GetLastInitIssues();
            DebugLog_Write(L"[app] Backend_Init failed issues=0x%08X", issues);
            DependencyInstallResult depRes = TryInstallMissingDependencies(hwnd, issues);
            bool backendReady = false;

            if (depRes == DependencyInstallResult::Installed)
            {
                // First try to continue in the same process after installer finished.
                backendReady = Backend_Init();
                DebugLog_Write(L"[app] Backend_Init after install result=%d issues=0x%08X", backendReady ? 1 : 0, Backend_GetLastInitIssues());
            }
            else if (depRes != DependencyInstallResult::Failed)
            {
                // User skipped/canceled install: give backend one more direct try.
                backendReady = Backend_Init();
                DebugLog_Write(L"[app] Backend_Init retry after skip result=%d issues=0x%08X", backendReady ? 1 : 0, Backend_GetLastInitIssues());
            }

            if (depRes == DependencyInstallResult::Failed || !backendReady)
            {
                DebugLog_Write(L"[app] backend not ready, continue in degraded mode");
                g_backendReady = false;
            }
            else
            {
                g_backendReady = true;
            }
        }

        if (g_backendReady)
            RealtimeLoop_Start();
        ApplyTimingSettings(hwnd);

        if (!MouseIpc_InitPublisher())
            DebugLog_Write(L"[app] mouse ipc init failed");
        PublishMouseIpcState();

        // Receive raw mouse deltas even when this window is not focused.
        RAWINPUTDEVICE rid{};
        rid.usUsagePage = HID_USAGE_PAGE_GENERIC;
        rid.usUsage = HID_USAGE_GENERIC_MOUSE;
        rid.dwFlags = RIDEV_INPUTSINK;
        rid.hwndTarget = hwnd;
        if (!RegisterRawInputDevices(&rid, 1, sizeof(rid)))
            DebugLog_Write(L"[app] RegisterRawInputDevices(mouse) failed err=%lu", GetLastError());
        else
            DebugLog_Write(L"[app] raw mouse input registered");

        DebugLog_Write(L"[app] init complete");

        return 0;
    }

    case WM_SIZE:
        ResizeChildren(hwnd);
        return 0;

    case WM_INPUT:
    {
        UINT sz = 0;
        if (GetRawInputData((HRAWINPUT)lParam, RID_INPUT, nullptr, &sz, sizeof(RAWINPUTHEADER)) != 0 || sz == 0)
            return 0;

        std::vector<BYTE> buf(sz);
        if (GetRawInputData((HRAWINPUT)lParam, RID_INPUT, buf.data(), &sz, sizeof(RAWINPUTHEADER)) == (UINT)-1)
            return 0;

        if (sz < sizeof(RAWINPUT)) return 0;
        RAWINPUT* ri = (RAWINPUT*)buf.data();
        if (ri->header.dwType == RIM_TYPEMOUSE)
        {
            const RAWMOUSE& rm = ri->data.mouse;
            LONG dx = 0;
            LONG dy = 0;
            if ((rm.usFlags & MOUSE_MOVE_ABSOLUTE) == 0)
            {
                dx = rm.lLastX;
                dy = rm.lLastY;
            }
            if (dx != 0 || dy != 0)
                Backend_AddMouseDelta((int)dx, (int)dy);
        }
        return 0;
    }

    case WM_DEVICECHANGE:
        if (wParam == DBT_DEVNODES_CHANGED ||
            wParam == DBT_DEVICEARRIVAL ||
            wParam == DBT_DEVICEREMOVECOMPLETE)
        {
            Backend_NotifyDeviceChange();
        }
        return 0;

    case WM_TIMER:
        if (wParam == UI_TIMER_ID)
        {
            PublishMouseIpcState();
            if (g_backendReady && !g_digitalFallbackWarnShown && Backend_ConsumeDigitalFallbackWarning())
            {
                g_digitalFallbackWarnShown = true;
                MessageBoxW(
                    hwnd,
                    L"HallJoy switched to compatibility input mode.\n\n"
                    L"Analog stream from Wooting SDK is not available right now, "
                    L"so key input is emulated from digital key states.\n\n"
                    L"Result: gamepad control works, but this is not true analog precision.",
                    L"HallJoy Warning",
                    MB_ICONWARNING | MB_OK);
            }
            if (g_hPageMain)
                KeyboardUI_OnTimerTick(g_hPageMain);
            return 0;
        }
        if (wParam == SETTINGS_SAVE_TIMER_ID)
        {
            KillTimer(hwnd, SETTINGS_SAVE_TIMER_ID);
            SaveSettingsByActiveGlobalProfile();
            return 0;
        }
        return 0;

    case WM_APP_REQUEST_SAVE:
        RequestSettingsSave(hwnd);
        return 0;

    case WM_APP_APPLY_TIMING:
        ApplyTimingSettings(hwnd);
        return 0;

    case WM_APP_KEYBOARD_LAYOUT_CHANGED:
        if (g_hPageMain && IsWindow(g_hPageMain))
            PostMessageW(g_hPageMain, WM_APP_KEYBOARD_LAYOUT_CHANGED, 0, 0);
        return 0;

    case WM_DESTROY:
        DebugLog_Write(L"[app] WM_DESTROY");
        g_mouseBlockPauseByRShift.store(false, std::memory_order_relaxed);
        g_mouseCursorLocked = false;
        MouseIpc_ShutdownPublisher();
        KillTimer(hwnd, UI_TIMER_ID);
        KillTimer(hwnd, SETTINGS_SAVE_TIMER_ID);

        WINDOWPLACEMENT wp{};
        wp.length = sizeof(wp);
        RECT wr{};
        if (GetWindowPlacement(hwnd, &wp))
            wr = wp.rcNormalPosition;
        else
            GetWindowRect(hwnd, &wr);

        int ww = std::max(0, (int)(wr.right - wr.left));
        int wh = std::max(0, (int)(wr.bottom - wr.top));
        if (ww >= 300 && wh >= 240)
        {
            Settings_SetMainWindowWidthPx(ww);
            Settings_SetMainWindowHeightPx(wh);
            Settings_SetMainWindowPosXPx((int)wr.left);
            Settings_SetMainWindowPosYPx((int)wr.top);
        }

        SaveSettingsByActiveGlobalProfile();

        if (g_backendReady)
        {
            RealtimeLoop_Stop();
            Backend_Shutdown();
            g_backendReady = false;
        }
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

int App_Run(HINSTANCE hInst, int nCmdShow)
{
    // Load settings before window creation so we can restore last window size.
    if (!SettingsIni_Load(AppPaths_SettingsIni().c_str()))
    {
        DebugLog_Write(L"[app] settings load failed, writing defaults path=%s", AppPaths_SettingsIni().c_str());
        SettingsIni_Save(AppPaths_SettingsIni().c_str());
    }
    else
    {
        DebugLog_Write(L"[app] settings loaded path=%s", AppPaths_SettingsIni().c_str());
    }

    // Overlay active global profile settings (all settings except layout/window).
    // Active profile name is read from base settings in SettingsIni_Load().
    if (!GlobalProfiles_IsDefault(GlobalProfiles_GetActiveName()))
    {
        std::wstring activeSettingsPath = AppPaths_ActiveSettingsIni();
        if (SettingsIni_LoadProfile(activeSettingsPath.c_str()))
        {
            DebugLog_Write(L"[app] active profile settings loaded profile=%s path=%s",
                GlobalProfiles_GetActiveName().c_str(), activeSettingsPath.c_str());
        }
        else
        {
            DebugLog_Write(L"[app] active profile settings missing, creating defaults profile=%s path=%s",
                GlobalProfiles_GetActiveName().c_str(), activeSettingsPath.c_str());
            SettingsIni_SaveProfile(activeSettingsPath.c_str());
        }
    }

    // IMPORTANT:
    // Ensure common controls are registered before we create any TabControl/Trackbar/etc.
    INITCOMMONCONTROLSEX icc{};
    icc.dwSize = sizeof(icc);
    icc.dwICC = ICC_WIN95_CLASSES | ICC_TAB_CLASSES | ICC_BAR_CLASSES;
    InitCommonControlsEx(&icc);

    WNDCLASSW wc{};
    wc.lpfnWndProc = MainWndProc;
    wc.hInstance = hInst;
    wc.lpszClassName = L"WootingVigemGui";
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = nullptr;
    wc.hIcon = (HICON)LoadImageW(hInst, MAKEINTRESOURCEW(IDI_HALLJOY), IMAGE_ICON, 0, 0, LR_DEFAULTSIZE);

    if (!RegisterClassW(&wc))
    {
        DebugLog_Write(L"[app] RegisterClass failed err=%lu", GetLastError());
        return 1;
    }

    UINT dpi = WinUtil_GetSystemDpiCompat();

    int defaultW = MulDiv(821, (int)dpi, 96);
    int defaultH = MulDiv(832, (int)dpi, 96);

    int w = Settings_GetMainWindowWidthPx();
    int h = Settings_GetMainWindowHeightPx();
    if (w <= 0) w = defaultW;
    if (h <= 0) h = defaultH;

    int minW = MulDiv(700, (int)dpi, 96);
    int minH = MulDiv(520, (int)dpi, 96);
    w = std::max(w, minW);
    h = std::max(h, minH);

    int x = Settings_GetMainWindowPosXPx();
    int y = Settings_GetMainWindowPosYPx();
    bool hasSavedPos = (x != std::numeric_limits<int>::min() &&
                        y != std::numeric_limits<int>::min());
    if (!hasSavedPos || !IsWindowRectVisibleOnAnyScreen(x, y, w, h))
    {
        x = CW_USEDEFAULT;
        y = CW_USEDEFAULT;
    }

    HWND hwnd = CreateWindowExW(
        0,
        wc.lpszClassName,
        L"HallJoy",
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
        x, y,
        w, h,
        nullptr, nullptr, hInst, nullptr);

    if (!hwnd) { DebugLog_Write(L"[app] CreateWindowEx failed err=%lu", GetLastError()); return 2; }
    g_hMainWnd = hwnd;
    DebugLog_Write(L"[app] main window created hwnd=%p pos=(%d,%d) size=(%d,%d)", hwnd, x, y, w, h);

    if (wc.hIcon)
        SendMessageW(hwnd, WM_SETICON, ICON_BIG, (LPARAM)wc.hIcon);
    HICON hSmall = (HICON)LoadImageW(hInst, MAKEINTRESOURCEW(IDI_SMALL), IMAGE_ICON, 0, 0, LR_DEFAULTSIZE);
    if (hSmall)
        SendMessageW(hwnd, WM_SETICON, (WPARAM)ICON_SMALL, (LPARAM)hSmall);

    ShowWindow(hwnd, nCmdShow);

    g_hKeyboardHook = SetWindowsHookExW(WH_KEYBOARD_LL, KeyboardBlockHookProc, GetModuleHandleW(nullptr), 0);
    DebugLog_Write(L"[app] keyboard hook=%p", g_hKeyboardHook);
    g_hMouseHook = SetWindowsHookExW(WH_MOUSE_LL, MouseBlockHookProc, GetModuleHandleW(nullptr), 0);
    DebugLog_Write(L"[app] mouse hook=%p", g_hMouseHook);

    MSG msg{};
    while (true)
    {
        BOOL gm = GetMessageW(&msg, nullptr, 0, 0);
        if (gm == -1)
        {
            DebugLog_Write(L"[app] GetMessage failed");
            return 3;
        }
        if (gm == 0)
            break;
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    if (g_hKeyboardHook)
    {
        UnhookWindowsHookEx(g_hKeyboardHook);
        g_hKeyboardHook = nullptr;
    }
    if (g_hMouseHook)
    {
        UnhookWindowsHookEx(g_hMouseHook);
        g_hMouseHook = nullptr;
    }
    g_hMainWnd = nullptr;
    DebugLog_Write(L"[app] message loop exit code=%d", (int)msg.wParam);

    return (int)msg.wParam;
}

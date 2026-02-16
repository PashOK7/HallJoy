#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <shellapi.h>
#include <urlmon.h>
#include <winhttp.h>
#include <wintrust.h>
#include <softpub.h>

#include <string>
#include <vector>
#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <regex>
#include <cwctype>

#include "app_deps.h"
#include "backend.h"
#include "debug_log.h"

#pragma comment(lib, "Urlmon.lib")
#pragma comment(lib, "Winhttp.lib")
#pragma comment(lib, "Wintrust.lib")

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
        s.replace(p, 2, L"/");
    p = 0;
    while ((p = s.find(L"\\u0026", p)) != std::wstring::npos)
        s.replace(p, 6, L"&");
    p = 0;
    while ((p = s.find(L"\\u003d", p)) != std::wstring::npos)
        s.replace(p, 6, L"=");
    return s;
}

static bool IsTrustedAssetUrl(const std::wstring& url)
{
    URL_COMPONENTSW uc{};
    uc.dwStructSize = sizeof(uc);
    uc.dwSchemeLength = (DWORD)-1;
    uc.dwHostNameLength = (DWORD)-1;
    if (!WinHttpCrackUrl(url.c_str(), 0, 0, &uc))
        return false;
    if (uc.nScheme != INTERNET_SCHEME_HTTPS)
        return false;

    std::wstring host = ToLowerCopy(std::wstring(uc.lpszHostName, uc.dwHostNameLength));
    if (host == L"github.com" ||
        host == L"api.github.com" ||
        host == L"objects.githubusercontent.com" ||
        host == L"release-assets.githubusercontent.com")
    {
        return true;
    }
    if (EndsWithNoCase(host, L".githubusercontent.com"))
        return true;
    return false;
}

static bool VerifyFileAuthenticodeTrusted(const std::wstring& filePath)
{
    WINTRUST_FILE_INFO fileInfo{};
    fileInfo.cbStruct = sizeof(fileInfo);
    fileInfo.pcwszFilePath = filePath.c_str();

    WINTRUST_DATA trustData{};
    trustData.cbStruct = sizeof(trustData);
    trustData.dwUIChoice = WTD_UI_NONE;
    trustData.fdwRevocationChecks = WTD_REVOKE_NONE;
    trustData.dwUnionChoice = WTD_CHOICE_FILE;
    trustData.pFile = &fileInfo;
    trustData.dwStateAction = WTD_STATEACTION_IGNORE;
    trustData.dwProvFlags = WTD_CACHE_ONLY_URL_RETRIEVAL;

    GUID policy = WINTRUST_ACTION_GENERIC_VERIFY_V2;
    LONG status = WinVerifyTrust(nullptr, &policy, &trustData);
    return status == ERROR_SUCCESS;
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

    HINTERNET hSession = WinHttpOpen(
        L"HallJoy/1.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS,
        0);
    if (!hSession)
    {
        DebugLog_Write(L"[deps.http] WinHttpOpen failed err=%lu", GetLastError());
        return false;
    }

    HINTERNET hConnect = WinHttpConnect(hSession, host.c_str(), uc.nPort, 0);
    if (!hConnect)
    {
        DebugLog_Write(L"[deps.http] WinHttpConnect failed err=%lu", GetLastError());
        WinHttpCloseHandle(hSession);
        return false;
    }

    DWORD flags = (uc.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET hReq = WinHttpOpenRequest(
        hConnect, L"GET", path.c_str(), nullptr,
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
        ok = WinHttpQueryHeaders(
            hReq,
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
    if (!IsTrustedAssetUrl(assetUrl))
    {
        DebugLog_Write(L"[deps] rejected untrusted asset url=%s", assetUrl.c_str());
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
    if (!VerifyFileAuthenticodeTrusted(installerPath))
    {
        DebugLog_Write(L"[deps.exec] Authenticode verification failed path=%s", installerPath.c_str());
        return false;
    }

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

DependencyInstallResult AppDeps_TryInstallMissingDependencies(HWND hwnd, uint32_t issues)
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
    prompt += L"\nDownload and run verified installers from trusted GitHub sources now?";
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


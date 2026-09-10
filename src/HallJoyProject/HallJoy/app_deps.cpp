#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <commctrl.h>
#include <shellapi.h>

#include <cstdint>
#include <cwchar>
#include <iterator>
#include <string>

#include "app_deps.h"
#include "backend.h"
#include "debug_log.h"
#include "dependency_guidance_policy.h"
#include "embedded_vigem_installer.h"
#include "embedded_analog_stack.h"

#pragma comment(lib, "comctl32.lib")
#pragma comment(linker, \
    "\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' " \
    "version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' " \
    "language='*'\"")

namespace
{
    std::wstring BuildIssuesText(std::uint32_t issues)
    {
        std::wstring text;
        if (issues & BackendInitIssue_VigemBusMissing)
            text += L"- ViGEm Bus is missing.\n";
        if (issues & BackendInitIssue_PrivateUapUnavailable)
            text += L"- HallJoy's embedded private analog runtime could not be prepared or loaded.\n";
        if (issues & BackendInitIssue_PrivateUapIncompatible)
            text += L"- HallJoy's embedded private analog runtime has an incompatible ABI.\n";
        if (issues & BackendInitIssue_PrivateUapNoDevices)
            text += L"- The private analog runtime found no supported analog device.\n";
        if (issues & BackendInitIssue_Unknown)
            text += L"- Unknown backend initialization issue.\n";
        if (text.empty())
            text = L"- Unknown backend initialization issue.\n";
        return text;
    }

    void ShowPrivateRuntimeGuidance(HWND hwnd, std::uint32_t issues)
    {
        std::wstring details = BuildIssuesText(issues);
        details += L"\nHallJoy uses its own embedded private runtime. Installing a system-wide Wooting Analog SDK or global UAP cannot repair this path.\n\n";
        if (!EmbeddedAnalogStack_PrivatePluginPath().empty())
        {
            details += L"Verified runtime path:\n";
            details += EmbeddedAnalogStack_PrivatePluginPath();
            details += L"\n\n";
        }
        details += L"Reinstall the same HallJoy build if the embedded runtime remains unavailable. Native protocol backends can continue independently when supported hardware is present.";
        MessageBoxW(hwnd, details.c_str(), L"HallJoy private analog runtime", MB_OK | MB_ICONWARNING);
        DebugLog_Write(
            L"[deps] private UAP issue; system SDK install intentionally unavailable location=%s error=%lu",
            EmbeddedAnalogStack_RuntimeLocationName(), EmbeddedAnalogStack_LastError());
    }

    enum class VigemUserAction
    {
        Install,
        OpenReleasePage,
        ContinueWithoutOutput,
    };

    VigemUserAction AskForVigemAction(HWND hwnd)
    {
        constexpr int installButton = 1001;
        constexpr int releasePageButton = 1002;
        constexpr int continueButton = 1003;
        const TASKDIALOG_BUTTON buttons[] = {
            { installButton,
              L"Install ViGEmBus 1.22.0 (recommended)\n"
              L"Run the verified installer embedded in HallJoy; Windows will ask for administrator approval." },
            { releasePageButton,
              L"Open the official release page\n"
              L"Use the browser if you prefer to download and install ViGEmBus yourself." },
            { continueButton,
              L"Continue without virtual gamepad output\n"
              L"HallJoy will open, but it cannot create an Xbox controller." },
        };

        TASKDIALOGCONFIG dialog{};
        dialog.cbSize = sizeof(dialog);
        dialog.hwndParent = hwnd;
        dialog.dwFlags = TDF_USE_COMMAND_LINKS |
            TDF_ALLOW_DIALOG_CANCELLATION |
            TDF_POSITION_RELATIVE_TO_WINDOW;
        dialog.dwCommonButtons = TDCBF_CANCEL_BUTTON;
        dialog.pszWindowTitle = L"HallJoy dependency required";
        dialog.pszMainIcon = TD_SHIELD_ICON;
        dialog.pszMainInstruction =
            L"ViGEmBus is required for the virtual Xbox controller";
        dialog.pszContent =
            L"HallJoy includes the official ViGEmBus 1.22.0 installer. "
            L"Its exact SHA-256 and Windows publisher signature are verified before it can run. "
            L"No installer is downloaded at runtime.";
        dialog.cButtons = static_cast<UINT>(std::size(buttons));
        dialog.pButtons = buttons;
        dialog.nDefaultButton = installButton;
        dialog.pszFooter =
            L"Publisher: Nefarius Software Solutions e.U.";

        int selected = IDCANCEL;
        if (SUCCEEDED(TaskDialogIndirect(
                &dialog, &selected, nullptr, nullptr)))
        {
            if (selected == installButton)
                return VigemUserAction::Install;
            if (selected == releasePageButton)
                return VigemUserAction::OpenReleasePage;
            if (selected == continueButton)
                return VigemUserAction::ContinueWithoutOutput;
            return VigemUserAction::ContinueWithoutOutput;
        }

        const int fallback = MessageBoxW(hwnd,
            L"ViGEmBus is required for the virtual Xbox controller.\n\n"
            L"Yes: install the verified copy embedded in HallJoy.\n"
            L"No: open the official ViGEmBus 1.22.0 page.\n"
            L"Cancel: continue without virtual gamepad output.",
            L"HallJoy dependency required",
            MB_YESNOCANCEL | MB_ICONWARNING | MB_DEFBUTTON1);
        if (fallback == IDYES)
            return VigemUserAction::Install;
        if (fallback == IDNO)
            return VigemUserAction::OpenReleasePage;
        return VigemUserAction::ContinueWithoutOutput;
    }

    bool OpenPinnedReleasePage(HWND hwnd)
    {
        const HINSTANCE opened = ShellExecuteW(hwnd, L"open",
            halljoy::deps::kPinnedVigemReleasePage,
            nullptr, nullptr, SW_SHOWNORMAL);
        const bool ok = reinterpret_cast<INT_PTR>(opened) > 32;
        DebugLog_Write(L"[deps] open pinned ViGEm release page result=%d native=%lld url=%s",
            ok ? 1 : 0, static_cast<long long>(reinterpret_cast<INT_PTR>(opened)),
            halljoy::deps::kPinnedVigemReleasePage);
        if (!ok)
        {
            std::wstring text =
                L"Windows could not open the official ViGEmBus page.\n\n";
            text += halljoy::deps::kPinnedVigemReleasePage;
            MessageBoxW(hwnd, text.c_str(), L"HallJoy", MB_OK | MB_ICONERROR);
        }
        return ok;
    }

    void ShowInstallFailure(HWND hwnd,
        const EmbeddedVigemInstallResult& result)
    {
        std::wstring text;
        if (result.status == EmbeddedVigemInstallStatus::UserCancelled)
        {
            text = L"Installation was cancelled. HallJoy will continue without virtual gamepad output.";
        }
        else if (result.status == EmbeddedVigemInstallStatus::TimedOut)
        {
            text = L"The installer is still running. Finish it, then restart HallJoy.";
        }
        else if (result.status == EmbeddedVigemInstallStatus::ResourceInvalid ||
                 result.status == EmbeddedVigemInstallStatus::SignatureInvalid)
        {
            text = L"HallJoy refused to run the embedded installer because its integrity or publisher signature could not be verified. Re-download the same HallJoy release.";
        }
        else
        {
            text = L"ViGEmBus installation did not complete successfully. You can retry by restarting HallJoy or use the official release page.";
        }
        MessageBoxW(hwnd, text.c_str(), L"HallJoy ViGEmBus installation",
            MB_OK | MB_ICONWARNING);
    }

    DependencyGuidanceResult InstallPinnedVigem(HINSTANCE hInst, HWND hwnd)
    {
        DebugLog_Write(
            L"[deps] embedded ViGEm install begin version=%s size=%u sha256=%s runtime_download=0",
            halljoy::deps::kPinnedVigemVersion,
            halljoy::deps::kPinnedVigemInstallerSize,
            halljoy::deps::kPinnedVigemInstallerSha256);
        const EmbeddedVigemInstallResult result =
            EmbeddedVigemInstaller_Run(hInst, hwnd);
        DebugLog_Write(
            L"[deps] embedded ViGEm install end status=%u native_error=%lu installer_exit=%lu",
            static_cast<unsigned>(result.status),
            static_cast<unsigned long>(result.nativeError),
            static_cast<unsigned long>(result.installerExitCode));

        if (result.status == EmbeddedVigemInstallStatus::Installed)
            return DependencyGuidanceResult::InstallCompleted;
        if (result.status == EmbeddedVigemInstallStatus::RestartRequired)
        {
            MessageBoxW(hwnd,
                L"ViGEmBus installation completed and Windows requires a restart.\n\n"
                L"Restart Windows, then launch HallJoy again.",
                L"HallJoy ViGEmBus installation",
                MB_OK | MB_ICONINFORMATION);
            return DependencyGuidanceResult::ManualInstallRequired;
        }
        ShowInstallFailure(hwnd, result);
        return DependencyGuidanceResult::ManualInstallRequired;
    }
}

DependencyGuidanceResult AppDeps_ShowMissingDependencyGuidance(
    HINSTANCE hInst, HWND hwnd, std::uint32_t issues)
{
    DebugLog_Write(L"[deps] begin guidance issues=0x%08X", issues);
    const auto plan = halljoy::deps::BuildGuidancePlan(
        (issues & BackendInitIssue_VigemBusMissing) != 0,
        (issues & (BackendInitIssue_PrivateUapUnavailable |
                   BackendInitIssue_PrivateUapIncompatible |
                   BackendInitIssue_PrivateUapNoDevices)) != 0);

    if (plan.showPrivateRuntimeGuidance)
        ShowPrivateRuntimeGuidance(hwnd, issues);
    if (!plan.showPinnedVigemRelease)
        return DependencyGuidanceResult::NoAction;

    switch (AskForVigemAction(hwnd))
    {
    case VigemUserAction::Install:
        return InstallPinnedVigem(hInst, hwnd);
    case VigemUserAction::OpenReleasePage:
        OpenPinnedReleasePage(hwnd);
        return DependencyGuidanceResult::ManualInstallRequired;
    case VigemUserAction::ContinueWithoutOutput:
    default:
        DebugLog_Write(L"[deps] user continued without ViGEm output");
        return DependencyGuidanceResult::ManualInstallRequired;
    }
}

bool AppDeps_TryRunEmbeddedInstallerVerificationCommand(
    HINSTANCE hInst, int& exitCode) noexcept
{
    constexpr wchar_t command[] =
        L"--halljoy-verify-embedded-vigem-installer";
    const wchar_t* commandLine = GetCommandLineW();
    if (!commandLine || !wcsstr(commandLine, command))
        return false;

    DWORD nativeError = ERROR_SUCCESS;
    exitCode = EmbeddedVigemInstaller_VerifyOnly(hInst, &nativeError) ? 0 :
        static_cast<int>(nativeError == ERROR_SUCCESS ? ERROR_INVALID_DATA : nativeError);
    return true;
}

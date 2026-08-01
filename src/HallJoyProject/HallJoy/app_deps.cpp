#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <cstdint>
#include <string>

#include "app_deps.h"
#include "backend.h"
#include "debug_log.h"
#include "dependency_guidance_policy.h"
#include "embedded_analog_stack.h"

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

    void ShowPinnedVigemGuidance(HWND hwnd)
    {
        std::wstring details =
            L"ViGEm Bus is required to create the virtual Xbox controller.\n\n"
            L"For security, HallJoy never downloads, starts, or elevates an installer. "
            L"Install the pinned official ViGEmBus release manually, then restart HallJoy.\n\n"
            L"Required version: ";
        details += halljoy::deps::kPinnedVigemVersion;
        details += L"\nOfficial release page:\n";
        details += halljoy::deps::kPinnedVigemReleasePage;
        details += L"\n\nYou can press Ctrl+C while this message is focused to copy its full text.";

        MessageBoxW(hwnd, details.c_str(), L"HallJoy dependency required", MB_OK | MB_ICONWARNING);
        DebugLog_Write(
            L"[deps] manual ViGEm guidance version=%s url=%s automatic_installer=disabled",
            halljoy::deps::kPinnedVigemVersion,
            halljoy::deps::kPinnedVigemReleasePage);
    }
}

DependencyGuidanceResult AppDeps_ShowMissingDependencyGuidance(HWND hwnd, std::uint32_t issues)
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

    ShowPinnedVigemGuidance(hwnd);
    return DependencyGuidanceResult::ManualInstallRequired;
}

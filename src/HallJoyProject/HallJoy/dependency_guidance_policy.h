#pragma once

namespace halljoy::deps
{
    inline constexpr wchar_t kPinnedVigemVersion[] = L"1.22.0";
    inline constexpr wchar_t kPinnedVigemReleasePage[] =
        L"https://github.com/nefarius/ViGEmBus/releases/tag/v1.22.0";
    inline constexpr wchar_t kPinnedVigemInstallerFileName[] =
        L"ViGEmBus_1.22.0_x64_x86_arm64.exe";
    inline constexpr wchar_t kPinnedVigemInstallerSha256[] =
        L"89220A7865076B342892F98865F3499FB7C4CFD673159E89D352C360FD014C6A";
    inline constexpr unsigned kPinnedVigemInstallerSize = 6278576u;

    struct GuidancePlan
    {
        bool showPrivateRuntimeGuidance = false;
        bool showPinnedVigemRelease = false;
    };

    [[nodiscard]] constexpr GuidancePlan BuildGuidancePlan(
        bool vigemBusMissing,
        bool privateRuntimeIssue) noexcept
    {
        return GuidancePlan{ privateRuntimeIssue, vigemBusMissing };
    }
}

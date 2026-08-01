#pragma once

namespace halljoy::deps
{
    inline constexpr wchar_t kPinnedVigemVersion[] = L"1.22.0";
    inline constexpr wchar_t kPinnedVigemReleasePage[] =
        L"https://github.com/nefarius/ViGEmBus/releases/tag/v1.22.0";

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

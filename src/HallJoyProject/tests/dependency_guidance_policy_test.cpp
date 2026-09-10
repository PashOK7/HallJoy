#include "../HallJoy/dependency_guidance_policy.h"

#include <cassert>
#include <iostream>
#include <string_view>

int main()
{
    using halljoy::deps::BuildGuidancePlan;

    const auto none = BuildGuidancePlan(false, false);
    assert(!none.showPrivateRuntimeGuidance);
    assert(!none.showPinnedVigemRelease);

    const auto vigem = BuildGuidancePlan(true, false);
    assert(!vigem.showPrivateRuntimeGuidance);
    assert(vigem.showPinnedVigemRelease);

    const auto privateRuntime = BuildGuidancePlan(false, true);
    assert(privateRuntime.showPrivateRuntimeGuidance);
    assert(!privateRuntime.showPinnedVigemRelease);

    const auto both = BuildGuidancePlan(true, true);
    assert(both.showPrivateRuntimeGuidance);
    assert(both.showPinnedVigemRelease);

    constexpr std::wstring_view version = halljoy::deps::kPinnedVigemVersion;
    constexpr std::wstring_view page = halljoy::deps::kPinnedVigemReleasePage;
    constexpr std::wstring_view fileName = halljoy::deps::kPinnedVigemInstallerFileName;
    constexpr std::wstring_view sha256 = halljoy::deps::kPinnedVigemInstallerSha256;
    assert(version == L"1.22.0");
    assert(page == L"https://github.com/nefarius/ViGEmBus/releases/tag/v1.22.0");
    assert(page.find(L"/latest") == std::wstring_view::npos);
    assert(fileName == L"ViGEmBus_1.22.0_x64_x86_arm64.exe");
    assert(sha256 == L"89220A7865076B342892F98865F3499FB7C4CFD673159E89D352C360FD014C6A");
    assert(halljoy::deps::kPinnedVigemInstallerSize == 6278576u);

    std::cout << "DEPENDENCY_GUIDANCE_POLICY_TEST=PASS\n";
    return 0;
}

#pragma once

#include <cstdint>
#include <string_view>

namespace halljoy::vigem_output
{

inline constexpr wchar_t kOutputHostArgument[] =
    L"--halljoy-vigem-output-host";
inline constexpr wchar_t kOutputSelfTestArgument[] =
    L"--halljoy-test-vigem-output-self-host";
inline constexpr wchar_t kOutputRealSelfTestArgument[] =
    L"--halljoy-test-vigem-output-real-child";
inline constexpr wchar_t kOutputRuntimeStressTestArgument[] =
    L"--halljoy-test-vigem-output-runtime-stress";

// `Real` is reserved for F2. Every fake mode is accepted only when the child
// image was compiled as the opt-in analogue simulator.
enum class OutputHostMode : std::uint32_t
{
    Real = 0,
    FakeNormal,
    FakeExitBeforeReady,
    FakeExitAfterReady,
    FakeExitBeforeSnapshotRead,
    FakeExitDuringUpdate,
    FakeExitAfterAcknowledgement,
    FakeExitDuringStop,
    FakeStallAfterReady,
};

inline std::wstring_view OutputHostModeToken(OutputHostMode mode) noexcept
{
    switch (mode)
    {
    case OutputHostMode::Real: return L"real";
    case OutputHostMode::FakeNormal: return L"fake-normal";
    case OutputHostMode::FakeExitBeforeReady: return L"fake-exit-before-ready";
    case OutputHostMode::FakeExitAfterReady: return L"fake-exit-after-ready";
    case OutputHostMode::FakeExitBeforeSnapshotRead: return L"fake-exit-before-read";
    case OutputHostMode::FakeExitDuringUpdate: return L"fake-exit-during-update";
    case OutputHostMode::FakeExitAfterAcknowledgement: return L"fake-exit-after-ack";
    case OutputHostMode::FakeExitDuringStop: return L"fake-exit-during-stop";
    case OutputHostMode::FakeStallAfterReady: return L"fake-stall-after-ready";
    }
    return {};
}

inline bool ParseOutputHostMode(std::wstring_view token,
    OutputHostMode& mode) noexcept
{
    constexpr OutputHostMode modes[] = {
        OutputHostMode::Real,
        OutputHostMode::FakeNormal,
        OutputHostMode::FakeExitBeforeReady,
        OutputHostMode::FakeExitAfterReady,
        OutputHostMode::FakeExitBeforeSnapshotRead,
        OutputHostMode::FakeExitDuringUpdate,
        OutputHostMode::FakeExitAfterAcknowledgement,
        OutputHostMode::FakeExitDuringStop,
        OutputHostMode::FakeStallAfterReady,
    };
    for (const OutputHostMode candidate : modes)
    {
        if (token == OutputHostModeToken(candidate))
        {
            mode = candidate;
            return true;
        }
    }
    return false;
}

} // namespace halljoy::vigem_output

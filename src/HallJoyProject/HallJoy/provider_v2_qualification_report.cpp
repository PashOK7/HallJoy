#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "provider_v2_qualification_report.h"

#if defined(HALLJOY_PROVIDER_V2_QUALIFICATION)

#include <array>
#include <cstdio>
#include <string>

#include "backend.h"
#include "provider_v2_qualification_model.h"

namespace
{
constexpr wchar_t kReportName[] = L"HallJoyProviderV2Qualification.txt";
constexpr wchar_t kTemporaryName[] = L"HallJoyProviderV2Qualification.tmp";
std::uint64_t g_sessionId = 0;

std::wstring PathNearExe(const wchar_t* name)
{
    std::array<wchar_t, 32768> path{};
    const DWORD length = GetModuleFileNameW(nullptr, path.data(),
        static_cast<DWORD>(path.size()));
    if (length == 0 || length >= path.size())
        return {};
    std::wstring result(path.data(), length);
    const std::size_t slash = result.find_last_of(L"\\/");
    if (slash == std::wstring::npos)
        return {};
    result.resize(slash + 1u);
    result += name;
    return result;
}

bool WriteAtomic(const std::string& text) noexcept
{
    try
    {
        const std::wstring temporary = PathNearExe(kTemporaryName);
        const std::wstring finalPath = PathNearExe(kReportName);
        if (temporary.empty() || finalPath.empty())
            return false;
        HANDLE file = CreateFileW(temporary.c_str(), GENERIC_WRITE, 0, nullptr,
            CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH,
            nullptr);
        if (file == INVALID_HANDLE_VALUE)
            return false;
        DWORD written = 0;
        const bool writeOk = text.size() <= MAXDWORD &&
            WriteFile(file, text.data(), static_cast<DWORD>(text.size()),
                &written, nullptr) &&
            static_cast<std::size_t>(written) == text.size() &&
            FlushFileBuffers(file);
        CloseHandle(file);
        if (!writeOk)
        {
            DeleteFileW(temporary.c_str());
            return false;
        }
        if (!MoveFileExW(temporary.c_str(), finalPath.c_str(),
            MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
        {
            DeleteFileW(temporary.c_str());
            return false;
        }
        return true;
    }
    catch (...)
    {
        return false;
    }
}

halljoy::provider_v2_qualification::SnapshotV1 CaptureSnapshot()
{
    BackendAnalogTelemetry source{};
    Backend_GetAnalogTelemetry(&source);
    halljoy::provider_v2_qualification::SnapshotV1 result{};
    result.available = source.providerV2ShadowAvailable;
    result.backendInitCount = source.providerV2ShadowBackendInitCount;
    result.eligibleTicks = source.providerV2ShadowEligibleTicks;
    result.matchedReports = source.providerV2ShadowMatchedReports;
    result.mismatchedReports = source.providerV2ShadowMismatchedReports;
    result.unavailableTicks = source.providerV2ShadowUnavailableTicks;
    result.digitalFallbackTicks = source.providerV2ShadowDigitalFallbackTicks;
    result.curveMutationTicks = source.providerV2ShadowCurveMutationTicks;
    for (std::size_t field = 0; field < result.fieldMismatches.size(); ++field)
        result.fieldMismatches[field] =
            source.providerV2ShadowFieldMismatches[field];
    result.uniqueSampleGenerations =
        source.providerV2ShadowUniqueSampleGenerations;
    result.firstEligibleTickMs = source.providerV2ShadowFirstEligibleTickMs;
    result.lastEligibleTickMs = source.providerV2ShadowLastEligibleTickMs;
    result.lastSampleGeneration =
        source.providerV2ShadowLastSampleGeneration;
    result.configuredFieldMask = source.providerV2ShadowConfiguredFieldMask;
    result.activatedFieldMask = source.providerV2ShadowActivatedFieldMask;
    result.releasedFieldMask = source.providerV2ShadowReleasedFieldMask;
    return result;
}

const char* VerdictName(
    halljoy::provider_v2_qualification::Verdict verdict) noexcept
{
    using halljoy::provider_v2_qualification::Verdict;
    switch (verdict)
    {
    case Verdict::Pass: return "PASS";
    case Verdict::Fail: return "FAIL";
    default: return "INCOMPLETE";
    }
}

std::string BuildText(bool finalized, int appExitCode)
{
    using namespace halljoy::provider_v2_qualification;
    const SnapshotV1 snapshot = finalized ? CaptureSnapshot() : SnapshotV1{};
    EvaluationV1 evaluation{};
    if (finalized)
        evaluation = Evaluate(snapshot, appExitCode);

    std::array<char, 8192> text{};
    const int length = std::snprintf(text.data(), text.size(),
        "HallJoy Provider V2 qualification\r\n"
        "schema=2\r\n"
        "session_id=%llu\r\n"
        "finalized=%u\r\n"
        "verdict=%s\r\n"
        "app_exit_code=%d\r\n"
        "evidence_gaps=0x%08X\r\n"
        "available=%u\r\n"
        "backend_init_count=%llu\r\n"
        "eligible_ticks=%llu\r\n"
        "eligible_duration_ms=%llu\r\n"
        "matched_reports=%llu\r\n"
        "mismatched_reports=%llu\r\n"
        "unavailable_ticks=%llu\r\n"
        "digital_fallback_ticks=%llu\r\n"
        "curve_mutation_ticks=%llu\r\n"
        "unique_sample_generations=%llu\r\n"
        "last_sample_generation=%llu\r\n"
        "configured_pad_field_mask=0x%08X\r\n"
        "activated_pad_field_mask=0x%08X\r\n"
        "released_pad_field_mask=0x%08X\r\n"
        "missing_activated_pad_field_mask=0x%08X\r\n"
        "missing_released_pad_field_mask=0x%08X\r\n"
        "field_mismatches_buttons=%llu\r\n"
        "field_mismatches_left_trigger=%llu\r\n"
        "field_mismatches_right_trigger=%llu\r\n"
        "field_mismatches_left_stick_x=%llu\r\n"
        "field_mismatches_left_stick_y=%llu\r\n"
        "field_mismatches_right_stick_x=%llu\r\n"
        "field_mismatches_right_stick_y=%llu\r\n"
        "duration_is_informational=1\r\n"
        "policy_minimum_unique_generations=100\r\n"
        "policy_requires_every_configured_field_activation_and_release=1\r\n"
        "coverage_source=provider_v2_raw_gated_legacy_shadow\r\n"
        "activation_policy=provider_raw_at_least_0.5_and_shadow_non_neutral\r\n"
        "release_policy=latched_until_provider_raw_zero_and_shadow_neutral\r\n"
        "coverage_mask_layout=bit_(pad_index*7+field_index)\r\n"
        "production_route=provider_v2_authoritative\r\n"
        "legacy_dense_shadow_submitted_to_vigem=0\r\n",
        static_cast<unsigned long long>(g_sessionId), finalized ? 1u : 0u,
        finalized ? VerdictName(evaluation.verdict) : "INCOMPLETE",
        finalized ? appExitCode : -1,
        finalized ? evaluation.evidenceGaps : EvidenceGap_AppExit,
        snapshot.available ? 1u : 0u,
        static_cast<unsigned long long>(snapshot.backendInitCount),
        static_cast<unsigned long long>(snapshot.eligibleTicks),
        static_cast<unsigned long long>(evaluation.eligibleDurationMs),
        static_cast<unsigned long long>(snapshot.matchedReports),
        static_cast<unsigned long long>(snapshot.mismatchedReports),
        static_cast<unsigned long long>(snapshot.unavailableTicks),
        static_cast<unsigned long long>(snapshot.digitalFallbackTicks),
        static_cast<unsigned long long>(snapshot.curveMutationTicks),
        static_cast<unsigned long long>(snapshot.uniqueSampleGenerations),
        static_cast<unsigned long long>(snapshot.lastSampleGeneration),
        snapshot.configuredFieldMask, snapshot.activatedFieldMask,
        snapshot.releasedFieldMask,
        finalized ? evaluation.missingActivatedFieldMask : 0u,
        finalized ? evaluation.missingReleasedFieldMask : 0u,
        static_cast<unsigned long long>(snapshot.fieldMismatches[0]),
        static_cast<unsigned long long>(snapshot.fieldMismatches[1]),
        static_cast<unsigned long long>(snapshot.fieldMismatches[2]),
        static_cast<unsigned long long>(snapshot.fieldMismatches[3]),
        static_cast<unsigned long long>(snapshot.fieldMismatches[4]),
        static_cast<unsigned long long>(snapshot.fieldMismatches[5]),
        static_cast<unsigned long long>(snapshot.fieldMismatches[6]));
    if (length <= 0 || static_cast<std::size_t>(length) >= text.size())
        return {};
    return std::string(text.data(), static_cast<std::size_t>(length));
}
}

#endif

bool ProviderV2QualificationReport_Begin() noexcept
{
#if defined(HALLJOY_PROVIDER_V2_QUALIFICATION)
    const std::wstring finalPath = PathNearExe(kReportName);
    const std::wstring temporary = PathNearExe(kTemporaryName);
    if (finalPath.empty() || temporary.empty())
        return false;
    if (!DeleteFileW(finalPath.c_str()) &&
        GetLastError() != ERROR_FILE_NOT_FOUND)
        return false;
    if (!DeleteFileW(temporary.c_str()) &&
        GetLastError() != ERROR_FILE_NOT_FOUND)
        return false;
    FILETIME now{};
    GetSystemTimeAsFileTime(&now);
    g_sessionId = (static_cast<std::uint64_t>(now.dwHighDateTime) << 32u) |
        now.dwLowDateTime;
    g_sessionId ^= static_cast<std::uint64_t>(GetCurrentProcessId()) << 16u;
    return WriteAtomic(BuildText(false, -1));
#else
    return true;
#endif
}

bool ProviderV2QualificationReport_Finalize(int appExitCode) noexcept
{
#if defined(HALLJOY_PROVIDER_V2_QUALIFICATION)
    return WriteAtomic(BuildText(true, appExitCode));
#else
    (void)appExitCode;
    return true;
#endif
}

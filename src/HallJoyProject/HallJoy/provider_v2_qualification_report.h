#pragma once

// These functions are no-ops in ordinary production builds. The dedicated
// qualification build writes exactly one small transactional summary beside
// its EXE: INCOMPLETE at start, then a final fail-closed verdict after normal
// application shutdown.
[[nodiscard]] bool ProviderV2QualificationReport_Begin() noexcept;
[[nodiscard]] bool ProviderV2QualificationReport_Finalize(
    int appExitCode) noexcept;

#pragma once

// Temporary, bounded, event-only verification trace for the stabilization
// programme. The implementation is enabled only when HALLJOY_STABILITY_TRACE
// is defined by the verification build. Calls compile to no-ops otherwise.

void StabilityTrace_Init() noexcept;
void StabilityTrace_Shutdown(int exitCode) noexcept;
void StabilityTrace_Write(
    const wchar_t* level,
    const wchar_t* component,
    const wchar_t* event,
    const wchar_t* fieldsFormat = nullptr,
    ...) noexcept;
void StabilityTrace_WriteCritical(
    const wchar_t* level,
    const wchar_t* component,
    const wchar_t* event,
    const wchar_t* fieldsFormat = nullptr,
    ...) noexcept;
bool StabilityTrace_IsEnabled() noexcept;
const wchar_t* StabilityTrace_Path() noexcept;

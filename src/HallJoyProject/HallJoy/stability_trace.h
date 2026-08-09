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
// Appends an already formatted diagnostic line to the same bounded sink. This
// lets the asynchronous DebugLog writer share the single mapped HallJoy.log
// without a second file owner or synchronous filesystem I/O on producers.
void StabilityTrace_AppendPlain(const wchar_t* line) noexcept;
bool StabilityTrace_IsEnabled() noexcept;
const wchar_t* StabilityTrace_Path() noexcept;

#if !defined(HALLJOY_STABILITY_TRACE) && !defined(HALLJOY_STABILITY_TRACE_IMPLEMENTATION)
// Compile the complete call site away in production. The unreachable calls keep
// diagnostic-only locals type-checked and syntactically used, while constexpr
// elimination guarantees zero argument evaluation and zero emitted code.
#define StabilityTrace_Init() ((void)0)
#define StabilityTrace_Shutdown(...) do { if constexpr (false) StabilityTrace_Shutdown(__VA_ARGS__); } while (false)
#define StabilityTrace_Write(...) do { if constexpr (false) StabilityTrace_Write(__VA_ARGS__); } while (false)
#define StabilityTrace_WriteCritical(...) do { if constexpr (false) StabilityTrace_WriteCritical(__VA_ARGS__); } while (false)
#define StabilityTrace_AppendPlain(...) do { if constexpr (false) StabilityTrace_AppendPlain(__VA_ARGS__); } while (false)
#define StabilityTrace_IsEnabled() false
#define StabilityTrace_Path() L""
#endif

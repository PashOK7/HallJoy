#pragma once
#include <array>
#include <atomic>
#include <cstdint>
#include <cstddef>

// Aggregate counters only: no key identities, text, depths or report contents.
// Enabled only in the focused diagnostic build. No allocation or disk I/O on
// the hook, native pump or realtime thread. Banks describe the setting observed
// at each stage, not a synchronized transaction across threads.
namespace halljoy::input_path {
enum Counter : unsigned { SourceFrames, SourcePositive, FnUnavailable,
    ConfiguredReads, RawPositive, FilteredPositive, BuiltFrames, ActiveFrames,
    Published, PublishRejected, BoundPassed, BoundBlocked, Count };
#if defined(HALLJOY_INPUT_PATH_DIAGNOSTIC)
inline std::array<std::array<std::atomic<std::uint64_t>,Count>,2> counters{};
inline std::atomic<std::uint64_t> latestPublication{0};
inline void Add(bool block, Counter counter, std::uint64_t count=1) noexcept {
    counters[block][counter].fetch_add(count,std::memory_order_relaxed);
}
inline std::uint64_t Read(bool block, Counter counter) noexcept {
    return counters[block][counter].load(std::memory_order_relaxed);
}
#else
inline void Add(bool, Counter, std::uint64_t=1) noexcept {}
#endif
}
#if defined(HALLJOY_INPUT_PATH_DIAGNOSTIC)
void Backend_InputPathStatus(char* text, std::size_t capacity) noexcept;
// Headless command only: uses real native registry, curves and report builder;
// does not start hardware, output processes or application UI.
bool Backend_TestSharkConfiguredPath(float w=0.5f, float a=1.0f);
#endif

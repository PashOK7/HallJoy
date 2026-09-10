#pragma once

#include <cstdint>

// A bounded posted-message bridge. The engine worker never calls UI code
// directly; the window procedure dispatches an opaque request token and the
// worker observes only a confirmed result.
namespace halljoy::engine_runtime::ui_bridge
{

enum class Operation : std::uint8_t
{
    ReleaseInput,
    RestoreInput,
    DependencyGuidance,
};

using Handler = bool (*)(Operation operation, std::uint32_t& nativeError) noexcept;

bool Start(void* windowHandle, unsigned message, Handler handler) noexcept;
bool Execute(Operation operation, std::uint32_t& nativeError) noexcept;
bool Dispatch(std::uintptr_t token) noexcept;
void CancelPending() noexcept;
bool Stop() noexcept;

} // namespace halljoy::engine_runtime::ui_bridge

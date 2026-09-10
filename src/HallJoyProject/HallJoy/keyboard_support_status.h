#pragma once

#include <cstdint>
namespace halljoy::keyboard_support
{
struct StatusSnapshot final
{
    bool searchCompleted = false;
    bool analogSourceConnected = false;
};

// A negative result is meaningful only after the engine owns an active
// generation. Until then the UI must remain silent while discovery starts.
void SetSearchObservation(bool searchCompleted, bool connected) noexcept;
[[nodiscard]] StatusSnapshot GetStatusSnapshot() noexcept;
}

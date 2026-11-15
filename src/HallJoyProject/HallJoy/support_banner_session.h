#pragma once
#include <cstdint>
#include "keyboard_support_status.h"

namespace halljoy::support_banner {
// Informational invitations may be hidden for this process. Operational and
// limited-firmware warnings remain visible, including after an invitation closes.
inline bool Dismissible(const keyboard_support::StatusSnapshot& status) noexcept {
    using namespace keyboard_support;
    return !status.communicationWarning && !status.unsupportedModel &&
        !(status.frozenModels & (LimitedModels | ~ImplementedModels)) &&
        (status.analogSourceConnected || !status.frozenModels);
}
struct Session {
    static constexpr std::uint64_t delayMs = 60000;
    std::uint64_t startedAt = 0;
    bool dismissed = false;
    bool Ready(std::uint64_t now) const noexcept {
        return !dismissed && now >= startedAt && now - startedAt >= delayMs;
    }
    bool Dismiss(std::uint64_t now, const keyboard_support::StatusSnapshot& status) noexcept {
        if (!Ready(now) || !Dismissible(status)) return false;
        dismissed = true;
        return true;
    }
    bool Visible(const keyboard_support::StatusSnapshot& status) const noexcept {
        return status.searchCompleted && (!status.analogSourceConnected ||
            status.frozenModels || status.communicationWarning) &&
            !(dismissed && Dismissible(status));
    }
};
}

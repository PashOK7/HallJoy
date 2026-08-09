#pragma once

#include <array>
#include <cstddef>
#include <string>
#include <vector>

namespace aula_win60he
{
// Fail-closed selection result for exact-fingerprint Aula HID candidates.
// candidateIndices is intentionally empty or contains exactly one index.
struct DeviceSelectionPlan
{
    std::vector<std::size_t> candidateIndices;
    bool ambiguous = false;
    bool retainedIdentityMissing = false;
    bool invalidEnumeration = false;
};

// Select an exact candidate using the strongest Windows identity evidence that
// is actually available. A non-empty SetupAPI instance ID is authoritative.
// Some valid HID stacks can enumerate and open an exact path while refusing the
// optional instance-ID query; in that limited case a previously retained exact
// path is the fail-closed reconnect boundary. Multiple unretained candidates,
// duplicate paths, duplicate non-empty instance IDs, or any identity drift are
// never guessed.
DeviceSelectionPlan PlanDeviceSelection(
    const std::vector<std::wstring>& paths,
    const std::vector<std::wstring>& instanceIds,
    const std::wstring& retainedPath,
    const std::wstring& retainedInstanceId);

// The sync payload reserves sixteen bytes for KeyboardSN. Treat only a
// printable, non-blank ASCII token as identity evidence. Zero/space/FF padding
// is ignored, while binary/control-filled fields are deliberately rejected.
bool IsMeaningfulDeviceSerial(
    const std::array<char, 17>& serial) noexcept;

bool SameDeviceSerial(
    const std::array<char, 17>& left,
    const std::array<char, 17>& right) noexcept;

// A retained non-empty SetupAPI instance ID remains mandatory when one was
// previously established. If none was available, exact case-insensitive HID
// path continuity is required instead. A retained meaningful firmware serial
// is an additional equality check; it never authorizes rebinding by itself.
bool MatchesRetainedDeviceIdentity(
    const std::wstring& retainedPath,
    const std::wstring& retainedInstanceId,
    const std::array<char, 17>& retainedSerial,
    bool retainedHasSerialEvidence,
    const std::wstring& candidatePath,
    const std::wstring& candidateInstanceId,
    const std::array<char, 17>& candidateSerial) noexcept;
}

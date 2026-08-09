#include "aula_win60he_session_policy.h"

#include <algorithm>
#include <cwctype>

namespace aula_win60he
{
namespace
{
bool EqualWindowsIdentity(
    const std::wstring& left,
    const std::wstring& right) noexcept
{
    if (left.empty() || right.empty() || left.size() != right.size())
        return false;
    for (std::size_t index = 0; index < left.size(); ++index)
    {
        if (std::towlower(left[index]) != std::towlower(right[index]))
            return false;
    }
    return true;
}

bool ContainsDuplicateIdentity(
    const std::vector<std::wstring>& values,
    bool ignoreEmpty) noexcept
{
    for (std::size_t left = 0; left < values.size(); ++left)
    {
        if (ignoreEmpty && values[left].empty())
            continue;
        for (std::size_t right = left + 1u; right < values.size(); ++right)
        {
            if (ignoreEmpty && values[right].empty())
                continue;
            if (EqualWindowsIdentity(values[left], values[right]))
                return true;
        }
    }
    return false;
}

struct CanonicalSerial
{
    std::array<char, 16> bytes{};
    std::size_t size = 0;
};

bool CanonicalFirmwareSerial(
    const std::array<char, 17>& serial,
    CanonicalSerial* out) noexcept
{
    if (out) *out = CanonicalSerial{};
    if (!out)
        return false;

    std::array<char, 16> value{};
    std::size_t valueSize = 0;
    bool paddingStarted = false;
    for (std::size_t index = 0; index + 1u < serial.size(); ++index)
    {
        const unsigned char byte =
            static_cast<unsigned char>(serial[index]);
        if (byte == 0u || byte == 0xFFu)
        {
            paddingStarted = true;
            continue;
        }
        if (paddingStarted)
        {
            // Non-padding bytes after a terminator are contradictory evidence.
            return false;
        }
        if (byte < 0x20u || byte > 0x7Eu)
            return false;
        value[valueSize++] = static_cast<char>(byte);
    }

    while (valueSize != 0u && value[valueSize - 1u] == ' ')
        --valueSize;
    std::size_t first = 0;
    while (first < valueSize && value[first] == ' ')
        ++first;
    if (first == valueSize)
        return false;
    out->size = valueSize - first;
    std::copy_n(value.begin() + static_cast<std::ptrdiff_t>(first),
        out->size, out->bytes.begin());
    return true;
}
}

DeviceSelectionPlan PlanDeviceSelection(
    const std::vector<std::wstring>& paths,
    const std::vector<std::wstring>& instanceIds,
    const std::wstring& retainedPath,
    const std::wstring& retainedInstanceId)
{
    DeviceSelectionPlan plan{};
    if (paths.empty() && instanceIds.empty())
        return plan;
    if (paths.size() != instanceIds.size())
    {
        plan.invalidEnumeration = true;
        return plan;
    }

    if (std::any_of(paths.begin(), paths.end(),
            [](const std::wstring& value) { return value.empty(); }) ||
        ContainsDuplicateIdentity(paths, false) ||
        ContainsDuplicateIdentity(instanceIds, true))
    {
        plan.invalidEnumeration = true;
        plan.ambiguous = true;
        return plan;
    }

    if (!retainedInstanceId.empty())
    {
        for (std::size_t index = 0; index < instanceIds.size(); ++index)
        {
            if (EqualWindowsIdentity(instanceIds[index], retainedInstanceId))
                plan.candidateIndices.push_back(index);
        }
        if (plan.candidateIndices.size() == 1u)
            return plan;
        plan.candidateIndices.clear();
        plan.retainedIdentityMissing = true;
        return plan;
    }

    if (!retainedPath.empty())
    {
        for (std::size_t index = 0; index < paths.size(); ++index)
        {
            if (EqualWindowsIdentity(paths[index], retainedPath))
                plan.candidateIndices.push_back(index);
        }
        if (plan.candidateIndices.size() == 1u)
            return plan;
        plan.candidateIndices.clear();
        plan.retainedIdentityMissing = true;
        return plan;
    }

    if (paths.size() == 1u)
        plan.candidateIndices.push_back(0u);
    else
        plan.ambiguous = true;
    return plan;
}

bool IsMeaningfulDeviceSerial(
    const std::array<char, 17>& serial) noexcept
{
    CanonicalSerial canonical{};
    return CanonicalFirmwareSerial(serial, &canonical);
}

bool SameDeviceSerial(
    const std::array<char, 17>& left,
    const std::array<char, 17>& right) noexcept
{
    CanonicalSerial canonicalLeft{};
    CanonicalSerial canonicalRight{};
    return CanonicalFirmwareSerial(left, &canonicalLeft) &&
        CanonicalFirmwareSerial(right, &canonicalRight) &&
        canonicalLeft.size == canonicalRight.size &&
        std::equal(canonicalLeft.bytes.begin(),
            canonicalLeft.bytes.begin() +
                static_cast<std::ptrdiff_t>(canonicalLeft.size),
            canonicalRight.bytes.begin());
}

bool MatchesRetainedDeviceIdentity(
    const std::wstring& retainedPath,
    const std::wstring& retainedInstanceId,
    const std::array<char, 17>& retainedSerial,
    bool retainedHasSerialEvidence,
    const std::wstring& candidatePath,
    const std::wstring& candidateInstanceId,
    const std::array<char, 17>& candidateSerial) noexcept
{
    if (!retainedInstanceId.empty())
    {
        if (!EqualWindowsIdentity(retainedInstanceId, candidateInstanceId))
            return false;
    }
    else if (!EqualWindowsIdentity(retainedPath, candidatePath))
    {
        return false;
    }

    if (!retainedHasSerialEvidence)
        return true;
    return SameDeviceSerial(retainedSerial, candidateSerial);
}
}

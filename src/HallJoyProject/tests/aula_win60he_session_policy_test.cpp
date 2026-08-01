#include "../HallJoy/aula_win60he_session_policy.h"

#include <array>
#include <cassert>
#include <iostream>
#include <vector>

namespace
{
using namespace aula_win60he;

std::array<char, 17> Serial(const char* text)
{
    std::array<char, 17> result{};
    if (!text)
        return result;
    for (std::size_t index = 0;
        text[index] != '\0' && index + 1u < result.size(); ++index)
        result[index] = text[index];
    return result;
}

void TestInitialSelection()
{
    const auto none = PlanDeviceSelection({}, {}, L"", L"");
    assert(none.candidateIndices.empty());
    assert(!none.ambiguous);
    assert(!none.invalidEnumeration);

    const auto one = PlanDeviceSelection(
        {L"path-a"}, {L"instance-a"}, L"", L"");
    assert((one.candidateIndices == std::vector<std::size_t>{0u}));
    assert(!one.ambiguous);

    const auto oneWithoutInstance = PlanDeviceSelection(
        {L"path-a"}, {L""}, L"", L"");
    assert((oneWithoutInstance.candidateIndices ==
        std::vector<std::size_t>{0u}));
    assert(!oneWithoutInstance.invalidEnumeration);

    const auto many = PlanDeviceSelection(
        {L"path-a", L"path-b"},
        {L"instance-a", L"instance-b"}, L"", L"");
    assert(many.candidateIndices.empty());
    assert(many.ambiguous);

    const auto manyWithoutInstances = PlanDeviceSelection(
        {L"path-a", L"path-b"}, {L"", L""}, L"", L"");
    assert(manyWithoutInstances.candidateIndices.empty());
    assert(manyWithoutInstances.ambiguous);
    assert(!manyWithoutInstances.invalidEnumeration);
}

void TestRetainedIdentitySelection()
{
    const auto retainedInstance = PlanDeviceSelection(
        {L"new-path-a", L"new-path-b"},
        {L"instance-a", L"INSTANCE-B"},
        L"old-path-b", L"instance-b");
    assert((retainedInstance.candidateIndices ==
        std::vector<std::size_t>{1u}));
    assert(!retainedInstance.ambiguous);
    assert(!retainedInstance.retainedIdentityMissing);

    const auto missingInstance = PlanDeviceSelection(
        {L"old-path"}, {L""}, L"old-path", L"old-instance");
    assert(missingInstance.candidateIndices.empty());
    assert(missingInstance.retainedIdentityMissing);

    const auto retainedPath = PlanDeviceSelection(
        {L"path-a", L"path-b"}, {L"", L""}, L"PATH-B", L"");
    assert((retainedPath.candidateIndices ==
        std::vector<std::size_t>{1u}));

    const auto driftedPath = PlanDeviceSelection(
        {L"path-new"}, {L""}, L"path-old", L"");
    assert(driftedPath.candidateIndices.empty());
    assert(driftedPath.retainedIdentityMissing);
}

void TestMalformedEnumerationFailsClosed()
{
    const auto sizeMismatch = PlanDeviceSelection(
        {L"path-a"}, {}, L"", L"");
    assert(sizeMismatch.candidateIndices.empty());
    assert(sizeMismatch.invalidEnumeration);

    const auto emptyPath = PlanDeviceSelection(
        {L""}, {L"instance-a"}, L"", L"");
    assert(emptyPath.candidateIndices.empty());
    assert(emptyPath.invalidEnumeration);

    const auto duplicatePath = PlanDeviceSelection(
        {L"path-a", L"PATH-A"},
        {L"instance-a", L"instance-b"}, L"", L"");
    assert(duplicatePath.candidateIndices.empty());
    assert(duplicatePath.invalidEnumeration);
    assert(duplicatePath.ambiguous);

    const auto duplicateInstance = PlanDeviceSelection(
        {L"path-a", L"path-b"},
        {L"same-instance", L"SAME-INSTANCE"}, L"", L"same-instance");
    assert(duplicateInstance.candidateIndices.empty());
    assert(duplicateInstance.invalidEnumeration);
    assert(duplicateInstance.ambiguous);
}

void TestFirmwareSerialEvidence()
{
    const auto empty = Serial("");
    const auto serialA = Serial("  AULA-0001  ");
    const auto serialACanonical = Serial("AULA-0001");
    const auto serialB = Serial("AULA-0002");
    const auto spaces = Serial("                ");
    std::array<char, 17> allFf{};
    allFf.fill(static_cast<char>(0xFF));
    std::array<char, 17> binary{};
    binary[0] = 1;
    std::array<char, 17> embeddedTerminator = Serial("AULA");
    embeddedTerminator[8] = 'X';

    assert(!IsMeaningfulDeviceSerial(empty));
    assert(!IsMeaningfulDeviceSerial(spaces));
    assert(!IsMeaningfulDeviceSerial(allFf));
    assert(!IsMeaningfulDeviceSerial(binary));
    assert(!IsMeaningfulDeviceSerial(embeddedTerminator));
    assert(IsMeaningfulDeviceSerial(serialA));
    assert(SameDeviceSerial(serialA, serialACanonical));
    assert(!SameDeviceSerial(serialA, serialB));

    assert(MatchesRetainedDeviceIdentity(
        L"old-path", L"instance-a", serialA, true,
        L"new-path", L"INSTANCE-A", serialACanonical));
    assert(!MatchesRetainedDeviceIdentity(
        L"old-path", L"instance-a", serialA, true,
        L"old-path", L"instance-b", serialACanonical));
    assert(!MatchesRetainedDeviceIdentity(
        L"old-path", L"instance-a", serialA, true,
        L"old-path", L"instance-a", serialB));

    assert(MatchesRetainedDeviceIdentity(
        L"path-a", L"", empty, false,
        L"PATH-A", L"", empty));
    assert(MatchesRetainedDeviceIdentity(
        L"path-a", L"", serialA, true,
        L"PATH-A", L"new-instance", serialACanonical));
    assert(!MatchesRetainedDeviceIdentity(
        L"path-a", L"", empty, false,
        L"path-b", L"", empty));
    assert(!MatchesRetainedDeviceIdentity(
        L"", L"", empty, false,
        L"", L"", empty));
}
}

int main()
{
    TestInitialSelection();
    TestRetainedIdentitySelection();
    TestMalformedEnumerationFailsClosed();
    TestFirmwareSerialEvidence();
    std::cout << "AULA_WIN60HE_SESSION_POLICY_TEST=PASS\n";
    return 0;
}

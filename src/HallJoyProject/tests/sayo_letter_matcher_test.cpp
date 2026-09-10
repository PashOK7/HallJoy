#include "sayo_letter_matcher.h"

#include <array>
#include <cassert>
#include <cstdint>
#include <initializer_list>
#include <iostream>

namespace
{
using halljoy::sayo::SayoLetterMatcher;

constexpr std::uint64_t kWindowMs = 80u;

class KeyboardReportModel final
{
public:
    std::uint8_t OnReport(std::initializer_list<std::uint8_t> usages,
        SayoLetterMatcher& matcher, std::uint64_t nowMs)
    {
        std::array<bool, 256> current{};
        std::uint8_t addedHid = 0u;
        unsigned addedCount = 0u;
        for (const std::uint8_t hid : usages)
        {
            if (hid == 0u)
                continue;
            current[hid] = true;
            if (!down_[hid])
            {
                addedHid = hid;
                ++addedCount;
            }
        }
        down_ = current;
        if (addedCount == 1u && addedHid != 0u)
            return matcher.TakeUniqueCandidate(nowMs, kWindowMs);
        matcher.Expire(nowMs, kWindowMs);
        return SayoLetterMatcher::kNoCandidate;
    }

private:
    std::array<bool, 256> down_{};
};

void TestSingleAndReverseOrder()
{
    SayoLetterMatcher matcher;
    KeyboardReportModel keyboard;
    matcher.ObservePhysicalEdge(0u, true, 0u);
    assert(keyboard.OnReport({ 0x04u }, matcher, 1u) == 0u);
    matcher.ObservePhysicalEdge(1u, true, 100u);
    assert(keyboard.OnReport({ 0x04u, 0x05u }, matcher, 105u) == 1u);

    matcher.ObservePhysicalEdge(2u, true, 200u);
    assert(keyboard.OnReport({ 0x04u, 0x05u, 0x06u }, matcher, 220u) == 2u);
}

void TestCrossReportAmbiguityDoesNotMislearn()
{
    SayoLetterMatcher matcher;
    KeyboardReportModel keyboard;
    matcher.ObservePhysicalEdge(0u, true, 100u); // physical A
    matcher.ObservePhysicalEdge(1u, true, 110u); // physical B replaces old global pending
    assert(keyboard.OnReport({ 0x04u }, matcher, 120u) == SayoLetterMatcher::kNoCandidate);
    // A second split keyboard report is still ambiguous while both physical
    // presses remain held; neither letter is assigned to the wrong key.
    assert(keyboard.OnReport({ 0x04u, 0x05u }, matcher, 125u) == SayoLetterMatcher::kNoCandidate);

    matcher.ObservePhysicalEdge(0u, false, 130u);
    matcher.ObservePhysicalEdge(1u, false, 131u);
    assert(keyboard.OnReport({}, matcher, 132u) == SayoLetterMatcher::kNoCandidate);
    matcher.ObservePhysicalEdge(1u, true, 140u);
    assert(keyboard.OnReport({ 0x05u }, matcher, 145u) == 1u);
}

void TestCombinedReportAutoRepeatAndDuplicateLetter()
{
    SayoLetterMatcher matcher;
    KeyboardReportModel keyboard;
    matcher.ObservePhysicalEdge(0u, true, 100u);
    assert(keyboard.OnReport({ 0x04u, 0x05u }, matcher, 105u) == SayoLetterMatcher::kNoCandidate);
    assert(keyboard.OnReport({}, matcher, 106u) == SayoLetterMatcher::kNoCandidate);
    assert(keyboard.OnReport({ 0x04u }, matcher, 110u) == 0u);

    matcher.ObservePhysicalEdge(2u, true, 200u);
    assert(keyboard.OnReport({ 0x04u, 0x06u }, matcher, 220u) == 2u);
    // Auto-repeat / an already-down keyboard usage creates no new candidate query.
    assert(keyboard.OnReport({ 0x04u, 0x06u }, matcher, 225u) == SayoLetterMatcher::kNoCandidate);
}

void TestHeldNeighbourTimeoutAndReset()
{
    SayoLetterMatcher matcher;
    KeyboardReportModel keyboard;
    matcher.ObservePhysicalEdge(0u, true, 100u);
    matcher.ObservePhysicalEdge(1u, true, 110u);
    matcher.ObservePhysicalEdge(1u, false, 120u);
    assert(keyboard.OnReport({ 0x04u }, matcher, 130u) == 0u);

    matcher.ObservePhysicalEdge(2u, true, 200u);
    assert(keyboard.OnReport({ 0x04u, 0x05u }, matcher, 281u) == SayoLetterMatcher::kNoCandidate);
    matcher.ObservePhysicalEdge(2u, true, 300u);
    matcher.Reset(); // reconnect/session reset
    assert(keyboard.OnReport({ 0x04u, 0x05u, 0x06u }, matcher, 305u) == SayoLetterMatcher::kNoCandidate);
}

void TestIdenticalUserBindingsRemainValid()
{
    SayoLetterMatcher matcher;
    KeyboardReportModel keyboard;
    matcher.ObservePhysicalEdge(0u, true, 100u);
    assert(keyboard.OnReport({ 0x07u }, matcher, 105u) == 0u);
    matcher.ObservePhysicalEdge(0u, false, 110u);
    assert(keyboard.OnReport({}, matcher, 111u) == SayoLetterMatcher::kNoCandidate);
    matcher.ObservePhysicalEdge(1u, true, 120u);
    // Two physical keys may intentionally share one configured letter; the
    // temporal correlation still identifies each edge independently.
    assert(keyboard.OnReport({ 0x07u }, matcher, 125u) == 1u);
}
} // namespace

int main()
{
    TestSingleAndReverseOrder();
    TestCrossReportAmbiguityDoesNotMislearn();
    TestCombinedReportAutoRepeatAndDuplicateLetter();
    TestHeldNeighbourTimeoutAndReset();
    TestIdenticalUserBindingsRemainValid();
    std::cout << "SAYO_LETTER_MATCHER_TEST=PASS old_bug=blocked ambiguity=fail_closed\n";
    return 0;
}

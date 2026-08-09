#include <array>
#include <cassert>
#include <cstdint>

#include "../HallJoy/latest_value_mailbox.h"

namespace
{
struct ReportBatch
{
    std::uint8_t count = 0;
    std::uint8_t validMask = 0;
    std::array<std::uint32_t, 4> reports{};
};
}

int main()
{
    using Mailbox = halljoy::output::LatestValueMailbox<ReportBatch>;
    Mailbox mailbox;
    ReportBatch observed{};
    std::uint64_t consumed = 0;
    std::uint64_t published = 0;

    assert(mailbox.TryReadAfter(consumed, &observed, &published) ==
        Mailbox::ReadResult::Unchanged);

    const ReportBatch first{ 2, 0x03, { 10, 20, 0, 0 } };
    assert(mailbox.TryPublish(first, &published));
    assert(published == 1);
    assert(mailbox.TryReadAfter(consumed, &observed, &consumed) ==
        Mailbox::ReadResult::Updated);
    assert(observed.count == first.count);
    assert(observed.reports == first.reports);

    // Multiple realtime publications before the output worker runs coalesce to
    // the newest complete batch, matching HallJoy's existing scheduler policy.
    const ReportBatch intermediate{ 3, 0x01, { 30, 31, 32, 0 } };
    const ReportBatch newest{ 3, 0x02, { 40, 50, 60, 0 } };
    assert(mailbox.TryPublish(intermediate));
    assert(mailbox.TryPublishMerged(newest,
        [](const ReportBatch& pending, ReportBatch& next) {
            next.validMask = static_cast<std::uint8_t>(next.validMask | pending.validMask);
        }));
    assert(mailbox.TryReadAfter(consumed, &observed, &consumed) ==
        Mailbox::ReadResult::Updated);
    assert(observed.count == newest.count);
    assert(observed.reports == newest.reports);
    assert(observed.validMask == 0x03);
    assert(consumed == mailbox.PublishedGeneration());
    assert(mailbox.TryReadAfter(consumed, &observed, &published) ==
        Mailbox::ReadResult::Unchanged);
}

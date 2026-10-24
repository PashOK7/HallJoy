#pragma once
// Reconciles the RongYuan change-driven travel stream with the keyboard's own travel table.
//
// Tester log 2026-10-21 (MonsGeek FUN60 Ultra TMR, board 2307): after releasing several keys
// at once, a key sometimes stayed at a small depth (about 75 of 1000) for seconds. Stock
// firmware 2307 v306: the stream producer writes each key's travel into a RAM table
// (0x20009050 + 0x10BC) and sends one 0x1B report per change; below 20 units it writes 0 and
// sends a single zero report. A lost report is never repeated, but the table holds the value
// the firmware meant to send, and command E5 FE reads that table (32 keys per page).
//
// The reader thread owns publication: it records every stream sample (Stream) and applies a
// table value only when no newer stream sample of that key arrived since the table read
// started (Accept). The reconciler thread reads only pages that hold a key whose value is
// non-zero and not yet confirmed by the table. No timer decides anything; reads are paced by
// the vendor's command exchange. Pure logic, unit-tested in rongyuan_stream_protocol_test.cpp.
#include <array>
#include <atomic>
#include <cstdint>
#include <vector>

namespace halljoy::ry_stream {
class Reconcile {
public:
    static constexpr unsigned kSlots = 128, kPageSlots = 32, kPages = kSlots / kPageSlots;
    struct Correction {
        std::uint8_t slot = 0;
        std::uint32_t seq = 0;   // the key's sample count when the table read started
        std::uint16_t raw = 0;   // the table value
        std::uint16_t stream = 0;  // the published value at the time of the read
    };
    using Seqs = std::array<std::uint32_t, kPageSlots>;

    void Reset() noexcept {
        for (auto& v : raw_) v.store(0);
        for (auto& v : seq_) v.store(0);
        checked_.fill(0);
    }

    // Reader thread: a published value of `slot` (a stream sample or an accepted correction).
    void Stream(unsigned slot, std::uint16_t raw) noexcept {
        if (slot >= kSlots) return;
        raw_[slot].store(raw);
        seq_[slot].fetch_add(1);
    }

    // Reconciler thread: the first page holding a non-zero key whose latest value the table
    // has not confirmed yet, or -1 when every key is idle or confirmed.
    int NextPage() const noexcept {
        for (unsigned page = 0; page < kPages; ++page)
            for (unsigned i = 0; i < kPageSlots; ++i) {
                const unsigned slot = page * kPageSlots + i;
                if (raw_[slot].load() && seq_[slot].load() != checked_[slot]) return static_cast<int>(page);
            }
        return -1;
    }

    // Reconciler thread: sample counts of a page, taken before its table read is sent.
    Seqs Before(unsigned page) const noexcept {
        Seqs seqs{};
        for (unsigned i = 0; i < kPageSlots && page < kPages; ++i) seqs[i] = seq_[page * kPageSlots + i].load();
        return seqs;
    }

    // Reconciler thread: the table values of `page`. Every key of the page counts as checked
    // at its pre-read sample count; differing values become corrections for the reader.
    void Table(unsigned page, const Seqs& seqs, const std::uint16_t* table, std::vector<Correction>& out) {
        if (page >= kPages || !table) return;
        for (unsigned i = 0; i < kPageSlots; ++i) {
            const unsigned slot = page * kPageSlots + i;
            checked_[slot] = seqs[i];
            const std::uint16_t published = raw_[slot].load();
            if (published != table[i])
                out.push_back({static_cast<std::uint8_t>(slot), seqs[i], table[i], published});
        }
    }

    // Reader thread: a correction applies only when the key got no newer stream sample.
    bool Accept(const Correction& c) const noexcept {
        return c.slot < kSlots && seq_[c.slot].load() == c.seq && raw_[c.slot].load() != c.raw;
    }

private:
    std::array<std::atomic<std::uint16_t>, kSlots> raw_{};
    std::array<std::atomic<std::uint32_t>, kSlots> seq_{};
    std::array<std::uint32_t, kSlots> checked_{};  // reconciler thread only
};
}  // namespace halljoy::ry_stream

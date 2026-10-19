#include "../HallJoy/melgeek_made68air_protocol.h"

#include <array>
#include <cassert>
#include <cstdio>
#include <string>

int main()
{
    namespace mp = halljoy::melgeek_made68air;

    assert(mp::SelfTest());
    assert(mp::kVendorId == 0x0416 && mp::kProductId == 0x0114);

    // CRC-16/CCITT-FALSE: the standard check value, and the empty payload is FF FF (HIVE core).
    {
        const std::uint8_t check[] = {'1', '2', '3', '4', '5', '6', '7', '8', '9'};
        assert(mp::Crc16(check, sizeof(check)) == 0x29b1);
        assert(mp::Crc16(nullptr, 0) == 0xffff);
        static_assert(mp::Crc16(nullptr, 0) == 0xffff);
    }

    // Safety contract: only the listed empty-payload getters and the guarded report-mode setter can be
    // built; everything else is refused.
    {
        assert(mp::kGetters.size() == 6);
        std::array<std::uint8_t, mp::kFrameBytes> frame{};
        for (const auto& getter : mp::kGetters) {
            assert(mp::IsAllowedGetter(getter.module, getter.op));
            for (const bool crc : {true, false}) {
                assert(mp::MakeGetterFrame(getter.module, getter.op, crc, frame));
                assert(frame[0] == getter.module && frame[1] == getter.op);
                assert(frame[2] == 0 && frame[3] == 0 && frame[4] == 0 && frame[5] == 0);  // empty payload
                assert(frame[6] == (crc ? 0xff : 0x00) && frame[7] == (crc ? 0xff : 0x00));
                for (std::size_t i = mp::kHeaderBytes; i < frame.size(); ++i) assert(frame[i] == 0);
            }
        }
        // Setters, calibration/interaction commands and arbitrary bytes are never buildable.
        unsigned allowed = 0;
        for (unsigned module = 0; module < 256; ++module)
            for (unsigned op = 0; op < 256; ++op) {
                std::array<std::uint8_t, mp::kFrameBytes> probe{};
                probe.fill(0x5a);
                const bool ok = mp::MakeGetterFrame(static_cast<std::uint8_t>(module),
                                                    static_cast<std::uint8_t>(op), true, probe);
                if (ok) ++allowed;
                else for (const auto b : probe) assert(b == 0x5a);   // output untouched on refusal
            }
        assert(allowed == mp::kGetters.size());
        // The report-mode setter: exactly the values 0 (off), 1 (mode 1, preferred since rev 6) and 2 (live), frame [0A 93 01 00 00 00 crc] + mode.
        unsigned setters = 0;
        for (unsigned mode = 0; mode < 256; ++mode) {
            std::array<std::uint8_t, mp::kFrameBytes> out{};
            out.fill(0x5a);
            const bool ok = mp::MakeSetReportFrame(static_cast<std::uint8_t>(mode), true, out);
            if (!ok) { for (const auto b : out) assert(b == 0x5a); continue; }
            ++setters;
            assert(mode == 0 || mode == 1 || mode == 2);
            assert(out[0] == 0x0a && out[1] == 0x93 && out[2] == 1 && out[3] == 0 && out[4] == 0 && out[5] == 0);
            assert(out[8] == mode);
            const std::uint8_t data[1] = {static_cast<std::uint8_t>(mode)};
            assert((out[6] | (out[7] << 8)) == mp::Crc16(data, 1));
            for (std::size_t i = 9; i < out.size(); ++i) assert(out[i] == 0);
            std::array<std::uint8_t, mp::kFrameBytes> plain{};
            assert(mp::MakeSetReportFrame(static_cast<std::uint8_t>(mode), false, plain));
            assert(plain[6] == 0 && plain[7] == 0 && plain[8] == mode);
        }
        assert(setters == 3 && mp::kReportOff == 0 && mp::kReportMode1 == 1 && mp::kPreferredMode == 1 && mp::kReportLive == 2);
        // Round trip through the receiver rules.
        {
            std::array<std::uint8_t, mp::kFrameBytes> live{};
            assert(mp::MakeSetReportFrame(mp::kReportLive, true, live));
            const auto h = mp::ParseHeader(live.data(), live.size());
            assert(h.module == 0x0a && h.op == 0x93 && h.length == 1 && h.state == mp::CrcState::Ok);
        }
        // The getter reply of the report mode (observed: 0A 14, one byte, CRC E1F0 for value 0).
        {
            std::array<std::uint8_t, mp::kFrameBytes> reply{};
            reply[0] = 0x0a; reply[1] = 0x14; reply[2] = 1; reply[6] = 0xf0; reply[7] = 0xe1; reply[8] = 0x00;
            assert(mp::ReportModeFromReply(reply.data(), reply.size()) == 0);
            reply[8] = 2;
            assert(mp::ReportModeFromReply(reply.data(), reply.size()) == -1);   // CRC no longer matches
            const std::uint8_t two[1] = {2};
            const auto crc = mp::Crc16(two, 1);
            reply[6] = static_cast<std::uint8_t>(crc & 0xff); reply[7] = static_cast<std::uint8_t>(crc >> 8);
            assert(mp::ReportModeFromReply(reply.data(), reply.size()) == 2);
            reply[1] = 0x94;                                                     // wrong op
            assert(mp::ReportModeFromReply(reply.data(), reply.size()) == -1);
            assert(mp::ReportModeFromReply(nullptr, 0) == -1);
            assert(mp::ReplyOp(0x87) == 0x07 && mp::ReplyOp(0x8b) == 0x0b && mp::ReplyOp(0x81) == 0x01 &&
                   mp::ReplyOp(0x94) == 0x14 && mp::ReplyOp(0x97) == 0x17 && mp::ReplyOp(0xa3) == 0x23 &&
                   mp::ReplyOp(0x93) == 0x13);
        }
        assert(!mp::IsAllowedGetter(0x0a, 0x93));  // set report is built only by MakeSetReportFrame
        assert(!mp::IsAllowedGetter(0xde, 0x86));  // ADC log lookup
        assert(!mp::IsAllowedGetter(0x01, 0x8e));
        assert(!mp::IsAllowedGetter(0x0a, 0x99));
    }

    // Header parsing follows the receiver rules.
    {
        std::array<std::uint8_t, 64> frame{};
        frame[0] = 0x01; frame[1] = 0x87; frame[2] = 3;
        frame[8] = 1; frame[9] = 2; frame[10] = 3;
        const auto crc = mp::Crc16(frame.data() + 8, 3);
        frame[6] = static_cast<std::uint8_t>(crc & 0xff); frame[7] = static_cast<std::uint8_t>(crc >> 8);
        auto header = mp::ParseHeader(frame.data(), frame.size());
        assert(header.module == 1 && header.op == 0x87 && header.length == 3 &&
               header.state == mp::CrcState::Ok && header.crc == crc);
        frame[9] ^= 0x40;
        assert(mp::ParseHeader(frame.data(), frame.size()).state == mp::CrcState::Bad);
        frame[6] = frame[7] = 0;
        assert(mp::ParseHeader(frame.data(), frame.size()).state == mp::CrcState::Skipped);
        assert(mp::ParseHeader(frame.data(), 7).state == mp::CrcState::TooShort);
        frame[2] = 60;  // longer than the bytes provided
        assert(mp::ParseHeader(frame.data(), 20).state == mp::CrcState::TooShort);
        assert(mp::ParseHeader(nullptr, 64).state == mp::CrcState::TooShort);
    }

    // Collection roles: only vendor pages are written; keyboard and mouse are never opened.
    {
        assert(mp::Classify(0xff60, 0x61) == mp::Role::Vendor && mp::IsWritten(mp::Role::Vendor));
        assert(mp::Classify(0x01, 0x06) == mp::Role::Keyboard && !mp::IsOpened(mp::Role::Keyboard));
        assert(mp::Classify(0x01, 0x02) == mp::Role::Mouse && !mp::IsOpened(mp::Role::Mouse));
        assert(mp::Classify(0x01, 0x05) == mp::Role::Gamepad && mp::IsOpened(mp::Role::Gamepad));
        assert(mp::Classify(0x01, 0x04) == mp::Role::Gamepad);
        assert(mp::Classify(0x0c, 0x01) == mp::Role::Consumer && mp::IsOpened(mp::Role::Consumer));
        assert(mp::Classify(0x01, 0x80) == mp::Role::SystemControl);
        assert(mp::Classify(0x0d, 0x01) == mp::Role::Other);
        for (const auto role : {mp::Role::Consumer, mp::Role::SystemControl, mp::Role::Gamepad,
                                mp::Role::Other, mp::Role::Keyboard, mp::Role::Mouse})
            assert(!mp::IsWritten(role));
    }

    // Statistics: constant bytes stay quiet, changing bytes report range and change count.
    {
        mp::ReportStats stats;
        std::array<std::uint8_t, 8> report{0x00, 0xaa, 0x10, 0x00, 0x00, 0x77, 0x00, 0x01};
        stats.Add(report.data(), report.size());
        assert(stats.frames == 1 && stats.length == 8 && stats.VaryingCount() == 0);
        report[2] = 0x40; report[3] = 0x01;
        stats.Add(report.data(), report.size());
        report[2] = 0x20;
        stats.Add(report.data(), report.size());
        report[2] = 0x20;  // repeat: no new change
        stats.Add(report.data(), report.size());
        assert(stats.frames == 4 && stats.VaryingCount() == 2);
        assert(stats.Varies(2) && stats.Min(2) == 0x10 && stats.Max(2) == 0x40 && stats.Changes(2) == 2);
        assert(stats.Varies(3) && stats.Min(3) == 0 && stats.Max(3) == 1 && stats.Changes(3) == 1);
        assert(!stats.Varies(1) && !stats.Varies(7) && !stats.Varies(99));
        // A longer report extends the tracked length without false changes in the old bytes.
        std::array<std::uint8_t, 10> longer{0x00, 0xaa, 0x20, 0x01, 0x00, 0x77, 0x00, 0x01, 0x05, 0x06};
        stats.Add(longer.data(), longer.size());
        assert(stats.length == 10 && !stats.Varies(8) && !stats.Varies(9));
        // Oversized input is bounded.
        std::array<std::uint8_t, 300> big{};
        mp::ReportStats bounded;
        bounded.Add(big.data(), big.size());
        assert(bounded.length == mp::ReportStats::kMaxBytes);
    }

    // The flush schedule grows and always advances.
    {
        std::uint64_t at = 1;
        unsigned flushes = 0;
        while (at < 1000000) { const auto next = mp::NextFlushAt(at); assert(next > at); at = next; ++flushes; }
        assert(flushes < 100);
        assert(mp::NextFlushAt(1) == 2 && mp::NextFlushAt(4) == 5 && mp::NextFlushAt(100) == 125);
    }

    // Frame hash is stable and sensitive to every byte.
    {
        const std::uint8_t a[] = {1, 2, 3, 4}, b[] = {1, 2, 3, 5};
        assert(mp::Fnv1a(a, 4) == mp::Fnv1a(a, 4) && mp::Fnv1a(a, 4) != mp::Fnv1a(b, 4));
    }

    // Live stream frames (module 08, op 09): a real frame from the tester log and synthetic ones.
    {
        const auto fromHex = [](const char* hex) {
            std::array<std::uint8_t, mp::kFrameBytes> out{};
            for (std::size_t i = 0; i < out.size() && hex[2 * i]; ++i) {
                const auto nibble = [](char ch) { return ch <= '9' ? ch - '0' : ch - 'a' + 10; };
                out[i] = static_cast<std::uint8_t>(nibble(hex[2 * i]) * 16 + nibble(hex[2 * i + 1]));
            }
            return out;
        };
        // First live frame of the tester log (idle keys 0..13, ADC around 3200).
        const auto real = fromHex(
            "0809380000006b080000790c0100d60c0200880d0300c20d0400f50c0500fd0c0600280c0700120d0800b80c"
            "0900050d0a008f0c0b00e60c0c00cc0c0d00ca0d");
        mp::KeyReading out[mp::kMaxEntries];
        int n = mp::DecodeStreamFrame(real.data(), real.size(), out);
        assert(n == 14);
        assert(out[0].id == 0 && out[0].position == 0 && out[0].adc == 0x0c79);
        assert(out[13].id == 13 && out[13].position == 0 && out[13].adc == 0x0dca);
        assert(mp::ParseHeader(real.data(), real.size()).state == mp::CrcState::Ok);

        // Builder for synthetic frames with the right CRC.
        const auto build = [](std::initializer_list<std::array<std::uint8_t, 4>> entries) {
            std::array<std::uint8_t, mp::kFrameBytes> f{};
            f[0] = 0x08; f[1] = 0x09; f[2] = static_cast<std::uint8_t>(entries.size() * 4);
            std::size_t at = mp::kHeaderBytes;
            for (const auto& e : entries) for (const auto b : e) f[at++] = b;
            const auto crc = mp::Crc16(f.data() + mp::kHeaderBytes, entries.size() * 4);
            f[6] = static_cast<std::uint8_t>(crc & 0xff); f[7] = static_cast<std::uint8_t>(crc >> 8);
            return f;
        };
        // Pressed keys as seen in the log: id 18 position 20 ADC 0x0b39, id 34 position 46 ADC 0x0928.
        auto f = build({{{18, 20, 0x39, 0x0b}}, {{34, 46, 0x28, 0x09}}, {{67, 0, 0xf6, 0x0c}}});
        n = mp::DecodeStreamFrame(f.data(), f.size(), out);
        assert(n == 3);
        assert(out[0].id == 18 && out[0].position == 20 && out[0].adc == 0x0b39);
        assert(out[1].id == 34 && out[1].position == 46 && out[1].adc == 0x0928);
        assert(out[2].id == 67 && out[2].position == 0);
        // Rejected whole: id outside the 68 keys, wrong module/op, bad CRC, odd length, empty frame, short input.
        auto bad = build({{{68, 0, 0, 0}}});
        assert(mp::DecodeStreamFrame(bad.data(), bad.size(), out) == -1);
        bad = build({{{1, 0, 0, 0}}, {{200, 0, 0, 0}}});
        assert(mp::DecodeStreamFrame(bad.data(), bad.size(), out) == -1);   // a bad entry drops the whole frame
        bad = f; bad[1] = 0x0a;
        assert(mp::DecodeStreamFrame(bad.data(), bad.size(), out) == -1);
        bad = f; bad[9] ^= 1;
        assert(mp::DecodeStreamFrame(bad.data(), bad.size(), out) == -1);
        bad = f; bad[2] = 11;
        assert(mp::DecodeStreamFrame(bad.data(), bad.size(), out) == -1);
        bad = f; bad[2] = 0;
        assert(mp::DecodeStreamFrame(bad.data(), bad.size(), out) == -1);
        assert(mp::DecodeStreamFrame(f.data(), 10, out) == -1);
        assert(mp::DecodeStreamFrame(nullptr, 64, out) == -1);
        // A position above the bottom is clamped.
        auto over = build({{{5, 200, 0, 0}}});
        assert(mp::DecodeStreamFrame(over.data(), over.size(), out) == 1 && out[0].position == mp::kMaxPosition);
        // 14 entries is the maximum; the unused tail of a 48-byte frame is not read.
        assert(mp::kMaxEntries == 14 && mp::kKeyCount == 68);

        // Position to depth: 0 -> 0, 46 -> 1000, monotonic, no value above 1000.
        assert(mp::PositionToMilli(0) == 0 && mp::PositionToMilli(46) == 1000 && mp::PositionToMilli(255) == 1000);
        for (unsigned pos = 1; pos <= 46; ++pos) assert(mp::PositionToMilli(static_cast<std::uint8_t>(pos)) >
                                                        mp::PositionToMilli(static_cast<std::uint8_t>(pos - 1)));
    }

    // Key map: the table (verified on hardware) and the checking/learning of Raw Input key-downs.
    {
        // Pairs the slow sequential tester log (message (8)) established: 65 ids, all equal to the table.
        const std::uint16_t verified[][2] = {{0, 0x29}, {1, 0x1e}, {9, 0x26}, {10, 0x27}, {11, 0x2d}, {12, 0x2e}, {13, 0x2a},
                                              {14, 0x35}, {15, 0x2b}, {17, 0x1a}, {19, 0x15}, {20, 0x17}, {21, 0x1c},
                                              {22, 0x18}, {23, 0x0c}, {28, 0x31}, {29, 0x4c}, {30, 0x39}, {31, 0x04},
                                              {32, 0x16}, {33, 0x07}, {34, 0x09}, {35, 0x0a}, {36, 0x0b}, {37, 0x0d},
                                              {38, 0x0e}, {42, 0x28}, {43, 0x4b}, {44, 0xe1}, {49, 0x05}, {55, 0xe5},
                                              {56, 0x52}, {57, 0x4e}, {58, 0xe0}, {60, 0xe2}, {61, 0x2c}, {62, 0xe6},
                                              {64, 0xe4}, {65, 0x50}, {66, 0x51}, {67, 0x4f}};
        for (const auto& pair : verified) assert(mp::kPriorKeyMap[pair[0]] == pair[1]);
        assert(mp::PriorIdOfHid(0x2c) == 61 && mp::PriorIdOfHid(0x11) == 50 && mp::PriorIdOfHid(0x10) == 51 &&
               mp::PriorIdOfHid(0x35) == 14 && mp::PriorIdOfHid(0x4a) == -1 && mp::PriorIdOfHid(0) == -1);
        assert(mp::kPriorKeyMap[63] == halljoy::keycode::kFn);   // Fn: no Windows key event, from the photo

        std::array<std::uint8_t, mp::kKeyCount> position{};
        unsigned learnedCalls = 0, confirmed = 0, firsts = 0, conflicts = 0;
        std::int64_t lastLag = 0;
        std::uint16_t lastLearnedHid = 0; std::uint8_t lastLearnedId = 255, lastConflictId = 255;
        const auto learn = [&](std::uint8_t id, std::uint16_t hid) { ++learnedCalls; lastLearnedId = id; lastLearnedHid = hid; };
        const auto confirm = [&](std::uint8_t, std::uint16_t, std::int64_t lag, bool first) {
            ++confirmed; lastLag = lag; if (first) ++firsts;
        };
        const auto conflict = [&](std::uint8_t id, std::uint16_t) { ++conflicts; lastConflictId = id; };

        // A key the table names: the key-down confirms it, also when the stream sample comes 60 ms later.
        mp::KeyLearner l;
        assert(l.KnownCount() == 68 && l.LearnedCount() == 0);
        position[33] = 5; l.OnPosition(33, 0, 5, 1000);
        l.OnKeyDown(0x07, 1030);                              // D
        l.Resolve(position, 1040, learn, confirm, conflict);
        assert(confirmed == 1 && firsts == 1 && lastLag == -30 && learnedCalls == 0 && conflicts == 0);
        l.OnKeyDown(0x07, 1100);                              // auto-repeat of the held key: ignored
        l.Resolve(position, 1110, learn, confirm, conflict);
        l.Resolve(position, 1300, learn, confirm, conflict);
        assert(confirmed == 1 && conflicts == 0);

        mp::KeyLearner late;
        late.OnKeyDown(0x1a, 2000);                           // W, key-down first
        late.Resolve(position, 2010, learn, confirm, conflict);
        assert(confirmed == 1);                               // nothing moved yet: pending
        std::array<std::uint8_t, mp::kKeyCount> p2{};
        p2[17] = 9; late.OnPosition(17, 0, 9, 2060);
        late.Resolve(p2, 2070, learn, confirm, conflict);
        assert(confirmed == 2 && lastLag == 60 && conflicts == 0);   // the stream was 60 ms behind

        // The mispair seen in the fast log: K moves, then N is pressed before N's own sample arrived. N is a mapped
        // key (id 50), so it can never be assigned to K's id; both keys are confirmed in the end.
        mp::KeyLearner rapid;
        std::array<std::uint8_t, mp::kKeyCount> p3{};
        p3[38] = 6; rapid.OnPosition(38, 0, 6, 3000);
        rapid.OnKeyDown(0x0e, 3020);                          // K
        rapid.OnKeyDown(0x11, 3080);                          // N, its sample is late
        rapid.Resolve(p3, 3090, learn, confirm, conflict);
        assert(confirmed == 3 && learnedCalls == 0 && conflicts == 0);
        p3[50] = 7; rapid.OnPosition(50, 0, 7, 3130);
        rapid.Resolve(p3, 3140, learn, confirm, conflict);
        assert(confirmed == 4 && learnedCalls == 0 && conflicts == 0 && rapid.LearnedCount() == 0);

        // A map with gaps (as the first versions had): usages the map does not know are learned for the single
        // unexplained moving id without a usage.
        auto sparse = mp::kPriorKeyMap;
        sparse[0] = 0; sparse[14] = 0; sparse[29] = 0;
        mp::KeyLearner esc; esc.prior = &sparse;
        assert(esc.KnownCount() == 65);
        std::array<std::uint8_t, mp::kKeyCount> p4{};
        p4[0] = 5; esc.OnPosition(0, 0, 5, 4000);
        esc.OnKeyDown(0x29, 4040);
        esc.Resolve(p4, 4050, learn, confirm, conflict);
        assert(learnedCalls == 1 && lastLearnedId == 0 && lastLearnedHid == 0x29 && esc.Hid(0) == 0x29);
        assert(esc.KnownCount() == 66 && esc.LearnedCount() == 1);
        // ... and not when two ids without a usage move together (chord): no guess, the key-down expires.
        mp::KeyLearner chord; chord.prior = &sparse;
        std::array<std::uint8_t, mp::kKeyCount> p5{};
        p5[14] = 4; chord.OnPosition(14, 0, 4, 5000);
        p5[29] = 4; chord.OnPosition(29, 0, 4, 5010);
        chord.OnKeyDown(0x4c, 5020);                          // Del
        chord.Resolve(p5, 5030, learn, confirm, conflict);
        chord.Resolve(p5, 5020 + mp::KeyLearner::kLearnAfterMs + 1, learn, confirm, conflict);
        assert(learnedCalls == 1 && chord.LearnedCount() == 0);
        // A mapped key that moves at the same time is explained by its own key-down and is not a candidate.
        mp::KeyLearner mixed; mixed.prior = &sparse;
        std::array<std::uint8_t, mp::kKeyCount> p6{};
        p6[38] = 6; mixed.OnPosition(38, 0, 6, 6000);          // K
        p6[14] = 3; mixed.OnPosition(14, 0, 3, 6010);          // id without a usage
        mixed.OnKeyDown(0x0e, 6020);                          // K confirms id 38 (claimed)
        mixed.OnKeyDown(0x4c, 6030);                          // Del: only id 14 is left
        mixed.Resolve(p6, 6040, learn, confirm, conflict);
        assert(learnedCalls == 2 && lastLearnedId == 14 && lastLearnedHid == 0x4c);
        assert(mixed.LearnedCount() == 1 && mixed.learned[38] == 0);

        // A wrong table entry shows up as a conflict and is never rewritten: L is pressed, but id 40 moves.
        mp::KeyLearner wrong;
        std::array<std::uint8_t, mp::kKeyCount> p7{};
        p7[40] = 8; wrong.OnPosition(40, 0, 8, 7000);
        wrong.OnKeyDown(0x0f, 7020);                          // L, the table puts L on id 39
        wrong.Resolve(p7, 7030, learn, confirm, conflict);
        assert(conflicts == 0);                                // still waiting for id 39
        wrong.Resolve(p7, 7020 + mp::KeyLearner::kLearnAfterMs + 1, learn, confirm, conflict);
        assert(conflicts == 1 && lastConflictId == 40 && wrong.Hid(40) == 0x33 && wrong.Hid(39) == 0x0f);

        // Far outside the window nothing is paired; a released key is never a candidate; ids >= 68 are ignored.
        mp::KeyLearner old; old.prior = &sparse;
        std::array<std::uint8_t, mp::kKeyCount> p8{};
        p8[0] = 9; old.OnPosition(0, 0, 9, 100);
        old.OnKeyDown(0x29, 100 + mp::KeyLearner::kLearnBeforeMs + 50);
        old.Resolve(p8, 100 + mp::KeyLearner::kLearnBeforeMs + 60, learn, confirm, conflict);
        assert(old.LearnedCount() == 0);
        mp::KeyLearner released; released.prior = &sparse;
        std::array<std::uint8_t, mp::kKeyCount> p9{};
        released.OnPosition(0, 0, 9, 300); released.OnPosition(0, 9, 0, 340);
        released.OnKeyDown(0x29, 350);
        released.Resolve(p9, 360, learn, confirm, conflict);
        assert(released.LearnedCount() == 0);
        released.OnPosition(200, 0, 5, 400);
        assert(released.Hid(200) == 0);
    }

    // Rev 6 key matching: quick taps, one press per key-down, misses only when nothing moved.
    {
        std::array<std::uint8_t, mp::kKeyCount> position{};
        unsigned confirmed = 0, conflicts = 0, misses = 0;
        std::int64_t lastLag = 0;
        std::uint8_t lastMissId = 255;
        const auto learn = [](std::uint8_t, std::uint16_t) { assert(false); };
        const auto confirm = [&](std::uint8_t, std::uint16_t, std::int64_t lag, bool) { ++confirmed; lastLag = lag; };
        const auto conflict = [&](std::uint8_t, std::uint16_t) { ++conflicts; };
        const auto miss = [&](std::uint8_t id, std::uint16_t) { ++misses; lastMissId = id; };

        // A quick tap whose release sample is applied before the key-down is resolved: still the same press.
        mp::KeyLearner tap;
        position[33] = 12; tap.OnPosition(33, 0, 12, 1000);
        position[33] = 0;  tap.OnPosition(33, 12, 0, 1040);
        tap.OnKeyDown(0x07, 1020);                            // D
        tap.Resolve(position, 1050, learn, confirm, conflict, miss);
        assert(confirmed == 1 && lastLag == -20 && misses == 0);
        // The next key-down of D does not take the old (claimed) press; it waits for its own press.
        tap.OnKeyDown(0x07, 1200);
        tap.Resolve(position, 1210, learn, confirm, conflict, miss);
        assert(confirmed == 1 && misses == 0);
        position[33] = 9; tap.OnPosition(33, 0, 9, 1230);
        tap.Resolve(position, 1240, learn, confirm, conflict, miss);
        assert(confirmed == 2 && lastLag == 30);
        // A press released long before the key-down is not that key-down's press: the key-down becomes a miss.
        mp::KeyLearner old;
        std::array<std::uint8_t, mp::kKeyCount> p2{};
        p2[33] = 6; old.OnPosition(33, 0, 6, 2000);
        p2[33] = 0; old.OnPosition(33, 6, 0, 2050);
        old.OnKeyDown(0x07, 2050 + mp::KeyLearner::kLearnAfterMs + 100);
        old.Resolve(p2, 2050 + mp::KeyLearner::kLearnAfterMs + 110, learn, confirm, conflict, miss);
        assert(confirmed == 2 && misses == 0);                // still pending
        old.Resolve(p2, 2050 + 2 * mp::KeyLearner::kLearnAfterMs + 200, learn, confirm, conflict, miss);
        assert(confirmed == 2 && misses == 1 && lastMissId == 33 && conflicts == 0);
        // Fn+Up sends PgUp: id 43 does not move, but Up (56) and Fn (63) do: no miss, no conflict (two movers).
        mp::KeyLearner fn;
        std::array<std::uint8_t, mp::kKeyCount> p3{};
        p3[63] = 20; fn.OnPosition(63, 0, 20, 3000);
        p3[56] = 15; fn.OnPosition(56, 0, 15, 3100);
        fn.OnKeyDown(0x4b, 3110);
        fn.Resolve(p3, 3110 + mp::KeyLearner::kLearnAfterMs + 1, learn, confirm, conflict, miss);
        assert(misses == 1 && conflicts == 0 && confirmed == 2);
        // The four-callback form still works (no miss reported).
        mp::KeyLearner four;
        four.OnKeyDown(0x07, 4000);
        four.Resolve(p3, 4000 + mp::KeyLearner::kLearnAfterMs + 1, learn, confirm, conflict);
        assert(misses == 1);
    }

    // Press records: a mode-2 style press (one sample per 50 ms) and an event-driven one.
    {
        mp::PressTracker t;
        mp::PressRecord r;
        assert(!t.Sample(10, 0, 0, r) && !t.Active(10));     // idle samples start nothing
        assert(!t.Sample(10, 20, 1000, r) && t.Active(10));
        assert(!t.Sample(10, 46, 51000, r));
        assert(!t.Sample(10, 46, 101000, r));
        assert(t.Sample(10, 0, 151000, r) && !t.Active(10));
        assert(r.samples == 4 && r.riseSamples == 2 && r.peak == 46 && r.RiseUs() == 50000 && r.DurationUs() == 150000);
        assert(r.gaps == 3 && r.gapMinUs == 50000 && r.gapMaxUs == 50000 && r.GapAvgUs() == 50000);
        // Event-driven: dense samples on the way down, nothing while held, then the release.
        assert(!t.Sample(11, 3, 0, r));
        for (int i = 1; i <= 9; ++i) assert(!t.Sample(11, static_cast<std::uint8_t>(3 + i * 4), i * 2000, r));
        assert(t.Sample(11, 0, 300000, r));
        assert(r.samples == 11 && r.riseSamples == 10 && r.peak == 39 && r.RiseUs() == 18000);
        assert(r.gapMinUs == 2000 && r.gapMaxUs == 282000 && r.gaps == 10);
        assert(!t.Sample(200, 5, 0, r) && !t.Active(200));    // ids outside the board are ignored
    }

    // Mode health: the fallback reasons, decided per key press only.
    {
        mp::ModeHealth h;
        assert(!h.FallbackReason());
        h.OnMiss(); h.OnMiss(); assert(!h.FallbackReason());
        h.OnConfirm(); h.OnMiss(); h.OnMiss(); assert(!h.FallbackReason());   // a confirmation breaks the run
        h.OnMiss(); assert(h.FallbackReason() && std::string(h.FallbackReason()) == "misses_in_row");
        mp::ModeHealth s;
        s.OnStuck(); assert(!s.FallbackReason());
        s.OnStuck(); assert(std::string(s.FallbackReason()) == "stuck_release");
        mp::ModeHealth ratio;                                  // 3 of 10 missed, never 3 in a row
        for (int i = 0; i < 7; ++i) { ratio.OnConfirm(); if (i < 3) { ratio.OnMiss(); } }
        assert(ratio.confirmed == 7 && ratio.misses == 3 && ratio.missRun == 0);
        assert(std::string(ratio.FallbackReason()) == "miss_ratio");
        mp::ModeHealth fine;                                   // 1 of 10: keep mode 1
        for (int i = 0; i < 9; ++i) fine.OnConfirm();
        fine.OnMiss();
        assert(!fine.FallbackReason());
    }


    std::puts("melgeek_made68air_protocol_test OK");
    return 0;
}

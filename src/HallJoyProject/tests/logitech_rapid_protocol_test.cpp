#include "../HallJoy/logitech_rapid_protocol.h"

#include <cassert>
#include <cstdio>
#include <cstring>
#include <utility>
#include <vector>
#include <set>

int main()
{
    namespace lr = halljoy::logitech_rapid;
    assert(lr::FindModel(0x046d, 0xc364) && lr::FindModel(0x046d, 0xc35b));
    assert(!lr::FindModel(0x046d, 0xc365) && !lr::FindModel(0x1234, 0xc364));

    // Key ids 00..56: 87 TKL keys, unique HID usages, Fn = HallJoy Fn.
    std::set<unsigned> usages;
    for (std::size_t id = 0; id < lr::kMappedKeyIds; ++id) {
        const auto hid = lr::kKeyMap[id];
        assert(hid && usages.insert(hid).second);
    }
    assert(usages.size() == 87 && lr::kKeyMap[0x57] == 0 && lr::kKeyMap[0xff] == 0);
    assert(lr::kKeyMap[0x1d] == 0x1a /* W */ && lr::kKeyMap[0x23] == 0x04 /* A */ &&
           lr::kKeyMap[0x22] == 0x16 /* S */ && lr::kKeyMap[0x21] == 0x07 /* D */ &&
           lr::kKeyMap[0x45] == 0x2c /* Space */ && lr::kKeyMap[0x47] == lr::kFn &&
           lr::kKeyMap[0x00] == 0x29 /* Esc */ && lr::kKeyMap[0x56] == 0x50 /* Left */);

    // Requests: long report, device 0xFF, function << 4 | software id.
    const auto get = lr::GetFeature(lr::kAnalogFeature);
    assert(get[0] == 0x11 && get[1] == 0xff && get[2] == 0 && get[3] == lr::kSoftwareId && get[4] == 0x1b && get[5] == 0x08);
    const auto on = lr::Build(9, 3, {1});
    assert(on[2] == 9 && on[3] == ((3 << 4) | lr::kSoftwareId) && on[4] == 1 && on[5] == 0);

    // Replies (long and short), errors, and foreign traffic.
    std::array<std::uint8_t, 16> params{}; std::uint8_t error = 0;
    std::uint8_t reply[20] = {0x11, 0xff, 0x00, lr::kSoftwareId, 0x0e, 0x00, 0x00};
    assert(lr::MatchReply(get, reply, 20, &params, &error) == lr::ReplyKind::Reply && params[0] == 0x0e);
    std::uint8_t shortReply[7] = {0x10, 0xff, 0x09, static_cast<std::uint8_t>((3 << 4) | lr::kSoftwareId), 1, 0, 0};
    assert(lr::MatchReply(on, shortReply, 7, &params, &error) == lr::ReplyKind::Reply && params[0] == 1);
    std::uint8_t err[20] = {0x11, 0xff, 0xff, 0x09, static_cast<std::uint8_t>((3 << 4) | lr::kSoftwareId), 0x07};
    assert(lr::MatchReply(on, err, 20, &params, &error) == lr::ReplyKind::Error && error == 7);
    std::uint8_t other[20] = {0x11, 0xff, 0x09, static_cast<std::uint8_t>((3 << 4) | 0x0a), 1};
    assert(lr::MatchReply(on, other, 20, &params, &error) == lr::ReplyKind::None); // G HUB's reply
    assert(lr::MatchReply(on, shortReply, 6, &params, &error) == lr::ReplyKind::None);

    // v0 (PRO X TKL RAPID) info "01 05 80 28": 4.0 mm in 0.1 mm, one depth byte.
    std::array<std::uint8_t, 16> info{0x01, 0x05, 0x80, 0x28};
    auto f = lr::FormatFromInfo(0, info);
    assert(f.travel == 0x28 && !f.wide && f.eventsVerified);
    info[3] = 5; assert(lr::FormatFromInfo(0, info).travel == 0);
    info[3] = 200; assert(lr::FormatFromInfo(0, info).travel == 0);
    // v2 (PRO X2 RAPID) info "02 05 80 01 90": 400 = 4.00 mm in 0.01 mm, frames.
    std::array<std::uint8_t, 16> v2{0x02, 0x05, 0x80, 0x01, 0x90};
    f = lr::FormatFromInfo(2, v2);
    assert(f.travel == 400 && f.wide && f.eventsVerified);
    assert(!lr::FormatFromInfo(3, v2).eventsVerified && !lr::FormatFromInfo(1, v2).eventsVerified);
    v2[3] = 0x00; v2[4] = 0x50; assert(lr::FormatFromInfo(2, v2).travel == 0); // 0.80 mm
    v2[3] = 0x01; v2[4] = 0x00; assert(lr::FormatFromInfo(2, v2).travel == 0); // 2.56 mm: not a magnetic switch
    v2[3] = 0x28; v2[4] = 0x00; f = lr::FormatFromInfo(2, v2);                 // 0x2800 too big: v0 reading
    assert(f.travel == 0x28 && !f.wide && !f.eventsVerified);

    // Depth events: function-0 notification of the analog feature, short or long.
    lr::DepthEvent ev{};
    std::uint8_t evShort[7] = {0x10, 0xff, 0x09, 0x00, 0x1d, 0x14, 0x00};
    assert(lr::DecodeDepth(9, false, evShort, 7, &ev) && ev.keyId == 0x1d && ev.depth == 0x14 && !ev.extraPayload);
    std::uint8_t evLong[20] = {0x11, 0xff, 0x09, 0x00, 0x42, 0x28};
    assert(lr::DecodeDepth(9, false, evLong, 20, &ev) && ev.keyId == 0x42 && ev.depth == 0x28);
    evLong[8] = 3; assert(lr::DecodeDepth(9, false, evLong, 20, &ev) && ev.extraPayload);
    assert(!lr::DecodeDepth(8, false, evShort, 7, &ev));                 // other feature
    evShort[3] = lr::kSoftwareId; assert(!lr::DecodeDepth(9, false, evShort, 7, &ev)); // a reply
    assert(!lr::DecodeDepth(0, false, evLong, 20, &ev));
    std::uint8_t evWide[7] = {0x10, 0xff, 0x09, 0x00, 0x1d, 0x01, 0x2c};  // 0x012c = 3.00 mm
    assert(lr::DecodeDepth(9, true, evWide, 7, &ev) && ev.depth == 300 && !ev.extraPayload);
    assert(lr::DepthPlausible(300, 400) && lr::DepthPlausible(440, 400) && !lr::DepthPlausible(441, 400));
    assert(!lr::DepthPlausible(0x1400, 400) && !lr::DepthPlausible(1, 0));
    // The first real v2 event (log 2026-10-06: byte 5 = 0x2d, byte 6 = 0, data in
    // payload bytes 1,3,4,7,10,13) is no single [key][depth16] record.
    std::uint8_t evV2[20] = {0x11, 0xff, 0x0d, 0x00, 0x00, 0x2d, 0x00, 0x01, 0x01, 0, 0, 0x01, 0, 0, 0x01, 0, 0, 0x01};
    assert(lr::DecodeDepth(0x0d, true, evV2, 20, &ev) && !lr::DepthPlausible(ev.depth, 400) && ev.extraPayload);

    // v2 key ids: unique HID usages, examples seen pressed in the tester capture.
    std::set<unsigned> v2usages;
    unsigned v2count = 0;
    for (const auto hid : lr::kKeyMapV2)
        if (hid) { ++v2count; assert(v2usages.insert(hid).second); }
    // All 84 keys of the PRO X2 RAPID (log V6), Fn = HallJoy Fn, every other id unknown.
    assert(v2count == 84 && lr::kKeyMapV2[0xff] == 0 && lr::kKeyMapV2[0x34] == 0 && lr::kKeyMapV2[0x56] == 0);
    assert(lr::kKeyMapV2[0x0f] == 0x22 /* 5, log v5 */ && lr::kKeyMapV2[0x0e] == 0x23 /* 6 */);
    assert(lr::kKeyMapV2[0x00] == 0x3b /* F2 */ && lr::kKeyMapV2[0x54] == 0x45 /* F12 */ &&
           lr::kKeyMapV2[0x35] == 0x52 /* Up */ && lr::kKeyMapV2[0x61] == 0x50 /* Left */ &&
           lr::kKeyMapV2[0x60] == 0x51 /* Down */ && lr::kKeyMapV2[0x64] == 0x4f /* Right */ &&
           lr::kKeyMapV2[0x2c] == lr::kFn && lr::kKeyMapV2[0x55] == 0x46 /* PrtSc */ &&
           lr::kKeyMapV2[0x3d] == 0x28 /* Enter */ && lr::kKeyMapV2[0x71] == 0x31 /* backslash */);
    for (const auto hid : {0x48u /* Pause */, 0x4bu /* PgUp */, 0x4eu /* PgDn */}) assert(!v2usages.count(hid));
    assert(lr::kKeyMapV2[0x10] == 0x1a /* W */ && lr::kKeyMapV2[0x19] == 0x04 /* A */ &&
           lr::kKeyMapV2[0x18] == 0x16 /* S */ && lr::kKeyMapV2[0x1b] == 0x07 /* D */ &&
           lr::kKeyMapV2[0x2d] == 0x2c /* Space */ && lr::kKeyMapV2[0x22] == 0xe1 /* LShift */ &&
           lr::kKeyMapV2[0x2a] == 0xe0 /* LCtrl */ && lr::kKeyMapV2[0x0d] == 0x21 /* 4 */ &&
           lr::kKeyMapV2[0x47] == 0x4c /* Delete */ && lr::kKeyMapV2[0x4f] == 0x2a /* Backspace */);
    // The X2's usages equal the TKL table's minus Pause/PgUp/PgDn.
    for (const auto hid : v2usages) assert(usages.count(hid));
    assert(v2usages.size() + 3 == usages.size());

    // v2 frames: real reports from the tester capture (key 4 = id 0x0d).
    lr::V2FrameAssembler frames;
    using R = lr::V2FrameAssembler::Result;
    const std::uint8_t press[20] = {0x11, 0xff, 0x0d, 0x00, 0x00, 0x0d, 0x00, 0x19, 0xff, 0, 0, 0xff, 0, 0, 0xff, 0, 0, 0xff, 0, 0};
    assert(frames.Feed(0x0d, 400, press, 20) == R::Frame);
    assert(frames.frame().count == 1 && frames.frame().ids[0] == 0x0d && frames.frame().depths[0] == 25);
    const std::uint8_t none[20] = {0x11, 0xff, 0x0d, 0x00, 0x00, 0xff, 0, 0, 0xff, 0, 0, 0xff, 0, 0, 0xff, 0, 0, 0xff, 0, 0};
    assert(frames.Feed(0x0d, 400, none, 20) == R::Frame && frames.frame().count == 0);
    // Seven keys: first report (more = 1) then the rest.
    const std::uint8_t first[20] = {0x11, 0xff, 0x0d, 0x00, 0x01, 0x2d, 0x01, 0x6d, 0x19, 0x01, 0x90, 0x18, 0x01, 0x90,
                                    0x3a, 0x01, 0x6d, 0x1c, 0x00, 0x96};
    const std::uint8_t rest[20] = {0x11, 0xff, 0x0d, 0x00, 0x00, 0x42, 0x00, 0x8c, 0x39, 0x00, 0x37, 0xff, 0, 0, 0xff, 0, 0, 0xff, 0, 0};
    assert(frames.Feed(0x0d, 400, first, 20) == R::None);
    assert(frames.Feed(0x0d, 400, rest, 20) == R::Frame && frames.frame().count == 7);
    assert(frames.frame().ids[0] == 0x2d && frames.frame().depths[0] == 365 && frames.frame().ids[6] == 0x39);
    // Lost last report: the next frame repeats an id, the partial frame is dropped.
    assert(frames.Feed(0x0d, 400, first, 20) == R::None);
    assert(frames.Feed(0x0d, 400, first, 20) == R::None);
    assert(frames.Feed(0x0d, 400, rest, 20) == R::Frame && frames.frame().count == 7);
    // Other features, replies and short reports are not frames.
    std::uint8_t foreign[20]; std::memcpy(foreign, press, 20);
    foreign[2] = 0x0e; assert(frames.Feed(0x0d, 400, foreign, 20) == R::None);
    std::memcpy(foreign, press, 20); foreign[3] = 0x3b; assert(frames.Feed(0x0d, 400, foreign, 20) == R::None);
    assert(frames.Feed(0x0d, 400, press, 7) == R::None && frames.Feed(0, 400, press, 20) == R::None);
    // Not this layout: depth beyond travel, zero depth, data after an empty slot, more > 1.
    std::uint8_t bad[20]; std::memcpy(bad, press, 20);
    bad[6] = 0x2d; bad[7] = 0x00; assert(frames.Feed(0x0d, 400, bad, 20) == R::Invalid);   // 0x2d00
    std::memcpy(bad, press, 20); bad[7] = 0; assert(frames.Feed(0x0d, 400, bad, 20) == R::Invalid);
    std::memcpy(bad, press, 20); bad[11] = 0x10; bad[13] = 5; assert(frames.Feed(0x0d, 400, bad, 20) == R::Invalid);
    std::memcpy(bad, press, 20); bad[4] = 2; assert(frames.Feed(0x0d, 400, bad, 20) == R::Invalid);
    std::memcpy(bad, press, 20); bad[9] = 1; assert(frames.Feed(0x0d, 400, bad, 20) == R::Invalid); // empty slot with depth
    assert(frames.Feed(0x0d, 400, press, 20) == R::Frame);

    // Key state: absent for one frame = held, absent for two = released.
    lr::V2KeyState state;
    std::array<int, 256> seen{}; seen.fill(-1);
    const auto record = [&](std::uint8_t id, std::uint16_t depth) { seen[id] = depth; };
    lr::V2Frame two{}; two.count = 2; two.ids[0] = 0x10; two.depths[0] = 300; two.ids[1] = 0x19; two.depths[1] = 40;
    state.Apply(two, record);
    assert(seen[0x10] == 300 && seen[0x19] == 40 && state.depth[0x10] == 300);
    lr::V2Frame one{}; one.count = 1; one.ids[0] = 0x19; one.depths[0] = 45;
    seen.fill(-1); state.Apply(one, record);
    assert(seen[0x10] == -1 && state.depth[0x10] == 300 && seen[0x19] == 45);  // lost report: held
    state.Apply(two, record);                                                    // back: no release
    assert(state.depth[0x10] == 300 && !state.held[0x10]);
    lr::V2Frame empty{};
    seen.fill(-1); state.Apply(empty, record);
    assert(seen[0x10] == -1 && seen[0x19] == -1 && state.depth[0x10] == 300);
    state.Apply(empty, record);
    assert(seen[0x10] == 0 && seen[0x19] == 0 && state.depth[0x10] == 0 && state.depth[0x19] == 0);

    // Run-time learning of ids missing from the table.
    {
        std::array<std::uint16_t, 256> depth{};
        std::vector<std::pair<unsigned, unsigned>> pairs;
        const auto learn = [&](std::uint8_t id, std::uint16_t hid) { pairs.emplace_back(id, hid); };
        lr::V2KeyLearner l;
        // Ids 0x80.. and usages 0x68.. (F13..) are not in the X2 table.
        assert(lr::V2KeyLearner::StaticHid(0x1a) && !lr::V2KeyLearner::StaticHid(0x68));
        // Unknown id 0x80 starts moving, Windows reports usage 0x68 50 ms later.
        l.OnDepth(0x80, 0, 30, 1000); depth[0x80] = 120;
        l.OnKeyDown(0x68, 1050); l.Resolve(depth, 1051, learn);
        assert(pairs.size() == 1 && pairs[0].first == 0x80 && pairs[0].second == 0x68 && l.learned[0x80] == 0x68);
        // Known usages and already learned usages/ids are not paired again.
        l.OnDepth(0x81, 0, 30, 2000); depth[0x81] = 90;
        l.OnKeyDown(0x1a, 2010); l.OnKeyDown(0x68, 2010); l.Resolve(depth, 2011, learn);
        assert(pairs.size() == 1 && l.learned[0x81] == 0);
        // Two unknown keys moving: ambiguous, nothing learned, the key-down expires.
        l.OnDepth(0x83, 0, 40, 2005); depth[0x83] = 80;
        l.OnKeyDown(0x69, 2020); l.Resolve(depth, 2021, learn);
        assert(pairs.size() == 1 && l.learned[0x81] == 0 && l.learned[0x83] == 0);
        depth[0x81] = depth[0x83] = 0;
        // A key that started moving too long ago is not paired.
        l.OnDepth(0x84, 0, 30, 3000); depth[0x84] = 400;
        l.OnKeyDown(0x6a, 3000 + lr::V2KeyLearner::kLearnBeforeMs + 1); l.Resolve(depth, 3500, learn);
        assert(pairs.size() == 1 && l.learned[0x84] == 0);
        depth[0x84] = 0;
        // Windows first, frame later (within kLearnAfterMs): paired on the frame.
        l.OnKeyDown(0x6b, 4000); l.Resolve(depth, 4000, learn);
        assert(pairs.size() == 1);
        l.OnDepth(0x85, 0, 25, 4010); depth[0x85] = 60; l.Resolve(depth, 4010, learn);
        assert(pairs.size() == 2 && l.learned[0x85] == 0x6b);
        // Expired: a key-down without any moving key is dropped after kLearnAfterMs.
        depth[0x85] = 0;
        l.OnKeyDown(0x6c, 5000); l.Resolve(depth, 5000 + lr::V2KeyLearner::kLearnAfterMs + 1, learn);
        l.OnDepth(0x86, 0, 25, 5100); depth[0x86] = 60; l.Resolve(depth, 5100, learn);
        assert(pairs.size() == 2 && l.learned[0x86] == 0);
        // A key of the table is never learned (id 0x10 = W is known).
        l.OnDepth(0x10, 0, 30, 7000); depth[0x10] = 90;
        l.OnKeyDown(0x6d, 7010); l.Resolve(depth, 7011, learn);
        assert(pairs.size() == 2 && l.learned[0x10] == 0);
        depth[0x10] = 0;
        // A keyboard outside the catalog has no table: W is learned like any key.
        lr::V2KeyLearner generic;
        generic.table = &lr::kNoKeyMap;
        assert(!generic.KnownHid(0x1a));
        generic.OnDepth(0x10, 0, 30, 8000); depth[0x10] = 90;
        generic.OnKeyDown(0x1a, 8040); generic.Resolve(depth, 8041, learn);
        assert(pairs.size() == 3 && generic.learned[0x10] == 0x1a && generic.KnownHid(0x1a));
        depth[0x10] = 0;
        l.OnKeyDown(0, 6000); l.Resolve(depth, 6000, learn); // usage 0 ignored
        assert(pairs.size() == 3);
    }

    // Steps to permille of the reported travel.
    assert(lr::ToMilli(0, 40) == 0 && lr::ToMilli(20, 40) == 500 && lr::ToMilli(40, 40) == 1000 &&
           lr::ToMilli(60, 40) == 1000 && lr::ToMilli(1, 40) == 25 && lr::ToMilli(5, 0) == 0);
    assert(lr::ToMilli(200, 400) == 500 && lr::ToMilli(5, 400) == 13);
    std::puts("LOGITECH_RAPID_PROTOCOL=PASS ids replies errors events travel v2_frames v2_keymap v2_lost_report v2_learning");
    return 0;
}

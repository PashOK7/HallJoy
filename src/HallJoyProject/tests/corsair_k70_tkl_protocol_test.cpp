#include <cassert>
#include <set>
#include "../HallJoy/corsair_k70_tkl_protocol.h"
#include "../HallJoy/generated/layout_pipeline/identities.h"

using namespace halljoy::corsair_k70;

int main() {
    const auto request = Request(Property::Calibration, 3);
    static_assert(kReportBytes == 257);
    assert(request[0] == 0 && request[1] == 8 && request[2] == 2 && request[3] == 0xc6);
    assert(request[4] == 0 && request[5] == 3 && request[256] == 0);
    Frame reply{};
    reply[2] = 2;
    assert(ReplyHeader(reply));
    reply[3] = 5;
    assert(!ReplyHeader(reply));
    reply[3] = 0;
    reply[4] = 1; reply[5] = 12; reply[6] = 54;
    assert(U32(reply, 4) == kFirmware);
    assert(!FindLayout(0) && !FindLayout(3) && !FindLayout(5));

    const std::array<unsigned, 3> space{62, 63, 66};
    const std::array<unsigned, 3> regions{1, 2, 4};
    const std::array<const wchar_t*, 3> presets{
        L"Corsair K70 PRO TKL ANSI", L"Corsair K70 PRO TKL ISO", L"Corsair K70 PRO TKL JIS"};
    std::set<std::uint64_t> tokens;
    for (std::size_t variant = 0; variant < regions.size(); ++variant) {
        const auto* layout = FindLayout(regions[variant]);
        assert(layout && layout->hid[space[variant]] == 44 && layout->count == space[variant] + 1);
        const auto token = halljoy::layout_identity::Token("corsair-k70-tkl", layout->product);
        assert(token && tokens.insert(token).second);
        assert(std::wstring_view(halljoy::layout_identity::Match(token)) == presets[variant]);

        std::set<unsigned> hids;
        Samples rest{}, bottom{};
        for (std::size_t i = 0; i < layout->count; ++i) {
            if (layout->hid[i]) assert(hids.insert(layout->hid[i]).second);
            rest[i] = 0x2500;
            bottom[i] = 0x3300;
        }
        assert(hids.size() == layout->count - 2);
        Profile profile{};
        assert(BuildProfile(*layout, rest, bottom, profile) == 0);
        auto bad = bottom;
        bad[space[variant]] = rest[space[variant]];
        assert(BuildProfile(*layout, rest, bad, profile) == 1);
        for (std::size_t i = 0; i < layout->count; ++i) {
            if (!layout->hid[i]) continue;
            assert(profile.scale[i] == 7);
            assert(Normalize(rest[i], profile, i) == 0);
            for (unsigned step = 1; step <= kTravelSteps; ++step) {
                const auto threshold = kCurves[layout->curve[i]][step] * profile.scale[i];
                assert(Normalize(static_cast<std::uint16_t>(rest[i] + threshold), profile, i) == 25 * (step - 1));
                assert(Normalize(static_cast<std::uint16_t>(rest[i] + threshold + 1), profile, i) == 25 * step);
            }
            assert(Normalize(bottom[i], profile, i) == 1000);
        }
        Frame snapshot{};
        snapshot[2] = 2;
        Samples words{};
        snapshot[4] = 0x52;
        snapshot[5] = 0x26;
        assert(ReadWords(snapshot, *layout, words) && words[0] == 0x2652);
        snapshot[kDataOffset + 2 * layout->count] = 1;
        assert(!ReadWords(snapshot, *layout, words));
    }
}

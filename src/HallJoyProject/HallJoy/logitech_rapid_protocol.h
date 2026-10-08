#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <initializer_list>

// Logitech G RAPID keyboards (magnetic analog switches) over HID++ 2.0.
// Evidence (docs/current/LOGITECH_RAPID_2026-10-05.md): the PRO X TKL RAPID
// (046D:C35B) exposes the undocumented feature 0x1B08 ("analog"): function 0
// returns the total travel in 0.1 mm (byte 3, 0x28 = 4.0 mm), function 3 [1/0]
// switches a live key-depth stream on/off whose events are function-0
// notifications [key id][depth in 0.1 mm]. Key ids are Logitech's own
// (they snake through the switch matrix), mapped to HID usages below; the
// TKL ids were matched physically against HID key-downs by the source.
// The PRO X2 RAPID (046D:C364) is a newer TKL model with feature 0x1B08 version
// 2: multi-key frames and its own key ids (see V2FrameAssembler, kKeyMapV2).
namespace halljoy::logitech_rapid {

constexpr std::uint16_t kVendorId = 0x046d;
constexpr std::uint16_t kAnalogFeature = 0x1b08;
constexpr std::uint16_t kFeatureSet = 0x0001;
constexpr std::uint8_t kShortReport = 0x10, kLongReport = 0x11, kDevice = 0xff;
constexpr std::size_t kShortBytes = 7, kLongBytes = 20;
constexpr std::uint8_t kSoftwareId = 0x0b; // our replies; notifications use 0
constexpr std::uint16_t kFn = 0x409;       // HallJoy's Fn code (no HID usage)

struct Model {
    std::uint16_t pid;
    const wchar_t* name;
    const wchar_t* shortName;
    const char* layoutProduct;
};
inline constexpr Model kModels[] = {
    {0xc35b, L"Logitech G PRO X TKL RAPID", L"PRO X TKL RAPID", "PROXTKLRAPID-046D-C35B"},
    {0xc364, L"Logitech G PRO X2 RAPID", L"PRO X2 RAPID", "PROX2RAPID-046D-C364"},
};
inline constexpr const Model* FindModel(std::uint16_t vid, std::uint16_t pid) noexcept {
    if (vid != kVendorId) return nullptr;
    for (const auto& m : kModels)
        if (m.pid == pid) return &m;
    return nullptr;
}

// Logitech analog key id -> HID usage (0 = unknown id).
inline constexpr std::array<std::uint16_t, 256> MakeKeyMap() noexcept {
    std::array<std::uint16_t, 256> map{};
    const std::uint16_t ordered[] = {
        0x29, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f, 0x40, 0x41, 0x42, 0x43, 0x44, 0x45, // 00-0C Esc F1-F12
        0x35, 0x1e, 0x1f, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x2d, 0x2e, // 0D-19 ` 1-0 - =
        0x2a, 0x2b, 0x14, 0x1a, 0x08, 0x15,                                           // 1A-1F Bksp Tab Q W E R
        0x09, 0x07, 0x16, 0x04, 0x39,                                                 // 20-24 F D S A Caps
        0xe1, 0x1d, 0x1b, 0x06, 0x17, 0x1c, 0x18, 0x0c, 0x12, 0x0f, 0x0e,             // 25-2F LShift Z X C T Y U I O L K
        0x0d, 0x0b, 0x0a, 0x19, 0x05, 0x11, 0x10,                                     // 30-36 J H G V B N M
        0x36, 0x13, 0x2f, 0x30, 0x31, 0x28, 0x34, 0x33, 0x37, 0x38, 0xe5,             // 37-41 , P [ ] \ Enter ' ; . / RShift
        0xe0, 0xe3, 0xe2, 0x2c, 0xe6, kFn, 0x65, 0xe4,                                // 42-49 LCtrl LWin LAlt Space RAlt Fn Menu RCtrl
        0x46, 0x47, 0x48, 0x4b, 0x4a, 0x49, 0x4c, 0x4d, 0x4e,                         // 4A-52 PrtSc ScrLk Pause PgUp Home Ins Del End PgDn
        0x52, 0x4f, 0x51, 0x50};                                                      // 53-56 Up Right Down Left
    for (std::size_t i = 0; i < sizeof(ordered) / sizeof(ordered[0]); ++i) map[i] = ordered[i];
    return map;
}
inline constexpr std::array<std::uint16_t, 256> kKeyMap = MakeKeyMap();
inline constexpr std::size_t kMappedKeyIds = 0x57;

using Request = std::array<std::uint8_t, kLongBytes>;

inline Request Build(std::uint8_t featureIndex, std::uint8_t function,
                     std::initializer_list<std::uint8_t> params = {}) noexcept {
    Request r{};
    r[0] = kLongReport; r[1] = kDevice; r[2] = featureIndex;
    r[3] = static_cast<std::uint8_t>((function << 4) | kSoftwareId);
    std::size_t i = 4;
    for (auto p : params) { if (i >= r.size()) break; r[i++] = p; }
    return r;
}
// Root (index 0) function 0: feature id -> index (0 = not supported).
inline Request GetFeature(std::uint16_t feature) noexcept {
    return Build(0, 0, {static_cast<std::uint8_t>(feature >> 8), static_cast<std::uint8_t>(feature)});
}

enum class ReplyKind : std::uint8_t { None, Reply, Error };
// A reply (or HID++ 2.0 error) to `request` within one input report (short or
// long). `params` receives the parameter bytes (zero padded), `error` the code.
inline ReplyKind MatchReply(const Request& request, const std::uint8_t* r, std::size_t n,
                            std::array<std::uint8_t, 16>* params, std::uint8_t* error) noexcept {
    if (!r || n < kShortBytes || (r[0] != kShortReport && r[0] != kLongReport) || r[1] != kDevice)
        return ReplyKind::None;
    const std::size_t size = r[0] == kLongReport ? kLongBytes : kShortBytes;
    if (n < size) return ReplyKind::None;
    if (r[2] == 0xff && r[3] == request[2] && r[4] == request[3]) {
        if (error) *error = r[5];
        return ReplyKind::Error;
    }
    if (r[2] != request[2] || r[3] != request[3]) return ReplyKind::None;
    if (params) {
        params->fill(0);
        for (std::size_t i = 4; i < size; ++i) (*params)[i - 4] = r[i];
    }
    return ReplyKind::Reply;
}

// Depth format of a session. Feature version 0 (PRO X TKL RAPID) reports the
// total travel in 0.1 mm at info byte 3 and one depth byte per event. Version 2
// (PRO X2 RAPID, info "02 05 80 01 90", actuation in 0.05 mm steps) carries a
// 16-bit big-endian travel in 0.01 mm at bytes 3..4 (0x0190 = 4.00 mm, matching
// the specification) when that is plausible (3.00..6.00 mm), and sends frames
// (V2FrameAssembler). Any other version or reading stays unverified: nothing
// is published and the raw stream is captured for the log.
struct AnalogFormat {
    std::uint16_t travel = 0;  // full travel in event units; 0 = unknown
    bool wide = false;         // version 2: frames with 16-bit depths in 0.01 mm
    bool eventsVerified = false; // event layout established: values may be published
};
inline AnalogFormat FormatFromInfo(std::uint8_t version, const std::array<std::uint8_t, 16>& p) noexcept {
    const bool narrowOk = p[3] >= 10 && p[3] <= 80;
    if (version == 0) return narrowOk ? AnalogFormat{p[3], false, true} : AnalogFormat{};
    const unsigned wide = (unsigned{p[3]} << 8) | p[4];
    if (wide >= 300 && wide <= 600) return {static_cast<std::uint16_t>(wide), true, version == 2};
    return narrowOk ? AnalogFormat{p[3], false, false} : AnalogFormat{};
}

// Version 2 key ids (PRO X2 RAPID) -> HID usage, from the tester's raw capture
// with Windows key events (logs "Logitech v4", "v4 + GHUB" and "v5", 2026-10-06).
// Ids come in blocks of 8 per keyboard row (0x0_ F-row and number row left,
// 0x1_ QWERTY/home left, 0x2_ shift row/bottom left, 0x3_ home/shift right,
// 0x4_ QWERTY/number right). In each block positions 0..3 follow physical
// columns: left half 2-W-S-X, 1-Q-A-Z, grave-Tab-Caps-LShift, 3-E-D-C; right
// half mirrored 0-P-;-/, 9-O-L-., 8-I-K-comma. Log "V6" (2026-10-07, every
// key pressed once with the run-time learner) completed all 84 keys: 28 pairs
// learned from Windows key-downs, PrtSc from its key-up (Windows reports no
// PrtSc key-down) and Fn from the raw capture (no Windows event; a separate
// HID++ notification fires with it). It also confirmed Tab, ; and / (earlier
// inferred from the column rule) and 6 (earlier the only id left).
// V2KeyLearner stays for ids this table does not know (other layouts).
inline constexpr std::array<std::uint16_t, 256> MakeKeyMapV2() noexcept {
    std::array<std::uint16_t, 256> map{};
    const std::uint16_t pairs[][2] = {
        {0x00, 0x3b}, {0x01, 0x3a}, {0x02, 0x29}, {0x03, 0x3c},                 // F2 F1 Esc F3
        {0x04, 0x40}, {0x05, 0x3d}, {0x06, 0x3f}, {0x07, 0x3e},                 // F7 F4 F6 F5
        {0x08, 0x1f}, {0x09, 0x1e}, {0x0a, 0x35}, {0x0b, 0x20},                 // 2 1 grave 3
        {0x0c, 0x24}, {0x0d, 0x21}, {0x0e, 0x23}, {0x0f, 0x22},                 // 7 4 6 5
        {0x10, 0x1a}, {0x11, 0x14}, {0x12, 0x2b}, {0x13, 0x08},                 // W Q Tab E
        {0x14, 0x1c}, {0x15, 0x15}, {0x16, 0x18}, {0x17, 0x17},                 // Y R U T
        {0x18, 0x16}, {0x19, 0x04}, {0x1a, 0x39}, {0x1b, 0x07},                 // S A Caps D
        {0x1c, 0x0d}, {0x1d, 0x09}, {0x1e, 0x0b}, {0x1f, 0x0a},                 // J F H G
        {0x20, 0x1b}, {0x21, 0x1d}, {0x22, 0xe1}, {0x23, 0x06},                 // X Z LShift C
        {0x24, 0x19}, {0x25, 0x10}, {0x26, 0x05}, {0x27, 0x11},                 // V M B N
        {0x28, 0xe2}, {0x29, 0xe3}, {0x2a, 0xe0}, {0x2b, 0xe4},                 // LAlt LWin LCtrl RCtrl
        {0x2c, kFn},  {0x2d, 0x2c}, {0x2e, 0x65}, {0x2f, 0xe6},                 // Fn Space Menu RAlt
        {0x30, 0x38}, {0x31, 0x37}, {0x32, 0x36}, {0x33, 0xe5}, {0x35, 0x52},   // / . comma RShift Up
        {0x38, 0x33}, {0x39, 0x0f}, {0x3a, 0x0e}, {0x3b, 0x34}, {0x3d, 0x28},   // ; L K ' Enter
        {0x40, 0x13}, {0x41, 0x12}, {0x42, 0x0c}, {0x43, 0x2f},                 // P O I [
        {0x44, 0x30}, {0x46, 0x4d}, {0x47, 0x4c},                               // ] End Delete
        {0x48, 0x27}, {0x49, 0x26}, {0x4a, 0x25}, {0x4b, 0x2d},                 // 0 9 8 -
        {0x4c, 0x4a}, {0x4d, 0x2e}, {0x4e, 0x49}, {0x4f, 0x2a},                 // Home = Insert Backspace
        {0x50, 0x43}, {0x51, 0x42}, {0x52, 0x41}, {0x53, 0x44},                 // F10 F9 F8 F11
        {0x54, 0x45}, {0x55, 0x46}, {0x57, 0x47},                               // F12 PrtSc ScrLk
        {0x60, 0x51}, {0x61, 0x50}, {0x64, 0x4f}, {0x71, 0x31},                 // Down Left Right backslash
    };
    for (std::size_t i = 0; i < sizeof(pairs) / sizeof(pairs[0]); ++i) map[pairs[i][0]] = pairs[i][1];
    return map;
}
inline constexpr std::array<std::uint16_t, 256> kKeyMapV2 = MakeKeyMapV2();

// Version 2 events, decoded from the tester capture (2026-10-06): function-0
// long reports [11 ff idx 00][more][5 x (key id, depth hi, depth lo)], key id
// 0xff = empty slot (empties trail), depth in 0.01 mm (0..travel). A frame
// lists every key with a non-zero depth, so a released key is simply absent;
// a frame of more than 5 keys spans reports with `more` = 1, the last has 0
// (an empty last report = no key pressed). A key id repeated within one frame
// means the previous frame's last report was lost: the partial frame is dropped.
struct V2Frame {
    std::array<std::uint8_t, 128> ids{};
    std::array<std::uint16_t, 128> depths{};
    std::size_t count = 0;
};
class V2FrameAssembler {
public:
    enum class Result : std::uint8_t { None, Frame, Invalid };
    // `travel` bounds depths (+10%); Invalid = not this layout.
    Result Feed(std::uint8_t analogIndex, std::uint16_t travel, const std::uint8_t* r, std::size_t n) noexcept {
        if (!r || !analogIndex || n < kLongBytes || r[0] != kLongReport || r[1] != kDevice ||
            r[2] != analogIndex || r[3] != 0)
            return Result::None;
        if (r[4] > 1) return Result::Invalid;
        bool empty = false;
        for (std::size_t slot = 0; slot < 5; ++slot) {
            const std::uint8_t* s = r + 5 + slot * 3;
            if (s[0] == 0xff) {
                if (s[1] || s[2]) return Result::Invalid;
                empty = true;
                continue;
            }
            if (empty) return Result::Invalid; // empties trail
            const std::uint16_t depth = static_cast<std::uint16_t>((unsigned{s[1]} << 8) | s[2]);
            if (!depth || unsigned{depth} * 10u > unsigned{travel} * 11u) return Result::Invalid;
            bool repeated = false;
            for (std::size_t i = 0; i < pending_.count; ++i) repeated |= pending_.ids[i] == s[0];
            if (repeated) pending_.count = 0;
            if (pending_.count < pending_.ids.size()) {
                pending_.ids[pending_.count] = s[0];
                pending_.depths[pending_.count] = depth;
                ++pending_.count;
            }
        }
        if (r[4]) return Result::None;
        frame_ = pending_;
        pending_.count = 0;
        return Result::Frame;
    }
    const V2Frame& frame() const noexcept { return frame_; }
private:
    V2Frame pending_{}, frame_{};
};

// Key state from frames. A key absent from one frame keeps its depth for that
// frame and is released when it is absent from the next one too: in the tester
// capture a pressed key vanished for exactly one frame 145 times, 132 of them
// next to frames spanning two reports (one report lost), while real releases
// stay away. The cost is one frame (about 1 ms) of release delay.
struct V2KeyState {
    std::array<std::uint16_t, 256> depth{};
    std::array<bool, 256> held{};
    // Applies a frame; `changed(id, depth)` is called for every key whose depth changes.
    template <class F> void Apply(const V2Frame& f, F&& changed) {
        std::array<bool, 256> seen{};
        for (std::size_t i = 0; i < f.count; ++i) {
            const auto id = f.ids[i];
            seen[id] = true;
            held[id] = false;
            if (depth[id] != f.depths[i]) { depth[id] = f.depths[i]; changed(id, depth[id]); }
        }
        for (std::size_t id = 0; id < depth.size(); ++id) {
            if (seen[id] || !depth[id]) continue;
            if (!held[id]) { held[id] = true; continue; }
            depth[id] = 0;
            held[id] = false;
            changed(static_cast<std::uint8_t>(id), std::uint16_t{0});
        }
    }
};

// Run-time learning of key ids missing from kKeyMapV2, from Windows key-downs
// of the same keyboard. The OS reports a key when its depth crosses the
// actuation point, shortly after the key started moving from rest. A key-down
// of an unknown HID usage is paired with the one unknown id that started
// moving from rest within kLearnBeforeMs before it (or kLearnAfterMs after it,
// for frames processed late). No pairing when two unknown keys qualify, when
// the usage or the id is already known, or for the usage 0. Pairs live for
// the session; the first press of such a key is digital only. The static
// table is kKeyMapV2 by default; a keyboard outside the catalog (generic
// HID++ 0x1B08 support) uses kNoKeyMap and learns every key.
inline constexpr std::array<std::uint16_t, 256> kNoKeyMap{};
class V2KeyLearner {
public:
    static constexpr std::uint64_t kLearnBeforeMs = 400, kLearnAfterMs = 60;
    std::array<std::uint16_t, 256> learned{}; // id -> HID usage
    const std::array<std::uint16_t, 256>* table = &kKeyMapV2;

    static bool StaticHid(std::uint16_t hid) noexcept {
        for (const auto known : kKeyMapV2) if (known == hid) return true;
        return false;
    }
    bool KnownHid(std::uint16_t hid) const noexcept {
        for (const auto known : *table) if (known == hid) return true;
        for (const auto known : learned) if (known == hid) return true;
        return false;
    }
    // A key's depth changed (`before` -> `after`).
    void OnDepth(std::uint8_t id, std::uint16_t before, std::uint16_t after, std::uint64_t now) noexcept {
        if (!before && after) pressStart_[id] = now;
    }
    void OnKeyDown(std::uint16_t hid, std::uint64_t now) noexcept {
        if (!hid || pendingCount_ >= pending_.size()) return;
        pending_[pendingCount_++] = Pending{hid, now};
    }
    // Pairs pending key-downs with ids pressed now (`depth`); `learn(id, hid)`
    // is called for every new pair. Unresolved key-downs expire after kLearnAfterMs.
    template <class F> void Resolve(const std::array<std::uint16_t, 256>& depth, std::uint64_t now, F&& learn) {
        std::size_t kept = 0;
        for (std::size_t i = 0; i < pendingCount_; ++i) {
            const auto p = pending_[i];
            if (KnownHid(p.hid)) continue;
            int candidate = -1, count = 0;
            for (std::size_t id = 0; id < depth.size(); ++id) {
                if ((*table)[id] || learned[id] || !depth[id] || !pressStart_[id]) continue;
                if (pressStart_[id] + kLearnBeforeMs < p.time || pressStart_[id] > p.time + kLearnAfterMs) continue;
                candidate = static_cast<int>(id); ++count;
            }
            if (count == 1) {
                learned[static_cast<std::size_t>(candidate)] = p.hid;
                learn(static_cast<std::uint8_t>(candidate), p.hid);
                continue;
            }
            if (count == 0 && now <= p.time + kLearnAfterMs) pending_[kept++] = p; // frame may still come
        }
        pendingCount_ = kept;
    }
private:
    struct Pending { std::uint16_t hid = 0; std::uint64_t time = 0; };
    std::array<std::uint64_t, 256> pressStart_{};
    std::array<Pending, 32> pending_{};
    std::size_t pendingCount_ = 0;
};

struct DepthEvent { std::uint8_t keyId = 0; std::uint16_t depth = 0; bool extraPayload = false; };
// Function-0 notification of the analog feature (software id 0).
inline bool DecodeDepth(std::uint8_t analogIndex, bool wide, const std::uint8_t* r, std::size_t n,
                        DepthEvent* out) noexcept {
    if (!r || !out || !analogIndex || n < kShortBytes || r[1] != kDevice || r[2] != analogIndex || r[3] != 0)
        return false;
    if (r[0] != kShortReport && r[0] != kLongReport) return false;
    const std::size_t size = r[0] == kLongReport ? kLongBytes : kShortBytes;
    if (n < size) return false;
    DepthEvent e{};
    e.keyId = r[4];
    e.depth = wide ? static_cast<std::uint16_t>((unsigned{r[5]} << 8) | r[6]) : r[5];
    for (std::size_t i = wide ? 7 : 6; i < size; ++i) e.extraPayload |= r[i] != 0;
    *out = e;
    return true;
}
// A depth beyond the full travel (+10%) means the format guess is wrong.
inline bool DepthPlausible(std::uint16_t depth, std::uint16_t travel) noexcept {
    return travel && unsigned{depth} * 10u <= unsigned{travel} * 11u;
}

inline std::uint16_t ToMilli(std::uint16_t depth, std::uint16_t travel) noexcept {
    if (!travel) return 0;
    const unsigned value = (unsigned{depth} * 1000u + travel / 2u) / travel;
    return static_cast<std::uint16_t>(value > 1000u ? 1000u : value);
}

} // namespace halljoy::logitech_rapid

#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>

#include "analog_key_codes.h"

// MelGeek MADE68 Air (0416:0114): live per-key travel through the vendor report mode.
//
// The keyboard streams the position of every key on its vendor HID collection (FF60:61, 64-byte
// frames, no report id) while the report mode (`set_report`, module 0A op 93) is 2; the vendor
// configurator uses the same mode on its Calibrate/Detect tabs and writes 0 when the tab is left.
// Verified on hardware from a tester log (2026-10-12, docs/current/MELGEEK_MADE68_AIR_2026-10-12.md):
// frames `08 09 <len> 00 00 00 <crc>` carry `len / 4` entries `[key id u8 0..67][position u8 0..46][ADC u16 LE]`,
// no calibration needed. Mode 2 is a full scan of all keys every 50.0 ms (log 9, 2026-10-15). Mode 1 was
// acknowledged and read back; at idle its stream stopped after ~0.5 s, so it may be event-driven (rev 6 tests it).
//
// Facts used here come from the public MelGeek HIVE web app (Rust/WASM core, static reading):
// a request/reply frame is 8 header bytes [module, op, data length, b3, b4, b5, crc lo, crc hi]
// followed by the data; the CRC is CRC-16/CCITT-FALSE (poly 0x1021, init 0xFFFF, MSB first) over
// the data bytes only, so an empty frame carries FF FF; a received frame whose CRC bytes are both
// zero is accepted without a check. The report-level framing (collection, report id, length) is
// NOT assumed: the backend reads it from the HID descriptor and logs it.
// See docs/current/MELGEEK_MADE68_AIR_2026-10-12.md.
//
// Safety contract (checked by melgeek_made68air_protocol_test.cpp): the only frames the backend can
// ever send are the empty-payload getters in kGetters and ONE setter: the live-report mode
// `set_report` (0A 93) with the value 0 (off), 1 or 2 (live per-key data), the same runtime toggle the
// vendor configurator sends when its Calibrate/Detect tab is opened (2) and left (0; owner approval
// 2026-10-12; value 1 approved for the speed work the same day). MakeGetterFrame and MakeSetReportFrame refuse everything else, so no calibration,
// interaction, factory, log or firmware command can be built from this file.
namespace halljoy::melgeek_made68air {

inline constexpr std::uint16_t kVendorId = 0x0416;
inline constexpr std::uint16_t kProductId = 0x0114;

// Switch for the whole backend (no device is opened and nothing is sent while false).
inline constexpr bool kEnabled = true;

// When `get_report` reads 0 (off), switch the live-report mode on and restore 0 when the session
// ends. False = read-only diagnostics (the rev 1/2 probe without the stream).
inline constexpr bool kLiveReportMode = true;
// No reports for this long while the mode is believed on: ask `get_report` and re-enable if it fell to 0
// (for example after the vendor configurator left its tab, or a keyboard reset).
inline constexpr std::uint64_t kSilenceCheckMs = 3000;

inline constexpr std::size_t kFrameBytes = 64;   // device-side frame (without the report id byte)
inline constexpr std::size_t kHeaderBytes = 8;

constexpr std::uint16_t Crc16(const std::uint8_t* data, std::size_t length) noexcept {
    std::uint16_t crc = 0xffff;
    for (std::size_t i = 0; i < length; ++i) {
        crc = static_cast<std::uint16_t>(crc ^ (static_cast<std::uint16_t>(data[i]) << 8));
        for (int bit = 0; bit < 8; ++bit)
            crc = (crc & 0x8000) ? static_cast<std::uint16_t>((crc << 1) ^ 0x1021)
                                 : static_cast<std::uint16_t>(crc << 1);
    }
    return crc;
}

struct Getter {
    std::uint8_t module;
    std::uint8_t op;
    const char* name;
};

// Read-only getters named "get_*" / "*_info" in the HIVE core, sent with an empty payload.
// Deliberately absent: every setter, calibration, interaction, factory, firmware and log command.
inline constexpr std::array<Getter, 6> kGetters{{
    {0x01, 0x87, "get_ver"},
    {0x01, 0x8b, "get_hw_info"},
    {0x0a, 0x81, "get_triggers"},
    {0x0a, 0x94, "get_report"},
    {0x0a, 0x97, "get_trigger_accuracy"},
    {0x0a, 0xa3, "switch_info"},
}};

// Reply op observed on the keyboard for every getter: the request op with bit 7 cleared.
constexpr std::uint8_t ReplyOp(std::uint8_t op) noexcept { return static_cast<std::uint8_t>(op & 0x7f); }

constexpr bool IsAllowedGetter(std::uint8_t module, std::uint8_t op) noexcept {
    for (const auto& getter : kGetters)
        if (getter.module == module && getter.op == op) return true;
    return false;
}

// Builds [module, op, 0, 0, 0, 0, crc lo, crc hi] + zero padding. With withCrc the CRC bytes are
// the CRC of the empty payload (FF FF); without, 00 00 (the receiver skips the check). Returns
// false, leaving `out` untouched, for any command that is not an allowed getter.
constexpr bool MakeGetterFrame(std::uint8_t module, std::uint8_t op, bool withCrc,
                               std::array<std::uint8_t, kFrameBytes>& out) noexcept {
    if (!IsAllowedGetter(module, op)) return false;
    out = {};
    out[0] = module;
    out[1] = op;
    const std::uint16_t crc = withCrc ? Crc16(nullptr, 0) : static_cast<std::uint16_t>(0);
    out[6] = static_cast<std::uint8_t>(crc & 0xff);
    out[7] = static_cast<std::uint8_t>(crc >> 8);
    return true;
}

// The one allowed setter. The HIVE core builds it as [0A, 93, 01, 00, b4, 00, crc lo, crc hi] + one data
// byte (the mode); b4 is a device-object field whose value is unknown, 0 was accepted by the getter.
inline constexpr Getter kSetReport{0x0a, 0x93, "set_report"};
inline constexpr std::uint8_t kReportOff = 0;
inline constexpr std::uint8_t kReportLive = 2;
// Mode 1 (the vendor UI only uses 0 and 2). Rev 6 starts in it (owner 2026-10-15: test the real mode 1) and
// falls back to mode 2 only on evidence from key presses (ModeHealth below), never because time passed.
inline constexpr std::uint8_t kReportMode1 = 1;
inline constexpr std::uint8_t kPreferredMode = kReportMode1;
inline constexpr std::uint8_t kGetReportOp = 0x94;

constexpr bool IsAllowedReportMode(std::uint8_t mode) noexcept {
    return mode == kReportOff || mode == kReportMode1 || mode == kReportLive;
}

constexpr bool MakeSetReportFrame(std::uint8_t mode, bool withCrc,
                                  std::array<std::uint8_t, kFrameBytes>& out) noexcept {
    if (!IsAllowedReportMode(mode)) return false;
    out = {};
    out[0] = kSetReport.module;
    out[1] = kSetReport.op;
    out[2] = 1;                 // one data byte
    out[kHeaderBytes] = mode;
    const std::uint16_t crc = withCrc ? Crc16(&out[kHeaderBytes], 1) : static_cast<std::uint16_t>(0);
    out[6] = static_cast<std::uint8_t>(crc & 0xff);
    out[7] = static_cast<std::uint8_t>(crc >> 8);
    return true;
}

enum class CrcState : std::uint8_t { TooShort, Skipped, Ok, Bad };

struct Header {
    std::uint8_t module = 0, op = 0, length = 0, b3 = 0, b4 = 0, b5 = 0;
    std::uint16_t crc = 0;
    CrcState state = CrcState::TooShort;
};

inline const char* CrcName(CrcState state) noexcept {
    switch (state) {
    case CrcState::Skipped: return "skipped";
    case CrcState::Ok: return "ok";
    case CrcState::Bad: return "bad";
    default: return "short";
    }
}

// Same rules as the HIVE receiver: at least 8 bytes and 8 + length bytes present, both CRC bytes
// zero means "not checked", otherwise the CRC of the data bytes must match.
inline Header ParseHeader(const std::uint8_t* frame, std::size_t size) noexcept {
    Header header;
    if (!frame || size < kHeaderBytes) return header;
    header.module = frame[0]; header.op = frame[1]; header.length = frame[2];
    header.b3 = frame[3]; header.b4 = frame[4]; header.b5 = frame[5];
    header.crc = static_cast<std::uint16_t>(frame[6] | (frame[7] << 8));
    if (size < kHeaderBytes + header.length) return header;
    if (header.crc == 0) { header.state = CrcState::Skipped; return header; }
    header.state = Crc16(frame + kHeaderBytes, header.length) == header.crc ? CrcState::Ok : CrcState::Bad;
    return header;
}

// The mode byte of a `get_report` reply (module 0A, op 14, one data byte), or -1.
inline int ReportModeFromReply(const std::uint8_t* frame, std::size_t size) noexcept {
    const Header header = ParseHeader(frame, size);
    if (header.state == CrcState::TooShort || header.state == CrcState::Bad) return -1;
    if (header.module != kSetReport.module || header.op != ReplyOp(kGetReportOp) || header.length < 1) return -1;
    return frame[kHeaderBytes];
}

// What a HID collection is for. Only a vendor-defined collection is ever written to.
enum class Role : std::uint8_t { Keyboard, Mouse, Consumer, SystemControl, Gamepad, Vendor, Other };

constexpr Role Classify(std::uint16_t usagePage, std::uint16_t usage) noexcept {
    if (usagePage >= 0xff00) return Role::Vendor;
    if (usagePage == 0x01) {
        if (usage == 0x06 || usage == 0x07) return Role::Keyboard;
        if (usage == 0x02) return Role::Mouse;
        if (usage == 0x04 || usage == 0x05) return Role::Gamepad;
        if (usage == 0x80) return Role::SystemControl;
    }
    if (usagePage == 0x0c) return Role::Consumer;
    return Role::Other;
}
inline const char* RoleName(Role role) noexcept {
    switch (role) {
    case Role::Keyboard: return "keyboard";
    case Role::Mouse: return "mouse";
    case Role::Consumer: return "consumer";
    case Role::SystemControl: return "system";
    case Role::Gamepad: return "gamepad";
    case Role::Vendor: return "vendor";
    default: return "other";
    }
}
// The operating system owns keyboard and mouse collections; they are never opened.
constexpr bool IsOpened(Role role) noexcept { return role != Role::Keyboard && role != Role::Mouse; }
constexpr bool IsWritten(Role role) noexcept { return role == Role::Vendor; }

// Per byte position summary of every received report of one collection: which bytes ever change
// and over what range. A live depth stream shows up as bytes that follow the keys.
class ReportStats {
public:
    static constexpr std::size_t kMaxBytes = 128;

    std::uint64_t frames = 0;
    std::size_t length = 0;

    void Add(const std::uint8_t* data, std::size_t size) noexcept {
        if (!data || !size) return;
        const std::size_t n = std::min(size, kMaxBytes);
        if (frames == 0) {
            for (std::size_t i = 0; i < n; ++i) min_[i] = max_[i] = data[i];
            length = n;
        } else {
            for (std::size_t i = 0; i < n; ++i) {
                if (i >= length) { min_[i] = max_[i] = data[i]; continue; }
                if (data[i] != last_[i]) ++changes_[i];
                min_[i] = std::min(min_[i], data[i]);
                max_[i] = std::max(max_[i], data[i]);
            }
            length = std::max(length, n);
        }
        for (std::size_t i = 0; i < n; ++i) last_[i] = data[i];
        ++frames;
    }

    bool Varies(std::size_t index) const noexcept {
        return index < length && (min_[index] != max_[index] || changes_[index] != 0);
    }
    std::size_t VaryingCount() const noexcept {
        std::size_t count = 0;
        for (std::size_t i = 0; i < length; ++i) count += Varies(i) ? 1 : 0;
        return count;
    }
    std::uint8_t Min(std::size_t index) const noexcept { return index < length ? min_[index] : 0; }
    std::uint8_t Max(std::size_t index) const noexcept { return index < length ? max_[index] : 0; }
    std::uint32_t Changes(std::size_t index) const noexcept { return index < length ? changes_[index] : 0; }

private:
    std::array<std::uint8_t, kMaxBytes> min_{}, max_{}, last_{};
    std::array<std::uint32_t, kMaxBytes> changes_{};
};

// Frame counts at which the statistics are written: every frame at first, then 25 % steps, so the
// number of log records stays small however long the stream runs.
constexpr std::uint64_t NextFlushAt(std::uint64_t current) noexcept {
    const std::uint64_t grown = current + current / 4;
    return grown > current ? grown : current + 1;
}

constexpr std::uint64_t Fnv1a(const std::uint8_t* data, std::size_t size) noexcept {
    std::uint64_t hash = 1469598103934665603ull;
    for (std::size_t i = 0; i < size; ++i) { hash ^= data[i]; hash *= 1099511628211ull; }
    return hash;
}

// ---- live stream ---------------------------------------------------------------------------------

inline constexpr std::uint8_t kStreamModule = 0x08;
inline constexpr std::uint8_t kStreamOp = 0x09;
inline constexpr std::size_t kKeyCount = 68;          // key ids 0..67 (the 68 keys of the board)
inline constexpr std::uint8_t kMaxPosition = 46;      // fully pressed; observed 45..46 at the bottom
inline constexpr std::size_t kEntryBytes = 4;
inline constexpr std::size_t kMaxEntries = (kFrameBytes - kHeaderBytes) / kEntryBytes;   // 14

struct KeyReading {
    std::uint8_t id = 0;
    std::uint8_t position = 0;   // 0 released .. kMaxPosition fully pressed
    std::uint16_t adc = 0;
};

// Decodes one stream frame (module 08, op 09, CRC ok, whole entries only, ids < kKeyCount).
// Returns the number of entries written to `out` (at most kMaxEntries), or -1 if the frame is not a
// valid stream frame (such frames are ignored whole; a partial frame is never half-applied).
inline int DecodeStreamFrame(const std::uint8_t* frame, std::size_t size, KeyReading* out) noexcept {
    if (!frame || !out) return -1;
    const Header header = ParseHeader(frame, size);
    if (header.module != kStreamModule || header.op != kStreamOp) return -1;
    if (header.state != CrcState::Ok) return -1;
    if (header.length == 0 || header.length % kEntryBytes != 0 || header.length > kFrameBytes - kHeaderBytes) return -1;
    const std::size_t count = header.length / kEntryBytes;
    for (std::size_t i = 0; i < count; ++i) {
        const std::uint8_t* entry = frame + kHeaderBytes + i * kEntryBytes;
        if (entry[0] >= kKeyCount) return -1;
    }
    for (std::size_t i = 0; i < count; ++i) {
        const std::uint8_t* entry = frame + kHeaderBytes + i * kEntryBytes;
        out[i].id = entry[0];
        out[i].position = std::min<std::uint8_t>(entry[1], kMaxPosition);
        out[i].adc = static_cast<std::uint16_t>(entry[2] | (entry[3] << 8));
    }
    return static_cast<int>(count);
}

// Position (0..46) to HallJoy's 0..1000 depth.
constexpr std::uint16_t PositionToMilli(std::uint8_t position) noexcept {
    if (position >= kMaxPosition) return 1000;
    return static_cast<std::uint16_t>((static_cast<unsigned>(position) * 1000u + kMaxPosition / 2u) / kMaxPosition);
}

// ---- press records and the report-mode decision ----------------------------------------------------
//
// One press of a key as the stream shows it: from the first sample with a position above 0 to the sample that
// brings it back to 0. Times are microseconds (QueryPerformanceCounter). What tells how fast a report mode is:
// the samples while the key goes down (rise), the gaps between samples of the key, and the lag behind the Windows
// key-down (KeyLearner). Mode 2 sends every key every 50 ms, so a press there has one sample per 50 ms.
struct PressRecord {
    std::int64_t startUs = 0, peakUs = 0, endUs = 0;
    std::uint32_t samples = 0, riseSamples = 0;   // samples of the press; samples up to the first peak sample
    std::uint8_t peak = 0;
    std::int64_t gapMinUs = 0, gapMaxUs = 0, gapSumUs = 0;
    std::uint32_t gaps = 0;
    std::int64_t DurationUs() const noexcept { return endUs - startUs; }
    std::int64_t RiseUs() const noexcept { return peakUs - startUs; }
    std::int64_t GapAvgUs() const noexcept { return gaps ? gapSumUs / gaps : 0; }
};

class PressTracker {
public:
    // One stream sample of key `id`. Returns true when the sample ended a press; the record is copied to `done`.
    bool Sample(std::uint8_t id, std::uint8_t position, std::int64_t nowUs, PressRecord& done) noexcept {
        if (id >= kKeyCount) return false;
        State& s = state_[id];
        if (!s.active) {
            if (!position) return false;
            s = State{};
            s.active = true;
            s.lastUs = nowUs;
            s.rec.startUs = s.rec.peakUs = nowUs;
            s.rec.samples = s.rec.riseSamples = 1;
            s.rec.peak = position;
            return false;
        }
        const std::int64_t gap = nowUs - s.lastUs;
        if (gap >= 0) {
            s.rec.gapMinUs = s.rec.gaps ? std::min(s.rec.gapMinUs, gap) : gap;
            s.rec.gapMaxUs = std::max(s.rec.gapMaxUs, gap);
            s.rec.gapSumUs += gap;
            ++s.rec.gaps;
        }
        s.lastUs = nowUs;
        ++s.rec.samples;
        if (position > s.rec.peak) { s.rec.peak = position; s.rec.peakUs = nowUs; s.rec.riseSamples = s.rec.samples; }
        if (position) return false;
        s.rec.endUs = nowUs;
        done = s.rec;
        s.active = false;
        return true;
    }
    bool Active(std::uint8_t id) const noexcept { return id < kKeyCount && state_[id].active; }

private:
    struct State { bool active = false; std::int64_t lastUs = 0; PressRecord rec; };
    std::array<State, kKeyCount> state_{};
};

// A Windows key-up whose key shows no later stream sample for this long, with its last position above 0, is a
// stuck release (the report mode did not send the release).
inline constexpr std::uint64_t kStuckAfterMs = 300;

// Does the preferred mode serve key presses? Judged per key press only, never by elapsed time:
//   miss  - Windows reported a key-down of a mapped key and the stream never showed that key moving (KeyLearner);
//   stuck - Windows reported the key-up and the stream never sent that key's release (kStuckAfterMs).
// The answer is "fall back to mode 2" after 3 misses in a row, 2 stuck releases, or more than 20 % misses once
// 10 key-downs were judged. Key-downs Windows never delivers (an elevated window in front) are not counted at all.
class ModeHealth {
public:
    unsigned confirmed = 0, misses = 0, missRun = 0, stuck = 0;
    void OnConfirm() noexcept { ++confirmed; missRun = 0; }
    void OnMiss() noexcept { ++misses; ++missRun; }
    void OnStuck() noexcept { ++stuck; }
    const char* FallbackReason() const noexcept {
        if (missRun >= 3) return "misses_in_row";
        if (stuck >= 2) return "stuck_release";
        const unsigned judged = confirmed + misses;
        if (judged >= 10 && misses * 5 > judged) return "miss_ratio";
        return nullptr;
    }
};

// ---- key map ---------------------------------------------------------------------------------------
//
// Key id (0..67) to HID usage. The board is the usual 68-key 65% layout: rows of 15, 15, 14, 14 and 10 keys,
// ids in reading order (top view photo supplied by the owner 2026-10-12, docs/current/MELGEEK_MADE68_AIR_2026-10-12.md):
//   0 Esc, 1..9 digits, 10 zero, 11 minus, 12 equals, 13 Backspace, 14 grave (the keycap shows a house symbol, but the
//   keyboard sends the grave usage 0x35)
//   15 Tab, 16..25 Q..P, 26 [, 27 ], 28 backslash, 29 Del
//   30 Caps, 31..39 A..L, 40 ;, 41 ', 42 Enter, 43 PgUp
//   44 LShift, 45..51 Z..M, 52 comma, 53 period, 54 slash, 55 RShift, 56 Up, 57 PgDn
//   58 LCtrl, 59 LWin, 60 LAlt, 61 Space, 62 RAlt, 63 Fn (the keycap shows a dot-grid symbol, Fn is the only key
//   missing from the bottom row), 64 RCtrl, 65 Left, 66 Down, 67 Right.
// Verified on hardware (slow sequential key presses, run-time pairing, tester log 2026-10-12 `message (8)`): 66 ids
// paired, 65 equal to this table; the only difference was id 14 (0x35, not Home). Not paired in that log: 59 (Win,
// the Start menu) and 63 (Fn, no Windows key event); both rest on the photo.
inline constexpr std::array<std::uint16_t, kKeyCount> MakePriorKeyMap() noexcept {
    std::array<std::uint16_t, kKeyCount> map{};
    const std::uint16_t pairs[][2] = {
        {0, 0x29},                                                                                             // Esc
        {1, 0x1e}, {2, 0x1f}, {3, 0x20}, {4, 0x21}, {5, 0x22}, {6, 0x23}, {7, 0x24}, {8, 0x25}, {9, 0x26},   // 1..9
        {10, 0x27}, {11, 0x2d}, {12, 0x2e}, {13, 0x2a}, {14, 0x35},                                          // 0 - = Backspace grave
        {15, 0x2b}, {16, 0x14}, {17, 0x1a}, {18, 0x08}, {19, 0x15}, {20, 0x17}, {21, 0x1c}, {22, 0x18},     // Tab Q W E R T Y U
        {23, 0x0c}, {24, 0x12}, {25, 0x13}, {26, 0x2f}, {27, 0x30}, {28, 0x31}, {29, 0x4c},                  // I O P [ ] backslash Del
        {30, 0x39}, {31, 0x04}, {32, 0x16}, {33, 0x07}, {34, 0x09}, {35, 0x0a}, {36, 0x0b}, {37, 0x0d},     // Caps A S D F G H J
        {38, 0x0e}, {39, 0x0f}, {40, 0x33}, {41, 0x34}, {42, 0x28}, {43, 0x4b},                              // K L ; ' Enter PgUp
        {44, 0xe1}, {45, 0x1d}, {46, 0x1b}, {47, 0x06}, {48, 0x19}, {49, 0x05}, {50, 0x11}, {51, 0x10},     // LShift Z X C V B N M
        {52, 0x36}, {53, 0x37}, {54, 0x38}, {55, 0xe5}, {56, 0x52}, {57, 0x4e},                              // , . / RShift Up PgDn
        {58, 0xe0}, {59, 0xe3}, {60, 0xe2}, {61, 0x2c}, {62, 0xe6}, {63, halljoy::keycode::kFn}, {64, 0xe4}, // LCtrl LWin LAlt Space RAlt Fn RCtrl
        {65, 0x50}, {66, 0x51}, {67, 0x4f},                                                                  // Left Down Right
    };
    for (std::size_t i = 0; i < sizeof(pairs) / sizeof(pairs[0]); ++i) map[pairs[i][0]] = pairs[i][1];
    return map;
}
inline constexpr std::array<std::uint16_t, kKeyCount> kPriorKeyMap = MakePriorKeyMap();

constexpr int PriorIdOfHid(std::uint16_t hid) noexcept {
    for (std::size_t id = 0; id < kKeyCount; ++id)
        if (hid && kPriorKeyMap[id] == hid) return static_cast<int>(id);
    return -1;
}

// The prior map is checked against the keyboard, not only trusted: a key-down of this keyboard from Raw
// Input (any HID usage) is paired with the key id whose position left 0 around the same moment. (Fn has no
// Windows key event, so it is never confirmed; its entry rests on the photo.)
//   - A usage that the prior map knows: the id the map names must be that key (confirm), otherwise the
//     only unexplained moving key is reported as a conflict (the map is wrong for that key; not applied).
//   - A usage the prior map does not know (Esc, Del, PgUp, ...): learned for the single unexplained
//     moving key whose id has no usage yet, so a wrong guess can never rename a mapped key.
// The stream is slow (bursts every ~50 ms), so the windows are wider than for a fast stream.
class KeyLearner {
public:
    static constexpr std::uint64_t kLearnBeforeMs = 600, kLearnAfterMs = 160;
    std::array<std::uint16_t, kKeyCount> learned{};   // id -> HID usage learned at run time (0 = none)
    const std::array<std::uint16_t, kKeyCount>* prior = &kPriorKeyMap;   // replaced only by tests

    std::uint16_t Hid(std::size_t id) const noexcept {
        return id < kKeyCount ? (learned[id] ? learned[id] : (*prior)[id]) : std::uint16_t{0};
    }
    int IdOfHid(std::uint16_t hid) const noexcept {
        for (std::size_t id = 0; id < kKeyCount; ++id)
            if (hid && Hid(id) == hid) return static_cast<int>(id);
        return -1;
    }
    std::size_t KnownCount() const noexcept {
        std::size_t n = 0;
        for (std::size_t id = 0; id < kKeyCount; ++id) n += Hid(id) ? 1u : 0u;
        return n;
    }
    std::size_t LearnedCount() const noexcept {
        std::size_t n = 0;
        for (const auto known : learned) n += known ? 1u : 0u;
        return n;
    }
    // A key's position changed (`before` -> `after`). The last press stays known after its release (a quick tap in
    // an event-driven mode can send its release before Windows delivers the key-down), and stays claimed until the
    // next press starts, so one press is never matched to two key-downs.
    void OnPosition(std::uint8_t id, std::uint8_t before, std::uint8_t after, std::uint64_t now) noexcept {
        if (id >= kKeyCount) return;
        if (!before && after) { pressStart_[id] = now; pressEnd_[id] = 0; claimed_[id] = false; }
        else if (before && !after) pressEnd_[id] = now;
    }
    void OnKeyDown(std::uint16_t hid, std::uint64_t now) noexcept {
        if (!hid || count_ >= pending_.size()) return;
        pending_[count_++] = Pending{hid, now};
    }
    // `position` is the current position of every id. Callbacks: learn(id, hid) for a new pair, confirm(id, hid,
    // lagMs) every time a key-down is matched to its id (lagMs = stream press start minus the key-down time, positive
    // when the stream is behind; first_confirm tells the first confirmation of an id), conflict(id, hid) when the
    // map disagrees, miss(id, hid) when the key-down of a mapped key expired and the stream showed no unexplained key
    // moving around it (not that key, not another one).
    template <class L, class C, class X>
    void Resolve(const std::array<std::uint8_t, kKeyCount>& position, std::uint64_t now, L&& learn, C&& confirm, X&& conflict) {
        Resolve(position, now, learn, confirm, conflict, [](std::uint8_t, std::uint16_t) {});
    }
    template <class L, class C, class X, class M>
    void Resolve(const std::array<std::uint8_t, kKeyCount>& position, std::uint64_t now, L&& learn, C&& confirm, X&& conflict,
                 M&& miss) {
        std::size_t kept = 0;
        for (std::size_t i = 0; i < count_; ++i) {
            const Pending p = pending_[i];
            const bool expired = now > p.time + kLearnAfterMs;
            const int owner = IdOfHid(p.hid);
            if (owner >= 0) {
                const auto o = static_cast<std::size_t>(owner);
                if (claimed_[o] && position[o]) continue;                       // auto-repeat of a held key
                if (Matches(o, position, p.time)) {
                    claimed_[o] = true;
                    const bool first = !confirmed_[o];
                    confirmed_[o] = true;
                    confirm(static_cast<std::uint8_t>(o), p.hid,
                            static_cast<std::int64_t>(pressStart_[o]) - static_cast<std::int64_t>(p.time), first);
                    continue;
                }
                if (expired) {
                    // The named key did not move. If exactly one other key moved without an explanation,
                    // the map is wrong for it.
                    int other = -1, matches = 0;
                    for (std::size_t id = 0; id < kKeyCount; ++id)
                        if (Matches(id, position, p.time)) { other = static_cast<int>(id); ++matches; }
                    if (matches == 1) conflict(static_cast<std::uint8_t>(other), p.hid);
                    // A miss only when no key moved at all (a Fn combination such as Fn+Up sending PgUp moves
                    // another key: the stream did report the press).
                    if (matches == 0) miss(static_cast<std::uint8_t>(o), p.hid);
                    continue;
                }
                pending_[kept++] = p;                                           // the sample may still come
                continue;
            }
            // A usage with no id yet: learn it for the single unexplained moving key without a usage.
            int candidate = -1, matches = 0;
            for (std::size_t id = 0; id < kKeyCount; ++id)
                if (!Hid(id) && !claimed_[id] && InWindow(id, position, p.time)) { candidate = static_cast<int>(id); ++matches; }
            if (matches == 1) {
                const auto c = static_cast<std::size_t>(candidate);
                learned[c] = p.hid;
                claimed_[c] = true;
                confirmed_[c] = true;
                learn(static_cast<std::uint8_t>(c), p.hid);
                continue;
            }
            if (!expired) pending_[kept++] = p;
        }
        count_ = kept;
    }

private:
    struct Pending { std::uint16_t hid = 0; std::uint64_t time = 0; };
    bool InWindow(std::size_t id, const std::array<std::uint8_t, kKeyCount>& position, std::uint64_t time) const noexcept {
        if (!position[id] || !pressStart_[id]) return false;
        return !(pressStart_[id] + kLearnBeforeMs < time || pressStart_[id] > time + kLearnAfterMs);
    }
    // The press a key-down of a mapped key belongs to: started within the window and not claimed yet, and either
    // still down or released at most kLearnAfterMs before the key-down.
    bool Matches(std::size_t id, const std::array<std::uint8_t, kKeyCount>& position, std::uint64_t time) const noexcept {
        if (!pressStart_[id] || claimed_[id]) return false;
        if (pressStart_[id] + kLearnBeforeMs < time || pressStart_[id] > time + kLearnAfterMs) return false;
        return position[id] || pressEnd_[id] + kLearnAfterMs >= time;
    }
    std::array<std::uint64_t, kKeyCount> pressStart_{}, pressEnd_{};
    std::array<bool, kKeyCount> claimed_{}, confirmed_{};
    std::array<Pending, 32> pending_{};
    std::size_t count_ = 0;
};

inline bool SelfTest() noexcept {
    const std::uint8_t check[] = {'1', '2', '3', '4', '5', '6', '7', '8', '9'};
    if (Crc16(check, sizeof(check)) != 0x29b1 || Crc16(nullptr, 0) != 0xffff) return false;
    std::array<std::uint8_t, kFrameBytes> frame{};
    if (!MakeGetterFrame(0x01, 0x87, true, frame) || frame[6] != 0xff || frame[7] != 0xff) return false;
    if (!MakeGetterFrame(0x01, 0x87, false, frame) || frame[6] != 0 || frame[7] != 0) return false;
    if (MakeGetterFrame(0x0a, 0x93, true, frame)) return false;   // set report is not a getter
    const auto empty = ParseHeader(frame.data(), frame.size());
    if (empty.state != CrcState::Skipped || empty.module != 0x01 || empty.op != 0x87) return false;
    if (!MakeSetReportFrame(kReportLive, true, frame) || frame[2] != 1 || frame[8] != 2) return false;
    if (!MakeSetReportFrame(kReportMode1, true, frame) || MakeSetReportFrame(3, true, frame) || MakeSetReportFrame(255, true, frame)) return false;
    if (ParseHeader(frame.data(), frame.size()).state != CrcState::Ok) return false;
    if (PositionToMilli(0) != 0 || PositionToMilli(kMaxPosition) != 1000 || PositionToMilli(23) != 500) return false;
    // Prior key map: every id has a usage, all unique.
    std::size_t known = 0;
    for (std::size_t id = 0; id < kKeyCount; ++id) {
        if (!kPriorKeyMap[id]) continue;
        ++known;
        for (std::size_t other = id + 1; other < kKeyCount; ++other)
            if (kPriorKeyMap[other] == kPriorKeyMap[id]) return false;
    }
    return known == kKeyCount && kPriorKeyMap[0] == 0x29 && kPriorKeyMap[17] == 0x1a && kPriorKeyMap[49] == 0x05 &&
           kPriorKeyMap[61] == 0x2c && kPriorKeyMap[63] == halljoy::keycode::kFn;
}

}  // namespace halljoy::melgeek_made68air

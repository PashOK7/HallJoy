#pragma once
// Press timing of keys bound to the gamepad on the RongYuan USB stream. Tester report
// 2026-10-16: a wired MonsGeek M1 V5 TMR sometimes lags (felt, not measured). Each finished press
// becomes one record that compares the stream with Windows' own key events of the same
// keyboard, so any log shows whether the keyboard's stream, the HallJoy reader (backlog) or
// the Windows message path (UI delay) was late. Times are QueryPerformanceCounter
// microseconds. Pure logic, unit-tested in rongyuan_stream_protocol_test.cpp.
#include <algorithm>
#include <array>
#include <cstdint>

namespace halljoy::ry_stream {
struct PressRecord {
    std::uint16_t hid = 0;
    bool os = false;              // Windows reported the key down during the press
    bool stream = false;          // the stream reported travel during the press
    std::int64_t leadUs = 0;      // Windows key-down minus the first stream sample (stream earlier: positive)
    std::int64_t downWaitUs = 0;  // Windows key-down arrived while the stream read 0: time to the first sample
    std::int64_t upWaitUs = 0;    // stream back to 0 minus Windows key-up (stream later: positive)
    std::int64_t maxGapUs = 0;    // longest pause between two stream samples of the key (holds included)
    std::int64_t durationUs = 0;
    std::int64_t afterReleaseUs = -1; // Windows-only press: time since the stream last released the key
    unsigned samples = 0, peak = 0, backlog = 0, uiDelayMs = 0;
};

class PressTiming {
public:
    static constexpr std::size_t kKeys = 0x100;

    // One stream sample of `hid` (0..1000). `backlog`: reports the reader found already queued.
    // True when the press is finished; `out` then holds its record.
    bool Sample(std::uint16_t hid, unsigned milli, std::int64_t nowUs, unsigned backlog, PressRecord& out) noexcept {
        if (hid >= kKeys) return false;
        Key& k = keys_[hid];
        if (!milli && !k.active) return false;
        if (!k.open) Open(k, hid);
        if (k.rec.samples) k.rec.maxGapUs = std::max(k.rec.maxGapUs, nowUs - k.last);
        else k.start = nowUs;
        k.last = nowUs;
        ++k.rec.samples;
        k.rec.backlog = std::max(k.rec.backlog, backlog);
        if (milli) {
            k.rec.stream = true;
            k.rec.peak = std::max(k.rec.peak, milli);
            if (k.waiting) { k.rec.downWaitUs = nowUs - k.osAt; k.waiting = false; }
            k.active = true;
            return false;
        }
        k.active = false;
        k.zeroAt = nowUs;
        k.lastRelease = nowUs;
        return !k.osDown && Finish(k, out);
    }

    // Windows key event of the same keyboard. Repeated key-downs (typematic) are ignored.
    bool OsKey(std::uint16_t hid, bool down, std::int64_t nowUs, unsigned uiDelayMs, PressRecord& out) noexcept {
        if (hid >= kKeys) return false;
        Key& k = keys_[hid];
        if (down) {
            if (k.osDown) return false;
            if (!k.open) Open(k, hid);
            k.osDown = true;
            k.osAt = nowUs;
            k.rec.os = true;
            k.rec.uiDelayMs = std::max(k.rec.uiDelayMs, uiDelayMs);
            if (!k.active) {
                k.waiting = true;
                if (!k.rec.stream && k.lastRelease) k.rec.afterReleaseUs = nowUs - k.lastRelease;
            }
            return false;
        }
        if (!k.osDown) return false;
        k.osDown = false;
        k.osUpAt = nowUs;
        k.rec.uiDelayMs = std::max(k.rec.uiDelayMs, uiDelayMs);
        return !k.active && Finish(k, out);
    }

    void Reset() noexcept { keys_ = {}; }

private:
    struct Key {
        PressRecord rec;
        bool open = false, active = false, osDown = false, waiting = false;
        std::int64_t start = 0, last = 0, osAt = 0, osUpAt = 0, zeroAt = 0, lastRelease = 0;
    };
    static void Open(Key& k, std::uint16_t hid) noexcept {
        k.rec = {};
        k.rec.hid = hid;
        k.open = true;
        k.waiting = false;
    }
    static bool Finish(Key& k, PressRecord& out) noexcept {
        PressRecord& r = k.rec;
        if (r.os && r.stream) {
            r.leadUs = k.osAt - k.start;
            r.upWaitUs = k.zeroAt - k.osUpAt;
        }
        const std::int64_t begin = r.stream && r.os ? std::min(k.start, k.osAt) : r.stream ? k.start : k.osAt;
        const std::int64_t end = r.stream && r.os ? std::max(k.zeroAt, k.osUpAt) : r.stream ? k.zeroAt : k.osUpAt;
        r.durationUs = end - begin;
        if (r.stream) r.afterReleaseUs = -1;
        out = r;
        const std::int64_t lastRelease = k.lastRelease;
        k = {};
        k.lastRelease = lastRelease;
        return true;
    }
    std::array<Key, kKeys> keys_{};
};
}  // namespace halljoy::ry_stream

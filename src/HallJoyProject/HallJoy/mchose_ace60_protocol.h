#pragma once
#include "analog_key_codes.h"
#include "mchose_jet75_protocol.h"

namespace halljoy::ace60 {
using jet75::Report;
using jet75::Settings;
using Base = std::array<std::uint8_t, 8>;
constexpr std::uint16_t Vid = 0x41e4, Pid = 0x2101;
constexpr auto Fn = keycode::kFn;
inline bool IsControl(unsigned vid, unsigned pid, unsigned page, unsigned usage, unsigned input, unsigned output) noexcept {
    return vid == Vid && pid == Pid && page == 1 && usage == 0 && input == 65 && output == 65;
}
inline bool InfoReply(const Report& reply) noexcept {
    return reply[0] == 0xaa && reply[1] == 3 && reply[2] == 0 &&
        reply[4] >= 2 && reply[4] <= 56 && !reply[5] && !reply[6] && !reply[7] &&
        reply[3] == jet75::Checksum(reply);
}
inline bool ReviewedWriter(const Report& reply) noexcept {
    constexpr char stamp[] = "Jan 10 2026,14:58:22";
    return InfoReply(reply) && reply[4] == 22 && jet75::Version(reply) == 0x0120 &&
        std::equal(stamp, stamp + sizeof(stamp) - 1, reply.begin() + 10);
}
inline bool Profile(const Base& base, unsigned& profile) noexcept {
    // Matches the exact 1.20 boot slice. The tail bytes and duplicate order
    // entries are not needed by this firmware to resolve its active profile.
    const unsigned index = base[0] < base[1] ? base[0] : 0;
    if (index >= 4) return false;
    profile = base[2 + index] <= 3 ? base[2 + index] : 0;
    return true;
}
enum class Mode : unsigned {
    Verified, AlreadyEnabled, UnreviewedWriter, ProbeFailed, ProfileRace,
    InvalidProfile, ResetArmed, AttemptSpent, Cancelled, WriteUnverified,
    NoOwnedWrite, AlreadyRestored
};
inline const char* Name(Mode mode) noexcept {
    constexpr const char* names[] = {"verified", "already_enabled", "unreviewed_writer", "read_failed",
        "profile_race", "invalid_profile", "reset_armed", "attempt_spent", "cancelled", "write_unverified",
        "no_owned_write", "already_restored"};
    return names[static_cast<unsigned>(mode)];
}
struct ModeLease {
    unsigned ownedProfile = 4;
    template<class Io>
    Mode Activate(Io& io, const Report& info, bool& attempted) {
        if (io.Cancelled()) return Mode::Cancelled;
        if (!ReviewedWriter(info)) return Mode::UnreviewedWriter;
        Base base{}, check{};
        Settings settings{};
        if (!io.ReadBase(base) || !io.ReadSettings(settings) || !io.ReadBase(check)) return Mode::ProbeFailed;
        if (base != check) return Mode::ProfileRace;
        unsigned profile = 0;
        if (!Profile(base, profile) || !jet75::ProfileValid(settings, profile)) return Mode::InvalidProfile;
        const unsigned flag = profile * 64 + 7;
        if (settings[flag] & 8) return Mode::AlreadyEnabled;
        if (settings[0] & 1) return Mode::ResetArmed;
        if (attempted) return Mode::AttemptSpent;
        if (io.Cancelled()) return Mode::Cancelled;
        attempted = true;
        ownedProfile = profile; // Even an ACK lost after the write needs cleanup.
        return Set(io, settings, base, profile, true);
    }
    template<class Io>
    Mode Restore(Io& io) {
        if (ownedProfile >= 4) return Mode::NoOwnedWrite;
        Base base{}, check{};
        Settings settings{};
        if (!io.ReadBase(base) || !io.ReadSettings(settings) || !io.ReadBase(check)) return Mode::ProbeFailed;
        if (base != check) return Mode::ProfileRace;
        unsigned active = 0;
        if (!Profile(base, active) || !jet75::ProfileValid(settings, active) ||
            !jet75::ProfileValid(settings, ownedProfile)) return Mode::InvalidProfile;
        if (!(settings[ownedProfile * 64 + 7] & 8)) return Mode::AlreadyRestored;
        if (settings[0] & 1) return Mode::ResetArmed;
        return Set(io, settings, base, ownedProfile, false);
    }
private:
    template<class Io>
    static Mode Set(Io& io, Settings expected, const Base& base, unsigned profile, bool enabled) {
        const unsigned flag = profile * 64 + 7;
        const auto request = jet75::FlagWrite(profile, expected[flag], enabled);
        io.Write(request);
        expected[flag] = request[8];
        Settings after{};
        Base check{};
        return io.ReadSettings(after) && io.ReadBase(check) && after == expected && check == base
            ? Mode::Verified : Mode::WriteUnverified;
    }
};
struct Travel {
    unsigned samples = 0, rejected = 0, distinct = 0, active = 0, peak = 0, releases = 0;
    std::array<std::uint16_t, 257> values{};
    std::array<bool, 257> seen{};
    bool Consume(const Report& p, const std::array<bool, 256>& allowed, jet75::Sample& sample) noexcept {
        if (p[0] != 0xa0) return false;
        const auto hid = p[1] == 0xf0 && p[2] == 0xff && p[3] == 1 ? Fn : jet75::Hid(p[1], p[2], p[3]);
        const unsigned depth = (p[6] << 8) | p[7], maximum = (p[14] << 8) | p[15];
        if (!hid || (hid != Fn && (hid >= allowed.size() || !allowed[hid])) || !maximum || depth > maximum) {
            ++rejected;
            return false;
        }
        sample = {hid, static_cast<std::uint16_t>((depth * 1000u + maximum / 2) / maximum)};
        const auto key = hid == Fn ? 256 : hid;
        ++samples;
        if (!seen[key]) { seen[key] = true; ++distinct; }
        if (values[key] && !sample.milli) { --active; ++releases; }
        else if (!values[key] && sample.milli) { ++active; peak = std::max(peak, active); }
        values[key] = sample.milli;
        return true;
    }
};
}

#pragma once
#include <cstdint>
#include <cwctype>
#include <string>
#include <string_view>
#include <vector>

// Game profile rules and the automatic switching policy. Pure and portable:
// no file, process or window access. The Windows service feeds it focus,
// title and process-exit events (docs/current/GAME_PROFILES_V2_2026-10-03.md).
namespace halljoy::profiles {

inline constexpr const wchar_t* kDefaultProfile = L"Default";
inline constexpr std::size_t kMaxRules = 512;
inline constexpr std::size_t kMaxProfileShortcuts = 12;
inline constexpr std::size_t kMaxTitleChars = 120;

enum class FocusLoss : std::uint8_t {
    KeepWhileRunning = 0, // keep the game's profile until it exits
    FollowFocus = 1,      // a window without a rule selects Default
};

struct Rule {
    std::wstring exe;     // file name ("cs2.exe") or full path
    std::wstring title;   // optional "contains" text, case-insensitive
    std::wstring profile;
    std::wstring path;    // last known full image path: the icon only, never matching

    Rule() = default;
    Rule(std::wstring exe_, std::wstring title_, std::wstring profile_, std::wstring path_ = {})
        : exe(std::move(exe_)), title(std::move(title_)), profile(std::move(profile_)), path(std::move(path_)) {}
};

struct ProfileShortcut {
    std::wstring profile;
    unsigned shortcut = 0;
};

struct Catalog {
    bool automatic = false;
    FocusLoss focusLoss = FocusLoss::KeepWhileRunning;
    bool notify = false;
    unsigned nextShortcut = 0;
    unsigned autoShortcut = 0;
    std::vector<Rule> games;
    std::vector<ProfileShortcut> shortcuts;
};

inline wchar_t Fold(wchar_t c) { return static_cast<wchar_t>(std::towlower(static_cast<wint_t>(c))); }

inline bool Same(std::wstring_view a, std::wstring_view b) {
    if (a.size() != b.size()) return false;
    for (std::size_t i = 0; i < a.size(); ++i) if (Fold(a[i]) != Fold(b[i])) return false;
    return true;
}

inline bool Contains(std::wstring_view text, std::wstring_view part) {
    if (part.empty()) return true;
    if (part.size() > text.size()) return false;
    for (std::size_t i = 0; i + part.size() <= text.size(); ++i)
        if (Same(text.substr(i, part.size()), part)) return true;
    return false;
}

inline bool IsFullPath(std::wstring_view exe) {
    return exe.find(L'\\') != std::wstring_view::npos || exe.find(L'/') != std::wstring_view::npos;
}

inline std::wstring_view FileName(std::wstring_view path) {
    const auto slash = path.find_last_of(L"\\/");
    return slash == std::wstring_view::npos ? path : path.substr(slash + 1);
}

// A rule's exe and title together identify it; two rules may not share both.
inline bool SameTarget(const Rule& a, const Rule& b) { return Same(a.exe, b.exe) && Same(a.title, b.title); }

inline bool ValidRule(const Rule& r) {
    if (r.exe.empty() || r.exe.size() > 32767 || r.profile.empty() || r.title.size() > kMaxTitleChars) return false;
    for (wchar_t c : r.exe) if (c < 32) return false;
    for (wchar_t c : r.title) if (c < 32) return false;
    if (r.path.size() > 32767) return false;
    for (wchar_t c : r.path) if (c < 32) return false;
    return true;
}

// Index of the most specific matching rule, or -1. A full path beats a file
// name and a title beats no title; ties keep the earlier rule.
inline int Match(const Catalog& c, std::wstring_view exePath, std::wstring_view title) {
    if (exePath.empty()) return -1;
    const auto name = FileName(exePath);
    int best = -1, bestScore = -1;
    for (std::size_t i = 0; i < c.games.size(); ++i) {
        const Rule& r = c.games[i];
        const bool full = IsFullPath(r.exe);
        if (full ? !Same(r.exe, exePath) : !Same(r.exe, name)) continue;
        if (!r.title.empty() && !Contains(title, r.title)) continue;
        const int score = (full ? 2 : 0) + (r.title.empty() ? 0 : 1);
        if (score > bestScore) { best = static_cast<int>(i); bestScore = score; }
    }
    return best;
}

inline bool AnyTitleRule(const Catalog& c) {
    for (const auto& r : c.games) if (!r.title.empty()) return true;
    return false;
}

inline unsigned ShortcutFor(const Catalog& c, std::wstring_view profile) {
    for (const auto& s : c.shortcuts) if (Same(s.profile, profile)) return s.shortcut;
    return 0;
}

// v1 stored exact paths. Use the file name when no other rule shares it;
// the old path stays as the icon path.
inline void UpgradeV1Paths(Catalog& c) {
    for (auto& r : c.games) {
        if (!IsFullPath(r.exe)) continue;
        if (r.path.empty()) r.path = r.exe;
        const std::wstring name(FileName(r.exe));
        bool shared = false;
        for (const auto& other : c.games)
            if (&other != &r && Same(FileName(other.exe), name) && !Same(other.profile, r.profile)) shared = true;
        if (shared) continue;
        bool duplicate = false;
        for (const auto& other : c.games)
            if (&other != &r && Same(other.exe, name) && Same(other.title, r.title)) duplicate = true;
        if (!duplicate) r.exe = name;
    }
}

// Rename or delete keep rules and shortcuts consistent.
inline void RenameProfile(Catalog& c, std::wstring_view from, const std::wstring& to) {
    for (auto& r : c.games) if (Same(r.profile, from)) r.profile = to;
    for (auto& s : c.shortcuts) if (Same(s.profile, from)) s.profile = to;
}
inline void ForgetProfile(Catalog& c, std::wstring_view name) {
    std::vector<Rule> games;
    for (auto& r : c.games) if (!Same(r.profile, name)) games.push_back(std::move(r));
    c.games = std::move(games);
    std::vector<ProfileShortcut> shortcuts;
    for (auto& s : c.shortcuts) if (!Same(s.profile, name)) shortcuts.push_back(std::move(s));
    c.shortcuts = std::move(shortcuts);
}

// What the focused window means for automatic switching.
struct Focus {
    bool ownWindow = false;   // a HallJoy window or dialog
    std::wstring exe;         // full image path; empty when not accessible
    std::wstring title;
    std::uint32_t pid = 0;
};

// Automatic switching state. The service reports focus changes and game exits
// and applies the returned profile name (empty: keep the current profile).
class Policy {
public:
    std::wstring OnFocus(const Catalog& c, const Focus& f, bool manual) {
        if (f.ownWindow || f.exe.empty()) return {};
        lastExternal_ = f;
        if (!c.automatic || manual) return {};
        const int rule = Match(c, f.exe, f.title);
        if (rule >= 0) {
            gamePid_ = f.pid;
            return c.games[static_cast<std::size_t>(rule)].profile;
        }
        if (c.focusLoss == FocusLoss::KeepWhileRunning && gamePid_ != 0) return {};
        gamePid_ = 0;
        return kDefaultProfile;
    }
    // The tracked game process ended. Re-evaluate the last external window.
    std::wstring OnGameExit(const Catalog& c, std::uint32_t pid, bool manual) {
        if (pid == 0 || pid != gamePid_) return {};
        gamePid_ = 0;
        if (!c.automatic || manual) return {};
        if (lastExternal_.pid == pid) return kDefaultProfile; // the game's own window was last
        return OnFocus(c, lastExternal_, manual);
    }
    // Automatic mode resumed or rules changed: decide from the last window.
    std::wstring Reevaluate(const Catalog& c, bool manual) {
        if (lastExternal_.exe.empty() || !c.automatic || manual) return {};
        const Focus last = lastExternal_;
        return OnFocus(c, last, manual);
    }
    std::uint32_t GamePid() const { return gamePid_; }
    const Focus& LastExternal() const { return lastExternal_; }
    void ForgetGame() { gamePid_ = 0; }
private:
    std::uint32_t gamePid_ = 0;
    Focus lastExternal_;
};

} // namespace halljoy::profiles

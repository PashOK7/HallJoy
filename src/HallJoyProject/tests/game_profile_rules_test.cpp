#include "game_profile_rules.h"
#include <cstdio>
#include <cstdlib>

using namespace halljoy::profiles;

static int failures = 0;
#define CHECK(x) do { if (!(x)) { std::printf("FAIL line %d: %s\n", __LINE__, #x); ++failures; } } while (0)

static Focus F(const wchar_t* exe, const wchar_t* title, std::uint32_t pid) {
    Focus f; f.exe = exe; f.title = title; f.pid = pid; return f;
}

int main() {
    Catalog c;
    c.automatic = true;
    c.games = {
        {L"cs2.exe", L"", L"CS2"},
        {L"javaw.exe", L"Minecraft", L"Minecraft"},
        {L"C:\\Emu\\retroarch.exe", L"", L"Emulator"},
        {L"retroarch.exe", L"", L"Other emulator"},
        {L"javaw.exe", L"", L"Java"},
    };

    // Matching: file name, case-insensitive; title "contains"; specificity.
    CHECK(Match(c, L"D:\\Steam\\steamapps\\common\\CS2\\game\\bin\\win64\\CS2.EXE", L"Counter-Strike 2") == 0);
    CHECK(Match(c, L"C:\\Java\\bin\\javaw.exe", L"Minecraft 1.21 - Singleplayer") == 1);
    CHECK(Match(c, L"C:\\Java\\bin\\javaw.exe", L"IntelliJ IDEA") == 4);
    CHECK(Match(c, L"c:\\emu\\RetroArch.exe", L"") == 2);   // full path beats name
    CHECK(Match(c, L"E:\\Other\\retroarch.exe", L"") == 3);
    CHECK(Match(c, L"C:\\Windows\\explorer.exe", L"") == -1);
    CHECK(Match(c, L"", L"Minecraft") == -1);
    CHECK(AnyTitleRule(c));

    // Keep while running (default): alt-tab keeps the game's profile.
    Policy p;
    CHECK(p.OnFocus(c, F(L"D:\\Games\\cs2.exe", L"Counter-Strike 2", 100), false) == L"CS2");
    CHECK(p.GamePid() == 100);
    CHECK(p.OnFocus(c, F(L"C:\\Apps\\Discord.exe", L"Discord", 200), false).empty());
    Focus own; own.ownWindow = true; own.exe = L"C:\\HallJoy\\HallJoy.exe"; own.pid = 1;
    CHECK(p.OnFocus(c, own, false).empty());                          // HallJoy never switches
    CHECK(p.LastExternal().pid == 200);
    CHECK(p.OnFocus(c, F(L"", L"elevated", 300), false).empty());     // inaccessible keeps choice
    CHECK(p.OnGameExit(c, 999, false).empty());                       // unrelated exit
    CHECK(p.OnGameExit(c, 100, false) == kDefaultProfile);            // Discord focused, game gone
    CHECK(p.GamePid() == 0);
    CHECK(p.OnFocus(c, F(L"C:\\Apps\\Discord.exe", L"Discord", 200), false) == kDefaultProfile);

    // Another game takes over; the first game's exit is then irrelevant.
    CHECK(p.OnFocus(c, F(L"D:\\Games\\cs2.exe", L"CS2", 100), false) == L"CS2");
    CHECK(p.OnFocus(c, F(L"C:\\Java\\javaw.exe", L"Minecraft", 400), false) == L"Minecraft");
    CHECK(p.OnGameExit(c, 100, false).empty());
    CHECK(p.GamePid() == 400);
    // The game's own window was the last external one.
    CHECK(p.OnGameExit(c, 400, false) == kDefaultProfile);

    // Follow focus: any unassigned window selects Default immediately.
    c.focusLoss = FocusLoss::FollowFocus;
    Policy q;
    CHECK(q.OnFocus(c, F(L"D:\\Games\\cs2.exe", L"", 100), false) == L"CS2");
    CHECK(q.OnFocus(c, F(L"C:\\Apps\\Discord.exe", L"", 200), false) == kDefaultProfile);
    CHECK(q.GamePid() == 0);

    // Manual mode and disabled automatic switching never select.
    CHECK(q.OnFocus(c, F(L"D:\\Games\\cs2.exe", L"", 100), true).empty());
    CHECK(q.Reevaluate(c, false) == L"CS2");                          // back to automatic
    c.automatic = false;
    CHECK(q.OnFocus(c, F(L"D:\\Games\\cs2.exe", L"", 100), false).empty());
    CHECK(q.Reevaluate(c, false).empty());
    c.automatic = true;

    // Title change of the focused window (emulator loading a game).
    c.focusLoss = FocusLoss::KeepWhileRunning;
    Policy t;
    CHECK(t.OnFocus(c, F(L"C:\\Java\\javaw.exe", L"Launcher", 500), false) == L"Java");
    CHECK(t.OnFocus(c, F(L"C:\\Java\\javaw.exe", L"Minecraft 1.21", 500), false) == L"Minecraft");

    // v1 upgrade: unambiguous full paths become file names.
    Catalog v1;
    v1.games = {{L"C:\\A\\game.exe", L"", L"A"}, {L"C:\\B\\tool.exe", L"", L"B"}, {L"D:\\C\\tool.exe", L"", L"C"}};
    UpgradeV1Paths(v1);
    CHECK(v1.games[0].exe == L"game.exe");
    CHECK(v1.games[0].path == L"C:\\A\\game.exe");          // the old path stays for the icon
    CHECK(v1.games[1].exe == L"C:\\B\\tool.exe" && v1.games[2].exe == L"D:\\C\\tool.exe");

    // Rename and delete keep rules and shortcuts consistent.
    c.shortcuts = {{L"CS2", 0x204}, {L"Java", 0x205}};
    RenameProfile(c, L"cs2", L"Counter-Strike");
    CHECK(c.games[0].profile == L"Counter-Strike" && ShortcutFor(c, L"counter-strike") == 0x204);
    ForgetProfile(c, L"Java");
    CHECK(c.games.size() == 4 && c.shortcuts.size() == 1 && ShortcutFor(c, L"Java") == 0);

    // Validation.
    CHECK(ValidRule({L"cs2.exe", L"", L"CS2"}));
    CHECK(!ValidRule({L"", L"", L"CS2"}));
    CHECK(!ValidRule({L"cs2.exe", L"", L""}));
    CHECK(!ValidRule({L"cs2.exe", std::wstring(kMaxTitleChars + 1, L'x'), L"CS2"}));
    CHECK(SameTarget({L"CS2.exe", L"", L"A"}, {L"cs2.exe", L"", L"B"}));
    CHECK(!SameTarget({L"cs2.exe", L"x", L"A"}, {L"cs2.exe", L"", L"B"}));

    if (failures) return EXIT_FAILURE;
    std::printf("GAME_PROFILE_RULES=PASS match specificity focus_loss title upgrade rename\n");
    return EXIT_SUCCESS;
}

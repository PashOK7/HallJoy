#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <windowsx.h>
#include <commdlg.h>
#include <shellapi.h>
#include <commctrl.h>
#include <algorithm>
#include <filesystem>
#include <map>
#include <string>
#include <vector>
#include <objidl.h>
#include <gdiplus.h>

#include "profiles_page.h"
#include "game_profile_service.h"
#include "custom_page_controls.h"
#include "custom_page_surface.h"
#include "premium_combo.h"
#include "ui_theme.h"
#include "ui_activity.h"
#include "win_util.h"
#include "app.h"
#include "keyboard_scan_hid.h"
#include "ui_popup_menu.h"

#pragma comment(lib, "Comdlg32.lib")
#pragma comment(lib, "Shell32.lib")
#pragma comment(lib, "Comctl32.lib")

using namespace Gdiplus;
namespace svc = halljoy::profiles::service;
namespace prof = halljoy::profiles;
namespace sc = halljoy::shortcuts;

namespace {

int S(HWND hwnd, int px) { return WinUtil_ScalePx(hwnd, px); }

enum Id : int {
    ID_NONE = 0, ID_NEW = 1, ID_IMPORT, ID_ACTIVATE, ID_DUPLICATE, ID_RENAME, ID_EXPORT, ID_DELETE,
    ID_ADD_RUNNING, ID_ADD_BROWSE, ID_SHORTCUT, ID_SHORTCUT_CLEAR, ID_AUTOMATIC, ID_FOCUS_COMBO,
    ID_NOTIFY, ID_NEXT_SC, ID_NEXT_SC_CLEAR, ID_AUTO_SC, ID_AUTO_SC_CLEAR, ID_RETURN_AUTO, ID_UNDO,
    ID_CARD = 1000, ID_RULE_TITLE = 2000, ID_RULE_REMOVE = 3000, ID_RECENT = 4000,
};
constexpr int kComboId = 7801, kEditId = 7802;
enum class Capture { None, Profile, Next, Auto };
enum class Edit { None, Rename, Title };

struct Hit { int id; RECT rc; bool enabled; };
struct RuleRow { std::size_t index; RECT rc, icon, text, title, remove; };
struct Chip { RECT rc; std::wstring exe, label; };

struct Page {
    CustomPageSurface surface;
    CustomPageScrollController scroll;
    int scrollY = 0, contentHeight = 0;
    std::wstring selected;
    std::vector<std::wstring> names;
    std::vector<Hit> hits;
    std::vector<RECT> cards;
    std::vector<RuleRow> rows;
    std::vector<Chip> chips;
    int hotId = 0, pressedId = 0;
    // Static layout
    RECT status{}, returnAuto{}, undo{}, error{};
    RECT leftLabel{}, newBtn{}, importBtn{};
    RECT name{}, subtitle{}, actions[5]{};
    RECT gamesLabel{}, gamesHint{}, emptyGames{}, addRunning{}, addBrowse{}, recentLabel{};
    RECT shortcutLabel{}, shortcutBtn{}, shortcutClear{};
    RECT autoLabel{}, automatic{}, focusLabel{}, focusCombo{}, notify{};
    RECT nextLabel{}, nextBtn{}, nextClear{}, autoScLabel{}, autoScBtn{}, autoScClear{};
    bool isDefault = false;
    HWND combo = nullptr;
    HWND edit = nullptr;
    Edit editKind = Edit::None;
    std::size_t editRule = 0;
    RECT editRect{};
    Capture capture = Capture::None;
    bool visualPending = false;
    std::map<std::wstring, HICON> icons; // lower-case exe file name
    std::map<std::wstring, std::wstring> knownPaths;
};

HWND g_page = nullptr;
HWND g_selector = nullptr;

Page* State(HWND h) { return reinterpret_cast<Page*>(GetWindowLongPtrW(h, GWLP_USERDATA)); }

std::wstring Lower(std::wstring s) { for (auto& c : s) c = prof::Fold(c); return s; }

// --- Data helpers ---------------------------------------------------------------
std::vector<std::size_t> RulesOf(const std::wstring& profile) {
    std::vector<std::size_t> out;
    const auto& games = svc::session.catalog.games;
    for (std::size_t i = 0; i < games.size(); ++i) if (prof::Same(games[i].profile, profile)) out.push_back(i);
    return out;
}

std::wstring ModeText() {
    if (!svc::Automatic()) return L"Automatic switching is off";
    return svc::Manual() ? L"Manual: chosen by you" : L"Automatic: follows the game in focus";
}

HICON IconFor(Page* st, const std::wstring& exe) {
    const auto key = Lower(std::wstring(prof::FileName(exe)));
    if (auto it = st->icons.find(key); it != st->icons.end()) return it->second;
    std::wstring path = prof::IsFullPath(exe) ? exe : std::wstring();
    for (const auto& rule : svc::session.catalog.games)
        if (path.empty() && !rule.path.empty() && prof::Same(prof::FileName(rule.path), key)) path = rule.path;
    if (path.empty()) if (auto it = st->knownPaths.find(key); it != st->knownPaths.end()) path = it->second;
    if (path.empty()) for (const auto& r : svc::recent) if (prof::Same(prof::FileName(r.exe), key)) path = r.exe;
    if (path.empty()) return nullptr; // retry later, when the path becomes known
    SHFILEINFOW info{};
    HICON icon = nullptr;
    if (SHGetFileInfoW(path.c_str(), 0, &info, sizeof(info), SHGFI_ICON | SHGFI_LARGEICON)) icon = info.hIcon;
    st->icons[key] = icon;
    return icon;
}

// --- Layout -----------------------------------------------------------------------
void AddHit(Page* st, int id, const RECT& rc, bool enabled = true) { st->hits.push_back({ id, rc, enabled }); }

SIZE TextSize(HWND h, const std::wstring& text) {
    HDC dc = GetDC(h);
    HGDIOBJ old = SelectObject(dc, GetStockObject(SYSTEM_FONT));
    RECT r{ 0, 0, 0, 0 };
    DrawTextW(dc, text.c_str(), -1, &r, DT_CALCRECT | DT_SINGLELINE);
    SelectObject(dc, old);
    ReleaseDC(h, dc);
    return { r.right - r.left, r.bottom - r.top };
}

void Layout(HWND h, Page* st) {
    RECT client{}; GetClientRect(h, &client);
    const int m = S(h, 16), gap = S(h, 8);
    const int width = std::max<int>(S(h, 360), client.right - S(h, 14));
    const bool twoCol = width >= S(h, 640);
    const int leftW = twoCol ? std::clamp(width * 32 / 100, S(h, 200), S(h, 280)) : width - 2 * m;
    const int xL = m, xR = twoCol ? m + leftW + S(h, 28) : m;
    const int rightW = std::max(S(h, 240), (twoCol ? width - xR : width - m) - m);
    const int rightEnd = xR + rightW;
    st->hits.clear(); st->cards.clear(); st->rows.clear(); st->chips.clear();
    GlobalProfiles_List(st->names);
    if (st->selected.empty() || !prof::Exists(st->selected)) st->selected = GlobalProfiles_GetActiveName();
    st->isDefault = GlobalProfiles_IsDefault(st->selected);
    const bool active = prof::Same(st->selected, GlobalProfiles_GetActiveName());

    int y = m;
    st->status = RECT{ m, y, width - m, y + S(h, 30) };
    const int smallBtn = S(h, 150);
    st->undo = RECT{ width - m - smallBtn, y, width - m, y + S(h, 30) };
    st->returnAuto = RECT{ st->undo.left - gap - S(h, 170), y, st->undo.left - gap, y + S(h, 30) };
    AddHit(st, ID_UNDO, st->undo, bool(svc::session.checkpoint));
    if (svc::Automatic() && svc::Manual()) AddHit(st, ID_RETURN_AUTO, st->returnAuto);
    y += S(h, 42);
    st->error = RECT{};
    if (!svc::session.error.empty()) { st->error = RECT{ m, y, width - m, y + S(h, 36) }; y += S(h, 46); }
    const int top = y;

    // Left: profile cards.
    int yl = top;
    st->leftLabel = RECT{ xL, yl, xL + leftW, yl + S(h, 18) }; yl += S(h, 26);
    for (std::size_t i = 0; i < st->names.size(); ++i) {
        RECT rc{ xL, yl, xL + leftW, yl + S(h, 52) };
        st->cards.push_back(rc);
        AddHit(st, ID_CARD + static_cast<int>(i), rc);
        yl += S(h, 58);
    }
    const int halfL = (leftW - gap) / 2;
    st->newBtn = RECT{ xL, yl + S(h, 4), xL + halfL, yl + S(h, 34) };
    st->importBtn = RECT{ xL + halfL + gap, yl + S(h, 4), xL + leftW, yl + S(h, 34) };
    AddHit(st, ID_NEW, st->newBtn); AddHit(st, ID_IMPORT, st->importBtn);
    yl = st->newBtn.bottom + m;

    // Right: selected profile.
    int yr = twoCol ? top : yl + S(h, 8);
    st->name = RECT{ xR, yr, rightEnd, yr + S(h, 26) }; yr += S(h, 26);
    st->subtitle = RECT{ xR, yr, rightEnd, yr + S(h, 20) }; yr += S(h, 30);
    const int bw = std::min(S(h, 112), (rightW - 4 * gap) / 5);
    const int actionIds[5] = { ID_ACTIVATE, ID_DUPLICATE, ID_RENAME, ID_EXPORT, ID_DELETE };
    for (int i = 0; i < 5; ++i) {
        st->actions[i] = RECT{ xR + i * (bw + gap), yr, xR + i * (bw + gap) + bw, yr + S(h, 30) };
        bool enabled = true;
        if (actionIds[i] == ID_ACTIVATE) enabled = !active;
        if (actionIds[i] == ID_RENAME) enabled = !st->isDefault;
        if (actionIds[i] == ID_DELETE) enabled = !st->isDefault && !active;
        AddHit(st, actionIds[i], st->actions[i], enabled && !svc::session.readOnly);
    }
    yr += S(h, 30) + S(h, 26);

    if (!st->isDefault) {
        st->gamesLabel = RECT{ xR, yr, rightEnd, yr + S(h, 18) }; yr += S(h, 20);
        st->gamesHint = RECT{ xR, yr, rightEnd, yr + S(h, 18) }; yr += S(h, 28);
        const auto rules = RulesOf(st->selected);
        st->emptyGames = RECT{};
        if (rules.empty()) { st->emptyGames = RECT{ xR, yr, rightEnd, yr + S(h, 40) }; yr += S(h, 48); }
        for (std::size_t k = 0; k < rules.size(); ++k) {
            RuleRow row{};
            row.index = rules[k];
            row.rc = RECT{ xR, yr, rightEnd, yr + S(h, 48) };
            const int is = S(h, 28);
            row.icon = RECT{ xR + S(h, 10), yr + (S(h, 48) - is) / 2, xR + S(h, 10) + is, yr + (S(h, 48) + is) / 2 };
            row.remove = RECT{ rightEnd - S(h, 10) - S(h, 30), yr + S(h, 10), rightEnd - S(h, 10), yr + S(h, 38) };
            row.title = RECT{ row.remove.left - gap - S(h, 128), yr + S(h, 10), row.remove.left - gap, yr + S(h, 38) };
            row.text = RECT{ row.icon.right + S(h, 10), yr + S(h, 6), row.title.left - gap, yr + S(h, 42) };
            AddHit(st, ID_RULE_TITLE + static_cast<int>(row.index), row.title);
            AddHit(st, ID_RULE_REMOVE + static_cast<int>(row.index), row.remove);
            st->rows.push_back(row);
            yr += S(h, 54);
        }
        const int ab = std::min(S(h, 170), (rightW - gap) / 2);
        st->addRunning = RECT{ xR, yr, xR + ab, yr + S(h, 30) };
        st->addBrowse = RECT{ xR + ab + gap, yr, xR + 2 * ab + gap, yr + S(h, 30) };
        AddHit(st, ID_ADD_RUNNING, st->addRunning, !svc::session.readOnly);
        AddHit(st, ID_ADD_BROWSE, st->addBrowse, !svc::session.readOnly);
        yr += S(h, 40);
        // One-click suggestions: recently focused programs that have no rule.
        std::vector<svc::RecentApp> suggestions;
        for (const auto& r : svc::recent) {
            bool assigned = false;
            for (const auto& g : svc::session.catalog.games)
                if (g.title.empty() && prof::Match(prof::Catalog{ false, {}, false, 0, 0, { g }, {} }, r.exe, r.title) >= 0) assigned = true;
            if (!assigned && suggestions.size() < 4) suggestions.push_back(r);
        }
        st->recentLabel = RECT{};
        if (!suggestions.empty()) {
            st->recentLabel = RECT{ xR, yr, rightEnd, yr + S(h, 18) }; yr += S(h, 24);
            int x = xR;
            for (std::size_t i = 0; i < suggestions.size(); ++i) {
                const std::wstring exe(prof::FileName(suggestions[i].exe));
                const std::wstring label = L"+  " + exe;
                const int w = std::min(rightW, int(TextSize(h, label).cx) + S(h, 28));
                if (x + w > rightEnd && x > xR) { x = xR; yr += S(h, 34); }
                Chip chip{ RECT{ x, yr, x + w, yr + S(h, 28) }, suggestions[i].exe, label };
                st->knownPaths[Lower(exe)] = suggestions[i].exe;
                AddHit(st, ID_RECENT + static_cast<int>(st->chips.size()), chip.rc, !svc::session.readOnly);
                st->chips.push_back(std::move(chip));
                x += w + gap;
            }
            yr += S(h, 40);
        }
        yr += S(h, 8);
        st->shortcutLabel = RECT{ xR, yr, rightEnd, yr + S(h, 18) }; yr += S(h, 24);
        st->shortcutBtn = RECT{ xR, yr, xR + S(h, 190), yr + S(h, 30) };
        st->shortcutClear = RECT{ st->shortcutBtn.right + gap, yr, st->shortcutBtn.right + gap + S(h, 80), yr + S(h, 30) };
        AddHit(st, ID_SHORTCUT, st->shortcutBtn, !svc::session.readOnly);
        AddHit(st, ID_SHORTCUT_CLEAR, st->shortcutClear, prof::ShortcutFor(svc::session.catalog, st->selected) != 0 || st->capture == Capture::Profile);
        yr += S(h, 30) + S(h, 28);
    } else {
        st->gamesLabel = st->gamesHint = st->emptyGames = st->addRunning = st->addBrowse = st->recentLabel = RECT{};
        st->shortcutLabel = RECT{ xR, yr, rightEnd, yr + S(h, 18) }; yr += S(h, 24);
        st->shortcutBtn = RECT{ xR, yr, xR + S(h, 190), yr + S(h, 30) };
        st->shortcutClear = RECT{ st->shortcutBtn.right + gap, yr, st->shortcutBtn.right + gap + S(h, 80), yr + S(h, 30) };
        AddHit(st, ID_SHORTCUT, st->shortcutBtn, !svc::session.readOnly);
        AddHit(st, ID_SHORTCUT_CLEAR, st->shortcutClear, prof::ShortcutFor(svc::session.catalog, st->selected) != 0 || st->capture == Capture::Profile);
        yr += S(h, 30) + S(h, 28);
    }

    // Automatic switching (application-wide).
    st->autoLabel = RECT{ xR, yr, rightEnd, yr + S(h, 18) }; yr += S(h, 26);
    st->automatic = RECT{ xR, yr, rightEnd, yr + S(h, 28) }; AddHit(st, ID_AUTOMATIC, st->automatic, !svc::session.readOnly); yr += S(h, 36);
    const int labelW = std::min(S(h, 200), rightW / 2);
    st->focusLabel = RECT{ xR, yr, xR + labelW, yr + S(h, 30) };
    st->focusCombo = RECT{ xR + labelW, yr, std::min(rightEnd, xR + labelW + S(h, 280)), yr + S(h, 30) };
    AddHit(st, ID_FOCUS_COMBO, st->focusCombo, !svc::session.readOnly); yr += S(h, 40);
    st->notify = RECT{ xR, yr, rightEnd, yr + S(h, 28) }; AddHit(st, ID_NOTIFY, st->notify, !svc::session.readOnly); yr += S(h, 40);
    auto shortcutRow = [&](RECT& label, RECT& btn, RECT& clear, int id, int clearId, bool assigned, bool capturing) {
        label = RECT{ xR, yr, xR + labelW, yr + S(h, 30) };
        btn = RECT{ xR + labelW, yr, xR + labelW + S(h, 190), yr + S(h, 30) };
        clear = RECT{ btn.right + gap, yr, btn.right + gap + S(h, 80), yr + S(h, 30) };
        AddHit(st, id, btn, !svc::session.readOnly);
        AddHit(st, clearId, clear, assigned || capturing);
        yr += S(h, 38);
    };
    shortcutRow(st->nextLabel, st->nextBtn, st->nextClear, ID_NEXT_SC, ID_NEXT_SC_CLEAR,
        svc::session.catalog.nextShortcut != 0, st->capture == Capture::Next);
    shortcutRow(st->autoScLabel, st->autoScBtn, st->autoScClear, ID_AUTO_SC, ID_AUTO_SC_CLEAR,
        svc::session.catalog.autoShortcut != 0, st->capture == Capture::Auto);
    yr += m;

    st->contentHeight = std::max(yl, yr);
    CustomPageSurface_SetContentHeight(h, &st->surface, st->contentHeight);
    st->scrollY = std::clamp(st->scrollY, 0, CustomPageSurface_GetMaxScroll(h, &st->surface));
    st->surface.scrollY = st->scrollY;
    CustomPageSurface_MarkDirty(h, &st->surface);
}

// --- Rendering -----------------------------------------------------------------
const Hit* FindHit(Page* st, int id) {
    for (const auto& hit : st->hits) if (hit.id == id) return &hit;
    return nullptr;
}
bool Enabled(Page* st, int id) { const Hit* hit = FindHit(st, id); return hit && hit->enabled; }

std::wstring CaptureText(Page* st, Capture which, unsigned shortcut) {
    if (st->capture == which) return L"Press keys... (Esc cancels)";
    return shortcut ? ShortcutText(shortcut) : L"Set shortcut";
}

void Render(HWND h, HDC hdc, const RECT&, void* user) {
    auto* st = static_cast<Page*>(user);
    HGDIOBJ oldFont = SelectObject(hdc, GetStockObject(SYSTEM_FONT));
    Graphics g(hdc);
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    g.SetPixelOffsetMode(PixelOffsetModeHighQuality);
    const COLORREF text = UiTheme::Color_Text(), muted = UiTheme::Color_TextMuted();
    const UINT one = DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS;
    auto button = [&](int id, const RECT& rc, const std::wstring& label) {
        CustomPage_DrawButton(g, hdc, rc, label, st->hotId == id, st->pressedId == id, Enabled(st, id));
    };
    const auto& catalog = svc::session.catalog;
    const auto active = GlobalProfiles_GetActiveName();

    // Status strip.
    CustomPage_DrawRoundRect(g, st->status, UiTheme::Color_ControlBg(), UiTheme::Color_Border(), (float)S(h, 6));
    RECT statusText = st->status; statusText.left += S(h, 12);
    statusText.right = (svc::Automatic() && svc::Manual() ? st->returnAuto.left : st->undo.left) - S(h, 8);
    CustomPage_DrawText(hdc, L"Active: " + active + L"   |   " + ModeText(), statusText, text, one);
    if (FindHit(st, ID_RETURN_AUTO)) button(ID_RETURN_AUTO, st->returnAuto, L"Return to automatic");
    button(ID_UNDO, st->undo, L"Undo edits");
    if (st->error.bottom > st->error.top)
        CustomPage_DrawText(hdc, svc::session.error, st->error, RGB(255, 185, 100), DT_LEFT | DT_WORDBREAK | DT_END_ELLIPSIS);

    // Profile cards.
    CustomPage_DrawText(hdc, L"Profiles", st->leftLabel, muted, one);
    for (std::size_t i = 0; i < st->cards.size(); ++i) {
        const RECT& rc = st->cards[i];
        const auto& name = st->names[i];
        const bool sel = prof::Same(name, st->selected), hot = st->hotId == ID_CARD + int(i);
        const bool isActive = prof::Same(name, active);
        CustomPage_DrawRoundRect(g, rc, sel ? RGB(46, 46, 50) : hot ? RGB(40, 40, 42) : UiTheme::Color_ControlBg(),
            sel ? UiTheme::Color_Accent() : UiTheme::Color_Border(), (float)S(h, 6));
        if (isActive) {
            const int d = S(h, 8);
            SolidBrush dot(Color(255, GetRValue(UiTheme::Color_Accent()), GetGValue(UiTheme::Color_Accent()), GetBValue(UiTheme::Color_Accent())));
            g.FillEllipse(&dot, rc.left + S(h, 10), (rc.top + rc.bottom - d) / 2, d, d);
        }
        RECT line1{ rc.left + S(h, 26), rc.top + S(h, 7), rc.right - S(h, 10), rc.top + S(h, 27) };
        RECT line2{ rc.left + S(h, 26), rc.top + S(h, 27), rc.right - S(h, 10), rc.bottom - S(h, 6) };
        CustomPage_DrawText(hdc, name, line1, text, one);
        std::wstring detail;
        if (GlobalProfiles_IsDefault(name)) detail = L"When no game is in focus";
        else {
            const auto n = RulesOf(name).size();
            detail = n == 0 ? L"No games" : n == 1 ? L"1 game" : std::to_wstring(n) + L" games";
        }
        if (isActive) detail = L"Active  |  " + detail;
        if (const unsigned s = prof::ShortcutFor(catalog, name)) detail += L"  |  " + ShortcutText(s);
        CustomPage_DrawText(hdc, detail, line2, muted, one);
    }
    button(ID_NEW, st->newBtn, L"+ New profile");
    button(ID_IMPORT, st->importBtn, L"Import...");

    // Selected profile.
    if (st->editKind != Edit::Rename) CustomPage_DrawText(hdc, st->selected, st->name, text, one);
    const bool selActive = prof::Same(st->selected, active);
    std::wstring sub = selActive ? L"Active profile. Edits in Remap and Configuration are saved automatically."
                                 : L"Not active. Activate it to edit its bindings and curves.";
    if (st->isDefault) sub = (selActive ? L"Active. " : L"") + std::wstring(L"Used when no assigned game is in focus.");
    CustomPage_DrawText(hdc, sub, st->subtitle, muted, one);
    const wchar_t* actionLabels[5] = { selActive ? L"Active" : L"Activate", L"Duplicate", L"Rename", L"Export...", L"Delete" };
    const int actionIds[5] = { ID_ACTIVATE, ID_DUPLICATE, ID_RENAME, ID_EXPORT, ID_DELETE };
    for (int i = 0; i < 5; ++i) {
        if (actionIds[i] == ID_DELETE && Enabled(st, ID_DELETE)) {
            const bool hot = st->hotId == ID_DELETE, pressed = st->pressedId == ID_DELETE;
            CustomPage_DrawRoundRect(g, st->actions[i], pressed ? RGB(82, 28, 34) : hot ? RGB(128, 41, 50) : RGB(108, 35, 43),
                RGB(222, 78, 91), 3.0f);
            CustomPage_DrawText(hdc, actionLabels[i], st->actions[i], RGB(255, 238, 240), DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        } else button(actionIds[i], st->actions[i], actionLabels[i]);
    }

    if (!st->isDefault) {
        CustomPage_DrawText(hdc, L"Games", st->gamesLabel, text, one);
        CustomPage_DrawText(hdc, L"This profile is selected automatically while one of these programs is in focus.", st->gamesHint, muted, one);
        if (st->emptyGames.bottom > st->emptyGames.top) {
            CustomPage_DrawRoundRect(g, st->emptyGames, UiTheme::Color_PanelBg(), UiTheme::Color_Border(), (float)S(h, 6));
            CustomPage_DrawText(hdc, L"No games yet. Start the game, then use \"Add running app\" or a suggestion below.",
                st->emptyGames, muted, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        }
        for (const auto& row : st->rows) {
            if (row.index >= catalog.games.size()) continue;
            const auto& rule = catalog.games[row.index];
            CustomPage_DrawRoundRect(g, row.rc, UiTheme::Color_ControlBg(), UiTheme::Color_Border(), (float)S(h, 6));
            if (HICON icon = IconFor(st, rule.exe))
                DrawIconEx(hdc, row.icon.left, row.icon.top, icon, row.icon.right - row.icon.left, row.icon.bottom - row.icon.top, 0, nullptr, DI_NORMAL);
            else {
                CustomPage_DrawRoundRect(g, row.icon, RGB(60, 60, 64), UiTheme::Color_Border(), (float)S(h, 5));
                const std::wstring letter(1, rule.exe.empty() ? L'?' : static_cast<wchar_t>(towupper(prof::FileName(rule.exe)[0])));
                CustomPage_DrawText(hdc, letter, row.icon, text, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            }
            RECT l1{ row.text.left, row.text.top, row.text.right, row.text.top + S(h, 18) };
            RECT l2{ row.text.left, row.text.top + S(h, 18), row.text.right, row.text.bottom };
            CustomPage_DrawText(hdc, std::wstring(prof::FileName(rule.exe)), l1, text, one);
            std::wstring where = rule.title.empty() ? L"Any window" : L"Window title contains \"" + rule.title + L"\"";
            if (prof::IsFullPath(rule.exe)) where += L"  |  " + rule.exe;
            CustomPage_DrawText(hdc, where, l2, muted, one);
            const int titleId = ID_RULE_TITLE + int(row.index), removeId = ID_RULE_REMOVE + int(row.index);
            if (!(st->editKind == Edit::Title && st->editRule == row.index))
                button(titleId, row.title, rule.title.empty() ? L"Match title..." : L"Edit title...");
            button(removeId, row.remove, L"X");
        }
        button(ID_ADD_RUNNING, st->addRunning, L"Add running app...");
        button(ID_ADD_BROWSE, st->addBrowse, L"Browse for EXE...");
        if (st->recentLabel.bottom > st->recentLabel.top) {
            CustomPage_DrawText(hdc, L"Recently in focus (click to add):", st->recentLabel, muted, one);
            for (std::size_t i = 0; i < st->chips.size(); ++i) {
                const bool hot = st->hotId == ID_RECENT + int(i);
                CustomPage_DrawRoundRect(g, st->chips[i].rc, hot ? RGB(46, 46, 50) : UiTheme::Color_ControlBg(),
                    hot ? UiTheme::Color_Accent() : UiTheme::Color_Border(), (float)S(h, 12));
                CustomPage_DrawText(hdc, st->chips[i].label, st->chips[i].rc, text, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
            }
        }
    }
    CustomPage_DrawText(hdc, L"Profile shortcut (switches to this profile, manual mode)", st->shortcutLabel, text, one);
    button(ID_SHORTCUT, st->shortcutBtn, CaptureText(st, Capture::Profile, prof::ShortcutFor(catalog, st->selected)));
    button(ID_SHORTCUT_CLEAR, st->shortcutClear, st->capture == Capture::Profile ? L"Cancel" : L"Clear");

    CustomPage_DrawText(hdc, L"Automatic switching", st->autoLabel, text, one);
    CustomPage_DrawCheckbox(g, hdc, h, st->automatic, L"Select the profile of the game in focus", catalog.automatic, Enabled(st, ID_AUTOMATIC));
    CustomPage_DrawText(hdc, L"When a game loses focus:", st->focusLabel, catalog.automatic ? text : muted, one);
    if (st->combo) PremiumCombo::PaintRetainedFace(st->combo, hdc, st->focusCombo, st->hotId == ID_FOCUS_COMBO);
    CustomPage_DrawCheckbox(g, hdc, h, st->notify, L"Show a notification when the profile changes", catalog.notify, Enabled(st, ID_NOTIFY));
    CustomPage_DrawText(hdc, L"Next profile:", st->nextLabel, text, one);
    button(ID_NEXT_SC, st->nextBtn, CaptureText(st, Capture::Next, catalog.nextShortcut));
    button(ID_NEXT_SC_CLEAR, st->nextClear, st->capture == Capture::Next ? L"Cancel" : L"Clear");
    CustomPage_DrawText(hdc, L"Return to automatic:", st->autoScLabel, text, one);
    button(ID_AUTO_SC, st->autoScBtn, CaptureText(st, Capture::Auto, catalog.autoShortcut));
    button(ID_AUTO_SC_CLEAR, st->autoScClear, st->capture == Capture::Auto ? L"Cancel" : L"Clear");
    SelectObject(hdc, oldFont);
}

int HitTest(Page* st, POINT client) {
    const POINT pt = CustomPageSurface_ClientToContent(&st->surface, client);
    for (auto it = st->hits.rbegin(); it != st->hits.rend(); ++it)
        if (PtInRect(&it->rc, pt)) return it->enabled ? it->id : -1;
    return 0;
}

// --- Actions -----------------------------------------------------------------------
void Refresh(HWND h, Page* st) { Layout(h, st); InvalidateRect(h, nullptr, FALSE); }

void ShowError(HWND h, const std::wstring& message) {
    MessageBoxW(h, message.c_str(), L"Profiles", MB_OK | MB_ICONWARNING);
}

bool StoreRule(HWND h, prof::Rule rule) {
    for (const auto& a : svc::session.catalog.games)
        if (prof::SameTarget(a, rule)) {
            if (prof::Same(a.profile, rule.profile)) return true;
            ShowError(h, std::wstring(prof::FileName(rule.exe)) + L" is already assigned to \"" + a.profile + L"\". Remove it there first.");
            return false;
        }
    auto next = svc::session.catalog;
    next.games.push_back(std::move(rule));
    if (!svc::Store(std::move(next))) { ShowError(h, svc::session.error); return false; }
    return true;
}

void AssignExe(HWND h, Page* st, const std::wstring& path, const std::wstring& title = {}) {
    if (path.empty()) return;
    const std::wstring exe(prof::FileName(path));
    st->knownPaths[Lower(exe)] = path;
    st->icons.erase(Lower(exe));
    StoreRule(h, prof::Rule{ exe, title, st->selected, path });
}

struct Running { std::wstring path, title; };
void AddRunningApp(HWND h, Page* st, const RECT& anchorContent) {
    std::vector<Running> apps;
    EnumWindows([](HWND w, LPARAM raw) -> BOOL {
        if (!IsWindowVisible(w) || GetWindow(w, GW_OWNER)) return TRUE;
        wchar_t title[256]{};
        if (!GetWindowTextW(w, title, 256)) return TRUE;
        DWORD pid = 0; GetWindowThreadProcessId(w, &pid);
        if (pid == GetCurrentProcessId()) return TRUE;
        auto path = svc::ProcessPath(pid);
        if (path.empty()) return TRUE;
        auto& list = *reinterpret_cast<std::vector<Running>*>(raw);
        for (const auto& a : list) if (prof::Same(a.path, path)) return TRUE;
        if (list.size() < 60) list.push_back({ std::move(path), title });
        return TRUE;
    }, reinterpret_cast<LPARAM>(&apps));
    std::sort(apps.begin(), apps.end(), [](const Running& a, const Running& b) { return _wcsicmp(a.title.c_str(), b.title.c_str()) < 0; });
    std::vector<halljoy::ui_menu::Item> items;
    std::vector<HICON> icons;
    for (std::size_t i = 0; i < apps.size(); ++i) {
        halljoy::ui_menu::Item item;
        item.id = static_cast<UINT>(i + 1);
        item.text = apps[i].title;
        item.detail = std::wstring(prof::FileName(apps[i].path));
        SHFILEINFOW info{};
        if (SHGetFileInfoW(apps[i].path.c_str(), 0, &info, sizeof(info), SHGFI_ICON | SHGFI_SMALLICON)) {
            item.icon = info.hIcon;
            icons.push_back(info.hIcon);
        }
        items.push_back(std::move(item));
    }
    if (apps.empty()) {
        halljoy::ui_menu::Item none; none.text = L"No accessible applications are running"; none.enabled = false;
        items.push_back(none);
    }
    RECT anchor = CustomPageSurface_ContentToClient(&st->surface, anchorContent);
    POINT pt{ anchor.left, anchor.bottom }; ClientToScreen(h, &pt);
    const UINT id = halljoy::ui_menu::Track(h, items, pt);
    for (HICON icon : icons) DestroyIcon(icon);
    if (id && id <= apps.size()) AssignExe(h, st, apps[id - 1].path);
}

void BrowseExe(HWND h, Page* st) {
    std::wstring path(32768, L'\0');
    OPENFILENAMEW of{ sizeof(of) };
    of.hwndOwner = h; of.lpstrFilter = L"Applications (*.exe)\0*.exe\0\0";
    of.lpstrFile = path.data(); of.nMaxFile = static_cast<DWORD>(path.size());
    of.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    of.lpstrTitle = L"Choose the game's executable (not its launcher)";
    if (GetOpenFileNameW(&of)) AssignExe(h, st, path.c_str());
}

void EndEdit(HWND h, Page* st, bool commit);

LRESULT CALLBACK EditProc(HWND e, UINT m, WPARAM w, LPARAM l, UINT_PTR, DWORD_PTR) {
    HWND page = GetParent(e);
    if (m == WM_KEYDOWN && (w == VK_RETURN || w == VK_ESCAPE)) {
        if (Page* st = State(page)) EndEdit(page, st, w == VK_RETURN);
        return 0;
    }
    if (m == WM_CHAR && (w == VK_RETURN || w == VK_ESCAPE)) return 0;
    if (m == WM_KILLFOCUS && State(page)) PostMessageW(page, WM_APP + 30, 0, 0);
    return DefSubclassProc(e, m, w, l);
}

void BeginEdit(HWND h, Page* st, Edit kind, const RECT& contentRect, const std::wstring& initial, std::size_t rule = 0) {
    EndEdit(h, st, false);
    st->editKind = kind; st->editRule = rule; st->editRect = contentRect;
    RECT r = CustomPageSurface_ContentToClient(&st->surface, contentRect);
    st->edit = CreateWindowExW(0, L"EDIT", initial.c_str(), WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
        r.left, r.top, r.right - r.left, r.bottom - r.top, h, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kEditId)),
        GetModuleHandleW(nullptr), nullptr);
    if (!st->edit) { st->editKind = Edit::None; return; }
    SendMessageW(st->edit, WM_SETFONT, reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)), TRUE);
    SendMessageW(st->edit, EM_SETLIMITTEXT, kind == Edit::Rename ? 120 : prof::kMaxTitleChars, 0);
    UiTheme::ApplyToControl(st->edit);
    SetWindowSubclass(st->edit, EditProc, 1, 0);
    SetFocus(st->edit);
    SendMessageW(st->edit, EM_SETSEL, 0, -1);
    CustomPageSurface_MarkDirty(h, &st->surface);
}

void EndEdit(HWND h, Page* st, bool commit) {
    if (!st->edit) return;
    wchar_t buffer[256]{};
    GetWindowTextW(st->edit, buffer, 256);
    HWND edit = st->edit; st->edit = nullptr;
    const Edit kind = st->editKind; st->editKind = Edit::None;
    RemoveWindowSubclass(edit, EditProc, 1);
    DestroyWindow(edit);
    std::wstring value(buffer);
    while (!value.empty() && value.back() == L' ') value.pop_back();
    while (!value.empty() && value.front() == L' ') value.erase(value.begin());
    if (commit && kind == Edit::Rename && !value.empty() && value != st->selected) {
        const auto clean = GlobalProfiles_SanitizeName(value);
        if (clean != value || !prof::NewName(value)) ShowError(h, L"Choose a unique name without \\ / : * ? \" < > |.");
        else if (svc::Rename(st->selected, value)) st->selected = value;
        else ShowError(h, svc::session.error);
    }
    if (commit && kind == Edit::Title && st->editRule < svc::session.catalog.games.size()) {
        auto next = svc::session.catalog;
        auto& rule = next.games[st->editRule];
        prof::Rule changed = rule; changed.title = value;
        bool clash = false;
        for (std::size_t i = 0; i < next.games.size(); ++i)
            if (i != st->editRule && prof::SameTarget(next.games[i], changed)) clash = true;
        if (clash) ShowError(h, L"Another rule already uses this program and title.");
        else if (changed.title != rule.title) { rule = changed; if (!svc::Store(std::move(next))) ShowError(h, svc::session.error); }
    }
    Refresh(h, st);
}

void StopCapture(HWND h, Page* st) {
    if (st->capture == Capture::None) return;
    st->capture = Capture::None;
    App_EndShortcutCapture(h);
}

void StartCapture(HWND h, Page* st, Capture which) {
    const bool same = st->capture == which;
    StopCapture(h, st);
    if (!same) { st->capture = which; SetFocus(h); App_BeginShortcutCapture(h); }
    Refresh(h, st);
}

unsigned ActionFor(Page* st, Capture which) {
    if (which == Capture::Next) return static_cast<unsigned>(sc::Action::NextProfile);
    if (which == Capture::Auto) return static_cast<unsigned>(sc::Action::AutoProfiles);
    const auto& list = svc::session.catalog.shortcuts;
    for (std::size_t i = 0; i < list.size() && i < sc::kProfileSlots; ++i)
        if (prof::Same(list[i].profile, st->selected)) return static_cast<unsigned>(sc::ProfileSlot(static_cast<unsigned>(i)));
    return 0;
}

void SetShortcut(HWND h, Page* st, Capture which, unsigned shortcut) {
    auto next = svc::session.catalog;
    if (which == Capture::Next) next.nextShortcut = shortcut;
    else if (which == Capture::Auto) next.autoShortcut = shortcut;
    else {
        next.shortcuts.erase(std::remove_if(next.shortcuts.begin(), next.shortcuts.end(),
            [&](const auto& s) { return prof::Same(s.profile, st->selected); }), next.shortcuts.end());
        if (shortcut) {
            if (next.shortcuts.size() >= prof::kMaxProfileShortcuts) { ShowError(h, L"At most 12 profiles can have their own shortcut."); return; }
            next.shortcuts.push_back({ st->selected, shortcut });
        }
    }
    if (!svc::Store(std::move(next))) ShowError(h, svc::session.error);
}

void OnCaptured(HWND h, Page* st, unsigned result) {
    const Capture which = st->capture;
    StopCapture(h, st);
    if (which != Capture::None && result && result != sc::kCaptureCancelled) {
        const DWORD error = App_ValidateCommandShortcut(ActionFor(st, which), result);
        if (error == ERROR_ALREADY_ASSIGNED) ShowError(h, L"This shortcut is already used by another command.");
        else if (error) ShowError(h, L"This key cannot be used as a shortcut.");
        else SetShortcut(h, st, which, result);
    }
    Refresh(h, st);
}

void NewProfile(HWND h, Page* st) {
    std::vector<halljoy::ui_menu::Item> items(2);
    items[0].id = 1; items[0].text = L"Copy of the active profile"; items[0].detail = GlobalProfiles_GetActiveName();
    items[1].id = 2; items[1].text = L"Factory defaults"; items[1].detail = L"empty bindings";
    RECT anchor = CustomPageSurface_ContentToClient(&st->surface, st->newBtn);
    POINT pt{ anchor.left, anchor.bottom }; ClientToScreen(h, &pt);
    const UINT choice = halljoy::ui_menu::Track(h, items, pt);
    if (!choice) return;
    const auto name = prof::UniqueName(L"New profile");
    if (name.empty() || !prof::Duplicate(GlobalProfiles_GetActiveName(), name, choice == 2)) { ShowError(h, L"Could not create a profile."); return; }
    st->selected = name;
    Refresh(h, st);
    BeginEdit(h, st, Edit::Rename, st->name, name);
}

void ExportProfile(HWND h, Page* st) {
    std::wstring path(32768, L'\0');
    const auto suggested = st->selected + prof::kExportExtension;
    std::copy(suggested.begin(), suggested.end(), path.begin());
    OPENFILENAMEW of{ sizeof(of) };
    of.hwndOwner = h; of.lpstrFilter = L"HallJoy profile (*.hjprofile)\0*.hjprofile\0\0";
    of.lpstrDefExt = L"hjprofile"; of.lpstrFile = path.data(); of.nMaxFile = static_cast<DWORD>(path.size());
    of.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    of.lpstrTitle = L"Export profile";
    if (!GetSaveFileNameW(&of)) return;
    std::wstring error;
    if (!prof::Export(st->selected, svc::session.catalog, path.c_str(), error)) ShowError(h, error);
}

void ImportProfile(HWND h, Page* st) {
    std::wstring path(32768, L'\0');
    OPENFILENAMEW of{ sizeof(of) };
    of.hwndOwner = h; of.lpstrFilter = L"HallJoy profile (*.hjprofile)\0*.hjprofile\0All files\0*.*\0\0";
    of.lpstrFile = path.data(); of.nMaxFile = static_cast<DWORD>(path.size());
    of.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    of.lpstrTitle = L"Import profile";
    if (!GetOpenFileNameW(&of)) return;
    prof::Imported imported; std::wstring error;
    if (!prof::Import(path.c_str(), imported, error)) { ShowError(h, error); return; }
    st->selected = imported.name;
    auto next = svc::session.catalog;
    std::size_t added = 0, skipped = 0;
    for (auto& rule : imported.rules) {
        bool clash = false;
        for (const auto& a : next.games) if (prof::SameTarget(a, rule)) clash = true;
        if (clash || next.games.size() >= prof::kMaxRules) { ++skipped; continue; }
        next.games.push_back(rule); ++added;
    }
    if (added && !svc::Store(std::move(next))) ShowError(h, svc::session.error);
    std::wstring message = L"Imported profile \"" + imported.name + L"\".";
    if (added) message += L" Games assigned: " + std::to_wstring(added) + L".";
    if (skipped) message += L" Games skipped because another profile already uses them: " + std::to_wstring(skipped) + L".";
    MessageBoxW(h, message.c_str(), L"Profiles", MB_OK | MB_ICONINFORMATION);
    Refresh(h, st);
}

void Command(HWND h, Page* st, int id) {
    auto& catalog = svc::session.catalog;
    if (id >= ID_RECENT) {
        const std::size_t i = static_cast<std::size_t>(id - ID_RECENT);
        if (i < st->chips.size()) AssignExe(h, st, st->chips[i].exe);
    } else if (id >= ID_RULE_REMOVE) {
        const std::size_t i = static_cast<std::size_t>(id - ID_RULE_REMOVE);
        if (i < catalog.games.size()) { auto next = catalog; next.games.erase(next.games.begin() + static_cast<std::ptrdiff_t>(i)); svc::Store(std::move(next)); }
    } else if (id >= ID_RULE_TITLE) {
        const std::size_t i = static_cast<std::size_t>(id - ID_RULE_TITLE);
        for (const auto& row : st->rows)
            if (row.index == i && i < catalog.games.size()) {
                RECT edit = row.title; edit.left = row.text.left + (row.text.right - row.text.left) / 3;
                BeginEdit(h, st, Edit::Title, edit, catalog.games[i].title, i);
            }
        return;
    } else if (id >= ID_CARD) {
        const std::size_t i = static_cast<std::size_t>(id - ID_CARD);
        if (i < st->names.size()) st->selected = st->names[i];
    } else switch (id) {
    case ID_NEW: NewProfile(h, st); return;
    case ID_IMPORT: ImportProfile(h, st); break;
    case ID_ACTIVATE: if (!svc::UserActivate(st->selected)) ShowError(h, svc::session.error); break;
    case ID_DUPLICATE: {
        const auto name = prof::UniqueName(st->selected + L" copy");
        if (name.empty() || !prof::Duplicate(st->selected, name)) { ShowError(h, L"Could not duplicate this profile."); break; }
        st->selected = name; Refresh(h, st);
        BeginEdit(h, st, Edit::Rename, st->name, name);
        return;
    }
    case ID_RENAME: BeginEdit(h, st, Edit::Rename, st->name, st->selected); return;
    case ID_EXPORT: ExportProfile(h, st); break;
    case ID_DELETE:
        if (MessageBoxW(h, (L"Delete profile \"" + st->selected + L"\" and its game assignments? A recovery copy is kept.").c_str(),
                L"Profiles", MB_YESNO | MB_ICONQUESTION) == IDYES && !svc::Remove(st->selected)) ShowError(h, svc::session.error);
        break;
    case ID_ADD_RUNNING: AddRunningApp(h, st, st->addRunning); break;
    case ID_ADD_BROWSE: BrowseExe(h, st); break;
    case ID_SHORTCUT: StartCapture(h, st, Capture::Profile); return;
    case ID_NEXT_SC: StartCapture(h, st, Capture::Next); return;
    case ID_AUTO_SC: StartCapture(h, st, Capture::Auto); return;
    case ID_SHORTCUT_CLEAR: if (st->capture == Capture::Profile) StopCapture(h, st); else SetShortcut(h, st, Capture::Profile, 0); break;
    case ID_NEXT_SC_CLEAR: if (st->capture == Capture::Next) StopCapture(h, st); else SetShortcut(h, st, Capture::Next, 0); break;
    case ID_AUTO_SC_CLEAR: if (st->capture == Capture::Auto) StopCapture(h, st); else SetShortcut(h, st, Capture::Auto, 0); break;
    case ID_AUTOMATIC: { auto next = catalog; next.automatic = !next.automatic; if (!svc::Store(std::move(next))) ShowError(h, svc::session.error); break; }
    case ID_NOTIFY: { auto next = catalog; next.notify = !next.notify; if (!svc::Store(std::move(next))) ShowError(h, svc::session.error); break; }
    case ID_FOCUS_COMBO:
        if (st->combo) {
            RECT view = CustomPageSurface_ContentToClient(&st->surface, st->focusCombo);
            SetWindowPos(st->combo, HWND_TOP, view.left, view.top, view.right - view.left, view.bottom - view.top, SWP_NOACTIVATE | SWP_SHOWWINDOW);
            SetFocus(st->combo);
            PremiumCombo::ShowDropDown(st->combo, true);
        }
        return;
    case ID_RETURN_AUTO: svc::ReturnToAutomatic(); break;
    case ID_UNDO:
        if (MessageBoxW(h, L"Undo all edits made since this profile was activated?", L"Profiles", MB_YESNO | MB_ICONQUESTION) == IDYES)
            svc::Undo();
        break;
    default: return;
    }
    Refresh(h, st);
}

void SyncCombo(Page* st) {
    if (!st->combo) return;
    PremiumCombo::SetCurSel(st->combo, svc::session.catalog.focusLoss == prof::FocusLoss::FollowFocus ? 1 : 0, false);
    PremiumCombo::SetEnabled(st->combo, !svc::session.readOnly);
}

LRESULT CALLBACK PageProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    Page* st = State(h);
    if (st && m == PremiumCombo::MsgDropStateChanged()) {
        if (!w) { ShowWindow(reinterpret_cast<HWND>(l), SW_HIDE); CustomPageSurface_MarkDirty(h, &st->surface); }
        return 0;
    }
    switch (m) {
    case WM_CREATE: {
        st = new Page();
        SetWindowLongPtrW(h, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(st));
        g_page = h;
        st->combo = PremiumCombo::Create(h, GetModuleHandleW(nullptr), 0, 0, 10, 10, kComboId, WS_CHILD | WS_TABSTOP);
        PremiumCombo::SetFont(st->combo, static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT)), false);
        PremiumCombo::AddString(st->combo, L"Keep its profile while it runs");
        PremiumCombo::AddString(st->combo, L"Switch to Default");
        ShowWindow(st->combo, SW_HIDE);
        SyncCombo(st);
        st->selected = GlobalProfiles_GetActiveName();
        Layout(h, st);
        return 0;
    }
    case WM_ERASEBKGND: return 1;
    case WM_PRINTCLIENT:
    case WM_PAINT:
        if (st) {
            st->surface.scrollY = st->scrollY;
            st->surface.contentHeight = st->contentHeight;
        }
        CustomPageSurface_Paint(h, m, w, st ? &st->surface : nullptr, Render, st, st && st->scroll.draggingThumb);
        return 0;
    case WM_SIZE: if (st) { EndEdit(h, st, false); Layout(h, st); } return 0;
    case WM_SHOWWINDOW:
        if (st && w) { SyncCombo(st); Layout(h, st); }
        if (st && !w) { StopCapture(h, st); EndEdit(h, st, false); }
        break;
    case WM_APP + 30: if (st && st->edit && GetFocus() != st->edit) EndEdit(h, st, true); return 0;
    case WM_LBUTTONDOWN:
    case WM_LBUTTONDBLCLK:
        if (st) {
            if (st->edit) EndEdit(h, st, true);
            CustomPageSurface_SetState(&st->surface, st->scrollY, st->contentHeight);
            if (CustomPageSurface_HandleScrollMessage(h, &st->surface, &st->scroll, WM_LBUTTONDOWN, w, l, S(h, 54)) != CustomPageScrollResult::NotHandled) {
                st->scrollY = st->surface.scrollY; return 0;
            }
            POINT pt{ GET_X_LPARAM(l), GET_Y_LPARAM(l) };
            const int id = HitTest(st, pt);
            if (m == WM_LBUTTONDBLCLK && id >= ID_CARD && id < ID_RULE_TITLE) {
                const std::size_t i = static_cast<std::size_t>(id - ID_CARD);
                if (i < st->names.size() && !svc::UserActivate(st->names[i])) ShowError(h, svc::session.error);
                Refresh(h, st);
                return 0;
            }
            st->pressedId = id > 0 ? id : 0;
            if (st->pressedId) { SetCapture(h); CustomPageSurface_MarkDirty(h, &st->surface); }
            return 0;
        }
        break;
    case WM_MOUSEMOVE:
        if (st) {
            if (CustomPageSurface_HandleScrollMessage(h, &st->surface, &st->scroll, m, w, l, S(h, 54)) != CustomPageScrollResult::NotHandled) {
                st->scrollY = st->surface.scrollY; return 0;
            }
            POINT pt{ GET_X_LPARAM(l), GET_Y_LPARAM(l) };
            const int hot = std::max(0, HitTest(st, pt));
            if (hot != st->hotId) {
                st->hotId = hot;
                CustomPageSurface_MarkDirty(h, &st->surface);
                TRACKMOUSEEVENT tme{ sizeof(tme), TME_LEAVE, h, 0 }; TrackMouseEvent(&tme);
            }
            return 0;
        }
        break;
    case WM_MOUSELEAVE: if (st && st->hotId) { st->hotId = 0; CustomPageSurface_MarkDirty(h, &st->surface); } return 0;
    case WM_LBUTTONUP:
        if (st) {
            const bool wasScroll = st->scroll.draggingThumb;
            CustomPageSurface_HandleScrollMessage(h, &st->surface, &st->scroll, m, w, l, S(h, 54));
            POINT pt{ GET_X_LPARAM(l), GET_Y_LPARAM(l) };
            const int pressed = st->pressedId;
            st->pressedId = 0;
            if (GetCapture() == h) ReleaseCapture();
            if (!wasScroll && pressed && HitTest(st, pt) == pressed) Command(h, st, pressed);
            CustomPageSurface_MarkDirty(h, &st->surface);
            return 0;
        }
        break;
    case WM_CAPTURECHANGED:
    case WM_CANCELMODE:
        if (st) {
            CustomPageSurface_HandleScrollMessage(h, &st->surface, &st->scroll, m, w, l, S(h, 54));
            st->pressedId = 0; CustomPageSurface_MarkDirty(h, &st->surface);
        }
        return 0;
    case WM_MOUSEWHEEL:
        if (st) {
            if (st->edit) EndEdit(h, st, true);
            if (st->combo) { PremiumCombo::ShowDropDown(st->combo, false); ShowWindow(st->combo, SW_HIDE); }
            CustomPageSurface_HandleScrollMessage(h, &st->surface, &st->scroll, m, w, l, S(h, 54));
            st->scrollY = st->surface.scrollY;
            return 0;
        }
        break;
    case WM_COMMAND:
        if (st && LOWORD(w) == kComboId && HIWORD(w) == CBN_SELCHANGE) {
            auto next = svc::session.catalog;
            next.focusLoss = PremiumCombo::GetCurSel(st->combo) == 1 ? prof::FocusLoss::FollowFocus : prof::FocusLoss::KeepWhileRunning;
            if (next.focusLoss != svc::session.catalog.focusLoss && !svc::Store(std::move(next))) ShowError(h, svc::session.error);
            SyncCombo(st);
            Refresh(h, st);
            return 0;
        }
        break;
    case WM_CTLCOLOREDIT:
        SetTextColor(reinterpret_cast<HDC>(w), UiTheme::Color_Text());
        SetBkColor(reinterpret_cast<HDC>(w), UiTheme::Color_ControlBg());
        return reinterpret_cast<LRESULT>(UiTheme::Brush_ControlBg());
    case sc::kCaptureMessage:
        if (st) OnCaptured(h, st, static_cast<unsigned>(l));
        return 0;
    case WM_DESTROY:
        if (st) {
            StopCapture(h, st);
            for (auto& [_, icon] : st->icons) if (icon) DestroyIcon(icon);
            CustomPageSurface_Destroy(&st->surface);
            delete st;
            SetWindowLongPtrW(h, GWLP_USERDATA, 0);
        }
        if (g_page == h) g_page = nullptr;
        return 0;
    }
    return DefWindowProcW(h, m, w, l);
}

// --- Header selector -------------------------------------------------------------
// Always visible at the right end of the tab strip, in the width that is left:
//   Full    - profile drop-down + mode button "Auto" / "Manual" / "Auto off"
//   Compact - narrower drop-down + mode button "Auto" / "Manual" / "Off"
//   Icon    - profile button (initial; opens the profile menu) + square mode
//             button (status dot)
// The mode button always works the same: Auto -> automatic switching off,
// Off -> on, Manual -> return to automatic.
constexpr int kSelectorCombo = 7811;
enum class SelectorMode { Full, Compact, Icon };
// hot: 0 none, 1 mode button, 2 profile button (icon mode)
struct Selector { HWND combo = nullptr; RECT chip{}; RECT button{}; int hot = 0; SelectorMode mode = SelectorMode::Full; };

constexpr UINT kMenuReturnAuto = 1, kMenuManage = 2, kMenuAutomatic = 3, kMenuProfileBase = 100;

void ModeClick(HWND h) {
    if (svc::Automatic() && svc::Manual()) { svc::ReturnToAutomatic(); return; }
    auto next = svc::session.catalog;
    next.automatic = !next.automatic;
    if (!svc::Store(std::move(next))) MessageBoxW(h, svc::session.error.c_str(), L"Profiles", MB_OK | MB_ICONWARNING);
}

std::wstring ChipText(bool compact) {
    if (!svc::Automatic()) return compact ? L"Off" : L"Auto off";
    return svc::Manual() ? L"Manual" : L"Auto";
}
COLORREF StatusColor() {
    if (!svc::Automatic()) return UiTheme::Color_TextMuted();
    return svc::Manual() ? RGB(214, 150, 62) : UiTheme::Color_Accent();
}

void SelectorFill(HWND h) {
    auto* s = reinterpret_cast<Selector*>(GetWindowLongPtrW(h, GWLP_USERDATA));
    if (!s || !s->combo) return;
    std::vector<std::wstring> names; GlobalProfiles_List(names);
    PremiumCombo::Clear(s->combo);
    int sel = 0;
    for (std::size_t i = 0; i < names.size(); ++i) {
        PremiumCombo::AddString(s->combo, names[i].c_str());
        if (prof::Same(names[i], GlobalProfiles_GetActiveName())) sel = static_cast<int>(i);
    }
    PremiumCombo::AddString(s->combo, L"Manage profiles...");
    PremiumCombo::SetDropMaxVisible(s->combo, 12);
    PremiumCombo::SetCurSel(s->combo, sel, false);
    InvalidateRect(h, nullptr, FALSE);
}

void SelectorLayout(HWND h, Selector* s) {
    RECT rc{}; GetClientRect(h, &rc);
    const int inset = S(h, 2);
    s->chip = s->button = RECT{};
    if (s->mode == SelectorMode::Icon) {
        s->chip = RECT{ rc.right - rc.bottom, rc.top, rc.right, rc.bottom };
        s->button = RECT{ s->chip.left - S(h, 4) - rc.bottom, rc.top, s->chip.left - S(h, 4), rc.bottom };
        ShowWindow(s->combo, SW_HIDE);
    } else {
        const int chipW = s->mode == SelectorMode::Full ? S(h, 74) : S(h, 56);
        s->chip = RECT{ rc.right - chipW, rc.top + inset, rc.right, rc.bottom - inset };
        SetWindowPos(s->combo, nullptr, 0, 0, std::max<int>(10, s->chip.left - S(h, 6)), rc.bottom,
            SWP_NOZORDER | SWP_NOACTIVATE | SWP_SHOWWINDOW);
    }
    InvalidateRect(h, nullptr, FALSE);
}

void ShowProfileMenu(HWND h, const RECT& anchorClient) {
    std::vector<std::wstring> names; GlobalProfiles_List(names);
    std::vector<halljoy::ui_menu::Item> items;
    for (std::size_t i = 0; i < names.size(); ++i) {
        halljoy::ui_menu::Item item;
        item.id = kMenuProfileBase + static_cast<UINT>(i);
        item.text = names[i];
        if (prof::Same(names[i], GlobalProfiles_GetActiveName())) item.detail = L"active";
        items.push_back(std::move(item));
    }
    items.push_back(halljoy::ui_menu::Separator());
    halljoy::ui_menu::Item automatic; automatic.id = kMenuAutomatic; automatic.text = L"Automatic switching";
    automatic.detail = svc::Automatic() ? L"on" : L"off"; items.push_back(automatic);
    if (svc::Automatic() && svc::Manual()) {
        halljoy::ui_menu::Item back; back.id = kMenuReturnAuto; back.text = L"Return to automatic"; items.push_back(back);
    }
    halljoy::ui_menu::Item manage; manage.id = kMenuManage; manage.text = L"Manage profiles..."; items.push_back(manage);
    POINT pt{ anchorClient.left, anchorClient.bottom }; ClientToScreen(h, &pt);
    const UINT id = halljoy::ui_menu::Track(h, items, pt);
    if (id == kMenuReturnAuto) svc::ReturnToAutomatic();
    else if (id == kMenuAutomatic) {
        auto next = svc::session.catalog;
        next.automatic = !next.automatic;
        if (!svc::Store(std::move(next))) MessageBoxW(h, svc::session.error.c_str(), L"Profiles", MB_OK | MB_ICONWARNING);
    }
    else if (id == kMenuManage) PostMessageW(GetParent(h), WM_APP_PROFILES_OPEN_TAB, 0, 0);
    else if (id >= kMenuProfileBase && id - kMenuProfileBase < names.size()) {
        if (!svc::UserActivate(names[id - kMenuProfileBase]))
            MessageBoxW(h, svc::session.error.c_str(), L"Profiles", MB_OK | MB_ICONWARNING);
    }
}

void DrawDot(Graphics& g, const RECT& area, int diameter, COLORREF color) {
    SolidBrush brush(Color(255, GetRValue(color), GetGValue(color), GetBValue(color)));
    g.FillEllipse(&brush, (area.left + area.right - diameter) / 2, (area.top + area.bottom - diameter) / 2, diameter, diameter);
}

LRESULT CALLBACK SelectorProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    auto* s = reinterpret_cast<Selector*>(GetWindowLongPtrW(h, GWLP_USERDATA));
    switch (m) {
    case WM_CREATE:
        s = new Selector();
        SetWindowLongPtrW(h, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(s));
        s->combo = PremiumCombo::Create(h, GetModuleHandleW(nullptr), 0, 0, 10, 10, kSelectorCombo, WS_CHILD | WS_VISIBLE | WS_TABSTOP);
        PremiumCombo::SetFont(s->combo, static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT)), false);
        SelectorFill(h);
        return 0;
    case WM_SIZE:
        if (s) SelectorLayout(h, s);
        return 0;
    case WM_ERASEBKGND: return 1;
    case WM_PRINTCLIENT:
    case WM_PAINT: {
        CustomPagePaintScope paint(h, m, w);
        HDC mem = paint.Dc();
        RECT rc{}; GetClientRect(h, &rc);
        // The tab strip behind the selector is painted with the panel colour.
        FillRect(mem, &rc, UiTheme::Brush_PanelBg());
        if (s) {
            HGDIOBJ oldFont = SelectObject(mem, GetStockObject(DEFAULT_GUI_FONT));
            Graphics g(mem); g.SetSmoothingMode(SmoothingModeAntiAlias);
            const bool chipHot = s->hot == 1, buttonHot = s->hot == 2;
            const COLORREF chipFill = chipHot ? RGB(46, 46, 50) : UiTheme::Color_ControlBg();
            const COLORREF chipBorder = svc::Automatic() ? StatusColor() : UiTheme::Color_Border();
            if (s->mode == SelectorMode::Icon) {
                CustomPage_DrawRoundRect(g, s->chip, chipFill, chipBorder, (float)S(h, 4));
                DrawDot(g, s->chip, S(h, 8), StatusColor());
                CustomPage_DrawRoundRect(g, s->button, buttonHot ? RGB(46, 46, 50) : UiTheme::Color_ControlBg(), UiTheme::Color_Border(), (float)S(h, 4));
                const auto name = GlobalProfiles_GetActiveName();
                const std::wstring letter(1, name.empty() ? L'?' : static_cast<wchar_t>(towupper(name[0])));
                CustomPage_DrawText(mem, letter, s->button, UiTheme::Color_Text(), DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            } else {
                CustomPage_DrawRoundRect(g, s->chip, chipFill, chipBorder, (float)S(h, 10));
                CustomPage_DrawText(mem, ChipText(s->mode == SelectorMode::Compact), s->chip,
                    svc::Automatic() ? UiTheme::Color_Text() : UiTheme::Color_TextMuted(), DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            }
            SelectObject(mem, oldFont);
        }
        return 0;
    }
    case WM_MOUSEMOVE:
        if (s) {
            POINT pt{ GET_X_LPARAM(l), GET_Y_LPARAM(l) };
            const int hot = PtInRect(&s->chip, pt) ? 1 : PtInRect(&s->button, pt) ? 2 : 0;
            if (hot != s->hot) {
                s->hot = hot; InvalidateRect(h, nullptr, FALSE);
                TRACKMOUSEEVENT tme{ sizeof(tme), TME_LEAVE, h, 0 }; TrackMouseEvent(&tme);
            }
        }
        return 0;
    case WM_MOUSELEAVE: if (s && s->hot) { s->hot = 0; InvalidateRect(h, nullptr, FALSE); } return 0;
    case WM_LBUTTONUP:
        if (s) {
            POINT pt{ GET_X_LPARAM(l), GET_Y_LPARAM(l) };
            if (PtInRect(&s->button, pt)) ShowProfileMenu(h, s->button);
            else if (PtInRect(&s->chip, pt)) ModeClick(h);
        }
        return 0;
    case WM_COMMAND:
        if (s && LOWORD(w) == kSelectorCombo && HIWORD(w) == CBN_SELCHANGE) {
            const int sel = PremiumCombo::GetCurSel(s->combo), count = PremiumCombo::GetCount(s->combo);
            if (sel == count - 1) { SelectorFill(h); PostMessageW(GetParent(h), WM_APP_PROFILES_OPEN_TAB, 0, 0); return 0; }
            wchar_t name[260]{};
            PremiumCombo::GetLBText(s->combo, sel, name, 260);
            if (!svc::UserActivate(name)) MessageBoxW(h, svc::session.error.c_str(), L"Profiles", MB_OK | MB_ICONWARNING);
            SelectorFill(h);
            return 0;
        }
        break;
    case WM_DESTROY:
        delete s;
        SetWindowLongPtrW(h, GWLP_USERDATA, 0);
        if (g_selector == h) g_selector = nullptr;
        return 0;
    }
    return DefWindowProcW(h, m, w, l);
}

} // namespace

HWND ProfilesPage_Create(HWND parent, HINSTANCE instance) {
    WNDCLASSW wc{};
    wc.style = CS_DBLCLKS;
    wc.lpfnWndProc = PageProc; wc.hInstance = instance;
    wc.lpszClassName = L"HallJoyProfilesPage"; wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    RegisterClassW(&wc);
    return CreateWindowW(wc.lpszClassName, L"", WS_CHILD | WS_CLIPCHILDREN, 0, 0, 100, 100, parent, nullptr, instance, nullptr);
}

HWND ProfilesPage_Window() { return g_page; }

HWND ProfileSelector_Create(HWND parent, HINSTANCE instance) {
    WNDCLASSW wc{};
    wc.lpfnWndProc = SelectorProc; wc.hInstance = instance;
    wc.lpszClassName = L"HallJoyProfileSelector"; wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    RegisterClassW(&wc);
    g_selector = CreateWindowW(wc.lpszClassName, L"", WS_CHILD | WS_CLIPSIBLINGS | WS_CLIPCHILDREN, 0, 0, 10, 10,
        parent, nullptr, instance, nullptr);
    return g_selector;
}

void ProfileSelector_Place(HWND selector, HWND tab) {
    if (!selector || !tab) return;
    auto* s = reinterpret_cast<Selector*>(GetWindowLongPtrW(selector, GWLP_USERDATA));
    const int count = TabCtrl_GetItemCount(tab);
    RECT last{}, tabRc{};
    if (count <= 0 || !TabCtrl_GetItemRect(tab, count - 1, &last)) { ShowWindow(selector, SW_HIDE); return; }
    GetClientRect(tab, &tabRc);
    const int height = std::max<int>(WinUtil_ScalePx(tab, 22), last.bottom - last.top - WinUtil_ScalePx(tab, 2));
    const int room = static_cast<int>(tabRc.right - last.right) - WinUtil_ScalePx(tab, 10);
    // Never hidden: shrink to the room that is left, down to one square button.
    SelectorMode mode = SelectorMode::Full;
    int width = std::min(WinUtil_ScalePx(tab, 300), room);
    if (width < WinUtil_ScalePx(tab, 200)) { mode = SelectorMode::Compact; width = std::min(WinUtil_ScalePx(tab, 180), room); }
    if (width < WinUtil_ScalePx(tab, 130)) { mode = SelectorMode::Icon; width = height * 2 + WinUtil_ScalePx(tab, 4); }
    if (s) s->mode = mode;
    POINT origin{ tabRc.right - width - WinUtil_ScalePx(tab, 2), last.top + (last.bottom - last.top - height) / 2 };
    MapWindowPoints(tab, GetParent(selector), &origin, 1);
    SetWindowPos(selector, HWND_TOP, origin.x, origin.y, width, height, SWP_NOACTIVATE | SWP_SHOWWINDOW);
    if (s) SelectorLayout(selector, s); // the mode may change without a size change
}

void ProfilesUi_Refresh() {
    if (g_selector) SelectorFill(g_selector);
    if (g_page) {
        if (Page* st = State(g_page)) {
            if (IsWindowVisible(g_page)) { SyncCombo(st); Layout(g_page, st); InvalidateRect(g_page, nullptr, FALSE); }
            else st->visualPending = true;
        }
    }
}

#if defined(HALLJOY_ANALOG_SIMULATOR)
#include "test_thread_desktop.h"
bool ProfilesPage_Test() {
    return halljoy::test_desktop::RunOnPrivateDesktop(L"HallJoyProfilesTest", []() -> bool {
        HWND root = CreateWindowW(L"STATIC", L"Profiles test", WS_POPUP, 0, 0, 900, 640, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
        HWND view = ProfilesPage_Create(root, GetModuleHandleW(nullptr));
        bool ok = view != nullptr && ProfilesPage_Window() == view;
        if (view) {
            Page* st = State(view);
            const auto before = GlobalProfiles_GetActiveName();
            SetWindowPos(view, nullptr, 0, 0, 760, 300, SWP_NOZORDER | SWP_SHOWWINDOW);
            ok &= st && !st->cards.empty() && st->contentHeight > 300;
            // Selecting a card browses only; it never activates.
            if (st && st->names.size() > 1) {
                const std::size_t other = prof::Same(st->names[0], before) ? 1 : 0;
                Command(view, st, ID_CARD + static_cast<int>(other));
                ok &= GlobalProfiles_GetActiveName() == before && prof::Same(st->selected, st->names[other]);
            }
            // Narrow windows stack the columns instead of overlapping them.
            SetWindowPos(view, nullptr, 0, 0, 420, 300, SWP_NOZORDER);
            ok &= st && st->name.top > st->newBtn.bottom;
            SendMessageW(view, WM_MOUSEWHEEL, MAKEWPARAM(0, static_cast<WORD>(-WHEEL_DELTA * 3)), 0);
            ok &= st && st->scrollY >= 0;
            DestroyWindow(view);
            ok &= ProfilesPage_Window() == nullptr;
        }
        HWND tab = CreateWindowW(WC_TABCONTROLW, L"", WS_CHILD | WS_VISIBLE, 0, 0, 900, 200, root, nullptr, GetModuleHandleW(nullptr), nullptr);
        TCITEMW item{ TCIF_TEXT }; item.pszText = const_cast<LPWSTR>(L"Remap"); TabCtrl_InsertItem(tab, 0, &item);
        HWND selector = ProfileSelector_Create(root, GetModuleHandleW(nullptr));
        ProfileSelector_Place(selector, tab);
        auto shown = [](HWND w) { return (GetWindowLongW(w, GWL_STYLE) & WS_VISIBLE) != 0; }; // root stays hidden
        auto* sel = selector ? reinterpret_cast<Selector*>(GetWindowLongPtrW(selector, GWLP_USERDATA)) : nullptr;
        ok &= selector && shown(selector) && sel && sel->mode == SelectorMode::Full;
        // Less room: never hidden, never over the tabs, just more compact.
        RECT last{}; TabCtrl_GetItemRect(tab, 0, &last);
        auto fits = [&]() { RECT r{}; GetWindowRect(selector, &r); MapWindowPoints(nullptr, root, reinterpret_cast<POINT*>(&r), 2);
            POINT edge{ last.right, 0 }; MapWindowPoints(tab, root, &edge, 1); return r.left >= edge.x; };
        SetWindowPos(tab, nullptr, 0, 0, last.right + 190, 200, SWP_NOZORDER);
        ProfileSelector_Place(selector, tab);
        ok &= shown(selector) && sel && sel->mode == SelectorMode::Compact && fits();
        SetWindowPos(tab, nullptr, 0, 0, last.right + 80, 200, SWP_NOZORDER);
        ProfileSelector_Place(selector, tab);
        RECT icon{}; GetClientRect(selector, &icon);
        ok &= shown(selector) && sel && sel->mode == SelectorMode::Icon && fits() &&
            sel->chip.right > sel->chip.left && sel->button.right > sel->button.left && sel->button.right <= sel->chip.left;
        DestroyWindow(root);
        return ok;
    });
}
#endif

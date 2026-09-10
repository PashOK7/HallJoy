// Runs only inside the simulator with an explicitly isolated data root.
#include "../HallJoy/app_paths.h"
#include "../HallJoy/global_profiles.h"
#include "../HallJoy/settings_ini.h"
#include "../HallJoy/settings.h"
#include "../HallJoy/profile_ini.h"
#include "../HallJoy/key_settings.h"
#include "../HallJoy/keyboard_layout.h"
#include "../HallJoy/bounded_ini.h"
#include "../HallJoy/ini_util.h"
#include "../HallJoy/profile_runtime_gate.h"
#include "../HallJoy/backend.h"
#include "../HallJoy/keyboard_profiles.h"
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <thread>

namespace fs = std::filesystem;
extern bool KeyboardSubpages_TestOverlayTextEditing();
extern bool KeyboardSubpages_TestLayoutEditor();
extern bool KeyboardSubpages_TestLayoutPicker();
namespace {
void Check(bool value, const char* reason) { if (!value) throw std::runtime_error(reason); }
void Write(const fs::path& path, const std::string& value) {
    std::ofstream f(path, std::ios::binary | std::ios::trunc); f << value; f.close();
    Check(!f.fail(), "fixture write failed");
}
std::string Bytes(const fs::path& path) {
    std::ifstream f(path, std::ios::binary);
    Check(f.good(), "fixture read failed");
    return {std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>()};
}
}
bool HallJoy_RunProfileTransactionTests() {
    if (AppPaths_Mode() != AppDataMode::SimulatorOverride) return false;
    const fs::path root(AppPaths_DataRoot());
    std::ofstream result(root / "profile-test-result.txt");
    try {
        Check(!fs::exists(AppPaths_SettingsIni()) && !fs::exists(AppPaths_BindingsIni()), "test root is not fresh");
        fs::create_directories(AppPaths_LayoutsDir());
        Write(fs::path(AppPaths_LayoutsDir()) / "Oversized.ini", "[LayoutPreset]\nCount=2147483647\n");
        Write(fs::path(AppPaths_LayoutsDir()) / "Valid.ini", "[LayoutPreset]\nCount=1\nK0=7|0|0|42|40|D\n");
        const auto preciseFixture = root / "precise-layout.ini";
        Write(preciseFixture, "[LayoutPreset]\nCount=1\nK0=7|2|0|42|40|D\nY0=115\n");
        int exactY = -1;
        Check(KeyboardLayout_TestReadPresetY(preciseFixture.c_str(), 0, &exactY) && exactY == 115,
            "half-row precision layout did not load");
        Write(preciseFixture, "[LayoutPreset]\nCount=1\nK0=7|2|0|42|40|D\nY0=115junk\n");
        Check(!KeyboardLayout_TestReadPresetY(preciseFixture.c_str(), 0, &exactY), "invalid exact Y accepted");
        Write(preciseFixture, "[LayoutPreset]\nCount=1\nK0=7|2|0|42|40|D\n");
        Check(KeyboardLayout_TestReadPresetY(preciseFixture.c_str(), 0, &exactY) && exactY == 92,
            "legacy row coordinates changed");
        bool validLayout = false;
        for (int i=0; i<KeyboardLayout_GetPresetCount(); ++i) {
            const std::wstring name = KeyboardLayout_GetPresetName(i);
            Check(name != L"Oversized", "oversized layout accepted");
            validLayout |= name == L"Valid";
        }
        Check(validLayout, "valid layout lost alongside invalid file");
        result << "layout_bounds=PASS\n";

        const auto fixture = root / "fixture.ini";
        Bindings_SetAxisPlus(Axis::LX, 26);
        for (const auto& invalid : {std::string(), std::string("unrelated"), std::string("[Pad1_Axes]\nUnrelated=7\n"),
             std::string("[Pad1_Axes]\nLX_Plus=65543\n"),
             std::string("[Pad1_Axes]\nLX_Plus=-1\n"),
             std::string("[Pad1_Axes]\nLX_Plus=7junk\n"),
             std::string("[HallJoyPersistence]\nSchemaVersion=9\nKind=Bindings\n[Pad1_Axes]\nLX_Plus=7\n")}) {
            Write(fixture, invalid);
            Check(!Profile_LoadIni(fixture.c_str()), "invalid profile accepted");
            Check(Bindings_GetAxis(Axis::LX).plusHid == 26, "failed load changed bindings");
        }
        std::string full = "[Pad1_Buttons]\nA=";
        for (unsigned i=1; i<halljoy::keycode::kCount; ++i) full += (i==1 ? "" : ",") + std::to_string(i);
        Write(fixture, full + "\n");
        Check(Profile_LoadIni(fixture.c_str()), "complete CSV rejected");
        for (unsigned i=1; i<halljoy::keycode::kCount; ++i)
            Check(Bindings_ButtonHasHid(GameButton::A, static_cast<uint16_t>(i)), "CSV truncated");
        result << "malformed_no_mutation=PASS csv_full_domain=PASS\n";

        Write(AppPaths_SettingsIni(), "[Main]\nPollingMs=3\nActiveGlobalProfile=Default\n");
        Write(AppPaths_BindingsIni(), "[Pad1_Axes]\nLX_Plus=7\n");
        const auto legacyBindings = Bytes(AppPaths_BindingsIni());
        Check(GlobalProfiles_Load(L"Default"), "legacy pair did not load");
        Check(Settings_GetPollingMs()==3 && Bindings_GetAxis(Axis::LX).plusHid==7, "legacy pair wrong");
        Check(GlobalProfiles_Save(L"Default"), "bundle save failed");
        Check(halljoy::ini::HasBundle(AppPaths_SettingsIni().c_str()), "bundle marker missing");
        Check(Bytes(AppPaths_BindingsIni())==legacyBindings, "legacy bindings changed");
        Check(fs::exists(AppPaths_SettingsIni()+L".pre-bundle.bak"), "legacy settings backup missing");
        Check(GlobalProfiles_Save(L"A"), "A save failed");
        Settings_SetPollingMs(9); Bindings_SetAxisPlus(Axis::LX, 26);
        KeyDeadzone unique{}; unique.useUnique=true; unique.low=0.2f;
        KeySettings_Set(halljoy::keycode::kFn, unique);
        Check(GlobalProfiles_Save(L"B"), "B save failed");
        Check(GlobalProfiles_Load(L"Default"), "bundle reload failed");
        Check(Settings_GetPollingMs()==3 && Bindings_GetAxis(Axis::LX).plusHid==7, "bundle reload mixed");
        Check(GlobalProfiles_Switch(L"A"), "switch A failed");
        Write(GlobalProfiles_GetSettingsPath(L"Missing"), "[Main]\nPollingMs=5\n");
        Check(!GlobalProfiles_Switch(L"Missing"), "incomplete profile switched");
        Write(GlobalProfiles_GetSettingsPath(L"InvalidSettings"), "[Main]\nPollingMs=garbage\n");
        Write(GlobalProfiles_GetBindingsPath(L"InvalidSettings"), "[Pad1_Axes]\nLX_Plus=7\n");
        Check(!GlobalProfiles_Switch(L"InvalidSettings"), "malformed numeric settings switched");
        Check(GlobalProfiles_GetActiveName()==L"A" && Settings_GetPollingMs()==3, "failed switch changed active state");
        Check(!GlobalProfiles_Delete(L"A"), "active profile deleted");
        HANDLE locked = CreateFileW(AppPaths_SettingsIni().c_str(), GENERIC_READ, FILE_SHARE_READ,
            nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        Check(locked!=INVALID_HANDLE_VALUE, "lock fixture failed");
        const bool switchedWhileLocked = GlobalProfiles_Switch(L"B");
        CloseHandle(locked);
        Check(!switchedWhileLocked && GlobalProfiles_GetActiveName()==L"A", "marker failure switched profile");
        Check(!GlobalProfiles_Delete(L"A"), "active profile deleted after failed switch");
        Check(GlobalProfiles_Switch(L"B"), "switch B failed");
        Check(Settings_GetPollingMs()==9 && Bindings_GetAxis(Axis::LX).plusHid==26 &&
            KeySettings_Get(halljoy::keycode::kFn).useUnique, "B not applied completely");
        result << "legacy_migration=PASS switch_failure=PASS delete_guard=PASS\n";

        std::atomic<bool> stop{false}, mixed{false};
        std::atomic<unsigned> reads{0};
        std::thread reader([&] {
            while (!stop.load()) {
                halljoy::profile_runtime::ReadLease lease;
                if (!lease) continue;
                const auto poll = Settings_GetPollingMs();
                const auto key = Bindings_GetAxis(Axis::LX).plusHid;
                if (!((poll==3 && key==7) || (poll==9 && key==26))) mixed = true;
                ++reads;
            }
        });
        bool loadsOk = true;
        for (int i=0; i<100; ++i) loadsOk &= GlobalProfiles_Load(i%2 ? L"B" : L"A");
        stop = true; reader.join();
        Check(loadsOk && !mixed && reads>0, "concurrent consumer observed a partial profile");
        result << "concurrent_profile_loads=100 mixed=0 PASS\n";
        const auto committed = Bytes(GlobalProfiles_GetSettingsPath(L"B"));
        for (const auto stage : {HallJoyPersistence::SaveStage::Prepare, HallJoyPersistence::SaveStage::Write,
             HallJoyPersistence::SaveStage::Flush, HallJoyPersistence::SaveStage::Validate, HallJoyPersistence::SaveStage::Replace}) {
            Settings_SetPollingMs(2); Bindings_SetAxisPlus(Axis::LX, 4);
            IniUtil_TestSetFailureStage(stage);
            const bool saved = GlobalProfiles_Save(L"B");
            IniUtil_TestSetFailureStage(HallJoyPersistence::SaveStage::None);
            Check(!saved && Bytes(GlobalProfiles_GetSettingsPath(L"B"))==committed, "failed save changed bundle");
            Check(GlobalProfiles_Load(L"B"), "reload after fault failed");
            Check(Settings_GetPollingMs()==9 && Bindings_GetAxis(Axis::LX).plusHid==26, "pair mixed after fault");
        }
        for (const auto& entry : fs::recursive_directory_iterator(root))
            Check(entry.path().filename().wstring().find(L".halljoy-new-")==std::wstring::npos, "temporary file survived");
        Check(Backend_FileOnlyTestForbiddenInitAttempts() == 0,
            "file-only profile test reached Backend_Init");
        // Real production serializers + loaders, with values deliberately
        // different from defaults. A successful write alone proves nothing.
        struct FloatField { void (*set)(float); float (*get)(); float value; };
        const FloatField floats[] = {
            {Settings_SetInputDeadzoneLow, Settings_GetInputDeadzoneLow, .15f},
            {Settings_SetInputDeadzoneHigh, Settings_GetInputDeadzoneHigh, .85f},
            {Settings_SetInputAntiDeadzone, Settings_GetInputAntiDeadzone, .12f},
            {Settings_SetInputOutputCap, Settings_GetInputOutputCap, .88f},
            {Settings_SetInputBezierCp1X, Settings_GetInputBezierCp1X, .31f},
            {Settings_SetInputBezierCp1Y, Settings_GetInputBezierCp1Y, .24f},
            {Settings_SetInputBezierCp2X, Settings_GetInputBezierCp2X, .72f},
            {Settings_SetInputBezierCp2Y, Settings_GetInputBezierCp2Y, .79f},
            {Settings_SetInputBezierCp1W, Settings_GetInputBezierCp1W, .43f},
            {Settings_SetInputBezierCp2W, Settings_GetInputBezierCp2W, .67f},
            {Settings_SetLastKeyPrioritySensitivity, Settings_GetLastKeyPrioritySensitivity, .29f},
        };
        for (const auto& field : floats) field.set(field.value);
        Settings_SetSnappyJoystick(true); Settings_SetLastKeyPriority(true);
        Settings_SetBlockBoundKeys(true); Settings_SetInputInvert(true);
        Settings_SetInputCurveMode(0); Settings_SetSparkPollMode(2); Settings_SetSparkRowLimit(7);
        KeyDeadzone perKey{};
        perKey.useUnique = true; perKey.invert = true; perKey.curveMode = 1;
        perKey.low = .17f; perKey.high = .83f; perKey.antiDeadzone = .11f; perKey.outputCap = .89f;
        perKey.cp1_x = .30f; perKey.cp1_y = .27f; perKey.cp2_x = .69f; perKey.cp2_y = .74f;
        perKey.cp1_w = .36f; perKey.cp2_w = .64f;
        KeySettings_Set(4, perKey); KeySettings_Set(halljoy::keycode::kFn, perKey);
        Check(GlobalProfiles_Save(L"ConfigurationRoundtrip"), "configuration write failed");
        Settings_SetDiagnosticLogging(true);
        Check(GlobalProfiles_Load(L"A"), "reset before roundtrip failed");
        Check(Settings_GetDiagnosticLogging(), "gameplay profile changed diagnostic preference");
        const auto loggingSettings = (root / L"logging-settings.ini").wstring();
        Check(SettingsIni_Save(loggingSettings.c_str()), "logging preference save failed");
        Settings_SetDiagnosticLogging(false);
        Check(SettingsIni_Load(loggingSettings.c_str()) && Settings_GetDiagnosticLogging(), "logging preference true did not roundtrip");
        Settings_SetDiagnosticLogging(false);
        Check(SettingsIni_Save(loggingSettings.c_str()), "logging disabled save failed");
        Settings_SetDiagnosticLogging(true);
        Check(SettingsIni_Load(loggingSettings.c_str()) && !Settings_GetDiagnosticLogging(), "logging preference false did not roundtrip");
        Check(WritePrivateProfileStringW(L"Main",L"DiagnosticLogging",nullptr,loggingSettings.c_str()) != 0, "remove optional preference failed");
        Settings_SetDiagnosticLogging(true);
        Check(SettingsIni_Load(loggingSettings.c_str()) && !Settings_GetDiagnosticLogging(), "pre-upgrade settings must load with logging off");
        Check(WritePrivateProfileStringW(L"Main",L"DiagnosticLogging",L"invalid",loggingSettings.c_str()) != 0, "invalid preference fixture failed");
        Check(!SettingsIni_Load(loggingSettings.c_str()), "invalid optional preference accepted");
        const auto blockSettings = (root / L"block-shortcut-settings.ini").wstring();
        const UINT shortcut = ((MOD_CONTROL | MOD_SHIFT) << 8) | VK_F8;
        Settings_SetBlockKeysAllowAltTab(false); Settings_SetBlockKeysHotkey(shortcut);
        Check(GlobalProfiles_Load(L"A"), "block preference profile fixture failed");
        Check(!Settings_GetBlockKeysAllowAltTab() && Settings_GetBlockKeysHotkey() == shortcut,
            "gameplay profile overwrote global block preferences");
        Check(SettingsIni_Save(blockSettings.c_str()), "block preferences save failed");
        Settings_SetBlockKeysAllowAltTab(true); Settings_SetBlockKeysHotkey(0);
        Check(SettingsIni_Load(blockSettings.c_str()) && !Settings_GetBlockKeysAllowAltTab() &&
            Settings_GetBlockKeysHotkey() == shortcut, "block preferences failed roundtrip");
        Check(WritePrivateProfileStringW(L"Main", L"BlockKeysAllowAltTab", nullptr, blockSettings.c_str()) != 0, "remove Alt preference failed");
        Check(WritePrivateProfileStringW(L"Main", L"BlockKeysHotkey", nullptr, blockSettings.c_str()) != 0, "remove shortcut preference failed");
        Check(SettingsIni_Load(blockSettings.c_str()) && Settings_GetBlockKeysAllowAltTab() &&
            !Settings_GetBlockKeysHotkey(), "legacy block defaults failed");
        for (const wchar_t* invalid : {L"16", L"4096", L"invalid"}) {
            Check(WritePrivateProfileStringW(L"Main", L"BlockKeysHotkey", invalid, blockSettings.c_str()) != 0, "invalid shortcut fixture failed");
            Settings_SetBlockKeysHotkey(shortcut);
            Check(!SettingsIni_Load(blockSettings.c_str()) && Settings_GetBlockKeysHotkey() == shortcut,
                "invalid shortcut accepted or changed settings");
        }
        Settings_SetBlockKeysHotkey(0);
        Check(GlobalProfiles_Load(L"ConfigurationRoundtrip"), "configuration read failed");
        for (const auto& field : floats)
            Check(std::fabs(field.get() - field.value) < .0006f, "configuration numeric field lost");
        Check(Settings_GetSnappyJoystick() && Settings_GetLastKeyPriority() && Settings_GetBlockBoundKeys() &&
            Settings_GetInputInvert() && Settings_GetInputCurveMode() == 0 &&
            Settings_GetSparkPollMode() == 2 && Settings_GetSparkRowLimit() == 7, "configuration toggle/mode lost");
        const auto checkCurve = [&](const KeyDeadzone& value) {
            Check(value.useUnique == perKey.useUnique && value.invert == perKey.invert && value.curveMode == perKey.curveMode,
                "curve flags lost");
            for (auto member : {&KeyDeadzone::low, &KeyDeadzone::high, &KeyDeadzone::antiDeadzone,
                &KeyDeadzone::outputCap, &KeyDeadzone::cp1_x, &KeyDeadzone::cp1_y, &KeyDeadzone::cp2_x,
                &KeyDeadzone::cp2_y, &KeyDeadzone::cp1_w, &KeyDeadzone::cp2_w})
                Check(std::fabs(value.*member - perKey.*member) < .0006f, "curve numeric field lost");
        };
        checkCurve(KeySettings_Get(4)); checkCurve(KeySettings_Get(halljoy::keycode::kFn));
        const auto curvePath = fs::path(AppPaths_CurvePresetsDir()) / L"Roundtrip.ini";
        fs::create_directories(curvePath.parent_path());
        Check(KeyboardProfiles::SavePreset(curvePath.wstring(), perKey), "curve preset write failed");
        KeyDeadzone reloadedCurve{};
        Check(KeyboardProfiles::LoadPreset(curvePath.wstring(), reloadedCurve), "curve preset read failed");
        checkCurve(reloadedCurve);
        result << "configuration_nondefault_roundtrip=PASS per_key_and_extended=PASS curve_preset=PASS\n";
        const auto windowFile = root / "window-roundtrip.ini";
        Check(SettingsIni_Save(windowFile.c_str()), "window base fixture failed");
        const auto nonWindow = [&]() {
            std::wstring contents;
            wchar_t sections[32768]{};
            Check(GetPrivateProfileSectionNamesW(sections, 32768, windowFile.c_str()) < 32766, "sections truncated");
            for (const wchar_t* section = sections; *section; section += wcslen(section) + 1) {
                if (_wcsicmp(section, L"Window") == 0) continue;
                wchar_t values[32768]{};
                DWORD size = GetPrivateProfileSectionW(section, values, 32768, windowFile.c_str());
                Check(size < 32766, "section truncated");
                contents.append(section).push_back(L'\0');
                contents.append(values, size);
            }
            return contents;
        };
        const auto unrelated = nonWindow();
        Settings_SetPollingMs(19); // Different active profile values must not leak.
        Settings_SetMainWindowWidthPx(920); Settings_SetMainWindowHeightPx(710);
        Settings_SetMainWindowPosXPx(-1400); Settings_SetMainWindowPosYPx(-750);
        Settings_SetMainWindowPlacementMeta(2, 144, true);
        Check(SettingsIni_SaveWindow(windowFile.c_str()), "window-only save failed");
        Check(nonWindow() == unrelated, "window-only update changed profile/layout/logging sections");
        Settings_SetMainWindowPlacementMeta(0, 0, false);
        Settings_SetMainWindowPosXPx(0);
        Check(SettingsIni_Load(windowFile.c_str()), "window roundtrip load failed");
        Check(Settings_GetMainWindowWidthPx() == 920 && Settings_GetMainWindowHeightPx() == 710 &&
            Settings_GetMainWindowPosXPx() == -1400 && Settings_GetMainWindowPosYPx() == -750 &&
            Settings_GetMainWindowPlacementVersion() == 2 && Settings_GetMainWindowDpi() == 144 &&
            Settings_GetMainWindowMaximized(), "window fields did not roundtrip");
        Check(SettingsIni_LoadProfile(windowFile.c_str()) && Settings_GetMainWindowPosXPx() == -1400 &&
            Settings_GetMainWindowMaximized(), "profile load changed global window geometry");
        const auto windowBeforeFault = Bytes(windowFile);
        Settings_SetMainWindowPosXPx(-1300);
        IniUtil_TestSetFailureStage(HallJoyPersistence::SaveStage::Replace);
        Check(!SettingsIni_SaveWindow(windowFile.c_str()), "window commit fault accepted");
        IniUtil_TestSetFailureStage(HallJoyPersistence::SaveStage::None);
        Check(Bytes(windowFile) == windowBeforeFault, "failed window save damaged base file");
        const auto missingWindow = root / "missing-window.ini";
        Check(!SettingsIni_SaveWindow(missingWindow.c_str()) && !fs::exists(missingWindow), "window-only save created an incomplete base");
        result << "window_roundtrip_and_profile_isolation=PASS window_atomic_failure=PASS\n";
        Check(KeyboardSubpages_TestOverlayTextEditing(), "overlay edit production event test failed");
        result << "overlay_edit_production_events=PASS private_desktop=PASS\n";
        KeyboardLayout_SetPresetIndex(0);
        KeyboardLayout_SetOverlayPresetIndex(-1);
        Check(KeyboardLayout_GetOverlaySnapshot() == KeyboardLayout_GetSnapshot(), "overlay default does not follow main");
        KeyboardLayout_SetPresetIndex(1);
        Check(KeyboardLayout_GetOverlaySnapshot() == KeyboardLayout_GetSnapshot(), "follow did not track layout change");
        int disposable = -1, selected = -1;
        Check(KeyboardLayout_CreatePreset(L"Overlay Test A", &disposable), "test layout A failed");
        Check(KeyboardLayout_CreatePreset(L"Overlay Test B", &selected), "test layout B failed");
        KeyboardLayout_SetPresetIndex(0);
        const auto mainLayout = KeyboardLayout_GetSnapshot();
        KeyboardLayout_SetOverlayPresetIndex(selected);
        const auto originalOverlay = KeyboardLayout_GetOverlaySnapshot();
        Check(originalOverlay != mainLayout, "independent overlay snapshot missing");
        std::vector<KeyDef> layoutKeys; std::vector<std::wstring> layoutLabels;
        bool uniform = false; int uniformGap = 0;
        Check(KeyboardLayout_GetPresetSnapshot(selected, layoutKeys, layoutLabels, &uniform, &uniformGap), "snapshot read failed");
        layoutLabels[0] = L"Overlay-only label";
        layoutKeys[0].x += 3;
        layoutKeys[0].y = KeyboardLayout_KeyY(layoutKeys[0]) + 23;
        Check(KeyboardLayout_StorePresetSnapshot(selected, layoutKeys, layoutLabels, true, true, 11), "overlay layout save failed");
        const auto changedOverlay = KeyboardLayout_GetOverlaySnapshot();
        Check(changedOverlay != originalOverlay && std::wstring(changedOverlay->keys[0].label) == L"Overlay-only label" &&
            std::wstring(originalOverlay->keys[0].label) != L"Overlay-only label" &&
            KeyboardLayout_GetSnapshot() == mainLayout, "overlay edit changed main or invalidated held snapshot");
        const auto overlayFile = root / "overlay-layout-roundtrip.ini";
        KeyboardLayout_SetPresetIndex(selected);
        const auto samePreset = KeyboardLayout_GetSnapshot();
        const auto savedPixelFile = root / "saved-pixel-layout.ini";
        Check(KeyboardLayout_TestSaveActivePresetToPath(savedPixelFile.c_str()) &&
            KeyboardLayout_TestReadPresetY(savedPixelFile.c_str(), 0, &exactY) && exactY == layoutKeys[0].y,
            "exact Y was lost after disk roundtrip");
        Check(samePreset->keys.size() == changedOverlay->keys.size(), "shared render count differs");
        for (size_t i = 0; i < samePreset->keys.size(); ++i)
            Check(samePreset->keys[i].x == changedOverlay->keys[i].x &&
                KeyboardLayout_KeyY(samePreset->keys[i]) == KeyboardLayout_KeyY(changedOverlay->keys[i]) &&
                samePreset->keys[i].w == changedOverlay->keys[i].w &&
                std::wstring(samePreset->keys[i].label) == changedOverlay->keys[i].label,
                "overlay uniform spacing differs from the main renderer");
        KeyboardLayout_SetPresetIndex(0);
        std::atomic<bool> overlayStop{false}, overlayInvalid{false};
        std::atomic<unsigned> overlayReads{0};
        std::thread overlayReader([&] {
            while (!overlayStop.load()) {
                const auto snapshot = KeyboardLayout_GetOverlaySnapshot();
                if (!snapshot || snapshot->keys.size() != snapshot->labels.size()) overlayInvalid = true;
                else for (size_t i = 0; i < snapshot->keys.size(); ++i)
                    if (snapshot->keys[i].label != snapshot->labels[i].c_str()) overlayInvalid = true;
                ++overlayReads;
            }
        });
        while (!overlayReads.load()) std::this_thread::yield();
        for (int i = 0; i < 2000; ++i) KeyboardLayout_SetOverlayPresetIndex(i % 2 ? selected : -1);
        overlayStop = true;
        overlayReader.join();
        Check(!overlayInvalid && overlayReads > 0, "concurrent overlay snapshot ownership failed");
        Check(SettingsIni_Save(overlayFile.c_str()), "overlay selection save failed");
        KeyboardLayout_SetOverlayPresetIndex(-1);
        Check(SettingsIni_Load(overlayFile.c_str()) && KeyboardLayout_GetOverlayPresetIndex() == selected,
            "overlay selection roundtrip failed");
        KeyboardLayout_SetOverlayPresetIndex(-1);
        Check(SettingsIni_LoadProfile(overlayFile.c_str()) && KeyboardLayout_GetOverlayPresetIndex() == -1,
            "profile load changed global overlay selection");
        KeyboardLayout_SetOverlayPresetIndex(selected);
        Check(KeyboardLayout_DeletePreset(disposable) && KeyboardLayout_GetOverlayPresetIndex() == selected - 1 &&
            std::wstring(KeyboardLayout_GetOverlayPresetName()) == L"Overlay Test B", "catalog deletion shifted overlay choice");
        Check(KeyboardLayout_DeletePreset(selected - 1) && KeyboardLayout_GetOverlayPresetIndex() == -1 &&
            KeyboardLayout_GetOverlaySnapshot() == KeyboardLayout_GetSnapshot(), "deleted selection did not fall back to follow");
        KeyboardLayout_SetOverlayPresetName(L"Missing layout");
        Check(KeyboardLayout_GetOverlayPresetIndex() == -1, "missing layout selected an unrelated preset");
        KeyboardLayout_SetPresetIndex(0);
        Check(KeyboardSubpages_TestLayoutEditor(), "layout editor production event test failed");
        Check(KeyboardSubpages_TestLayoutPicker(), "brand/model picker production event test failed");
        result << "layout_brand_model_production_events=PASS metadata_roundtrip=PASS\n";
        result << "overlay_layout_selection_and_persistence=PASS layout_editor_production_events=PASS\n";
        result << "bundle_failure_stages=5 PASS backend_init_attempts=0\nPROFILE_TRANSACTION_WINDOWS_TEST=PASS\n";
        return true;
    } catch (const std::exception& e) {
        IniUtil_TestSetFailureStage(HallJoyPersistence::SaveStage::None);
        result << "PROFILE_TRANSACTION_WINDOWS_TEST=FAIL " << e.what() << "\n";
        return false;
    }
}

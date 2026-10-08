#pragma once
#include "../HallJoy/game_profiles.h"
namespace halljoy::profiles::test {
inline bool Run() {
    const auto original=GlobalProfiles_GetActiveName();
    if(!GlobalProfiles_Save(original))return false;
    std::function<void()> restore;if(!GlobalProfiles_Prepare(original,restore))return false;
    bool ok=true;
    Session s;s.Initialize();
    const auto beforeLow=Settings_GetInputDeadzoneLow();
    ok &= Duplicate(original,L"Game Test Copy");
    ok &= !Duplicate(original,L"game test copy");
    ok &= !Duplicate(original,L"../unsafe");
    ok &= Duplicate(original,L"Game Test Factory",true);
    ok &= GlobalProfiles_GetActiveName()==original && Settings_GetInputDeadzoneLow()==beforeLow;
    ok &= s.Assign({L"game.exe",L"",L"Game Test Copy",L"C:\\Games\\Example\\game.exe"}); // icon path persists
    ok &= !s.Assign({L"GAME.exe",L"",L"Game Test Factory"});            // same target, other profile
    ok &= s.Assign({L"game.exe",L"Editor",L"Game Test Factory"});       // a title makes it a distinct rule
    ok &= s.SetShortcut(L"Game Test Copy",shortcuts::Make(58,shortcuts::kCtrl));
    auto c=s.catalog;c.automatic=true;c.focusLoss=FocusLoss::FollowFocus;c.notify=true;
    c.nextShortcut=shortcuts::Make(59,shortcuts::kCtrl);c.autoShortcut=shortcuts::Make(60,shortcuts::kCtrl);
    ok &= s.Store(c);
    Policy p;
    Focus game;game.exe=L"C:\\Games\\Example\\game.exe";game.title=L"Example";game.pid=10;
    Focus editor=game;editor.title=L"Example Editor";
    Focus desktop;desktop.exe=L"C:\\Windows\\explorer.exe";desktop.pid=11;
    ok &= p.OnFocus(c,game,false)==L"Game Test Copy";
    ok &= p.OnFocus(c,editor,false)==L"Game Test Factory";
    ok &= p.OnFocus(c,desktop,false)==kDefaultProfile;
    Catalog loaded;
    ok &= ReadCatalog(CatalogPath().c_str(),loaded)&&CatalogEqual(loaded,c);
    // Version 1 files are read and upgraded: exact paths become file names.
    {
        const auto v1=CatalogPath()+L".v1test";
        WritePrivateProfileStringW(L"Profiles",L"Version",L"1",v1.c_str());
        WritePrivateProfileStringW(L"Profiles",L"Automatic",L"1",v1.c_str());
        WritePrivateProfileStringW(L"Profiles",L"Count",L"1",v1.c_str());
        WritePrivateProfileStringW(L"Game0",L"Exe",L"C:\\Games\\Old\\old.exe",v1.c_str());
        WritePrivateProfileStringW(L"Game0",L"Profile",L"Game Test Copy",v1.c_str());
        Catalog old;ok &= ReadCatalog(v1.c_str(),old)&&old.automatic&&old.games.size()==1&&old.games[0].exe==L"old.exe"&&
            old.games[0].path==L"C:\\Games\\Old\\old.exe";
        DeleteFileW(v1.c_str());
    }
    IniUtil_TestSetFailureStage(HallJoyPersistence::SaveStage::Replace);
    auto rejected=c;rejected.automatic=false;ok &= !s.Store(rejected);
    IniUtil_TestSetFailureStage(HallJoyPersistence::SaveStage::None);
    ok &= s.catalog.automatic&&ReadCatalog(CatalogPath().c_str(),loaded)&&loaded.automatic;
    // Export and import round trip, for an inactive bundle profile and for Default.
    {
        const auto file=std::filesystem::path(AppPaths_DataRoot())/L"export-test.hjprofile";
        std::wstring error;
        ok &= Export(L"Game Test Copy",s.catalog,file.wstring(),error);
        Imported imported;
        ok &= Import(file.wstring(),imported,error)&&Exists(imported.name)&&!Same(imported.name,L"Game Test Copy");
        ok &= imported.rules.size()==1&&imported.rules[0].exe==L"game.exe"&&Same(imported.rules[0].profile,imported.name);
        ok &= GlobalProfiles_GetActiveName()==original;                  // export/import never activate
        wchar_t probe[16]{};
        GetPrivateProfileStringW(L"Window",L"PosX",L"",probe,16,GlobalProfiles_GetSettingsPath(imported.name).c_str());
        ok &= probe[0]==0;                                               // global sections never travel
        ok &= s.Remove(imported.name);
        ok &= Export(L"Default",s.catalog,file.wstring(),error);
        Imported fromDefault;
        ok &= Import(file.wstring(),fromDefault,error)&&Exists(fromDefault.name)&&fromDefault.rules.empty();
        ok &= s.Remove(fromDefault.name);
        const auto junk=std::filesystem::path(AppPaths_DataRoot())/L"not-a-profile.hjprofile";
        WritePrivateProfileStringW(L"Main",L"Something",L"1",junk.c_str());
        Imported none;ok &= !Import(junk.wstring(),none,error)&&!error.empty();
        DeleteFileW(file.c_str());DeleteFileW(junk.c_str());
    }
    const auto generation=profile_runtime::revision.load();
    ok &= s.Activate(L"Game Test Copy",true)&&s.manual&&profile_runtime::revision.load()>generation;
    Settings_SetInputDeadzoneLow(.37f);
    ok &= GlobalProfiles_Save(GlobalProfiles_GetActiveName()); // Autosave must not destroy the editing checkpoint.
    ok &= s.Undo()&&std::abs(Settings_GetInputDeadzoneLow()-beforeLow)<.001f;
    // A backend compatibility notice never blocks a profile (Default must stay
    // reachable); it is reported after activation.
    s.activationNotice=[](const std::wstring&){return std::wstring(L"test notice");};
    const auto noticeFrom=GlobalProfiles_GetActiveName();
    ok &= s.Activate(L"Game Test Factory",false)&&GlobalProfiles_GetActiveName()==L"Game Test Factory"&&s.error==L"test notice";
    s.activationNotice={};
    ok &= s.Activate(noticeFrom,false)&&GlobalProfiles_GetActiveName()==noticeFrom&&s.error.empty();
    const auto active=GlobalProfiles_GetActiveName();
    ok &= !s.Activate(L"Missing game profile",false)&&GlobalProfiles_GetActiveName()==active;
    ok &= !s.Remove(active);
    ok &= s.Rename(L"Game Test Copy",L"Game Test Renamed");
    ok &= GlobalProfiles_GetActiveName()==L"Game Test Renamed"&&s.catalog.games[0].profile==L"Game Test Renamed";
    ok &= ShortcutFor(s.catalog,L"Game Test Renamed")!=0&&ShortcutFor(s.catalog,L"Game Test Copy")==0;
    ok &= !Exists(L"Game Test Copy");
    ok &= s.Activate(L"Game Test Factory",false);
    ok &= std::abs(Settings_GetInputDeadzoneLow()-.08f)<.001f && Bindings_GetAxis(Axis::LX).plusHid==0;
    ok &= s.Remove(L"Game Test Renamed")&&!Exists(L"Game Test Renamed")&&s.catalog.games.size()==1&&s.catalog.shortcuts.empty();
    ok &= !s.Remove(L"Default")&&!s.Rename(L"Default",L"Bad rename");
    ok &= s.Activate(original,false);
    {profile_runtime::CommitLease lease;if(lease)restore();else ok=false;}
    ok &= s.Remove(L"Game Test Factory")&&s.catalog.games.empty();
    ok &= s.Store({});
    const auto path=CatalogPath();
    ok &= WritePrivateProfileStringW(L"Profiles",L"Version",L"999",path.c_str())!=FALSE;
    Session damaged;damaged.Initialize();
    ok &= damaged.readOnly&&!damaged.error.empty()&&!damaged.Store({});
    wchar_t version[16]{};GetPrivateProfileStringW(L"Profiles",L"Version",L"",version,16,path.c_str());
    ok &= wcscmp(version,L"999")==0; // Corrupt/unknown versions are preserved, never silently replaced.
    ok &= SaveCatalog({});
    return ok;
}
}

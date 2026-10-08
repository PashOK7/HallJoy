#pragma once
#include <windows.h>
#include <filesystem>
#include <algorithm>
#include <functional>
#include <string>
#include <vector>
#include "game_profile_rules.h"
#include "global_profiles.h"
#include "app_paths.h"
#include "file_name_policy.h"
#include "ini_util.h"
#include "ini_write_batch.h"
#include "bounded_ini.h"
#include "settings_ini.h"
#include "profile_ini.h"
#include "profile_runtime_gate.h"
#include "input_shortcuts.h"

// UI-thread owner of game profile storage. Gameplay never reads files or
// enumerates processes. Rules and switching policy: game_profile_rules.h.
namespace halljoy::profiles {

inline std::wstring CatalogPath() { return (std::filesystem::path(AppPaths_DataRoot())/L"GameProfiles.ini").wstring(); }

namespace detail {
inline bool ReadFlag(const wchar_t* path, const wchar_t* key, bool& out) {
    std::wstring text;uint32_t v=0;
    if(!ini::Read(path,L"Profiles",key,text))return false;
    if(text.empty()){out=false;return true;}
    if(!ini::Unsigned(text,1,v))return false;
    out=v!=0;return true;
}
inline bool ReadShortcut(const wchar_t* path, const wchar_t* section, const wchar_t* key, unsigned& out) {
    uint32_t v=0;
    if(!ini::ReadUnsigned(path,section,key,0xFFF,0,v)||!shortcuts::Valid(v))return false;
    out=v;return true;
}
}

// Version 1 (exact paths, no titles/shortcuts) is read and upgraded on save.
// Unknown versions and damaged files are rejected and preserved by Session.
inline bool ReadCatalog(const wchar_t* path,Catalog& result) {
    ini::ReadFile file(path); if(!file)return false;
    std::wstring version,count;
    if(!ini::Read(path,L"Profiles",L"Version",version)||(version!=L"1"&&version!=L"2")||
       !ini::Read(path,L"Profiles",L"Count",count))return false;
    const bool v1=version==L"1";
    uint32_t n=0; if(!ini::Unsigned(count,kMaxRules,n))return false;
    Catalog next;
    if(!detail::ReadFlag(path,L"Automatic",next.automatic))return false;
    if(!v1) {
        uint32_t focus=0,shortcutCount=0;bool notify=false;
        if(!ini::ReadUnsigned(path,L"Profiles",L"FocusLoss",1,0,focus)||!detail::ReadFlag(path,L"Notify",notify)||
           !detail::ReadShortcut(path,L"Profiles",L"NextShortcut",next.nextShortcut)||
           !detail::ReadShortcut(path,L"Profiles",L"AutoShortcut",next.autoShortcut)||
           !ini::ReadUnsigned(path,L"Profiles",L"ShortcutCount",kMaxProfileShortcuts,0,shortcutCount))return false;
        next.focusLoss=static_cast<FocusLoss>(focus);next.notify=notify;
        for(uint32_t i=0;i<shortcutCount;++i) {
            ProfileShortcut s;const auto section=L"Shortcut"+std::to_wstring(i);
            if(!ini::Read(path,section.c_str(),L"Profile",s.profile,256)||s.profile.empty()||
               GlobalProfiles_SanitizeName(s.profile)!=s.profile||
               !detail::ReadShortcut(path,section.c_str(),L"Shortcut",s.shortcut)||!s.shortcut)return false;
            for(const auto& old:next.shortcuts)if(Same(old.profile,s.profile)||old.shortcut==s.shortcut)return false;
            next.shortcuts.push_back(std::move(s));
        }
    }
    for(uint32_t i=0;i<n;++i) {
        Rule r;const auto section=L"Game"+std::to_wstring(i);
        if(!ini::Read(path,section.c_str(),L"Exe",r.exe,32768)||
           (!v1&&!ini::Read(path,section.c_str(),L"Title",r.title,kMaxTitleChars+2))||
           (!v1&&!ini::Read(path,section.c_str(),L"Path",r.path,32768))||
           !ini::Read(path,section.c_str(),L"Profile",r.profile,256)||
           GlobalProfiles_SanitizeName(r.profile)!=r.profile||!ValidRule(r))return false;
        for(const auto& old:next.games)if(SameTarget(old,r))return false;
        next.games.push_back(std::move(r));
    }
    if(v1)UpgradeV1Paths(next);
    result=std::move(next);return true;
}
inline bool CatalogEqual(const Catalog& a,const Catalog& b) {
    if(a.automatic!=b.automatic||a.focusLoss!=b.focusLoss||a.notify!=b.notify||a.nextShortcut!=b.nextShortcut||
       a.autoShortcut!=b.autoShortcut||a.games.size()!=b.games.size()||a.shortcuts.size()!=b.shortcuts.size())return false;
    for(size_t i=0;i<a.games.size();++i)
        if(a.games[i].exe!=b.games[i].exe||a.games[i].title!=b.games[i].title||a.games[i].profile!=b.games[i].profile||
           a.games[i].path!=b.games[i].path)return false;
    for(size_t i=0;i<a.shortcuts.size();++i)
        if(a.shortcuts[i].profile!=b.shortcuts[i].profile||a.shortcuts[i].shortcut!=b.shortcuts[i].shortcut)return false;
    return true;
}
inline bool SaveCatalog(const Catalog& catalog) {
    if(IniUtil_IsSessionReadOnly()||catalog.games.size()>kMaxRules||catalog.shortcuts.size()>kMaxProfileShortcuts)return false;
    const auto writer=[](const wchar_t* p,void* raw,DWORD* error) {
        const auto& c=*static_cast<const Catalog*>(raw);ini::WriteBatch b(p);
        auto put=[&](const std::wstring& s,const wchar_t* k,const std::wstring& v){return b.Put(s.c_str(),k,v.c_str(),p);};
        bool ok=put(L"Profiles",L"Version",L"2")&&put(L"Profiles",L"Automatic",c.automatic?L"1":L"0")&&
            put(L"Profiles",L"FocusLoss",std::to_wstring(static_cast<unsigned>(c.focusLoss)))&&
            put(L"Profiles",L"Notify",c.notify?L"1":L"0")&&
            put(L"Profiles",L"NextShortcut",std::to_wstring(c.nextShortcut))&&
            put(L"Profiles",L"AutoShortcut",std::to_wstring(c.autoShortcut))&&
            put(L"Profiles",L"Count",std::to_wstring(c.games.size()))&&
            put(L"Profiles",L"ShortcutCount",std::to_wstring(c.shortcuts.size()));
        for(size_t i=0;i<c.games.size();++i) {
            const auto s=L"Game"+std::to_wstring(i);
            ok=ok&&put(s,L"Exe",c.games[i].exe)&&put(s,L"Title",c.games[i].title)&&put(s,L"Profile",c.games[i].profile)&&
                put(s,L"Path",c.games[i].path);
        }
        for(size_t i=0;i<c.shortcuts.size();++i) {
            const auto s=L"Shortcut"+std::to_wstring(i);
            ok=ok&&put(s,L"Profile",c.shortcuts[i].profile)&&put(s,L"Shortcut",std::to_wstring(c.shortcuts[i].shortcut));
        }
        return ok&&b.Finish(error);
    };
    const auto validate=[](const wchar_t* p,void* raw,DWORD*) {
        Catalog c;return ReadCatalog(p,c)&&CatalogEqual(c,*static_cast<const Catalog*>(raw));
    };
    return IniUtil_SaveAtomic(CatalogPath().c_str(),writer,validate,const_cast<Catalog*>(&catalog)).Succeeded();
}
inline bool Exists(const std::wstring& name) {
    std::vector<std::wstring> names;GlobalProfiles_List(names);
    for(const auto& n:names)if(FileNamePolicy_Equivalent(n,name))return true;
    return false;
}
inline bool NewName(const std::wstring& name) {
    return !name.empty()&&name==GlobalProfiles_SanitizeName(name)&&!Exists(name)&&
        GetFileAttributesW(GlobalProfiles_GetBindingsPath(name).c_str())==INVALID_FILE_ATTRIBUTES;
}
inline std::wstring UniqueName(const std::wstring& stem) {
    const auto clean=GlobalProfiles_SanitizeName(stem);
    const auto base=clean.empty()?std::wstring(L"Profile"):clean;
    if(NewName(base))return base;
    for(int i=2;i<10000;++i){auto n=base+L" "+std::to_wstring(i);if(NewName(n))return n;}
    return {};
}
// Copy without ever applying the browsed profile. Legacy pairs remain readable.
inline bool Duplicate(const std::wstring& source,const std::wstring& name,bool factory=false) {
    if(IniUtil_IsSessionReadOnly()||!NewName(name))return false;
    if(!factory && Same(source,GlobalProfiles_GetActiveName())&&!GlobalProfiles_Save(source))return false;
    if(!factory) {std::function<void()> proof;if(!GlobalProfiles_Prepare(source,proof))return false;}
    const auto settings=GlobalProfiles_GetSettingsPath(name);
    const auto bindings=GlobalProfiles_GetBindingsPath(name);
    const auto stage=settings+L".new";
    if(GetFileAttributesW(stage.c_str())!=INVALID_FILE_ATTRIBUTES)return false;
    bool copiedBindings=false,ok=false;
    if(factory) {
        const char seed[]="[Input]\r\nDeadzoneLow=80\r\n[Pad1_Axes]\r\nLX_Plus=0\r\n";
        HANDLE f=CreateFileW(stage.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
        if(f==INVALID_HANDLE_VALUE)return false;
        DWORD written=0;ok=WriteFile(f,seed,sizeof(seed)-1,&written,nullptr)&&written==sizeof(seed)-1&&FlushFileBuffers(f);CloseHandle(f);
        if(ok) {ok=CopyFileW(stage.c_str(),bindings.c_str(),TRUE)!=FALSE;copiedBindings=ok;}
    } else {
        ok=CopyFileW(GlobalProfiles_GetSettingsPath(source).c_str(),stage.c_str(),TRUE)!=FALSE;
        if(ok&&!ini::HasBundle(stage.c_str())) {ok=CopyFileW(GlobalProfiles_GetBindingsPath(source).c_str(),bindings.c_str(),TRUE)!=FALSE;copiedBindings=ok;}
    }
    std::function<void()> prepared;BindingsSnapshot bs;
    ok=ok&&SettingsIni_PrepareProfile(stage.c_str(),prepared)&&Profile_PrepareIni((ini::HasBundle(stage.c_str())?stage:bindings).c_str(),bs);
    if(ok)ok=MoveFileExW(stage.c_str(),settings.c_str(),MOVEFILE_WRITE_THROUGH)!=FALSE;
    if(!ok) {DeleteFileW(stage.c_str());if(copiedBindings)DeleteFileW(bindings.c_str());}
    return ok;
}
inline bool Archive(const std::wstring& name) {
    if(IniUtil_IsSessionReadOnly()||GlobalProfiles_IsDefault(name))return false;
    auto dir=std::filesystem::path(AppPaths_DataRoot())/L".internal"/L"DeletedProfiles"/
        (name+L"-"+std::to_wstring(GetTickCount64()));
    std::error_code error;std::filesystem::create_directories(dir,error);if(error)return false;
    const auto s=GlobalProfiles_GetSettingsPath(name),b=GlobalProfiles_GetBindingsPath(name);
    if(!CopyFileW(s.c_str(),(dir/L"settings.ini").c_str(),TRUE))return false;
    return Same(s,b)||CopyFileW(b.c_str(),(dir/L"bindings.ini").c_str(),TRUE)!=FALSE;
}

// ---------------------------------------------------------------------------
// Export / import: one validated INI bundle (.hjprofile).
inline constexpr const wchar_t* kExportExtension = L".hjprofile";
inline std::wstring StagePath(const wchar_t* tag) {
    const auto dir=std::filesystem::path(AppPaths_DataRoot())/L".internal";
    std::error_code e;std::filesystem::create_directories(dir,e);
    return (dir/(std::wstring(tag)+L"-"+std::to_wstring(GetCurrentProcessId())+L"-"+std::to_wstring(GetTickCount64())+L".ini")).wstring();
}
inline bool ValidBundle(const std::wstring& path) {
    std::function<void()> prepared;BindingsSnapshot bs;
    return ini::HasBundle(path.c_str())&&SettingsIni_PrepareProfile(path.c_str(),prepared)&&Profile_PrepareIni(path.c_str(),bs);
}
// Writes profile `name` (active or not, any storage kind) and the EXE names
// and titles of its rules to `target`. Never applies the profile.
inline bool Export(const std::wstring& name,const Catalog& catalog,const std::wstring& target,std::wstring& error) {
    error.clear();
    if(Same(name,GlobalProfiles_GetActiveName())&&!GlobalProfiles_Save(name)) {error=L"Could not save the active profile before export.";return false;}
    const auto settings=GlobalProfiles_GetSettingsPath(name),bindings=GlobalProfiles_GetBindingsPath(name);
    BindingsSnapshot snapshot;std::function<void()> prepared;
    {
        ini::ReadFile pinS(settings.c_str()),pinB(bindings.c_str());
        if(!pinS||!pinB||!SettingsIni_PrepareProfile(settings.c_str(),prepared)||!Profile_PrepareIni(bindings.c_str(),snapshot)) {
            error=L"This profile could not be read.";return false;
        }
    }
    const auto stage=StagePath(L"Export");
    bool ok=CopyFileW(settings.c_str(),stage.c_str(),TRUE)!=FALSE;
    if(ok) {
        // Global-only sections never travel with a profile.
        for(const wchar_t* s:{L"Window",L"InputOverlay",L"HallJoyExport"})WritePrivateProfileStringW(s,nullptr,nullptr,stage.c_str());
        WritePrivateProfileStringW(L"Main",L"ActiveGlobalProfile",nullptr,stage.c_str());
        ok=Profile_WriteBindingsSnapshot(stage.c_str(),snapshot);
    }
    if(ok) {
        auto put=[&](const std::wstring& k,const std::wstring& v){return WritePrivateProfileStringW(L"HallJoyExport",k.c_str(),v.c_str(),stage.c_str())!=FALSE;};
        ok=put(L"Version",L"1")&&put(L"Name",name);
        unsigned n=0;
        for(const auto& r:catalog.games)if(Same(r.profile,name)) {
            const auto i=std::to_wstring(n++);
            ok=ok&&put(L"Exe"+i,std::wstring(FileName(r.exe)))&&put(L"Title"+i,r.title);
        }
        ok=ok&&put(L"RuleCount",std::to_wstring(n));
    }
    WritePrivateProfileStringW(nullptr,nullptr,nullptr,stage.c_str()); // flush the profile API cache
    ok=ok&&ValidBundle(stage)&&CopyFileW(stage.c_str(),target.c_str(),FALSE)!=FALSE;
    DeleteFileW(stage.c_str());
    if(!ok&&error.empty())error=L"Could not write the export file.";
    return ok;
}
struct Imported { std::wstring name; std::vector<Rule> rules; };
// Validates `source` in staging, then creates a new profile with a unique name.
inline bool Import(const std::wstring& source,Imported& out,std::wstring& error) {
    error.clear();out={};
    if(IniUtil_IsSessionReadOnly()){error=L"Profiles cannot be changed in this session.";return false;}
    WIN32_FILE_ATTRIBUTE_DATA info{};
    if(!GetFileAttributesExW(source.c_str(),GetFileExInfoStandard,&info)||info.nFileSizeHigh||info.nFileSizeLow>1024*1024) {
        error=L"This file is not a HallJoy profile.";return false;
    }
    const auto stage=StagePath(L"Import");
    if(!CopyFileW(source.c_str(),stage.c_str(),TRUE)){error=L"Could not read the file.";return false;}
    bool ok=ValidBundle(stage);
    std::wstring name;uint32_t count=0;
    if(ok) {
        ini::Read(stage.c_str(),L"HallJoyExport",L"Name",name,256);
        if(GlobalProfiles_SanitizeName(name).empty())name=std::filesystem::path(source).stem().wstring();
        ini::ReadUnsigned(stage.c_str(),L"HallJoyExport",L"RuleCount",kMaxRules,0,count);
        for(uint32_t i=0;i<count;++i) {
            Rule r;const auto k=std::to_wstring(i);
            ini::Read(stage.c_str(),L"HallJoyExport",(L"Exe"+k).c_str(),r.exe,32768);
            ini::Read(stage.c_str(),L"HallJoyExport",(L"Title"+k).c_str(),r.title,kMaxTitleChars+2);
            r.exe=std::wstring(FileName(r.exe));r.profile=L"x";
            if(ValidRule(r))out.rules.push_back(std::move(r));
        }
        for(const wchar_t* s:{L"Window",L"InputOverlay",L"HallJoyExport"})WritePrivateProfileStringW(s,nullptr,nullptr,stage.c_str());
        WritePrivateProfileStringW(L"Main",L"ActiveGlobalProfile",nullptr,stage.c_str());
        WritePrivateProfileStringW(nullptr,nullptr,nullptr,stage.c_str());
        ok=ValidBundle(stage);
    }
    if(!ok){DeleteFileW(stage.c_str());error=L"This file is not a valid HallJoy profile.";return false;}
    out.name=UniqueName(name);
    for(auto& r:out.rules)r.profile=out.name;
    ok=!out.name.empty()&&MoveFileExW(stage.c_str(),GlobalProfiles_GetSettingsPath(out.name).c_str(),MOVEFILE_COPY_ALLOWED|MOVEFILE_WRITE_THROUGH)!=FALSE;
    if(!ok){DeleteFileW(stage.c_str());error=L"Could not create the imported profile.";}
    return ok;
}

// ---------------------------------------------------------------------------
struct Session {
    Catalog catalog; bool manual=false,readOnly=false;std::wstring error;
    std::function<void()> checkpoint;
    // Advisory only: a profile is never refused for backend limits (the user must
    // always be able to reach and fix any profile). Shown after activation.
    std::function<std::wstring(const std::wstring&)> activationNotice;
    void Initialize() {
        const auto p=CatalogPath();
        if(GetFileAttributesW(p.c_str())!=INVALID_FILE_ATTRIBUTES&&!ReadCatalog(p.c_str(),catalog)) {
            readOnly=true;error=L"Game associations could not be read. The original file has been preserved.";
        }
        GlobalProfiles_Prepare(GlobalProfiles_GetActiveName(),checkpoint);
    }
    bool Store(Catalog next) {
        if(readOnly||!SaveCatalog(next)){error=L"Could not save game associations. Check access to the HallJoy data folder.";return false;}
        catalog=std::move(next);error.clear();return true;
    }
    bool Assign(Rule rule) {
        if(!ValidRule(rule)){error=L"This game rule is not valid.";return false;}
        for(const auto& a:catalog.games)if(SameTarget(a,rule)) {
            if(Same(a.profile,rule.profile))return true;
            error=L"This game is already assigned to "+a.profile+L". Remove that assignment first.";return false;
        }
        if(catalog.games.size()>=kMaxRules){error=L"Too many game assignments.";return false;}
        auto next=catalog;next.games.push_back(std::move(rule));return Store(std::move(next));
    }
    bool SetShortcut(const std::wstring& profile,unsigned shortcut) {
        auto next=catalog;
        next.shortcuts.erase(std::remove_if(next.shortcuts.begin(),next.shortcuts.end(),[&](const auto& s){return Same(s.profile,profile);}),next.shortcuts.end());
        if(shortcut) {
            if(next.shortcuts.size()>=kMaxProfileShortcuts){error=L"At most 12 profiles can have their own shortcut.";return false;}
            next.shortcuts.push_back({profile,shortcut});
        }
        return Store(std::move(next));
    }
    bool Activate(const std::wstring& name,bool byUser) {
        if(Same(name,GlobalProfiles_GetActiveName())){if(byUser)manual=true;error.clear();return true;}
        std::function<void()> nextCheckpoint;
        if(!GlobalProfiles_Prepare(name,nextCheckpoint)) {error=L"Could not read profile '"+name+L"'. The previous profile is still active.";return false;}
        if(!GlobalProfiles_Switch(name)) {error=L"Could not save or apply profile '"+name+L"'. The previous profile is still active.";return false;}
        checkpoint=std::move(nextCheckpoint);if(byUser)manual=true;
        error=activationNotice?activationNotice(name):std::wstring{};return true;
    }
    bool Undo() {
        if(!checkpoint){error=L"No editing checkpoint is available.";return false;}
        {profile_runtime::CommitLease lease;if(!lease)return false;checkpoint();profile_runtime::Changed();}
        GlobalProfiles_SetDirty(true);error.clear();return true;
    }
    bool Remove(const std::wstring& name) {
        if(GlobalProfiles_IsDefault(name)||Same(name,GlobalProfiles_GetActiveName())) {
            error=L"Activate another profile before deleting this one.";return false;
        }
        if(!Archive(name)){error=L"Could not back up this profile; it has not been deleted.";return false;}
        auto previous=catalog,next=catalog;ForgetProfile(next,name);
        if(!Store(next))return false;
        if(!GlobalProfiles_Delete(name)){Store(previous);error=L"Could not delete the profile. A recovery copy is retained.";return false;}
        return true;
    }
    bool Rename(const std::wstring& old,const std::wstring& name) {
        if(GlobalProfiles_IsDefault(old)||!NewName(name)){error=L"Choose a unique valid name. Default cannot be renamed.";return false;}
        if(!Duplicate(old,name)) {error=L"Could not create the renamed profile.";return false;}
        auto previous=catalog,next=catalog;RenameProfile(next,old,name);
        if(!Store(next)){GlobalProfiles_Delete(name);return false;}
        if(Same(old,GlobalProfiles_GetActiveName())&&!Activate(name,false)) {Store(previous);GlobalProfiles_Delete(name);return false;}
        if(!Archive(old)||!GlobalProfiles_Delete(old)) {error=L"The new name is ready, but the old copy could not be removed.";return false;}
        return true;
    }
};
}

// keyboard_profiles.h
#pragma once
#include <string>
#include <vector>

#include "key_settings.h" // KeyDeadzone

// IMPORTANT (new meaning):
// This module now manages CURVE PRESETS, not full profiles.
//
// A "Curve preset" stores ONLY one curve (KeyDeadzone) as currently shown in the graph.
// It does NOT store:
//  - polling rate / UI refresh
//  - bindings
//  - per-key list of all keys
//  - any "app state"
//
// Applying a preset is handled by UI logic (KeySettingsPanel), because only UI knows
// whether a key is selected and must auto-enable Override for it.

namespace KeyboardProfiles
{
    struct ProfileInfo
    {
        std::wstring name;
        std::wstring path;
    };

    // Load list of .ini presets from folder.
    // Populates 'outList' with names and paths.
    // Returns index of "active" preset if one was previously selected/saved, or -1.
    // (Active preset name is tracked in this module as UI state.)
    int RefreshList(std::vector<ProfileInfo>& outList);

    // Load preset file into outKs (single curve).
    bool LoadPreset(const std::wstring& path, KeyDeadzone& outKs);

    // Save preset file from ks (single curve).
    bool SavePreset(const std::wstring& path, const KeyDeadzone& ks);

    // Create a new preset with given name (auto-generates filename) and save ks into it.
    bool CreatePreset(const std::wstring& name, const KeyDeadzone& ks);

    // Delete a preset file.
    bool DeletePreset(const std::wstring& path);

    // UI state: currently active preset name (for highlighting in combo).
    // Empty => none/modified.
    std::wstring GetActiveProfileName();
    void SetActiveProfileName(const std::wstring& name);

    // Mark current curve as "modified" (unsaved changes vs active preset).
    void SetDirty(bool dirty);
    bool IsDirty();
}
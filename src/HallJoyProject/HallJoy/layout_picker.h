#pragma once
#include "keyboard_layout.h"
#include "premium_combo.h"
#include "layout_sort.h"
#include <algorithm>
#include <string>
#include <vector>

// UI-thread controller shared by retained settings pages and the editor.
// Browsing a manufacturer never commits a layout. Follow is a separate brand
// sentinel (empty internal name), committed explicitly by the overlay owner.
// Rows are explicitly mapped to the
// shared catalog; special rows cannot accidentally select/delete a preset.
struct LayoutPicker
{
    HWND brand = nullptr, model = nullptr, variant = nullptr;
    struct Group { std::wstring label; std::vector<int> members; };
    std::vector<Group> groups;
    std::vector<int> variants;
    std::vector<std::wstring> brands;
    std::vector<int> presets;
    std::wstring browsing;
    int observed = -2, catalogCount = -1;
    uint64_t revision = 0;
    bool follow = false, editable = false;
    static constexpr int Invalid = -2, Create = -3;
    // All is a view, never persisted as a layout's manufacturer.
    const wchar_t* CreationBrand() const { return browsing == L"All" ? L"Custom" : browsing.c_str(); }

    bool Current() const { return revision == KeyboardLayout_GetCatalogRevision(); }
    int PresetAt(int row) const { return Current() && row >= 0 && row < (int)presets.size() ? presets[row] : Invalid; }
    static std::wstring VariantName(int preset) {
        const auto name = KeyboardLayout_GetPresetModel(preset);
        // User/technical names are opaque; never merge them by a guessed suffix.
        const auto maker = KeyboardLayout_GetPresetBrand(preset);
        if (maker == L"Custom" || maker == L"Other") return L"";
        for (const auto* suffix : {L"ANSI", L"ISO", L"JIS"}) {
            const std::wstring tail = std::wstring(L" ") + suffix;
            if (name.size() > tail.size() && name.compare(name.size()-tail.size(),tail.size(),tail)==0)
                return suffix;
        }
        return L"";
    }
    static std::wstring ModelName(int preset, bool full) {
        auto name = full ? KeyboardLayout_GetPresetDisplayName(preset) : KeyboardLayout_GetPresetModel(preset);
        const auto suffix = VariantName(preset);
        if (!suffix.empty()) name.resize(name.size()-suffix.size()-1);
        return name;
    }
    bool HasVariants() const { return variants.size() > 1; }
    bool Following() const { return follow && browsing.empty(); }
    int VariantAt(int row) const { return Current() && row >= 0 && row < (int)variants.size() ? variants[row] : Invalid; }
    int Selected() const {
        if (Current() && Following()) return -1;
        const int row = PremiumCombo::GetCurSel(model);
        const int preset = PresetAt(row);
        const int selectedVariant = VariantAt(PremiumCombo::GetCurSel(variant));
        if (preset >= 0 && row < (int)groups.size() &&
            std::find(groups[row].members.begin(),groups[row].members.end(),selectedVariant)!=groups[row].members.end())
            return selectedVariant;
        return preset;
    }
    int Row(int preset) const {
        for (int row=0;row<(int)groups.size();++row)
            if (std::find(groups[row].members.begin(),groups[row].members.end(),preset)!=groups[row].members.end()) return row;
        return -1;
    }
    void Variants(int active) {
        PremiumCombo::Clear(variant); variants.clear();
        const int row = PremiumCombo::GetCurSel(model);
        if (row >= 0 && row < (int)groups.size() && PresetAt(row)>=0) variants=groups[row].members;
        int selected=-1;
        for (int i=0;i<(int)variants.size();++i) {
            PremiumCombo::AddString(variant,VariantName(variants[i]).c_str());
            if (variants[i]==active) selected=i;
            if (editable && HasVariants() && catalogCount>1)
                PremiumCombo::SetItemButtonKind(variant,i,PremiumCombo::ItemButtonKind::Delete);
        }
        PremiumCombo::SetCurSel(variant,selected>=0 ? selected : variants.empty() ? -1 : 0,false);
    }
    int ChooseModel() {
        // Prefer the previous physical variant when another model offers it.
        int active = PresetAt(PremiumCombo::GetCurSel(model));
        const auto previous = observed>=0 ? VariantName(observed) : L"";
        const int row = Row(active);
        if (row>=0 && active>=0)
            for (int member : groups[row].members)
                if (member==observed || (!previous.empty() && VariantName(member)==previous)) { active=member; break; }
        Variants(active);
        return Selected();
    }
    void Models(int active) {
        PremiumCombo::Clear(model);
        presets.clear(); groups.clear();
        struct Entry { int preset; std::wstring label; };
        std::vector<Entry> entries;
        entries.reserve(KeyboardLayout_GetPresetCount());
        for (int i = 0; i < KeyboardLayout_GetPresetCount(); ++i) {
            if (browsing != L"All" && KeyboardLayout_GetPresetBrand(i) != browsing) continue;
            entries.push_back({i,ModelName(i,browsing == L"All")});
        }
        std::stable_sort(entries.begin(),entries.end(),[](const Entry& a,const Entry& b) {
            return halljoy::layout_sort::Less(a.label,b.label);
        });
        for (const auto& entry : entries) {
            auto found=std::find_if(groups.begin(),groups.end(),[&](const Group& group) {
                return group.label==entry.label && group.members.front()>=0 &&
                    KeyboardLayout_GetPresetBrand(group.members.front())==KeyboardLayout_GetPresetBrand(entry.preset) &&
                    !VariantName(entry.preset).empty() &&
                    std::all_of(group.members.begin(),group.members.end(),[&](int member) {
                        return !VariantName(member).empty() && VariantName(member)!=VariantName(entry.preset);
                    });
            });
            if (found==groups.end()) groups.push_back({entry.label,{entry.preset}});
            else found->members.push_back(entry.preset);
        }
        if (editable) groups.push_back({L"+ Create New Layout...",{Create}});
        for (auto& group : groups) {
            std::stable_sort(group.members.begin(),group.members.end(),[](int a,int b) {
                return a>=0 && b>=0 && VariantName(a)<VariantName(b);
            });
            const int row=PremiumCombo::AddString(model,group.label.c_str());
            presets.push_back(group.members.front());
            if (editable && catalogCount>1 && group.members.front()>=0 && group.members.size()==1)
                PremiumCombo::SetItemButtonKind(model, row, PremiumCombo::ItemButtonKind::Delete);
        }
        PremiumCombo::SetPlaceholderText(model, L"Choose model");
        PremiumCombo::SetCurSel(model, Row(active), false);
        Variants(active);
    }
    void Refresh(int active, bool force = false) {
        if (!force && active == observed && Current()) return;
        observed = active; catalogCount = KeyboardLayout_GetPresetCount();
        revision = KeyboardLayout_GetCatalogRevision();
        if ((follow && active < 0) || browsing != L"All") browsing = active >= 0 ? KeyboardLayout_GetPresetBrand(active) : L"";
        brands.clear();
        for (int i = 0; i < catalogCount; ++i) {
            const auto name = KeyboardLayout_GetPresetBrand(i);
            if (name != L"All" && name != L"Other" && name != L"Custom" && std::find(brands.begin(), brands.end(), name) == brands.end())
                brands.push_back(name);
        }
        std::sort(brands.begin(), brands.end());
        brands.insert(brands.begin(), L"All");
        if (follow) brands.insert(brands.begin(), L"");
        brands.push_back(L"Other"); brands.push_back(L"Custom");
        PremiumCombo::Clear(brand);
        for (const auto& name : brands) PremiumCombo::AddString(brand, name.empty() && follow ? L"Same as main layout" : name.c_str());
        PremiumCombo::SetPlaceholderText(brand, L"Choose brand");
        const auto it = std::find(brands.begin(), brands.end(), browsing);
        PremiumCombo::SetCurSel(brand, it == brands.end() ? -1 : (int)(it - brands.begin()), false);
        Models(active);
    }
    void Browse(int active) {
        const int row = PremiumCombo::GetCurSel(brand);
        if (row < 0 || row >= (int)brands.size()) return;
        browsing = brands[row];
        Models(active);
    }
};

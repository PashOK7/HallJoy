#pragma once
#include <cstdint>
#include <string_view>
#include "keychron_layout_identities.h"
#include "../../../third_party/UniversalAnalogPluginFixed/halljoy_drunkdeer_identity.h"

namespace halljoy::layout_selection
{
// Only the owner's validated query can set verifiedModel. Product strings
// alone (including a plausible exact name) never identify an A75 variant.
inline const wchar_t* MatchDrunkDeer(std::uint16_t vid,std::uint16_t pid,
    unsigned rows,unsigned columns,bool verifiedModel,std::string_view name)
{
    if (vid!=0x352d || rows!=6 || columns!=21 || !verifiedModel) return nullptr;
    using namespace halljoy::drunkdeer_identity;
    constexpr const wchar_t* names[]={L"DrunkDeer A75 ANSI",L"DrunkDeer A75 Pro",
        L"DrunkDeer A75 ISO",L"DrunkDeer G60 ANSI",L"DrunkDeer G65 ANSI",
        L"DrunkDeer G75 ANSI",L"DrunkDeer G75 JIS"};
    for (unsigned i=1;i<=7;++i) {
        const auto model=static_cast<Model>(i);
        if (MatchesProduct(model,pid) && name==Name(model)) return names[i-1];
    }
    return nullptr;
}
// Immutable catalogue: exact model/variant and complete matrix dimensions.
inline const wchar_t* Match(std::uint16_t vid, std::uint16_t pid,
    std::uint16_t page, std::uint16_t usage, unsigned rows, unsigned columns)
{
    if (page != 0xFF60 || usage != 0x61) return nullptr;
    if (vid == 0x362D && rows == 6 && columns == 15) {
        if (pid == 0x0610) return L"Lemokey P1 HE ANSI";
        if (pid == 0x0611) return L"Lemokey P1 HE ISO";
    }
    if (vid != 0x3434) return nullptr;
    for (const auto& entry : kKeychronLayouts)
        if (pid == entry.pid && rows == entry.rows && columns == entry.columns) return entry.name;
    return nullptr;
}

class FirstRun final
{
    bool pending_ = false;
public:
    void Arm() noexcept { pending_ = true; }
    void Cancel() noexcept { pending_ = false; }
    bool Pending() const noexcept { return pending_; }
    // Called only for a coherent discovery snapshot. Even an ambiguous/empty
    // result completes the one-shot decision; hotplug never steals selection.
    bool Consume(bool ready, unsigned sourceCount, bool exactMatch) noexcept
    {
        if (!pending_ || !ready) return false;
        pending_ = false;
        return sourceCount == 1 && exactMatch;
    }
};
}

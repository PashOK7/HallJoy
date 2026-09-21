#pragma once
#include "aula_win60he_client.h"
#include "generated/layout_pipeline/identities.h"
#include "native_layout_state.h"

namespace halljoy::sparkplayjoy_layout {
inline std::uint64_t Token(std::uint16_t vid, std::uint16_t pid,
                           const aula_win60he::CapabilityProof& proof) noexcept {
    const auto* identity = aula_win60he::FindKnownUsbIdentity(vid, pid);
    if (!identity || identity->boardId != proof.sync.boardId ||
        proof.compatibilityMismatchMask) return 0;
    switch (identity->boardId) {
    case 0x0A021902: return layout_identity::Token("aula-rm6x21", "0A021902");
    case 0x16052201: return layout_identity::Token("aula-rm6x21", "16052201");
    case 0x16052202: return layout_identity::Token("aula-rm6x21", "16052202");
    case 0x2E022201: return layout_identity::Token("aula-rm6x21", "2E022201");
    default: return 0;
    }
}
inline aula_win60he::ActiveKeyMap Factory(const aula_win60he::KeyMap& map) noexcept {
    aula_win60he::ActiveKeyMap result{};
    for (std::size_t r=0;r<map.size();++r)
        for (std::size_t c=0;c<map[r].size();++c)
            result[r][c] = map[r][c] == 1 ? keycode::kFn :
                aula_win60he::PublishedKeyCodeForFunction(map[r][c]);
    return result;
}
inline bool Publish(std::uint64_t token, const aula_win60he::CapabilityProof& proof) {
    const auto factory = Factory(proof.defaultKeyMap);
    std::array<native_layout::Key, aula_win60he::kRows*aula_win60he::kColumns> keys{};
    std::size_t count=0;
    for (std::size_t r=0;r<factory.size();++r)
        for (std::size_t c=0;c<factory[r].size();++c)
            if (factory[r][c]) keys[count++]={factory[r][c], proof.keyMap[r][c]};
    return native_layout::Publish(token,keys.data(),count);
}
} // namespace halljoy::sparkplayjoy_layout

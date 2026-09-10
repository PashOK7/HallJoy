#pragma once
#include <array>
#include <cstddef>
#include <cstdint>

namespace halljoy::drunkdeer_identity {
enum class Model : std::uint8_t { Unknown, A75Ansi, A75Pro, A75Iso, G60, G65, G75Ansi, G75Jis };
struct Identity {
    Model model=Model::Unknown;
    std::array<std::uint8_t,3> signature{};
};
inline constexpr std::array<std::uint8_t,64> Request() {
    std::array<std::uint8_t,64> r{};
    r[0]=4; r[1]=0xA0; r[2]=2;
    return r;
}
// Native full report includes report ID; Antler payload offsets are one less.
// Unknown signatures, short replies and another command never become A75.
inline Identity Parse(const std::uint8_t* r,std::size_t size) noexcept {
    Identity id;
    if (!r || size!=64 || r[0]!=4 || r[1]!=0xA0 || r[2]!=2 || r[3]!=0) return id;
    id.signature={r[5],r[6],r[7]};
    if (id.signature==std::array<std::uint8_t,3>{11,1,1} || id.signature==std::array<std::uint8_t,3>{11,4,1}) id.model=Model::A75Ansi;
    else if (id.signature==std::array<std::uint8_t,3>{11,4,3}) id.model=Model::A75Pro;
    else if (id.signature==std::array<std::uint8_t,3>{11,4,2}) id.model=Model::A75Iso;
    else if (id.signature==std::array<std::uint8_t,3>{11,3,1}) id.model=Model::G60;
    else if (id.signature==std::array<std::uint8_t,3>{11,2,1} || id.signature==std::array<std::uint8_t,3>{15,1,1}) id.model=Model::G65;
    else if (id.signature==std::array<std::uint8_t,3>{11,4,5}) id.model=Model::G75Ansi;
    else if (id.signature==std::array<std::uint8_t,3>{11,4,7}) id.model=Model::G75Jis;
    return id;
}
inline constexpr const char* Name(Model model) noexcept {
    switch(model) {
    case Model::A75Ansi:return "DrunkDeer A75 ANSI";
    case Model::A75Pro:return "DrunkDeer A75 Pro";
    case Model::A75Iso:return "DrunkDeer A75 ISO";
    case Model::G60:return "DrunkDeer G60 ANSI";
    case Model::G65:return "DrunkDeer G65 ANSI";
    case Model::G75Ansi:return "DrunkDeer G75 ANSI";
    case Model::G75Jis:return "DrunkDeer G75 JIS";
    default:return nullptr;
    }
}
inline constexpr std::uint16_t Product(Model model) noexcept {
    switch(model) {
    case Model::A75Ansi:case Model::A75Pro:case Model::A75Iso:return 0x2383;
    case Model::G65:return 0x2382;
    case Model::G60:return 0x2384;
    case Model::G75Ansi:return 0x2386;
    case Model::G75Jis:return 0x2391;
    default:return 0;
    }
}
inline constexpr bool MatchesProduct(Model model,std::uint16_t pid) noexcept {
    return model!=Model::Unknown && Product(model)==pid;
}
}

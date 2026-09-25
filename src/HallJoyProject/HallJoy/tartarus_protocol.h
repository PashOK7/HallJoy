#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
namespace halljoy::tartarus {
constexpr unsigned kVid=0x1532, kPid=0x0244;
// Report positions 01..20, labelled by factory assignments. Synapse remaps do
// not change these physical identities. Thumb/d-pad/wheel are separate digital input.
inline constexpr std::array<std::uint16_t,20> kFactoryHids={
 30,31,32,33,34,43,20,26,8,21,57,4,22,7,9,225,29,27,6,44};
using Values=std::array<std::uint16_t,20>;
inline bool Parse(const unsigned char* p,std::size_t size,Values& out) noexcept {
 if(!p || size<21 || size>1024 || p[0]!=6)return false;
 Values v{};
 for(unsigned i=0;i<v.size();++i)v[i]=static_cast<std::uint16_t>((unsigned(p[i+1])*1000+127)/255);
 out=v;return true; // Uninterpreted tail is not another analog channel.
}
}

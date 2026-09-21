#pragma once
#include "native_layout_state.h"
namespace halljoy::sayo::layout {
inline std::atomic<std::uint64_t> token{0};
inline std::array<std::atomic<std::uint16_t>,3> windowsHid{};
inline bool Active() noexcept { return native_layout::UsesRemapping(token.load(std::memory_order_acquire)); }
template<class IsBound> bool IsWindowsBound(std::uint16_t hid,IsBound bound) noexcept {
    if(!hid || !Active()) return false;
    for(std::size_t i=0;i<3;++i)
        if(windowsHid[i].load(std::memory_order_relaxed)==hid && bound(static_cast<std::uint16_t>(keycode::kO3cFirst+i))) return true;
    return false;
}
}

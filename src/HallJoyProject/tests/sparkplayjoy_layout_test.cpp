#include "../HallJoy/sparkplayjoy_layout.h"
#include <cassert>
int main() {
    using namespace halljoy;
    for (const auto id : aula_win60he::kKnownUsbIdentities) {
        aula_win60he::CapabilityProof proof{};
        proof.sync.boardId=id.boardId;
        proof.defaultKeyMap[0][0]=4; proof.defaultKeyMap[0][1]=7;
        proof.defaultKeyMap[1][0]=1;
        proof.keyMap[0][0]=26; proof.keyMap[0][1]=26;
        proof.keyMap[1][0]=keycode::kFn;
        const auto token=sparkplayjoy_layout::Token(id.vendorId,id.productId,proof);
        assert(token && sparkplayjoy_layout::Publish(token,proof));
        const auto map=native_layout::Read(token);
        assert(map.complete && map.count==3 && map.keys[0].factory==4 && map.keys[0].assigned==26);
        assert(map.keys[1].factory==7 && map.keys[1].assigned==26);
        const auto factory=sparkplayjoy_layout::Factory(proof.defaultKeyMap);
        assert(factory[0][0]==4 && factory[0][1]==7 && factory[1][0]==keycode::kFn);
        native_layout::activeToken.store(token);
        assert(native_layout::UsesRemapping(token));
        assert(sparkplayjoy_layout::Publish(token,proof));
        assert(native_layout::activeToken.load()==token);
        assert(native_layout::Read(token).revision>map.revision);
        native_layout::enabled.store(false);assert(!native_layout::UsesRemapping(token));
        native_layout::enabled.store(true);
        proof.compatibilityMismatchMask=1;
        assert(!sparkplayjoy_layout::Token(id.vendorId,id.productId,proof));
        proof.compatibilityMismatchMask=0;proof.sync.boardId^=1;
        assert(!sparkplayjoy_layout::Token(id.vendorId,id.productId,proof));
        native_layout::Clear(token);assert(!native_layout::Read(token).complete);
    }
}

#include "../HallJoy/attackshark_pro_layouts.h"
#include "../HallJoy/attackshark_pro_layout_identity.h"
#include <cassert>
#include "../HallJoy/attackshark_pro_native_model.h"
#include "../HallJoy/generated/layout_pipeline/layouts.h"
#include <cwchar>
#include <iterator>
#include <set>
template<size_t N> void Check(const KeyDef (&keys)[N], size_t expected) {
    assert(N==expected);
    std::set<unsigned> usages;
    bool fn=false, w=false;
    for(const auto& k:keys) {
        assert(k.x>=0 && k.y>=0 && k.w>=18 && k.h>=18);
        assert(usages.insert(k.hid).second);
        fn|=k.hid==1033;w|=k.hid==26;
        for(const auto& other:keys) {
            if(&k==&other)continue;
            assert(!(k.x<other.x+other.w && other.x<k.x+k.w &&
                k.y<other.y+other.h && other.y<k.y+k.h));
        }
    }
    assert(fn && w);
}
int main() {
    using namespace halljoy::sharklayout;
    struct Preset { const wchar_t* name; const KeyDef* keys; int count; const wchar_t* brand; };
    const Preset presets[]={
#include "../HallJoy/generated/layout_pipeline/presets.inc"
        {X65,g_shark_x65_ansi,(int)std::size(g_shark_x65_ansi),L"ATTACK SHARK"},
        {X68,g_shark_x68_ansi,(int)std::size(g_shark_x68_ansi),L"ATTACK SHARK"},
        {X82,g_shark_x82_ansi,(int)std::size(g_shark_x82_ansi),L"ATTACK SHARK"}
    };
    assert(std::size(Identities)==37);
    for(const auto& profile:halljoy::sharkplay::Profiles){
        assert(Token(profile.id) && Match(Token(profile.id)));
        bool found=false;
        for(const auto& preset:presets)if(std::wcscmp(preset.name,Match(Token(profile.id)))==0){
            found=true;assert(std::wcscmp(preset.brand,L"ATTACK SHARK")==0);
            std::set<unsigned> seen;
            for(int i=0;i<preset.count;++i){
                const auto& key=preset.keys[i];assert(seen.insert(key.hid).second);
                bool mapped=false;for(auto hid:profile.factory)mapped|=hid==key.hid;
                assert(mapped); // Every visible key has a native factory assignment.
            }
        }
        assert(found);
    }
    Check(g_shark_x65_ansi,66);Check(g_shark_x68_ansi,66);Check(g_shark_x82_ansi,83);
    for(unsigned id:{2308u,2938u})assert(std::wcscmp(Match(Token(id)),X65)==0);
    for(unsigned id:{2370u,2901u})assert(std::wcscmp(Match(Token(id)),X68)==0);
    for(unsigned id:{2356u,2935u})assert(std::wcscmp(Match(Token(id)),X82)==0);
    assert(!Token(0) && !Token(9999) && !Token(0x502f));
    assert(!Match(0) && !Match(0x502f));
    assert(Token(2308)!=Token(2370) && Token(2356)!=Token(2370));
}

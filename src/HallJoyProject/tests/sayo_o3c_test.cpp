#include "sayo_o3c_protocol.h"
#include "native_layout_state.h"
#include "sayo_layout_state.h"
#include "block_keys_policy.h"
#include <string>
#include <cassert>
#include <cstdio>
#include <vector>
using namespace halljoy::sayo::o3c;
static void Put(std::uint8_t* p,unsigned v) {p[0]=std::uint8_t(v);p[1]=std::uint8_t(v>>8);}
static std::array<std::uint8_t,1024> Reply(unsigned cmd,unsigned index,std::vector<std::uint8_t> payload,unsigned echo=ConfigEcho) {
    auto p=Read(std::uint8_t(cmd),std::uint8_t(index));p[1]=std::uint8_t(echo);Put(p.data()+4,unsigned(payload.size()+4));
    for(std::size_t i=0;i<payload.size();++i)p[8+i]=payload[i];
    unsigned sum=0;for(std::size_t i=0;i<8+payload.size();i+=2)if(i!=2)sum+=U16(p.data()+i);
    Put(p.data()+2,sum);return p;
}
int main() {
    Frame f;std::uint16_t hid=0;
    auto request=Read(0x10,2);assert(Parse(request.data(),request.size(),f));assert(f.size==0 && f.index==2);
    auto identity=Reply(0,0,{9,0,1,0});assert(Parse(identity.data(),1024,f) && Identity(f));
    for(unsigned index=0;index<3;++index) {
        std::vector<std::uint8_t> key(56);key[0]=1;Put(key.data()+4,1000+index*2000);Put(key.data()+6,3000);
        Put(key.data()+8,1800);Put(key.data()+10,1800);key[21]=std::uint8_t(Factory[index]);
        auto parseKey=[&]() {auto r=Reply(0x10,index,key);assert(Parse(r.data(),r.size(),f));
            return Binding(f,std::uint8_t(index),hid);};
        assert(parseKey() && hid==Factory[index]);
        key[21]=0x1a;assert(parseKey() && hid==0x1a);
        key[20]=1;assert(!parseKey()); // Ctrl+W is not W.
        key[21]=0;assert(parseKey() && hid==0xe0);
        key[20]=0x80;assert(parseKey() && hid==0xe7);
        key[20]=3;assert(!parseKey());
        key[20]=0;assert(parseKey() && hid==0);
        key[16]=3;assert(!parseKey()); // Mouse mode is not a keyboard usage.
        key[16]=0;key[22]=4;assert(!parseKey());
        key[22]=0;key[4]^=1;assert(!parseKey());
    }
    auto depth=Reply(0x15,1,{0,0,0xd0,7,0xa0,15},DepthEcho);
    std::array<std::uint16_t,3> raw{};assert(Parse(depth.data(),1024,f) && Depth(f,raw));
    assert(raw[0]==0 && Normalize(raw[1])==500 && Normalize(raw[2])==1000);
    assert(!Binding(f,0,hid));
    for(std::size_t n=0;n<14;++n) assert(!Parse(depth.data(),n,f));
    for(unsigned offset:{0u,1u,2u,4u,6u,7u,8u,12u}) {
        auto bad=depth;bad[offset]^=1;assert(!Parse(bad.data(),1024,f));
    }
    auto wrong=Reply(0x10,1,{0,0,0xd0,7,0xa0,15},DepthEcho);
    assert(Parse(wrong.data(),1024,f) && !Depth(f,raw));
    wrong=Reply(0x15,0,{0,0,0xd0,7,0xa0,15},DepthEcho);
    assert(Parse(wrong.data(),1024,f) && !Depth(f,raw));
    wrong=Reply(0x15,1,{0,0,0xd0,7,0xa0,15},ConfigEcho);
    assert(Parse(wrong.data(),1024,f) && !Depth(f,raw));
    assert(Value(9,{9,9,10},{750,0,500})==750);
    assert(Value(9,{9,9,10},{0,250,500})==250);
    assert(Value(9,{9,9,10},{0,0,500})==0);
    assert(Value(0,{0,0,0},{1000,1000,1000})==0);
    assert(Normalize(0)==0 && Normalize(12)==0 && Normalize(8000)==1000);
    halljoy::native_layout::Key keys[]={{29,9},{27,10},{6,11}};
    assert(halljoy::native_layout::Publish(123,keys,3));
    halljoy::native_layout::enabled=true;halljoy::native_layout::activeToken=123;
    assert(halljoy::native_layout::UsesRemapping(123));
    halljoy::native_layout::enabled=false;assert(!halljoy::native_layout::UsesRemapping(123));
    halljoy::native_layout::Clear(123);assert(!halljoy::native_layout::Read(123).complete);
    for(const auto bindings : {std::array<std::uint16_t,3>{}, {9,0,11}, {9,9,9}}) {
        const auto map=Layout(bindings);
        assert(halljoy::native_layout::Publish(123,map.data(),map.size()));
        const auto snap=halljoy::native_layout::Read(123);
        assert(snap.complete && snap.count==3);
        for(std::size_t i=0;i<3;++i) {
            assert(map[i].assigned==halljoy::keycode::kO3cFirst+i && map[i].factory==Factory[i]);
            assert(map[i].labelHid==bindings[i]);
            if(!bindings[i]) assert(std::wstring(map[i].label.data())==L"Key "+std::to_wstring(i+1));
        }
    }
    assert(Matches(0,halljoy::keycode::kO3cFirst,true,0));
    assert(!Matches(1,halljoy::keycode::kO3cFirst,true,0));
    assert(Matches(0,9,true,9) && Matches(1,9,true,9));
    assert(!Matches(0,9,false,9) && Matches(0,Factory[0],false,9));
    assert(!Matches(0,halljoy::keycode::kO3cFirst,false,9));
    assert(!Matches(0,0,true,0));
    halljoy::sayo::layout::token=123;
    halljoy::sayo::layout::windowsHid[0]=9;
    halljoy::sayo::layout::windowsHid[1]=9;
    halljoy::native_layout::enabled=true;halljoy::native_layout::activeToken=123;
    auto bound=[](std::uint16_t code){return code==halljoy::keycode::kO3cFirst+1;};
    assert(halljoy::block_keys::IsWindowsKeyBound(9,bound));
    assert(!halljoy::block_keys::IsWindowsKeyBound(10,bound));
    halljoy::native_layout::enabled=false;
    assert(!halljoy::block_keys::IsWindowsKeyBound(9,bound));
    halljoy::native_layout::Clear(123);halljoy::sayo::layout::token=0;
    std::puts("SAYO_O3C_TEST=PASS");
}

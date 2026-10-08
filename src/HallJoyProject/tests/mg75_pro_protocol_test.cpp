#include "mg75_pro_protocol.h"
#include "jingtai_v1_profiles.h"
#include "wlmouse_ying75_protocol.h"
#include "physical_analog_state.h"
#include <set>
#include <cassert>
#include <iostream>
using namespace halljoy::mg75pro;
Frame Assemble(const std::array<std::uint8_t, 132> &data) {
  Frame f;
  for (unsigned part = 0; part < 3; ++part) {
    Report r{};
    r.fill(0xee);
    r[0] = 0;
    for (unsigned i = 0; i < 64 && part * 64 + i < data.size(); ++i)
      r[i + 1] = data[part * 64 + i];
    assert(f.Push(r, 65, 0x12));
    assert(f.Complete() == (part == 2));
  }
  return f;
}
int main() {
  {
    // WLMOUSE Ying75: exact identity, 84 unique keys, Fn, 3300 um range.
    namespace ying=halljoy::wlmouse_ying75;
    assert(ying::ExactModel(0x36a7,0xf887,L"WLKB YING 75"));
    assert(!ying::ExactModel(0x36a7,0xf887,L"KEYBOARD") && !ying::ExactModel(0x36a7,0x0001,L"WLKB YING 75"));
    std::set<unsigned> keys; unsigned fn=0;
    for(const auto action:ying::kModel.actions) if(action){ fn+=action==0xf001; assert(keys.insert(Decode(action)).second); }
    assert(keys.size()==ying::kModel.count && ying::kModel.count==84 && fn==1 && ying::kModel.range==3300);
    assert(Decode(ying::kModel.actions[0])==41 && Decode(ying::kModel.actions[126-5-1])==79);
    assert(Normalize(3300,ying::kModel.range)==1000 && Normalize(1650,ying::kModel.range)==500);
    // A travel reply whose length/status/checksum bytes do not follow the
    // legacy rules: strict mode names the cause, lenient (official client)
    // mode accepts the three reports and the values parse.
    std::array<std::uint8_t,192> raw{};
    raw[0]=0x5c; raw[1]=0; raw[2]=0x92; raw[3]=0x00; raw[4]=7; raw[5]=2;
    for(unsigned i=0;i<63;++i){ raw[6+2*i]=static_cast<std::uint8_t>(i*10); raw[7+2*i]=static_cast<std::uint8_t>(i>>3); }
    const auto push=[&](Frame& f){
      for(unsigned part=0;part<3;++part){ Report r{}; r[0]=0; std::copy_n(raw.begin()+part*64,64,r.begin()+1);
        if(!f.Push(r,65,0x12)) return false; if(f.Complete()) return part==2; }
      return f.Complete(); };
    Frame strict; assert(!push(strict) && strict.reject==kRejectLength); // length byte 0
    assert(strict.head[0]==0x5c && strict.head[2]==0x92 && strict.reports==1);
    Frame lenient; lenient.lenient=true; assert(push(lenient) && lenient.size==132 && lenient.reports==3);
    Values values{}; assert(ParseTravel(lenient,values) && values[0]==0 && values[62]==((62*10)&0xff)+((62>>3)<<8));
    raw[2]=0x93; Frame wrong; wrong.lenient=true; assert(!push(wrong) && wrong.reject==kRejectCommand); raw[2]=0x92;
    // Factory map proof for Ying75: Windows matrix, or the macOS one with Left Win/Alt swapped.
    for(unsigned row=0;row<6;row+=2) for(int mac=0;mac<2;++mac){
      auto actions=ying::kModel.actions; if(mac) std::swap(actions[106],actions[107]);
      std::array<std::uint8_t,49> fr{}; fr[0]=0x5c; fr[1]=45; fr[2]=0xab; fr[5]=static_cast<std::uint8_t>(row); fr[27]=static_cast<std::uint8_t>(row+1);
      for(unsigned c=0;c<21;++c){ auto sel=[&](unsigned s){ return actions[s]==0xf001?1:static_cast<std::uint8_t>(actions[s]); };
        fr[6+c]=sel(row*21+c); fr[28+c]=sel((row+1)*21+c); }
      fr[3]=Checksum(fr.data());
      Frame f; Report r{}; r[0]=0; std::copy(fr.begin(),fr.end(),r.begin()+1); assert(f.Push(r,65,0x2b) && f.Complete());
      assert(MatchFactoryActions(f,row,actions));
      assert(MatchFactoryActions(f,row,ying::kModel.actions)==(row!=4 || !mac));
      if(row==0) assert(!MatchFactory(f,row)); // top row differs from MG75 Pro
    }
  }
  // Exercise exact admission and both sparse halves through the real physical
  // publication component, including Fn, aliased assignments and release.
  for (const auto& identity : halljoy::jingtai_v1::identities) {
    const auto* model=halljoy::jingtai_v1::Find(identity.vid,identity.pid,identity.product);
    assert(model==identity.model);
    assert(model->range==3500 || model->range==3600 || model->range==4000);
    assert(Normalize(0,model->range)==0);
    assert(Normalize(static_cast<std::uint16_t>(model->range/2),model->range)==500);
    assert(Normalize(static_cast<std::uint16_t>(model->range),model->range)==1000);
    assert(Normalize(static_cast<std::uint16_t>(model->range+100),model->range)==1000);
    assert(!halljoy::jingtai_v1::Find(identity.vid,identity.pid,L"keyboard"));
    assert(!halljoy::jingtai_v1::Find(0xffff,identity.pid,identity.product));
    halljoy::physical_analog::Publication factory,assigned;
    std::set<unsigned> hids;
    unsigned mapped=0;
    for(unsigned slot=0;slot<126;++slot) {
      const auto hid=Decode(model->actions[slot]);
      if(!hid)continue;
      assert(hids.insert(hid).second);
      assert(factory.Bind(static_cast<std::uint8_t>(slot+1),hid));
      assert(assigned.Bind(static_cast<std::uint8_t>(slot+1),4));
      const auto depth=static_cast<std::uint16_t>(slot<63?500:1000);
      factory.Publish(static_cast<std::uint8_t>(slot+1),depth,100);
      assigned.Publish(static_cast<std::uint8_t>(slot+1),depth,100);
      assert(factory.Read(hid,100,150).milli==depth);
      ++mapped;
    }
    assert(mapped==model->count && hids.count(kFn));
    assert(assigned.Read(4,100,150).milli==1000);
    for(unsigned slot=63;slot<126;++slot)
      if(model->actions[slot])assigned.Publish(static_cast<std::uint8_t>(slot+1),0,101);
    assert(assigned.Read(4,101,150).milli==500);
    for(unsigned slot=0;slot<63;++slot)
      if(model->actions[slot])assigned.Publish(static_cast<std::uint8_t>(slot+1),0,102);
    assert(assigned.Read(4,102,150).milli==0);
    assert(factory.Read(kFn,251,150).milli==0);
    factory.Clear();assigned.Clear();
    assert(!factory.Owns(kFn) && !assigned.Owns(4));
  }

  const auto request = Travel(1);
  assert(request[1] == 0x5c && request[2] == 4 && request[3] == 0x12 &&
         request[4] == 0xa6 && request[5] == 2 && request[6] == 1);
  assert(Travel(0) == Report{} && Travel(3) == Report{});
  assert(ExactModel(0x1ca5, 0x0807, L"IROK MG75 PRO"));
  assert(!ExactModel(0x1ca5, 0x0807, L"IROK MG75"));
  unsigned mapped = 0;
  for (auto a : kFactoryActions)
    mapped += a != 0;
  assert(mapped == 81);
  assert(kFactoryActions[116] == 0xf001 && Decode(0xf001) == kFn &&
         Decode(0xf101) == 0);
  assert(Normalize(0) == 0 && Normalize(1750) == 500 &&
         Normalize(3500) == 1000);
  // Exact bytes from executing Pro 1.1.0 serializer 0x0804951C with
  // 63 distinct depths i*53; not synthesized by the production packet code.
  const std::array<std::uint8_t, 132> wire = {
      {92,  128, 146, 175, 0,   2,   0,   0,   53,  0,   106, 0,  159, 0,   212,
       0,   9,   1,   62,  1,   115, 1,   168, 1,   221, 1,   18, 2,   71,  2,
       124, 2,   177, 2,   230, 2,   27,  3,   80,  3,   133, 3,  186, 3,   239,
       3,   36,  4,   89,  4,   142, 4,   195, 4,   248, 4,   45, 5,   98,  5,
       151, 5,   204, 5,   1,   6,   54,  6,   107, 6,   160, 6,  213, 6,   10,
       7,   63,  7,   116, 7,   169, 7,   222, 7,   19,  8,   72, 8,   125, 8,
       178, 8,   231, 8,   28,  9,   81,  9,   134, 9,   187, 9,  240, 9,   37,
       10,  90,  10,  143, 10,  196, 10,  249, 10,  46,  11,  99, 11,  152, 11,
       205, 11,  2,   12,  55,  12,  108, 12,  161, 12,  214, 12}};
  auto frame = Assemble(wire);
  Values values{};
  assert(ParseTravel(frame, values));
  for (unsigned i = 0; i < 63; ++i)
    assert(values[i] == i * 53);
  auto bad = wire;
  bad[3] ^= 1;
  Frame invalid;
  for (unsigned part = 0; part < 3; ++part) {
    Report r{};
    for (unsigned i = 0; i < 64 && part * 64 + i < bad.size(); ++i)
      r[i + 1] = bad[part * 64 + i];
    const bool ok = invalid.Push(r, 65, 0x12);
    assert(ok == (part < 2));
  }
  assert(!ParseTravel(invalid, values));
  Frame partial;
  Report r{};
  std::copy_n(wire.begin(), 64, r.begin() + 1);
  assert(partial.Push(r, 65, 0x12));
  assert(!partial.Complete() && !ParseTravel(partial, values));
  Frame wrong;
  assert(!wrong.Push(r, 64, 0x12));
  wrong = {};
  r[0] = 1;
  assert(!wrong.Push(r, 65, 0x12));
  wrong = {};
  r[0] = 0;
  r[3] = 0xa3;
  assert(!wrong.Push(r, 65, 0x12));
  // Actual firmware command 23 emits fourteen entries, including invalid
  // padding.
  {
    const Keys keys = {
        {41, 58, 59, 60, 61, 62, 63, 64, 65, 66, 67, 68, 69, 70}};
    const std::array<std::uint8_t, 61> wireMap = {
        {92, 57, 163, 109, 0, 41, 0, 41, 0, 58, 0, 58, 0, 59, 0, 59,
         0,  60, 0,   60,  0, 61, 0, 61, 0, 62, 0, 62, 0, 63, 0, 63,
         0,  64, 0,   64,  0, 65, 0, 65, 0, 66, 0, 66, 0, 67, 0, 67,
         0,  68, 0,   68,  0, 69, 0, 69, 0, 70, 0, 70, 0}};
    Report r{};
    std::copy(wireMap.begin(), wireMap.end(), r.begin() + 1);
    Frame f;
    assert(f.Push(r, 65, 0x23));
    Assignments values{};
    assert(ParseLayout(f, keys, values));
    for (std::size_t i = 0; i < keys.size(); ++i)
      if (keys[i])
        assert(values[i] == (keys[i] == 1 ? kFn : keys[i]));
    auto wrong = keys;
    wrong[0] = 255;
    assert(!ParseLayout(f, wrong, values));
  }
  {
    const Keys keys = {
        {76, 53, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 45, 46}};
    const std::array<std::uint8_t, 61> wireMap = {
        {92, 57, 163, 109, 0, 76, 0, 76, 0, 53, 0, 53, 0, 30, 0, 30,
         0,  31, 0,   31,  0, 32, 0, 32, 0, 33, 0, 33, 0, 34, 0, 34,
         0,  35, 0,   35,  0, 36, 0, 36, 0, 37, 0, 37, 0, 38, 0, 38,
         0,  39, 0,   39,  0, 45, 0, 45, 0, 46, 0, 46, 0}};
    Report r{};
    std::copy(wireMap.begin(), wireMap.end(), r.begin() + 1);
    Frame f;
    assert(f.Push(r, 65, 0x23));
    Assignments values{};
    assert(ParseLayout(f, keys, values));
    for (std::size_t i = 0; i < keys.size(); ++i)
      if (keys[i])
        assert(values[i] == (keys[i] == 1 ? kFn : keys[i]));
    auto wrong = keys;
    wrong[0] = 255;
    assert(!ParseLayout(f, wrong, values));
  }
  {
    const Keys keys = {{42, 73, 43, 20, 26, 8, 21, 23, 28, 24, 12, 18, 19, 47}};
    const std::array<std::uint8_t, 61> wireMap = {
        {92, 57, 163, 109, 0, 42, 0, 42, 0, 73, 0, 73, 0, 43, 0, 43,
         0,  20, 0,   20,  0, 26, 0, 26, 0, 8,  0, 8,  0, 21, 0, 21,
         0,  23, 0,   23,  0, 28, 0, 28, 0, 24, 0, 24, 0, 12, 0, 12,
         0,  18, 0,   18,  0, 19, 0, 19, 0, 47, 0, 47, 0}};
    Report r{};
    std::copy(wireMap.begin(), wireMap.end(), r.begin() + 1);
    Frame f;
    assert(f.Push(r, 65, 0x23));
    Assignments values{};
    assert(ParseLayout(f, keys, values));
    for (std::size_t i = 0; i < keys.size(); ++i)
      if (keys[i])
        assert(values[i] == (keys[i] == 1 ? kFn : keys[i]));
    auto wrong = keys;
    wrong[0] = 255;
    assert(!ParseLayout(f, wrong, values));
  }
  {
    const Keys keys = {{48, 49, 75, 57, 4, 22, 7, 9, 10, 11, 13, 14, 15, 51}};
    const std::array<std::uint8_t, 61> wireMap = {
        {92, 57, 163, 109, 0, 48, 0, 48, 0, 49, 0, 49, 0, 75, 0, 75,
         0,  57, 0,   57,  0, 4,  0, 4,  0, 22, 0, 22, 0, 7,  0, 7,
         0,  9,  0,   9,   0, 10, 0, 10, 0, 11, 0, 11, 0, 13, 0, 13,
         0,  14, 0,   14,  0, 15, 0, 15, 0, 51, 0, 51, 0}};
    Report r{};
    std::copy(wireMap.begin(), wireMap.end(), r.begin() + 1);
    Frame f;
    assert(f.Push(r, 65, 0x23));
    Assignments values{};
    assert(ParseLayout(f, keys, values));
    for (std::size_t i = 0; i < keys.size(); ++i)
      if (keys[i])
        assert(values[i] == (keys[i] == 1 ? kFn : keys[i]));
    auto wrong = keys;
    wrong[0] = 255;
    assert(!ParseLayout(f, wrong, values));
  }
  {
    const Keys keys = {{52, 40, 78, 225, 29, 27, 6, 25, 5, 17, 16, 54, 55, 56}};
    const std::array<std::uint8_t, 61> wireMap = {
        {92, 57,  163, 109, 0, 52, 0, 52, 0, 40, 0, 40, 0, 78, 0, 78,
         0,  225, 0,   225, 0, 29, 0, 29, 0, 27, 0, 27, 0, 6,  0, 6,
         0,  25,  0,   25,  0, 5,  0, 5,  0, 17, 0, 17, 0, 16, 0, 16,
         0,  54,  0,   54,  0, 55, 0, 55, 0, 56, 0, 56, 0}};
    Report r{};
    std::copy(wireMap.begin(), wireMap.end(), r.begin() + 1);
    Frame f;
    assert(f.Push(r, 65, 0x23));
    Assignments values{};
    assert(ParseLayout(f, keys, values));
    for (std::size_t i = 0; i < keys.size(); ++i)
      if (keys[i])
        assert(values[i] == (keys[i] == 1 ? kFn : keys[i]));
    auto wrong = keys;
    wrong[0] = 255;
    assert(!ParseLayout(f, wrong, values));
  }
  {
    const Keys keys = {{229, 82, 224, 227, 226, 44, 230, 1, 80, 81, 79}};
    const std::array<std::uint8_t, 61> wireMap = {
        {92,  57,  163, 109, 0,   229, 0,   229, 0,   82, 0,   82,  0,
         224, 0,   224, 0,   227, 0,   227, 0,   226, 0,  226, 0,   44,
         0,   44,  0,   230, 0,   230, 0,   1,   0,   1,  240, 80,  0,
         80,  0,   81,  0,   81,  0,   79,  0,   79,  0,  255, 255, 255,
         0,   255, 255, 255, 0,   255, 255, 255, 0}};
    Report r{};
    std::copy(wireMap.begin(), wireMap.end(), r.begin() + 1);
    Frame f;
    assert(f.Push(r, 65, 0x23));
    Assignments values{};
    assert(ParseLayout(f, keys, values));
    for (std::size_t i = 0; i < keys.size(); ++i)
      if (keys[i])
        assert(values[i] == (keys[i] == 1 ? kFn : keys[i]));
    auto wrong = keys;
    wrong[0] = 255;
    assert(!ParseLayout(f, wrong, values));
  }
  {
    const std::array<std::uint8_t, 49> factory = {
        {92, 45, 171, 105, 0,  0,  41, 58, 59, 60, 61, 62, 63, 64, 65, 66, 67,
         68, 69, 70,  76,  0,  0,  0,  0,  0,  0,  1,  53, 30, 31, 32, 33, 34,
         35, 36, 37,  38,  39, 45, 46, 42, 73, 0,  0,  0,  0,  0,  0}};
    Report r{};
    std::copy(factory.begin(), factory.end(), r.begin() + 1);
    Frame f;
    assert(f.Push(r, 65, 0x2b));
    assert(MatchFactory(f, 0));
  }
  {
    const std::array<std::uint8_t, 49> factory = {
        {92, 45, 171, 105, 0,  2,  43, 20, 26, 8, 21, 23, 28, 24, 12, 18, 19,
         47, 48, 49,  75,  0,  0,  0,  0,  0,  0, 3,  57, 4,  22, 7,  9,  10,
         11, 13, 14,  15,  51, 52, 0,  40, 78, 0, 0,  0,  0,  0,  0}};
    Report r{};
    std::copy(factory.begin(), factory.end(), r.begin() + 1);
    Frame f;
    assert(f.Push(r, 65, 0x2b));
    assert(MatchFactory(f, 2));
  }
  {
    const std::array<std::uint8_t, 49> factory = {
        {92, 45, 171, 105, 0,   4, 225, 0,  29, 27, 6, 25, 5,
         17, 16, 54,  55,  56,  0, 229, 82, 0,  0,  0, 0,  0,
         0,  5,  224, 227, 226, 0, 0,   0,  44, 0,  0, 0,  230,
         1,  80, 81,  79,  0,   0, 0,   0,  0,  0}};
    Report r{};
    std::copy(factory.begin(), factory.end(), r.begin() + 1);
    Frame f;
    assert(f.Push(r, 65, 0x2b));
    assert(MatchFactory(f, 4));
  }
  std::cout << "MG75_PRO_PROTOCOL=PASS firmware_fixture padding partial "
               "checksum echo identity Fn\n";
}

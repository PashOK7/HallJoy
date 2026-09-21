#include "../src/HallJoyProject/HallJoy/ajazz_raw_limits.h"
#include <cassert>
#include <iostream>
using halljoy::ajazz::DynamicLimits;
static unsigned Sample(DynamicLimits& m,unsigned p,unsigned raw) {
    unsigned v=0;for(unsigned i=0;i<3;++i)m.Raw(p,raw,0,v);return v;
}
int main() {
    DynamicLimits m;unsigned value=999;
    assert(m.Raw(68,2400,0,value) && value==0);
    assert(m.Ready()==1);
    assert(Sample(m,68,1876)>=499 && Sample(m,68,1876)<=501);
    assert(Sample(m,68,1200)==1000 && m.Inspect(68).full==1200);
    assert(Sample(m,69,2500)==0);
    assert(Sample(m,69,1355)==1000);
    // Release from raw alone; another held key remains fully pressed.
    assert(Sample(m,68,2400)==0 && Sample(m,69,1355)==1000);
    assert(Sample(m,68,2500)==0 && m.Inspect(68).rest==2500);
    assert(Sample(m,121,2400)==0 && Sample(m,121,1289)==1000);
    assert(halljoy::ajazz::HostCode(250)==1033);
    assert(!m.Raw(131,2400,0,value));
    const auto before=m.Inspect(68);
    m.Raw(68,60000,0,value);m.Raw(68,2500,0,value);m.Raw(68,2500,0,value);
    assert(m.Inspect(68).rest==before.rest);
    m.Raw(68,1,0,value);m.Raw(68,2500,0,value);m.Raw(68,2500,0,value);
    assert(m.Inspect(68).full==before.full);
    assert(!m.Raw(68,0,0,value) && value==0);
    DynamicLimits held;
    assert(!held.Raw(68,1200,0,value) && value==0);
    assert(Sample(held,68,2400)==0 && held.Inspect(68).ready);
    assert(Sample(held,68,1200)==1000);
    std::cout<<"AJAZZ_DYNAMIC_LIMITS_TEST=PASS immediate_input=1 per_key=1 release=1 fn=1 outliers=1 held_start_recovery=1\n";
}

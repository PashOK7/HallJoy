#include "keychron_onboard_precision.h"
#include <cassert>
#include <algorithm>
#include <set>
#include <iostream>
static double polynomial(double x) {
    return 426.88962-.48358*x+2.04637e-4*x*x-2.99368e-8*x*x*x;
}
int main() {
    unsigned checked=0;
    // Independent double-precision reference, many zero offsets and spans.
    for(unsigned zero=2800;zero<=3300;zero+=100) {
        for(unsigned span=800;span<=1200;span+=50) {
            const float scale=float(40./(polynomial(3121-span)-polynomial(3121)));
            float previous=0;std::set<int> codes;
            for(unsigned raw=zero;raw>=zero-span;--raw) {
                const auto value=hjk4_precise_depth(raw,zero,scale,3121,-.48358f,2.04637e-4f,-2.99368e-8f);
                const auto reference=std::clamp((polynomial(double(raw)-zero+3121)-polynomial(3121))*scale/40.,0.,1.);
                assert(std::isfinite(value) && value>=previous && value<=1);
                assert(std::abs(value-reference)<.00001);
                codes.insert(int(std::lround(value*32767.f)));previous=value;++checked;
            }
            assert(previous>.99999f && codes.size()==span+1);
            assert(hjk4_precise_depth(zero+1,zero,scale,3121,-.48358f,2.04637e-4f,-2.99368e-8f)==0);
        }
    }
    assert(hjk4_precise_depth(2000,3000,NAN,3121,-.48358f,2.04637e-4f,-2.99368e-8f)==0);
    assert(hjk4_precise_depth(2000,3000,-1,3121,-.48358f,2.04637e-4f,-2.99368e-8f)==0);
    std::cout<<"Precise depth monotonicity, every ADC code, endpoints and reference PASS: "<<checked<<" samples\n";
}

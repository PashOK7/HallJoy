#include "keychron_onboard_compact.h"
#include <array>
#include <cassert>
#include <iostream>
int main(){
    std::array<uint8_t,114> travel{};std::array<uint16_t,114> out{};
    std::array<uint8_t,HJK4_COMPACT_BYTES> frame{};
    for(unsigned value=0;value<=240;++value){
        travel.fill(static_cast<uint8_t>(value));hjk4_receiver r{7,0,0,0};
        hjk4_compact_encode(frame.data(),7,value+1,2345,HJK4_CALIBRATED,travel.data());
        assert(hjk4_compact_accept(&r,frame.data(),frame.size(),out.data()));
        for(auto depth:out)assert(depth==value*65535u/240u);
        assert(!hjk4_compact_accept(&r,frame.data(),frame.size(),out.data()));
    }
    const auto saved=out;
    for(unsigned i=0;i<frame.size();++i){
        auto bad=frame;bad[i]^=1;hjk4_receiver r{7,0,0,0};
        assert(!hjk4_compact_accept(&r,bad.data(),bad.size(),out.data()) && out==saved && !r.have_sequence);
    }
    for(unsigned n=0;n<frame.size();++n){hjk4_receiver r{7,0,0,0};assert(!hjk4_compact_accept(&r,frame.data(),n,out.data()));}
    frame[12]=241;hjk4_put32(frame.data()+126,hjk4_crc32(frame.data(),126));hjk4_receiver r{7,0,0,0};
    assert(!hjk4_compact_accept(&r,frame.data(),frame.size(),out.data()) && out==saved);
    std::cout<<"Compact travel precision, corruption, truncation, ordering PASS\n";
}

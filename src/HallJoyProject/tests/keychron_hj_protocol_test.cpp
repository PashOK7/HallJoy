#include "../HallJoy/keychron_hj_protocol.h"
#include <array>
#include <cassert>
#include <iostream>

int main() {
    assert(hjk4_crc32(reinterpret_cast<const uint8_t*>("123456789"),9)==0xcbf43926u);
    std::array<uint16_t,HJK4_SLOTS> input{}, output{};
    for (size_t i=0;i<input.size();++i) input[i]=static_cast<uint16_t>(i*577);
    input[0]=0; input[113]=65535;
    std::array<uint8_t,HJK4_FRAME_BYTES> frame{};
    assert(hjk4_encode(frame.data(),frame.size(),HJK4_DEPTH_FRAME,42,17,12345,980,HJK4_CALIBRATED,input.data()));
    hjk4_receiver state{42,0,0,0};
    assert(hjk4_accept(&state,frame.data(),frame.size(),output.data()));
    assert(input==output && state.sequence==17 && state.missing_scans==0);
    assert(!hjk4_accept(&state,frame.data(),frame.size(),output.data()));
    for (size_t bit=0;bit<frame.size()*8;++bit) {
        auto damaged=frame; damaged[bit/8]^=static_cast<uint8_t>(1u<<(bit%8));
        hjk4_receiver fresh{42,0,0,0}; output.fill(0x1234);
        assert(!hjk4_accept(&fresh,damaged.data(),damaged.size(),output.data()));
        assert(!fresh.have_sequence && output[0]==0x1234 && output[113]==0x1234);
    }
    for (size_t length=0;length<frame.size();++length) {
        hjk4_receiver fresh{42,0,0,0};
        assert(!hjk4_accept(&fresh,frame.data(),length,output.data()));
    }
    // Recompute CRC after semantic damage: these exercise field validation,
    // independently of the checksum rejection tested above.
    for (const auto field : {4u,5u,6u,7u,8u,20u,21u,22u,23u}) {
        auto damaged=frame;
        if (field==20u || field==21u) { damaged[20]=0; damaged[21]=0; }
        else if (field==23u) damaged[field]=0x81;
        else damaged[field]^=0x40;
        hjk4_put32(damaged.data()+252,hjk4_crc32(damaged.data(),252));
        hjk4_receiver fresh{42,0,0,0};
        assert(!hjk4_accept(&fresh,damaged.data(),damaged.size(),output.data()));
    }
    hjk4_receiver other{43,0,0,0};
    assert(!hjk4_accept(&other,frame.data(),frame.size(),output.data()));
    assert(hjk4_encode(frame.data(),frame.size(),HJK4_DEPTH_FRAME,42,20,15000,1100,HJK4_CALIBRATED|HJK4_OVERRUN,input.data()));
    assert(hjk4_accept(&state,frame.data(),frame.size(),output.data()));
    assert(state.missing_scans==2); // skipped measurements reported, not fabricated
    assert(hjk4_encode(frame.data(),frame.size(),HJK4_DEPTH_FRAME,42,19,16000,980,HJK4_CALIBRATED,input.data()));
    assert(!hjk4_accept(&state,frame.data(),frame.size(),output.data()));
    state.sequence=UINT32_MAX;
    assert(hjk4_encode(frame.data(),frame.size(),HJK4_DEPTH_FRAME,42,0,0,980,HJK4_CALIBRATED,input.data()));
    assert(hjk4_accept(&state,frame.data(),frame.size(),output.data()));
    state.have_sequence=0;
    assert(hjk4_encode(frame.data(),frame.size(),HJK4_RAW_FRAME,42,1,1000,980,HJK4_CALIBRATED,input.data()));
    assert(!hjk4_accept(&state,frame.data(),frame.size(),output.data()));
    assert(hjk4_encode(frame.data(),frame.size(),HJK4_DEPTH_FRAME,42,1,1000,980,0,input.data()));
    assert(!hjk4_accept(&state,frame.data(),frame.size(),output.data()));
    assert(!hjk4_encode(frame.data(),255,HJK4_DEPTH_FRAME,42,1,0,980,HJK4_CALIBRATED,input.data()));
    std::cout << "KEYCHRON_HJ_PROTOCOL_TEST=PASS corruption_bits=2048 truncated_lengths=256 sequence_wrap=1\n";
}

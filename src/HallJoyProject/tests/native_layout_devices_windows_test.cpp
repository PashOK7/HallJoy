#include "native_layout_devices.h"
#include <windows.h>
#include <cassert>
#include <iostream>
#include <thread>
#include <vector>
int main() {
    assert(NativeLayoutDevices_Query(nullptr,0)==0);
    assert(NativeLayoutDevices_Query(nullptr,1)==-2);
    const std::uint32_t absent=0xffffffffu;
    std::vector<std::thread> threads;
    for(int i=0;i<4;++i) threads.emplace_back([&]{
        for(int j=0;j<50;++j) {
            if(j%10==0) NativeLayoutDevices_Invalidate();
            assert((NativeLayoutDevices_QueryFrozen() & ~127u)==0);
            const auto value=NativeLayoutDevices_Query(&absent,1);
            assert(value==0 || value==-1 || value==-2);
        }
    });
    for(auto& thread:threads) thread.join();
    int value=-1;
    const auto start=GetTickCount64();
    while(value==-1 && GetTickCount64()-start<5000) {
        value=NativeLayoutDevices_Query(&absent,1);Sleep(5);
    }
    assert(value==0 || value==-2);
    assert(NativeLayoutDevices_Query(nullptr,0)==0);
    std::cout << "NATIVE_LAYOUT_DEVICES_WINDOWS_TEST=PASS async_metadata_only=1 concurrent_refresh=1\n";
}

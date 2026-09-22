// Read-only Windows controller/timer probe of the actual tester Sample path.
// Deliberately no GUI, device writes, profile changes or synthetic input.
#include "../src/HallJoyProject/HallJoy/gamepad_latency.cpp"
#include <iostream>
int main(){
 winrt::init_apartment(winrt::apartment_type::multi_threaded);
 auto sample=std::make_unique<State>();
 for(int i=0;i<40;++i){auto pads=Gamepad::Gamepads();if(pads.Size()==1){sample->selected=pads.GetAt(0);break;}if(pads.Size()>1){std::cerr<<"Select unique controller for probe\n";return 2;}Sleep(50);}
 if(!sample->selected){std::cerr<<"No controller; no hardware timing claim\n";return 3;}
 HANDLE timer=CreateWaitableTimerExW(nullptr,nullptr,CREATE_WAITABLE_TIMER_HIGH_RESOLUTION,TIMER_ALL_ACCESS);if(!timer)return 4;
 std::vector<double> gaps;double readMax=0;
 for(unsigned i=0;i<2000;++i){sample->Sample();if(!sample->current.connected){CloseHandle(timer);return 5;}if(i)gaps.push_back(sample->lastGapMs);readMax=std::max(readMax,sample->lastReadMs);LARGE_INTEGER due{};due.QuadPart=-5000;if(!SetWaitableTimer(timer,&due,0,nullptr,nullptr,FALSE)||WaitForSingleObject(timer,1000)!=WAIT_OBJECT_0){CloseHandle(timer);return 6;}}
 CloseHandle(timer);std::sort(gaps.begin(),gaps.end());
 std::cout<<"samples=2000 gap_median_ms="<<gaps[gaps.size()/2]<<" gap_p95_ms="<<gaps[gaps.size()*95/100]<<" gap_max_ms="<<gaps.back()<<" read_max_ms="<<readMax<<" changes="<<sample->count<<" rendering_not_tested=1\n";
 return 0;
}

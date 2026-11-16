#include "../HallJoy/analog_host_observation_parent.h"
#include "../HallJoy/midi_observation_engine.h"
#include <cassert>
#include <cstdlib>
#include <iostream>
#include <vector>
using namespace halljoy;
using namespace halljoy::observation;
using namespace halljoy::observation::host_history;
void SupportLog_Event(const char*,std::uint64_t,SupportLogDetail) noexcept {}
constexpr std::uint64_t nonce=0x824357980213ab43ull;

static int ChildMain(HANDLE control) {
    auto* shared=static_cast<HallJoyAnalogHost::SharedState*>(MapViewOfFile(control,FILE_MAP_ALL_ACCESS,0,0,sizeof(HallJoyAnalogHost::SharedState)));
    if(!shared || shared->ownerPid==0 || Read(shared->launchNonce)!=nonce || shared->hostPid!=static_cast<LONG>(GetCurrentProcessId())) return 20;
    Child history;
    const auto deadline=GetTickCount64()+10000;
    while(!Read(shared->observationHistoryGeneration) && GetTickCount64()<deadline) Sleep(1);
    history.Refresh(*shared);
    SnapshotValues values{};SnapshotValidity valid;valid.set(4);valid.set(5);valid.set(0x409);
    std::uint64_t timestamp=100000;
    auto publish=[&] {
        timestamp+=10000;
        history.Capture(*shared,[&](auto put){put(10,values,timestamp,0,&valid);});
    };
    publish();
    // Every complete chord is published before the parent starts draining.
    for(unsigned i=0;i<100;++i) {values[4]=500;values[5]=600;values[0x409]=700;publish();values.fill(0);publish();}
    InterlockedExchange64(&shared->totalSuccessfulPolls,1);
    auto seenGeneration=Read(shared->observationHistoryGeneration);
    while(InterlockedCompareExchange(&shared->observationCaptureWanted,0,0) && GetTickCount64()<deadline) {
        history.Refresh(*shared);
        const auto generation=Read(shared->observationHistoryGeneration);
        if(generation && generation!=seenGeneration) {
            publish();seenGeneration=generation;InterlockedExchange64(&shared->totalPolls,static_cast<LONG64>(generation));
        }
        const auto command=InterlockedExchange(&shared->diagnosticCrashAfterPolls,0);
        if(command==1) {
            for(unsigned i=0;i<40000;++i) {values[4]=i%2?500:0;publish();}
            values[4]=500;publish();InterlockedExchange64(&shared->totalSuccessfulPolls,2);
        } else if(command==2) {
            values.fill(0);publish();values[4]=600;publish();values.fill(0);publish();
            InterlockedExchange64(&shared->totalSuccessfulPolls,4);
        }
        Sleep(1);
    }
    history.Refresh(*shared);
    unsigned disabledScans=0;
    for(unsigned i=0;i<1000;++i) history.Capture(*shared,[&](auto){++disabledScans;});
    UnmapViewOfFile(shared);CloseHandle(control);
    return disabledScans ? 21 : 0;
}
static void DirectWireChecks() {
    Mapping owner;assert(owner.Create(GetCurrentProcessId(),nonce,1,GetCurrentProcessId()));
    HANDLE transferred=nullptr;
    assert(DuplicateHandle(GetCurrentProcess(),owner.Handle(),GetCurrentProcess(),&transferred,0,FALSE,DUPLICATE_SAME_ACCESS));
    Mapping rejected;assert(!rejected.Adopt(transferred,GetCurrentProcessId(),nonce^1,1,GetCurrentProcessId()));
    assert(DuplicateHandle(GetCurrentProcess(),owner.Handle(),GetCurrentProcess(),&transferred,0,FALSE,DUPLICATE_SAME_ACCESS));
    Mapping writer;assert(writer.Adopt(transferred,GetCurrentProcessId(),nonce,1,GetCurrentProcessId()));
    Event e{};e.source=1;e.connection=1;e.physical=e.hid=4;e.timeUs=100;e.timing=Timing::HostSnapshot;
    for(unsigned i=0;i<kCapacity;++i) {e.milli=static_cast<std::uint16_t>(i%1001);assert(writer.Push(e));}
    assert(!writer.Push(e) && Read(owner.Get()->lost)==1);
    unsigned count=0;assert(owner.Drain([&](const Event& sample){assert(sample.sequence==++count);assert(sample.milli==(count-1)%1001);})==kCapacity);
    assert(writer.Push(e));assert(owner.Drain([](auto){})==1);
    // A child can observe a superseded tag while atomically claiming the next
    // transfer. The immutable handle generation must survive the publication
    // window, without consuming a future transfer as an invalid old request.
    Mapping pending;assert(pending.Create(GetCurrentProcessId(),nonce,3,GetCurrentProcessId()));
    assert(DuplicateHandle(GetCurrentProcess(),pending.Handle(),GetCurrentProcess(),&transferred,0,FALSE,DUPLICATE_SAME_ACCESS));
    auto control=std::make_unique<HallJoyAnalogHost::SharedState>();
    control->ownerPid=GetCurrentProcessId();control->launchNonce=nonce;control->observationCaptureWanted=1;
    control->observationHistoryGeneration=2;control->observationHistoryHandle=reinterpret_cast<std::uintptr_t>(transferred);
    Child child;child.Refresh(*control);assert(pending.Get()->ready==1);
    unsigned scans=0;auto visit=[&](auto){++scans;};
    child.Capture(*control,visit);assert(scans==0);
    child.Refresh(*control);control->observationHistoryGeneration=0;child.Refresh(*control);
    control->observationHistoryGeneration=3;child.Refresh(*control);child.Capture(*control,visit);assert(scans==1);
    control->observationCaptureWanted=0;child.Refresh(*control);child.Capture(*control,visit);assert(scans==1);
    // Malformed cells cannot index the parent tracker.
    e.hid=0xffff;assert(writer.Push(e));unsigned accepted=0;
    owner.Drain([&](auto){++accepted;});assert(accepted==0 && Read(owner.Get()->lost)==1 && Read(owner.Get()->malformed)==1);
}
int main(int argc,char** argv) {
    if(argc==3 && std::string(argv[1])=="--child") return ChildMain(reinterpret_cast<HANDLE>(static_cast<std::uintptr_t>(std::strtoull(argv[2],nullptr,10))));
    DirectWireChecks();
    SECURITY_ATTRIBUTES security{sizeof(SECURITY_ATTRIBUTES),nullptr,TRUE};
    HANDLE control=CreateFileMappingW(INVALID_HANDLE_VALUE,&security,PAGE_READWRITE,0,sizeof(HallJoyAnalogHost::SharedState),nullptr);assert(control);
    auto* shared=static_cast<HallJoyAnalogHost::SharedState*>(MapViewOfFile(control,FILE_MAP_ALL_ACCESS,0,0,sizeof(HallJoyAnalogHost::SharedState)));assert(shared);
    new(shared) HallJoyAnalogHost::SharedState{};shared->ownerPid=static_cast<LONG>(GetCurrentProcessId());shared->launchNonce=nonce;
    HANDLE job=CreateJobObjectW(nullptr,nullptr);assert(job);
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limit{};limit.BasicLimitInformation.LimitFlags=JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    assert(SetInformationJobObject(job,JobObjectExtendedLimitInformation,&limit,sizeof(limit)));
    wchar_t exe[32768]{};assert(GetModuleFileNameW(nullptr,exe,32768));
    std::wstring command=L"\""+std::wstring(exe)+L"\" --child "+std::to_wstring(reinterpret_cast<std::uintptr_t>(control));
    STARTUPINFOEXW startup{};startup.StartupInfo.cb=sizeof(startup);
    SIZE_T bytes=0;InitializeProcThreadAttributeList(nullptr,1,0,&bytes);
    std::vector<unsigned char> attrs(bytes);startup.lpAttributeList=reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(attrs.data());
    assert(InitializeProcThreadAttributeList(startup.lpAttributeList,1,0,&bytes));
    assert(UpdateProcThreadAttribute(startup.lpAttributeList,0,PROC_THREAD_ATTRIBUTE_HANDLE_LIST,&control,sizeof(control),nullptr,nullptr));
    PROCESS_INFORMATION process{};
    assert(CreateProcessW(exe,command.data(),nullptr,nullptr,TRUE,CREATE_SUSPENDED|CREATE_NO_WINDOW|EXTENDED_STARTUPINFO_PRESENT,nullptr,nullptr,&startup.StartupInfo,&process));
    DeleteProcThreadAttributeList(startup.lpAttributeList);
    assert(AssignProcessToJobObject(job,process.hProcess));shared->hostPid=static_cast<LONG>(process.dwProcessId);
    auto journal=std::make_shared<Journal>();current.store(journal);enabled=true;
    auto tracker=std::make_shared<SnapshotTracker>();
    {
        // A history-specific access/identity failure retains the snapshot route
        // instead of vetoing analog or gamepad startup.
        {auto fallback=std::make_shared<SnapshotTracker>();Parent unavailable(*shared,nullptr);assert(!unavailable.Update(fallback,process.dwProcessId));assert(!Read(shared->observationHistoryGeneration));}
        Parent parent(*shared,job);
        DWORD before=0,after=0;assert(GetProcessHandleCount(process.hProcess,&before));
        assert(parent.Update(nullptr,process.dwProcessId));
        assert(GetProcessHandleCount(process.hProcess,&after) && before==after);
        shared->observationCaptureWanted=1;
        assert(parent.Update(tracker,process.dwProcessId));
        assert(Read(shared->observationHistoryHandle)!=0);
        assert(GetProcessHandleCount(process.hProcess,&after) && after==before+1);
        // Stop before the suspended child can claim the transfer: exact remote
        // handle ownership must be reclaimed, without a leaked section handle.
        assert(parent.Update(nullptr,process.dwProcessId));
        assert(!Read(shared->observationHistoryHandle));
        assert(GetProcessHandleCount(process.hProcess,&after) && after==before);
        assert(parent.Update(tracker,process.dwProcessId));
        ResumeThread(process.hThread);
        const auto deadline=GetTickCount64()+10000;
        while(!Read(shared->totalSuccessfulPolls) && GetTickCount64()<deadline) Sleep(1);
        assert(Read(shared->totalSuccessfulPolls)==1);
        // Deliberately no bridge consumption until the complete motions ended.
        assert(parent.Update(tracker,process.dwProcessId));
        midi::Profile config;config.enabled=true;config.depthPressure=false;config.keys[4]=61;config.keys[5]=62;config.keys[0x409]=63;
        midi::ObservationEngine engine;std::vector<std::uint32_t> messages;
        auto emit=[&](std::uint32_t m){messages.push_back(m);};
        auto consume=[&] {
        engine.BeginFrame();
        struct Context {midi::ObservationEngine* engine;midi::Profile* config;decltype(emit)* emitFn;} context{&engine,&config,&emit};
        tracker->Visit({&context,+[](void* opaque,const Event& e){auto& c=*static_cast<Context*>(opaque);c.engine->Route(e,*c.config,*c.emitFn);}});
        std::array<Event,1024> batch{};
        for(auto count=journal->Drain(batch.data(),batch.size());count;count=journal->Drain(batch.data(),batch.size()))
            for(std::size_t i=0;i<count;++i) engine.Replay(batch[i],config,emit);
        engine.EndFrame(config,emit);
        };
        consume();
        assert(messages.size()==600 && journal->lost==0 && tracker->gaps==0);
        for(unsigned a=60;a<63;++a) assert(!engine.Notes()[a].velocity);
        // Overflow the actual cross-process ring while the bridge is asleep.
        // The parent must report loss, replace the stream and suppress its held
        // baseline, then accept the next deliberate released/rearmed attack.
        InterlockedExchange(&shared->diagnosticCrashAfterPolls,1);
        while(Read(shared->totalSuccessfulPolls)!=2 && GetTickCount64()<deadline) Sleep(1);
        assert(Read(shared->totalSuccessfulPolls)==2);
        assert(parent.Update(tracker,process.dwProcessId));assert(journal->lost>0 && tracker->gaps>0);
        engine.Reset(emit);journal->Discard();messages.clear();
        assert(parent.Update(tracker,process.dwProcessId));
        const auto replacement=Read(shared->observationHistoryGeneration);assert(replacement);
        while(Read(shared->totalPolls)!=replacement && GetTickCount64()<deadline) Sleep(1);
        assert(Read(shared->totalPolls)==replacement);
        assert(parent.Update(tracker,process.dwProcessId));consume();assert(messages.empty());
        InterlockedExchange(&shared->diagnosticCrashAfterPolls,2);
        while(Read(shared->totalSuccessfulPolls)!=4 && GetTickCount64()<deadline) Sleep(1);
        assert(Read(shared->totalSuccessfulPolls)==4);
        assert(parent.Update(tracker,process.dwProcessId));consume();assert(messages.size()==2);
        shared->observationCaptureWanted=0;enabled=false;
        assert(parent.Update(nullptr,process.dwProcessId));
    }
    assert(WaitForSingleObject(process.hProcess,10000)==WAIT_OBJECT_0);
    DWORD exit=1;assert(GetExitCodeProcess(process.hProcess,&exit) && exit==0);
    current.store(nullptr);tracker.reset();journal.reset();
    CloseHandle(process.hThread);CloseHandle(process.hProcess);CloseHandle(job);UnmapViewOfFile(shared);CloseHandle(control);
    std::cout<<"MIDI_OBSERVATION_HOST_WINDOWS_TEST=PASS received_chords extended_codes delayed_bridge unnamed_handle_transfer unclaimed_cleanup overflow loss_replacement held_suppression_rearm invalid_generation transfer_race malformed zero_disabled fake_child_only\n";
}

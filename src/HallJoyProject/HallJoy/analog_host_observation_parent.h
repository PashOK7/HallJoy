#pragma once
#include "analog_host_observation_history.h"
#include "support_log.h"

namespace halljoy::observation::host_history {
inline std::atomic<std::uint64_t> nextGeneration{1};
class Parent {
    Mapping mapping_;
    std::weak_ptr<SnapshotTracker> tracker_;
    HallJoyAnalogHost::SharedState& shared_;
    DWORD host_=0;
    HANDLE job_=nullptr,child_=nullptr;
    std::uint64_t generation_=0,startedMs_=0;
    bool failed_=false,needsReset_=false;
    void Close() noexcept {
        InterlockedExchange64(&shared_.observationHistoryGeneration,0);
        const auto unclaimed=reinterpret_cast<HANDLE>(static_cast<std::uintptr_t>(InterlockedExchange64(&shared_.observationHistoryHandle,0)));
        if(unclaimed && child_) {
            HANDLE local=nullptr;
            if(DuplicateHandle(child_,unclaimed,GetCurrentProcess(),&local,0,FALSE,DUPLICATE_SAME_ACCESS|DUPLICATE_CLOSE_SOURCE)) CloseHandle(local);
        }
        if(child_) CloseHandle(child_);
        child_=nullptr;
        mapping_.Close();generation_=0;
    }
    void Loss(const std::shared_ptr<SnapshotTracker>& tracker) {
        const auto lost=Read(mapping_.Get()->lost),invalid=Read(mapping_.Get()->malformed);
        ++tracker->gaps;
        if(auto journal=current.load(std::memory_order_acquire)) {journal->lost.fetch_add(lost);journal->malformed.fetch_add(invalid);}
        SupportLog_Event("midi.uap_history_loss",lost,SupportLog_Data(invalid));
        needsReset_=true;Close();
    }
public:
    Parent(HallJoyAnalogHost::SharedState& shared,HANDLE job):shared_(shared),job_(job) {}
    ~Parent(){Close();}
    // true means the ordered route owns this session, including its brief
    // startup handshake; false selects the explicitly lower-fidelity snapshot
    // compatibility path after a concrete mapping/open/timeout failure.
    bool Update(const std::shared_ptr<SnapshotTracker>& tracker,DWORD host) {
        if(!enabled.load(std::memory_order_acquire) || !tracker || !host) {
            Close();tracker_.reset();host_=0;failed_=false;needsReset_=false;return true;
        }
        if(tracker_.lock()!=tracker || host_!=host) {
            Close();tracker_=tracker;host_=host;failed_=false;needsReset_=true;
        }
        if(needsReset_) {
            if(!tracker->ResetStream()) return true;
            needsReset_=false;
        }
        if(failed_) return false;
        if(!mapping_.Get()) {
            generation_=nextGeneration.fetch_add(1,std::memory_order_relaxed);
            child_=OpenProcess(PROCESS_DUP_HANDLE|PROCESS_QUERY_LIMITED_INFORMATION|SYNCHRONIZE,FALSE,host);
            BOOL inJob=FALSE;
            const bool childValid=child_ && job_ && IsProcessInJob(child_,job_,&inJob) && inJob &&
                static_cast<DWORD>(InterlockedCompareExchange(&shared_.hostPid,0,0))==host &&
                WaitForSingleObject(child_,0)==WAIT_TIMEOUT;
            HANDLE transferred=nullptr;
            const bool created=childValid && mapping_.Create(static_cast<DWORD>(shared_.ownerPid),Read(shared_.launchNonce),generation_,host);
            const bool duplicated=created && DuplicateHandle(GetCurrentProcess(),mapping_.Handle(),child_,&transferred,0,FALSE,DUPLICATE_SAME_ACCESS);
            if(!duplicated) {
                const auto error=childValid ? GetLastError() : ERROR_INVALID_HANDLE;failed_=true;Close();++tracker->gaps;
                SupportLog_Event("midi.uap_history_unavailable",1,SupportLog_Win32(error));return false;
            }
            startedMs_=GetTickCount64();tracker->streamMode.store(3,std::memory_order_relaxed);
            InterlockedExchange(&shared_.observationHistoryError,0);
            InterlockedExchange64(&shared_.observationHistoryHandle,static_cast<LONG64>(reinterpret_cast<std::uintptr_t>(transferred)));
            InterlockedExchange64(&shared_.observationHistoryGeneration,static_cast<LONG64>(generation_));
        }
        auto& wire=*mapping_.Get();
        const auto childError=InterlockedCompareExchange(&shared_.observationHistoryError,0,0);
        if(childError || (!InterlockedCompareExchange(&wire.ready,0,0) && GetTickCount64()-startedMs_>2000)) {
            failed_=true;Close();++tracker->gaps;
            SupportLog_Event("midi.uap_history_unavailable",2,SupportLog_Win32(childError?childError:ERROR_TIMEOUT));return false;
        }
        if(!InterlockedCompareExchange(&wire.ready,0,0)) return true;
        if(Read(wire.lost) || Read(wire.malformed)) {Loss(tracker);return true;}
        const auto connection=Read(shared_.launchNonce)^generation_^(std::uint64_t(host)<<32);
        tracker->ImportStream(connection?connection:1,[&](auto put){mapping_.Drain(put);});
        // A concurrent producer overflow or malformed record is never silently
        // accepted merely because some records were copied successfully.
        if(Read(wire.lost) || Read(wire.malformed)) Loss(tracker);
        return true;
    }
};
}

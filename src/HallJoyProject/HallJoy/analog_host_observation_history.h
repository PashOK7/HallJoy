#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <cstdio>
#include <cwchar>
#include <memory>
#include <string>
#include <type_traits>
#include "analog_observation_stream.h"
#include "analog_host_shared.h"

namespace halljoy::observation::host_history {
constexpr std::uint32_t kMagic=0x484F4A48, kVersion=1;
constexpr unsigned kCapacity=32768;
// This ABI is used only between the same x64 HallJoy image and its own child.
// Indices use Windows interlocked operations, not cross-process std::atomic.
struct alignas(64) Wire {
    std::uint32_t magic=0,version=0,bytes=0,ownerPid=0,hostPid=0;
    std::uint64_t nonce=0,generation=0;
    volatile LONG ready=0;
    alignas(64) volatile LONG64 written=0;
    alignas(64) volatile LONG64 consumed=0;
    alignas(64) volatile LONG64 lost=0,malformed=0;
    Event events[kCapacity]{};
};
static_assert(std::is_standard_layout_v<Wire> && std::is_trivially_copyable_v<Wire>);
inline std::uint64_t Read(volatile LONG64& value) noexcept {
    return static_cast<std::uint64_t>(InterlockedCompareExchange64(&value,0,0));
}
class Mapping {
    HANDLE handle_=nullptr;
    Wire* wire_=nullptr;
public:
    Mapping()=default;
    Mapping(const Mapping&)=delete;
    Mapping& operator=(const Mapping&)=delete;
    ~Mapping(){Close();}
    Wire* Get() const noexcept {return wire_;}
    void Close() noexcept {
        if(wire_) UnmapViewOfFile(wire_);
        if(handle_) CloseHandle(handle_);
        wire_=nullptr;handle_=nullptr;
    }
    HANDLE Handle() const noexcept {return handle_;}
    bool Create(DWORD owner,std::uint64_t nonce,std::uint64_t generation,DWORD host) {
        Close();
        handle_=CreateFileMappingW(INVALID_HANDLE_VALUE,nullptr,PAGE_READWRITE,0,sizeof(Wire),nullptr);
        if(!handle_) return false;
        wire_=static_cast<Wire*>(MapViewOfFile(handle_,FILE_MAP_ALL_ACCESS,0,0,sizeof(Wire)));
        if(!wire_) {const auto error=GetLastError();Close();SetLastError(error);return false;}
        new(wire_) Wire{};
        wire_->magic=kMagic;wire_->version=kVersion;wire_->bytes=sizeof(Wire);
        wire_->ownerPid=owner;wire_->hostPid=host;wire_->nonce=nonce;wire_->generation=generation;
        return true;
    }
    // Takes an explicitly duplicated child-owned handle. There is no public
    // object name or globally discoverable MIDI history mapping.
    bool Adopt(HANDLE transferred,DWORD owner,std::uint64_t nonce,std::uint64_t generation,DWORD host) {
        Close();handle_=transferred;
        if(!handle_ || handle_==INVALID_HANDLE_VALUE) {handle_=nullptr;SetLastError(ERROR_INVALID_HANDLE);return false;}
        wire_=static_cast<Wire*>(MapViewOfFile(handle_,FILE_MAP_ALL_ACCESS,0,0,sizeof(Wire)));
        if(!wire_) {const auto error=GetLastError();Close();SetLastError(error);return false;}
        if(wire_->magic!=kMagic || wire_->version!=kVersion || wire_->bytes!=sizeof(Wire) ||
           wire_->ownerPid!=owner || wire_->hostPid!=host || wire_->nonce!=nonce || !wire_->generation || (generation && wire_->generation!=generation)) {
            Close();SetLastError(ERROR_INVALID_DATA);return false;
        }
        return true;
    }
    // Exactly one producer (isolated acquisition) and one consumer (parent bridge).
    bool Push(Event event) noexcept {
        auto& w=*wire_;const auto head=Read(w.written),tail=Read(w.consumed);
        if(tail>head || head-tail>=kCapacity) {InterlockedIncrement64(&w.lost);return false;}
        event.sequence=head+1;w.events[head%kCapacity]=event;
        InterlockedExchange64(&w.written,static_cast<LONG64>(head+1));return true;
    }
    void Lose() noexcept {if(wire_) InterlockedIncrement64(&wire_->lost);}
    template<class Put> unsigned Drain(Put put) {
        auto& w=*wire_;auto tail=Read(w.consumed);const auto head=Read(w.written);
        if(head<tail || head-tail>kCapacity) {InterlockedIncrement64(&w.malformed);return 0;}
        const auto initial=tail;
        // The captured head bounds this pass; a busy producer cannot prolong it.
        while(tail<head) {
            const auto e=w.events[tail%kCapacity];
            const bool valid=e.sequence==tail+1 && e.source>0 && e.source<=16 && e.connection &&
                (e.kind==Kind::Disconnect || (e.kind==Kind::Sample && e.hid>0 && e.hid<keycode::kCount &&
                 e.physical==e.hid && e.milli<=1000 && e.previous<=1000 && e.timeUs &&
                 e.timing==Timing::HostSnapshot && e.keySpace==KeySpace::BindableCode));
            if(!valid) InterlockedIncrement64(&w.malformed);else put(e);
            ++tail;InterlockedExchange64(&w.consumed,static_cast<LONG64>(tail));
        }
        return static_cast<unsigned>(tail-initial);
    }
};
class Child {
    Mapping mapping_;
    std::unique_ptr<StreamEncoder> encoder_;
    std::uint64_t generation_=0;
public:
    void Refresh(HallJoyAnalogHost::SharedState& shared) {
        const auto requested=Read(shared.observationHistoryGeneration);
        const bool wanted=InterlockedCompareExchange(&shared.observationCaptureWanted,0,0)!=0;
        if(!wanted) {mapping_.Close();encoder_.reset();generation_=0;return;}
        // A zero generation while still wanted is a parent transition. Keep a
        // claimed mapping pinned but do no capture until its generation matches.
        if(!requested || (mapping_.Get() && generation_>requested)) return;
        if(generation_==requested) return; // Failed requests are not retried per poll.
        const auto transferred=reinterpret_cast<HANDLE>(static_cast<std::uintptr_t>(InterlockedExchange64(&shared.observationHistoryHandle,0)));
        if(!transferred) return; // Parent may have reclaimed a superseded transfer.
        mapping_.Close();encoder_.reset();generation_=requested;
        // The transferred handle's immutable generation is authoritative. The
        // parent may publish a newer handle just before publishing its new tag.
        if(!mapping_.Adopt(transferred,static_cast<DWORD>(shared.ownerPid),Read(shared.launchNonce),0,GetCurrentProcessId())) {
            const auto error=GetLastError();
            if(Read(shared.observationHistoryGeneration)==requested)
                InterlockedExchange(&shared.observationHistoryError,static_cast<LONG>(error));
            return;
        }
        generation_=mapping_.Get()->generation;
        if(generation_<requested) {mapping_.Close();generation_=0;return;}
        try {encoder_=std::make_unique<StreamEncoder>();}
        catch(...) {
            mapping_.Close();
            if(Read(shared.observationHistoryGeneration)==generation_)
                InterlockedExchange(&shared.observationHistoryError,ERROR_NOT_ENOUGH_MEMORY);
            return;
        }
        if(Read(shared.observationHistoryGeneration)==generation_)
            InterlockedExchange(&shared.observationHistoryError,0);
        InterlockedExchange(&mapping_.Get()->ready,1);
    }
    template<class Visit> void Capture(HallJoyAnalogHost::SharedState& shared,Visit visit) {
        // Disable is visible immediately before any history table scan.
        if(!encoder_ || !InterlockedCompareExchange(&shared.observationCaptureWanted,0,0) ||
           Read(shared.observationHistoryGeneration)!=generation_) return;
        encoder_->Frame(visit,[&](const Event& e){mapping_.Push(e);},[&]{mapping_.Lose();});
    }
};
}

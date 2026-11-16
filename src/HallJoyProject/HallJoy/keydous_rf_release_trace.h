#pragma once
#include <array>
#include <cstdint>
#include <cstdio>

namespace halljoy::keydous_rf_trace {
constexpr unsigned kRevision = 8;
constexpr unsigned kSlots = 128;
constexpr unsigned kPages = 4;
constexpr bool Matches(unsigned vid,unsigned pid,unsigned board) noexcept {
    return vid==0x3151 && pid==0x5038 && board==3793;
}
enum class Source : unsigned { None, Stream, Table };

// The backend serializes both producers and the snapshot under the existing
// receiver publication lock. This state never drives input or performs I/O.
struct Slot {
    std::uint64_t streamMs=0,streamZeroMs=0,tableMs=0,tableZeroMs=0,skippedZeroMs=0,appliedMs=0;
    std::uint64_t streamZeroAppliedMs=0,tableZeroAppliedMs=0,drainedMs=0,drainedZeroMs=0;
    unsigned drainedRaw=0;
    unsigned streamRaw=0,streamCount=0,tableRaw=0,tableCount=0,tableSkipped=0;
    unsigned tableRequest=0,appliedRaw=0;
    bool tableAccepted=false;
    Source source=Source::None;
    void Stream(unsigned raw,std::uint64_t now) noexcept {
        streamMs=now;streamRaw=raw;++streamCount;
        if(!raw)streamZeroMs=now;
    }
    void Table(unsigned raw,bool accepted,unsigned request,std::uint64_t now) noexcept {
        tableMs=now;tableRaw=raw;tableAccepted=accepted;tableRequest=request;++tableCount;
        if(!raw)tableZeroMs=now;
        if(!accepted){++tableSkipped;if(!raw)skippedZeroMs=now;}
    }
    void Drained(unsigned raw,std::uint64_t now) noexcept {
        drainedMs=now;drainedRaw=raw;
        if(!raw)drainedZeroMs=now;
    }
    void Applied(Source from,unsigned raw,std::uint64_t now) noexcept {
        source=from;appliedRaw=raw;appliedMs=now;
        if(!raw){
            if(from==Source::Stream)streamZeroAppliedMs=now;
            else if(from==Source::Table)tableZeroAppliedMs=now;
        }
    }
};
struct Page {
    unsigned attempts=0,completed=0,failed=0,stage=0,error=0,reason=0,drains=0,status=0;
    unsigned lastFailureStage=0,lastFailureError=0,drainedProblem=0;
    bool pending=false;
    std::uint64_t attemptMs=0,sendMs=0,endMs=0,lastReplyMs=0,lastFailureMs=0,drainedMs=0;
    void Begin(std::uint64_t now) noexcept {
        ++attempts;attemptMs=now;sendMs=0;endMs=0;
    }
    void Sent(std::uint64_t now) noexcept {sendMs=now;}
    void Finish(bool ok,unsigned transportStage,unsigned nativeError,unsigned problem,
                unsigned drained,unsigned ready,bool outstanding,std::uint64_t now) noexcept {
        endMs=now;stage=transportStage;error=nativeError;reason=problem;drains=drained;
        status=ready;pending=outstanding;
        if(ok){++completed;lastReplyMs=now;}
        else {++failed;lastFailureMs=now;lastFailureStage=transportStage;lastFailureError=nativeError;}
    }
};
struct Control {
    unsigned attempts=0,acks=0,failures=0,markerReads=0,statusWaits=0,fetchWaits=0,version=0,stage=0,lastKind=0,reason=0,abandonedCommand=0;
    bool active=false,fetchPending=false;
    unsigned resumedPages=0,dispatchWaits=0,dispatchUs=0,pendingPage=4;
    std::uint64_t pendingAgeUs=0;
};
inline void FormatControl(char* line,std::size_t length,unsigned session,const Control& c) noexcept {
    std::snprintf(line,length,
        "trace.keydous_rf.control revision=%u session=%u active=%u attempts=%u acks=%u failures=%u marker_reads=%u status_waits=%u fetch_waits=%u receiver_version=%08x stage=%u marker_reply_kind=%u fallback_reason=%u abandoned_metadata=%02x fetch_pending=%u",
        kRevision,session,c.active?1:0,c.attempts,c.acks,c.failures,c.markerReads,c.statusWaits,c.fetchWaits,c.version,c.stage,c.lastKind,c.reason,c.abandonedCommand,c.fetchPending?1:0);
}
inline void FormatProgress(char* line,std::size_t length,unsigned session,const Control& c) noexcept {
    std::snprintf(line,length,
        "trace.keydous_rf.progress revision=%u session=%u resumed_pages=%u dispatch_waits=%u dispatch_gap_us=%u pending_page=%u pending_age_us=%llu",
        kRevision,session,c.resumedPages,c.dispatchWaits,c.dispatchUs,c.pendingPage,static_cast<unsigned long long>(c.pendingAgeUs));
}
struct State {
    Control control{};
    unsigned session=0,units=0;
    std::array<Slot,kSlots> slots{};
    std::array<Page,kPages> pages{};
};
inline void FormatSlot(char* line,std::size_t length,unsigned session,unsigned slot,unsigned hid,
                       const Slot& s,unsigned physicalMilli,unsigned effectiveMilli) noexcept {
    std::snprintf(line,length,
        "trace.keydous_rf.slot session=%u slot=%u page=%u hid=%x stream_count=%u stream_ms=%llu stream_raw=%u stream_zero_ms=%llu "
        "table_count=%u table_ms=%llu table_request=%u table_raw=%u table_accepted=%u table_zero_ms=%llu skipped_zero_ms=%llu skipped_count=%u "
        "applied_source=%u applied_ms=%llu applied_raw=%u stream_zero_applied_ms=%llu table_zero_applied_ms=%llu "
        "physical_milli=%u effective_milli=%u drained_ms=%llu drained_raw=%u drained_zero_ms=%llu",
        session,slot,slot/32,hid,s.streamCount,static_cast<unsigned long long>(s.streamMs),s.streamRaw,
        static_cast<unsigned long long>(s.streamZeroMs),s.tableCount,static_cast<unsigned long long>(s.tableMs),
        s.tableRequest,s.tableRaw,s.tableAccepted?1:0,static_cast<unsigned long long>(s.tableZeroMs),
        static_cast<unsigned long long>(s.skippedZeroMs),s.tableSkipped,static_cast<unsigned>(s.source),
        static_cast<unsigned long long>(s.appliedMs),s.appliedRaw,static_cast<unsigned long long>(s.streamZeroAppliedMs),
        static_cast<unsigned long long>(s.tableZeroAppliedMs),physicalMilli,effectiveMilli,
        static_cast<unsigned long long>(s.drainedMs),s.drainedRaw,static_cast<unsigned long long>(s.drainedZeroMs));
}
inline void FormatPage(char* line,std::size_t length,unsigned session,unsigned page,const Page& p) noexcept {
    std::snprintf(line,length,
        "trace.keydous_rf.page session=%u page=%u attempts=%u completed=%u attempt_ms=%llu send_ms=%llu end_ms=%llu "
        "last_reply_ms=%llu stage=%u error=%u snapshot_reason=%u drains=%u f7=%x pending=%u "
        "failed=%u last_failure_ms=%llu last_failure_stage=%u last_failure_error=%u drained_ms=%llu drained_problem=%u",
        session,page,p.attempts,p.completed,static_cast<unsigned long long>(p.attemptMs),
        static_cast<unsigned long long>(p.sendMs),static_cast<unsigned long long>(p.endMs),
        static_cast<unsigned long long>(p.lastReplyMs),p.stage,p.error,p.reason,p.drains,p.status,p.pending?1:0,
        p.failed,static_cast<unsigned long long>(p.lastFailureMs),p.lastFailureStage,p.lastFailureError,
        static_cast<unsigned long long>(p.drainedMs),p.drainedProblem);
}
}

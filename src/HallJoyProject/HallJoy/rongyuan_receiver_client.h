#pragma once
#include "rongyuan_stream_protocol.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>

namespace halljoy::ry_receiver {
using Report = rongyuan::Report;
enum class Stage : unsigned {
  None, StatusWrite, StatusRead, StatusShape, Offline, Bootloader,
  SendReady, SelectKeyboard, CommandWrite, ReadReady, ReadNotice,
  ResponseRead, ResponseShape, Cancelled, MarkerWrite, MarkerRead, MarkerReply, FetchPending, SnapshotUnconfirmed
};
inline bool Candidate(unsigned vid,unsigned pid) noexcept {
  return (vid==0x3151 && pid==0x5038) || (vid==0x379a && pid==0x1804);
}
inline bool Identity(unsigned vid,unsigned pid,unsigned page,unsigned usage,
                     unsigned feature,unsigned interfaceNumber) noexcept {
  return Candidate(vid,pid) && page==0xffff && usage==2 &&
         feature==65 && interfaceNumber==2;
}
inline Report Control(unsigned command,unsigned value=0) noexcept {
  // Dongle F6/F7/FC are raw padded controls, not checksummed keyboard commands.
  Report r{};r[1]=static_cast<std::uint8_t>(command);r[2]=static_cast<std::uint8_t>(value);return r;
}
inline const char* StageName(Stage stage) noexcept {
  constexpr const char* names[]={"none","status_write","status_read","status_shape","offline","bootloader",
      "send_ready","select_keyboard","command_write","read_ready","read_notice","response_read","response_shape","cancelled",
      "marker_write","marker_read","marker_reply","fetch_pending","snapshot_unconfirmed"};
  return names[static_cast<unsigned>(stage)];
}
struct Status {
  bool online=false,send=false,read=false,boot=false;
  unsigned devices=0,keyboardState=0;
};
inline bool ParseStatus(const Report& r,Status& status) noexcept {
  // Native reports include report ID0; vendor payload offsets are one lower.
  if(r[0] || r[1]>1 || r[6]>1 || r[9]>1 || r[7]<1 || r[7]>3)return false;
  status={(r[7]&1)!=0 && r[4]==0,r[6]==1,r[1]==1,r[9]==1,r[7],r[4]};
  return true;
}
inline bool He108Trial(unsigned vid,unsigned pid,unsigned board) noexcept {
  return vid==0x3151 && pid==0x5038 && board==3365;
}
inline const ry_stream::Model* Model(unsigned board,unsigned vid=0x3151,unsigned pid=0x5038) noexcept {
  if(vid==0x379a && pid==0x1804 && ry_stream::HatorUltima(board))
    return ry_stream::Find(board,0x379a,0x1803);
  // The receiver PID is shared. The paired board must be proved independently.
  const bool keydous=vid==0x3151 && pid==0x5038 && board==3793;
  return (keydous || He108Trial(vid,pid,board)) ? ry_stream::Find(board,0x3151,0x5030) : nullptr;
}

class StreamOrder {
 public:
  using Ticket=std::array<unsigned,32>;
  Ticket Before(unsigned page) const noexcept {Ticket t{};for(unsigned i=0;i<32;++i)t[i]=seq_[page*32+i].load();return t;}
  void Event(unsigned slot,unsigned raw=0) noexcept {raw_[slot].store(raw);seq_[slot].fetch_add(1);}
  void Table(unsigned slot,unsigned raw) noexcept {raw_[slot].store(raw);}
  unsigned Current(unsigned slot) const noexcept {return raw_[slot].load();}
  bool Accept(unsigned page,unsigned i,const Ticket& t) const noexcept {return seq_[page*32+i].load()==t[i];}
 private:
  std::array<std::atomic<unsigned>,128> seq_{},raw_{};
};
// Delta reports may be quiet while a key is held. Valid F7 online replies prove
// radio liveness separately from depth changes; table-only data still ages out.
class Liveness {
 public:
  static constexpr std::uint64_t kFreshMs=1500;
  void Reset() noexcept {online_.store(false);lastOnline_.store(0);stream_.store(false);}
  void Status(std::uint64_t now,bool online) noexcept {lastOnline_.store(online?now:0);online_.store(online);}
  void ConfirmStream() noexcept {stream_.store(true);}
  void StopStream() noexcept {stream_.store(false);}
  bool Streaming() const noexcept {return stream_.load();}
  bool Connected(std::uint64_t now,std::uint64_t lastDepth) const noexcept {
    if(!online_.load())return false;
    return Recent(now,Streaming()?lastOnline_.load():lastDepth);
  }
 private:
  static bool Recent(std::uint64_t now,std::uint64_t stamp) noexcept {
    // A concurrent observation can advance the stamp after the caller read now.
    return stamp && (now<stamp || now-stamp<=kFreshMs);
  }
  std::atomic<std::uint64_t> lastOnline_{0};
  std::atomic<bool> online_{false},stream_{false};
};
inline unsigned ParsePage(const Report& report,unsigned index,unsigned units,rongyuan::Values& out) noexcept {
  if(!units)return 4;
  if(report[0])return 1;
  if(report[1]==0xe5 && report[2]==0xfe && report[3]==1 && report[4]==index)return 2;
  return rongyuan::ParseTravel(report,units,out)?0:3;
}
// IO owns the physical exclusive handle and bounded/cancellable Win32 operations.
// The same client is executed by the fake-radio tests and by production.
struct BeforeSendNothing {void operator()() const noexcept {}};
template<class IO> class Client {
 public:
  // Official RY receiver transport waits10ms between forwarding a remote
  // command and the next control SET. SET_REPORT only stages a single mailbox;
  // F7 can overwrite it before the worker forwards E5. This is a control-path
  // handoff interval, never a sensor age/release rule or interrupt-stream cap.
  static constexpr unsigned kRemoteDispatchUs=10000;
  explicit Client(IO& io,bool hator=false,unsigned dispatchUs=0):io_(io),hator_(hator),dispatchUs_(dispatchUs){}
  Stage stage=Stage::None;
  unsigned command=0,statusReads=0,exchanges=0,page=4,snapshotProblem=0,drains=0,resumedPages=0;
  unsigned timeoutUs=200000;
  Status lastStatus{};
  bool HasPendingReply() const noexcept {return pending_;}
  unsigned PendingPage() const noexcept {return pending_?pendingPage_:4;}
  std::uint64_t PendingAgeUs(){return pending_?io_.ClockUs()-pendingStartUs_:0;}
  unsigned DispatchUs() const noexcept {return dispatchUs_;}
  unsigned dispatchWaits=0;
  // Verify this receiver's F0 echo before accepting any untagged table page.
  // Failure of the local read-only marker must not prevent a 1B stream attempt.
  void RequireResponseSync() noexcept {syncWanted_=true;}
  bool ResponseSyncActive() const noexcept {return syncActive_;}
  bool BeginResponseSync(){
    syncWanted_=true;
    if(!Marker()){if(stage==Stage::MarkerReply){syncUnconfirmed_=true;sync.reason=1;}return false;}
    syncActive_=true;return true;
  }
  bool ResponseSyncUnconfirmed() const noexcept {return syncUnconfirmed_;}
  bool TrialNeedsStreamFallback() const noexcept {return snapshotProblem || syncUnconfirmed_;}
  void StreamOnlyForPendingMetadata() noexcept {
    if(pendingPage_!=4 || (!pending_ && !fetchPending_))return;
    // No analog page has been issued: abandon only optional metadata and stop
    // ALL later table requests in this session, so a late untyped map can never
    // be relabeled as depth. Still attempt real1B and drain status/inbox normally.
    syncUnconfirmed_=true;syncWanted_=true;syncActive_=false;sync.reason=2;
    sync.abandonedCommand=pendingCommand_;
    markerPending_=false;fetchPending_=false;pending_=false;pendingCommand_=0;
  }
  bool MaintainLink(){return Prepare();} // Link/status only; never publishes a depth page.
  bool FetchOutstanding() const noexcept {return fetchPending_;}
  struct SyncEvidence {
    unsigned attempts=0,acks=0,failures=0,markerReads=0,statusWaits=0,fetchWaits=0,version=0,lastKind=0,reason=0,abandonedCommand=0;
  } sync;
  static bool MarkerWait(Stage s) noexcept {return s==Stage::MarkerReply || s==Stage::FetchPending || s==Stage::SnapshotUnconfirmed;}
  static bool Busy(Stage s) noexcept {return s==Stage::SendReady || s==Stage::ReadReady;}
  static bool Transient(Stage s) noexcept {return Busy(s) || s==Stage::StatusShape || MarkerWait(s);}
  // Official 1B control has no keyboard reply. Transport readiness still gates it.
  bool SendKeyboard(const Report& request) {
    if(ending_){stage=Stage::Cancelled;return false;}
    stage=Stage::None;command=request[1];
    if(!syncActive_)markerPending_=false; // supersede an unsupported read-only probe, never a depth fetch
    if(!Prepare() || !Write(request,Stage::CommandWrite))return false;
    return DispatchReady();
  }
  template<class Validate,class BeforeSend=BeforeSendNothing> bool Query(const Report& request,Report& reply,Validate valid,BeforeSend before={}) {
    if(ending_){stage=Stage::Cancelled;return false;}
    stage=Stage::None;command=request[1];
    if(!syncActive_)markerPending_=false;
    if(!Prepare())return false;
    if(syncActive_ && !Marker())return false;
    before(); // Snapshot ordering begins at command issue, not at readiness wait.
    pending_=true; // A late response must be drained before a DIFFERENT request.
    pendingCommand_=request[1];
    pendingPage_=request[1]==0xe5 && request[2]==0xfe && request[3]==1 && request[4]<4 ?request[4]:4;
    pendingStartUs_=io_.ClockUs();
    if(!Write(request,Stage::CommandWrite) || !Ready(true,syncActive_))return false;
    reply={};
    if(!Fetch(reply))return false;
    pending_=false;pendingCommand_=0;
    if(!Check())return false;
    if(!valid(reply)){stage=Stage::ResponseShape;return false;}
    ++exchanges;stage=Stage::None;return true;
  }
  template<class BeforeSend=BeforeSendNothing> bool Page(unsigned index,unsigned units,rongyuan::Values& out,BeforeSend before={}) {
    if(ending_){stage=Stage::Cancelled;return false;}
    page=index;snapshotProblem=0;
    if(index>=4){stage=Stage::ResponseShape;return false;}
    if(syncWanted_ && !syncActive_){
      if(syncUnconfirmed_){stage=Stage::SnapshotUnconfirmed;return false;}
      if(!BeginResponseSync())return false;
    }
    Report response{};rongyuan::Values values{};
    if(pending_ && pendingPage_==index){
      // Resume the ORIGINAL request, not a second request for the same page.
      // Its caller retains the send-time stream ticket across I/O deadlines.
      // A pending FC may ONLY receive GETs; F0/F7 would overwrite its result.
      command=pendingCommand_;
      if(units!=pendingUnits_){stage=Stage::ResponseShape;return false;}
      if(!fetchPending_ && !Ready(true))return false;
      if(!Fetch(response))return false;
      pending_=false;pendingCommand_=0;
      if(!Check())return false;
      snapshotProblem=ParsePage(response,index,units,values);
      if(snapshotProblem){stage=Stage::ResponseShape;return false;}
      ++exchanges;++resumedPages;stage=Stage::None;out=values;return true;
    }
    const auto wirePage=static_cast<std::uint8_t>(index);
    pendingUnits_=units;
    const auto request=hator_?ry_stream::HatorRequest(0xe5,0xfe,1,wirePage):ry_stream::Request(0xe5,0xfe,1,wirePage);
    if(!Query(request,response,
      [&](const Report& r) {
        snapshotProblem=ParsePage(r,index,units,values);
        return snapshotProblem==0;
      },before))return false;
    out=values;return true;
  }
  bool Frame(unsigned units,std::array<std::uint16_t,128>& out) {
    std::array<std::uint16_t,128> next{};
    for(unsigned i=0;i<4;++i){
      rongyuan::Values values{};if(!Page(i,units,values))return false;
      std::copy(values.begin(),values.end(),next.begin()+32*i);
    }
    page=4;out=next;return true;
  }
  bool EndStream(const Report& disable){
    // Terminal cleanup: no later table can consume the abandoned untyped reply.
    // Wait for SEND readiness, not for a missing remote query reply. A live
    // session must use SendKeyboard instead and preserve pending ownership.
    ending_=true;pending_=false;pendingPage_=4;pendingCommand_=0;
    fetchPending_=false;markerPending_=false;stage=Stage::None;
    return Ready(false) && Write(disable,Stage::CommandWrite) && DispatchReady();
  }
 private:
  IO& io_;
  bool hator_;
  unsigned dispatchUs_=0;
  std::uint64_t dispatchUntil_=0,pendingStartUs_=0;
  bool selected_=false,pending_=false,ending_=false;
  bool syncWanted_=false,syncActive_=false,syncUnconfirmed_=false,markerPending_=false,fetchPending_=false;
  unsigned pendingPage_=4,pendingCommand_=0,markerTag_=0,fetchTag_=0,pendingUnits_=0;
  Report markerRequest_{};
  // F0 is the local receiver's version GET, not a remote keyboard command.
  // The reviewed receiver echoes payload byte7 in its reply byte7. Choose a
  // different high-bit tag from the currently visible reply, with valid Bit7
  // checksum. Tags make the marker impossible as a depth page at all reviewed
  // scales (10/100/200/1000), while accepting every genuine depth array.
  static bool IsMarker(const Report& r,unsigned tag) noexcept {
    return r[0]==0 && r[1]==0xf0 && r[8]==tag;
  }
  bool Marker(){
    if(!markerPending_){
      Report baseline{};
      if(!Read(baseline,Stage::MarkerRead))return false;
      markerTag_=0x80u|((unsigned(baseline[8])+1u)&0x7fu);
      auto request=Control(0xf0,(255u-markerTag_-0xf0u)&255u);
      request[8]=static_cast<std::uint8_t>(markerTag_);
      std::fill(request.begin()+9,request.end(),std::uint8_t{255});
      markerRequest_=request; // The local GET clears its output; a request echo is not proof.
      ++sync.attempts;markerPending_=true;
      if(!Write(request,Stage::MarkerWrite))return false;
    }
    const auto start=io_.ClockUs();
    do {
      Report reply{};
      if(!Read(reply,Stage::MarkerRead))return false;
      ++sync.markerReads;
      if(IsMarker(reply,markerTag_) && reply!=markerRequest_){
        sync.lastKind=0;markerPending_=false;++sync.acks;
        sync.version=unsigned(reply[2])|(unsigned(reply[3])<<8)|(unsigned(reply[4])<<16)|(unsigned(reply[5])<<24);
        return true;
      }
      Status status{};
      sync.lastKind=reply==markerRequest_?4u:reply[1]==0xf0?1u:ParseStatus(reply,status)?2u:3u;
      io_.YieldBusy();
    }while(io_.ClockUs()-start<timeoutUs);
    ++sync.failures;stage=Stage::MarkerReply;return false;
  }
  bool Replaced(Report& reply,unsigned tag,bool status){
    const auto start=io_.ClockUs();
    do {
      if(!Read(reply,status?Stage::StatusRead:Stage::ResponseRead))return false;
      if(!IsMarker(reply,tag))return true;
      if(status)++sync.statusWaits;else ++sync.fetchWaits;
      io_.YieldBusy();
    }while(io_.ClockUs()-start<timeoutUs);
    stage=status?Stage::StatusShape:Stage::FetchPending;return false;
  }
  bool Fetch(Report& reply){
    if(!syncActive_)
      return Write(Control(0xfc),Stage::ReadNotice) && Read(reply,Stage::ResponseRead);
    if(!fetchPending_){
      if(!Marker())return false;
      fetchTag_=markerTag_;fetchPending_=true;
      if(!Write(Control(0xfc),Stage::ReadNotice))return false;
    }
    // On a deadline, resume ONLY these GETs. A new F7/F0 would overwrite the
    // pending fetch and could relabel a late page as a different page.
    if(!Replaced(reply,fetchTag_,false))return false;
    fetchPending_=false;return true;
  }
  bool Check(){if(io_.Check())return true;stage=Stage::Cancelled;return false;}
  bool DispatchReady(){
    if(!dispatchUntil_)return true;
    if(io_.ClockUs()<dispatchUntil_){
      ++dispatchWaits;
      for(;;){
        const auto now=io_.ClockUs();
        if(now>=dispatchUntil_)break;
        if(!Check())return false;
        if constexpr(requires(IO& io){io.WaitDispatch(1u);})
          io_.WaitDispatch(static_cast<unsigned>(dispatchUntil_-now));
        else {Report ignored{};if(!Read(ignored,Stage::ResponseRead))return false;}
      }
    }
    dispatchUntil_=0;return Check();
  }
  bool Write(const Report& r,Stage failure) {
    if(!Check() || !DispatchReady())return false;
    const bool ok=io_.Write(r);
    if(dispatchUs_ && r[1]!=0xf0 && r[1]!=0xf7 && r[1]!=0xfc)
      dispatchUntil_=io_.ClockUs()+dispatchUs_;
    if(ok)return true;stage=failure;return false;
  }
  bool Read(Report& r,Stage failure) {
    if(!Check())return false;
    if(io_.Read(r))return true;stage=failure;return false;
  }
  bool Drain(){
    Report stale{};
    if(!Fetch(stale))return false;
    if constexpr(requires(IO& io){io.DrainedPage(0u,stale);}) {
      if(pending_ && pendingPage_<4)io_.DrainedPage(pendingPage_,stale);
    }
    pending_=false;pendingCommand_=0;++drains;return true;
  }
  bool Prepare(){
    if(fetchPending_){if(!Drain())return false;}
    else if(pending_ && (!Ready(true) || !Drain()))return false;
    if(!Ready(false))return false;
    // Exclusive ownership permits the same target cache used by the official client.
    if(!selected_){if(!Write(Control(0xf6,0x0a),Stage::SelectKeyboard))return false;selected_=true;}
    return true;
  }
  bool Ready(bool read,bool marked=false) {
    if(syncActive_ && !marked && !Marker())return false;
    const auto start=io_.ClockUs();bool malformed=false;
    do {
      if(!Write(Control(0xf7),Stage::StatusWrite))return false;
      Report r{};
      if(syncActive_){if(!Replaced(r,markerTag_,true))return false;}
      else if(!Read(r,Stage::StatusRead))return false;
      ++statusReads;
      if(!ParseStatus(r,lastStatus)){malformed=true;}
      else {
        malformed=false;
        io_.Status(lastStatus.online && !lastStatus.boot);
        if(!lastStatus.online){stage=Stage::Offline;return false;}
        if(lastStatus.boot){stage=Stage::Bootloader;return false;}
        if(read){if(lastStatus.read)return true;}
        else if(lastStatus.read){
          // Prepare has already consumed our outstanding reply. A leftover inbox
          // must be drained BEFORE waiting for send-ready; otherwise it can hold
          // send=0 forever. Never drain the response owned by an active Query.
          if(!Drain())return false;
          if(syncActive_ && !Marker())return false;
        }else if(lastStatus.send)return true;
      }
      io_.YieldBusy(); // Only a busy/late receiver yields; no fixed delay or polling cap.
    } while(io_.ClockUs()-start<timeoutUs);
    stage=malformed?Stage::StatusShape:read?Stage::ReadReady:Stage::SendReady;
    return false;
  }
};
} // namespace halljoy::ry_receiver

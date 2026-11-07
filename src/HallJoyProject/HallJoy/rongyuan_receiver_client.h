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
  ResponseRead, ResponseShape, Cancelled
};
inline bool Identity(unsigned vid,unsigned pid,unsigned page,unsigned usage,
                     unsigned feature,unsigned interfaceNumber) noexcept {
  return vid==0x3151 && pid==0x5038 && page==0xffff && usage==2 &&
         feature==65 && interfaceNumber==2;
}
inline Report Control(unsigned command,unsigned value=0) noexcept {
  // Dongle F6/F7/FC are raw padded controls, not checksummed keyboard commands.
  Report r{};r[1]=static_cast<std::uint8_t>(command);r[2]=static_cast<std::uint8_t>(value);return r;
}
inline const char* StageName(Stage stage) noexcept {
  constexpr const char* names[]={"none","status_write","status_read","status_shape","offline","bootloader",
      "send_ready","select_keyboard","command_write","read_ready","read_notice","response_read","response_shape","cancelled"};
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
inline const ry_stream::Model* Model(unsigned board) noexcept {
  // Receiver PID is shared. Only this remotely proved keyboard is authorized.
  return board==3793 ? ry_stream::Find(board,0x3151,0x5030) : nullptr;
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
// IO owns the physical exclusive handle and bounded/cancellable Win32 operations.
// The same client is executed by the fake-radio tests and by production.
struct BeforeSendNothing {void operator()() const noexcept {}};
template<class IO> class Client {
 public:
  explicit Client(IO& io):io_(io){}
  Stage stage=Stage::None;
  unsigned command=0,statusReads=0,exchanges=0,page=4,snapshotProblem=0,drains=0;
  unsigned timeoutUs=200000;
  Status lastStatus{};
  static bool Busy(Stage s) noexcept {return s==Stage::SendReady || s==Stage::ReadReady;}
  static bool Transient(Stage s) noexcept {return Busy(s) || s==Stage::StatusShape;}
  // Official 1B control has no keyboard reply. Transport readiness still gates it.
  bool SendKeyboard(const Report& request) {
    stage=Stage::None;command=request[1];
    if(!Prepare() || !Write(request,Stage::CommandWrite))return false;
    return true;
  }
  template<class Validate,class BeforeSend=BeforeSendNothing> bool Query(const Report& request,Report& reply,Validate valid,BeforeSend before={}) {
    stage=Stage::None;command=request[1];
    if(!Prepare())return false;
    before(); // Snapshot ordering begins at command issue, not at readiness wait.
    pending_=true; // A late response must be drained before a DIFFERENT request.
    if(!Write(request,Stage::CommandWrite) || !Ready(true) ||
       !Write(Control(0xfc),Stage::ReadNotice))return false;
    reply={};
    if(!Read(reply,Stage::ResponseRead))return false;
    pending_=false;
    if(!valid(reply)){stage=Stage::ResponseShape;return false;}
    ++exchanges;stage=Stage::None;return true;
  }
  template<class BeforeSend=BeforeSendNothing> bool Page(unsigned index,unsigned units,rongyuan::Values& out,BeforeSend before={}) {
    page=index;snapshotProblem=0;
    if(index>=4){stage=Stage::ResponseShape;return false;}
    Report response{};rongyuan::Values values{};
    if(!Query(ry_stream::Request(0xe5,0xfe,1,static_cast<std::uint8_t>(index)),response,
      [&](const Report& r) {
        if(!units){snapshotProblem=4;return false;}
        if(r[0]){snapshotProblem=1;return false;}
        if(r[1]==0xe5 && r[2]==0xfe && r[3]==1 && r[4]==index){snapshotProblem=2;return false;}
        if(!rongyuan::ParseTravel(r,units,values)){snapshotProblem=3;return false;}
        return true;
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
 private:
  IO& io_;
  bool selected_=false,pending_=false;
  bool Check(){if(io_.Check())return true;stage=Stage::Cancelled;return false;}
  bool Write(const Report& r,Stage failure) {
    if(!Check())return false;
    if(io_.Write(r))return true;stage=failure;return false;
  }
  bool Read(Report& r,Stage failure) {
    if(!Check())return false;
    if(io_.Read(r))return true;stage=failure;return false;
  }
  bool Drain(){
    Report stale{};
    if(!Write(Control(0xfc),Stage::ReadNotice) || !Read(stale,Stage::ResponseRead))return false;
    pending_=false;++drains;return true;
  }
  bool Prepare(){
    if(pending_ && (!Ready(true) || !Drain()))return false;
    if(!Ready(false))return false;
    // Exclusive ownership permits the same target cache used by the official client.
    if(!selected_){if(!Write(Control(0xf6,0x0a),Stage::SelectKeyboard))return false;selected_=true;}
    return true;
  }
  bool Ready(bool read) {
    const auto start=io_.ClockUs();bool malformed=false;
    do {
      if(!Write(Control(0xf7),Stage::StatusWrite))return false;
      Report r{};if(!Read(r,Stage::StatusRead))return false;
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
        }else if(lastStatus.send)return true;
      }
      io_.YieldBusy(); // Only a busy/late receiver yields; no fixed delay or polling cap.
    } while(io_.ClockUs()-start<timeoutUs);
    stage=malformed?Stage::StatusShape:read?Stage::ReadReady:Stage::SendReady;
    return false;
  }
};
} // namespace halljoy::ry_receiver

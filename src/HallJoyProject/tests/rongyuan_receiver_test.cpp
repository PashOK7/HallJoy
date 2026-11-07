#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
#include "rongyuan_receiver_client.h"
#include "physical_analog_state.h"
#include "keyboard_communication_health.h"
#include "generated/layout_pipeline/identities.h"
#include <cassert>
#include <iostream>
#include <vector>
using namespace halljoy;
using namespace ry_receiver;
struct Fake {
  std::vector<Report> writes;
  Report control{},pending{};
  unsigned operations=0,failAt=0,delays=0,cancelAt=0,busy=0;
  bool online=true,boot=false,invalid=false,echo=false,stale=false,selected=false;
  bool malformedDepth=false,readNever=false,inboxBlocksSend=false,duringReply=false;
  StreamOrder* waitingOrder=nullptr;unsigned waitingSlot=14;
  unsigned wrongBoard=0;
  std::array<std::uint16_t,128> depth{};
  std::uint64_t clock=0;
  bool Step(){clock+=1000;return ++operations!=failAt;}
  bool Check(){return ++delays!=cancelAt;}
  std::uint64_t ClockUs(){return clock;}
  void YieldBusy(){++clock;}
  unsigned statusOnline=0,statusOffline=0;
  void Status(bool online){if(online)++statusOnline;else ++statusOffline;}
  bool Write(const Report& r){
    if(!Step())return false;
    writes.push_back(r);control=r;
    if(r[1]==0xf6){assert(r==Control(0xf6,0x0a));selected=true;}
    else if(r[1]!=0xf7 && r[1]!=0xfc){assert(selected);pending=r;stale=true;}
    return true;
  }
  bool Read(Report& r){
    if(!Step())return false;
    r={};
    if(control[1]==0xf7){
      r[7]=invalid?0:1;r[4]=online?0:1;r[9]=boot?1:0;
      if(waitingOrder && (busy || (duringReply && stale && pending[1]==0xe5)))waitingOrder->Event(waitingSlot,33);
      r[6]=(busy || (inboxBlocksSend && stale))?0:1;
      r[1]=busy?0:(stale && !readNever?1:0);if(busy)--busy;
      return true;
    }
    assert(control[1]==0xfc);
    stale=false;
    if(echo){r=pending;return true;}
    if(pending[1]==0x8f){
      r[1]=0x8f;const unsigned board=wrongBoard?wrongBoard:3793;
      for(unsigned i=0;i<4;++i)r[2+i]=static_cast<unsigned char>(board>>(8*i));
    } else if(pending[1]==0xe5){
      assert(pending[2]==0xfe && pending[3]==1 && pending[4]<4);
      for(unsigned i=0;i<32;++i){
        const unsigned v=malformedDepth?65535:depth[32*pending[4]+i];
        r[1+2*i]=static_cast<unsigned char>(v);r[2+2*i]=static_cast<unsigned char>(v>>8);
      }
    }
    return true;
  }
};
int main(){
  assert(Identity(0x3151,0x5038,0xffff,2,65,2));
  assert(!Identity(0x3151,0x5030,0xffff,2,65,2));
  assert(!Identity(0x3151,0x5038,0xff00,2,65,2));
  assert(!Identity(0x3151,0x5038,0xffff,1,65,2));
  assert(!Identity(0x3151,0x5038,0xffff,2,64,2));
  assert(!Identity(0x3151,0x5038,0xffff,2,65,1));
  assert(Model(3793) && !Model(3496) && !Model(0) && !Model(3794));
  assert(layout_identity::Token("rongyuan-stream",Model(3793)->product));
  const auto raw=Control(0xf7);assert(raw[1]==0xf7 && raw[8]==0);
  Status s;Report status{};assert(!ParseStatus(status,s));status[7]=1;
  assert(ParseStatus(status,s) && s.online && !s.send && !s.read);
  status[1]=2;assert(!ParseStatus(status,s));
  Fake io;Client client(io);Report identity{};
  assert(client.Query(ry_stream::Request(0x8f),identity,[](const auto& r){return Model(ry_stream::Board(r))!=nullptr;}));
  assert(ry_stream::Board(identity)==3793 && client.exchanges==1);
  // A previous response is explicitly consumed, never accepted as our proof.
  Fake old;old.stale=true;old.pending=ry_stream::Request(0x80);Client oldClient(old);
  assert(oldClient.Query(ry_stream::Request(0x8f),identity,[](const auto& r){return Model(ry_stream::Board(r))!=nullptr;}));
  Fake foreign;foreign.wrongBoard=3496;Client foreignClient(foreign);
  assert(!foreignClient.Query(ry_stream::Request(0x8f),identity,[](const auto& r){return Model(ry_stream::Board(r))!=nullptr;}));
  assert(foreignClient.stage==Stage::ResponseShape);
  for(unsigned mode=0;mode<5;++mode){
    Fake bad;bad.online=mode!=0;bad.boot=mode==1;bad.invalid=mode==2;bad.busy=mode==3?100000:0;bad.cancelAt=mode==4?1:0;
    Client c(bad);assert(!c.Query(ry_stream::Request(0x8f),identity,[](const auto&){return true;}));
    const Stage expected[]={Stage::Offline,Stage::Bootloader,Stage::StatusShape,Stage::SendReady,Stage::Cancelled};
    assert(c.stage==expected[mode]);
    for(const auto& write:bad.writes)assert(write[1]==0xf7); // no remote command before readiness
  }
  Fake frameIO;Client frameClient(frameIO);std::array<std::uint16_t,128> frame{};
  frameIO.depth[14]=150;frameIO.depth[9]=75;frameIO.depth[41]=320;frameIO.depth[100]=5;
  assert(frameClient.Frame(100,frame) && frame==frameIO.depth && frameClient.exchanges==4);
  physical_analog::Publication publication;
  const auto model=Model(3793);
  for(unsigned slot=0;slot<128;++slot){const auto hid=ry_stream::Decode(model->matrix.data()+4*slot);if(hid)assert(publication.Bind(static_cast<unsigned char>(slot+1),hid));}
  auto publish=[&](unsigned now){for(unsigned slot=0;slot<128;++slot)publication.Publish(static_cast<unsigned char>(slot+1),ry_stream::Normalize(frame[slot],100,4000),now);};
  publish(100);assert(publication.Read(26,100).milli==375 && publication.Read(4,100).milli==188);
  assert(publication.Read(44,100).milli==800);
  assert(frameClient.Frame(100,frame));publish(110);assert(publication.Read(26,110).milli==375); // stationary hold
  frameIO.depth[14]=0;assert(frameClient.Frame(100,frame));publish(120);
  assert(publication.Read(26,120).milli==0 && publication.Read(4,120).milli==188);
  assert(!publication.Read(4,1700,1500).fresh && !publication.Read(4,1700,1500).milli);
  publication.Clear();assert(!publication.Read(4,120).milli);
  Fake counted;Client countedClient(counted);assert(countedClient.Frame(100,frame));
  const unsigned ioCount=counted.operations;assert(ioCount==29);
  assert(counted.clock==ioCount*1000); // ONLY modeled I/O time: no artificial scheduling delay.
  assert(std::count_if(counted.writes.begin(),counted.writes.end(),[](const auto& r){return r[1]==0xf6;})==1);
  // Every individual transport failure leaves the caller's old frame untouched.
  for(unsigned failure=1;failure<=ioCount;++failure){
    Fake bad;bad.failAt=failure;Client c(bad);frame.fill(123);
    assert(!c.Frame(100,frame));for(auto v:frame)assert(v==123);
  }
  for(unsigned cancellation=1;cancellation<=ioCount;++cancellation){
    Fake bad;bad.cancelAt=cancellation;Client c(bad);frame.fill(123);
    assert(!c.Frame(100,frame));for(auto v:frame)assert(v==123);
    assert(c.stage==Stage::Cancelled);
  }
  {Fake bad;bad.readNever=true;Client c(bad);assert(!c.Frame(100,frame) && c.stage==Stage::ReadReady);}
  for(unsigned mode=0;mode<2;++mode){
    Fake bad;bad.echo=mode==0;bad.malformedDepth=mode==1;Client c(bad);frame.fill(99);
    assert(!c.Frame(1000,frame));for(auto v:frame)assert(v==99);
    assert(c.snapshotProblem==(mode==0?2u:3u) && c.page==0);
  }
  {
    Fake slow;slow.depth[0]=11;slow.depth[32]=22;slow.readNever=true;Client c(slow);
    rongyuan::Values page{};page.fill(99);
    assert(!c.Page(0,100,page) && c.stage==Stage::ReadReady && Client<Fake>::Transient(c.stage));
    for(auto v:page)assert(v==99);
    slow.readNever=false;
    assert(c.Page(1,100,page) && page[0]==22); // late page0 was drained, never mislabeled as page1
    assert(c.exchanges==1 && c.page==1);
    assert(c.SendKeyboard(ry_stream::Request(0x1b,1)));
    assert(c.Page(0,100,page) && page[0]==11);
    assert(c.SendKeyboard(ry_stream::Request(0x1b,0)));
  }
  {
    StreamOrder order;const auto ticket=order.Before(0);
    assert(order.Accept(0,14,ticket));order.Event(14);
    assert(!order.Accept(0,14,ticket) && order.Accept(0,9,ticket));
    const auto next=order.Before(0);assert(order.Accept(0,14,next));
  }
  {
    // Log31: a completed startup table and online replies, with zero stream
    // packets. Sending 1B cannot grant an unlimited lifetime to those values.
    Liveness link;
    physical_analog::Publication values;
    assert(values.Bind(1,4) && values.Bind(2,7) && values.Bind(3,4));
    values.Publish(1,863,1000);
    values.Publish(2,838,1000);
    link.Status(7390,true);
    assert(!link.Connected(7390,1000));
    assert(!values.Read(4,7390,Liveness::kFreshMs,link.Streaming()).fresh);

    // A real packet for an aliased key must not revive the old table value,
    // even when both physical keys feed the same remapped HID.
    values.Publish(3,400,7400,true);
    link.Status(7400,true);
    link.ConfirmStream();
    assert(link.Connected(7400,7400));
    assert(values.Read(4,7400,Liveness::kFreshMs,link.Streaming()).milli==400);
    assert(!values.Read(7,7400,Liveness::kFreshMs,link.Streaming()).fresh);
    link.Status(37400,true);
    assert(values.Read(4,37400,Liveness::kFreshMs,link.Streaming()).milli==400);
    values.Publish(3,0,37401,true);
    assert(values.Read(4,37401,Liveness::kFreshMs,link.Streaming()).milli==0);
    link.StopStream();
    assert(!values.Read(4,38902,Liveness::kFreshMs,link.Streaming()).fresh);
  }
  for(const auto& write:frameIO.writes)
    assert(write[1]==0xf7 || write[1]==0xf6 || write[1]==0xfc || write[1]==0xe5);
  {
    Liveness link;physical_analog::Publication values;keyboard_support::CommunicationHealth health;
    assert(values.Bind(1,4));values.Publish(1,555,1000);
    assert(!link.Connected(1000,1000));link.Status(1000,true);link.ConfirmStream();
    values.Publish(1,555,1000,true);
    // Thirty seconds without a NEW depth: real online replies preserve the held
    // sample and connection, without changing its timestamp or inventing packets.
    for(std::uint64_t now=1000;now<=31000;now+=250){
      link.Status(now,true);assert(link.Connected(now,1000));
      assert(values.Read(4,now,Liveness::kFreshMs,link.Streaming()).milli==555);
      assert(!health.Observe(now,true,link.Connected(now,1000)));
    }
    assert(!values.Read(4,31000,1500).fresh); // reproduces the old false expiry
    assert(!link.Connected(32501,1000)); // no heartbeat: still fails closed
    link.Status(33000,true);assert(link.Connected(33000,1000));
    values.Publish(1,0,33001);assert(values.Read(4,33001,Liveness::kFreshMs,link.Streaming()).milli==0);
    link.Status(33002,false);assert(!link.Connected(33002,33001));
    link.Status(34000,true);link.StopStream();
    assert(!link.Connected(34000,1000));assert(link.Connected(34000,33999));
    link.Reset();assert(!link.Streaming() && !link.Connected(34000,33999));
    assert(Client<Fake>::Busy(Stage::SendReady) && Client<Fake>::Busy(Stage::ReadReady));
    assert(!Client<Fake>::Busy(Stage::StatusShape) && !Client<Fake>::Busy(Stage::Offline));
    Fake busy;busy.busy=100000;Client c(busy);Report reply{};
    assert(!c.Query(ry_stream::Request(0x8f),reply,[](const auto&){return true;}));
    assert(c.stage==Stage::SendReady && busy.statusOnline>0 && !busy.statusOffline);
    Fake off;off.online=false;Client offline(off);
    assert(!offline.Query(ry_stream::Request(0x8f),reply,[](const auto&){return true;}));
    assert(offline.stage==Stage::Offline && off.statusOffline==1);
  }
  {
    // A receiver may refuse every send until its leftover readable inbox is
    // fetched with FC. This reproduced send_ready forever in the old client.
    Fake inbox;inbox.inboxBlocksSend=true;inbox.stale=true;inbox.pending=ry_stream::Request(0x80);
    inbox.depth[0]=11;inbox.depth[32]=22;Client c(inbox);rongyuan::Values page{};
    assert(c.Page(0,100,page) && page[0]==11 && c.drains==1);
    inbox.stale=true;inbox.pending=ry_stream::Request(0x80);
    assert(c.Page(1,100,page) && page[0]==22 && c.drains==2);
    assert(c.exchanges==2); // our requested replies were consumed, never discarded
    for(unsigned failure=3;failure<=4;++failure){
      Fake broken;broken.inboxBlocksSend=true;broken.stale=true;broken.failAt=failure;
      Client bad(broken);page.fill(99);assert(!bad.Page(0,100,page));
      for(auto value:page)assert(value==99);
    }
  }
  {
    // Stream motion BEFORE the table command is sent cannot supersede that
    // table. Capture after readiness/draining; retain protection for later motion.
    StreamOrder order;Fake wait;wait.waitingOrder=&order;wait.busy=8;Client c(wait);
    const auto legacy=order.Before(0);StreamOrder::Ticket sent{};rongyuan::Values page{};
    assert(c.Page(0,100,page,[&]{sent=order.Before(0);}));
    assert(!order.Accept(0,14,legacy) && order.Accept(0,14,sent));
    assert(order.Current(14)==33 && page[14]==0);
    physical_analog::Publication actual;assert(actual.Bind(1,4));actual.Publish(1,83,1000);
    if(order.Accept(0,14,sent)){actual.Publish(1,0,1100);order.Table(14,0);}
    assert(actual.Read(4,1100,~std::uint64_t{0}).milli==0 && order.Current(14)==0);
    StreamOrder newer;Fake response;response.waitingOrder=&newer;response.duringReply=true;Client late(response);
    StreamOrder::Ticket before{};assert(late.Page(0,100,page,[&]{before=newer.Before(0);}));
    assert(!newer.Accept(0,14,before) && newer.Current(14)==33);
    Fake offline;offline.online=false;Client missing(offline);bool issued=false;
    assert(!missing.Page(0,100,page,[&]{issued=true;}) && !issued);
  }
  std::cout<<"RONGYUAN_RECEIVER=PASS exact remote proof, raw controls, stale drain, 4 pages, multikey holds/releases, no partial publication, cancellation, 29 I/O failures/cancellations, no fixed wait, cached target, late-reply recovery, stream enable/disable, snapshot order, 30s quiet hold/idle, online heartbeat, offline/stale clearing and no false warning, blocked inbox recovery, send-time table tickets and late-stream protection\n";
}

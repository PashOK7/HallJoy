#pragma once
// Isolated production-image scenarios. Only the Feature transport is a private
// pipe; Run/Proof/Map/Travel/Publish/cleanup are the normal compiled backend.
struct SnapshotFixture {
  const char* name;
  unsigned variant;
};
inline constexpr SnapshotFixture kSnapshotPositive[] = {
  {"tester_3131_factory",0},{"all_aliases",1},{"all_unbound",2},
  {"single_bound",3},{"unknown_actions",4},{"physical_remap",5},
  {"nonphysical_poison",6},{"malformed_assignment",7},{"missing_profile",8},
  {"legacy_no_feature",9},{"precision_1000",10},{"layout_cache_full",11},
  {"incomplete_assignments",12},{"new_firmware_version",13}};
inline constexpr SnapshotFixture kSnapshotNegative[] = {
  {"foreign_board",100},{"foreign_pid",101},{"wrong_collection",102},
  {"transport_open_failure",103},{"unsupported_units",104},
  {"invalid_depth",105},{"echo_only_depth",106},{"short_feature",107},
  {"inconsistent_board",108},{"ownership_collision",109},{"cancelled",110}};
struct SnapshotFixtureResult {
  bool okay=true,zero=false,multiple=false,independentRelease=false,released=false;
  unsigned depth=0,commands=0,identity=0;
};
bool FixtureTransfer(HANDLE handle,bool write,mg::Report& report,DWORD length=65) {
  HidIoOperation op(handle);DWORD error=0,done=0;
  auto start=write?op.StartWrite(report.data(),length,&error):op.StartRead(report.data(),65,&error);
  if(start==HidIoOperation::StartResult::Failed) return false;
  if(start==HidIoOperation::StartResult::Pending && op.Wait(2000)!=WAIT_OBJECT_0) {
    op.CancelAndDrain(&done,&error);return false;
  }
  return op.Finish(&done,&error,false) && done==(write?length:65);
}
bool SnapshotFixtureCase(SnapshotFixture f,unsigned boardId) {
  const auto* fixtureModel=mg::Find(boardId);if(!fixtureModel)return false;
  const auto& matrix=*fixtureModel->matrix;unsigned physicalCount=0;
  for(unsigned slot=0;slot<128;++slot)physicalCount+=mg::Decode(matrix.data()+slot*4)!=0;
  if(boardId==3113 && f.variant==0)f.name="tester_3113_factory";
  Clear();g_stop=false;NativeAnalogRouting_Reset();
  {std::lock_guard<std::mutex> lock(g_evidenceLock);g_evidence={};}
  const bool positive=f.variant<100;
  Candidate c; c.privatePipe=true;c.attributes.VendorID=0x3151;
  c.attributes.ProductID=f.variant==101?0x5030:0x5029;
  c.caps.UsagePage=0xffff;c.caps.Usage=f.variant==102?1:2;c.caps.FeatureReportByteLength=65;
  c.path=LR"(\\.\pipe\HallJoy-Akko3131-)"+std::to_wstring(GetCurrentProcessId())+L"-"+std::to_wstring(f.variant);
  SnapshotFixtureResult result;
  std::array<halljoy::native_layout::Snapshot,16> savedLayouts{};
  if(f.variant==11) {
    std::lock_guard<std::mutex> lock(halljoy::native_layout::mutex);
    savedLayouts=halljoy::native_layout::sources;
    for(unsigned i=0;i<16;++i) halljoy::native_layout::sources[i].token=i+1;
  }
  if(f.variant==103 || f.variant==102 || f.variant==110) {
    if(f.variant==110) g_stop=true;
    Run(c);
    const bool pass=!Connected() && !g_factory.Owns(26);
    g_stop=false;NativeAnalogRouting_Reset();return pass;
  }
  if(f.variant==109) NativeAnalogRouting_Claim(0x3151,0x5029,c.path.c_str(),NativeAnalogProtocol::RongYuanStream);
  Handle server(CreateNamedPipeW(c.path.c_str(),PIPE_ACCESS_DUPLEX|FILE_FLAG_OVERLAPPED|FILE_FLAG_FIRST_PIPE_INSTANCE,
      PIPE_TYPE_BYTE|PIPE_READMODE_BYTE|PIPE_WAIT|PIPE_REJECT_REMOTE_CLIENTS,1,4096,4096,0,nullptr));
  if(!server) {wprintf(L"AKKO3131_PIPE_FAILED=%lu\n",GetLastError());return false;}
  std::thread responder([&] {
    Handle event(CreateEventW(nullptr,TRUE,FALSE,nullptr));OVERLAPPED connect{};connect.hEvent=event.v;
    const BOOL linked=ConnectNamedPipe(server.v,&connect);const auto error=linked?0:GetLastError();
    if(!linked && error!=ERROR_PIPE_CONNECTED) {
      DWORD transferred=0;
      if(error!=ERROR_IO_PENDING || WaitForSingleObject(event.v,2000)!=WAIT_OBJECT_0 ||
         !GetOverlappedResult(server.v,&connect,&transferred,FALSE)) {result.okay=false;return;}
    }
    unsigned units=f.variant==10?1000:200;
    while(!g_stop.load()) {
      mg::Report request{},reply{};
      if(!FixtureTransfer(server.v,false,request)) break;
      ++result.commands;
      unsigned checksum=0;for(unsigned i=1;i<=8;++i)checksum+=request[i];
      if((checksum&255)!=255 || request[0]) {result.okay=false;break;}
      const auto cmd=request[1],page=request[4];
      if(cmd==0x8f) {
        ++result.identity;unsigned board=f.variant==100?3132:boardId;
        if(f.variant==108 && result.identity>1) board=3132;
        reply[1]=0x8f;for(unsigned i=0;i<4;++i)reply[2+i]=std::uint8_t(board>>(8*i));
        const unsigned version=f.variant==13?0x717:boardId==3113?0x510:0x509;reply[8]=std::uint8_t(version&255);reply[9]=std::uint8_t(version>>8);
      } else if(cmd==0xe6) {
        if(f.variant!=9) {reply[1]=0xe6;reply[2]=0xaa;reply[3]=f.variant==104?7:f.variant==10?2:1;}
      } else if(cmd==0x84) {
        if(f.variant!=8) {reply[1]=0x84;reply[2]=2;}
      } else if(cmd==0x8a) {
        auto assigned=matrix;
        for(unsigned slot=0;slot<128;++slot) {
          const bool physical=mg::Decode(matrix.data()+4*slot)!=0;
          if((f.variant==1 || f.variant==2 || f.variant==3 || f.variant==4) && physical) {
            assigned[slot*4]=f.variant==4?0x20:0;assigned[slot*4+1]=0;
            assigned[slot*4+2]=f.variant==1?132:f.variant==3 && slot==14?26:0;
            assigned[slot*4+3]=0;
          }
          if(f.variant==5 && physical && assigned[slot*4+2]==26) assigned[slot*4+2]=82;
          if(f.variant==6 && !physical) {assigned[slot*4]=0x20;assigned[slot*4+1]=251;}
        }
        std::copy_n(assigned.begin()+page*64,64,reply.begin()+1);
        if(f.variant==7 || (f.variant==12 && page==4)) reply[1]=255;
      } else if(cmd==0xe5 && request[2]==0xfe && request[3]==1 && page<4) {
        const unsigned n=result.depth++;
        if(n==8) {
          result.zero=Connected() && g_token.load()==halljoy::layout_identity::Token("rongyuan-snapshot",fixtureModel->product) && g_mapped.load()==physicalCount;
          for(unsigned slot=0;slot<128;++slot) if(mg::Decode(matrix.data()+slot*4))
            result.zero=result.zero && !g_factory.PhysicalMilli(std::uint8_t(slot+1));
        }
        if(n==12 || n==16 || n==20) {
          bool values=true;
          for(unsigned slot=0;slot<128;++slot) if(mg::Decode(matrix.data()+slot*4)) {
            const auto hid=mg::Decode(matrix.data()+slot*4);
            unsigned raw=(units*fixtureModel->rangeUm/1000)*(slot%5+1)/5;
            if(n==16 && hid==26)raw=0;
            if(n==20)raw=0;
            values=values && g_factory.PhysicalMilli(std::uint8_t(slot+1))==mg::Normalize(raw,units,fixtureModel->rangeUm);
          }
          if(n==12) {
            result.multiple=values && g_factory.Active(GetTickCount64())==physicalCount;
            halljoy::native_layout::activeToken=g_token.load();
            if(f.variant==1) result.multiple=result.multiple && Get(132)==1000;
            else if(f.variant==2 || f.variant==4) result.multiple=result.multiple && !Owns(26);
            else result.multiple=result.multiple && (f.variant==5?Owns(82):Owns(26));
          }
          if(n==16) result.independentRelease=values && g_factory.Active(GetTickCount64())==physicalCount-1;
          if(n==20) result.released=values && !g_factory.Active(GetTickCount64());
        }
        for(unsigned i=0;i<32;++i) {
          const auto slot=unsigned(page)*32+i;
          const auto hid=mg::Decode(matrix.data()+slot*4);
          unsigned raw=0;
          if(n>=8 && n<16 && hid) raw=(units*fixtureModel->rangeUm/1000)*(slot%5+1)/5;
          if(n>=12 && n<16 && hid==26) raw=0;
          if(f.variant==6 && !hid) raw=65535;
          reply[1+2*i]=std::uint8_t(raw&255);reply[2+2*i]=std::uint8_t(raw>>8);
        }
        if(f.variant==105) reply[2]=255;
        if(f.variant==106) reply=request;
        if(f.variant==107) {
          FixtureTransfer(server.v,true,reply,20);break;
        }
        if(n>=20) g_stop=true;
      } else {result.okay=false;break;}
      if(!FixtureTransfer(server.v,true,reply)) break;
    }
  });
  Run(c);
  g_stop=true;CancelIoEx(server.v,nullptr);DisconnectNamedPipe(server.v);
  responder.join();
  bool pass=result.okay && !Connected() && !g_factory.Owns(26) && !g_assigned.Owns(26) && g_active==INVALID_HANDLE_VALUE && !g_token.load();
  if(positive) pass=pass && result.zero && result.multiple && result.independentRelease && result.released;
  else if(f.variant==105 || f.variant==106 || f.variant==107) pass=pass && result.depth==1;
  else pass=pass && result.depth==(f.variant==109?4u:0u);
  if(f.variant==11) {
    std::lock_guard<std::mutex> lock(halljoy::native_layout::mutex);
    halljoy::native_layout::sources=savedLayouts;
  }
  g_stop=false;NativeAnalogRouting_Reset();
  if(!pass) wprintf(L"AKKO3131_CASE_FAIL=%hs commands=%u depth=%u zero=%u multikey=%u independent=%u release=%u server=%u\n",
    f.name,result.commands,result.depth,result.zero,result.multiple,result.independentRelease,result.released,result.okay);
  return pass;
}
bool SnapshotSelfTest(unsigned boardId) {
  for(const auto& f:kSnapshotPositive) if(!SnapshotFixtureCase(f,boardId)) return false;
  for(const auto& f:kSnapshotNegative) if(!SnapshotFixtureCase(f,boardId)) return false;
  return true;
}

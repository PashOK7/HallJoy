// Runs only with the explicit headless self-test argument. No HID inventory.
struct Gt75Fixture {const char* name;unsigned variant;};
constexpr Gt75Fixture gt75Positive[]={
    {"factory81_fn",0},{"unknown_firmware",1},{"missing_info",2},
    {"unused_count",3},{"unused_base_magic",4},{"effective_profile_fallback",5},
    {"preenabled_preserved",6},{"settings_unavailable_passive",7},
    {"invalid_profile_passive",8},{"reset_armed_passive",9},
    {"lost_write_ack_readback",10},{"cleanup_after_profile_change",11},
    {"layout_cache_full",12},{"new_declared_maximum",13},{"missing_base_passive",14},
    {"attempt_spent_passive",15},{"unknown_frames_continue",16},{"quiet_hold_retained",17}
};
constexpr Gt75Fixture gt75Negative[]={
    {"foreign_id",100},{"boot_id",101},{"wrong_collection",102},{"open_failure",103},
    {"cancelled",104},{"partial_transfer",105},{"ownership_collision",106},{"invalid_depth",107},
    {"cancel_after_read",108}
};
struct Gt75FixtureOutput:Output {
    bool okay=true,zero=false,multiple=false,independent=false,released=false;
    unsigned count=0,closes=0;std::array<unsigned,128> values{};
    void Identity(const f75::Candidate& c,const f75::Info& i){
        Output::Identity(c,i);okay=okay && !connected.load() && halljoy::layout_identity::Match(token.load());
    }
    void Map(const f75::Assignments& map,bool metadata){
        Output::Map(map,metadata);okay=okay && !connected.load();
        for(auto s:f75::Positions())okay=okay && factoryValues.Owns(f75::Factory[s]);
    }
    void Sample(unsigned slot,unsigned raw,unsigned adc,unsigned maximum,unsigned route){
        Output::Sample(slot,raw,adc,maximum,route);if(collision.load())return;
        if(!count)zero=raw==0;++count;
        const auto old=values[slot];values[slot]=raw;
        okay=okay && connected.load() && factoryValues.PhysicalMilli(std::uint8_t(slot+1))==f75::Milli(raw,maximum);
        unsigned held=0;for(auto v:values)held+=v>0;
        if(held==81)multiple=true;
        if(multiple && held==80 && old && !raw)independent=true;
        if(independent && !held)released=true;
        // Quiet change-driven input remains measured, even beyond a UI age.
        if(raw)okay=okay && factoryValues.Read(f75::Factory[slot],GetTickCount64()+60000,500,true).milli==f75::Milli(raw,maximum);
    }
    void Closed(){Output::Closed();++closes;}
};
class Gt75FixtureIo {
    unsigned v_;bool queued_=false,stopped_=false,cleanup_=false,ready_=false;
    f75::Report reply_{};f75::Base base_{};f75::Settings settings_{};
    std::vector<f75::Report> stream_;std::size_t index_=0;
    f75::Settings initial_{};
    static f75::Report Depth(unsigned slot,unsigned raw,unsigned maximum){
        f75::Report r{};r[1]=0xa0;const auto hid=f75::Factory[slot];
        if(hid==0x409){r[2]=0xf0;r[3]=0xff;r[4]=1;}
        else if(hid>=224 && hid<=231){r[2]=0x10;r[3]=std::uint8_t(1u<<(hid-224));}
        else {r[2]=0x10;r[4]=std::uint8_t(hid);}
        r[7]=std::uint8_t(raw>>8);r[8]=std::uint8_t(raw);r[15]=std::uint8_t(maximum>>8);r[16]=std::uint8_t(maximum);return r;
    }
    void MakeStream(){
        const auto positions=f75::Positions();const auto max=v_==13?1300u:341u;
        if(v_==107){stream_.push_back(Depth(positions[0],342,341));stream_.push_back(Depth(positions[0],0,0));return;}
        stream_.push_back(Depth(positions[0],0,max));
        if(v_==16){f75::Report r{};r[1]=0xbb;stream_.push_back(r);r[1]=0xa0;r[2]=0x7f;stream_.push_back(r);}
        for(auto slot:positions)stream_.push_back(Depth(slot,max,max));
        stream_.push_back(Depth(positions[0],0,max));
        for(auto slot:positions)stream_.push_back(Depth(slot,0,max));
    }
public:
    unsigned opens=0,closes=0,writes=0;bool forbidden=false;
    explicit Gt75FixtureIo(unsigned v):v_(v){
        base_[1]=4;for(unsigned i=0;i<4;++i){base_[2+i]=std::uint8_t(i);settings_[64*i+2]=0xaa;settings_[64*i+3]=0xbb;settings_[64*i+4]=2;settings_[64*i+7]=0xa0;}
        if(v==3)base_[1]=255;
        if(v==4){base_[6]=22;base_[7]=33;}
        if(v==5){base_[0]=255;base_[2]=255;}
        if(v==6)settings_[7]|=8;
        if(v==8)settings_[2]=0;
        if(v==9)settings_[0]=1;
        initial_=settings_;MakeStream();
    }
    bool Open(const f75::Candidate&){++opens;return v_!=103;}
    void Close(){++closes;}
    void CleanupMode(bool x){cleanup_=x;}
    bool Stopping()const{return stopped_ || v_==104 || collision.load();}
    std::uint64_t Now()const{return GetTickCount64();}
    unsigned Error()const{return ERROR_TIMEOUT;}
    bool Send(const f75::Report& r,unsigned bytes,unsigned){
        const unsigned cmd=r[2],offset=f75::Offset(r);
        if(bytes!=65 || r[0] || r[1]!=0x55 || r[4]!=f75::Checksum(r) || (cmd!=3 && cmd!=4 && cmd!=5 && cmd!=6))forbidden=true;
        reply_=r;reply_[1]=0xaa;queued_=true;
        if(cmd==3){
            if(v_==2){queued_=false;return true;}
            reply_[5]=22;reply_[9]=v_==1?0x99:0x08;reply_[10]=v_==1?0xfe:1;
        }else if(cmd==4){
            if(v_==14){queued_=false;return true;}
            std::copy_n(base_.begin()+offset,r[5],reply_.begin()+9);
        }else if(cmd==5){
            if(v_==7){queued_=false;return true;}
            std::copy_n(settings_.begin()+offset,r[5],reply_.begin()+9);
        }else if(cmd==6){
            ++writes;
            if(r[5]!=1 || offset!=7 || (r[9]^settings_[offset])!=8)forbidden=true;
            settings_[offset]=r[9];
            if(v_==10 && !cleanup_)queued_=false;
        }
        reply_[4]=f75::Checksum(reply_);return true;
    }
    bool Receive(f75::Report& r,unsigned& bytes,unsigned timeout){
        if(queued_){queued_=false;r=reply_;bytes=65;return true;}
        if(timeout!=INFINITE)return false;
        if(index_<stream_.size()){
            ready_=true;r=stream_[index_++];bytes=v_==105?64:65;if(v_==108)stopped_=true;return true;
        }
        if(v_==11)base_[0]=1;
        stopped_=true;return false;
    }
    bool Restored()const{return settings_==initial_;}
};
bool Gt75FixtureCase(const Gt75Fixture& f){
    const auto v=f.variant;stop=false;collision=false;Clear();
    NativeAnalogRouting_Reset();halljoy::native_layout::sources={};halljoy::native_layout::activeToken=0;
    f75::Candidate c{L"isolated-gt75",L"GT75",0x3837,0x302d,1,0,65,65};
    if(v==100)c.pid=0x300d;if(v==101)c.pid=0x302f;if(v==102)c.page=0xff00;
    if(v==12){std::array<halljoy::native_layout::Key,1> key{{{4,4}}};for(unsigned i=1;i<=halljoy::native_layout::sources.size();++i)halljoy::native_layout::Publish(0x700000+i,key.data(),1);}
    if(v==106)NativeAnalogRouting_Claim(0x3837,0x302d,c.path.c_str(),NativeAnalogProtocol::AulaF75);
    Gt75FixtureIo io(v);Gt75FixtureOutput output;f75::Client<Gt75FixtureIo,Gt75FixtureOutput> client(io,output,v!=15);
    client.Run(c);
    bool okay=!io.forbidden && !connected.load() && !factoryValues.Owns(4) && !token.load() && io.Restored();
    if(v<100)okay=okay && output.okay && output.zero && output.multiple && output.independent && output.released && output.count>=163 && io.closes==1;
    else okay=okay && !output.count;
    if(v==6 || v==7 || v==8 || v==9 || v==14 || v==15)okay=okay && !io.writes;
    if(v<100 && v!=6 && v!=7 && v!=8 && v!=9 && v!=14 && v!=15)okay=okay && io.writes==2;
    if(v==100 || v==101 || v==102 || v==104)okay=okay && !io.opens;
    if(!okay)printf("MCHOSE_GT75_CASE_FAIL=%s count=%u writes=%u restored=%u output=%u\n",f.name,output.count,io.writes,io.Restored(),output.okay);
    collision=false;NativeAnalogRouting_Reset();Clear();halljoy::native_layout::sources={};return okay;
}
// Windows pipe tests below reuse the production overlapped transport, including
// notification just before read, pending cancellation, and unrelated topology.
HANDLE f75FixtureInventorySeen=nullptr;
bool F75TransportSwitchTest() {
    const f75::Candidate radio{L"usb",L"GT75",0x3837,0x302d,1,0,65,65};
    // Production overlapped I/O on isolated pipes, with an injected metadata
    // inventory. No real HID is opened, even for the topology check.
    for(unsigned phase=0;phase<3;++phase) {
        const auto path=std::wstring(L"\\\\.\\pipe\\HallJoy.GT75.switch.")+std::to_wstring(GetCurrentProcessId())+L"."+std::to_wstring(phase);
        Handle server(CreateNamedPipeW(path.c_str(),PIPE_ACCESS_DUPLEX,PIPE_TYPE_BYTE|PIPE_READMODE_BYTE|PIPE_WAIT,1,65,65,0,nullptr));
        Handle cancelled(CreateEventW(nullptr,TRUE,FALSE,nullptr)),changed(CreateEventW(nullptr,FALSE,FALSE,nullptr));
        Handle entered(CreateEventW(nullptr,TRUE,FALSE,nullptr)),finished(CreateEventW(nullptr,TRUE,FALSE,nullptr));
        Handle inspected(CreateEventW(nullptr,TRUE,FALSE,nullptr));
        if(!server || !cancelled || !changed || !entered || !finished || !inspected)return false;
        cancelEvent=cancelled.h;wake=changed.h;f75FixtureInventorySeen=inspected.h;
        HidTransport::Inventory inventory=phase==2?
            +[](const f75::Candidate* c){SetEvent(f75FixtureInventorySeen);return std::vector<f75::Candidate>{*c};}:
            +[](const f75::Candidate*){return std::vector<f75::Candidate>{};};
        HidTransport io(topology.load(),inventory);auto c=radio;c.path=path;
        if(!io.Open(c)){cancelEvent=wake=f75FixtureInventorySeen=nullptr;return false;}
        const bool attached=ConnectNamedPipe(server.h,nullptr)!=FALSE || GetLastError()==ERROR_PIPE_CONNECTED;
        if(!attached){io.Close();cancelEvent=wake=f75FixtureInventorySeen=nullptr;return false;}
        bool accepted=true;
        if(!phase)Notify();
        std::thread reader([&]{
            SetEvent(entered.h);f75::Report frame{};unsigned bytes=0;
            accepted=io.Receive(frame,bytes,INFINITE);SetEvent(finished.h);
        });
        bool pass=WaitForSingleObject(entered.h,3000)==WAIT_OBJECT_0;
        if(phase)Notify();
        if(phase==2) {
            pass=pass && WaitForSingleObject(inspected.h,3000)==WAIT_OBJECT_0;
            f75::Report actual{};DWORD sent=0;
            pass=(WriteFile(server.h,actual.data(),65,&sent,nullptr)!=FALSE && sent==65) && pass;
        }
        const bool completed=WaitForSingleObject(finished.h,3000)==WAIT_OBJECT_0;
        if(!completed)CancelIoEx(active,nullptr);
        reader.join();
        pass=pass && completed && (phase==2?(accepted && !io.Stopping() && !io.Error()):(!accepted && io.Stopping() && io.Error()==ERROR_OPERATION_ABORTED));
        io.Close();cancelEvent=wake=f75FixtureInventorySeen=nullptr;
        if(!pass){printf("GT75_TRANSPORT_FAIL phase=%u accepted=%u error=%u\n",phase,accepted,io.Error());return false;}
    }
    return true;
}
bool Gt75SelfTest(){
    for(const auto& f:gt75Positive)if(!Gt75FixtureCase(f))return false;
    for(const auto& f:gt75Negative)if(!Gt75FixtureCase(f))return false;
    return F75TransportSwitchTest();
}


// Included inside the backend namespace. Hardware-isolated fixtures execute the
// production Client and Output, including physical publication and cleanup.
struct F75Fixture {const char* name;unsigned variant;};
constexpr F75Fixture f75Positive[]={
    {"usb_2729",0},{"usb_2742",1},{"radio_65",2},{"radio_33",3},
    {"all_aliases",4},{"all_unbound",5},{"single_assignment",6},
    {"unknown_actions",7},{"physical_remap",8},{"missing_assignments",9},
    {"partial_assignments",10},{"unknown_firmware_metadata",11},
    {"missing_info",12},{"layout_cache_full",13},{"echo68_monitor66",14},
    {"no68_monitor66",15},{"monitor_no_ack_live_sample",16},
    {"radio_local_info",17},{"radio_echo68_monitor33",18},
    {"out_of_scale68_monitor66",19},{"all_zero_first_reply",20},
    {"monitor_mixed_frames_then_depth",21}
};
constexpr F75Fixture f75Negative[]={
    {"foreign_usb",100},{"foreign_receiver_name",101},{"wrong_collection",102},
    {"foreign_remote",103},{"open_failure",104},{"cancelled",105},
    {"partial_transfers",106},{"ownership_collision",107},
    {"invalid_stream_range",108},{"radio_disconnect",109},{"ack_only_no_analog",110}
};
struct F75FixtureOutput:Output {
    bool okay=true,zero=false,multiple=false,independent=false,released=false;
    unsigned count=0,closed=0;
    std::array<unsigned,128> raw{};
    void Identity(const f75::Candidate& c,const f75::Info& info) {
        Output::Identity(c,info);
        okay=okay && token.load()==f75::Token(c) && halljoy::layout_identity::Match(token.load()) && !connected.load();
    }
    void Map(const f75::Assignments& map,bool metadata) {
        Output::Map(map,metadata);
        okay=okay && !connected.load();
        for(auto slot:f75::Positions())okay=okay && factoryValues.Owns(f75::Factory[slot]);
        halljoy::native_layout::activeToken=token.load();
    }
    void Sample(unsigned slot,unsigned value,unsigned adc,unsigned stroke,unsigned route) {
        const auto prior=raw[slot];
        Output::Sample(slot,value,adc,stroke,route);
        if(collision.load())return;
        ++count;raw[slot]=value;
        okay=okay && connected.load() && factoryValues.PhysicalMilli(std::uint8_t(slot+1))==f75::Milli(value,stroke);
        if(assignments[slot])okay=okay && assignedValues.PhysicalMilli(std::uint8_t(slot+1))==f75::Milli(value,stroke);
        unsigned held=0;for(auto x:raw)held+=x>0;
        if(count==1)zero=value==0;
        if(held==80)multiple=true;
        if(multiple && held==79 && prior && !value)independent=true;
        if(multiple && independent && !held)released=true;
        // Verify duplicate HID publication uses the live maximum independently
        // of the factory geometry, including one alias releasing before another.
        if(assignments[slot]) {
            unsigned peak=0;
            for(auto other:f75::Positions())if(assignments[other]==assignments[slot])peak=std::max(peak,f75::Milli(raw[other],stroke));
            okay=okay && assignedValues.Read(assignments[slot],GetTickCount64(),500,true).milli==peak;
        }
    }
    void Closed() {Output::Closed();++closed;}
};
class F75FixtureIo {
    const F75Fixture& fixture_;
    f75::Candidate candidate_;
    f75::Report queued_{};
    std::vector<f75::Report> stream_;
    std::size_t streamIndex_=0;
    unsigned phase_=1,statusCalls_=0,error_=0;
    std::array<bool,128> covered_{};
    bool queued=false,stop_=false,stopAfter_=false;
    static void Put(f75::Report& r,unsigned at,unsigned value) {r[at]=std::uint8_t(value);r[at+1]=std::uint8_t(value>>8);}
    static f75::Report Depth(unsigned slot,unsigned raw,unsigned stroke=34) {
        f75::Report r{};r[1]=0x55;r[2]=0xfb;r[3]=std::uint8_t(slot);r[4]=1;
        Put(r,5,3000);Put(r,7,1000);Put(r,9,2000+slot);Put(r,11,raw);Put(r,13,stroke);return r;
    }
    void MakeStream() {
        const auto v=fixture_.variant;
        if(v==110){f75::Report r{};r[1]=0x55;r[2]=0x66;stream_.push_back(r);return;}
        if(v==109) {f75::Report r{};r[1]=0x55;r[2]=0xfc;r[3]=4;stream_.push_back(r);return;}
        if(v==108){stream_.push_back(Depth(17,65535));return;}
        const auto slots=f75::Positions();
        if(v==16)stream_.push_back(Depth(slots[0],0));
        else {f75::Report ack{};ack[1]=0x55;ack[2]=0x66;stream_.push_back(ack);stream_.push_back(Depth(slots[0],0));}
        if(v==21) {
            f75::Report other{};other[1]=0x55;other[2]=0xe1;
            stream_.insert(stream_.begin()+1,{Depth(slots[0],100),other,f75::Report{},Depth(slots[0],65535)});
        }
        stream_.push_back(Depth(127,65535)); // Nonphysical extra is not a source veto.
        for(auto slot:slots)stream_.push_back(Depth(slot,34*(slot%5+1)*2));
        stream_.push_back(Depth(slots[0],0));
        for(auto slot:slots)stream_.push_back(Depth(slot,0));
    }
public:
    unsigned opens=0,closes=0,commands=0,monitorOn=0,monitorOff=0,depthQueries=0;
    bool forbidden=false;
    explicit F75FixtureIo(const F75Fixture& f):fixture_(f){}
    bool Open(const f75::Candidate& c) {++opens;candidate_=c;if(fixture_.variant==104){error_=ERROR_SHARING_VIOLATION;return false;}return true;}
    void Close(){++closes;}
    bool Stopping()const{return stop_ || fixture_.variant==105 || collision.load();}
    std::uint64_t Now()const{return GetTickCount64();}
    unsigned Error()const{return error_;}
    bool Send(const f75::Report& r,unsigned bytes,unsigned) {
        ++commands;const unsigned cmd=r[2],v=fixture_.variant;
        if(bytes!=candidate_.outputBytes || r[0] || r[1]!=0xaa)forbidden=true;
        if(cmd==0x67){++monitorOff;return true;}
        if(cmd==0x66){++monitorOn;MakeStream();return true;}
        queued_=r;queued_[1]=0x55;queued=true;
        if(cmd==0x10) {
            if(v==12){queued=false;return true;}
            std::array<std::uint8_t,56> info{};
            auto word=[&](unsigned at,unsigned x){info[at]=std::uint8_t(x);info[at+1]=std::uint8_t(x>>8);};
            word(4,v==17?0x0c45:0x38a6);word(6,v==103?0x9999:v==17?0xfefe:candidate_.pid==0x2729?0x2729:0x2742);
            word(8,v==11?0xfe99:v==110?0x0155:0x103);word(12,0x1234);info[29]=255;info[30]=255;
            const unsigned offset=f75::Word(r.data()+4);
            for(unsigned i=0;i<r[3];++i)queued_[9+i]=offset+i<info.size()?info[offset+i]:std::uint8_t(i+cmd);
        } else if(cmd==0x12) {
            const unsigned offset=f75::Word(r.data()+4);
            if(v==9 || (v==10 && offset>=48)){queued=false;return true;}
            std::array<std::uint8_t,512> map{};
            for(unsigned slot=0;slot<128;++slot) {
                if(v==4){map[slot*4]=2;map[slot*4+2]=132;}
                if(v==5 || (v==6 && slot!=17))map[slot*4]=3;
                if(v==7)map[slot*4]=77;
                if(v==8 && slot==17){map[slot*4]=2;map[slot*4+2]=82;}
                if(!f75::Factory[slot])for(unsigned j=0;j<4;++j)map[slot*4+j]=255;
            }
            for(unsigned i=0;i<r[3];++i)queued_[9+i]=map[offset+i];
        } else if(cmd==0x68) {
            ++depthQueries;++statusCalls_;
            if(v==14 || v==18 || v==21 || v==110)return true; // Reviewed generic ACK echo.
            if(v==15 || v==16 || v>=108){queued=false;return true;}
            const bool proof=statusCalls_==1;
            for(unsigned i=0;i<unsigned(r[3])/8;++i) {
                const unsigned slot=r[9+8*i];
                const unsigned raw=v==19?65535:proof?0:phase_==1?34*(slot%5+1)*2:phase_==2?(slot==17?0:34*(slot%5+1)*2):0;
                queued_[9+8*i]=0;Put(queued_,10+8*i,proof && v==20?0:2000+slot);Put(queued_,12+8*i,raw);
                if(!proof)covered_[slot]=true;
            }
            if(!proof) {
                bool all=true;for(auto slot:f75::Positions())all=all && covered_[slot];
                if(all){covered_.fill(false);++phase_;if(phase_==4)stopAfter_=true;}
            }
        } else forbidden=true;
        return true;
    }
    bool Receive(f75::Report& reply,unsigned& bytes,unsigned) {
        bytes=candidate_.inputBytes;if(fixture_.variant==106)--bytes;
        if(queued){reply=queued_;queued=false;if(stopAfter_)stop_=true;return true;}
        if(streamIndex_<stream_.size()) {
            if(fixture_.variant==21 && streamIndex_==1)--bytes;
            reply=stream_[streamIndex_++];return true;
        }
        error_=WAIT_TIMEOUT;
        if(monitorOn)stop_=true;
        return false;
    }
};
bool F75FixtureCase(const F75Fixture& f) {
    const auto v=f.variant;const bool positive=v<100;
    NativeAnalogRouting_Reset();stop=false;collision=false;Clear();
    auto saved=halljoy::native_layout::sources;
    if(v==13){std::lock_guard<std::mutex> lock(halljoy::native_layout::mutex);for(unsigned i=0;i<16;++i)halljoy::native_layout::sources[i].token=1000+i;}
    f75::Candidate c{L"HallJoy isolated F75 fixture",L"AULA F75 HE Dongle",0x38a6,0x2729,0xff68,0x61,65,65};
    if(v==1)c.pid=0x2742;
    if(v==2 || v==3 || v==17 || v==18 || v==101 || v==109){c.vid=0x0c45;c.pid=0xfefe;c.page=0xff60;}
    if(v==3 || v==18)c.inputBytes=c.outputBytes=33;
    if(v==100)c.pid=0x2728;
    if(v==101)c.name=L"Unrelated receiver";
    if(v==102)c.page=0x01;
    if(v==107)NativeAnalogRouting_Claim(std::uint16_t(c.vid),std::uint16_t(c.pid),c.path.c_str(),NativeAnalogProtocol::Hex80);
    F75FixtureIo io(f);F75FixtureOutput output;
    {f75::Client<F75FixtureIo,F75FixtureOutput> client(io,output);client.Run(c);}
    bool pass=output.okay && !io.forbidden && !connected.load() && !token.load() && !factoryValues.Owns(26) && !assignedValues.Owns(26);
    if(positive)pass=pass && output.zero && output.multiple && output.independent && output.released && io.opens==1 && io.closes==1 && output.closed==1;
    else pass=pass && !output.count;
    if(v==100 || v==101 || v==102 || v==105)pass=pass && !io.opens && !io.commands;
    if(v==103)pass=pass && !io.monitorOn && !io.depthQueries;
    if(io.monitorOn)pass=pass && io.monitorOff==1;
    if(positive && v>=14 && v<=19 && v!=17)pass=pass && io.monitorOn==1;
    if(v==21 || v==110) {
        const auto received=[](f75::MonitorFrame frame){return stats.monitorFrames[static_cast<std::size_t>(frame)];};
        pass=pass && io.monitorOn==1 && received(f75::MonitorFrame::Ack)==1 &&
            received(f75::MonitorFrame::Depth)==output.count;
        if(v==21)pass=pass && received(f75::MonitorFrame::Framing)==1 &&
            received(f75::MonitorFrame::OtherOpcode)==1 && received(f75::MonitorFrame::OtherHeader)==1 &&
            received(f75::MonitorFrame::InvalidRange)==1 && received(f75::MonitorFrame::InvalidSlot)==1;
        else pass=pass && stats.replies==stats.queries-1 && stats.samples==0;
    }
    if(v==13){std::lock_guard<std::mutex> lock(halljoy::native_layout::mutex);halljoy::native_layout::sources=saved;}
    if(!pass)printf("AULA_F75_CASE_FAIL=%s samples=%u zero=%u multi=%u independent=%u release=%u opens=%u closes=%u commands=%u monitor=%u/%u output=%u forbidden=%u\n",f.name,output.count,output.zero,output.multiple,output.independent,output.released,io.opens,io.closes,io.commands,io.monitorOn,io.monitorOff,output.okay,io.forbidden);
    stop=false;collision=false;NativeAnalogRouting_Reset();return pass;
}
HANDLE f75FixtureInventorySeen=nullptr;
bool F75TransportSwitchTest() {
    const f75::Candidate radio{L"radio",L"AULA F75 HE Dongle",0x0c45,0xfefe,0xff60,0x61,33,33};
    auto usb=radio;usb.path=L"usb";usb.vid=0x38a6;usb.pid=0x2742;usb.page=0xff68;
    std::vector<f75::Candidate> order{radio,usb};f75::PreferWired(order);
    if(order.front().path!=L"usb" || order.back().path!=L"radio" ||
        !f75::NeedsSwitch(radio,order) || f75::NeedsSwitch(usb,order) || f75::NeedsSwitch(radio,{radio}))return false;
    // Production overlapped I/O on isolated pipes, with an injected metadata
    // inventory. No real HID is opened, even for the topology check.
    for(unsigned phase=0;phase<3;++phase) {
        const auto path=std::wstring(L"\\\\.\\pipe\\HallJoy.F75.switch.")+std::to_wstring(GetCurrentProcessId())+L"."+std::to_wstring(phase);
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
            pass=(WriteFile(server.h,actual.data(),33,&sent,nullptr)!=FALSE && sent==33) && pass;
        }
        const bool completed=WaitForSingleObject(finished.h,3000)==WAIT_OBJECT_0;
        if(!completed)CancelIoEx(active,nullptr);
        reader.join();
        pass=pass && completed && (phase==2?(accepted && !io.Stopping() && !io.Error()):(!accepted && io.Stopping() && io.Error()==ERROR_OPERATION_ABORTED));
        io.Close();cancelEvent=wake=f75FixtureInventorySeen=nullptr;
        if(!pass){printf("AULA_F75_TRANSPORT_FAIL phase=%u accepted=%u error=%u\n",phase,accepted,io.Error());return false;}
    }
    return true;
}
bool F75SelfTest() {
    for(const auto& f:f75Positive)if(!F75FixtureCase(f))return false;
    for(const auto& f:f75Negative)if(!F75FixtureCase(f))return false;
    Output output;output.Reset();
    referenceIdentity=0x38a62729;
    const auto generation=referenceGeneration.load();
    for(const auto hid:{4u,7u,4u})RecordDigitalReference(hid,true,referenceIdentity.load(),generation);
    for(const auto hid:{4u,7u,4u})RecordDigitalReference(hid,false,referenceIdentity.load(),generation);
    const bool referenceOkay=stats.referenceDowns==2 && stats.referenceUps==2 && stats.referenceHeld==0 &&
        stats.referencePeakHeld==2 && !stats.samples && !connected.load() && !factoryValues.Owns(4);
    Clear();
    RecordDigitalReference(4,true,0x38a62729,generation);
    if(!referenceOkay || stats.referenceDowns!=2)return false;
    return F75TransportSwitchTest();
}

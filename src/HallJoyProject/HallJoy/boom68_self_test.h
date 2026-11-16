// Included inside the shared Sonix backend. No physical HID is opened.
struct BoomFixtureOutput:Output {
    bool okay=true,zero=false,multiple=false,independent=false,released=false;
    unsigned count=0,closed=0;std::array<unsigned,128> values{};
    void Identity(const f75::Candidate& c,const f75::Info& i){
        Output::Identity(c,i);okay=okay && f75::Token(c)!=0 && halljoy::layout_identity::Match(f75::Token(c)) && !connected.load();
    }
    void Map(const f75::Assignments& map,bool available){
        Output::Map(map,available);
        for(auto hid:halljoy::boom68::Factory)if(hid)okay=okay && factoryValues.Owns(hid);
    }
    void Sample(unsigned slot,unsigned raw,unsigned adc,unsigned stroke,unsigned route){
        const auto prior=values[slot];Output::Sample(slot,raw,adc,stroke,route);
        if(collision.load())return;
        ++count;values[slot]=raw;
        okay=okay && connected.load() && factoryValues.PhysicalMilli(std::uint8_t(slot+1))==f75::Milli(raw,stroke);
        if(count==1){
            NativeAnalogBackendTelemetry telemetry{};Telemetry(&telemetry);
            okay=okay && telemetry.connected && telemetry.genericProtocol==(route!=3);
        }
        unsigned held=0;for(auto v:values)held+=v>0;
        if(count==1)zero=raw==0;
        if(held==67)multiple=true;
        if(multiple && held==66 && prior && !raw)independent=true;
        if(independent && !held)released=true;
    }
    void Closed(){++closed;Output::Closed();}
};
struct BoomIo:halljoy::boom68::fixture::Io {
    using Io::Io;
    bool Stopping()const{return Io::Stopping() || collision.load();}
};
constexpr unsigned boomPositive[]={0,1,2,3,4,5,6,7,9,10,11,12,13,14,15,16,17,18};
constexpr unsigned boomNegative[]={100,101,102,103,104,105,106,107,108,109,110,111,112,113};
bool BoomFixtureCase(unsigned variant){
    NativeAnalogRouting_Reset();stop=false;collision=false;Clear();
    auto saved=halljoy::native_layout::sources;
    if(variant==11){std::lock_guard<std::mutex> lock(halljoy::native_layout::mutex);for(unsigned i=0;i<16;++i)halljoy::native_layout::sources[i].token=1000+i;}
    f75::Candidate c{L"HallJoy isolated BOOM68 fixture",L"BOOM68-USB",0x0c45,0x80b5,0xff68,0x61,65,65,0x0110};
    if(variant==7)c.version=0xffff;
    if(variant==112)c.name=L"Another 80B5 product";
    if(variant==9)c.inputBytes=c.outputBytes=33;
    if(variant==100)c.pid=0x80b4;
    if(variant==101)c.page=1;
    if(variant==105)NativeAnalogRouting_Claim(std::uint16_t(c.vid),std::uint16_t(c.pid),c.path.c_str(),NativeAnalogProtocol::Hex80);
    BoomIo io(variant);BoomFixtureOutput output;
    {halljoy::boom68::Client<BoomIo,BoomFixtureOutput,halljoy::boom68::fixture::Rom> client(io,output);client.Run(c);}
    bool pass=output.okay && !io.forbidden && !connected.load() && !token.load();
    if(variant<100)pass=pass && output.zero && output.multiple && output.independent && output.released && io.opens==1 && io.closes==1 && output.closed==1;
    else pass=pass && !output.count;
    if(variant==100 || variant==101 || variant==103 || variant==105 || variant==112)pass=pass && !io.opens && !io.commands;
    if(variant==11){std::lock_guard<std::mutex> lock(halljoy::native_layout::mutex);halljoy::native_layout::sources=saved;}
    if(!pass)printf("BOOM68_CASE_FAIL=%u samples=%u zero=%u multi=%u independent=%u released=%u opens=%u closes=%u commands=%u forbidden=%u\n",variant,output.count,output.zero,output.multiple,output.independent,output.released,io.opens,io.closes,io.commands,io.forbidden);
    collision=false;NativeAnalogRouting_Reset();return pass;
}
bool BoomEvidenceTest(){
    Stats s{};s.vid=0x0c45;s.pid=0x80b5;s.queries=s.replies=s.statusQueries=s.samples=s.batches=s.releases=s.changes=s.ignored=s.monitorOn=s.monitorOff=~std::uint64_t(0);
    s.rawPeak=s.adcMin=s.adcMax=s.strokeMin=s.strokeMax=s.meanUs=s.maxUs=~0u;
    s.fingerprints.fill(~std::uint64_t(0));std::string joined;unsigned records=0;
    const bool okay=EmitSonixTrialEvidence(s,true,128,128,128,128,[&](const char* text){if(strlen(text)<384)++records;joined+=text;joined+='\n';});
    return okay && records==7 && joined.find("no_analog_yet=")!=std::string::npos && joined.find("release_observation_pending=")!=std::string::npos && joined.find("status_scale_hardware_unverified=")!=std::string::npos && joined.find("no_host_expiry=1")!=std::string::npos;
}
bool BoomSelfTest(){
    for(auto v:boomPositive)if(!BoomFixtureCase(v))return false;
    for(auto v:boomNegative)if(!BoomFixtureCase(v))return false;
    return BoomEvidenceTest() && halljoy::boom68::Stock110::CodeHash==0x1ea95d27cac2a12eull;
}

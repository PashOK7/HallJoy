// Isolated production parser/publication test. Reconstructed frames, no HID opens.
struct SparkSu75Fixture final : SparkTestTransport {
    unsigned pid=0x3007,variant=0,writes=0;bool pending=false,badType=false,badRoute=false,failRead=false;
    std::array<std::uint8_t,65> reply{};
    std::array<std::uint16_t,126> depth{};
    bool Write(const std::uint8_t* p,DWORD len,DWORD* error) override {
        ++writes;if(error)*error=0;
        if(len!=65 || p[0]!=0)return false;
        ++p;reply.fill(0);reply[1]=p[0];reply[2]=p[1];pending=true;
        if(p[0]==1 && p[1]==2) {
            reply[3]=badType?2:1;
            const auto board=(variant==9?0x00300055u:pid==0x3002?0x00300002u:0x00300007u);
            for(int i=0;i<4;++i)reply[5+i]=std::uint8_t(board>>(24-i*8));
            if(variant==8){reply[9]=9;reply[10]=8;reply[11]=7;reply[12]=6;}
        } else if(p[0]==3 && p[1]==1) {
            if(p[3]>=6 || (variant==5 && p[3]==2)){pending=false;return true;}
            reply[3]=p[2];reply[4]=p[3];
            for(unsigned col=0;col<21;++col) {
                const auto i=p[3]*21+col;auto code=halljoy::sparklink::su75::Factory[i];
                if(variant==1)code=0;
                if(variant==2)code=code?26:0;
                if(variant==3)code=code?0xabcd:0;
                if(variant==4 && code==41)code=26;
                if(variant==6 && !code)code=4; // Benign assignments in nonphysical slots.
                if(variant==7 && code!=41)code=0;
                reply[5+2*col]=std::uint8_t(code);reply[6+2*col]=std::uint8_t(code>>8);
            }
        } else if(p[0]==4 && p[1]==3 && p[2]==1 && p[3]<6) {
            reply[3]=1;reply[4]=badRoute?std::uint8_t(p[3]+1):p[3];
            for(unsigned col=0;col<21;++col){const auto value=depth[p[3]*21+col];reply[5+2*col]=std::uint8_t(value);reply[6+2*col]=std::uint8_t(value>>8);}
        } else return false;
        return true;
    }
    bool Read(std::uint8_t* p,DWORD len,DWORD* count,DWORD* error) override {
        if(!pending || failRead){if(count)*count=0;if(error)*error=ERROR_DEVICE_NOT_CONNECTED;return false;}
        if(len<reply.size())return false;
        memcpy(p,reply.data(),reply.size());pending=false;if(count)*count=DWORD(reply.size());if(error)*error=0;return true;
    }
};
bool Backend_TestSparkSu75() {
    if(g_sparkHandle || g_sparkConnected.load())return false;
    bool pass=true;int line=0;
    auto check=[&](bool value,int at){if(!value && pass){pass=false;line=at;}return value;};
#define SU_CHECK(x) do{if(!check(bool(x),__LINE__))goto finish;}while(0)
    {
    SparkSu75Fixture io;
    struct Restore {~Restore(){SparkReleasePublishedInput();Spark_ResetKeyState();g_sparkTestTransport=nullptr;g_sparkHandle=nullptr;g_sparkExactInterfaceId.store(0);halljoy::native_layout::activeToken.store(0);SupportLog_SetSparkEvidenceProvider(nullptr);}} restore;
    g_sparkTestTransport=&io;g_sparkHandle=reinterpret_cast<HANDLE>(std::uintptr_t(1));
    g_sparkExactInterfaceId.store(0x5355373500000001ull);
    g_sparkInputReportLen=65;g_sparkOutputReportLen=65;g_sparkWriteCapable.store(true);g_sparkUsagePage.store(0xffb0);
    for(unsigned pid:{0x3002u,0x3007u})for(unsigned variant=0;variant<10;++variant) {
        Spark_ResetKeyState();io.pid=pid;io.variant=variant;io.depth.fill(0);
        SU_CHECK(halljoy::sparklink::ProbeInterface(0x1ca6,pid,0xffb0,1));
        std::uint8_t info[64]{};SU_CHECK(SparkQueryDeviceInfo(info) && info[2]==1);
        SparkObserveInfo(0x1ca6,pid,info);g_sparkConnectedVid.store(0x1ca6);g_sparkConnectedPid.store(std::uint16_t(pid));
        const auto token=g_sparkModelToken.load();SU_CHECK((token!=0)==(variant!=9));
        std::array<std::uint16_t,21> row{};SU_CHECK(SparkQueryRouteRow(0,row)); // zero is valid proof
        SparkDiscoverLayout();SU_CHECK(g_sparkRowCount.load()==6);
        g_sparkConnected.store(true);
        NativeAnalogBackendTelemetry t{};BackendNative_SparkTelemetry(&t);SU_CHECK(t.verifiedLayoutToken==0);
        for(unsigned r=0;r<6;++r){SU_CHECK(SparkQueryRouteRow(std::uint8_t(r),row));const auto now=GetTickCount64();SparkRecordRouteResult(std::uint8_t(r),true,now);SparkCommitRouteRow(std::uint8_t(r),row,now,6);}
        BackendNative_SparkTelemetry(&t);SU_CHECK(variant==9?t.verifiedLayoutToken==0:t.verifiedLayoutToken==token);
        io.depth[0]=1750;io.depth[1]=3500;io.depth[2]=875;
        SU_CHECK(SparkQueryRouteRow(0,row));auto now=GetTickCount64();SparkRecordRouteResult(0,true,now);SU_CHECK(SparkCommitRouteRow(0,row,now,6));
        SU_CHECK(BackendNative_SparkGetMilli(41)==500 && BackendNative_SparkGetMilli(58)==1000 && BackendNative_SparkGetMilli(59)==250);
        if(token) {
            const auto map=halljoy::native_layout::Read(token);SU_CHECK(map.complete && map.count==81);
            SU_CHECK(halljoy::layout_identity::Match(token)!=nullptr);
            halljoy::native_layout::activeToken.store(token);
            if(variant==1 || variant==3)SU_CHECK(!BackendNative_SparkOwnsHid(41));
            if(variant==2)SU_CHECK(BackendNative_SparkGetMilli(26)==1000);
            if(variant==4)SU_CHECK(BackendNative_SparkGetMilli(26)==500);
            halljoy::native_layout::activeToken.store(0);
        }
        using namespace halljoy::analog_provider_v2;
        AnalogSnapshotHeaderV2 header{};std::array<AnalogDeviceV2,1> devices{};std::array<AnalogSampleV2,128> samples{};
        SU_CHECK(BackendNative_SparkGetSnapshotV2({&header,devices.data(),devices.size(),samples.data(),samples.size()}) && header.deviceCount==1 && header.sampleCount>=81);
        io.depth.fill(0);SU_CHECK(SparkQueryRouteRow(0,row));now=GetTickCount64();SparkRecordRouteResult(0,true,now);SU_CHECK(SparkCommitRouteRow(0,row,now,6));
        SU_CHECK(BackendNative_SparkGetMilli(41)==0 && BackendNative_SparkGetMilli(58)==0 && BackendNative_SparkGetMilli(59)==0);
        if(token)SU_CHECK(g_sparkPhysicalReleases.load()==3);
        SparkEvidence();SparkReleasePublishedInput();SU_CHECK(!BackendNative_SparkPresent() && g_sparkModelToken.load()==0 && !halljoy::native_layout::Read(token).complete);
    }
    const unsigned before=io.writes;
    SU_CHECK(!halljoy::sparklink::ProbeInterface(0x1038,0x3007,0xffb0,1));
    SU_CHECK(!halljoy::sparklink::ProbeInterface(0x1ca6,0x3007,0xffa0,1));
    SU_CHECK(!halljoy::sparklink::ProbeInterface(0x1ca6,0x3007,0xffb0,2));SU_CHECK(io.writes==before);
    io.variant=0;io.badType=true;std::uint8_t info[64]{};SU_CHECK(SparkQueryDeviceInfo(info) && info[2]!=1);io.badType=false;
    std::array<std::uint16_t,21> row{};io.badRoute=true;SU_CHECK(!SparkQueryRouteRow(0,row));io.badRoute=false;
    io.failRead=true;SU_CHECK(!SparkQueryDeviceInfo(info));io.failRead=false;
    }
finish:
    printf("SPARK_SU75_SELF_TEST=%s scenarios=20 negatives=6 physical_channels=81 proof_to_subscribe=1 first_sample=1 multikey=1 release=1 cleanup=1 hardware_opened=0 line=%d\n",pass?"PASS":"FAIL",line);
#undef SU_CHECK
    return pass;
}

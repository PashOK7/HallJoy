#pragma once
#include "corsair_vanguard_protocol.h"

namespace halljoy::corsair_vanguard {
// Shared production transaction state machine. Transport owns exact-sized,
// synchronous exchanges, cancellation and reply framing. No configuration-write
// or calibration-start builders exist here.
template<class Transport> class Client {
    Transport& io_;
    std::uint8_t session_=0;
    std::uint32_t token_=0;
    Stage stage_=Stage::Idle;
    std::array<std::uint16_t,100> previousRaw_{};
    bool haveRaw_=false;
    void SetStage(Stage next) {
        stage_=next;
        if constexpr(requires {io_.ObserveStage(next);})io_.ObserveStage(next);
    }
    bool Exchange(Frame& request,Frame& reply,ReplyKind kind=ReplyKind::Normal,bool cleanup=false) {
        if(!io_.Exchange(request,reply,kind,cleanup))return Rejected(&reject,Reject::Transport);
        if(!Prefix(request,reply,kind))return Rejected(&reject,Reject::ReplyPrefix);
        return true;
    }
    bool Property(std::uint16_t property,Frame& reply,int sub=-1) {
        auto request=Request(session_,2);request[4]=static_cast<std::uint8_t>(property);request[5]=static_cast<std::uint8_t>(property>>8);
        if(sub>=0)request[6]=static_cast<std::uint8_t>(sub);
        return Exchange(request,reply,sub>=0?ReplyKind::Calibration:ReplyKind::Normal);
    }
    bool File(std::uint16_t id,std::uint8_t handle,std::size_t maximum,std::vector<std::uint8_t>& data) {
        // Only the preflighted caller may use 4B; an absent file is never opened.
        currentFile=id;fileBytes=0;
        if(id!=0x11 && id!=0x4b)return Rejected(&reject,Reject::FileSize);
        auto request=Request(session_,13);request[4]=handle;request[5]=static_cast<std::uint8_t>(id);request[6]=static_cast<std::uint8_t>(id>>8);
        Frame reply{};
        if(!Exchange(request,reply))return false;
        request=Request(session_,9);request[4]=handle;
        if(!Exchange(request,reply))return false;
        const auto bytes=U32(reply.data()+7);
        fileBytes=bytes;
        if(!bytes || bytes>maximum)return Rejected(&reject,Reject::FileSize);
        data.clear();data.reserve(bytes);
        while(data.size()<bytes) {
            request=Request(session_,8);request[4]=handle;
            if(!Exchange(request,reply))return false;
            const auto count=std::min<std::size_t>(bytes-data.size(),1019);
            data.insert(data.end(),reply.begin()+5,reply.begin()+5+count);
        }
        if(id==0x11) {
            request=Request(session_,5);request[4]=1;request[5]=handle;
            if(!Exchange(request,reply))return false;
        }
        // Never opcode5-close 4B: the firmware treats it as a settings reload.
        return true;
    }
public:
    explicit Client(Transport& io):io_(io){}
    ~Client(){Close();}
    Client(const Client&)=delete;Client& operator=(const Client&)=delete;
    Stage CurrentStage() const noexcept{return stage_;}
    Reject reject=Reject::None;
    std::uint16_t observedVid=0,observedPid=0,bragi=0,currentFile=0;
    std::uint32_t fileBytes=0,firmware=0;
    unsigned layoutId=0,rawChanged=0;
    bool savedCalibration=false,switchFile=false;
    Profile profile{};
    bool Open(std::uint32_t token) {
        if(session_)return false;
        SetStage(Stage::Session);token_=token;
        auto request=Request(0,27);request[4]=1;Put32(request.data()+5,token_);
        Frame reply{};
        if(!Exchange(request,reply))return false;
        if(U32(reply.data()+5)!=token_ || !reply[9] || reply[9]==255)return Rejected(&reject,Reject::SessionEcho);
        session_=reply[9];return true;
    }
    bool Close() {
        if(!session_)return true;
        const auto id=session_;session_=0;
        auto request=Request(id,27);request[4]=2;Put32(request.data()+5,token_);request[9]=id;
        Frame reply{};
        if(!Exchange(request,reply,ReplyKind::Normal,true))return false;
        if(U32(reply.data()+5)!=token_ || reply[9]!=id)return Rejected(&reject,Reject::SessionEcho);
        return true;
    }
    bool Initialize(std::uint32_t token) {
        profile={};haveRaw_=false;rawChanged=0;reject=Reject::None;observedVid=observedPid=bragi=currentFile=0;fileBytes=firmware=0;layoutId=0;savedCalibration=switchFile=false;
        if(!Open(token))return false;
        Frame reply{};SetStage(Stage::Identity);
        if(!Property(0x11,reply))return false;
        observedVid=U16(reply.data()+5);if(observedVid!=kVid)return Rejected(&reject,Reject::Vendor);
        if(!Property(0x12,reply))return false;
        observedPid=U16(reply.data()+5);if(observedPid!=kPid)return Rejected(&reject,Reject::Product);
        SetStage(Stage::Firmware);
        if(!Property(0x13,reply))return false;
        firmware=U32(reply.data()+5);
        if(firmware!=0x00ab0802)return Rejected(&reject,Reject::Firmware);
        if(!Property(0x105,reply))return false;
        bragi=U16(reply.data()+5);if(bragi!=0x0501)return Rejected(&reject,Reject::Bragi);
        SetStage(Stage::Layout);
        if(!Property(0x41,reply))return false;
        layoutId=reply[5];const auto* layout=FindLayout(layoutId);if(!layout)return Rejected(&reject,Reject::Layout);
        SetStage(Stage::Directory);
        std::vector<std::uint8_t> directory;
        if(!File(0x11,0,8004,directory))return false;
        if(!Directory(directory,savedCalibration,switchFile))return Rejected(&reject,Reject::DirectoryFormat);
        SetStage(Stage::Calibration);
        std::array<std::uint16_t,100> rest{},bottom{},verify{};
        const int restSub=savedCalibration?6:3,bottomSub=savedCalibration?5:2;
        currentFile=0;
        if(!Property(0xc6,reply,restSub) || !CalibrationWords(reply,layout->count,rest,&reject) ||
           !Property(0xc6,reply,bottomSub) || !CalibrationWords(reply,layout->count,bottom,&reject) ||
           !Property(0xc6,reply,restSub) || !CalibrationWords(reply,layout->count,verify,&reject))return false;
        if(rest!=verify)return Rejected(&reject,Reject::CalibrationChanged);
        Profile calibrationProof{};
        if(!BuildProfile(*layout,savedCalibration,rest,bottom,nullptr,calibrationProof,&reject))return false;
        SetStage(Stage::SwitchSettings);
        std::vector<std::uint8_t> switches;
        if(switchFile && !File(0x4b,1,3+139*4,switches))return false;
        if(!BuildProfile(*layout,savedCalibration,rest,bottom,switchFile?&switches:nullptr,profile,&reject))return false;
        // A fresh directory verifies that files were not added/removed during
        // initialization. Existing 4B stays open until session shutdown.
        SetStage(Stage::Directory);
        std::vector<std::uint8_t> finalDirectory;
        if(!File(0x11,0,8004,finalDirectory))return false;
        if(finalDirectory!=directory)return Rejected(&reject,Reject::DirectoryChanged);
        currentFile=0;fileBytes=0;SetStage(Stage::Cleanup);
        if(!Close())return false;
        if(!Open(token^0x79b9f531u))return false;
        SetStage(Stage::Sampling);return true;
    }
    bool Read(std::array<std::uint16_t,100>& values) {
        if(!session_ || !profile.layout)return false;
        SetStage(Stage::Sampling);reject=Reject::None;currentFile=0;Frame reply{};
        if(!Property(0xc7,reply) || !Snapshot(reply,profile,values,&reject))return false;
        rawChanged=0;
        for(unsigned i=0;i<profile.layout->count;++i)if(profile.layout->hid[i]) {
            const auto raw=U16(reply.data()+5+2*i);
            rawChanged+=haveRaw_ && previousRaw_[i]!=raw;
            previousRaw_[i]=raw;
        }
        haveRaw_=true;return true;
    }
};
}

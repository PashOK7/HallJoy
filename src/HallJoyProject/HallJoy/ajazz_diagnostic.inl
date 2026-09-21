// Included inside the NA87 transport namespace; diagnostic builds only.
// No calibration, settings writes, digital substitution or synthetic analog values.
// Raw rows are live sensor samples, not calibrated travel or an atomic snapshot.
struct AjazzRawDiagnostic {
    struct Bin { unsigned count=0, minimum=65535, maximum=0; unsigned long long sum=0; };
    std::array<Bin,132> positions{};
    std::array<std::array<Bin,41>,132> depthBins{};
    std::array<unsigned,132> last{};
    std::array<unsigned long long,6> rowCounts{}, lastLogMs{};
    unsigned mask=0, invalid=0;
    unsigned long long packets=0;
    static void Add(Bin& b,unsigned v) {
        ++b.count;b.minimum=std::min(b.minimum,v);b.maximum=std::max(b.maximum,v);b.sum+=v;
    }
    bool Accept(const irok_nd75::Report& r,const std::array<std::uint8_t,132>& depths,
        const std::array<unsigned,132>& depthCounts) {
        if(r[0]!=1 || r[1]!=0x23) return false;
        if(r[2]!=0 || r[3]!=0 || r[4]<1 || r[4]>6 || r[5]!=44) {++invalid;return false;}
        const unsigned row=r[4]-1;
        ++packets;++rowCounts[row];mask|=1u<<row;
        for(unsigned col=0;col<22;++col) {
            const unsigned p=row*22+col,v=(unsigned(r[6+2*col])<<8)|r[7+2*col];
            last[p]=v;Add(positions[p],v);
            // A cached event depth can be stale. These bins are observations,
            // never assumed simultaneous calibration pairs.
            if(depthCounts[p] && depths[p]<=40) Add(depthBins[p][depths[p]],v);
        }
        const auto now=GetTickCount64();
        if(rowCounts[row]==1 || now-lastLogMs[row]>=100) {
            lastLogMs[row]=now;
            std::wstring values;
            for(unsigned col=0;col<22;++col) {
                if(col) values+=L",";
                values+=std::to_wstring(last[row*22+col]);
            }
            StabilityTrace_Write(L"INFO",L"ajazz",L"adc.row",
                L"row=%u row_sequence=%llu samples=%ls",row,rowCounts[row],values.c_str());
        }
        return true;
    }
    unsigned Changed() const {
        unsigned n=0;for(const auto& b:positions) if(b.count && b.maximum>b.minimum) ++n;return n;
    }
    void Summary() const {
        StabilityTrace_Write(L"INFO",L"ajazz",L"adc.summary",
            L"packets=%llu row_mask=%u changed_positions=%u invalid=%u",packets,mask,Changed(),invalid);
        for(unsigned p=0;p<132;++p) if(positions[p].count) {
            const auto& b=positions[p];
            StabilityTrace_Write(L"INFO",L"ajazz",L"adc.position",
                L"row=%u col=%u count=%u minimum=%u maximum=%u last=%u sum=%llu",
                p/22,p%22,b.count,b.minimum,b.maximum,last[p],b.sum);
            for(unsigned d=0;d<=40;++d) if(depthBins[p][d].count) {
                const auto& q=depthBins[p][d];
                StabilityTrace_Write(L"INFO",L"ajazz",L"adc.cached_depth_bin",
                    L"row=%u col=%u cached_depth=%u count=%u minimum=%u maximum=%u sum=%llu simultaneous=0",
                    p/22,p%22,d,q.count,q.minimum,q.maximum,q.sum);
            }
        }
    }
};
inline irok_nd75::Report AjazzRawRequest(bool enabled) {
    irok_nd75::Report r{};r[0]=1;r[1]=0x23;r[6]=enabled?1:0;return r;
}
bool RunAjazzDiagnostic(const Candidate& candidate)
{
    g_diagnosticStatus.store(0);
    Session session(candidate, false, false);
    if (!session.Open()) {
        StabilityTrace_Write(L"WARN",L"ajazz",L"open.failed",L"win32=%lu",GetLastError());
        return false;
    }
    if (!session.Send(irok_nd75::BuildIdentityRequest())) {
        StabilityTrace_Write(L"WARN",L"ajazz",L"identity.write_failed",L"win32=%lu",GetLastError());
        return false;
    }
    irok_nd75::DeviceInfo identity{};
    irok_nd75::Report report{};
    bool identified=false;
    const auto deadline=GetTickCount64()+kProofTimeoutMs;
    while (!g_stop.load() && GetTickCount64()<deadline) {
        if (session.Read(&report,100) && irok_nd75::DecodeDeviceInfo(report.data(),report.size(),&identity)) {
            identified=true; break;
        }
    }
    if (!identified) {
        StabilityTrace_Write(L"WARN",L"ajazz",L"identity.timeout",L"vid=0416 pid=7372");
        return false;
    }
    StabilityTrace_Write(L"INFO",L"ajazz",L"identity",L"controller=%hs product=%hs firmware=%hs",
        identity.controller.data(),identity.product.data(),identity.firmware.data());
    if (std::strcmp(identity.controller.data(),"M484") ||
        (std::strcmp(identity.product.data(),"SG8994HE") && std::strcmp(identity.product.data(),"SG8994HERGB"))) return false;
    if(std::strcmp(identity.product.data(),"SG8994HERGB") || std::strncmp(identity.firmware.data(),"V1.13.17",8)) return false;
    irok_nd75::CapabilityInfo capability{};
    if (!ReceiveCapability(session,&capability)) {
        StabilityTrace_Write(L"WARN",L"ajazz",L"capability.failed",L"no_subscription=1");return false;
    }
    StabilityTrace_Write(L"INFO",L"ajazz",L"capability",L"nominal=%u",capability.sensitivity);
    if (capability.sensitivity != 40 || !BeginAjazzPlay(session)) return false;
    struct PlayCleanup { ~PlayCleanup() { EndAjazzPlay(); } } playCleanup;
    std::array<std::uint8_t,irok_nd75::kColumns> mask{}; mask.fill(63);
    if (!session.Send(irok_nd75::BuildSubscriptionRequest(mask))) {
        session.Send(irok_nd75::BuildUnsubscribeRequest());
        StabilityTrace_Write(L"WARN",L"ajazz",L"subscribe.failed",L"win32=%lu",GetLastError());return false;
    }
    StabilityTrace_Write(L"INFO",L"ajazz",L"stream.begin",L"no_time_limit=1 depth_log_only=1 calibration=0");
    // Disable on every exit even when the enable write result is uncertain.
    struct RawCleanup {
        Session& session;
        ~RawCleanup() {
            const bool ok=session.Send(AjazzRawRequest(false));
            StabilityTrace_Write(L"INFO",L"ajazz",L"adc.disable",L"write_ok=%u",ok?1u:0u);
        }
    } rawCleanup{session};
    const bool rawEnabled=session.Send(AjazzRawRequest(true));
    StabilityTrace_Write(L"INFO",L"ajazz",L"adc.enable",L"write_ok=%u response_required=1",rawEnabled?1u:0u);
    AjazzRawDiagnostic raw;
    g_diagnosticStatus.store(10);
    std::array<unsigned,132> counts{},releases{};
    std::array<std::uint8_t,132> values{},maximum{};
    unsigned active=0,peak=0,unique=0,errors=0; unsigned long long events=0;
    auto lastSummary=GetTickCount64();
    while (!g_stop.load()) {
        if (!session.Read(&report,100)) {
            const DWORD error=GetLastError();
            if (error!=WAIT_TIMEOUT && error!=ERROR_TIMEOUT) {
                StabilityTrace_Write(L"WARN",L"ajazz",L"read.failed",L"win32=%lu consecutive=%u",error,++errors);
                if (errors>=3) break;
            }
        } else {
            errors=0;
            irok_nd75::LiveEvent e{};
            if (irok_nd75::DecodeLiveEvent(report.data(),report.size(),&e)) {
                ObserveAjazzDepth(e);
                const auto pos=std::size_t(e.row)*22+e.column;
                if (pos<values.size()) {
                    if (!counts[pos]++) ++unique;
                    if (!values[pos] && e.travel) ++active;
                    if (values[pos] && !e.travel) {--active;++releases[pos];}
                    values[pos]=e.travel;maximum[pos]=std::max(maximum[pos],e.travel);
                    peak=std::max(peak,active);++events;
                    // First transitions retain packet order; aggregate indefinitely afterwards.
                    if (events<=4096) StabilityTrace_Write(L"INFO",L"ajazz",L"depth",L"seq=%llu row=%u col=%u raw=%u",events,e.row,e.column,e.travel);
                }
            } else if (raw.Accept(report,values,counts)) {
                PublishAjazzRawRow(report);
            } else if (events==0) {
                // Header only: no arbitrary payloads, serials, paths or keyboard text.
                static unsigned headers=0;
                if (headers++<16) StabilityTrace_Write(L"INFO",L"ajazz",L"reply.header",L"id=%u command=%u kind=%u",report[0],report[1],report[6]);
            }
        }
        if (GetTickCount64()-lastSummary>=2000) {
            lastSummary=GetTickCount64();
            unsigned released=0;for(auto n:releases) if(n)++released;
            if(AjazzReadyKeys()) g_diagnosticStatus.store(13);
            LogAjazzLearning();
            StabilityTrace_Write(L"INFO",L"ajazz",L"dynamic_limits.progress",L"ready_keys=%u preliminary=1 manual_calibration=0",AjazzReadyKeys());
            StabilityTrace_Write(L"INFO",L"ajazz",L"adc.progress",L"packets=%llu row_mask=%u changed_positions=%u invalid=%u",raw.packets,raw.mask,raw.Changed(),raw.invalid);
            StabilityTrace_Write(L"INFO",L"ajazz",L"stream.progress",L"events=%llu positions=%u released_positions=%u cached_active=%u cached_peak=%u simultaneous_not_proven=1",events,unique,released,active,peak);
            for(std::size_t i=0;i<counts.size();++i) if(counts[i])
                StabilityTrace_Write(L"INFO",L"ajazz",L"position.progress",L"row=%u col=%u events=%u releases=%u max=%u last=%u",unsigned(i/22),unsigned(i%22),counts[i],releases[i],maximum[i],values[i]);
        }
    }
    LogAjazzLearning();
    raw.Summary();
    const bool stopped=session.Send(irok_nd75::BuildUnsubscribeRequest());
    for(std::size_t i=0;i<counts.size();++i) if(counts[i])
        StabilityTrace_Write(L"INFO",L"ajazz",L"position.summary",L"row=%u col=%u events=%u releases=%u max=%u last=%u",unsigned(i/22),unsigned(i%22),counts[i],releases[i],maximum[i],values[i]);
    StabilityTrace_Write(L"INFO",L"ajazz",L"stream.end",L"events=%llu positions=%u unsubscribe_ok=%u",events,unique,stopped?1u:0u);
    g_diagnosticStatus.store(12);
    return true;
}

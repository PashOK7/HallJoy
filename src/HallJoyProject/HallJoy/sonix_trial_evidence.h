#pragma once
#include <cstdio>
// The ordinary evidence store retains383 text bytes. Keep separate records;
// allocating a larger producer buffer cannot enlarge that store.
template<class Stats,class Sink> bool EmitSonixTrialEvidence(const Stats& s,bool connected,
        unsigned seen,unsigned positive,unsigned released,unsigned held,Sink sink) {
    const bool boom=s.vid==0x0c45 && s.pid==0x80b5;
    const char* family=boom?"boom68":"aula_f75";
    bool okay=true;
    const auto emit=[&](const char* format,auto... args){
        char line[384]{};const int n=std::snprintf(line,sizeof(line),format,family,args...);
        if(n<0 || n>=int(sizeof(line))){okay=false;sink("evidence.sonix.record_overflow fields_not_retained=1");}
        else sink(line);
    };
    emit("evidence.%s.current revision=2 vid=%04X pid=%04X page=%04X input=%u output=%u identity=%u info_vid=%04X info_pid=%04X firmware_bcd=%04X connected=%u no_analog_yet=%u",
        s.vid,s.pid,s.page,s.input,s.output,unsigned(s.identity),s.infoVid,s.infoPid,s.firmware,unsigned(connected),s.samples?0:1);
    emit("evidence.%s.metadata revision=2 precision_metadata=%u frame_metadata=%u assignments_available=%u mapped=%u route=%u fingerprint0=%016llX fingerprint1=%016llX",
        s.precision,s.frame,s.metadata,s.mapped,s.route,s.fingerprints[0],s.fingerprints[1]);
    emit("evidence.%s.control revision=2 queries=%llu replies=%llu status_queries=%llu monitor_on=%llu monitor_off=%llu fixed_delay_us=0",
        s.queries,s.replies,s.statusQueries,s.monitorOn,s.monitorOff);
    emit("evidence.%s.depth revision=2 batches=%llu samples=%llu observed_channels=%u positive_channels=%u released_channels=%u release_events=%llu changes=%llu held_last=%u max_activity=%u",
        s.batches,s.samples,seen,positive,released,s.releases,s.changes,held,s.maximumActive);
    emit("evidence.%s.range revision=2 raw_peak=%u adc_min=%u adc_max=%u adc_available=%u stroke_min_tenths=%u stroke_max_tenths=%u first_sample_zero=%u ignored_slots=%llu",
        s.rawPeak,s.adcMin==65535?0:s.adcMin,s.adcMax,s.route==3?0:1,s.strokeMin==65535?0:s.strokeMin,s.strokeMax,unsigned(s.firstZero),s.ignored);
    emit("evidence.%s.timing revision=2 mean_batch_us=%u max_batch_us=%u release_observation_pending=%u matrix_sampling=per_reply_not_atomic",
        s.meanUs,s.maxUs,(!positive || released<positive)?1:0);
    emit("evidence.%s.units revision=2 units_basis=depth_cache_0.01mm status_scale_hardware_unverified=%u status_range_um=%u stream_range=per_frame ram_range=per_axis no_host_expiry=1",
        s.route==1?1:0,boom?3300u:3400u);
    return okay;
}

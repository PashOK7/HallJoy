#include "../HallJoy/midi_observation_engine.h"
#include "../HallJoy/midi_velocity_trace.h"
#include <cassert>
#include <cstring>
#include <iostream>
#include <memory>
#include <vector>
using namespace halljoy;
using namespace halljoy::midi;
using Sample = observation::Event;
struct Result {
    std::vector<std::uint32_t> messages;
    TouchReport touch;
    std::vector<VelocityTraceRecord> attacks;
};
Sample Event(unsigned depth, std::uint64_t time, unsigned hid = 4) {
    Sample event{};
    event.source = 1; event.connection = 1; event.physical = static_cast<std::uint16_t>(hid);
    event.hid = static_cast<std::uint16_t>(hid); event.milli = static_cast<std::uint16_t>(depth);
    event.timeUs = time;
    return event;
}
Result Replay(const std::vector<Sample>& samples, unsigned batch, Profile config = {}, bool capture = false) {
    config.enabled = true; config.keys[4] = 61; config.keys[5] = 65; config.depthPressure = false;
    auto engine = std::make_unique<ObservationEngine>();
    if(capture)engine->EnableDiagnostics();
    Result result;
    auto emit = [&](auto message) { result.messages.push_back(message); };
    const AttackSink sink{capture?&result:nullptr,capture?+[](void* raw,const AttackState& key,const MotionTrace& motion,const Profile& profile,const AttackContext& input) {
        VelocityTraceRecord record;
        record.context=input;record.motion=motion;record.touch=key.last;
        record.velocity=key.velocity;record.reference=FullVelocitySpeed(profile);
        record.attack=profile.attackMilli;record.release=profile.releaseMilli;record.noise=profile.noiseMilli;
        record.curve=profile.velocityCurve;
        static_cast<Result*>(raw)->attacks.push_back(record);
    }:nullptr};
    auto route = [&] {
        engine->BeginFrame();
        for (unsigned hid : {4u, 5u}) {
            auto baseline = Event(0, 0, hid);
            baseline.kind = observation::Kind::Baseline;
            engine->Route(baseline, config, emit);
        }
    };
    route(); engine->EndFrame(config, emit);
    for (unsigned i = 0; i < samples.size(); ++i) {
        if (i % batch == 0) route();
        engine->Replay(samples[i], config, emit, sink);
        if (i % batch == batch - 1 || i + 1 == samples.size()) engine->EndFrame(config, emit);
    }
    result.touch = engine->Touch();
    return result;
}
std::vector<Sample> Stroke(unsigned speed, std::uint64_t idle = 0, unsigned step = 25) {
    std::vector<Sample> samples{Event(0, 1)};
    for (unsigned depth = step; depth <= 200; depth += step)
        samples.push_back(Event(depth, idle + 1 + std::uint64_t(depth) * 1000000 / speed));
    samples.push_back(Event(0, idle + 1000000));
    return samples;
}
unsigned Velocity(const Result& result) {
    assert(result.messages.size() == 2);
    assert((result.messages[0] & 0xf0) == 0x90 && (result.messages[1] & 0xf0) == 0x80);
    return (result.messages[0] >> 16) & 127;
}
int main() {
    Profile config;
    assert(ValidTouch(config) && CurveAxisMaximum(config) == 70000 &&
        CurveSpeed(config, 4) == CurveAxisMaximum(config));
    assert(VelocityForSpeed(70000, config) == 127 && VelocityForSpeed(35000, config) == 64);
    unsigned previous = 0;
    for (unsigned speed : {1000u, 2500u, 5000u, 10000u, 20000u, 35000u, 65000u}) {
        const auto samples = Stroke(speed);
        const auto immediate = Replay(samples, 1);
        const auto delayed = Replay(samples, 1000);
        assert(immediate.messages == delayed.messages);
        assert(immediate.touch.speedMilli == delayed.touch.speedMilli);
        assert(immediate.touch.quality == delayed.touch.quality);
        const unsigned velocity = Velocity(immediate);
        assert(velocity >= previous); previous = velocity;
        assert(std::abs(int(immediate.touch.speedMilli) - int(speed)) <= int(speed / 1000 + 1));
        assert(immediate.touch.quality == TouchQuality::CapturedMotion);
        assert(immediate.messages == Replay(Stroke(speed, 90000000), 3).messages);
    }
    assert(Velocity(Replay(Stroke(10000, 0, 25), 1)) == Velocity(Replay(Stroke(10000, 0, 100), 2)));
    const auto base = Stroke(10000);
    std::vector<Sample> chord;
    for (const auto& sample : base) {
        chord.push_back(sample);
        auto second = sample; second.physical = second.hid = 5; chord.push_back(second);
    }
    const auto chordResult = Replay(chord, 1);
    assert(chordResult.messages.size() == 4 && chordResult.messages == Replay(chord, 1000).messages);
    assert(((chordResult.messages[0] >> 16) & 127) == ((chordResult.messages[1] >> 16) & 127));
    // A meaningful reversal discards the first stroke's speed.
    const auto reversed = Replay({Event(0, 1), Event(100, 1001), Event(80, 2001),
        Event(100, 3001), Event(200, 13001), Event(0, 14001)}, 2);
    assert(reversed.touch.speedMilli == 10000 && reversed.touch.quality == TouchQuality::CapturedMotion);
    // Partial release is a separate musical threshold, with no rest or timeout.
    config.releaseMilli = 150;
    const auto partial = Replay({Event(0, 1), Event(100, 1001), Event(200, 2001),
        Event(400, 3001), Event(151, 4001), Event(150, 5001),
        Event(175, 6001), Event(200, 7001), Event(150, 8001)}, 1000, config);
    assert(partial.messages.size() == 4);
    assert((partial.messages[0] & 0xf0) == 0x90 && (partial.messages[1] & 0xf0) == 0x80);
    // Small movements accumulate without inventing time or a faster fake stroke.
    config = {}; config.noiseMilli = 10;
    std::vector<Sample> noisy{Event(0, 1)};
    for (unsigned i = 1; i <= 200; ++i) noisy.push_back(Event(i, 1 + i * 100));
    noisy.push_back(Event(0, 30000));
    assert(Replay(noisy, 1, config).touch.speedMilli == 10000);
    // Snapshot intervals and missing/equal times are explicitly qualified.
    auto snapshots = base;
    for (auto& event : snapshots) event.timing = observation::Timing::HostSnapshot;
    assert(Replay(snapshots, 2).touch.quality == TouchQuality::SnapshotMotion);
    auto single = Replay({Event(0, 1), Event(500, 10001), Event(0, 20001)}, 3);
    assert(single.touch.quality == TouchQuality::SparseInterval && single.touch.sparse == 1);
    auto unknown = Replay({Event(500, 10001), Event(0, 20001)}, 2);
    assert(unknown.touch.quality == TouchQuality::UnresolvedTiming && unknown.touch.intervalUs == 0);
    auto equal = Replay({Event(0, 1), Event(100, 1001), Event(200, 1001), Event(0, 2001)}, 1);
    assert(equal.touch.quality == TouchQuality::UnresolvedTiming);
    auto gap = Replay({Event(0, 1), Event(100, 1001), Event(200, 101001), Event(0, 102001)}, 1);
    assert(gap.touch.quality == TouchQuality::SparseInterval);
    // Clock reversal releases ownership and requires a fresh released baseline.
    auto backwards = Replay({Event(0, 100), Event(100, 200), Event(200, 150),
        Event(500, 300), Event(0, 400), Event(100, 500), Event(200, 600), Event(0, 700)}, 1);
    assert(backwards.messages.size() == 2);
    // Versioned curves remain monotone for every legal output encoding.
    config = {}; config.touchVersion=1;config.velocityCurve = {0, 100, 300, 600, 1000};
    for (unsigned i = 0; i <= 100000; ++i) {
        const unsigned value = VelocityForSpeed(i, config);
        assert(value >= 1 && value <= 127);
        if (i) assert(value >= VelocityForSpeed(i - 1, config));
    }
    config.fullSpeedMilli = 10000;
    assert(VelocityForSpeed(10000, config) == 127 && VelocityForSpeed(0, config) == 1);
    config.velocityCurve[2] = 0; assert(!ValidTouch(config));
    config = {}; config.releaseMilli = config.attackMilli; assert(!ValidTouch(config));
    config = {}; config.touchVersion = 3; assert(!ValidTouch(config));
    // Diagnostics must preserve every attack in one batch and leave messages unchanged.
    const std::vector<Sample> repeated{Event(0,1),Event(100,1001),Event(200,2001),
        Event(0,3001),Event(500,1003001),Event(0,1004001),
        Event(50,1005001),Event(150,1006001),Event(300,1007001),Event(0,1008001)};
    const auto traced=Replay(repeated,100,{},true);
    assert(traced.messages==Replay(repeated,1).messages && traced.attacks.size()==3);
    const auto& measured=traced.attacks[0];
    assert(measured.motion.measured && measured.motion.startUs==1001 &&
        measured.touch.intervalUs==1000 && measured.touch.speedMilli==100000);
    const auto& sparse=traced.attacks[1];
    assert(sparse.touch.quality==TouchQuality::SparseInterval && sparse.motion.gap &&
        sparse.motion.priorUs==3001 && sparse.motion.startDepth==0 &&
        sparse.touch.intervalUs==1000000 && sparse.velocity==VelocityForSpeed(500,Profile{}));
    assert(sparse.motion.count==2 && sparse.motion.Point(0).depth==0 &&
        sparse.motion.Point(1).depth==500 && sparse.context.hid==4 &&
        sparse.context.source==1 && sparse.context.code==61);
    std::vector<Sample> slowStroke{Event(0,1)};
    for(unsigned depth=1;depth<=200;++depth)slowStroke.push_back(Event(depth,1+depth*1000));
    const auto longTrace=Replay(slowStroke,200,{},true);
    const auto& points=longTrace.attacks[0].motion;
    assert(points.count==201 && points.Point(0).depth==0 && points.Point(3).depth==3 &&
        points.Point(4).depth==189 && points.Point(15).depth==200);
    // Bottom clipping must not discard the wider, faster acquired onset.
    Profile firm;firm.touchVersion=1;firm.touch=2;firm.attackMilli=800;
    firm.velocityCurve={0,500,707,866,1000};
    auto confirmedZero=Event(0,1000);confirmedZero.previousValid=true;
    for(unsigned first:{670u,754u,761u,749u,792u,782u}) {
        const auto clipped=Replay({confirmedZero,Event(first,9000),Event(1000,17000),Event(0,25000)},4,firm,true);
        assert(Velocity(clipped)==127 && clipped.attacks.size()==1 &&
            clipped.attacks[0].motion.onsetSelected && clipped.touch.travelMilli==first &&
            clipped.touch.intervalUs==8000 && clipped.attacks[0].motion.endDepth==int(first));
        assert(clipped.messages==Replay({confirmedZero,Event(first,9000),Event(1000,17000),Event(0,25000)},1,firm,true).messages);
    }
    const auto noProof=Replay({Event(0,1000),Event(754,9000),Event(1000,17000),Event(0,25000)},4,firm,true);
    assert(!noProof.attacks[0].motion.onsetSelected && noProof.touch.travelMilli==246);
    // A stale onset and a tiny early jitter cannot inflate subsequent slow motion.
    const auto stale=Replay({confirmedZero,Event(754,1001000),Event(1000,1009000),Event(0,1017000)},4,firm,true);
    assert(!stale.attacks[0].motion.onsetSelected && stale.touch.travelMilli==246);
    const auto jitter=Replay({confirmedZero,Event(2,1001),Event(800,51001),Event(0,60000)},4,firm,true);
    assert(!jitter.attacks[0].motion.onsetSelected && jitter.touch.speedMilli==15960 && Velocity(jitter)<127);
    const auto slow=Replay({confirmedZero,Event(100,9000),Event(300,29000),Event(800,79000),Event(0,90000)},5,firm,true);
    assert(!slow.attacks[0].motion.onsetSelected && Velocity(slow)<90);
    Profile absolute;
    absolute.velocitySpeed={4000,12000,45000,98000,180000};
    absolute.velocityCurve={80,240,440,690,905};
    assert(ValidTouch(absolute) && VelocityForSpeed(3999,absolute)==0 &&
        VelocityForSpeed(4000,absolute)==10 && VelocityForSpeed(45000,absolute)==56 &&
        VelocityForSpeed(180000,absolute)==115 && VelocityForSpeed(1000000000,absolute)==115);
    assert(Replay(Stroke(1000),1,absolute).messages.empty()); // No MIDI off/on-zero at the dead zone.
    assert(Velocity(Replay(Stroke(10000),1,absolute))>0);
    auto zoom=absolute;zoom.velocityAxisMaximum=1000000;
    assert(SamePerformance(zoom,absolute) && VelocityForSpeed(34000,zoom)==VelocityForSpeed(34000,absolute));
    auto edited=absolute;edited.velocitySpeed[1]=17000;
    assert(!SamePerformance(edited,absolute) && VelocityForSpeed(66000,edited)==VelocityForSpeed(66000,absolute));
    auto legacy=firm;legacy.fullSpeedMilli=42317;
    const auto old=legacy;MakeEditableCurve(legacy);
    assert(ValidTouch(legacy) && legacy.touchVersion==2 && legacy.velocitySpeed.back()==42317);
    for(unsigned speed=0;speed<90000;speed+=137)
        assert(std::abs(int(VelocityForSpeed(speed,old))-int(VelocityForSpeed(speed,legacy)))<=1);
    auto store=std::make_unique<VelocityTrace>();
    for(unsigned i=0;i<VelocityTrace::kCapacity+7;++i){
        auto record=measured;record.epoch=i;store->Record(record);
    }
    auto batch=std::make_unique<VelocityTrace::Batch>();
    assert(store->Pending());store->Drain(*batch);
    assert(batch->overwritten==7 && batch->count==VelocityTrace::kCapacity &&
        batch->records[0].sequence==8 && !store->Pending());
    for(unsigned i=0;i<batch->count;++i)WriteVelocityTrace(batch->records[i],[](const char* line){
        assert(std::strlen(line)<352 && std::strncmp(line,"evidence.midi.",14)==0);
    });
    store->Drain(*batch);assert(!batch->count && !batch->overwritten);
    std::cout << "MIDI_TOUCH_TEST=PASS motion_units idle schedule cadence chords reversal partial_release noise timing curve version" << std::endl;
}

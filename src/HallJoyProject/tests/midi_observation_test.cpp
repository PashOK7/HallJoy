#include "../HallJoy/physical_analog_state.h"
#include "../HallJoy/midi_observation_engine.h"
#include "../HallJoy/analog_observation_snapshot.h"
#include <cassert>
#include <thread>
#include <vector>
#include <iostream>
using namespace halljoy;
int main() {
    auto journal=std::make_shared<observation::Journal>();
    observation::current.store(journal);observation::enabled=true;
    physical_analog::Publication board,other;
    assert(board.Bind(1,4) && board.Bind(2,4) && other.Bind(1,4));
    board.Publish(1,0,1);board.Publish(2,0,1);other.Publish(1,0,1);
    midi::Profile config;config.enabled=true;config.depthPressure=false;config.keys[4]=61;
    // Keep409KB engines off the default1MB Windows test stack.
    auto engineStorage=std::make_unique<midi::ObservationEngine>();auto& engine=*engineStorage;
    std::vector<std::uint32_t> messages;
    auto emit=[&](std::uint32_t message){messages.push_back(message);};
    auto frame=[&] {
        engine.BeginFrame();
        struct Context {midi::ObservationEngine* engine;midi::Profile* config;decltype(emit)* emitFn;} context{&engine,&config,&emit};
        observation::Sink route{&context,+[](void* p,const observation::Event& e){auto& c=*static_cast<Context*>(p);c.engine->Route(e,*c.config,*c.emitFn);}};
        board.Observations(route);other.Observations(route);
        std::array<observation::Event,256> events{};
        for(auto n=journal->Drain(events.data(),events.size());n;n=journal->Drain(events.data(),events.size()))
            for(std::size_t i=0;i<n;++i)engine.Replay(events[i],config,emit);
        engine.EndFrame(config,emit);
    };
    frame();messages.clear();
    // An entire chord and release arrives while the MIDI consumer is not running.
    for(unsigned i=0;i<100;++i) {board.Publish(1,500,2);board.Publish(2,700,2);board.Publish(1,0,3);board.Publish(2,0,3);}
    frame();assert(messages.size()==200 && !engine.Notes()[60].velocity);
    for(unsigned i=0;i<messages.size();++i) assert((messages[i]&0xf0)==(i%2?0x80:0x90));
    board.Publish(1,500,4);other.Publish(1,700,4);frame();assert(engine.Notes()[60].velocity);
    messages.clear();board.Publish(1,0,5);frame();assert(messages.empty() && engine.Notes()[60].velocity);
    // Quiet is not release; loss of one source does not release the other.
    board.Clear();frame();assert(engine.Notes()[60].velocity);
    other.Clear();frame();assert(!engine.Notes()[60].velocity);
    // Reconnect-held is suppressed, followed by a real release/new attack.
    other.Bind(1,4);other.Publish(1,700,6);frame();assert(!engine.Notes()[60].velocity);
    other.Publish(1,0,7);frame();other.Publish(1,600,8);frame();assert(engine.Notes()[60].velocity);
    for(unsigned i=0;i<100;++i){other.Clear();other.Bind(1,4);other.Publish(1,0,10+i);frame();}
    assert(engine.CapacityLoss()==0);
    // The same received timestamps give the same messages at different drain schedules.
    auto replay=[&](bool batched) {
        auto storage=std::make_unique<midi::ObservationEngine>();auto& e=*storage;std::vector<std::uint32_t> out;
        auto send=[&](auto m){out.push_back(m);};
        observation::Event base{};base.source=800;base.connection=1;base.physical=1;base.hid=4;base.kind=observation::Kind::Baseline;
        e.BeginFrame();e.Route(base,config,send);e.EndFrame(config,send);
        for(unsigned i=0;i<6;++i) {
            if(!batched){e.BeginFrame();e.Route(base,config,send);}
            auto sample=base;sample.kind=observation::Kind::Sample;sample.timeUs=1000+i*1000;
            sample.milli=static_cast<std::uint16_t>(i<3?i*100:(5-i)*100);sample.previousValid=true;
            e.Replay(sample,config,send);
            if(!batched)e.EndFrame(config,send);
        }
        if(batched)e.EndFrame(config,send);
        return out;
    };
    assert(replay(false)==replay(true));
    {
        auto storage=std::make_unique<midi::ObservationEngine>();auto& deferred=*storage;std::vector<std::uint32_t> out;
        auto send=[&](auto message){out.push_back(message);};
        deferred.BeginFrame();observation::Event e{};e.kind=observation::Kind::RetainSnapshot;
        deferred.Route(e,config,send);e.kind=observation::Kind::Sample;e.timing=observation::Timing::HostSnapshot;
        e.source=999;e.connection=1;e.physical=4;e.hid=4;e.previousValid=true;e.timeUs=1000;e.milli=500;
        deferred.Replay(e,config,send);e.timeUs=2000;e.milli=0;e.previous=500;
        deferred.Replay(e,config,send);deferred.EndFrame(config,send);
        assert(out.size()==2 && (out[0]&0xf0)==0x90 && (out[1]&0xf0)==0x80);
    }
    // A fresh full-frame poll can prove a zero shortly before a one-step attack.
    // Preserve that genuine sample without publishing all stationary keyboard frames.
    {
        auto capture=std::make_shared<observation::Journal>();observation::current.store(capture);
        physical_analog::Publication polled;
        assert(polled.Bind(1,4));
        polled.Publish(1,0,1,false,1000,0);
        for(unsigned i=1;i<=22;++i)polled.Publish(1,0,1+i*8,false,1000+i*8000,1000+(i-1)*8000);
        std::array<observation::Event,16> acquired{};
        assert(capture->Drain(acquired.data(),acquired.size())==1); // Initial zero only.
        polled.Publish(1,959,185,false,185000,177000);
        polled.Publish(1,0,193,false,193000,185000);
        const auto count=capture->Drain(acquired.data(),acquired.size());
        assert(count==4 && acquired[0].milli==0 && acquired[0].timeUs==177000 &&
            acquired[1].milli==959 && acquired[1].timeUs==185000);
        midi::Profile touch;touch.enabled=true;touch.depthPressure=false;
        touch.attackMilli=800;touch.touch=2;touch.keys[4]=61;
        auto polledEngine=std::make_unique<midi::ObservationEngine>();std::vector<std::uint32_t> out;
        auto send=[&](auto message){out.push_back(message);};
        polledEngine->BeginFrame();
        polled.Observations({polledEngine.get(),+[](void* raw,const observation::Event& base){
            midi::Profile cfg;cfg.enabled=true;cfg.keys[4]=61;
            static_cast<midi::ObservationEngine*>(raw)->Route(base,cfg,[](auto){});
        }});
        for(unsigned i=0;i<count;++i)polledEngine->Replay(acquired[i],touch,send);
        polledEngine->EndFrame(touch,send);
        assert(out.size()==2 && ((out[0]>>16)&127)==127 &&
            polledEngine->Touch().intervalUs==8000 && polledEngine->Touch().speedMilli==119875);
        assert(polledEngine->Touch().quality==midi::TouchQuality::SparseInterval);
        // Missing or invalid preceding timestamps never create a fabricated sample.
        observation::Source delta;
        delta.Publish(1,4,900,0,true,200000);
        assert(capture->Drain(acquired.data(),acquired.size())==1);
        delta.Publish(1,4,900,0,true,200000,observation::Timing::HostPublication,true,200001);
        assert(capture->Drain(acquired.data(),acquired.size())==1);
    }
    // Multiple acquisition owners preserve one global order without overwriting unread data.
    journal=std::make_shared<observation::Journal>();observation::current.store(journal);
    std::array<observation::Source,4> sources;
    std::array<std::thread,4> threads;
    for(unsigned p=0;p<4;++p)threads[p]=std::thread([&,p]{for(unsigned i=0;i<2000;++i)sources[p].Publish(1,4,i%2?0:100,i%2?100:0);});
    for(auto& thread:threads)thread.join();
    assert(journal->lost==0);std::uint64_t last=0,count=0;
    std::array<observation::Event,512> batch{};
    for(auto n=journal->Drain(batch.data(),batch.size());n;n=journal->Drain(batch.data(),batch.size()))
        for(std::size_t i=0;i<n;++i){assert(batch[i].sequence>last);last=batch[i].sequence;++count;}
    assert(count==8000);
    // Discard fences old reserved/queued entries; malformed and full history are explicit.
    sources[0].Publish(1,4,100,0);journal->Discard();assert(!journal->Drain(batch.data(),batch.size()));
    sources[0].Publish(1,4,1001,0);assert(journal->malformed==1);
    for(unsigned i=0;i<40000;++i)sources[0].Publish(1,4,i%2?0:100,i%2?100:0);
    assert(journal->lost>0);
    observation::SnapshotTracker snapshots;
    auto snapshot=[&](std::uint64_t generation,int depth){snapshots.Frame(1,generation,[&](auto put){observation::SnapshotValues v{};v[4]=static_cast<std::uint16_t>(depth);put(99,v,generation*1000);});};
    snapshot(1,0);snapshot(2,500);snapshot(4,0);assert(snapshots.gaps==1);
    {
        observation::SnapshotTracker duplicate;
        duplicate.Frame(1,1,[&](auto put){observation::SnapshotValues v{};put(77,v,1000,1);put(77,v,1000,2);});
        struct Seen {std::uint64_t a=0,b=0;} seen;
        duplicate.Visit({&seen,+[](void* p,const observation::Event& e){if(e.hid!=4)return;auto& s=*static_cast<Seen*>(p);if(!s.a)s.a=e.source;else if(e.source!=s.a)s.b=e.source;}});
        assert(seen.a && seen.b && seen.a!=seen.b);
    }
    observation::enabled=false;observation::current.store(nullptr);
    sources[0].Publish(1,4,500,0);assert(!observation::current.load());
    std::cout<<"MIDI_OBSERVATION_TEST=PASS complete_edges physical_alias two_sources quiet reconnect timestamps mpsc overflow snapshot_gaps disabled\n";
}

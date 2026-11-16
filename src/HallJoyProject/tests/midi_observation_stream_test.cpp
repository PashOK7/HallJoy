#include "../HallJoy/analog_observation_stream.h"
#include "../HallJoy/midi_observation_engine.h"
#include <cassert>
#include <iostream>
#include <vector>
using namespace halljoy;
int main() {
    using namespace observation;
    auto journal=std::make_shared<Journal>();current.store(journal);enabled=true;
    StreamEncoder encoder;SnapshotTracker tracker;
    midi::ObservationEngine engine;midi::Profile config;config.enabled=true;config.depthPressure=false;config.keys[4]=61;
    SnapshotValues a{},b{};SnapshotValidity valid;valid.set(4);
    std::uint64_t now=10000;unsigned loss=0;bool second=true,first=true;
    std::vector<Event> wire;
    auto publish=[&] {
        now+=1000;
        encoder.Frame([&](auto put){if(first)put(77,a,now,1,&valid);if(second)put(77,b,now,2,&valid);},
            [&](auto e){wire.push_back(e);},[&]{++loss;});
    };
    std::vector<std::uint32_t> messages;auto emit=[&](auto m){messages.push_back(m);};
    auto consume=[&] {
        assert(tracker.ImportStream(1,[&](auto put){for(auto e:wire)put(e);}));wire.clear();
        engine.BeginFrame();struct C {midi::ObservationEngine* engine;midi::Profile* config;decltype(emit)* emitFn;} c{&engine,&config,&emit};
        tracker.Visit({&c,+[](void* p,const Event& e){auto& c=*static_cast<C*>(p);c.engine->Route(e,*c.config,*c.emitFn);}});
        std::array<Event,256> batch{};
        for(auto n=journal->Drain(batch.data(),batch.size());n;n=journal->Drain(batch.data(),batch.size()))for(std::size_t i=0;i<n;++i)engine.Replay(batch[i],config,emit);
        engine.EndFrame(config,emit);
    };
    publish();consume();assert(messages.empty());
    a[4]=b[4]=500;publish();consume();assert(messages.size()==1);
    messages.clear();a[4]=0;publish();consume();assert(messages.empty() && engine.Notes()[60].velocity);
    second=false;publish();consume();assert(messages.size()==1 && !engine.Notes()[60].velocity);
    // Reconnect while held is state, not a new note, even with a duplicate SDK id.
    messages.clear();second=true;publish();consume();assert(messages.empty());
    b[4]=0;publish();b[4]=700;publish();b[4]=0;publish();consume();assert(messages.size()==2);
    // Availability loss releases; restored held depth must wait for release.
    a[4]=500;publish();consume();assert(engine.Notes()[60].velocity);
    valid.reset(4);publish();consume();assert(!engine.Notes()[60].velocity);
    messages.clear();valid.set(4);publish();consume();assert(messages.empty());
    a[4]=0;publish();a[4]=600;publish();a[4]=0;publish();consume();assert(messages.size()==2);
    first=false;second=false;publish();consume();assert(!engine.Notes()[60].velocity && loss==0);
    // Capacity limits and duplicate identities are reported, never aliased.
    StreamEncoder bounded;SnapshotValues zero{};
    bounded.Frame([&](auto put){for(unsigned i=1;i<=17;++i)put(i,zero,now,0,&valid);},[](auto){},[&]{++loss;});assert(loss==1);
    enabled=false;current.store(nullptr);
    std::cout<<"MIDI_OBSERVATION_STREAM_TEST=PASS duplicate_ids aliases disconnect reconnect_held availability delayed_edges bounded_sources\n";
}

#include "../HallJoy/midi_engine.h"
#include "../HallJoy/midi_event_queue.h"
#include <cassert>
#include <iostream>
#include <thread>
#include <vector>
using namespace halljoy::midi;
int main() {
    Engine engine;
    Profile profile; profile.enabled = true; profile.keys[4] = 61; profile.keys[5] = 65;
    std::array<Input, halljoy::keycode::kCount> input{};
    input[4].available = input[5].available = true;
    std::vector<std::uint32_t> events;
    auto tick = [&](std::uint64_t time) {
        engine.Tick(profile, time, [&](std::uint16_t hid) { return input[hid]; },
            [&](std::uint32_t message) { events.push_back(message); });
    };
    auto reset = [&] { engine.Reset([&](std::uint32_t message) { events.push_back(message); }); };
    tick(1000); input[4].milli = 100; tick(2000); assert(events.empty());
    input[4].milli = 200; tick(3000);
    assert((events[0] & 0xF0) == 0x90 && ((events[0] >> 16) & 127) > 0);
    const auto hardVelocity = engine.Notes()[60].velocity;
    input[4].milli = 900; tick(4000);
    assert(engine.Notes()[60].velocity == hardVelocity && engine.Notes()[60].pressure == 114);
    input[5].milli = 600; tick(5000);
    assert(engine.Notes()[60].velocity && engine.Notes()[64].velocity);
    input[4].milli = 150; tick(6000); assert(engine.Notes()[60].velocity); // Hysteresis.
    input[4].milli = 0; tick(7000); assert(!engine.Notes()[60].velocity && engine.Notes()[64].velocity);
    // Disconnect releases immediately, and a reconnect-held key cannot retrigger.
    input[5].available = false; tick(8000); assert(!engine.Notes()[64].velocity);
    input[5].available = true; tick(9000); assert(!engine.Notes()[64].velocity);
    input[5].milli = 0; tick(10000); input[5].milli = 800; tick(11000);
    assert(engine.Notes()[64].velocity);
    reset(); assert((events.back() & 0xF0) == 0x80);
    tick(12000); assert(!engine.Notes()[64].velocity); // Panic held key blocked.
    input[5].milli = 0; tick(13000);
    // A slower attack produces a lower velocity at the same trigger depth.
    input[4].milli = 0; tick(20000); input[4].milli = 100; tick(30000);
    input[4].milli = 200; tick(40000);
    assert(engine.Notes()[60].velocity < hardVelocity && engine.Notes()[60].velocity >= 1);
    // Aliases of the same note do not retrigger or release each other.
    reset(); profile.keys[5] = 61; input[4].milli = input[5].milli = 0; tick(50000);
    input[4].milli = 500; tick(51000); events.clear();
    input[5].milli = 700; tick(52000); input[4].milli = 0; tick(53000);
    for (auto event : events) assert((event & 0xF0) == 0xA0);
    input[5].milli = 0; tick(54000); assert(!engine.Notes()[60].velocity);
    // Every sampled fast edge is preserved, across all notes/channels, including 0 and 127.
    reset(); profile.depthPressure = false; profile.keys.fill(0); profile.keys[4] = 2048;
    input[4].milli = 0; tick(60000); events.clear();
    for (unsigned i = 0; i < 100; ++i) { input[4].milli = 400; tick(61000+i*2000); input[4].milli = 0; tick(62000+i*2000); }
    assert(events.size() == 200);
    for (unsigned i = 0; i < events.size(); ++i) {
        assert((events[i] & 0xF0) == (i % 2 ? 0x80 : 0x90)); assert(Address(events[i]) == 2047);
        if (!(i % 2)) assert(((events[i] >> 16) & 127) >= 1);
    }
    // Full queue refuses overwrite and preserves FIFO, including under concurrent wraparound.
    EventQueue<unsigned, 4> small; unsigned value = 0;
    assert(small.Push(1) && small.Push(2) && small.Push(3) && !small.Push(4));
    assert(small.Pop(value) && value == 1 && small.Push(4));
    for (unsigned expected : {2u,3u,4u}) assert(small.Pop(value) && value == expected);
    EventQueue<unsigned, 64> queue;
    std::thread producer([&] { for (unsigned i = 1; i <= 100000; ++i) while (!queue.Push(i)) std::this_thread::yield(); });
    for (unsigned i = 1; i <= 100000; ++i) { while (!queue.Pop(value)) std::this_thread::yield(); assert(value == i); }
    producer.join();
    std::cout << "MIDI_ENGINE_TEST=PASS velocity chord hysteresis alias disconnect panic repeat pressure ordered_queue\n";
}

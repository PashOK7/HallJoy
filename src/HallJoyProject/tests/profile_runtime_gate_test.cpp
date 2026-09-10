#include "profile_runtime_gate.h"
#include <atomic>
#include <cassert>
#include <thread>
#include <iostream>

int main() {
    using namespace halljoy::profile_runtime;
    { CommitLease writer; assert(writer); ReadLease blocked; assert(!blocked); }
    int settings = 0, bindings = 0;
    std::atomic<bool> stop{false}, mixed{false};
    std::atomic<unsigned> observed{0};
    std::thread reader([&] {
        while (!stop.load()) {
            ReadLease lease;
            if (!lease) continue;
            const int first = settings;
            std::this_thread::yield();
            if (first != bindings) mixed = true;
            ++observed;
        }
    });
    while (!observed.load()) std::this_thread::yield();
    for (int i=1; i<=2000; ++i) {
        CommitLease commit; assert(commit);
        settings = i;
        std::this_thread::yield();
        bindings = i;
    }
    stop = true; reader.join();
    assert(!mixed && observed > 0);
    // Contending readers may race the counter but must never create a writer
    // bit or wait. The bounded admission still lets both make progress.
    std::atomic<unsigned> simultaneous{0};
    std::thread readerA([&] { for (unsigned i = 0; i < 10000u; ++i) { ReadLease lease; if (lease) ++simultaneous; } });
    std::thread readerB([&] { for (unsigned i = 0; i < 10000u; ++i) { ReadLease lease; if (lease) ++simultaneous; } });
    readerA.join(); readerB.join();
    assert(simultaneous > 0);
    { ReadLease held; assert(held); CommitLease timeout; assert(!timeout); }
    { CommitLease recovered; assert(recovered); }
    std::cout << "PROFILE_RUNTIME_GATE_TEST=PASS commits=2000 mixed=0 timeout_recovered=1\n";
}

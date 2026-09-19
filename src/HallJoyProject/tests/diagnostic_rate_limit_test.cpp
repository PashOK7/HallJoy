#include "diagnostic_rate_limit.h"
#include <cassert>
#include <cstdio>
#include <thread>
#include <vector>
int main() {
    halljoy::DiagnosticRateLimit budget;std::uint64_t suppressed=99;
    assert(budget.Take(1000,5000,suppressed) && suppressed==0);
    std::vector<std::thread> producers;
    for(unsigned i=0;i<8;++i)producers.emplace_back([&]{
        for(unsigned n=0;n<10000;++n){std::uint64_t ignored=0;assert(!budget.Take(2000,5000,ignored));}
    });
    for(auto& t:producers)t.join();
    assert(budget.Take(6000,5000,suppressed) && suppressed==80000);
    assert(!budget.Take(6000,5000,suppressed));
    assert(budget.Take(11000,5000,suppressed) && suppressed==1);
    std::puts("DIAGNOSTIC_RATE_LIMIT=PASS concurrent=80000 complete_suppression_count=1 bounded=1");
}

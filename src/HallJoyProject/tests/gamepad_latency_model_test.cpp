#include "gamepad_latency_model.h"
#include <cassert>
#include <limits>
#include <iostream>
int main(){
 halljoy::latency::Edges e;
 assert(e.Observe(true,0.) && e.known && !e.active);
 assert(e.Observe(true,1e-12) && e.presses==1); // no hidden deadzone
 assert(!e.Observe(true,.5) && e.presses==1);
 assert(e.Observe(true,0.) && e.releases==1);
 assert(e.Observe(true,-1e-12) && e.presses==2);
 e.Disconnect();assert(!e.known && !e.active && e.releases==1);
 assert(e.Observe(true,1.) && e.presses==2); // reconnect is baseline, not edge
 assert(e.Observe(true,std::numeric_limits<double>::quiet_NaN()) && !e.known);
 assert(!e.Observe(false,1.) && e.presses==2 && e.releases==1);
 assert(e.Observe(true,0.) && e.releases==1);
 std::cout<<"Latency edge semantics: zero, signed onset, no hidden threshold, disconnect/reconnect PASS\n";
}

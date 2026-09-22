#pragma once
#include <cmath>
#include <cstdint>
namespace halljoy::latency {
struct Edges {
    bool known=false,active=false;
    uint64_t presses=0,releases=0;
    void Disconnect() { known=false;active=false; }
    bool Observe(bool connected,double value) {
        if(!connected || !std::isfinite(value)){const bool changed=known;Disconnect();return changed;}
        const bool next=value!=0.; // no hidden deadzone, smoothing or hysteresis
        const bool changed=!known || next!=active;
        if(known && next!=active) {if(next)++presses;else ++releases;}
        known=true;active=next;return changed;
    }
};
}

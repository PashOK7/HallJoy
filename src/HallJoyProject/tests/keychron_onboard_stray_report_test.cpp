// Unsolicited RAW reports (QMK notifications share the endpoint) must not fail a
// query when they precede the reply; a flood longer than the bound must fail it.
#include "keychron_onboard_client.h"
#include <cassert>
#include <deque>
#include <iostream>

using namespace halljoy::k4_onboard;

// Models the RAW input queue: the first packet answers Exchange, the rest arrive via Read.
struct Queue : Channel {
    std::deque<Packet> packets;
    bool Exchange(const Packet&, Packet& r) override {
        if (packets.empty()) return false;
        r=packets.front(); packets.pop_front(); return true;
    }
    bool Read(Packet& r) override {
        if (packets.empty()) return false;
        r=packets.front(); packets.pop_front(); return true;
    }
    bool Reconnect(std::uint16_t) override { return false; }
    std::uint64_t NowMs() const override { return 0; }
    bool Cancelled() const override { return false; }
};

// QMK layer notification: KC_GET_DEFAULT_LAYER group (0xA3), not an HJO1 reply.
Packet LayerNotification() {
    Packet p{}; p[0]=0xa3; p[1]=2; return p;
}

// HJO1 status reply announcing the r9 HJP2 profile size (5100 = 0x13EC, little endian).
Packet StatusReply() {
    Packet p{}; p[0]=0xa9; p[1]=0x70; p[2]=0;
    std::memcpy(p.data()+8,"HJO1",4);
    p[18]=0xec; p[19]=0x13;
    return p;
}

int main() {
    {   // One notification before the reply is skipped and counted.
        Queue q; q.packets={LayerNotification(),StatusReply()};
        Client c(q); Packet reply{};
        assert(c.Status(reply));
        assert(c.StrayReports()==1 && c.LastStatus()==0);
    }
    {   // Up to kMaxStrayReports (8) notifications still succeed.
        Queue q; for(unsigned i=0;i<8;++i) q.packets.push_back(LayerNotification());
        q.packets.push_back(StatusReply());
        Client c(q); Packet reply{};
        assert(c.Status(reply));
        assert(c.StrayReports()==8 && c.LastStatus()==0);
    }
    {   // A flood longer than the bound fails the query instead of reading on.
        Queue q; for(unsigned i=0;i<9;++i) q.packets.push_back(LayerNotification());
        q.packets.push_back(StatusReply());
        Client c(q); Packet reply{};
        assert(!c.Status(reply));
        assert(c.LastStatus()==0xff);
    }
    {   // A transport failure is still a plain failure, with nothing skipped.
        Queue q; Client c(q); Packet reply{};
        assert(!c.Status(reply));
        assert(c.StrayReports()==0 && c.LastStatus()==0xff);
    }
    std::cout<<"Keychron onboard stray RAW report skipping PASS\n";
    return 0;
}

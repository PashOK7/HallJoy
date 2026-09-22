#include "../HallJoy/keychron_onboard_session.h"
#include <cassert>
#include <iostream>

int main() {
    hjo_session s{};
    assert(s.phase == HJO_OFF);
    assert(!hjo_start(&s, 1, 1, 0));
    const auto first = hjo_open(&s, 100);
    assert(first == 1 && s.phase == HJO_ARMED);
    assert(!hjo_open(&s, 110));
    assert(!hjo_start(&s, first + 1, 1, 120));
    assert(!hjo_start(&s, first, 0, 120));
    assert(hjo_start(&s, first, 1, 120));
    assert(!hjo_start(&s, first, 2, 130));
    assert(!hjo_host_stop(&s, first + 1));
    assert(!hjo_heartbeat(&s, first, 1, 200)); // replay cannot renew
    assert(!hjo_heartbeat(&s, first + 1, 2, 200));
    assert(s.refreshed_ms == 120);
    assert(hjo_heartbeat(&s, first, 2, 220));
    hjo_tick(&s, 719);
    assert(s.phase == HJO_ACTIVE);
    assert(!hjo_heartbeat(&s, first, 3, 720)); // arrival on expiry is too late
    assert(s.phase == HJO_OFF && s.neutral_pending);
    assert(!hjo_open(&s, 721)); // blocked until neutral actually delivered
    assert(!hjo_start(&s, first, 4, 721));
    hjo_neutral_delivered(&s);
    const auto second = hjo_open(&s, 722);
    assert(second == 2);
    assert(!hjo_host_stop(&s, first));
    assert(hjo_start(&s, second, 1, 723));
    assert(hjo_host_stop(&s, second));
    assert(s.neutral_pending && s.phase == HJO_OFF);
    assert(hjo_host_stop(&s, second));
    hjo_disconnect(&s);
    assert(!s.neutral_pending && s.generation == second);
    assert(hjo_open(&s, UINT32_MAX - 200u) == 3);
    assert(hjo_start(&s, 3, 1, UINT32_MAX - 200u));
    hjo_tick(&s, 298);
    assert(s.phase == HJO_ACTIVE); // 499 ms across timer wrap
    hjo_tick(&s, 299);
    assert(s.phase == HJO_OFF && s.neutral_pending);
    hjo_disconnect(&s);
    assert(hjo_open(&s, 1000) == 4);
    hjo_tick(&s, 6000);
    assert(s.phase == HJO_OFF && !s.neutral_pending); // never started
    assert(!hjo_start(&s, 4, 1, 6001));
    assert(hjo_new_sequence(0, UINT32_MAX));
    assert(!hjo_new_sequence(UINT32_MAX, 0));
    assert(!hjo_new_sequence(0x80000000u, 0));
    s.generation = UINT32_MAX;
    assert(!hjo_open(&s, 6002));
    std::cout << "onboard session lifecycle PASS\n";
}

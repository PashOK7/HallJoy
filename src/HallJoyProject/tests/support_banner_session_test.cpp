#include "support_banner_session.h"
#include <cassert>
#include <iostream>
int main() {
    using namespace halljoy;
    keyboard_support::StatusSnapshot info{true, true, keyboard_support::GenericProtocol};
    support_banner::Session run{1000};
    assert(run.Visible(info));
    assert(!run.Ready(999) && !run.Ready(1000) && !run.Ready(60999));
    assert(!run.Dismiss(60999, info) && run.Visible(info));
    assert(run.Ready(61000) && run.Dismiss(61000, info));
    assert(!run.Visible(info) && !run.Dismiss(62000, info));
    info.communicationWarning = true;
    assert(run.Visible(info));
    info.communicationWarning = false;
    for (auto warning : {keyboard_support::Mad68DualLimited, keyboard_support::Mix87Limited,
                        keyboard_support::CorsairVanguard, keyboard_support::KeyboardBusy,
                        keyboard_support::CorsairVanguardPending, keyboard_support::Alumix104Research}) {
        info.frozenModels = warning;
        assert(run.Visible(info));
        support_banner::Session fresh{0};
        assert(!fresh.Dismiss(60000, info));
    }
    info.frozenModels = keyboard_support::GenericProtocol;
    assert(!run.Visible(info)); // Reconnection does not reset dismissal.
    support_banner::Session nextRun{90000};
    assert(nextRun.Visible(info) && !nextRun.Ready(90000));
    assert(nextRun.Dismiss(150000, info));
    info.searchCompleted = false;
    assert(!nextRun.Visible(info));
    std::cout << "support banner: minute boundary, per-run reset and operational warnings passed\n";
}

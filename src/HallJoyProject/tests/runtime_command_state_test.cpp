#include <cassert>

#include "../HallJoy/runtime_command_state.h"

using halljoy::runtime_command::Controller;
using halljoy::runtime_command::RequestStatus;
using halljoy::runtime_command::State;

int main()
{
    Controller controller;
    assert(controller.Snapshot().state == State::Paused);
    assert(!controller.Snapshot().opensAdmitted);
    assert(controller.RequestPause() == RequestStatus::NoChange);

    assert(controller.RequestResume() == RequestStatus::Accepted);
    assert(controller.AdvanceResume());
    assert(controller.AdvanceResume());
    assert(controller.AbortResume(55u));
    assert(controller.Snapshot().state == State::Paused);
    assert(!controller.Snapshot().opensAdmitted);
    assert(controller.Snapshot().lastNativeError == 55u);

    assert(controller.RequestResume() == RequestStatus::Accepted);
    assert(controller.AdvanceResume());
    assert(controller.AdvanceResume());
    assert(controller.AdvanceResume());
    assert(controller.AdvanceResume());
    assert(controller.Snapshot().state == State::Active);
    assert(!controller.Snapshot().opensAdmitted);
    assert(controller.ConfirmAdmissionOpen());
    assert(controller.Snapshot().opensAdmitted);

    assert(controller.RequestPause() == RequestStatus::Accepted);
    assert(controller.RequestPause() == RequestStatus::NoChange);
    assert(!controller.Snapshot().opensAdmitted);
    // Both accepted resume attempts (including the safely aborted one) and
    // this accepted pause own distinct command generations.
    assert(controller.Snapshot().commandGeneration == 3);
    assert(controller.AdvancePause());
    assert(controller.AdvancePause());
    assert(controller.AdvancePause());
    assert(controller.AdvancePause());
    assert(controller.Snapshot().state == State::Paused);
    assert(controller.RequestPause() == RequestStatus::NoChange);

    assert(controller.RequestResume() == RequestStatus::Accepted);
    assert(controller.RequestResume() == RequestStatus::NoChange);
    assert(controller.Snapshot().commandGeneration == 4);
    assert(!controller.Snapshot().opensAdmitted);
    assert(controller.AdvanceResume());
    assert(controller.AdvanceResume());
    assert(controller.AdvanceResume());
    assert(controller.AdvanceResume());
    assert(controller.Snapshot().state == State::Active);
    assert(!controller.Snapshot().opensAdmitted);
    assert(controller.ConfirmAdmissionOpen());
    assert(controller.Snapshot().opensAdmitted);

    assert(controller.RequestPause() == RequestStatus::Accepted);
    controller.Fault(1234u);
    assert(controller.Snapshot().state == State::PauseFaulted);
    assert(!controller.Snapshot().opensAdmitted);
    assert(controller.Snapshot().lastNativeError == 1234u);
    assert(controller.RequestPause() == RequestStatus::Rejected);
    assert(controller.RequestResume() == RequestStatus::Rejected);
    return 0;
}

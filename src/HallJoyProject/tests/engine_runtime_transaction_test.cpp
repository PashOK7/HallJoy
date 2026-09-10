#include <cassert>
#include <cstdint>
#include <string>
#include <vector>

#include "../HallJoy/engine_runtime_transaction.h"

namespace
{
struct FakeOperations
{
    std::string log;
    const char* fail = nullptr;
    std::uint32_t error = 88u;
    std::vector<halljoy::runtime_command::State> observedStates;

    bool Step(const char* name, std::uint32_t& out) noexcept
    {
        log += name;
        log += ";";
        if (fail && std::string(name) == fail)
        {
            out = error;
            return false;
        }
        return true;
    }

    bool CloseAdmission(std::uint32_t& e) noexcept { return Step("close", e); }
    bool StopRecoverySupervisor(std::uint32_t& e) noexcept { return Step("supervisor", e); }
    bool PublishNeutral(std::uint32_t& e) noexcept { return Step("neutral", e); }
    bool StopRealtime(std::uint32_t& e) noexcept { return Step("realtime", e); }
    bool ReleaseUiInput(std::uint32_t& e) noexcept { return Step("ui-release", e); }
    bool StopNativeProviders(std::uint32_t& e) noexcept { return Step("native", e); }
    bool ReleaseBackendLeases(std::uint32_t& e) noexcept { return Step("leases", e); }
    bool EnumerateFresh(std::uint32_t& e) noexcept { return Step("enumerate", e); }
    bool ProveCapabilities(std::uint32_t& e) noexcept { return Step("prove", e); }
    bool StartFreshGeneration(std::uint32_t& e) noexcept { return Step("start", e); }
    bool PublishNeutralGeneration(std::uint32_t& e) noexcept { return Step("publish-neutral", e); }
    bool RestoreUiInput(std::uint32_t& e) noexcept { return Step("ui-restore", e); }
    bool OpenAdmission(std::uint32_t& e) noexcept { return Step("open", e); }
    bool ReleaseFailedResume(std::uint32_t& e) noexcept { return Step("resume-release", e); }
    void StateChanged(const halljoy::runtime_command::SnapshotV1& snapshot) noexcept
    {
        observedStates.push_back(snapshot.state);
    }
};
}

int main()
{
    using halljoy::engine_runtime::ExecutePause;
    using halljoy::engine_runtime::ExecuteResume;
    using halljoy::engine_runtime::TransactionResult;
    using halljoy::runtime_command::Controller;
    using halljoy::runtime_command::State;

    Controller controller;
    FakeOperations ops;
    std::uint32_t error = 0;

    assert(ExecuteResume(controller, ops, error) == TransactionResult::Completed);
    assert(controller.Snapshot().state == State::Active);
    assert(ops.log == "enumerate;prove;start;publish-neutral;ui-restore;open;");
    assert((ops.observedStates == std::vector<State>{
        State::ResumeRequested, State::Enumerating, State::ProvingCapabilities,
        State::PublishingNeutralGeneration, State::Active }));

    ops.log.clear();
    ops.observedStates.clear();
    assert(ExecutePause(controller, ops, error) == TransactionResult::Completed);
    assert(controller.Snapshot().state == State::Paused);
    assert(ops.log == "close;supervisor;neutral;realtime;ui-release;native;leases;");
    assert((ops.observedStates == std::vector<State>{
        State::PauseRequested, State::Neutralizing, State::StoppingProviders,
        State::ReleasingLeases, State::Paused }));

    ops.fail = "prove";
    ops.log.clear();
    ops.observedStates.clear();
    assert(ExecuteResume(controller, ops, error) == TransactionResult::RetryableResumeFailure);
    assert(controller.Snapshot().state == State::Paused);
    assert(error == ops.error);
    assert(ops.log == "enumerate;prove;resume-release;");
    assert((ops.observedStates == std::vector<State>{
        State::ResumeRequested, State::Enumerating, State::Paused }));

    ops.fail = nullptr;
    ops.fail = "leases";
    ops.log.clear();
    assert(ExecuteResume(controller, ops, error) == TransactionResult::Completed);
    assert(ExecutePause(controller, ops, error) == TransactionResult::Faulted);
    assert(controller.Snapshot().state == State::PauseFaulted);
    return 0;
}

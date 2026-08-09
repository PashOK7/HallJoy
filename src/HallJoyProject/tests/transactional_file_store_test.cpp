#include "transactional_file_store.h"

#include <cassert>
#include <cstdint>
#include <string>

using HallJoyPersistence::SaveStage;

namespace
{
    struct FakeAdapter
    {
        SaveStage injectedFailure = SaveStage::None;
        std::string destination = "GOOD";
        std::string temporary;
        bool temporaryExists = false;
        bool cleaned = false;
        std::uint32_t error = 0;

        bool Step(SaveStage stage)
        {
            if (injectedFailure != stage) return true;
            error = 1000u + static_cast<std::uint32_t>(stage);
            return false;
        }

        bool Prepare()
        {
            temporaryExists = true;
            temporary.clear();
            return Step(SaveStage::Prepare);
        }

        bool Write()
        {
            temporary = "NEW";
            return Step(SaveStage::Write);
        }

        bool Flush() { return Step(SaveStage::Flush); }
        bool Validate() { return temporary == "NEW" && Step(SaveStage::Validate); }

        bool Replace()
        {
            if (!Step(SaveStage::Replace)) return false;
            destination = temporary;
            temporaryExists = false;
            return true;
        }

        void Cleanup() noexcept
        {
            cleaned = true;
            temporaryExists = false;
            temporary.clear();
        }

        std::uint32_t LastError() const noexcept { return error; }
    };
}

int main()
{
    constexpr SaveStage failureStages[] = {
        SaveStage::Prepare,
        SaveStage::Write,
        SaveStage::Flush,
        SaveStage::Validate,
        SaveStage::Replace,
    };

    for (const SaveStage stage : failureStages)
    {
        FakeAdapter adapter;
        adapter.injectedFailure = stage;
        const auto result = HallJoyPersistence::SaveTransaction(adapter);
        assert(!result.Succeeded());
        assert(result.stage == stage);
        assert(result.nativeError != 0);
        assert(adapter.destination == "GOOD");
        assert(!adapter.temporaryExists);
        assert(adapter.cleaned);
    }

    FakeAdapter adapter;
    const auto result = HallJoyPersistence::SaveTransaction(adapter);
    assert(result.Succeeded());
    assert(adapter.destination == "NEW");
    assert(!adapter.temporaryExists);
    assert(!adapter.cleaned);
    return 0;
}

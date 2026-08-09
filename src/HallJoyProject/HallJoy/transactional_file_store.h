#pragma once

#include <cstdint>

namespace HallJoyPersistence
{
    enum class SaveStage : std::uint8_t
    {
        None = 0,
        Prepare,
        Write,
        Flush,
        Validate,
        Replace,
        UnexpectedException,
    };

    struct SaveResult
    {
        SaveStage stage = SaveStage::None;
        std::uint32_t nativeError = 0;

        constexpr bool Succeeded() const noexcept
        {
            return stage == SaveStage::None;
        }
    };

    // The adapter owns the temporary file and must implement:
    // Prepare, Write, Flush, Validate, Replace, Cleanup and LastError.
    // Replace is the only operation allowed to modify the destination.
    template <typename Adapter>
    SaveResult SaveTransaction(Adapter& adapter)
    {
        auto fail = [&](SaveStage stage) noexcept
        {
            const std::uint32_t error = adapter.LastError();
            adapter.Cleanup();
            return SaveResult{ stage, error != 0 ? error : 1u };
        };

        try
        {
            if (!adapter.Prepare()) return fail(SaveStage::Prepare);
            if (!adapter.Write()) return fail(SaveStage::Write);
            if (!adapter.Flush()) return fail(SaveStage::Flush);
            if (!adapter.Validate()) return fail(SaveStage::Validate);
            if (!adapter.Replace()) return fail(SaveStage::Replace);
            return {};
        }
        catch (...)
        {
            adapter.Cleanup();
            return { SaveStage::UnexpectedException, 1u };
        }
    }
}

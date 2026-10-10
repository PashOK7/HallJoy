#include "../HallJoy/configured_xusb_builder.h"

#include <cassert>
#include <cstdlib>

// Two keys on one stick direction: the most pressed (post-curve) value wins.
// The per-key curves that split a direction into ranges are applied before this
// point, so the builder only sees the curved outputs.
int main()
{
    using namespace halljoy::configured_xusb;
    constexpr std::uint16_t kD = 7, kF = 9, kG = 10;   // any supported keys
    constexpr std::uint16_t kRightA = 26, kRightB = 27;

    const auto stick = [](const halljoy::controller::VirtualControllerFrameV1& frame) { return frame.leftStickX; };
    const auto near = [](int value, int expected, int tolerance) {
        return std::abs(value - expected) <= tolerance;
    };

    // One key per side keeps the old behaviour: plus - minus.
    {
        PadConfiguration configuration{};
        configuration.axes[0] = AxisBinding::Single(kD, kRightA);
        InputValues input{};
        input.filtered[kD] = 0.6f;
        BuilderState state{};
        const auto frame = BuildReport(configuration, input, state);
        assert(near(stick(frame), -19660, 2));
    }

    // Two minus keys do not add up: the most pressed one (0.6) wins, not 1.1.
    {
        PadConfiguration configuration{};
        configuration.axes[0].minusHids = { kD, kF, 0, 0, 0, 0, 0, 0 };
        InputValues input{};
        input.filtered[kD] = 0.5f;
        input.filtered[kF] = 0.6f;
        BuilderState state{};
        assert(near(stick(BuildReport(configuration, input, state)), -19660, 2));
    }

    // Equal values on two keys: the output is that value, not a sum.
    {
        PadConfiguration configuration{};
        configuration.axes[0].minusHids = { kD, kF, 0, 0, 0, 0, 0, 0 };
        InputValues input{};
        input.filtered[kD] = 0.5f;
        input.filtered[kF] = 0.5f;
        BuilderState state{};
        assert(near(stick(BuildReport(configuration, input, state)), -16384, 2));
    }

    // Releasing the most pressed key falls back to the next most pressed one.
    {
        PadConfiguration configuration{};
        configuration.axes[0].minusHids = { kD, kF, kG, 0, 0, 0, 0, 0 };
        InputValues input{};
        input.filtered[kD] = 0.0f;
        input.filtered[kF] = 0.3f;
        input.filtered[kG] = 0.8f;
        BuilderState state{};
        assert(near(stick(BuildReport(configuration, input, state)), -26214, 2));
        input.filtered[kG] = 0.0f;
        assert(near(stick(BuildReport(configuration, input, state)), -9830, 2));
    }

    // Plus side takes the most pressed key too; the minus side stays idle.
    {
        PadConfiguration configuration{};
        configuration.axes[0].plusHids = { kRightA, kRightB, 0, 0, 0, 0, 0, 0 };
        InputValues input{};
        input.filtered[kRightA] = 0.25f;
        input.filtered[kRightB] = 0.75f;
        BuilderState state{};
        assert(near(stick(BuildReport(configuration, input, state)), 24575, 2));
    }

    // Empty keys contribute nothing; an all-empty side reads as zero.
    {
        PadConfiguration configuration{};
        configuration.axes[0].minusHids = { 0, 0, 0, 0, 0, 0, 0, 0 };
        InputValues input{};
        BuilderState state{};
        assert(stick(BuildReport(configuration, input, state)) == 0);
    }
    return 0;
}

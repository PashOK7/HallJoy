#include "keychron_onboard_curve.h"
#include "backend_curve.h"
#include "curve_math.h"
#include "key_settings.h"
#include <cassert>
#include <cmath>
#include <iostream>
#include <random>

int main() {
    std::mt19937 rng(0x484a);
    auto unit = [&]() { return static_cast<float>(rng() % 65536) / 65535.0f; };
    unsigned checked = 0;
    for (unsigned trial = 0; trial < 400; ++trial) {
        KeyDeadzone k{};
        k.useUnique = true; k.invert = trial & 1;
        k.curveMode = (trial >> 1) & 1;
        k.low = unit() * .25f; k.high = .75f + unit() * .25f;
        k.cp1_x = k.low + (k.high-k.low) * .3f;
        k.cp2_x = k.low + (k.high-k.low) * .7f;
        k.antiDeadzone = unit(); k.outputCap = unit();
        k.cp1_y = unit(); k.cp2_y = unit();
        k.cp1_w = trial % 3 == 0 ? 0 : (trial % 3 == 1 ? 1 : unit());
        k.cp2_w = unit();
        KeySettings_Set(4, k);
        // Read the production-normalized definition actually used by HallJoy.
        k = KeySettings_Get(4);
        hjo_curve c{};
        BackendCurve_ExportPrepared(4, c);
        assert(hjo_curve_valid(&c));
        for (unsigned sample = 0; sample <= 256; ++sample) {
            const float raw = static_cast<float>(sample) / 256.0f;
            const float actual = hjo_curve_apply(&c, raw);
            const float expected = BackendCurve_ApplyByHid(4, raw);
            assert(std::fabs(actual-expected) <= 1e-6f);
            ++checked;
        }
        for (float x : c.x) {
            const float raw = k.invert ? 1-x : x;
            assert(std::fabs(hjo_curve_apply(&c,raw)-BackendCurve_ApplyByHid(4,raw))<=1e-6f);
        }
    }
    std::cout << checked << " onboard/production curve comparisons PASS\n";
}

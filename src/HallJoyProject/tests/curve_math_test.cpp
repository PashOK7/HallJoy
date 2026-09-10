#include <cassert>
#include <cmath>
#include <limits>

#include "curve_math.h"

int main()
{
    CurveMath::Curve01 curve{};
    assert(CurveMath::Clamp01(std::numeric_limits<float>::quiet_NaN()) == 0.0f);
    assert(CurveMath::Clamp01(std::numeric_limits<float>::infinity()) == 0.0f);
    assert(CurveMath::EvalRationalYForX(curve, 0.0f) <= 0.0001f);
    assert(CurveMath::EvalRationalYForX(curve, 1.0f) >= 0.9999f);

    curve.x1 = 0.85f;
    curve.x2 = 0.15f;
    curve.w1 = std::numeric_limits<float>::quiet_NaN();
    curve.w2 = std::numeric_limits<float>::infinity();
    const float result = CurveMath::EvalRationalYForX(
        curve, std::numeric_limits<float>::quiet_NaN());
    assert(std::isfinite(result) && result >= 0.0f && result <= 1.0f);
    return 0;
}

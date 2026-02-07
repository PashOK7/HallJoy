// curve_math.cpp
#define NOMINMAX
#include "curve_math.h"

namespace CurveMath
{
    static inline float Bernstein0(float t) { float u = 1.0f - t; return u * u * u; }
    static inline float Bernstein1(float t) { float u = 1.0f - t; return 3.0f * u * u * t; }
    static inline float Bernstein2(float t) { float u = 1.0f - t; return 3.0f * u * t * t; }
    static inline float Bernstein3(float t) { return t * t * t; }

    float Weight01ToRational(float w)
    {
        w = Clamp01(w);
        if (w <= 0.0f) return 0.0f;
        if (w >= 1.0f) return 200.0f;

        const float gamma = 1.2f; // попробуй 1.3 .. 2.2
        float r = w / (1.0f - w);
        r = powf(r, gamma);
        return std::clamp(r, 0.0f, 200.0f);
    }

    Vec2 EvalRationalBezier(const Curve01& c, float t01)
    {
        float t = Clamp01(t01);

        // Endpoint weights are fixed to 1
        const float w0 = 1.0f;
        const float w3 = 1.0f;

        // CP weights are user-controlled in [0..1], mapped to rational weights
        const float w1 = Weight01ToRational(c.w1);
        const float w2 = Weight01ToRational(c.w2);

        float b0 = Bernstein0(t);
        float b1 = Bernstein1(t);
        float b2 = Bernstein2(t);
        float b3 = Bernstein3(t);

        float d = b0 * w0 + b1 * w1 + b2 * w2 + b3 * w3;
        if (d <= 1e-8f) d = 1e-8f;

        float x = (b0 * w0 * c.x0 + b1 * w1 * c.x1 + b2 * w2 * c.x2 + b3 * w3 * c.x3) / d;
        float y = (b0 * w0 * c.y0 + b1 * w1 * c.y1 + b2 * w2 * c.y2 + b3 * w3 * c.y3) / d;

        return { x, y };
    }

    float EvalRationalX(const Curve01& c, float t01)
    {
        return EvalRationalBezier(c, t01).x;
    }

    float EvalRationalY(const Curve01& c, float t01)
    {
        return EvalRationalBezier(c, t01).y;
    }

    float EvalRationalYForX(const Curve01& c, float x01, int iters)
    {
        x01 = Clamp01(x01);

        // Binary search for t in [0..1] such that x(t) ~= x01
        float lo = 0.0f;
        float hi = 1.0f;

        // Clamp iterations
        iters = std::clamp(iters, 6, 30);

        for (int i = 0; i < iters; ++i)
        {
            float mid = 0.5f * (lo + hi);
            float xm = EvalRationalX(c, mid);

            if (xm < x01) lo = mid;
            else          hi = mid;
        }

        float t = 0.5f * (lo + hi);
        float y = EvalRationalY(c, t);

        return Clamp01(y);
    }
}
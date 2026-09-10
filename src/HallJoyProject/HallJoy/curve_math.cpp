// curve_math.cpp
#define NOMINMAX
#include "curve_math.h"

namespace CurveMath
{
    static Curve01 SanitizeCurve(Curve01 c)
    {
        c.x0 = Clamp01(c.x0); c.y0 = Clamp01(c.y0);
        c.x1 = Clamp01(c.x1); c.y1 = Clamp01(c.y1);
        c.x2 = Clamp01(c.x2); c.y2 = Clamp01(c.y2);
        c.x3 = Clamp01(c.x3); c.y3 = Clamp01(c.y3);
        c.w1 = Clamp01(c.w1); c.w2 = Clamp01(c.w2);
        return c;
    }

    static inline float Bernstein0(float t) { float u = 1.0f - t; return u * u * u; }
    static inline float Bernstein1(float t) { float u = 1.0f - t; return 3.0f * u * u * t; }
    static inline float Bernstein2(float t) { float u = 1.0f - t; return 3.0f * u * t * t; }
    static inline float Bernstein3(float t) { return t * t * t; }

    static inline float RationalDenom(float b0, float b1, float b2, float b3, float rw1, float rw2)
    {
        float d = b0 + b1 * rw1 + b2 * rw2 + b3;
        return (d > 1e-8f) ? d : 1e-8f;
    }

    static inline float EvalRationalXRawUnchecked(const Curve01& c, float t, float rw1, float rw2)
    {
        float b0 = Bernstein0(t);
        float b1 = Bernstein1(t);
        float b2 = Bernstein2(t);
        float b3 = Bernstein3(t);
        float d = RationalDenom(b0, b1, b2, b3, rw1, rw2);
        return (b0 * c.x0 + b1 * rw1 * c.x1 + b2 * rw2 * c.x2 + b3 * c.x3) / d;
    }

    static inline float EvalRationalYRawUnchecked(const Curve01& c, float t, float rw1, float rw2)
    {
        float b0 = Bernstein0(t);
        float b1 = Bernstein1(t);
        float b2 = Bernstein2(t);
        float b3 = Bernstein3(t);
        float d = RationalDenom(b0, b1, b2, b3, rw1, rw2);
        return (b0 * c.y0 + b1 * rw1 * c.y1 + b2 * rw2 * c.y2 + b3 * c.y3) / d;
    }

    static Vec2 EvalRationalBezierRaw(const Curve01& c, float t01, float rw1, float rw2)
    {
        float t = Clamp01(t01);

        float b0 = Bernstein0(t);
        float b1 = Bernstein1(t);
        float b2 = Bernstein2(t);
        float b3 = Bernstein3(t);
        float d = RationalDenom(b0, b1, b2, b3, rw1, rw2);

        float x = (b0 * c.x0 + b1 * rw1 * c.x1 + b2 * rw2 * c.x2 + b3 * c.x3) / d;
        float y = (b0 * c.y0 + b1 * rw1 * c.y1 + b2 * rw2 * c.y2 + b3 * c.y3) / d;

        return { x, y };
    }

    float Weight01ToRational(float w)
    {
        w = Clamp01(w);
        if (w <= 0.0f) return 0.0f;
        if (w >= 1.0f) return 200.0f;

        const float gamma = 1.2f;
        float r = w / (1.0f - w);
        r = powf(r, gamma);
        return std::clamp(r, 0.0f, 200.0f);
    }

    Vec2 EvalRationalBezier(const Curve01& c, float t01)
    {
        const Curve01 safe = SanitizeCurve(c);
        const float rw1 = Weight01ToRational(safe.w1);
        const float rw2 = Weight01ToRational(safe.w2);
        return EvalRationalBezierRaw(safe, t01, rw1, rw2);
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
        const Curve01 safe = SanitizeCurve(c);
        x01 = Clamp01(x01);
        if (x01 <= safe.x0) return safe.y0;
        if (x01 >= safe.x3) return safe.y3;
        const float rw1 = Weight01ToRational(safe.w1);
        const float rw2 = Weight01ToRational(safe.w2);

        // Binary search for t in [0..1] such that x(t) ~= x01
        float lo = 0.0f;
        float hi = 1.0f;

        // Clamp iterations
        iters = std::clamp(iters, 6, 30);

        for (int i = 0; i < iters; ++i)
        {
            float mid = 0.5f * (lo + hi);
            float xm = EvalRationalXRawUnchecked(safe, mid, rw1, rw2);

            if (xm < x01) lo = mid;
            else          hi = mid;
        }

        float t = 0.5f * (lo + hi);
        float y = EvalRationalYRawUnchecked(safe, t, rw1, rw2);

        return Clamp01(y);
    }
}

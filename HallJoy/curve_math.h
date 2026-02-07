// curve_math.h
#pragma once
#define NOMINMAX

#include <algorithm>
#include <cmath>
#include <cstdint>

#include "key_settings.h"

// Shared curve math for BOTH:
//  - UI graph rendering
//  - Backend input mapping
//
// Goal: single source of truth so visuals match behavior.
//
// We implement a Rational Cubic Bezier in 2D (x,y):
//   - Endpoints have implicit weight 1
//   - Control points CP1/CP2 have user weights in [0..1], mapped to rational weights
//
// Meaning of cp*_w (0..1):
//   0 => control point has no influence (curve becomes straight line between endpoints)
//   1 => very strong influence (curve hugs control point, looks "almost segmented" but stays smooth)
//
// Notes:
// - Rational Bezier is always geometrically smooth (no true corners).
// - "Segmented (Linear)" mode remains separate and is NOT handled here (UI/backend can call their own segment logic).

namespace CurveMath
{
    struct Vec2
    {
        float x = 0.0f;
        float y = 0.0f;
    };

    // Generic curve definition in normalized graph space [0..1].
    // x0/y0 = Start (P0)
    // x1/y1 = Control Point 1 (P1)
    // x2/y2 = Control Point 2 (P2)
    // x3/y3 = End (P3)
    // w1/w2 = CP weights in [0..1] (NOT rational weights; those are derived internally)
    struct Curve01
    {
        float x0 = 0.0f, y0 = 0.0f;
        float x1 = 0.0f, y1 = 0.0f;
        float x2 = 0.0f, y2 = 0.0f;
        float x3 = 1.0f, y3 = 1.0f;

        float w1 = 1.0f; // [0..1]
        float w2 = 1.0f; // [0..1]
    };

    inline float Clamp01(float v) { return std::clamp(v, 0.0f, 1.0f); }
    inline float Lerp(float a, float b, float t) { return a + (b - a) * t; }

    // Map [0..1] user weight into a rational Bezier weight.
    // We use a "blow-up near 1" mapping so top-end has strong effect.
    // Returned value is clamped to a safe maximum.
    float Weight01ToRational(float w01);

    // Evaluate 2D rational cubic Bezier at parameter t in [0..1].
    // Uses:
    //   w0 = w3 = 1
    //   w1 = Weight01ToRational(curve.w1)
    //   w2 = Weight01ToRational(curve.w2)
    Vec2 EvalRationalBezier(const Curve01& c, float t01);

    // Convenience: evaluate only X or Y at t
    float EvalRationalX(const Curve01& c, float t01);
    float EvalRationalY(const Curve01& c, float t01);

    // Solve x(t)=x for t via binary search, then return y(t).
    // Assumes x(t) is monotonic increasing on [0..1] (your project already enforces that).
    float EvalRationalYForX(const Curve01& c, float x01, int iters = 18);

    // Helper: build Curve01 from KeyDeadzone (UI/backend share the same meaning).
    inline Curve01 FromKeyDeadzone(const KeyDeadzone& ks)
    {
        Curve01 c{};
        c.x0 = ks.low;          c.y0 = ks.antiDeadzone;
        c.x1 = ks.cp1_x;        c.y1 = ks.cp1_y;
        c.x2 = ks.cp2_x;        c.y2 = ks.cp2_y;
        c.x3 = ks.high;         c.y3 = ks.outputCap;
        c.w1 = ks.cp1_w;
        c.w2 = ks.cp2_w;
        return c;
    }
}
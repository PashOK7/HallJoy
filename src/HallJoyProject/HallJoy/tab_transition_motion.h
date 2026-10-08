#pragma once
#include <algorithm>
#include <cmath>

// Camera motion of the tab transition (portable, tested on every platform).
// Position is in tab units: 2.5 is halfway between tabs 2 and 3.
namespace halljoy::tab_transition {

inline constexpr double kDuration = 0.20; // seconds, owner decision 2026-10-03 (was 0.30)

// Quintic Hermite from (x0, v0) to (x1, 0) with zero acceleration at both
// ends. From rest it is smootherstep (6s^5 - 15s^4 + 10s^3): a slower start
// and finish and a faster middle than cubic ease-in-out (peak speed 1.875x
// the mean instead of 1.5x). A retarget keeps the current speed, so a click
// during a transition bends the path instead of jerking it.
struct Motion {
    double x0 = 0, v0 = 0, x1 = 0, duration = kDuration;

    double S(double t) const { return duration > 0 ? std::clamp(t / duration, 0.0, 1.0) : 1.0; }
    double Position(double t) const {
        const double s = S(t), s3 = s * s * s, s4 = s3 * s, s5 = s4 * s;
        const double h5 = 10 * s3 - 15 * s4 + 6 * s5;      // 0 -> 1
        const double h1 = s - 6 * s3 + 8 * s4 - 3 * s5;     // initial velocity
        return x0 + (x1 - x0) * h5 + duration * v0 * h1;
    }
    double Velocity(double t) const {
        if (duration <= 0 || t >= duration) return 0;
        const double s = S(t), s2 = s * s, s3 = s2 * s, s4 = s3 * s;
        const double d5 = 30 * s2 - 60 * s3 + 30 * s4;
        const double d1 = 1 - 18 * s2 + 32 * s3 - 15 * s4;
        return ((x1 - x0) * d5 + duration * v0 * d1) / duration;
    }
    bool Done(double t) const { return t >= duration; }
};

// New motion that starts where `current` is at time t (seconds since its
// start), with its velocity. Velocity toward the target is limited so the
// camera never passes it and comes back: dx/ds = (1-s)^2 (30 D s^2 +
// T v0 (1 + 2s - 15 s^2)) stays >= 0 on [0, 1] exactly when T v0 <= 2.5 D.
// Velocity away from the target is kept: the camera turns around smoothly.
inline Motion Retarget(const Motion& current, double t, double target, double duration = kDuration) {
    Motion next;
    next.x0 = current.Position(t);
    next.v0 = current.Velocity(t);
    next.x1 = target;
    next.duration = duration;
    const double distance = target - next.x0;
    if (distance * next.v0 > 0) {
        const double limit = 2.5 * std::abs(distance) / duration;
        next.v0 = std::clamp(next.v0, -limit, limit);
    }
    return next;
}

} // namespace halljoy::tab_transition

#include "tab_transition_motion.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>

using namespace halljoy::tab_transition;

static int failures = 0;
#define CHECK(x) do { if (!(x)) { std::printf("FAIL line %d: %s\n", __LINE__, #x); ++failures; } } while (0)
static bool Near(double a, double b, double eps = 1e-9) { return std::abs(a - b) <= eps; }

int main() {
    // From rest: exact endpoints, monotone, zero speed at both ends, 0.3 s.
    Motion m; m.x0 = 0; m.x1 = 5; m.v0 = 0;
    const double T = kDuration;
    CHECK(Near(T, 0.20) && Near(m.duration, T));
    CHECK(Near(m.Position(0), 0) && Near(m.Position(m.duration), 5) && Near(m.Position(1.0), 5));
    CHECK(Near(m.Velocity(0), 0) && Near(m.Velocity(m.duration), 0));
    CHECK(!m.Done(T * 0.99) && m.Done(T));
    double previous = m.Position(0), maxStep = 0;
    const int steps = 1000;                              // T / 1000 steps
    for (int i = 1; i <= steps; ++i) {
        const double p = m.Position(T * i / steps);
        CHECK(p >= previous - 1e-12);                   // never moves backwards
        maxStep = std::max(maxStep, p - previous);
        previous = p;
    }
    // Smootherstep: peak speed 1.875x the mean, slow start and finish.
    CHECK(maxStep < 5.0 / steps * 1.875 + 1e-6 && maxStep > 5.0 / steps * 1.8);
    CHECK(m.Position(T * 0.1) < 5.0 * 0.01);            // first 10% of time: under 1% of the way
    CHECK(Near(m.Position(T * 0.5), 2.5));              // symmetric ease-in-out

    // Retarget keeps position and velocity continuous.
    const double t = T * 0.4;
    const Motion back = Retarget(m, t, 1.0);
    CHECK(Near(back.Position(0), m.Position(t)));
    CHECK(Near(back.Velocity(0), m.Velocity(t), 1e-6));  // moving away: kept, the camera turns around
    CHECK(Near(back.Position(back.duration), 1.0));
    double furthest = back.Position(0);
    for (int i = 0; i <= steps; ++i) furthest = std::max(furthest, back.Position(T * i / steps));
    CHECK(furthest > back.Position(0));                 // it first continues, then turns
    // Toward a near target at high speed: limited, never overshoots.
    const Motion near = Retarget(m, T * 0.5, 2.7);
    CHECK(near.v0 > 0 && near.v0 * near.duration <= 2.5 * (2.7 - near.x0) + 1e-9);
    for (int i = 0; i <= steps; ++i) CHECK(near.Position(T * i / steps) <= 2.7 + 1e-9);
    // At the exact bound the path is still monotone.
    Motion edge; edge.x0 = 0; edge.x1 = 1; edge.v0 = 2.5 / edge.duration;
    for (int i = 1; i <= steps; ++i) CHECK(edge.Position(T * i / steps) >= edge.Position(T * (i - 1) / steps) - 1e-12);
    for (int i = 0; i <= steps; ++i) CHECK(edge.Position(T * i / steps) <= 1.0 + 1e-9);
    CHECK(Near(near.Position(near.duration), 2.7));
    // Retarget to the same target keeps a valid motion.
    const Motion same = Retarget(m, T, 5.0);
    CHECK(Near(same.Position(0), 5.0) && Near(same.Position(same.duration), 5.0));

    if (failures) return EXIT_FAILURE;
    std::printf("TAB_TRANSITION_MOTION=PASS endpoints smooth retarget continuity no_overshoot turnaround\n");
    return EXIT_SUCCESS;
}

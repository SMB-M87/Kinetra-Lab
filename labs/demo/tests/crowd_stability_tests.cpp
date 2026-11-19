// Soak tests for dense Demo crowds: 60 simulated seconds per seed. These seeds stalled
// only after sustained crowd motion, beyond the old five-frame smoke checks, so the
// tests step simulated time rather than rendering speed. CTest labels them `soak`.
//
// Both seeds run with the default steering wander. One of them also runs with route
// wander, so the mode that the debug menu offers for comparison stays covered.
#include "../include/sandbox_application.h"

#include <doctest/doctest.h>

#include <algorithm>

namespace {
/// Fixed steps per simulated run: 60 seconds at the Demo's 120 Hz clock.
constexpr int soak_ticks = 7200;

/// Runs a 240-agent crowd that wanders by `wander` for `soak_ticks` frames and checks
/// every tick that contact never exhausts the swept-event budget and that all motion
/// stays finite.
void soak(unsigned seed, sandbox::WanderMode wander) {
    sandbox::SandboxApplication application(240, seed);
    application.tuning().wander.mode = wander;
    kinetra::Camera2D camera;
    camera.viewport = {1280, 800};
    camera.pixels_per_unit = 17;
    for (int tick = 0; tick < soak_ticks; ++tick) {
        INFO("tick " << tick);
        application.advance(1.0 / 120, {}, camera);
        const auto &world = application.scene().world;
        REQUIRE_MESSAGE(!world.sweep_budget_exhausted(),
                        "Ordinary crowd contact exhausted the swept-event budget");
        // One assertion per tick instead of per body keeps the soak run as fast as before.
        const auto bodies = world.bodies();
        const bool finite = std::all_of(bodies.begin(), bodies.end(), [](const kinetra::Body &body) {
            return kinetra::finite(body.position) && kinetra::finite(body.velocity);
        });
        REQUIRE_MESSAGE(finite, "Crowd motion produced nonfinite state");
    }
}
} // namespace

TEST_CASE("a dense crowd with seed 123 keeps finite motion and its sweep budget for 60 seconds") {
    soak(123, sandbox::WanderMode::steering);
}

TEST_CASE("a dense crowd with seed 456 keeps finite motion and its sweep budget for 60 seconds") {
    soak(456, sandbox::WanderMode::steering);
}

TEST_CASE("a dense crowd of route wanderers keeps finite motion and its sweep budget for 60 seconds") {
    soak(123, sandbox::WanderMode::route);
}

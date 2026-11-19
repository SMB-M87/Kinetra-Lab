// Soak tests for dense Demo crowds: 60 simulated seconds per seed. These seeds stalled
// only after sustained crowd motion, beyond the old five-frame smoke checks, so the
// tests step simulated time rather than rendering speed. CTest labels them `soak`.
//
// Both seeds run with the default steering wander. One of them also runs with route
// wander, so the mode that the debug menu offers for comparison stays covered.
//
// The last two cases watch a minute of a small crowd around a standing player: the
// reproduction of agents that walked away and never came back.
#include "demo/sandbox_application.h"

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

/// Checks that the agents around a player who stands still are still there a minute
/// later. Before agents had a home range they walked out of the active area, fell asleep
/// there and left the surroundings of the player empty: 1 of 48 was still awake.
void check_agents_stay_around_a_standing_player(sandbox::WanderMode wander) {
    // One simulated second lets the first streaming pass populate and wake the area.
    constexpr int warm_up_ticks = 120;
    sandbox::SandboxApplication application(48, 42);
    application.tuning().wander.mode = wander;
    kinetra::Camera2D camera;
    camera.viewport = {1280, 800};
    camera.pixels_per_unit = 17;
    const auto active = [&] {
        const auto agents = application.scene().agents();
        return std::count_if(agents.begin(), agents.end(), [](const auto &agent) { return agent.active(); });
    };
    for (int tick = 0; tick < warm_up_ticks; ++tick)
        application.advance(1.0 / 120, {}, camera);
    const auto early = active();
    // A wanderer at 4 units per second leaves the active area well within the minute.
    for (int tick = warm_up_ticks; tick < soak_ticks; ++tick)
        application.advance(1.0 / 120, {}, camera);
    const auto late = active();
    INFO("active agents after 1 s: " << early << ", after 60 s: " << late);
    REQUIRE(early > 0);
    // Agents may drift in and out at the edge, but the area must not empty.
    CHECK(late * 5 >= early * 4);
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

TEST_CASE("agents stay around a standing player when they wander by steering") {
    check_agents_stay_around_a_standing_player(sandbox::WanderMode::steering);
}

TEST_CASE("agents stay around a standing player when they wander by route") {
    check_agents_stay_around_a_standing_player(sandbox::WanderMode::route);
}

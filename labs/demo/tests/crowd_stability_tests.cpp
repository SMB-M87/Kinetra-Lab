// Soak tests for Demo crowds. The dense runs cover 60 simulated seconds per seed. These seeds stalled
// only after sustained crowd motion, beyond the old five-frame smoke checks, so the
// tests step simulated time rather than rendering speed. CTest labels them `soak`.
//
// Both seeds run with the default steering wander. One of them also runs with route
// wander, so the mode that the debug menu offers for comparison stays covered.
//
// The last two cases follow a small crowd around a standing player for 100,000 ticks: the
// reproduction of agents that walked away and never came back.
#include "demo/application.h"
#include "kinetra_lab/debug.h"
#include "kinetra_lab/testing/test_support.h"

#include <doctest/doctest.h>

#include <algorithm>

namespace {
constexpr int ticks_per_second = static_cast<int>(demo::real_time_ups);
constexpr int dense_crowd_ticks = 60 * ticks_per_second;

/// Runs a 240-agent crowd that wanders by `wander` for `dense_crowd_ticks` ticks and checks
/// every tick that contact never exhausts the swept-event budget and that all motion
/// stays finite.
void soak(unsigned seed, demo::WanderMode wander) {
    demo::Application application(240, seed);
    application.tuning().wander.mode = wander;
    kinetra::Camera2D camera;
    camera.viewport = {1280, 800};
    camera.pixels_per_unit = 17;
    for (int tick = 0; tick < dense_crowd_ticks; ++tick) {
        INFO("tick " << tick);
        application.advance(demo::tick_seconds, {}, camera);
        const auto &world = application.scene().world();
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

/// Checks that agents remain around a standing player after a minute and at the end of
/// the sustained run. Before agents had a home range they walked out of the active area
/// and fell asleep there. Only 1 of 48 was still awake.
void check_agents_stay_around_a_standing_player(demo::WanderMode wander) {
    demo::Application application(48, 42);
    application.tuning().wander.mode = wander;
    kinetra::Camera2D camera;
    camera.viewport = {1280, 800};
    camera.pixels_per_unit = 17;
    const auto active = [&] {
        const auto agents = application.scene().agents();
        return std::count_if(agents.begin(), agents.end(), [](const auto &agent) { return agent.active(); });
    };
    demo::FrameCommands commands;
    commands.ups = kinetra_lab::fastest_ups;
    const double elapsed_per_tick = 1.0 / static_cast<double>(commands.ups);
    int tick = 0;
    const auto advance_to = [&](int checkpoint) {
        for (; tick < checkpoint; ++tick) {
            application.advance(elapsed_per_tick, commands, camera);
            REQUIRE(application.ticks_run() == 1);
        }
    };
    // One simulated second lets the first streaming pass populate and wake the area.
    advance_to(ticks_per_second);
    const auto early = active();
    REQUIRE(early > 0);
    for (const int checkpoint : {dense_crowd_ticks, kinetra_lab::testing::long_run_ticks}) {
        advance_to(checkpoint);
        const auto retained = active();
        INFO("tick " << tick << ", initial active agents: " << early << ", now: " << retained);
        // Allow movement across the active area's edge while requiring the crowd to stay nearby.
        CHECK(retained * 5 >= early * 4);
    }
}
} // namespace

TEST_CASE("a dense crowd with seed 123 keeps finite motion and its sweep budget for 60 seconds") {
    soak(123, demo::WanderMode::steering);
}

TEST_CASE("a dense crowd with seed 456 keeps finite motion and its sweep budget for 60 seconds") {
    soak(456, demo::WanderMode::steering);
}

TEST_CASE("a dense crowd of route wanderers keeps finite motion and its sweep budget for 60 seconds") {
    soak(123, demo::WanderMode::route);
}

TEST_CASE("agents stay around a standing player when they wander by steering") {
    check_agents_stay_around_a_standing_player(demo::WanderMode::steering);
}

TEST_CASE("agents stay around a standing player when they wander by route") {
    check_agents_stay_around_a_standing_player(demo::WanderMode::route);
}

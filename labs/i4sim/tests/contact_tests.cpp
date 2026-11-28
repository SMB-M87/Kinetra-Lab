// Long headless runs that check i4sim movers never touch: every bundled factory under the
// fullest motion configuration (Reciprocal, plain A*, recovery and every traffic policy), and
// every steering profile with its defaults. Each run lasts `long_run_ticks`, long enough for
// the rare situations that short runs miss: on 2026-11-28 a sweep of 300,000 ticks per run
// found contacts on 4320x2400 in nine of ten seeds and on test-environment in three, all
// gone with SAFETY STOP. A run also checks that production goes on, that SAFETY STOP stays
// an exception rather than the way movers stop, and that no mover stays stranded after
// MAKE WAY.
//
// Labelled soak: together they take about a minute in a Release build.
#include "i4sim/simulation/simulation.h"
#include "i4sim/testing/showcase.h"
#include "kinetra_lab/testing/test_support.h"

#include <doctest/doctest.h>

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

using namespace i4sim;

namespace {
using kinetra_lab::testing::long_run_ticks;

/// Reciprocal steering on plain A* routes, with recovery and every traffic policy: the
/// configuration with the most coordination at once.
MotionSettings fullest_motion() {
    MotionSettings motion;
    motion.profile = SteeringProfile::reciprocal;
    motion.path_planning = true;
    motion.heatmap = false;
    motion.recovery = motion.parking_traffic = motion.junction_traffic = motion.corridor_traffic = true;
    motion.predictive_passing = motion.make_way = motion.departure_check = motion.right_of_way = true;
    return motion;
}

std::filesystem::path blueprint_path(const std::string &name) {
    if (name == "test-environment")
        return test::fixture_blueprint(name);
    return default_blueprint_directory() / (name + ".json");
}

/// What a run shows; the checks read it after the run.
struct Outcome {
    std::uint64_t ticks{}, completed{}, collisions{}, contact_ticks{}, safety_stops{};
    /// Longest a mover stood still on its own under MAKE WAY after its trip ended, in ticks.
    std::uint64_t longest_stranded{};
};

Outcome run(const std::string &name, std::uint32_t seed, MotionSettings motion) {
    Simulation simulation(load_blueprint(blueprint_path(name)), seed);
    simulation.configure_motion(motion);
    simulation.record_events(true);
    simulation.run();
    const auto count = simulation.movers().size();
    std::vector<std::optional<std::uint64_t>> ended(count);
    Outcome outcome;
    std::size_t seen = 0;
    while (simulation.running() && simulation.ticks() < std::uint64_t(long_run_ticks)) {
        simulation.tick();
        const auto events = simulation.events();
        for (; seen < events.size(); ++seen)
            if (events[seen].kind == EventKind::make_way_ended)
                ended[events[seen].movers[1]] = events[seen].tick;
        for (std::size_t i = 0; i < count; ++i) {
            const auto &m = simulation.movers()[i];
            if (m.goal != GoalKind::making_way)
                ended[i].reset();
            else if (ended[i] && (m.moving() || m.held))
                ended[i] = simulation.ticks();
            else if (ended[i])
                outcome.longest_stranded = std::max(outcome.longest_stranded, simulation.ticks() - *ended[i]);
        }
    }
    outcome.ticks = simulation.ticks();
    outcome.completed = simulation.completed();
    outcome.collisions = simulation.collisions();
    outcome.contact_ticks = simulation.contact_ticks();
    outcome.safety_stops = simulation.safety_stops();
    return outcome;
}

/// Checks one run: the whole run without a contact, products completing, SAFETY STOP used less
/// than once per `ticks_per_stop` ticks, and no mover left standing under MAKE WAY much longer
/// than the 240 ticks after which it is given back.
void check(const Outcome &outcome, std::uint64_t least_completed, std::uint64_t ticks_per_stop) {
    CAPTURE(outcome.completed);
    CAPTURE(outcome.safety_stops);
    CAPTURE(outcome.longest_stranded);
    CHECK(outcome.ticks == std::uint64_t(long_run_ticks));
    CHECK(outcome.collisions == 0);
    CHECK(outcome.contact_ticks == 0);
    CHECK(outcome.completed >= least_completed);
    CHECK(outcome.safety_stops * ticks_per_stop <= outcome.ticks);
    CHECK(outcome.longest_stranded <= 241);
}
} // namespace

// The least completed products lie well below what the 2026-11-28 sweep measured over the
// first 100,000 ticks, and the SAFETY STOP ceilings well above the most it measured, so the
// checks catch a collapse rather than noise.

TEST_CASE("test-environment never touches under the fullest motion configuration") {
    for (const std::uint32_t seed : {42u, 915839214u}) {
        CAPTURE(seed);
        check(run("test-environment", seed, fullest_motion()), 20, 1000);
    }
}

TEST_CASE("1680x960+ never touches under the fullest motion configuration") {
    for (const std::uint32_t seed : {42u, 915839214u}) {
        CAPTURE(seed);
        check(run("1680x960+", seed, fullest_motion()), 25, 1000);
    }
}

TEST_CASE("3840x2400 never touches under the fullest motion configuration") {
    for (const std::uint32_t seed : {42u, 915839214u}) {
        CAPTURE(seed);
        check(run("3840x2400", seed, fullest_motion()), 90, 400);
    }
}

TEST_CASE("4320x2400 never touches under the fullest motion configuration") {
    for (const std::uint32_t seed : {42u, 915839214u}) {
        CAPTURE(seed);
        check(run("4320x2400", seed, fullest_motion()), 70, 100);
    }
}

TEST_CASE("8640x4800 never touches under the fullest motion configuration") {
    check(run("8640x4800", 42, fullest_motion()), 50, 50);
}

TEST_CASE("every steering profile with its defaults never touches") {
    // Improved's defaults touched 16 times in 20,000 ticks on test-environment, seed 42,
    // before SAFETY STOP; RVO experiment 24 times in 10,000 on 1680x960+, seed 7.
    for (const auto profile : {SteeringProfile::improved, SteeringProfile::original,
                               SteeringProfile::experimental_rvo, SteeringProfile::reciprocal})
        for (const std::string name : {"test-environment", "1680x960+"}) {
            const std::string profile_label = profile_name(profile);
            CAPTURE(profile_label);
            CAPTURE(name);
            MotionSettings motion;
            motion.profile = profile;
            const auto outcome = run(name, 7, motion);
            CAPTURE(outcome.safety_stops);
            CHECK(outcome.ticks == std::uint64_t(long_run_ticks));
            CHECK(outcome.collisions == 0);
            CHECK(outcome.contact_ticks == 0);
            CHECK(outcome.completed > 0);
        }
}

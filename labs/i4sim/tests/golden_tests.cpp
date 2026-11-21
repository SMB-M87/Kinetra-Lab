// Golden summaries that pin the observable i4sim behavior before it is restructured.
//
// Each scenario runs a seeded simulation for a fixed number of ticks and reduces it to
// one line: the totals the HUD and summary export show, plus a 64-bit fingerprint of
// the trajectory, meaning the exact position and velocity bits of every mover after
// every tick and the open products at the end. A behavior-preserving refactor leaves
// every line unchanged. A deliberate behavior change updates the expected lines in the
// same commit and says why in the commit message.
//
// Set KINETRA_PRINT_GOLDEN=1 to print the current line of every scenario, for a
// deliberate update of the expected values below. The run still compares, so a changed
// line also fails its case and shows expected and actual.
//
// The recorder and its floating-point caveats are in recorder.h, shared with the
// pressure scenarios.
#include "i4sim/simulation/simulation.h"
#include "i4sim/testing/recorder.h"
#include "i4sim/testing/showcase.h"

#include <doctest/doctest.h>

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>

namespace {
using namespace i4sim;

using i4sim::test::fixture_blueprint;
using i4sim::test::Recorder;
using i4sim::test::showcase_blueprint;

/// Motion settings with every coordination feature on, on the steering profile they target.
MotionSettings coordinated_motion() {
    MotionSettings motion;
    motion.profile = SteeringProfile::reciprocal;
    motion.recovery = motion.parking_traffic = motion.junction_traffic = motion.corridor_traffic = true;
    motion.predictive_passing = true;
    return motion;
}

MotionSettings profile_motion(SteeringProfile profile) {
    MotionSettings motion;
    motion.profile = profile;
    return motion;
}

/// Motion settings for one steering profile with the features `enable` switches on.
MotionSettings profile_motion(SteeringProfile profile, void (*enable)(MotionSettings &)) {
    auto motion = profile_motion(profile);
    enable(motion);
    return motion;
}

/// Path of `<name>.json` in the catalog copied beside the test executable.
std::filesystem::path catalog_blueprint(std::string_view name) {
    return default_blueprint_directory() / (std::string(name) + ".json");
}

/// Runs a factory blueprint with its automatic workload.
Simulation run_factory(Recorder &recorder, const std::filesystem::path &blueprint, std::uint32_t seed,
                       MotionSettings motion, std::uint64_t ticks) {
    Simulation simulation(load_blueprint(blueprint), seed);
    simulation.configure_motion(motion);
    simulation.run();
    recorder.run(simulation, ticks);
    return simulation;
}

/// Sends the two movers of a traffic fixture to manual goals, without workload.
Simulation run_pair(Recorder &recorder, std::string_view fixture, MotionSettings motion, Vec2 first_goal,
                    Vec2 second_goal, std::uint64_t ticks) {
    Simulation simulation(load_blueprint(showcase_blueprint(fixture)));
    simulation.set_workload(false);
    simulation.configure_motion(motion);
    REQUIRE_MESSAGE((simulation.move_to(0, first_goal) && simulation.move_to(1, second_goal)),
                    "Traffic fixture goals rejected");
    simulation.run();
    recorder.run(simulation, ticks);
    return simulation;
}

/// Lets two movers stall in a corridor without coordination, then enables corridor
/// traffic so recovery has to resolve the deadlock.
Simulation run_corridor_recovery(Recorder &recorder) {
    auto motion = profile_motion(SteeringProfile::reciprocal);
    auto simulation = run_pair(recorder, "corridor-recovery", motion, {1900, 700}, {300, 700}, 1500);
    simulation.pause();
    motion.corridor_traffic = true;
    simulation.configure_motion(motion);
    simulation.run();
    recorder.run(simulation, 6000);
    return simulation;
}

/// Compares the summary of a finished scenario with `expected`; a mismatch shows both lines.
void check_summary(std::string_view name, const Recorder &recorder, const Simulation &simulation,
                   const std::string &expected) {
    i4sim::test::check_line(name, recorder.summary(simulation), expected);
}
} // namespace

TEST_CASE("the factory with default motion reproduces its recorded summary") {
    Recorder recorder;
    const auto simulation =
        run_factory(recorder, fixture_blueprint("MAS-demonstrator"), 42, MotionSettings{}, 20000);
    check_summary(
        "factory default motion", recorder, simulation,
        "ticks=20000 completed=5 collisions=16 contact_ticks=16 interactions=33 distance_mm=27436.008 "
        "trajectory=8b06d3dac6e3297a");
}

TEST_CASE("the coordinated factory reproduces its recorded summary") {
    Recorder recorder;
    const auto simulation =
        run_factory(recorder, fixture_blueprint("MAS-demonstrator"), 42, coordinated_motion(), 20000);
    check_summary(
        "factory coordinated", recorder, simulation,
        "ticks=20000 completed=5 collisions=0 contact_ticks=0 interactions=33 distance_mm=28843.187 "
        "trajectory=0db5b6760196d5cd");
}

TEST_CASE("the coordinated large factory reproduces its recorded summary") {
    Recorder recorder;
    const auto simulation =
        run_factory(recorder, catalog_blueprint("3840x2400"), 915839214, coordinated_motion(), 12000);
    check_summary(
        "large factory coordinated", recorder, simulation,
        "ticks=12000 completed=8 collisions=0 contact_ticks=0 interactions=61 distance_mm=68484.373 "
        "trajectory=469ffc63a79206b5");
}

TEST_CASE("the original steering profile reproduces its recorded summary") {
    Recorder recorder;
    const auto simulation = run_factory(recorder, catalog_blueprint("1680x960+"), 7,
                                        profile_motion(SteeringProfile::original), 10000);
    check_summary("original profile", recorder, simulation,
                  "ticks=10000 completed=1 collisions=0 contact_ticks=0 interactions=13 distance_mm=8523.861 "
                  "trajectory=acc39b34b344dcc6");
}

TEST_CASE("the experimental RVO profile reproduces its recorded summary") {
    Recorder recorder;
    const auto simulation = run_factory(recorder, catalog_blueprint("1680x960+"), 7,
                                        profile_motion(SteeringProfile::experimental_rvo), 10000);
    check_summary(
        "experimental rvo profile", recorder, simulation,
        "ticks=10000 completed=1 collisions=24 contact_ticks=24 interactions=14 distance_mm=17582.890 "
        "trajectory=bc246a58724f4083");
}

TEST_CASE("the improved profile with recovery reproduces its recorded summary") {
    Recorder recorder;
    const auto simulation = run_factory(
        recorder, catalog_blueprint("1680x960+"), 7,
        profile_motion(SteeringProfile::improved, [](MotionSettings &m) { m.recovery = true; }), 10000);
    check_summary(
        "improved profile with recovery", recorder, simulation,
        "ticks=10000 completed=1 collisions=20 contact_ticks=22 interactions=13 distance_mm=9216.312 "
        "trajectory=98f18427ff86ace4");
}

TEST_CASE("the junction crossing reproduces its recorded summary") {
    Recorder recorder;
    const auto simulation = run_pair(
        recorder, "junction-crossing",
        profile_motion(SteeringProfile::reciprocal, [](MotionSettings &m) { m.junction_traffic = true; }),
        {1100, 700}, {700, 1100}, 3000);
    check_summary("junction crossing", recorder, simulation,
                  "ticks=3000 completed=0 collisions=0 contact_ticks=0 interactions=0 distance_mm=1604.742 "
                  "trajectory=ca08c2981c7b0de5");
}

TEST_CASE("corridor passing reproduces its recorded summary") {
    Recorder recorder;
    const auto simulation = run_pair(
        recorder, "corridor-passing",
        profile_motion(SteeringProfile::reciprocal, [](MotionSettings &m) { m.corridor_traffic = true; }),
        {1900, 700}, {300, 700}, 5000);
    check_summary("corridor passing", recorder, simulation,
                  "ticks=5000 completed=0 collisions=0 contact_ticks=0 interactions=0 distance_mm=3894.937 "
                  "trajectory=3c1878d51b3249aa");
}

TEST_CASE("corridor recovery reproduces its recorded summary") {
    Recorder recorder;
    const auto simulation = run_corridor_recovery(recorder);
    check_summary("corridor recovery", recorder, simulation,
                  "ticks=7500 completed=0 collisions=0 contact_ticks=0 interactions=0 distance_mm=3322.049 "
                  "trajectory=32a56480b9ed1ded");
}

TEST_CASE("predictive passing reproduces its recorded summary") {
    Recorder recorder;
    const auto simulation = run_pair(recorder, "predictive-passing",
                                     profile_motion(SteeringProfile::reciprocal,
                                                    [](MotionSettings &m) {
                                                        m.predictive_passing = true;
                                                        m.heatmap = false;
                                                    }),
                                     {1900, 800}, {500, 800}, 5000);
    check_summary("predictive passing", recorder, simulation,
                  "ticks=5000 completed=0 collisions=0 contact_ticks=0 interactions=0 distance_mm=2984.297 "
                  "trajectory=10d7c9a7f3702bc1");
}

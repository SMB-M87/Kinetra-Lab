// Predictive passing under pressure: recorded outcomes, the baseline for roadmap phase 7.
//
// Each case runs one blueprint fixture with manual goals under one steering profile,
// with predictive passing on and heatmap costs off. It first checks that the situation
// the scenario is about really occurs, and then compares one recorded line: contacts,
// passing plans and how they ended, arrivals, flow, and a fingerprint of trajectory and
// event log. The lines record today's behavior, including its failures; they are not
// requirements yet. Phase 7 turns them into required outcomes.
//
// KINETRA_PRINT_GOLDEN=1 prints the current lines, as in the golden suite.
#include "flow_metrics.h"
#include "recorder.h"
#include "simulation.h"

#include <doctest/doctest.h>

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <iomanip>
#include <limits>
#include <optional>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace {
using namespace i4sim;

using i4sim::test::Recorder;

/// A fixture with one manual goal per mover, run for a fixed number of ticks.
struct Scenario {
    std::string_view fixture;
    std::vector<Vec2> goals;
    std::uint64_t ticks;
};

/// Called after every tick, to observe whether the scenario's situation occurs.
using Watch = std::function<void(const Simulation &)>;

/// Counts the events of `kind`, and for `passing_ended` only those that ended with `end`.
std::size_t count(std::span<const SimulationEvent> events, EventKind kind,
                  std::optional<PassingEnd> end = std::nullopt) {
    return std::size_t(std::count_if(events.begin(), events.end(), [&](const SimulationEvent &event) {
        return event.kind == kind && (!end || event.end == end);
    }));
}

/// Reduces a finished run to its recorded line.
std::string outcome(const Recorder &recorder, const Simulation &simulation, const FlowMetrics &flow) {
    const auto events = simulation.events();
    auto fingerprint = recorder.trajectory();
    for (const auto &event : events) {
        fingerprint.add(event.tick);
        fingerprint.add(std::uint64_t(event.kind));
        fingerprint.add(event.plan);
        for (std::size_t side = 0; side < 2; ++side) {
            fingerprint.add(std::uint64_t{event.movers[side]});
            fingerprint.add(event.entry[side]);
            fingerprint.add(event.exit[side]);
        }
        fingerprint.add(event.end ? std::uint64_t(*event.end) : ~std::uint64_t{});
        fingerprint.add(std::uint64_t{event.border});
        fingerprint.add(std::uint64_t{event.passing});
    }
    std::size_t arrived = 0;
    for (const auto &mover : simulation.movers()) {
        fingerprint.add(std::uint64_t{mover.moving});
        arrived += !mover.moving && kinetra::distance(mover.body.position, mover.destination) < 0.1f;
    }
    const auto passing_contacts =
        std::count_if(events.begin(), events.end(), [](const SimulationEvent &event) {
            return event.kind == EventKind::contact && event.passing;
        });
    std::ostringstream line;
    line << "ticks=" << simulation.ticks() << " contacts=" << simulation.collisions()
         << " contact_ticks=" << simulation.contact_ticks() << " passing_contacts=" << passing_contacts
         << " plans=" << count(events, EventKind::passing_formed)
         << " crossings=" << count(events, EventKind::passing_crossing) << " ends=";
    bool any = false;
    for (auto end : {PassingEnd::completed, PassingEnd::disabled, PassingEnd::participant_stopped,
                     PassingEnd::participant_held, PassingEnd::traffic_control, PassingEnd::no_progress,
                     PassingEnd::goal_changed})
        if (const auto ended = count(events, EventKind::passing_ended, end)) {
            line << (any ? "," : "") << name(end) << ':' << ended;
            any = true;
        }
    if (!any)
        line << "none";
    line << " arrived=" << arrived << '/' << simulation.movers().size()
         << " waiting_ticks=" << flow.waiting_ticks() << " stuck=" << flow.stuck_movers()
         << " fingerprint=" << std::hex << std::setw(16) << std::setfill('0') << fingerprint.value();
    return line.str();
}

/// Runs `scenario` under `profile` and returns its recorded line; `watch` sees every tick.
std::string run(const Scenario &scenario, SteeringProfile profile, const Watch &watch) {
    Simulation simulation(load_blueprint(std::filesystem::path(__FILE__).parent_path() /
                                         (std::string(scenario.fixture) + ".json")));
    REQUIRE(simulation.movers().size() == scenario.goals.size());
    simulation.workload = false;
    MotionSettings motion;
    motion.profile = profile;
    motion.predictive_passing = true;
    motion.heatmap = false;
    simulation.configure_motion(motion);
    for (std::size_t mover = 0; mover < scenario.goals.size(); ++mover)
        REQUIRE_MESSAGE(simulation.move_to(mover, scenario.goals[mover]),
                        "Goal of mover " << mover << " rejected");
    simulation.record_events(true);
    simulation.paused = false;
    Recorder recorder;
    FlowMetrics flow;
    for (std::uint64_t tick = 0; tick < scenario.ticks; ++tick) {
        recorder.step(simulation);
        flow.observe(simulation);
        watch(simulation);
    }
    return outcome(recorder, simulation, flow);
}

/// True while movers 0 and 1 share a passing plan: they hold the only plan.
bool first_pair_passing(const Simulation &simulation) {
    return simulation.predictive_pairs() == 1 && simulation.passing_target(0) && simulation.passing_target(1);
}

/// Mover 0 meets movers 1 and 2, which drive in a column on its line toward it.
void check_three_oncoming(SteeringProfile profile, const std::string &expected) {
    bool paired = false;
    const auto line =
        run({"pressure-three-oncoming", {{3300, 800}, {300, 800}, {700, 800}}, 6000}, profile,
            [&](const Simulation &simulation) { paired = paired || first_pair_passing(simulation); });
    CHECK_MESSAGE(paired, "The lone mover never formed a plan with the first oncoming mover");
    i4sim::test::check_line("three oncoming", line, expected);
}

/// Movers 0 and 1 pass head-on while mover 2 crosses their path at right angles.
void check_lane_intrusion(SteeringProfile profile, const std::string &expected) {
    // Centers closer than 250 mm put the bodies (113 mm squares) within a body width of
    // each other, so the crossing mover is inside the passing lanes.
    float closest = std::numeric_limits<float>::infinity();
    const auto line =
        run({"pressure-lane-intrusion", {{1900, 800}, {500, 800}, {1300, 1500}}, 5000}, profile,
            [&](const Simulation &simulation) {
                if (!first_pair_passing(simulation))
                    return;
                const auto crossing = simulation.movers()[2].body.position;
                for (std::size_t mover = 0; mover < 2; ++mover)
                    closest = std::min(closest,
                                       kinetra::distance(crossing, simulation.movers()[mover].body.position));
            });
    CHECK_MESSAGE(closest < 250, "The crossing mover never came near an active passing pair");
    i4sim::test::check_line("lane intrusion", line, expected);
}

/// Two head-on pairs on parallel lines 400 mm apart.
void check_simultaneous(SteeringProfile profile, const std::string &expected) {
    std::size_t most = 0;
    const auto line =
        run({"pressure-simultaneous", {{1900, 600}, {500, 600}, {1900, 1000}, {500, 1000}}, 5000}, profile,
            [&](const Simulation &simulation) { most = std::max(most, simulation.predictive_pairs()); });
    CHECK_MESSAGE(most == 2, "The two pairs never held passing plans at the same time");
    i4sim::test::check_line("simultaneous", line, expected);
}

/// A short and a long mover meet in a vertical aisle two cells (400 mm) wide, between
/// y = 600 and y = 1600.
void check_narrow_aisle(SteeringProfile profile, const std::string &expected) {
    bool met = false;
    const auto line = run({"pressure-narrow-aisle", {{800, 1900}, {800, 300}}, 6000}, profile,
                          [&](const Simulation &simulation) {
                              const auto inside = [&](std::size_t mover) {
                                  const float y = simulation.movers()[mover].body.position.y;
                                  return y > 600 && y < 1600;
                              };
                              met = met || (inside(0) && inside(1));
                          });
    CHECK_MESSAGE(met, "The movers were never in the aisle at the same time");
    i4sim::test::check_line("narrow aisle", line, expected);
}
} // namespace

// Known failure: after the first pass, the lone mover and the second oncoming mover form
// no plan and shuffle in front of each other until the end; only the first oncoming
// mover arrives.
TEST_CASE("three oncoming movers under reciprocal steering keep their recorded outcome") {
    check_three_oncoming(
        SteeringProfile::reciprocal,
        "ticks=6000 contacts=0 contact_ticks=0 passing_contacts=0 plans=1 crossings=1 ends=completed:1 "
        "arrived=1/3 waiting_ticks=4 stuck=1 fingerprint=c35453d8b129aaa2");
}

TEST_CASE("three oncoming movers under improved steering keep their recorded outcome") {
    check_three_oncoming(
        SteeringProfile::improved,
        "ticks=6000 contacts=0 contact_ticks=0 passing_contacts=0 plans=1 crossings=1 ends=completed:1 "
        "arrived=3/3 waiting_ticks=30 stuck=0 fingerprint=916dbae033e3481b");
}

TEST_CASE("a mover crossing passing lanes under reciprocal steering keeps its recorded outcome") {
    check_lane_intrusion(
        SteeringProfile::reciprocal,
        "ticks=5000 contacts=0 contact_ticks=0 passing_contacts=0 plans=1 crossings=1 ends=completed:1 "
        "arrived=3/3 waiting_ticks=58 stuck=0 fingerprint=4e789d8607c8e81e");
}

TEST_CASE("a mover crossing passing lanes under improved steering keeps its recorded outcome") {
    check_lane_intrusion(
        SteeringProfile::improved,
        "ticks=5000 contacts=0 contact_ticks=0 passing_contacts=0 plans=1 crossings=1 ends=completed:1 "
        "arrived=3/3 waiting_ticks=116 stuck=0 fingerprint=07bb0939d96a3673");
}

TEST_CASE("simultaneous passing pairs under reciprocal steering keep their recorded outcome") {
    check_simultaneous(
        SteeringProfile::reciprocal,
        "ticks=5000 contacts=0 contact_ticks=0 passing_contacts=0 plans=2 crossings=2 ends=completed:2 "
        "arrived=4/4 waiting_ticks=8 stuck=0 fingerprint=62657db7b0850622");
}

TEST_CASE("simultaneous passing pairs under improved steering keep their recorded outcome") {
    check_simultaneous(
        SteeringProfile::improved,
        "ticks=5000 contacts=0 contact_ticks=0 passing_contacts=0 plans=2 crossings=2 ends=completed:2 "
        "arrived=4/4 waiting_ticks=8 stuck=0 fingerprint=43747191bd7ca665");
}

TEST_CASE("a narrow aisle under reciprocal steering keeps its recorded outcome") {
    check_narrow_aisle(
        SteeringProfile::reciprocal,
        "ticks=6000 contacts=0 contact_ticks=0 passing_contacts=0 plans=1 crossings=1 ends=completed:1 "
        "arrived=2/2 waiting_ticks=48 stuck=0 fingerprint=68f6981bfc4d901d");
}

TEST_CASE("a narrow aisle under improved steering keeps its recorded outcome") {
    check_narrow_aisle(
        SteeringProfile::improved,
        "ticks=6000 contacts=0 contact_ticks=0 passing_contacts=0 plans=1 crossings=1 ends=completed:1 "
        "arrived=2/2 waiting_ticks=48 stuck=0 fingerprint=3fbe98faca6ee9a6");
}

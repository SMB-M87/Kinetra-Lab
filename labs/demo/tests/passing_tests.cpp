// Demo agents passing under pressure, with PASSING on.
//
// Each case arranges agents on commanded travel in an open arena or a doorway and runs
// the scene tick by tick, under reciprocal avoidance and under legacy steering. It first
// checks that the situation the case is about occurs (a plan forms, ends as obstructed,
// and so on), then requires the outcome: every agent reaches its goal and no two moving
// bodies overlap by more than contact correction leaves. The cases assert outcomes, not a
// recorded fingerprint, so they hold on every toolchain (see the roadmap item *Golden
// tests on Windows*).
#include "demo/application.h"

#include <kinetra/simulation/physics.h>

#include <doctest/doctest.h>

#include <algorithm>
#include <cstddef>
#include <utility>
#include <vector>

namespace {
using kinetra::Body;
using kinetra::PassingEventKind;
using kinetra::PassingOutcome;
using kinetra::Vec2;

/// The Demo's fixed tick in seconds.
constexpr float tick = 1.f / 120;
/// Largest allowed overlap between two moving bodies, in world units: contact correction
/// leaves about 0.001, more means the bodies ran into each other.
constexpr float max_penetration = 0.01f;
/// Distance from its goal within which an agent has arrived; the agent's own threshold.
constexpr float arrival = 0.6f;

Body wall(Vec2 position, Vec2 half_extents) {
    Body body;
    body.shape = kinetra::Shape::box;
    body.motion = kinetra::Motion::fixed;
    body.position = position;
    body.half_extents = half_extents;
    return body;
}

Body mover(Vec2 position, float radius = 0.45f) {
    Body body;
    body.motion = kinetra::Motion::controlled;
    body.position = position;
    body.radius = radius;
    body.friction = 0;
    return body;
}

/// One trip: an agent's start and goal.
struct Trip {
    Vec2 from, to;
};

/// Outcome of a run: what passing did and whether every agent arrived without contact.
struct Run {
    std::size_t formed{}, crossings{}, completed{}, obstructed{}, other_ends{};
    /// Ticks in which some pair of moving bodies overlapped by more than max_penetration.
    std::size_t contact_ticks{};
    std::vector<bool> arrived;
    bool all_arrived() const {
        return std::ranges::all_of(arrived, [](bool a) { return a; });
    }
};

/// A Demo scene in the authored arena: the frame walls of the fixed scenarios, `inner`
/// walls inside it, the player in a corner, and one agent per trip with PASSING on.
struct Arena {
    demo::Scene scene{0, 7, demo::Scenario::maze};
    std::vector<Trip> trips;

    Arena(std::vector<Trip> agent_trips, bool reciprocal, std::vector<Body> inner = {})
        : trips(std::move(agent_trips)) {
        std::vector walls{wall({0, -14.5f}, {23, 0.5f}), wall({0, 14.5f}, {23, 0.5f}),
                          wall({-22.5f, 0}, {0.5f, 14}), wall({22.5f, 0}, {0.5f, 14})};
        walls.insert(walls.end(), inner.begin(), inner.end());
        auto player = mover({-20, -12}, 0.4f);
        std::vector movers{player};
        std::vector<std::size_t> agents;
        for (const auto &trip : trips) {
            agents.push_back(movers.size());
            movers.push_back(mover(trip.from));
        }
        scene.arrange(std::move(movers), 0, std::move(walls), {{-22, -14}, {22, 14}});
        scene.place_agents(agents, 7);
        scene.set_reciprocal_avoidance(reciprocal);
        scene.set_passing(true);
        for (std::size_t i = 0; i < trips.size(); ++i)
            scene.command_agent(i, trips[i].to);
    }

    /// Runs `ticks` ticks; `each` sees the scene and the run after every tick.
    template <typename Each> Run run(int ticks, Each each) {
        Run result;
        result.arrived.assign(trips.size(), false);
        for (int t = 0; t < ticks; ++t) {
            scene.update(tick, {});
            for (const auto &event : scene.passing_events()) {
                if (event.kind == PassingEventKind::formed)
                    ++result.formed;
                else if (event.kind == PassingEventKind::crossing)
                    ++result.crossings;
                else if (event.outcome == PassingOutcome::completed)
                    ++result.completed;
                else if (event.outcome == PassingOutcome::obstructed)
                    ++result.obstructed;
                else
                    ++result.other_ends;
            }
            const auto &world = scene.world();
            bool contact = false;
            for (std::size_t a = 0; a < scene.moving_body_count(); ++a)
                for (std::size_t b = a + 1; b < scene.moving_body_count(); ++b)
                    if (const auto hit = kinetra::collide(world.body(a), world.body(b));
                        hit && hit->penetration > max_penetration)
                        contact = true;
            result.contact_ticks += contact;
            for (std::size_t i = 0; i < trips.size(); ++i)
                if (kinetra::distance(world.body(scene.agents()[i].body_id()).position, trips[i].to) <
                    arrival)
                    result.arrived[i] = true;
            each(scene, result);
        }
        return result;
    }
    Run run(int ticks) {
        return run(ticks, [](const demo::Scene &, const Run &) {});
    }

    /// Runs ticks until a passing plan is active; the agents start too far apart for one.
    void form() {
        for (int t = 0; t < 1200 && scene.passing_plans().empty(); ++t)
            scene.update(tick, {});
        REQUIRE_FALSE(scene.passing_plans().empty());
    }
};

void report(const Run &run) {
    MESSAGE("formed=" << run.formed << " crossings=" << run.crossings << " completed=" << run.completed
                      << " obstructed=" << run.obstructed << " other=" << run.other_ends
                      << " contact_ticks=" << run.contact_ticks);
}
} // namespace

TEST_CASE("two agents head-on in the open pass in agreed lanes and both arrive without contact") {
    for (const bool reciprocal : {true, false}) {
        CAPTURE(reciprocal);
        Arena arena({{{-12, 0}, {12, 0}}, {{12, 0}, {-12, 0}}}, reciprocal);
        const auto run = arena.run(1800);
        report(run);
        CHECK(run.formed == 1);
        CHECK(run.completed == 1);
        CHECK(run.all_arrived());
        CHECK(run.contact_ticks == 0);
    }
}

TEST_CASE("one agent against two in a column plans to pass and every agent arrives without contact") {
    for (const bool reciprocal : {true, false}) {
        CAPTURE(reciprocal);
        Arena arena({{{-14, 0}, {14, 0}}, {{12, 0}, {-12, 0}}, {{15, 0}, {-15, 0}}}, reciprocal);
        const auto run = arena.run(2400);
        report(run);
        // The trailing agent of the column drives into the lane of the first plan, which
        // the guard may end as obstructed; the agents then pass by avoidance alone.
        CHECK(run.formed >= 1);
        CHECK(run.all_arrived());
        CHECK(run.contact_ticks == 0);
    }
}

TEST_CASE("a body that stops on a lane ends the plan as obstructed and both agents still arrive") {
    for (const bool reciprocal : {true, false}) {
        CAPTURE(reciprocal);
        Arena arena({{{-12, 0}, {12, 0}}, {{12, 0}, {-12, 0}}}, reciprocal);
        bool placed = false;
        const auto run = arena.run(2400, [&](demo::Scene &scene, const Run &) {
            const auto plans = scene.passing_plans();
            if (placed || plans.empty())
                return;
            // The player appears on the first participant's exit and stands there.
            auto &player = scene.moving_body(scene.player());
            player.position = plans.front().lanes[0].exit;
            player.velocity = {};
            placed = true;
        });
        report(run);
        CHECK(placed);
        CHECK(run.obstructed >= 1);
        CHECK(run.all_arrived());
        CHECK(run.contact_ticks == 0);
    }
}

TEST_CASE("two streams through a doorway plan to pass and every agent arrives without contact") {
    for (const bool reciprocal : {true, false}) {
        CAPTURE(reciprocal);
        std::vector<Trip> trips;
        for (const float side : {-1.f, 1.f})
            for (int row = 0; row < 3; ++row) {
                const Vec2 home{side * (9.f + 2.f * float(row)), 1.8f * float(row - 1)};
                trips.push_back({home, {-home.x, home.y}});
            }
        Arena arena(trips, reciprocal, {wall({0, -8.5f}, {0.7f, 5.5f}), wall({0, 8.5f}, {0.7f, 5.5f})});
        const auto run = arena.run(3600);
        report(run);
        // Pairs agree on lanes before the doorway; the other agents of the streams soon
        // stand or drive in them, so most plans end as obstructed.
        CHECK(run.formed >= 1);
        CHECK(run.all_arrived());
        CHECK(run.contact_ticks == 0);
    }
}

TEST_CASE("PASSING runs at the debug menu's extreme tuning") {
    // The fastest top speed with the weakest acceleration brakes over the longest distance,
    // which widens the coordinator's formation range most; every horizon the menu offers.
    for (const float horizon : {0.25f, 1.5f, 4.f}) {
        CAPTURE(horizon);
        Arena arena({{{-12, 0}, {12, 0}}, {{12, 0}, {-12, 0}}}, true);
        auto &motion = arena.scene.tuning().motion;
        motion.max_speed = 16;
        motion.max_acceleration = 1;
        motion.horizon = horizon;
        CHECK_NOTHROW(arena.run(600));
    }
}

TEST_CASE("a new goal, PASSING off and renumbered bodies end the plans") {
    {
        Arena arena({{{-12, 0}, {12, 0}}, {{12, 0}, {-12, 0}}}, true);
        arena.form();
        // A new goal for one participant cancels the plan; both still reach their goals.
        arena.trips[0].to = {-12, 8};
        arena.scene.command_agent(0, arena.trips[0].to);
        CHECK(arena.scene.passing_plans().empty());
        const auto run = arena.run(2400);
        CHECK(run.all_arrived());
        CHECK(run.contact_ticks == 0);
    }
    {
        Arena arena({{{-12, 0}, {12, 0}}, {{12, 0}, {-12, 0}}}, true);
        arena.form();
        arena.scene.set_passing(false);
        CHECK(arena.scene.passing_plans().empty());
        CHECK_FALSE(arena.scene.passing());
        const auto run = arena.run(2400);
        CHECK(run.formed == 0);
        CHECK(run.all_arrived());
    }
    {
        Arena arena({{{-12, 0}, {12, 0}}, {{12, 0}, {-12, 0}}}, true);
        arena.form();
        // New agents on the same bodies: the old plan's ids would name other agents.
        const std::vector<std::size_t> bodies{2, 1};
        arena.scene.place_agents(bodies, 7);
        CHECK(arena.scene.passing_plans().empty());
    }
}

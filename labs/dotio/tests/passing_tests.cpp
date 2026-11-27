// Dotio enemies passing under pressure, with PASSING on.
//
// Each case wakes path-wandering enemies on a grid of walls with an open room or corridor
// carved into it, gives each a route to its goal and runs the world update by update, as
// the application does. It first checks that the situation the case is about occurs (a
// plan forms, ends as obstructed, or cannot form), then requires the outcome: every enemy
// reaches its goal, and where a plan holds, no two enemies touch. The cases assert outcomes, not a
// recorded fingerprint, so they hold on every toolchain.
#include "dotio/world.h"

#include <doctest/doctest.h>

#include <algorithm>
#include <cstddef>
#include <memory>
#include <vector>

namespace {
using kinetra::PassingEventKind;
using kinetra::PassingOutcome;
using kinetra::Vec2;

/// Center of a cell in legacy pixels.
Vec2 cell_center(int row, int column) {
    return {(float(column) + 0.5f) * dotio::Grid::cell_size, (float(row) + 0.5f) * dotio::Grid::cell_size};
}

/// A route enemy's distance from its goal, in legacy pixels, within which it has arrived:
/// a path wanderer passes a waypoint within two thirds of a cell and sways around it, so it
/// may never come closer.
constexpr float arrival = 70;

/// One trip of an enemy: start and goal cells.
struct Trip {
    int row, column, goal_row, goal_column;
};

/// What passing did, and whether every enemy arrived.
struct Run {
    std::size_t formed{}, crossings{}, completed{}, obstructed{}, other_ends{};
    /// Updates after which two of the enemies touched. Contact resolution separates bodies
    /// within the update, so enemies that ran into each other end it touching.
    std::size_t contact_updates{};
    std::vector<bool> arrived;
    bool all_arrived() const {
        return std::ranges::all_of(arrived, [](bool a) { return a; });
    }
};

/// A Medium world whose grid is all wall except the cells a case opens, with the player
/// sealed in a cell far away, so the enemies see no one and keep wandering along their
/// routes. Enemy `i` takes pool slot `i`.
struct Arena {
    std::unique_ptr<dotio::World> world = std::make_unique<dotio::World>();
    std::vector<Trip> trips;

    Arena(std::vector<Trip> enemy_trips, bool passing) : trips(std::move(enemy_trips)) {
        world->initialize(42);
        world->grid().fill(1);
        world->grid().set_cell(150, 150, 0);
        world->get_player().position = cell_center(150, 150);
        world->enemy_tuning().wander = dotio::WanderMode::path;
        world->set_passing(passing);
    }

    /// Opens the cells from (row, column) through (last_row, last_column).
    void carve(int row, int column, int last_row, int last_column) {
        for (int y = row; y <= last_row; ++y)
            for (int x = column; x <= last_column; ++x)
                world->grid().set_cell(y, x, 0);
    }

    /// Wakes the enemies at their starts, at rest, each with a route to its goal.
    void wake() {
        for (std::size_t i = 0; i < trips.size(); ++i) {
            auto &enemy = world->get_enemies()[i];
            enemy.position = cell_center(trips[i].row, trips[i].column);
            enemy.velocity = {};
            enemy.is_active = true;
            enemy.frozen = false;
            enemy.path = dotio::plan_route(world->grid(), enemy.position,
                                           cell_center(trips[i].goal_row, trips[i].goal_column));
            REQUIRE_FALSE(enemy.path.empty());
        }
    }

    /// Runs `updates` 60 Hz updates, with the 20 Hz decision tick before every third one;
    /// `each` sees the world after every update.
    template <typename Each> Run run(int updates, Each each) {
        Run result;
        result.arrived.assign(trips.size(), false);
        for (int update = 0; update < updates; ++update) {
            if (update % 3 == 2)
                for (auto &enemy : world->get_enemies())
                    if (enemy.is_active && !enemy.frozen)
                        enemy.tick();
            world->update_enemies(1, false);
            for (const auto &event : world->passing_events()) {
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
            bool contact = false;
            for (std::size_t a = 0; a < trips.size(); ++a) {
                const auto &enemy = world->get_enemies()[a];
                if (kinetra::distance(enemy.position, cell_center(trips[a].goal_row, trips[a].goal_column)) <
                    arrival)
                    result.arrived[a] = true;
                for (std::size_t b = a + 1; b < trips.size(); ++b) {
                    const auto &other = world->get_enemies()[b];
                    if (kinetra::distance(enemy.position, other.position) <
                        enemy.radius + other.radius + 0.5f)
                        contact = true;
                }
            }
            result.contact_updates += contact;
            each(*world, result);
        }
        return result;
    }
    Run run(int updates) {
        return run(updates, [](dotio::World &, const Run &) {});
    }
};

void report(const Run &run) {
    MESSAGE("formed=" << run.formed << " crossings=" << run.crossings << " completed=" << run.completed
                      << " obstructed=" << run.obstructed << " other=" << run.other_ends
                      << " contact_updates=" << run.contact_updates);
}
} // namespace

TEST_CASE("two enemies head-on in an open room pass in agreed lanes and both arrive without contact") {
    Arena arena({{23, 14, 23, 46}, {23, 46, 23, 14}}, true);
    arena.carve(20, 10, 26, 50);
    arena.wake();
    const auto run = arena.run(2400);
    report(run);
    CHECK(run.formed == 1);
    CHECK(run.completed == 1);
    CHECK(run.all_arrived());
    CHECK(run.contact_updates == 0);
}

TEST_CASE("without passing the same two enemies run into each other") {
    Arena arena({{23, 14, 23, 46}, {23, 46, 23, 14}}, false);
    arena.carve(20, 10, 26, 50);
    arena.wake();
    const auto run = arena.run(2400);
    report(run);
    CHECK(run.formed == 0);
    CHECK(run.contact_updates > 0);
}

TEST_CASE("one enemy against two in a column plans to pass and every enemy arrives") {
    Arena arena({{23, 14, 23, 46}, {23, 44, 23, 12}, {23, 47, 23, 15}}, true);
    arena.carve(20, 10, 26, 50);
    arena.wake();
    const auto run = arena.run(2400);
    report(run);
    // The trailing enemy of the column drives into the lane of the first plan, which the
    // guard may end as obstructed; the enemies then pass by contact alone.
    CHECK(run.formed >= 1);
    CHECK(run.all_arrived());
}

TEST_CASE("a frozen enemy on a lane ends the plan as obstructed and both enemies still arrive") {
    Arena arena({{23, 14, 23, 46}, {23, 46, 23, 14}}, true);
    arena.carve(20, 10, 26, 50);
    arena.wake();
    bool placed = false;
    const auto run = arena.run(2400, [&](dotio::World &world, const Run &) {
        const auto plans = world.passing_plans();
        if (placed || plans.empty())
            return;
        // A third enemy stands on the first participant's exit, frozen as outside the
        // activity window.
        auto &parked = world.get_enemies()[2];
        parked.position = plans.front().lanes[0].exit;
        parked.velocity = {};
        parked.is_active = true;
        parked.frozen = true;
        placed = true;
    });
    report(run);
    CHECK(placed);
    CHECK(run.obstructed >= 1);
    CHECK(run.all_arrived());
}

TEST_CASE("in a corridor one cell wide no plan fits and both enemies still get through") {
    Arena arena({{23, 14, 23, 46}, {23, 46, 23, 14}}, true);
    arena.carve(23, 10, 23, 50);
    arena.wake();
    const auto run = arena.run(2400);
    report(run);
    CHECK(run.formed == 0);
    CHECK(run.all_arrived());
}

TEST_CASE("PASSING runs at the debug menu's extreme movement tuning") {
    // The slowest and fastest MOVE SPEED, with every route force at its smallest step and at
    // its largest value: the speed and acceleration the coordinator is given span their range.
    for (const float movement : {0.25f, dotio::max_movement_multiplier})
        for (const float force : {0.05f, dotio::max_state_force}) {
            CAPTURE(movement);
            CAPTURE(force);
            Arena arena({{23, 14, 23, 46}, {23, 46, 23, 14}}, true);
            arena.carve(20, 10, 26, 50);
            arena.wake();
            arena.world->settings().movement_multiplier = movement;
            auto &tuning = arena.world->enemy_tuning();
            tuning.wander_force = tuning.pursue_force = tuning.recover_force = force;
            CHECK_NOTHROW(arena.run(600));
        }
}

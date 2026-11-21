// Headless tests for Dotio: legacy math, arena generation and streaming, geometry and
// routes, fuzzy AI states, combat, spawning, difficulty and the application commands.
//
// Every world owns its grid. Cases that build on the plain bordered arena call open_map()
// on their world's grid; fixture() starts on one.
#include "dotio/ai/fuzzy.h"
#include "dotio/ai/wander.h"
#include "dotio/application.h"

#include <doctest/doctest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <memory>
#include <numbers>
#include <random>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {
/// Turns `grid` into an empty arena with a one-cell wall along its border.
void open_map(dotio::Grid &grid) {
    for (int row = 0; row < dotio::Grid::Rows; ++row)
        for (int column = 0; column < dotio::Grid::Columns; ++column)
            grid.SetCell(row, column,
                         row == 0 || column == 0 || row == dotio::Grid::Rows - 1 ||
                             column == dotio::Grid::Columns - 1);
}

/// A small easy world on the current grid: player and Ghost at (550, 550) without
/// cooldown, and one wandering enemy at (850, 550) aimed at the player.
std::unique_ptr<dotio::World> fixture() {
    auto world = std::make_unique<dotio::World>(dotio::Difficulty::easy);
    open_map(world->grid());
    world->SetPlayer(dotio::Player(world.get(), {550, 550}, 0));
    world->SetGhost(dotio::Player(world.get(), {550, 550}, 0));
    world->GetPlayer().Cooldown = 0;
    world->AddEnemy({850, 550}, {550, 550}, 8, true);
    world->GetEnemies()[0].Initialize();
    return world;
}

/// Streams a seeded world with enemies around its player for `duration` seconds in steps
/// of `dt`, checks spawn placement, spacing and admission, and returns the positions of
/// the active enemies. `active` is the normal player view in world units.
std::vector<dotio::math::Vec2> population(float dt, float resources, dotio::math::Vec2 active = {1120, 700},
                                          dotio::Difficulty difficulty = dotio::Difficulty::easy,
                                          float duration = 20) {
    CAPTURE(dt);
    CAPTURE(duration);
    auto world = std::make_unique<dotio::World>();
    world->Initialize(42, difficulty);
    auto &player = world->GetPlayer();
    const auto center = player.Position;
    const dotio::math::Vec2 retained{8000, 8000};
    world->StreamEnemies(center, active / 1.75f, active * (9.f / 1.75f), retained);
    open_map(world->grid());
    player.Health = player.Mana = resources;
    for (int frame = 0; float(frame) < duration / dt; ++frame)
        world->StreamEnemies(center, active / 1.75f, active * (9.f / 1.75f), retained, dt);
    std::vector<dotio::math::Vec2> positions;
    for (const auto &enemy : world->GetEnemies())
        if (enemy.IsActive) {
            const auto delta = enemy.Position - center;
            CHECK_MESSAGE((std::abs(delta.X) - enemy.Radius > active.X / 1.75f * 1.1f ||
                           std::abs(delta.Y) - enemy.Radius > active.Y / 1.75f * 1.1f),
                          "Spawned enemy body overlaps 1.1x maximum normal player view");
            for (const auto position : positions)
                CHECK_MESSAGE((enemy.Position - position).GetLength() >=
                                  dotio::settings_for(difficulty).spawn_spacing,
                              "Spawn spacing allowed a dense enemy cluster");
            positions.push_back(enemy.Position);
        }
    int local_count = 0;
    bool expanded = false;
    const auto spawn_half = world->SpawnHalf(active / 1.75f, active * (9.f / 1.75f));
    for (auto position : positions) {
        const auto delta = position - center;
        const bool local = std::abs(delta.X) <= active.X && std::abs(delta.Y) <= active.Y;
        local_count += local;
        expanded = expanded || !local;
        CHECK_MESSAGE((std::abs(delta.X) <= spawn_half.X && std::abs(delta.Y) <= spawn_half.Y),
                      "Spawn escaped its time-dependent placement rectangle");
    }
    CHECK_MESSAGE((!positions.empty() && local_count <= dotio::settings_for(difficulty).active_spawn_limit),
                  "Spawn admission did not bound the local population");
    if (duration >= 120)
        CHECK_MESSAGE(expanded, "Survival did not place enemies beyond the initial spawn range");
    return positions;
}

/// Observable behavior of one enemy attacking the player for one second at a difficulty.
struct Encounter {
    int shots;
    float speed, acceleration, cooldown;
    bool pursuing;
};

/// Runs one second of an attacking enemy 700 units from the player on the open arena.
Encounter encounter(dotio::Difficulty difficulty) {
    auto world = std::make_unique<dotio::World>();
    world->Initialize(42, difficulty);
    open_map(world->grid());
    auto &enemy = world->GetEnemies()[0];
    enemy.Position = world->GetPlayer().Position + dotio::math::Vec2{700, 0};
    enemy.Target = world->GetPlayer().Position;
    enemy.Fear = 9;
    enemy.IsActive = true;
    enemy.Frozen = false;
    enemy.SetState(enemy.GetAttackState());
    enemy.ResetCooldown();
    const float cooldown = enemy.GetCooldown();
    enemy.GetState().Execute(1);
    const float acceleration = enemy.Velocity.GetLength();
    for (int tick = 0; tick < 60; ++tick)
        enemy.Tick();
    int shots = 0;
    float speed = 0;
    for (const auto &arrow : world->GetArrows())
        if (arrow.IsActive) {
            ++shots;
            speed = arrow.Velocity.GetLength();
        }
    enemy.ResetPursue();
    for (int cycle = 0; cycle < 10; ++cycle)
        enemy.PursueCountdown();
    return Encounter{shots, speed, acceleration, cooldown, !enemy.PursueCountdownPassed()};
}

/// Fills the global grid with walls; carve(world->grid(), ) then opens the floor a case needs.
void walled_map(dotio::Grid &grid) {
    grid.Fill(1);
}

/// Every cell of `grid`, row by row.
std::vector<int> cells_of(const dotio::Grid &grid) {
    std::vector<int> cells;
    for (int row = 0; row < dotio::Grid::Rows; ++row)
        for (int column = 0; column < dotio::Grid::Columns; ++column)
            cells.push_back(grid.Cell(row, column));
    return cells;
}

/// Opens the cells from (row, column) through (last_row, last_column), both included.
void carve(dotio::Grid &grid, int row, int column, int last_row, int last_column) {
    for (int y = row; y <= last_row; ++y)
        for (int x = column; x <= last_column; ++x)
            grid.SetCell(y, x, 0);
}

/// Center of a cell in legacy coordinates.
dotio::math::Vec2 cell_center(int row, int column) {
    return {(float(column) + 0.5f) * dotio::Grid::CellSize, (float(row) + 0.5f) * dotio::Grid::CellSize};
}

/// A seeded Medium world on a grid of walls: the player stands in a sealed cell, so it sees
/// no enemy and every enemy keeps wandering. The pool is dormant; a case carves its floor
/// and wakes the enemies it needs with wanderer().
std::unique_ptr<dotio::World> walled_world(std::uint32_t seed = 42) {
    auto world = std::make_unique<dotio::World>();
    world->Initialize(seed);
    walled_map(world->grid());
    carve(world->grid(), 150, 150, 150, 150);
    world->GetPlayer().Position = cell_center(150, 150);
    return world;
}

/// Wakes pool slot `index` as a wandering enemy at rest at `position`, facing `heading`
/// radians in legacy coordinates.
dotio::Enemy &wanderer(dotio::World &world, dotio::math::Vec2 position, float heading, int index = 0) {
    auto &enemy = world.GetEnemies()[index];
    enemy.Position = position;
    enemy.Velocity = {};
    enemy.IsActive = true;
    enemy.Frozen = false;
    enemy.WanderHeading = heading;
    return enemy;
}

/// Runs `updates` 60 Hz updates the way the application does: before every third one the
/// active enemies get their 20 Hz decision tick. `first` continues an earlier run.
void run(dotio::World &world, int updates, int first = 0) {
    for (int update = first; update < first + updates; ++update) {
        if (update % 3 == 2)
            for (auto &enemy : world.GetEnemies())
                if (enemy.IsActive && !enemy.Frozen)
                    enemy.Tick();
        world.UpdateEnemies(1, false);
    }
}

/// Speed a wandering enemy settles at, in legacy pixels per update: every update it gains
/// its force times the movement multiplier and then loses a fifth of its speed.
float wander_speed(const dotio::World &world) {
    return 4 * world.enemy_tuning().wander_force * world.difficulty_settings().movement_multiplier;
}

/// Speed an enemy at rest gains from one update of `state`, in legacy pixels per update.
/// The enemy gets a waypoint far along its row and sight of the player, so every state
/// that needs a path or a target has one.
float acceleration(dotio::Enemy &enemy, dotio::State &state) {
    enemy.Velocity = {};
    enemy.Path = dotio::Route({cell_center(5, 60)});
    enemy.LineOfSight = true;
    enemy.SetState(state);
    state.Execute(1);
    return enemy.Velocity.GetLength();
}

/// Distance the fixture's enemy covers in one second on the open arena at a movement
/// multiplier, wandering east by steering or pursuing along a path to the east.
float travelled(float movement_multiplier, bool pursuing) {
    auto world = fixture();
    world->settings().movement_multiplier = movement_multiplier;
    auto &enemy = world->GetEnemies()[0];
    const auto start = enemy.Position;
    enemy.WanderHeading = 0;
    if (pursuing) {
        enemy.SetState(enemy.GetPursueState());
        enemy.Path = dotio::Route({cell_center(5, 60)});
    }
    for (int update = 0; update < 60; ++update)
        world->UpdateEnemies(1, false);
    return (enemy.Position - start).GetLength();
}
} // namespace

TEST_CASE("incremental residency matches the terrain of a full rebuild and keeps the planner in step") {
    auto world = std::make_unique<dotio::World>();
    world->Initialize(42);
    for (int step = 0; step < 12; ++step) {
        if (step == 8)
            world->Initialize(43);
        const dotio::math::Position center{7050.f + float(step * 100), 7950.f + float((step % 3) * 100)};
        const float half = step % 4 == 0 ? 1200.f : 2400.f;
        world->StreamEnemies(center, {100, 100}, {500, 500}, {half, half});
        for (int row = 0; row < dotio::Grid::Rows; ++row)
            for (int col = 0; col < dotio::Grid::Columns; ++col) {
                const bool inside = row >= int(std::floor((center.Y - half) / 100)) &&
                                    row <= int(std::floor((center.Y + half) / 100)) &&
                                    col >= int(std::floor((center.X - half) / 100)) &&
                                    col <= int(std::floor((center.X + half) / 100));
                CHECK_MESSAGE(world->grid().Cell(row, col) == (inside ? world->TerrainCell(row, col) : 1),
                              "Incremental residency changed terrain occupancy");
            }
        const auto &planner = world->grid().Navigation();
        for (int row = 0; row < dotio::Grid::Rows; ++row)
            for (int col = 0; col < dotio::Grid::Columns; ++col)
                REQUIRE_MESSAGE(planner.blocked(row * dotio::Grid::Columns + col) ==
                                    (world->grid().Cell(row, col) != 0),
                                "The route planner disagrees with the raster after residency changed");
        if (step == 5) {
            world->GetPlayer().Position = {11250, 11250};
            world->StreamAroundPlayer();
        }
    }
}

TEST_CASE("projectile segments and line of sight stop at walls and routes and shelters avoid them") {
    auto world = fixture();
    dotio::math::Point2 hit{};
    CHECK_MESSAGE(
        ((dotio::Grid::SegementCircleIntersection({{550, 300}, {550, 800}}, {550, 550}, 20, hit) & 1) &&
         std::abs(hit.Y - 530) < 0.01f),
        "Vertical projectile failed continuous collision");
    CHECK_MESSAGE(
        !(dotio::Grid::SegementCircleIntersection({{500, 300}, {500, 800}}, {550, 550}, 20, hit) & 1),
        "Projectile hit a circle outside its segment");
    CHECK_MESSAGE(
        (dotio::Grid::SegementCircleIntersection({{550, 550}, {550, 550}}, {550, 550}, 20, hit) & 1),
        "Point segment inside circle must hit");
    world->grid().SetCell(7, 5, 1);
    CHECK_MESSAGE(
        (world->grid().IsIntersectingAnyCell({{550, 550}, {550, 900}}, hit) && std::abs(hit.Y - 700) < 0.01f),
        "Vertical line of sight must stop at wall");
    CHECK_MESSAGE(!world->grid().IsIntersectingAnyCell({{450, 550}, {450, 900}}, hit),
                  "Clear vertical sight falsely blocked");
    CHECK_MESSAGE(world->grid().IsIntersectingAnyCell({{550, 550}, {-50, 550}}, hit),
                  "Finite world boundary must block projectiles");
    const auto route = dotio::plan_route(world->grid(), cell_center(5, 5), cell_center(9, 5));
    REQUIRE_MESSAGE(!route.empty(), "No route around the wall");
    CHECK_MESSAGE(route.destination() == cell_center(9, 5), "The route does not end at its goal");
    auto from = cell_center(5, 5);
    for (const auto waypoint : route.remaining()) {
        CHECK_MESSAGE(!world->grid().IsIntersectingAnyCell({from, waypoint}, hit),
                      "A route leg crosses a wall");
        from = waypoint;
    }
    const auto shelter = dotio::plan_shelter(world->grid(), cell_center(5, 5), {550, 950}, 2);
    REQUIRE_MESSAGE(!shelter.empty(), "No shelter behind the wall");
    CHECK_MESSAGE(world->grid().IsIntersectingAnyCell({shelter.destination(), {550, 950}}, hit),
                  "The shelter is in the player's sight");
    world->grid().SetCell(7, 5, 0);
}

TEST_CASE("fuzzy rules move an enemy between wander attack kamikaze recovery and pursuit") {
    CHECK_MESSAGE(dotio::Fuzzy::Evaluate(400, 3, 4), "Original close/warrior/midway fuzzy rule missing");
    CHECK_MESSAGE((!dotio::Fuzzy::Evaluate(2000, 3, 4) && !dotio::Fuzzy::Evaluate(400, 9, 4)),
                  "Fuzzy kamikaze trigger ignored distance/fear");
    auto world = fixture();
    auto &enemy = world->GetEnemies()[0];
    CHECK_MESSAGE(enemy.IsWandering(), "Enemy must spawn wandering");
    enemy.Tick();
    CHECK_MESSAGE(enemy.IsAttacking(), "Visible player must trigger attack");
    enemy.Fear = 3;
    for (int i = 0; i < 3; ++i)
        enemy.AmmoFired();
    for (int health : {3, 2, 0}) {
        enemy.HitsRemaining = health;
        enemy.GetAttackState().Update();
        CHECK_MESSAGE(enemy.IsAttacking(), "Fuzzy rule triggered kamikaze without exactly one health");
    }
    enemy.HitsRemaining = 1;
    enemy.Tick();
    CHECK_MESSAGE(enemy.IsKamikazing(), "Fuzzy rule must trigger kamikaze at one health");
    enemy.Reset();
    enemy.Fear = 9;
    enemy.SetState(enemy.GetAttackState());
    for (int i = 0; i < 10; ++i)
        enemy.AmmoFired();
    enemy.Tick();
    CHECK_MESSAGE(enemy.IsRecovering(), "Empty ammo must trigger recovery");
    enemy.SetState(enemy.GetAttackState());
    enemy.Reset();
    enemy.Fear = 9;
    enemy.SetState(enemy.GetAttackState());
    world->grid().SetCell(5, 7, 1);
    enemy.Tick();
    CHECK_MESSAGE(enemy.IsPursueing(), "Losing sight must trigger pursue");
    for (int i = 0; i < 11; ++i)
        enemy.PursueCountdown();
    enemy.Tick();
    CHECK_MESSAGE(enemy.IsWandering(), "Expired pursuit must resume wander");
    world->grid().SetCell(5, 7, 0);
    enemy.Reset();
    enemy.SetState(enemy.GetRecoverState());
    enemy.Tick();
    CHECK_MESSAGE(enemy.IsPursueing(), "Reloaded recovery must return to pursuit");
}

TEST_CASE("a failed escape triggers kamikaze only at exactly one health") {
    auto world = fixture();
    open_map(world->grid());
    auto &enemy = world->GetEnemies()[0];
    enemy.LineOfSight = true;
    enemy.Fear = 9;
    for (int i = 0; i < 10; ++i)
        enemy.AmmoFired();
    CHECK_MESSAGE(dotio::plan_shelter(world->grid(), cell_center(5, 8), enemy.Target, enemy.Fear).empty(),
                  "Recovery health fixture must have no shelter");
    for (int health : {3, 2, 0, 1}) {
        enemy.HitsRemaining = health;
        enemy.Path.clear();
        enemy.SetState(enemy.GetRecoverState());
        enemy.GetRecoverState().Update();
        CHECK_MESSAGE((health == 1 ? enemy.IsKamikazing() : enemy.IsRecovering()),
                      "Failed escape must only trigger kamikaze at exactly one health");
    }
}

TEST_CASE("combat rewards hits behind no wall and reset restores resources and AI state") {
    auto world = fixture();
    auto &player = world->GetPlayer();
    auto &enemy = world->GetEnemies()[0];
    player.Shoot({850, 550}, false);
    CHECK_MESSAGE((player.Mana == 90 && player.Cooldown > 0), "Shooting must spend mana and set cooldown");
    for (auto &bolt : world->GetBolts())
        bolt.IsActive = false;
    player.Health = 50;
    world->AddBolt({550, 550}, {600, 0});
    world->UpdateBolts(1);
    CHECK_MESSAGE((!enemy.IsActive && player.Kills == 2 && player.Health == 60), "Bolt kill/reward missing");
    enemy.Reset();
    world->grid().SetCell(5, 7, 1);
    world->AddBolt({550, 550}, {600, 0});
    world->UpdateBolts(1);
    CHECK_MESSAGE(enemy.IsActive, "Bullet killed enemy behind wall");
    world->grid().SetCell(5, 7, 0);
    world->AddArrow({550, 350}, {0, 400});
    world->UpdateArrows(1, false);
    CHECK_MESSAGE(player.Health == 50, "Enemy arrow did not damage player");
    world->AddArrow({550, 350}, {0, 400});
    world->UpdateArrows(1, true);
    CHECK_MESSAGE(player.Health == 50, "Debug mode must prevent projectile damage");
    world->AddBolt({550, 550}, {0, 5});
    world->Reset();
    CHECK_MESSAGE((player.Health == 100 && player.Mana == 100 && player.Kills == 1 && enemy.IsWandering()),
                  "Reset did not restore game resources and AI state");
    for (auto &bolt : world->GetBolts())
        CHECK_MESSAGE(!bolt.IsActive, "Reset retained live bolts");
    enemy.IsActive = true;
    enemy.Frozen = false;
    enemy.Position = player.Position + dotio::math::Vec2{10, 0};
    enemy.Target = player.Position;
    player.Mana = 50;
    enemy.Tick();
    CHECK_MESSAGE(player.Mana >= 70, "Contact kill must restore mana");
    CHECK_MESSAGE((!enemy.IsActive && enemy.Frozen && enemy.Path.empty()),
                  "Contact kill bypassed the timed offscreen spawn controller");
    enemy.IsActive = true;
    enemy.Frozen = false;
    enemy.Position = player.Position + dotio::math::Vec2{10, 0};
    enemy.Target = player.Position;
    enemy.SetState(enemy.GetKamikazeState());
    enemy.Debug = true;
    enemy.Tick();
    CHECK_MESSAGE(player.Health == 100, "Debug mode must protect against kamikaze contact");
}

TEST_CASE("the generated arena is reproducible per seed and its floor is connected") {
    dotio::Grid grid;
    grid.Generate(42);
    const auto first = cells_of(grid);
    grid.Generate(42);
    std::size_t index = 0;
    for (int row = 0; row < dotio::Grid::Rows; ++row)
        for (int col = 0; col < dotio::Grid::Columns; ++col) {
            CHECK_MESSAGE(grid.Cell(row, col) == first[index++], "Arena seed is not reproducible");
            CHECK_MESSAGE(grid.Cell(row, col) == grid.TerrainCell(42, row, col),
                          "Streamed window differs from absolute terrain");
        }
    // Flood fill verifies every free cell belongs to the playable region.
    std::vector<std::pair<int, int>> queue{{2, 2}};
    std::vector<bool> visited(dotio::Grid::Rows * dotio::Grid::Columns);
    visited[2 * dotio::Grid::Columns + 2] = true;
    for (std::size_t i = 0; i < queue.size(); ++i) {
        const auto [row, col] = queue[i];
        for (const auto &[dy, dx] : {std::pair{1, 0}, {-1, 0}, {0, 1}, {0, -1}}) {
            const int y = row + dy, x = col + dx;
            if (y < 0 || x < 0 || y >= dotio::Grid::Rows || x >= dotio::Grid::Columns)
                continue;
            const int id = y * dotio::Grid::Columns + x;
            if (!visited[id] && !grid.Cell(y, x)) {
                visited[id] = true;
                queue.emplace_back(y, x);
            }
        }
    }
    bool changed = false;
    for (int row = 0; row < dotio::Grid::Rows; ++row)
        for (int col = 0; col < dotio::Grid::Columns; ++col)
            CHECK_MESSAGE((row >= 128 || col >= 128 || grid.Cell(row, col) ||
                           visited[row * dotio::Grid::Columns + col]),
                          "Disconnected arena floor");
    grid.Generate(43);
    changed = cells_of(grid) != first;
    CHECK_MESSAGE(changed, "Different arena seeds produced identical geometry");
}

namespace {
/// Plays `seconds` of a Medium game from `seed` with a scripted player that walks in a
/// turning circle and shoots ahead, and returns a hash of the player's and every enemy's
/// position, state and resources, sampled every tick.
std::uint64_t scripted_run(std::uint32_t seed, int seconds) {
    auto application = std::make_unique<dotio::Application>(seed);
    kinetra::Camera2D camera;
    camera.viewport = {1280, 720};
    camera.pixels_per_unit = dotio::initial_zoom;
    std::uint64_t hash = 1469598103934665603ull;
    const auto mix = [&hash](double value) {
        hash = (hash ^ std::uint64_t(std::int64_t(std::llround(value * 64)))) * 1099511628211ull;
    };
    for (int tick = 0; tick < seconds * 60; ++tick) {
        const float angle = float(tick) * 0.01f;
        dotio::Commands commands;
        commands.direction = {std::cos(angle), std::sin(angle)};
        commands.shoot = tick % 30 < 15;
        commands.aim = camera.position + kinetra::Vec2{std::cos(angle * 3), std::sin(angle * 3)} * 10.f;
        application->advance(1.0 / 60, commands, camera);
        auto &world = application->world();
        const auto &player = world.GetPlayer();
        mix(player.Position.X);
        mix(player.Position.Y);
        mix(player.Health);
        mix(player.Kills);
        mix(double(world.ResetCount()));
        for (auto &enemy : world.GetEnemies()) {
            if (!enemy.IsActive)
                continue;
            mix(enemy.Position.X);
            mix(enemy.Position.Y);
            mix(enemy.Fear);
            mix(enemy.IsAttacking() + 2 * enemy.IsPursueing() + 4 * enemy.IsRecovering() +
                8 * enemy.IsKamikazing());
        }
    }
    return hash;
}
} // namespace

TEST_CASE("one seed and one command sequence give the same run and another seed differs") {
    const auto first = scripted_run(42, 30);
    CHECK_MESSAGE(first == scripted_run(42, 30), "The same seed and commands gave two different runs");
    CHECK_MESSAGE(first != scripted_run(43, 30), "Two seeds gave the same run");
}

namespace {
/// A random free cell of `world`'s grid at least three cells inside the window and in the
/// connected part of the generated floor (rows and columns below 128), drawn from `random`.
dotio::math::Vec2 free_cell(dotio::World &world, std::mt19937 &random) {
    for (;;) {
        const int row = 3 + int(random() % 124), column = 3 + int(random() % 124);
        if (!world.grid().Cell(row, column))
            return cell_center(row, column);
    }
}

/// Whether every leg of `route`, starting at `from`, stays off the walls of `grid`.
bool clear_of_walls(const dotio::Grid &grid, dotio::math::Vec2 from, const dotio::Route &route) {
    for (const auto waypoint : route.remaining()) {
        dotio::math::Point2 hit{};
        if (grid.IsIntersectingAnyCell({from, waypoint}, hit))
            return false;
        from = waypoint;
    }
    return true;
}
} // namespace

TEST_CASE("routes between free cells of generated mazes exist and stay clear of walls") {
    for (const std::uint32_t seed : {1u, 7u, 42u}) {
        CAPTURE(seed);
        auto world = std::make_unique<dotio::World>();
        world->Initialize(seed);
        std::mt19937 random(seed);
        for (int pair = 0; pair < 40; ++pair) {
            const auto from = free_cell(*world, random), to = free_cell(*world, random);
            const auto route = dotio::plan_route(world->grid(), from, to);
            REQUIRE_MESSAGE(!route.empty(), "No route between two cells of the connected floor");
            CHECK_MESSAGE(route.destination() == to, "A route ends away from its goal");
            CHECK_MESSAGE(clear_of_walls(world->grid(), from, route), "A route leg crosses a wall");
        }
    }
}

TEST_CASE("a pursuer far away in a generated maze reaches a hidden player along one route") {
    for (const std::uint32_t seed : {1u, 7u, 42u}) {
        CAPTURE(seed);
        auto world = std::make_unique<dotio::World>();
        world->Initialize(seed);
        world->settings().pursuit_cycles = 1000; // The pursuit must not give up first.
        auto &player = world->GetPlayer();
        std::mt19937 random(seed);
        // An enemy out of sight whose route to the player is longer than the 50 cells the
        // original path storage held.
        dotio::math::Vec2 start{};
        std::size_t length = 0;
        for (int attempt = 0; attempt < 400 && length <= 50; ++attempt) {
            start = free_cell(*world, random);
            dotio::math::Point2 hit{};
            if (world->grid().IsIntersectingAnyCell({start, player.Position}, hit))
                length = dotio::plan_route(world->grid(), start, player.Position).size();
        }
        REQUIRE_MESSAGE(length > 50, "The maze has no hidden cell more than 50 cells from the player");
        CAPTURE(length);
        auto &enemy = world->GetEnemies()[0];
        enemy.Position = start;
        enemy.Velocity = {};
        enemy.IsActive = true;
        enemy.Frozen = false;
        enemy.SetState(enemy.GetPursueState());
        enemy.ResetPursue();
        int updates = 0, plans = 0;
        std::size_t remaining = 0;
        for (; updates < 120 * 60 && enemy.IsPursueing(); ++updates) {
            run(*world, 1, updates);
            // A new route is longer than what was left of the old one.
            plans += enemy.Path.size() > remaining;
            remaining = enemy.Path.size();
        }
        CAPTURE(updates);
        CHECK_MESSAGE(enemy.IsAttacking(), "The pursuer did not get the player in sight");
        CHECK_MESSAGE(plans <= 2, "A pursuit of a standing player replanned its route");
    }
}

TEST_CASE("no enemy stands still in a moving state around a standing player for two seconds") {
    for (const auto difficulty : {dotio::Difficulty::easy, dotio::Difficulty::hard}) {
        auto application = std::make_unique<dotio::Application>(5, difficulty);
        application->debug = application->invulnerable = true; // The player stays put.
        kinetra::Camera2D camera;
        camera.viewport = {1280, 720};
        camera.pixels_per_unit = dotio::initial_zoom;
        auto &enemies = application->world().GetEnemies();
        struct Anchor {
            dotio::math::Vec2 position;
            int since{};
        };
        std::vector<Anchor> anchors(enemies.GetCapacity());
        int moving = 0;
        for (int tick = 0; tick < 90 * 60; ++tick) {
            application->advance(1.0 / 60, {}, camera);
            std::size_t index = 0;
            for (auto &enemy : enemies) {
                auto &anchor = anchors[index++];
                // Attacking enemies hold their distance, and a recovering one reloads at its
                // shelter: both stand still on purpose.
                const bool holds = enemy.IsAttacking() || (enemy.IsRecovering() && enemy.Path.empty());
                if (!enemy.IsActive || enemy.Frozen || holds ||
                    (enemy.Position - anchor.position).GetLength() > 25) {
                    anchor = {enemy.Position, tick};
                    continue;
                }
                ++moving;
                CHECK_MESSAGE(tick - anchor.since < 120,
                              "An enemy in a moving state stood still for two seconds");
            }
        }
        CHECK_MESSAGE(moving > 0, "No enemy was simulated");
    }
}

// Pins a whole seeded game, so a refactor that changes any enemy decision, route or random
// draw shows here. When a deliberate behavior change moves it, record the new value and say
// why in the commit. Floating-point results differ between compilers, so CTest skips this
// case on Windows (CMakePresets.json).
TEST_CASE("a seeded game reproduces its recorded fingerprint") {
    CHECK(scripted_run(42, 30) == 0x9d126f5dc5068339ull);
}

TEST_CASE("two worlds keep their own grids") {
    auto first = std::make_unique<dotio::World>();
    auto second = std::make_unique<dotio::World>();
    first->Initialize(42);
    second->Initialize(43);
    const auto before = second->grid().Cell(80, 80);
    bool different = false;
    for (int row = 0; row < dotio::Grid::Rows; ++row)
        for (int column = 0; column < dotio::Grid::Columns; ++column)
            different |= first->grid().Cell(row, column) != second->grid().Cell(row, column);
    CHECK_MESSAGE(different, "Two seeds gave two worlds the same grid");
    first->grid().SetCell(80, 80, 1 - first->grid().Cell(80, 80));
    first->Initialize(44);
    CHECK_MESSAGE(second->grid().Cell(80, 80) == before, "One world changed the grid of another");
    for (int row = 0; row < dotio::Grid::Rows; ++row)
        for (int column = 0; column < dotio::Grid::Columns; ++column)
            REQUIRE(second->grid().Cell(row, column) == second->TerrainCell(row, column));
}

TEST_CASE("crates interact with actors and streaming keeps geometry continuous across rebases") {
    dotio::World world;
    world.Initialize(42);
    const auto initial_offset = world.WorldOffset();
    auto &player = world.GetPlayer();
    const auto first = cells_of(world.grid());
    REQUIRE_MESSAGE(!world.GetObjects().empty(), "No physics objects generated");
    const auto crate = world.GetObjects().front().body;
    for (auto &enemy : world.GetEnemies())
        enemy.IsActive = false;
    player.Position = {crate.position.x - crate.radius - player.Radius + 2, crate.position.y};
    player.Velocity = {4, 0};
    world.UpdateObjects(1);
    CHECK_MESSAGE(world.GetObjects().front().body.velocity.x > 0, "Player cannot push crate");
    // Projectiles hit the crate before an actor behind it and transfer momentum.
    auto &enemy = world.GetEnemies()[0];
    enemy.IsActive = true;
    enemy.Position = {crate.position.x + 60, crate.position.y};
    world.AddBolt({crate.position.x - 80, crate.position.y}, {160, 0});
    world.UpdateBolts(1);
    CHECK_MESSAGE((enemy.IsActive && !world.GetBolts()[0].IsActive), "Crate failed to intercept projectile");
    enemy.IsActive = false;
    player.Position = {crate.position.x + 60, crate.position.y};
    player.Health = 100;
    world.AddArrow({crate.position.x - 80, crate.position.y}, {160, 0});
    world.UpdateArrows(1, false);
    CHECK_MESSAGE((player.Health == 100 && !world.GetArrows()[0].IsActive), "Crate failed to shield player");
    const auto moving = world.GetObjects().front().body.position;
    player.Position = {8050, 8050};
    player.Velocity = {};
    world.UpdateObjects(1);
    CHECK_MESSAGE(world.GetObjects().front().body.position.x > moving.x, "Released crate did not move");
    // Cross both axes repeatedly, including negative world cells, and verify geometry continuity.
    for (const float coordinate : {11250.f, 11250.f, 3250.f, 3250.f, 3250.f, 3250.f}) {
        player.Position = {coordinate, coordinate};
        const auto absolute = player.Position + world.WorldOffset();
        const auto old_position = player.Position;
        const auto old_ghost = world.GetGhost().Position;
        const auto shift = world.StreamAroundPlayer();
        CHECK_MESSAGE(shift != dotio::math::Vec2{}, "Streaming window did not advance");
        CHECK_MESSAGE(player.Position + world.WorldOffset() == absolute,
                      "Streaming changed absolute player position");
        CHECK_MESSAGE(world.GetGhost().Position == old_ghost - shift, "Streaming failed to rebase camera");
        CHECK_MESSAGE(player.Position != old_position, "Local origin was not rebased");
        for (const auto &resident : world.GetEnemies())
            CHECK_MESSAGE(resident.Path.empty(), "Streaming retained stale navigation path");
        for (int row = 0; row < dotio::Grid::Rows; ++row)
            for (int col = 0; col < dotio::Grid::Columns; ++col)
                CHECK_MESSAGE(world.grid().Cell(row, col) == world.TerrainCell(row, col),
                              "Streamed collision and terrain geometry diverged");
    }
    CHECK_MESSAGE(world.WorldOffset().X < initial_offset.X, "Negative world exploration did not stream");
    player.Position = dotio::math::Vec2{8050, 8050} - world.WorldOffset();
    world.StreamAroundPlayer();
    CHECK_MESSAGE(world.WorldOffset() == initial_offset, "Return trip did not recover original window");
    CHECK_MESSAGE(cells_of(world.grid()) == first, "Revisited world geometry changed");
    int solid = 0;
    for (int cell : first)
        solid += cell != 0;
    CHECK_MESSAGE((solid > int(first.size() / 4) && solid < int(first.size() * 3 / 4)),
                  "Generated world lacks shelter walls");
    // Every district edge offers several shared openings instead of one repeated exit.
    for (int district = -2; district <= 2; ++district) {
        int horizontal = 0, vertical = 0;
        for (int cell = 0; cell < 32; ++cell) {
            horizontal += !world.grid().TerrainCell(42, district * 32 - 1, cell) &&
                          !world.grid().TerrainCell(42, district * 32, cell);
            vertical += !world.grid().TerrainCell(42, cell, district * 32 - 1) &&
                        !world.grid().TerrainCell(42, cell, district * 32);
        }
        CHECK_MESSAGE((horizontal >= 4 && vertical >= 4), "District edge lacks alternate exits");
    }
    for (int i = 0; i < 64; ++i) {
        const auto cell = world.grid().GenerateGridEntry({150, 150}, world.behavior_random());
        CHECK_MESSAGE((cell.Row >= 0 && cell.Row < dotio::Grid::Rows && cell.Column >= 0 &&
                       cell.Column < dotio::Grid::Columns && !world.grid().Cell(cell.Row, cell.Column)),
                      "Edge wander target escaped streamed raster");
    }
    open_map(world.grid());
}

TEST_CASE("bolts hit enemies within their combined radii including moving targets") {
    {
        auto matching = fixture();
        auto &target = matching->GetEnemies()[0];
        target.Position = {575.0001f, 550};
        matching->UpdateEnemies(0, false);
        target.Position += dotio::math::Vec2{599.9998f, 0};
        matching->AddBolt({550, 550}, {600, 0});
        matching->UpdateBolts(1);
        CHECK_MESSAGE(!target.IsActive, "Co-moving bullet tunneled through enemy at tiny relative speed");
    }
    auto world = fixture();
    auto &enemy = world->GetEnemies()[0];
    enemy.Position = {850, 574};
    world->AddBolt({550, 550}, {600, 0});
    world->UpdateBolts(1);
    CHECK_MESSAGE(!enemy.IsActive, "Visible bullet edge passed through enemy");
    enemy.Reset();
    enemy.Position = {850, 576};
    world->AddBolt({550, 550}, {600, 0});
    world->UpdateBolts(1);
    CHECK_MESSAGE(enemy.IsActive, "Bullet outside combined radii caused a hit");
    enemy.Position = {850, 450};
    world->UpdateEnemies(0, false); // Snapshot start poses for this physics step.
    enemy.Position = {850, 650};
    world->AddBolt({550, 550}, {600, 0});
    world->UpdateBolts(1);
    CHECK_MESSAGE(!enemy.IsActive, "Enemy crossing bullet trajectory was missed");
}

TEST_CASE("the debug menu toggles a row once per click at every size") {
    kinetra::DebugMenu menu;
    bool enabled = false;
    const std::vector<kinetra::DebugEntry> entries{kinetra::debug_check("TEST", enabled, 0)};
    for (const auto viewport : {kinetra::Vec2{1280, 800}, kinetra::Vec2{200, 150}}) {
        kinetra::Input input;
        menu.visible = true;
        const float scale = menu.scale(viewport);
        input.mouse =
            menu.origin(viewport) +
            kinetra::Vec2{kinetra::DebugMenu::label_left(0), kinetra::DebugMenu::row_top(0) + 6} * scale;
        input.clicked = true;
        const bool before = enabled;
        CHECK_MESSAGE((menu.update(input, viewport, entries) && enabled != before),
                      "Debug menu resize/picking failed");
        input.clicked = false;
        input.mouse_down = true;
        CHECK_MESSAGE((menu.update(input, viewport, entries) && enabled != before),
                      "Held click toggled option repeatedly");
    }
}

TEST_CASE("debug teleport accepts only free targets in range and keeps absolute coordinates") {
    dotio::Application application(42);
    kinetra::Camera2D camera;
    auto &player = application.world().GetPlayer();
    for (auto &enemy : application.world().GetEnemies())
        enemy.IsActive = false;
    open_map(application.world().grid());
    const auto original = player.Position;
    dotio::Commands commands;
    commands.teleport = true;
    commands.teleport_target = dotio::to_world({8150, 8050});
    application.advance(0, commands, camera);
    CHECK_MESSAGE(player.Position == original, "Teleport accepted outside debug mode");
    open_map(application.world().grid());
    application.debug = true;
    application.ghost = true;
    application.world().GetGhost().Position = player.Position;
    application.paused = true;
    player.Velocity = {4, 5};
    application.advance(0, commands, camera);
    CHECK_MESSAGE(
        (player.Position == dotio::math::Vec2{8150, 8050} && player.Velocity == dotio::math::Vec2{}),
        "Paused debug teleport failed");
    application.world().grid().SetCell(80, 82, 1);
    commands.teleport_target = dotio::to_world({8190, 8050});
    application.advance(0, commands, camera);
    CHECK_MESSAGE((player.Position == dotio::math::Vec2{8150, 8050}),
                  "Teleport accepted circle overlapping wall");
    commands.teleport_target = dotio::to_world({100000000, 550});
    application.advance(0, commands, camera);
    CHECK_MESSAGE((player.Position == dotio::math::Vec2{8150, 8050}),
                  "Teleport accepted outside supported coordinate range");
    // A distant Ghost click can cross the current local raster while paused.
    bool teleported = false;
    const auto offset = application.world().WorldOffset();
    for (int row = 75; row < 85 && !teleported; ++row)
        for (int col = -20; col < -1 && !teleported; ++col) {
            const dotio::math::Vec2 target{(float(col) + 0.5f) * 100, (float(row) + 0.5f) * 100};
            if (!application.teleport_clear(target, player.Radius))
                continue;
            commands.teleport_target = dotio::to_world(target);
            application.world().GetGhost().Position = target;
            application.advance(0, commands, camera);
            CHECK_MESSAGE(player.Position + application.world().WorldOffset() == target + offset,
                          "Distant paused teleport lost absolute coordinates");
            teleported = true;
        }
    CHECK_MESSAGE(teleported, "No free distant Ghost destination found");
    open_map(application.world().grid());
}

namespace {
/// The labels of the application's debug menu as it stands, in display order.
std::vector<std::string> menu_labels(dotio::Application &application) {
    std::vector<std::string> labels;
    for (const auto &row : application.debug_entries())
        labels.push_back(row.label);
    return labels;
}

/// The row `label` of the application's debug menu as it stands.
kinetra::DebugEntry menu_row(dotio::Application &application, std::string_view label) {
    for (auto &row : application.debug_entries())
        if (row.label == label)
            return row;
    FAIL("The debug menu has no row " << label);
    return {};
}
} // namespace

TEST_CASE("the debug menu starts with the simulation section and ends with zoom ghost camera and reset") {
    dotio::Application application(42);
    CHECK(menu_labels(application) ==
          std::vector<std::string>{"SIMULATION", "RUNNING", "SINGLE TICK", "SPEED", "INVULNERABLE", "LAYERS",
                                   "ENEMIES", "SPAWNING", "ZOOM", "GHOST CAMERA", "RESET PARAMETERS"});
    const auto ghost = menu_row(application, "GHOST CAMERA");
    CHECK((ghost.kind == kinetra::DebugEntryKind::check && ghost.depth == 0 && !ghost.active()));
    ghost.action();
    CHECK(application.ghost);
    CHECK_MESSAGE(menu_row(application, "ZOOM").rule_above, "a rule sets the commands apart");
    open_map(application.world().grid());
}

TEST_CASE("the layers section lists paths with unchecked parts in their state colors while it is on") {
    dotio::Application application(42);
    menu_row(application, "LAYERS").action();
    CHECK(menu_labels(application) ==
          std::vector<std::string>{"SIMULATION", "LAYERS", "VECTORS", "STEERING PROBES", "PATHS",
                                   "COLLISION SHAPES", "ACTIVE AGENTS", "AI STATES", "HIDING HITS", "ENEMIES",
                                   "SPAWNING", "ZOOM", "GHOST CAMERA", "RESET PARAMETERS"});
    menu_row(application, "PATHS").action();
    REQUIRE(application.paths[0]);
    CHECK_MESSAGE(
        (!application.paths[1] && !application.paths[2] && !application.paths[3] && !application.paths[4]),
        "switching PATHS on switched its parts on");
    const auto labels = menu_labels(application);
    CHECK(std::vector<std::string>(labels.begin() + 5, labels.begin() + 9) ==
          std::vector<std::string>{"WANDER", "PURSUE", "KAMIKAZE", "RECOVERY"});
    const auto pursue = menu_row(application, "PURSUE");
    CHECK((pursue.kind == kinetra::DebugEntryKind::check && pursue.depth == 2));
    REQUIRE(pursue.swatch);
    CHECK((pursue.swatch->r == dotio::state_color::pursue.r &&
           pursue.swatch->g == dotio::state_color::pursue.g));
    pursue.action();
    CHECK(application.paths[2]);
    menu_row(application, "HIDING HITS").action();
    CHECK(application.paths[5]);
    open_map(application.world().grid());
}

TEST_CASE("the enemies and spawning sections change the world's balance within their limits") {
    dotio::Application application(42);
    auto &world = application.world();
    const auto preset = world.difficulty_settings();
    menu_row(application, "ENEMIES").action();
    CHECK(menu_labels(application) ==
          std::vector<std::string>{"SIMULATION", "LAYERS", "ENEMIES", "WANDER", "MOVE SPEED", "FIRE INTERVAL",
                                   "ARROW SPEED", "PREDICTION", "PURSUIT CYCLES", "HITS", "WANDER FORCE",
                                   "ATTACK FORCE", "PURSUE FORCE", "RECOVER FORCE", "KAMIKAZE FORCE",
                                   "SPAWNING", "ZOOM", "GHOST CAMERA", "RESET PARAMETERS"});
    CHECK(menu_row(application, "WANDER").value() == "STEERING");
    menu_row(application, "WANDER").action();
    menu_row(application, "PATH").action();
    CHECK(world.enemy_tuning().wander == dotio::WanderMode::path);
    CHECK(menu_row(application, "WANDER").value() == "PATH");
    CHECK_MESSAGE(menu_labels(application).size() == 19, "choosing a value closes the list");

    menu_row(application, "MOVE SPEED").increase();
    CHECK(world.difficulty_settings().movement_multiplier ==
          doctest::Approx(preset.movement_multiplier * 1.25f));
    menu_row(application, "HITS").increase();
    CHECK(world.difficulty_settings().enemy_hits == preset.enemy_hits + 1);
    menu_row(application, "KAMIKAZE FORCE").decrease();
    CHECK(world.enemy_tuning().kamikaze_force == doctest::Approx(1.25f));
    // Every row stops at a value the world can run with, however often it is clicked.
    for (const char *label :
         {"MOVE SPEED", "FIRE INTERVAL", "ARROW SPEED", "PREDICTION", "PURSUIT CYCLES", "HITS",
          "WANDER FORCE", "ATTACK FORCE", "PURSUE FORCE", "RECOVER FORCE", "KAMIKAZE FORCE"})
        for (const bool up : {true, false}) {
            for (int click = 0; click < 200; ++click)
                up ? menu_row(application, label).increase() : menu_row(application, label).decrease();
            CAPTURE(label);
            CAPTURE(up);
            CHECK_NOTHROW(dotio::validate(world.difficulty_settings()));
            CHECK_NOTHROW(dotio::validate(world.enemy_tuning()));
        }
    menu_row(application, "SPAWNING").action();
    for (const char *label : {"ACTIVE LIMIT", "SPAWN RATE", "CROWD LOAD", "SPACING"})
        for (const bool up : {true, false}) {
            for (int click = 0; click < 200; ++click)
                up ? menu_row(application, label).increase() : menu_row(application, label).decrease();
            CAPTURE(label);
            CHECK_NOTHROW(dotio::validate(world.difficulty_settings()));
        }
    CHECK(world.difficulty_settings().active_spawn_limit == dotio::tuning_step::smallest_spawn_limit);

    application.time_scale = 4;
    menu_row(application, "RESET PARAMETERS").action();
    CHECK(world.difficulty_settings() == preset);
    CHECK(world.enemy_tuning() == dotio::EnemyTuning{});
    CHECK(application.time_scale == 1);
    open_map(application.world().grid());
}

TEST_CASE("the time scale and a single tick decide how many updates a frame runs") {
    dotio::Application application(42);
    for (auto &enemy : application.world().GetEnemies())
        enemy.IsActive = false;
    open_map(application.world().grid());
    kinetra::Camera2D camera;
    auto &player = application.world().GetPlayer();
    dotio::Commands right;
    right.direction = {1, 0};
    const auto origin = player.Position;
    // How far the player gets in a fifth of a second of frames at the given time scale. It
    // starts from the same open spot every time and stays within its cell, so neither a wall
    // nor the streaming of the world around the player can come in between.
    const auto travelled = [&](float scale) {
        open_map(application.world().grid());
        application.time_scale = scale;
        player.Position = origin;
        player.Velocity = {};
        for (int frame = 0; frame < 12; ++frame)
            application.advance(1.0 / 60, right, camera);
        return player.Position.X - origin.X;
    };
    const float normal = travelled(1);
    REQUIRE(normal > 0);
    CHECK_MESSAGE(travelled(0.5f) < normal * 0.75f, "half speed did not slow the world down");
    CHECK_MESSAGE(travelled(2) > normal * 1.5f, "double speed did not speed the world up");

    open_map(application.world().grid());
    player.Position = origin;
    application.time_scale = 1;
    application.paused = true;
    player.Velocity = {};
    const auto standing = player.Position;
    application.advance(1.0, right, camera);
    CHECK_MESSAGE(player.Position == standing, "a paused world moved");
    application.paused = false;
    menu_row(application, "SINGLE TICK").action();
    CHECK_MESSAGE(application.paused, "a single tick pauses a running world");
    application.advance(1.0, right, camera);
    const auto after_tick = player.Position;
    CHECK_MESSAGE(after_tick != standing, "the single tick did not run");
    application.advance(1.0, right, camera);
    CHECK_MESSAGE(player.Position == after_tick, "more than one tick ran");
    CHECK_FALSE(menu_row(application, "RUNNING").active());
    menu_row(application, "RUNNING").action();
    CHECK_FALSE(application.paused);
    open_map(application.world().grid());
}

TEST_CASE("the zoom row zooms within the limits of the current mode") {
    dotio::Application application(42);
    kinetra::Camera2D camera;
    camera.pixels_per_unit = dotio::initial_zoom;
    application.debug = true;
    application.advance(0, {}, camera);
    CHECK(menu_row(application, "ZOOM").value() == "25");
    menu_row(application, "ZOOM").decrease();
    application.advance(0, {}, camera);
    CHECK(camera.pixels_per_unit == doctest::Approx(25 / kinetra::zoom_factor));
    application.debug = false;
    application.advance(0, {}, camera);
    CHECK_MESSAGE(camera.pixels_per_unit == dotio::zoom_limits.minimum, "leaving debug mode clamps the zoom");
    open_map(application.world().grid());
}

TEST_CASE("the debug camera detaches from the player while WASD keeps moving the player") {
    dotio::Application application(42);
    for (auto &enemy : application.world().GetEnemies())
        enemy.IsActive = false;
    kinetra::Camera2D camera;
    auto &player = application.world().GetPlayer();
    const auto start = player.Position;
    dotio::Commands commands;
    commands.camera_pan = {1, 0};
    application.advance(1.0 / 60, commands, camera);
    CHECK_MESSAGE((player.Position == start && !application.ghost), "Arrows moved player outside debug");
    application.debug = true;
    application.ghost = true;
    application.world().GetGhost().Position = player.Position;
    application.paused = true;
    application.advance(1.0 / 60, commands, camera);
    CHECK_MESSAGE(
        (application.ghost && camera.position.x > dotio::to_world(start).x && player.Position == start),
        "Debug arrows did not detach camera while paused");
    const auto detached = camera.position;
    commands = {};
    commands.direction = {1, 0};
    application.paused = false;
    application.advance(1.0 / 60, commands, camera);
    CHECK_MESSAGE((player.Position == start && camera.position.x > detached.x),
                  "WASD stopped controlling player with detached camera");
    application.debug = false;
    application.advance(0, {}, camera);
    CHECK_MESSAGE((!application.ghost && camera.position.x == dotio::to_world(player.Position).x),
                  "Disabling debug did not restore player camera");
}

TEST_CASE("help pause and Ghost commands work and long play keeps finite state") {
    dotio::Application application{42};
    kinetra::Camera2D camera;
    auto &player = application.world().GetPlayer();
    const auto start = player.Position;
    dotio::Commands commands;
    CHECK_MESSAGE((!application.help && !application.menu.visible), "Dotio menus must start hidden");
    commands.help = true;
    application.advance(0, commands, camera);
    CHECK_MESSAGE(application.help, "H did not show Dotio help");
    application.advance(0, commands, camera);
    CHECK_MESSAGE(!application.help, "H did not hide Dotio help");
    commands = {};
    commands.pause = true;
    commands.direction = {1, 0};
    application.advance(1, commands, camera);
    CHECK_MESSAGE((application.paused && player.Position == start), "Pause advanced game");
    commands = {};
    commands.debug = true;
    commands.ghost = true;
    commands.camera_pan = {1, 0};
    application.advance(0.1, commands, camera);
    CHECK_MESSAGE(
        (application.ghost && player.Position == start && application.world().GetGhost().Position != start),
        "Ghost must move while player is paused");
    commands = {};
    commands.pause = true;
    commands.ghost = true;
    application.advance(0, commands, camera);
    for (int frame = 0; frame < 180; ++frame) {
        commands = {};
        commands.direction = {frame % 60 < 30 ? 1.f : -1.f, 0};
        commands.shoot = true;
        commands.aim = dotio::to_world(player.Position + dotio::math::Vec2{500, 0});
        application.advance(1.0 / 60, commands, camera);
        CHECK_MESSAGE((std::isfinite(player.Position.X) && std::isfinite(player.Position.Y)),
                      "Nonfinite player pose");
        for (auto &enemy : application.world().GetEnemies())
            if (enemy.IsActive)
                CHECK_MESSAGE((std::isfinite(enemy.Position.X) && std::isfinite(enemy.Position.Y) &&
                               std::isfinite(enemy.Velocity.X) && std::isfinite(enemy.Velocity.Y)),
                              "Nonfinite enemy state");
    }
}

TEST_CASE("fuzzy spawn rates fall continuously with crowding") {
    CHECK_MESSAGE((dotio::Fuzzy::SpawnRate(0) == 1 && dotio::Fuzzy::SpawnRate(3) == 0.25f &&
                   dotio::Fuzzy::SpawnRate(5) == 0 && dotio::Fuzzy::SpawnRate(100) == 0),
                  "Fuzzy spawn rules lost sparse/occupied/crowded outputs");
    float previous = 1;
    for (int step = 0; step <= 100; ++step) {
        const float rate = dotio::Fuzzy::SpawnRate(float(step) / 10.f);
        CHECK_MESSAGE((rate >= 0 && rate <= previous && previous - rate < 0.1f),
                      "Fuzzy spawn rate is discontinuous or increases with crowding");
        previous = rate;
    }
}

TEST_CASE("spawned enemies stay outside the view and spaced and only difficulty changes their number") {
    CHECK_MESSAGE(population(0.125f, 100) == population(0.125f, 1),
                  "Spawn population depends on health or mana");
    CHECK_MESSAGE(std::abs(int(population(0.125f, 100).size()) - int(population(0.03125f, 100).size())) <= 3,
                  "Spawn pacing changes substantially with frame rate");
    const auto easy = population(0.125f, 100).size();
    const auto medium = population(0.125f, 100, {1120, 700}, dotio::Difficulty::medium).size();
    const auto hard = population(0.125f, 100, {1120, 700}, dotio::Difficulty::hard).size();
    CHECK_MESSAGE((easy < medium && medium < hard), "Difficulty did not increase actual spawn population");
    population(0.125f, 100, {1120, 700}, dotio::Difficulty::easy, 120);
    population(0.125f, 100, {700, 1120}); // Portrait viewport uses the same proportional exclusion.
}

TEST_CASE("paused frames and a crowded player area spawn no new enemies") {
    auto world = std::make_unique<dotio::World>();
    world->Initialize(42, dotio::Difficulty::easy);
    const auto center = world->GetPlayer().Position;
    for (int frame = 0; frame < 1000; ++frame)
        world->StreamEnemies(center, {640, 400}, {5760, 3600}, {8000, 5000}, 0);
    for (const auto &enemy : world->GetEnemies())
        CHECK_MESSAGE(!enemy.IsActive, "Paused/render-only frames created enemies");
    for (int i = 0; i < 5; ++i) {
        auto &enemy = world->GetEnemies()[i];
        enemy.Position = center + dotio::math::Vec2{float(100 + i * 50), 0};
        enemy.IsActive = true;
    }
    for (int second = 0; second < 120; ++second)
        world->StreamEnemies(center, {640, 400}, {5760, 3600}, {8000, 5000}, 1);
    int count = 0;
    for (const auto &enemy : world->GetEnemies())
        count += enemy.IsActive;
    CHECK_MESSAGE(count == 5, "Crowded player area continued receiving new enemies");
}

TEST_CASE("difficulty sets enemy hit points and the spawn range grows with survival time") {
    for (auto difficulty : {dotio::Difficulty::easy, dotio::Difficulty::medium, dotio::Difficulty::hard}) {
        auto world = std::make_unique<dotio::World>();
        world->Initialize(42, difficulty);
        open_map(world->grid());
        auto &enemy = world->GetEnemies()[0];
        enemy.Reset();
        enemy.Position = {850, 550};
        world->GetPlayer().Position = {550, 550};
        world->GetPlayer().Health = 50;
        const int kills = world->GetPlayer().Kills;
        const int hits = dotio::settings_for(difficulty).enemy_hits;
        for (int shot = 1; shot <= hits; ++shot) {
            world->AddBolt({550, 550}, {600, 0});
            world->UpdateBolts(1);
            CHECK_MESSAGE((enemy.IsActive == (shot < hits) && enemy.HitsRemaining == hits - shot),
                          "Difficulty hit points do not consume exactly one hit per bolt");
            CHECK_MESSAGE(world->GetPlayer().Kills == kills + (shot == hits),
                          "A nonlethal hit awarded a kill or lethal hit lost reward");
        }
        enemy.Reset();
        CHECK_MESSAGE(enemy.HitsRemaining == hits, "Reused enemy retained previous damage");
        const dotio::math::Vec2 normal{640, 400}, active{5760, 3600}, retained{8000, 5000};
        const auto initial = world->SpawnHalf(normal, active);
        CHECK_MESSAGE(initial == normal * 1.75f, "Initial spawn range changed");
        world->StreamEnemies(world->GetPlayer().Position, normal, active, retained, 150);
        const auto middle = world->SpawnHalf(normal, active);
        CHECK_MESSAGE(middle == (initial + active) * 0.5f, "Spawn range did not grow linearly");
        world->StreamEnemies(world->GetPlayer().Position, normal, active, retained, 0);
        CHECK_MESSAGE(world->SpawnHalf(normal, active) == middle, "Paused time expands spawn range");
        world->StreamEnemies(world->GetPlayer().Position, normal, active, retained, 150);
        CHECK_MESSAGE(world->SpawnHalf(normal, active) == active,
                      "Five minutes did not reach active debug range");
        world->StreamEnemies(world->GetPlayer().Position, normal, active, retained, 1000);
        CHECK_MESSAGE(world->SpawnHalf(normal, active) == active,
                      "Spawn range exceeded debug activity bounds");
        world->Reset();
        CHECK_MESSAGE((world->survival_seconds() == 0 && world->SpawnHalf(normal, active) == initial),
                      "New life retained survival progression");
    }
}

TEST_CASE("harder difficulties fire more and move and pursue more aggressively") {
    const auto easy = encounter(dotio::Difficulty::easy), medium = encounter(dotio::Difficulty::medium),
               hard = encounter(dotio::Difficulty::hard);
    CHECK_MESSAGE((easy.shots < hard.shots && easy.speed < medium.speed && medium.speed < hard.speed &&
                   easy.acceleration < medium.acceleration && medium.acceleration < hard.acceleration &&
                   easy.cooldown > medium.cooldown && medium.cooldown > hard.cooldown && !easy.pursuing &&
                   hard.pursuing),
                  "Difficulty did not change actual movement, firing or pursuit behavior");
}

TEST_CASE("agent statistics count resident enemies and Medium is the default difficulty") {
    dotio::Application application(42);
    CHECK_MESSAGE(application.world().difficulty_settings().name == "MEDIUM", "Default mode is not Medium");
    CHECK_MESSAGE(application.agent_counts().loaded == 0, "Unused pool slots counted as map agents");
    auto &enemies = application.world().GetEnemies();
    enemies[0].IsActive = true;
    enemies[0].Frozen = false;
    enemies[1].IsActive = true;
    enemies[1].Frozen = true;
    CHECK_MESSAGE((application.agent_counts().loaded == 2 && application.agent_counts().active == 1),
                  "Statistics omitted frozen residents or counted them as moving agents");
    enemies[0].IsActive = false;
    CHECK_MESSAGE((application.agent_counts().loaded == 1 && application.agent_counts().active == 0),
                  "Statistics retained a retired enemy");
    application.world().Reset();
    CHECK_MESSAGE((application.agent_counts().loaded == 0 &&
                   application.world().difficulty_settings().name == "MEDIUM"),
                  "Death/reset spawned the whole pool or lost difficulty");
}

TEST_CASE("enemies freeze outside the activity window and release their state when evicted") {
    auto world = fixture();
    auto &enemy = world->GetEnemies()[0];
    const auto before = enemy.Position;
    world->StreamEnemies({550, 550}, {50, 50}, {100, 100}, {1000, 1000});
    CHECK_MESSAGE((enemy.IsActive && enemy.Frozen), "Enemy beyond normal view did not freeze");
    const auto cooldown = enemy.GetCooldown();
    enemy.Tick();
    world->UpdateEnemies(1, false);
    CHECK_MESSAGE((enemy.Position == before && enemy.GetCooldown() == cooldown),
                  "Frozen enemy advanced motion or AI timer");
    enemy.LastPartition[0] = {5, 8};
    world->EnemyPartitionMap[{5, 8}].insert(&enemy);
    world->StreamEnemies({10000, 10000}, {50, 50}, {100, 100}, {1000, 1000});
    CHECK_MESSAGE(
        (!enemy.IsActive && enemy.Path.empty() && !world->EnemyPartitionMap[{5, 8}].contains(&enemy)),
        "Distant enemy retained active state, route or partition reference");
    dotio::Application application(42);
    kinetra::Camera2D camera;
    dotio::Commands commands;
    commands.zoom_steps = -100;
    application.advance(0, commands, camera);
    CHECK_MESSAGE(camera.pixels_per_unit == 25, "Normal Dotio zoom escaped limit");
    const auto player_center = application.world().GetPlayer().Position;
    auto &population = application.world().GetEnemies();
    for (int i = 0; i < 3; ++i) {
        population[i].IsActive = true;
        population[i].Frozen = true;
        population[i].Position =
            player_center +
            dotio::math::Vec2{i == 0 ? 2000.f : camera.viewport.x * 4.5f + (i == 1 ? -1.f : 1.f), 0};
    }
    application.advance(0, {}, camera);
    CHECK_MESSAGE((!population[0].Frozen && !population[1].Frozen && population[2].Frozen),
                  "Agents did not stay active until 90 percent of maximum debug view in normal mode");
    population[2].Path = dotio::Route({cell_center(80, 80)});
    application.debug = true;
    application.ghost = true;
    application.world().GetGhost().Position =
        application.world().GetPlayer().Position + dotio::math::Vec2{5000, 5000};
    application.advance(0, commands, camera);
    for (const auto &agent : application.world().GetEnemies())
        if (agent.IsActive) {
            const auto delta = agent.Position - application.world().GetPlayer().Position;
            CHECK_MESSAGE(agent.Frozen == (std::abs(delta.X) > camera.viewport.x * 4.5f ||
                                           std::abs(delta.Y) > camera.viewport.y * 4.5f),
                          "Ghost changed the player-centered activity window");
            if (agent.Frozen)
                CHECK_MESSAGE(agent.Path.empty(), "Frozen agent retained a path");
        }
    CHECK_MESSAGE(camera.pixels_per_unit == 2.5f, "Debug Dotio zoom failed to extend view");
}

TEST_CASE("legacy vector math and coordinate conversion keep their contracts") {
    const dotio::math::Vec2 vector{3, 4};
    CHECK_MESSAGE((vector.GetLength() == 5 && vector.DotProduct({2, -1}) == 2),
                  "Legacy vector units or dot product changed");
    CHECK_MESSAGE(vector.Determinant({2, -1}) == -11, "Legacy cross product changed");
    CHECK_MESSAGE(dotio::math::Vec2{}.GetNormalized() == dotio::math::Vec2{},
                  "Legacy zero normalization changed");
    auto normalized = vector.GetNormalized();
    CHECK_MESSAGE((std::abs(normalized.X - 0.6f) < 0.000001f && std::abs(normalized.Y - 0.8f) < 0.000001f),
                  "Legacy normalization changed");
    auto rotated = dotio::math::Vec2{1, 0}.Rotate(std::numbers::pi_v<float> / 2);
    CHECK_MESSAGE((std::abs(rotated.X) < 0.000001f && std::abs(rotated.Y - 1) < 0.000001f),
                  "Legacy rotation orientation changed");
    CHECK_MESSAGE(std::abs(dotio::math::Vec2{1, 0}.SignedAngleDifference({0, 1}) -
                           std::numbers::pi_v<float> / 2) < 0.000001f,
                  "Legacy signed angle changed");
    CHECK_MESSAGE((dotio::from_world(dotio::to_world({250, -125})) == dotio::math::Vec2{250, -125}),
                  "Legacy/world coordinate conversion changed");
}

TEST_CASE("a world starts with the preset of its difficulty and the default enemy tuning") {
    for (auto difficulty : {dotio::Difficulty::easy, dotio::Difficulty::medium, dotio::Difficulty::hard}) {
        const auto constructed = std::make_unique<dotio::World>(difficulty);
        CHECK_MESSAGE(constructed->settings() == dotio::settings_for(difficulty),
                      "A constructed world does not start with its preset");
        auto world = std::make_unique<dotio::World>();
        world->Initialize(42, difficulty);
        CHECK_MESSAGE(world->settings() == dotio::settings_for(difficulty),
                      "An initialized world does not start with its preset");
        CHECK_MESSAGE(&world->difficulty_settings() == &world->settings(),
                      "Readers of the difficulty settings do not see the tunable copy");
        CHECK_MESSAGE(world->enemy_tuning() == dotio::EnemyTuning{},
                      "Enemy tuning does not start at default");
    }
    const dotio::EnemyTuning defaults;
    CHECK_MESSAGE((defaults.wander == dotio::WanderMode::steering && defaults.wander_force == 0.75f &&
                   defaults.attack_force == 0.85f && defaults.pursue_force == 0.85f &&
                   defaults.recover_force == 1.1f && defaults.kamikaze_force == 1.3f),
                  "Default enemy tuning lost steering wander or a force of the original game");
}

TEST_CASE("tuned balance survives the player's death and reset_tuning restores preset and defaults") {
    auto world = std::make_unique<dotio::World>();
    world->Initialize(42, dotio::Difficulty::hard);
    open_map(world->grid());
    const auto preset = dotio::settings_for(dotio::Difficulty::hard);
    world->settings().movement_multiplier = 2;
    world->settings().enemy_hits = 5;
    world->settings().active_spawn_limit = 3;
    world->enemy_tuning().wander = dotio::WanderMode::path;
    world->enemy_tuning().pursue_force = 2;
    const auto tuned_settings = world->settings();
    const auto tuned_enemies = world->enemy_tuning();
    world->Reset();
    CHECK_MESSAGE((world->settings() == tuned_settings && world->enemy_tuning() == tuned_enemies),
                  "The player's death discarded tuned values");
    auto &enemy = world->GetEnemies()[0];
    enemy.Reset();
    CHECK_MESSAGE(enemy.HitsRemaining == 5, "A respawned enemy ignores the tuned hit points");
    world->reset_tuning();
    CHECK_MESSAGE(world->settings() == preset, "reset_tuning did not restore the preset of the difficulty");
    CHECK_MESSAGE(world->enemy_tuning() == dotio::EnemyTuning{},
                  "reset_tuning did not restore the default enemy tuning");
    CHECK_MESSAGE(world->difficulty_settings().name == "HARD", "reset_tuning changed the difficulty");
    world->settings().movement_multiplier = 2;
    world->enemy_tuning().wander_force = 2;
    world->Initialize(42, dotio::Difficulty::easy);
    CHECK_MESSAGE((world->settings() == dotio::settings_for(dotio::Difficulty::easy) &&
                   world->enemy_tuning() == dotio::EnemyTuning{}),
                  "A new world kept tuning of the previous one");
}

TEST_CASE("a changed movement multiplier changes how far wandering and pursuing enemies get") {
    for (const bool pursuing : {false, true}) {
        CAPTURE(pursuing);
        const float preset = dotio::settings_for(dotio::Difficulty::easy).movement_multiplier;
        const float normal = travelled(preset, pursuing), doubled = travelled(2 * preset, pursuing);
        CHECK_MESSAGE(normal > dotio::Grid::CellSize, "The enemy hardly moved at its preset");
        // Speed gained and speed lost are both proportional, so the distance scales with it.
        CHECK_MESSAGE(std::abs(doubled - 2 * normal) < 0.01f * normal,
                      "Twice the movement multiplier did not carry the enemy twice as far");
        CHECK_MESSAGE(travelled(0, pursuing) == 0, "An enemy moved without a movement multiplier");
    }
}

TEST_CASE("validation accepts the presets and rejects balance a world cannot run with") {
    using Settings = dotio::DifficultySettings;
    using Tuning = dotio::EnemyTuning;
    for (const auto &preset : dotio::difficulty_presets)
        CHECK_NOTHROW_MESSAGE(dotio::validate(preset), preset.name);
    CHECK_NOTHROW(dotio::validate(Tuning{}));
    const std::vector<std::pair<const char *, void (*)(Settings &)>> accepted_settings{
        {"no spawns", [](Settings &s) { s.active_spawn_limit = 0; }},
        {"no spawn rate", [](Settings &s) { s.spawn_rate_multiplier = 0; }},
        {"standing enemies", [](Settings &s) { s.movement_multiplier = 0; }},
        {"fastest enemies", [](Settings &s) { s.movement_multiplier = dotio::max_movement_multiplier; }},
        {"a shot per decision", [](Settings &s) { s.fire_interval_multiplier = 0; }},
        {"no prediction", [](Settings &s) { s.prediction = 0; }},
        {"one pursuit cycle and one hit", [](Settings &s) { s.pursuit_cycles = s.enemy_hits = 1; }},
    };
    for (const auto &[what, change] : accepted_settings) {
        auto settings = dotio::settings_for(dotio::Difficulty::medium);
        change(settings);
        CHECK_NOTHROW_MESSAGE(dotio::validate(settings), what);
    }
    const std::vector<std::pair<const char *, void (*)(Settings &)>> rejected_settings{
        {"negative spawn limit", [](Settings &s) { s.active_spawn_limit = -1; }},
        {"negative spawn rate", [](Settings &s) { s.spawn_rate_multiplier = -0.5f; }},
        {"nonfinite spawn rate", [](Settings &s) { s.spawn_rate_multiplier = std::nanf(""); }},
        {"negative crowd load", [](Settings &s) { s.crowd_load_multiplier = -1; }},
        {"nonfinite crowd load", [](Settings &s) { s.crowd_load_multiplier = std::nanf(""); }},
        {"negative spawn spacing", [](Settings &s) { s.spawn_spacing = -1; }},
        {"nonfinite spawn spacing", [](Settings &s) { s.spawn_spacing = std::nanf(""); }},
        {"negative movement", [](Settings &s) { s.movement_multiplier = -1; }},
        {"nonfinite movement", [](Settings &s) { s.movement_multiplier = std::nanf(""); }},
        {"movement past walls",
         [](Settings &s) { s.movement_multiplier = std::nextafter(dotio::max_movement_multiplier, 100.f); }},
        {"negative fire interval", [](Settings &s) { s.fire_interval_multiplier = -1; }},
        {"nonfinite fire interval",
         [](Settings &s) { s.fire_interval_multiplier = std::numeric_limits<float>::infinity(); }},
        {"projectile without speed", [](Settings &s) { s.projectile_speed = 0; }},
        {"negative projectile speed", [](Settings &s) { s.projectile_speed = -13; }},
        {"nonfinite projectile speed",
         [](Settings &s) { s.projectile_speed = std::numeric_limits<float>::infinity(); }},
        {"negative prediction", [](Settings &s) { s.prediction = -0.1f; }},
        {"nonfinite prediction", [](Settings &s) { s.prediction = std::nanf(""); }},
        {"no pursuit cycle", [](Settings &s) { s.pursuit_cycles = 0; }},
        {"no hit point", [](Settings &s) { s.enemy_hits = 0; }},
    };
    for (const auto &[what, change] : rejected_settings) {
        auto settings = dotio::settings_for(dotio::Difficulty::medium);
        change(settings);
        CHECK_THROWS_AS_MESSAGE(dotio::validate(settings), std::invalid_argument, what);
    }
    for (const auto force : {&Tuning::wander_force, &Tuning::attack_force, &Tuning::pursue_force,
                             &Tuning::recover_force, &Tuning::kamikaze_force}) {
        for (const float accepted : {0.f, dotio::max_state_force}) {
            Tuning tuning;
            tuning.*force = accepted;
            CHECK_NOTHROW_MESSAGE(dotio::validate(tuning), accepted);
        }
        for (const float rejected : {-0.1f, std::nextafter(dotio::max_state_force, 100.f), std::nanf(""),
                                     std::numeric_limits<float>::infinity()}) {
            Tuning tuning;
            tuning.*force = rejected;
            CHECK_THROWS_AS_MESSAGE(dotio::validate(tuning), std::invalid_argument, rejected);
        }
    }
    Tuning unknown_mode;
    unknown_mode.wander = static_cast<dotio::WanderMode>(2);
    CHECK_THROWS_AS(dotio::validate(unknown_mode), std::invalid_argument);
}

TEST_CASE("the largest accepted speed does not carry an enemy through a wall") {
    auto world = walled_world();
    // A corridor along row 10 that a single wall cell at column 21 cuts in two.
    carve(world->grid(), 10, 10, 10, 20);
    carve(world->grid(), 10, 22, 10, 30);
    world->settings().movement_multiplier = dotio::max_movement_multiplier;
    world->enemy_tuning().attack_force = dotio::max_state_force;
    REQUIRE_NOTHROW(dotio::validate(world->settings()));
    REQUIRE_NOTHROW(dotio::validate(world->enemy_tuning()));
    world->GetPlayer().Position = cell_center(10, 29);
    auto &enemy = wanderer(*world, cell_center(10, 11), 0);
    enemy.Fear = 0; // Without fear an attacker charges the player instead of keeping its distance.
    enemy.SetState(enemy.GetAttackState());
    const float wall = 21 * dotio::Grid::CellSize;
    float fastest = 0, deepest = 0;
    // Different starts make the last and longest step meet the wall at different depths.
    for (int start = 0; start < 50; start += 5) {
        enemy.Position = cell_center(10, 11) + dotio::math::Vec2{float(start), 0};
        enemy.Velocity = {};
        for (int update = 0; update < 120; ++update) {
            const auto before = enemy.Position;
            world->UpdateEnemies(1, false);
            fastest = std::max(fastest, (enemy.Position - before).GetLength());
            deepest = std::max(deepest, enemy.Position.X + enemy.Radius - wall);
        }
    }
    CHECK_MESSAGE(fastest > 0.4f * dotio::Grid::CellSize, "The enemy did not reach the speed under test");
    // Contact resolution leaves a body a fraction of a pixel inside the face it hit.
    CHECK_MESSAGE(deepest < 1, "An enemy at the largest accepted speed got into a wall");
}

TEST_CASE("every state accelerates an enemy with its force from the enemy tuning") {
    auto world = fixture();
    auto &enemy = world->GetEnemies()[0];
    auto &tuning = world->enemy_tuning();
    const float multiplier = world->difficulty_settings().movement_multiplier;
    struct StateForce {
        const char *name;
        dotio::State &state;
        float dotio::EnemyTuning::*force;
        float original;
    };
    const std::array<StateForce, 5> states{
        {{"wander", enemy.GetWanderState(), &dotio::EnemyTuning::wander_force, 0.75f},
         {"attack", enemy.GetAttackState(), &dotio::EnemyTuning::attack_force, 0.85f},
         {"pursue", enemy.GetPursueState(), &dotio::EnemyTuning::pursue_force, 0.85f},
         {"recover", enemy.GetRecoverState(), &dotio::EnemyTuning::recover_force, 1.1f},
         {"kamikaze", enemy.GetKamikazeState(), &dotio::EnemyTuning::kamikaze_force, 1.3f}}};
    const auto near = [](float actual, float expected) { return std::abs(actual - expected) < 0.0001f; };
    for (const auto mode : {dotio::WanderMode::steering, dotio::WanderMode::path}) {
        for (const auto &tuned : states) {
            CAPTURE(tuned.name);
            tuning = {};
            tuning.wander = mode;
            CHECK_MESSAGE(near(acceleration(enemy, tuned.state), tuned.original * multiplier),
                          "The default force does not reproduce the original acceleration");
            tuning.*tuned.force = 2;
            for (const auto &measured : states)
                CHECK_MESSAGE(near(acceleration(enemy, measured.state),
                                   (&measured == &tuned ? 2 : measured.original) * multiplier),
                              "A tuned force did not change exactly the acceleration of its own state");
        }
    }
}

TEST_CASE("a steering wanderer moves down a corridor without a path and stays off its walls") {
    auto world = walled_world();
    carve(world->grid(), 10, 10, 10, 40);
    auto &enemy = wanderer(*world, cell_center(10, 12), 0);
    const float middle = cell_center(10, 12).Y;
    float off_middle = 0;
    bool path_free = true;
    for (int update = 0; update < 300; ++update) {
        run(*world, 1, update);
        path_free = path_free && enemy.Path.empty();
        off_middle = std::max(off_middle, std::abs(enemy.Position.Y - middle));
    }
    CHECK_MESSAGE(enemy.IsWandering(), "The enemy left its wander state without seeing the player");
    CHECK_MESSAGE(path_free, "Steering wander planned a path");
    CHECK_MESSAGE(enemy.Position.X > cell_center(10, 18).X, "The wanderer did not advance down the corridor");
    CHECK_MESSAGE(off_middle + enemy.Radius < dotio::Grid::CellSize / 2,
                  "The wanderer's body touched a corridor wall");
}

TEST_CASE("a steering wanderer turns away from a wall ahead before touching it and keeps moving") {
    auto world = walled_world();
    carve(world->grid(), 10, 10, 30, 30);
    const float wall = 10 * dotio::Grid::CellSize; // The room's west face.
    auto &enemy = wanderer(*world, cell_center(20, 12), std::numbers::pi_v<float>);
    float nearest = enemy.Position.X, slowest_second = std::numeric_limits<float>::infinity();
    float retreat = 0; // Furthest the enemy got from the wall again after its nearest approach.
    for (int second = 0; second < 5; ++second) {
        float walked = 0;
        for (int update = second * 60; update < second * 60 + 60; ++update) {
            const auto before = enemy.Position;
            run(*world, 1, update);
            walked += (enemy.Position - before).GetLength();
            if (enemy.Position.X < nearest) {
                nearest = enemy.Position.X;
                retreat = 0;
            }
            retreat = std::max(retreat, enemy.Position.X - nearest);
        }
        slowest_second = std::min(slowest_second, walked);
    }
    CHECK_MESSAGE(nearest < cell_center(20, 11).X, "The wanderer never approached the wall");
    CHECK_MESSAGE(nearest - enemy.Radius > wall, "The wanderer's body reached the wall it was heading for");
    CHECK_MESSAGE(slowest_second > 0.5f * 60 * wander_speed(*world), "The wanderer stalled at the wall");
    CHECK_MESSAGE(retreat > dotio::Grid::CellSize, "The wanderer stayed at the wall");
}

TEST_CASE("a steering wanderer turns around in a dead end and leaves it") {
    auto world = walled_world();
    carve(world->grid(), 10, 10, 10, 20);
    auto &enemy = wanderer(*world, cell_center(10, 18), 0);
    float deepest = enemy.Position.X;
    int updates = 0;
    for (; updates < 600 && enemy.Position.X > cell_center(10, 14).X; ++updates) {
        run(*world, 1, updates);
        deepest = std::max(deepest, enemy.Position.X);
    }
    CHECK_MESSAGE(deepest > cell_center(10, 19).X, "The wanderer never walked into the dead end");
    CHECK_MESSAGE(deepest + enemy.Radius < 21 * dotio::Grid::CellSize, "The wanderer ran into the end wall");
    CHECK_MESSAGE(updates < 600, "The wanderer did not leave the dead end within ten seconds");
    CHECK_MESSAGE(enemy.Path.empty(), "Leaving a dead end planned a path");
}

TEST_CASE("a steering wanderer heading into a corner keeps walking and gets out of it") {
    const float pi = std::numbers::pi_v<float>;
    // Toward the corner of a room at its top right, from straight at one wall to straight at the other.
    for (const float heading : {0.f, -pi / 8, -pi / 4, -3 * pi / 8, -pi / 2}) {
        CAPTURE(heading);
        auto world = walled_world();
        carve(world->grid(), 10, 10, 30, 30);
        const dotio::math::Vec2 corner{31 * dotio::Grid::CellSize, 10 * dotio::Grid::CellSize};
        auto &enemy = wanderer(*world, cell_center(11, 29), heading);
        float slowest_half_second = std::numeric_limits<float>::infinity();
        float closest = std::numeric_limits<float>::infinity();
        float retreat = 0; // Furthest the enemy got from the corner again after its closest approach.
        int sharp_turns = 0;
        for (int window = 0; window < 6; ++window) {
            float walked = 0;
            for (int update = window * 30; update < window * 30 + 30; ++update) {
                const auto before = enemy.Position;
                const float facing = enemy.WanderHeading;
                run(*world, 1, update);
                walked += (enemy.Position - before).GetLength();
                const float distance = (enemy.Position - corner).GetLength();
                if (distance < closest) {
                    closest = distance;
                    retreat = 0;
                }
                retreat = std::max(retreat, distance - closest);
                // Drift and a side feeler turn less than this per update; a blocked forward feeler more.
                sharp_turns += std::abs(std::remainder(enemy.WanderHeading - facing, 2 * pi)) > 0.75f;
            }
            if (window > 0) // The enemy starts at rest.
                slowest_half_second = std::min(slowest_half_second, walked);
        }
        CHECK_MESSAGE(slowest_half_second > 0.5f * 30 * wander_speed(*world),
                      "The wanderer stalled in the corner");
        CHECK_MESSAGE(sharp_turns <= 4, "The wanderer kept turning back and forth in the corner");
        CHECK_MESSAGE(retreat > dotio::Grid::CellSize, "The wanderer did not get out of the corner");
    }
}

TEST_CASE("the wander feelers follow the heading and steering reacts to exactly those segments") {
    auto world = walled_world();
    carve(world->grid(), 10, 10, 30, 30);
    const float cell = dotio::Grid::CellSize;
    auto &enemy = wanderer(*world, cell_center(20, 20), 0.5f);
    const auto feelers = dotio::WanderFeelers(enemy);
    const std::array<float, 3> lengths{dotio::wander_forward_feeler_cells * cell,
                                       dotio::wander_side_feeler_cells * cell,
                                       dotio::wander_side_feeler_cells * cell};
    const std::array<float, 3> angles{0.5f, 0.5f + dotio::wander_side_feeler_angle_rad,
                                      0.5f - dotio::wander_side_feeler_angle_rad};
    for (std::size_t i = 0; i < feelers.size(); ++i) {
        const auto reach = feelers[i].End - feelers[i].Start;
        CHECK_MESSAGE(feelers[i].Start == enemy.Position, "A feeler does not start at the enemy");
        CHECK_MESSAGE(std::abs(reach.GetLength() - lengths[i]) < 0.001f, "A feeler has the wrong length");
        CHECK_MESSAGE(std::abs(reach.GetDirection() - angles[i]) < 0.0001f, "A feeler has the wrong angle");
    }
    CHECK_MESSAGE((dotio::wander_forward_feeler == 0 && dotio::wander_positive_feeler == 1 &&
                   dotio::wander_negative_feeler == 2),
                  "The feeler order changed");
    const auto blocked = [&world](const dotio::math::Segment &feeler) {
        dotio::math::Point2 hit{};
        return world->grid().IsIntersectingAnyCell(feeler, hit);
    };
    const auto blocked_feelers = [&](const dotio::Enemy &subject) {
        const auto tested = dotio::WanderFeelers(subject);
        return std::array<bool, 3>{blocked(tested[0]), blocked(tested[1]), blocked(tested[2])};
    };
    // In the middle of the room no feeler touches a wall and the heading is left alone.
    CHECK_MESSAGE((blocked_feelers(enemy) == std::array{false, false, false}),
                  "A feeler is blocked in the open");
    enemy.GetState().Execute(1);
    CHECK_MESSAGE(enemy.WanderHeading == 0.5f, "Steering changed the heading although no feeler was blocked");
    // Along the east wall, close enough for the side feeler toward it only.
    const float east_wall = 31 * cell;
    enemy.Position = {east_wall - 30, cell_center(20, 30).Y};
    enemy.WanderHeading = std::numbers::pi_v<float> / 2;
    enemy.WanderSide = -1;
    CHECK_MESSAGE((blocked_feelers(enemy) == std::array{false, false, true}),
                  "Only the side feeler toward the wall should be blocked");
    enemy.GetState().Execute(1);
    CHECK_MESSAGE(std::abs(enemy.WanderHeading - (std::numbers::pi_v<float> / 2 +
                                                  dotio::wander_side_turn_rad_per_update)) < 0.0001f,
                  "A blocked side feeler did not nudge the heading away from its side");
    CHECK_MESSAGE(enemy.WanderSide == 1, "The side away from the wall did not become the preferred one");
    // Straight at the east wall: the forward feeler is blocked and the heading turns until it is free.
    enemy.Position = {east_wall - 50, cell_center(20, 30).Y};
    enemy.Velocity = {};
    enemy.WanderHeading = 0;
    CHECK_MESSAGE(blocked_feelers(enemy)[dotio::wander_forward_feeler], "The forward feeler misses the wall");
    enemy.GetState().Execute(1);
    CHECK_MESSAGE(!blocked_feelers(enemy)[dotio::wander_forward_feeler],
                  "Steering left the heading on a blocked forward feeler");
    // The smallest turn points at the wall on both sides; the second is free on the preferred side.
    CHECK_MESSAGE(std::abs(enemy.WanderHeading - dotio::wander_fan_rad[1]) < 0.0001f,
                  "The heading did not turn to the first free direction on the preferred side");
    CHECK_MESSAGE(enemy.WanderSide == 1, "A turn to the preferred side changed the preference");
    CHECK_MESSAGE(enemy.Velocity.X < 0, "The enemy still accelerates toward the wall");
    // In the room's top right corner the preferred side is walled in, so the other side wins and
    // becomes the preferred one.
    enemy.Position = cell_center(10, 30);
    enemy.WanderHeading = 0;
    enemy.WanderSide = -1;
    enemy.GetState().Execute(1);
    CHECK_MESSAGE(std::abs(enemy.WanderHeading - dotio::wander_fan_rad[1]) < 0.0001f,
                  "The heading did not turn to the free side");
    CHECK_MESSAGE(enemy.WanderSide == 1, "The side that was free did not become the preferred one");
}

TEST_CASE("steering wander repeats for a world seed and differs between seeds") {
    struct Pose {
        float heading, x, y;
        bool operator==(const Pose &) const = default;
    };
    // One wanderer in a room; the rest of the world is the same in every run.
    const auto wander = [](std::uint32_t seed) {
        auto world = walled_world(seed);
        carve(world->grid(), 10, 10, 20, 20);
        std::vector<std::pair<float, float>> initial;
        for (const auto &enemy : world->GetEnemies())
            initial.emplace_back(enemy.WanderHeading, enemy.WanderSide);
        auto &enemy = wanderer(*world, cell_center(15, 15), world->GetEnemies()[7].WanderHeading, 7);
        std::vector<Pose> poses;
        for (int update = 0; update < 600; ++update) {
            run(*world, 1, update);
            poses.push_back({enemy.WanderHeading, enemy.Position.X, enemy.Position.Y});
        }
        return std::pair{initial, poses};
    };
    const auto first = wander(42), again = wander(42), other = wander(43);
    CHECK_MESSAGE(first.first == again.first, "Enemies of one seed start with different headings or sides");
    CHECK_MESSAGE(first.second == again.second, "Steering wander is not repeatable for a world seed");
    CHECK_MESSAGE(first.first != other.first, "Another seed gave the same starting headings");
    CHECK_MESSAGE(first.second != other.second, "Another seed gave the same wander");
}

TEST_CASE("path mode still plans wander paths and steering drops them again") {
    auto world = walled_world();
    carve(world->grid(), 10, 10, 20, 20);
    world->enemy_tuning().wander = dotio::WanderMode::path;
    auto &enemy = wanderer(*world, cell_center(15, 15), 0);
    // The random goal can be the enemy's own cell, which gives an empty path; the next
    // decision tick plans again.
    int updates = 0;
    for (; updates < 60 && enemy.Path.empty(); ++updates)
        run(*world, 1, updates);
    REQUIRE_MESSAGE(!enemy.Path.empty(), "Path mode planned no wander path");
    for (const auto waypoint : enemy.Path.remaining())
        CHECK_MESSAGE(world->grid().Cell(int(waypoint.Y / 100), int(waypoint.X / 100)) == 0,
                      "A wander path enters a wall");
    const float heading = enemy.WanderHeading;
    run(*world, 30, updates);
    CHECK_MESSAGE(enemy.WanderHeading == heading, "Path mode changed the steering heading");
    CHECK_MESSAGE(enemy.IsWandering(), "The enemy left its wander state without seeing the player");
    world->enemy_tuning().wander = dotio::WanderMode::steering;
    enemy.Path = dotio::Route({cell_center(15, 15)});
    world->UpdateEnemies(1, false);
    CHECK_MESSAGE(enemy.Path.empty(), "Steering wander kept a path from path mode");
}

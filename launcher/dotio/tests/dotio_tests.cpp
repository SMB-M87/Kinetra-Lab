#include "../include/application.h"
#include "fuzzy.h"
#include <iostream>
#include <vector>

namespace {
void require(bool condition, const char *message) {
    if (!condition)
        throw std::runtime_error(message);
}
void open_map() {
    for (int row = 0; row < logic::Grid::Rows; ++row)
        for (int column = 0; column < logic::Grid::Columns; ++column)
            logic::Grid::Array[row][column] =
                row == 0 || column == 0 || row == logic::Grid::Rows - 1 || column == logic::Grid::Columns - 1;
}
std::unique_ptr<logic::World> fixture() {
    auto world = std::make_unique<logic::World>(logic::Difficulty::easy);
    world->ExtractGraph();
    world->SetPlayer(logic::Player(world.get(), {550, 550}, 0));
    world->SetGhost(logic::Player(world.get(), {550, 550}, 0));
    world->GetPlayer().Cooldown = 0;
    world->AddEnemy({850, 550}, {550, 550}, 8, true);
    world->GetEnemies()[0].Initialize();
    return world;
}
// Incremental residency must preserve the exact graph produced by a full rebuild.
void incremental_residency_graph() {
    auto world = std::make_unique<logic::World>();
    world->Initialize(42);
    for (int step = 0; step < 12; ++step) {
        if (step == 8)
            world->Initialize(43);
        const gmath::Position center{7050.f + step * 100, 7950.f + (step % 3) * 100};
        const float half = step % 4 == 0 ? 1200.f : 2400.f;
        world->StreamEnemies(center, {100, 100}, {500, 500}, {half, half});
        for (int row = 0; row < logic::Grid::Rows; ++row)
            for (int col = 0; col < logic::Grid::Columns; ++col) {
                const bool inside = row >= int(std::floor((center.Y - half) / 100)) &&
                                    row <= int(std::floor((center.Y + half) / 100)) &&
                                    col >= int(std::floor((center.X - half) / 100)) &&
                                    col <= int(std::floor((center.X + half) / 100));
                require(logic::Grid::Array[row][col] == (inside ? world->TerrainCell(row, col) : 1),
                        "Incremental residency changed terrain occupancy");
            }
        const auto incremental = world->GetGraph().vertexMap;
        world->ExtractGraph();
        const auto &rebuilt = world->GetGraph().vertexMap;
        require(incremental.size() == rebuilt.size(), "Incremental graph retained missing/extra cells");
        for (const auto &[cell, vertex] : rebuilt) {
            const auto found = incremental.find(cell);
            require(found != incremental.end(), "Incremental graph omitted a free cell");
            const auto &actual = found->second;
            require(actual.straightIndex == vertex.straightIndex &&
                        actual.diagonalIndex == vertex.diagonalIndex,
                    "Incremental graph has stale adjacency counts");
            for (int i = 0; i < vertex.straightIndex; ++i)
                require(actual.straight[i] == vertex.straight[i], "Incremental straight link changed");
            for (int i = 0; i < vertex.diagonalIndex; ++i)
                require(actual.diagonal[i] == vertex.diagonal[i], "Incremental diagonal link changed");
        }
        if (step == 5) {
            world->GetPlayer().Position = {11250, 11250};
            world->StreamAroundPlayer();
        }
    }
    open_map();
}
void geometry_and_routes() {
    gmath::Point2 hit{};
    require((logic::Grid::SegementCircleIntersection({{550, 300}, {550, 800}}, {550, 550}, 20, hit) & 1) &&
                std::abs(hit.Y - 530) < 0.01f,
            "Vertical projectile failed continuous collision");
    require(!(logic::Grid::SegementCircleIntersection({{500, 300}, {500, 800}}, {550, 550}, 20, hit) & 1),
            "Projectile hit a circle outside its segment");
    require(logic::Grid::SegementCircleIntersection({{550, 550}, {550, 550}}, {550, 550}, 20, hit) & 1,
            "Point segment inside circle must hit");
    logic::Grid::Array[7][5] = 1;
    require(logic::Grid::IsIntersectingAnyCell({{550, 550}, {550, 900}}, &logic::Grid::Array[0][0], hit) &&
                std::abs(hit.Y - 700) < 0.01f,
            "Vertical line of sight must stop at wall");
    require(!logic::Grid::IsIntersectingAnyCell({{450, 550}, {450, 900}}, &logic::Grid::Array[0][0], hit),
            "Clear vertical sight falsely blocked");
    require(logic::Grid::IsIntersectingAnyCell({{550, 550}, {-50, 550}}, &logic::Grid::Array[0][0], hit),
            "Finite world boundary must block projectiles");
    auto world = fixture();
    logic::Vertex goal;
    require(world->GetGraph().AStar({5, 5}, {9, 5}, goal), "A* did not find wall detour");
    auto path = world->GetGraph().GetPath({5, 5}, goal);
    require(!path.empty(), "A* returned no waypoints");
    while (!path.empty()) {
        const auto cell = path.top();
        require(logic::Grid::Array[cell.Row][cell.Column] == 0, "A* path entered wall");
        path.pop();
    }
    require(world->GetGraph().HideStar({5, 5}, {550, 950}, 2, goal), "HideStar did not find shelter");
    logic::Grid::Array[7][5] = 0;
}
void states_and_fuzzy() {
    require(logic::Fuzzy::Evaluate(400, 3, 4), "Original close/warrior/midway fuzzy rule missing");
    require(!logic::Fuzzy::Evaluate(2000, 3, 4) && !logic::Fuzzy::Evaluate(400, 9, 4),
            "Fuzzy kamikaze trigger ignored distance/fear");
    auto world = fixture();
    auto &enemy = world->GetEnemies()[0];
    require(enemy.IsWandering(), "Enemy must spawn wandering");
    enemy.Tick();
    require(enemy.IsAttacking(), "Visible player must trigger attack");
    enemy.Fear = 3;
    for (int i = 0; i < 3; ++i)
        enemy.AmmoFired();
    for (int health : {3, 2, 0}) {
        enemy.HitsRemaining = health;
        enemy.GetAttackState().Update();
        require(enemy.IsAttacking(), "Fuzzy rule triggered kamikaze without exactly one health");
    }
    enemy.HitsRemaining = 1;
    enemy.Tick();
    require(enemy.IsKamikazing(), "Fuzzy rule must trigger kamikaze at one health");
    enemy.Reset();
    enemy.Fear = 9;
    enemy.SetState(enemy.GetAttackState());
    for (int i = 0; i < 10; ++i)
        enemy.AmmoFired();
    enemy.Tick();
    require(enemy.IsRecovering(), "Empty ammo must trigger recovery");
    enemy.SetState(enemy.GetAttackState());
    enemy.Reset();
    enemy.Fear = 9;
    enemy.SetState(enemy.GetAttackState());
    logic::Grid::Array[5][7] = 1;
    enemy.Tick();
    require(enemy.IsPursueing(), "Losing sight must trigger pursue");
    for (int i = 0; i < 11; ++i)
        enemy.PursueCountdown();
    enemy.Tick();
    require(enemy.IsWandering(), "Expired pursuit must resume wander");
    logic::Grid::Array[5][7] = 0;
    enemy.Reset();
    enemy.SetState(enemy.GetRecoverState());
    enemy.Tick();
    require(enemy.IsPursueing(), "Reloaded recovery must return to pursuit");
}
void recovery_kamikaze_health_gate() {
    auto world = fixture();
    open_map();
    world->ExtractGraph();
    auto &enemy = world->GetEnemies()[0];
    enemy.LineOfSight = true;
    enemy.Fear = 9;
    for (int i = 0; i < 10; ++i)
        enemy.AmmoFired();
    logic::Vertex shelter;
    require(!world->GetGraph().HideStar({5, 8}, enemy.Target, enemy.Fear, shelter),
            "Recovery health fixture must have no shelter");
    for (int health : {3, 2, 0, 1}) {
        enemy.HitsRemaining = health;
        enemy.Path.clear();
        enemy.SetState(enemy.GetRecoverState());
        enemy.GetRecoverState().Update();
        require(health == 1 ? enemy.IsKamikazing() : enemy.IsRecovering(),
                "Failed escape must only trigger kamikaze at exactly one health");
    }
}
void combat_and_reset() {
    auto world = fixture();
    auto &player = world->GetPlayer();
    auto &enemy = world->GetEnemies()[0];
    player.Shoot({850, 550}, false);
    require(player.Mana == 90 && player.Cooldown > 0, "Shooting must spend mana and set cooldown");
    for (auto &bolt : world->GetBolts())
        bolt.IsActive = false;
    player.Health = 50;
    world->AddBolt({550, 550}, {600, 0});
    world->UpdateBolts(1);
    require(!enemy.IsActive && player.Kills == 2 && player.Health == 60, "Bolt kill/reward missing");
    enemy.Reset();
    logic::Grid::Array[5][7] = 1;
    world->AddBolt({550, 550}, {600, 0});
    world->UpdateBolts(1);
    require(enemy.IsActive, "Bullet killed enemy behind wall");
    logic::Grid::Array[5][7] = 0;
    world->AddArrow({550, 350}, {0, 400});
    world->UpdateArrows(1, false);
    require(player.Health == 50, "Enemy arrow did not damage player");
    world->AddArrow({550, 350}, {0, 400});
    world->UpdateArrows(1, true);
    require(player.Health == 50, "Debug mode must prevent projectile damage");
    world->AddBolt({550, 550}, {0, 5});
    world->Reset();
    require(player.Health == 100 && player.Mana == 100 && player.Kills == 1 && enemy.IsWandering(),
            "Reset did not restore game resources and AI state");
    for (auto &bolt : world->GetBolts())
        require(!bolt.IsActive, "Reset retained live bolts");
    enemy.IsActive = true;
    enemy.Frozen = false;
    enemy.Position = player.Position + gmath::Vec2{10, 0};
    enemy.Target = player.Position;
    player.Mana = 50;
    enemy.Tick();
    require(player.Mana >= 70, "Contact kill must restore mana");
    require(!enemy.IsActive && enemy.Frozen && enemy.Path.empty(),
            "Contact kill bypassed the timed offscreen spawn controller");
    enemy.IsActive = true;
    enemy.Frozen = false;
    enemy.Position = player.Position + gmath::Vec2{10, 0};
    enemy.Target = player.Position;
    enemy.SetState(enemy.GetKamikazeState());
    enemy.Debug = true;
    enemy.Tick();
    require(player.Health == 100, "Debug mode must protect against kamikaze contact");
}
void generated_arena() {
    logic::Grid::Generate(42);
    std::vector<int> first;
    for (const auto &row : logic::Grid::Array)
        first.insert(first.end(), std::begin(row), std::end(row));
    logic::Grid::Generate(42);
    std::size_t index = 0;
    for (int row = 0; row < logic::Grid::Rows; ++row)
        for (int col = 0; col < logic::Grid::Columns; ++col) {
            require(logic::Grid::Array[row][col] == first[index++], "Arena seed is not reproducible");
            require(logic::Grid::Array[row][col] == logic::Grid::TerrainCell(42, row, col),
                    "Streamed window differs from absolute terrain");
        }
    // Flood fill verifies every free cell belongs to the playable region.
    std::vector<std::pair<int, int>> queue{{2, 2}};
    std::vector<bool> visited(logic::Grid::Rows * logic::Grid::Columns);
    visited[2 * logic::Grid::Columns + 2] = true;
    for (std::size_t i = 0; i < queue.size(); ++i) {
        const auto [row, col] = queue[i];
        for (const auto &[dy, dx] : {std::pair{1, 0}, {-1, 0}, {0, 1}, {0, -1}}) {
            const int y = row + dy, x = col + dx;
            if (y < 0 || x < 0 || y >= logic::Grid::Rows || x >= logic::Grid::Columns)
                continue;
            const int id = y * logic::Grid::Columns + x;
            if (!visited[id] && !logic::Grid::Array[y][x]) {
                visited[id] = true;
                queue.emplace_back(y, x);
            }
        }
    }
    bool changed = false;
    for (int row = 0; row < logic::Grid::Rows; ++row)
        for (int col = 0; col < logic::Grid::Columns; ++col)
            require(row >= 128 || col >= 128 || logic::Grid::Array[row][col] ||
                        visited[row * logic::Grid::Columns + col],
                    "Disconnected arena floor");
    logic::Grid::Generate(43);
    index = 0;
    for (const auto &row : logic::Grid::Array)
        for (const auto cell : row)
            changed |= cell != first[index++];
    require(changed, "Different arena seeds produced identical geometry");
    open_map();
}
void streamed_world_and_objects() {
    logic::World world;
    world.Initialize(42);
    const auto initial_offset = world.WorldOffset();
    auto &player = world.GetPlayer();
    std::vector<int> first;
    for (const auto &row : logic::Grid::Array)
        first.insert(first.end(), std::begin(row), std::end(row));
    require(!world.GetObjects().empty(), "No physics objects generated");
    const auto crate = world.GetObjects().front().body;
    for (auto &enemy : world.GetEnemies())
        enemy.IsActive = false;
    player.Position = {crate.position.x - crate.radius - player.Radius + 2, crate.position.y};
    player.Velocity = {4, 0};
    world.UpdateObjects(1);
    require(world.GetObjects().front().body.velocity.x > 0, "Player cannot push crate");
    // Projectiles hit the crate before an actor behind it and transfer momentum.
    auto &enemy = world.GetEnemies()[0];
    enemy.IsActive = true;
    enemy.Position = {crate.position.x + 60, crate.position.y};
    world.AddBolt({crate.position.x - 80, crate.position.y}, {160, 0});
    world.UpdateBolts(1);
    require(enemy.IsActive && !world.GetBolts()[0].IsActive, "Crate failed to intercept projectile");
    enemy.IsActive = false;
    player.Position = {crate.position.x + 60, crate.position.y};
    player.Health = 100;
    world.AddArrow({crate.position.x - 80, crate.position.y}, {160, 0});
    world.UpdateArrows(1, false);
    require(player.Health == 100 && !world.GetArrows()[0].IsActive, "Crate failed to shield player");
    const auto moving = world.GetObjects().front().body.position;
    player.Position = {8050, 8050};
    player.Velocity = {};
    world.UpdateObjects(1);
    require(world.GetObjects().front().body.position.x > moving.x, "Released crate did not move");
    // Cross both axes repeatedly, including negative world cells, and verify geometry continuity.
    for (const float coordinate : {11250.f, 11250.f, 3250.f, 3250.f, 3250.f, 3250.f}) {
        player.Position = {coordinate, coordinate};
        const auto absolute = player.Position + world.WorldOffset();
        const auto old_position = player.Position;
        const auto old_ghost = world.GetGhost().Position;
        const auto shift = world.StreamAroundPlayer();
        require(shift != gmath::Vec2{}, "Streaming window did not advance");
        require(player.Position + world.WorldOffset() == absolute,
                "Streaming changed absolute player position");
        require(world.GetGhost().Position == old_ghost - shift, "Streaming failed to rebase camera");
        require(player.Position != old_position, "Local origin was not rebased");
        for (auto &enemy : world.GetEnemies())
            require(enemy.Path.empty(), "Streaming retained stale navigation path");
        for (int row = 0; row < logic::Grid::Rows; ++row)
            for (int col = 0; col < logic::Grid::Columns; ++col)
                require(logic::Grid::Array[row][col] == world.TerrainCell(row, col),
                        "Streamed collision and terrain geometry diverged");
    }
    require(world.WorldOffset().X < initial_offset.X, "Negative world exploration did not stream");
    player.Position = gmath::Vec2{8050, 8050} - world.WorldOffset();
    world.StreamAroundPlayer();
    require(world.WorldOffset() == initial_offset, "Return trip did not recover original window");
    std::size_t index = 0;
    for (const auto &row : logic::Grid::Array)
        for (int cell : row)
            require(cell == first[index++], "Revisited world geometry changed");
    int solid = 0;
    for (int cell : first)
        solid += cell != 0;
    require(solid > int(first.size() / 4) && solid < int(first.size() * 3 / 4),
            "Generated world lacks shelter walls");
    // Every district edge offers several shared openings instead of one repeated exit.
    for (int district = -2; district <= 2; ++district) {
        int horizontal = 0, vertical = 0;
        for (int cell = 0; cell < 32; ++cell) {
            horizontal += !logic::Grid::TerrainCell(42, district * 32 - 1, cell) &&
                          !logic::Grid::TerrainCell(42, district * 32, cell);
            vertical += !logic::Grid::TerrainCell(42, cell, district * 32 - 1) &&
                        !logic::Grid::TerrainCell(42, cell, district * 32);
        }
        require(horizontal >= 4 && vertical >= 4, "District edge lacks alternate exits");
    }
    for (int i = 0; i < 64; ++i) {
        const auto cell = logic::Grid::GenerateGridEntry({150, 150});
        require(cell.Row >= 0 && cell.Row < logic::Grid::Rows && cell.Column >= 0 &&
                    cell.Column < logic::Grid::Columns && !logic::Grid::Array[cell.Row][cell.Column],
                "Edge wander target escaped streamed raster");
    }
    open_map();
}

void projectile_radius_and_motion() {
    {
        auto matching = fixture();
        auto &target = matching->GetEnemies()[0];
        target.Position = {575.0001f, 550};
        matching->UpdateEnemies(0, false);
        target.Position += gmath::Vec2{599.9998f, 0};
        matching->AddBolt({550, 550}, {600, 0});
        matching->UpdateBolts(1);
        require(!target.IsActive, "Co-moving bullet tunneled through enemy at tiny relative speed");
    }
    auto world = fixture();
    auto &enemy = world->GetEnemies()[0];
    enemy.Position = {850, 574};
    world->AddBolt({550, 550}, {600, 0});
    world->UpdateBolts(1);
    require(!enemy.IsActive, "Visible bullet edge passed through enemy");
    enemy.Reset();
    enemy.Position = {850, 576};
    world->AddBolt({550, 550}, {600, 0});
    world->UpdateBolts(1);
    require(enemy.IsActive, "Bullet outside combined radii caused a hit");
    enemy.Position = {850, 450};
    world->UpdateEnemies(0, false); // Snapshot start poses for this physics step.
    enemy.Position = {850, 650};
    world->AddBolt({550, 550}, {600, 0});
    world->UpdateBolts(1);
    require(!enemy.IsActive, "Enemy crossing bullet trajectory was missed");
}
void debug_menu_and_teleport() {
    kinetra::DebugMenu menu;
    bool enabled = false;
    std::array<kinetra::DebugOption, 1> options{{{"TEST", &enabled}}};
    for (const auto viewport : {kinetra::Vec2{1280, 800}, kinetra::Vec2{200, 150}}) {
        kinetra::Input input;
        menu.visible = true;
        const float scale = menu.scale(viewport);
        input.mouse = {viewport.x - 180 * scale, 36 * scale};
        input.clicked = true;
        const bool before = enabled;
        require(menu.update(input, viewport, options) && enabled != before,
                "Debug menu resize/picking failed");
        input.clicked = false;
        input.mouse_down = true;
        require(menu.update(input, viewport, options) && enabled != before,
                "Held click toggled option repeatedly");
    }
    dotio::Application application(42);
    kinetra::Camera2D camera;
    auto &player = application.world().GetPlayer();
    for (auto &enemy : application.world().GetEnemies())
        enemy.IsActive = false;
    open_map();
    const auto original = player.Position;
    dotio::Commands commands;
    commands.teleport = true;
    commands.teleport_target = dotio::to_world({8150, 8050});
    application.advance(0, commands, camera);
    require(player.Position == original, "Teleport accepted outside debug mode");
    open_map();
    application.debug = true;
    application.ghost = true;
    application.world().GetGhost().Position = player.Position;
    application.paused = true;
    player.Velocity = {4, 5};
    application.advance(0, commands, camera);
    require(player.Position == gmath::Vec2{8150, 8050} && player.Velocity == gmath::Vec2{},
            "Paused debug teleport failed");
    logic::Grid::Array[80][82] = 1;
    commands.teleport_target = dotio::to_world({8190, 8050});
    application.advance(0, commands, camera);
    require(player.Position == gmath::Vec2{8150, 8050}, "Teleport accepted circle overlapping wall");
    commands.teleport_target = dotio::to_world({100000000, 550});
    application.advance(0, commands, camera);
    require(player.Position == gmath::Vec2{8150, 8050},
            "Teleport accepted outside supported coordinate range");
    // A distant Ghost click can cross the current local raster while paused.
    bool teleported = false;
    const auto offset = application.world().WorldOffset();
    for (int row = 75; row < 85 && !teleported; ++row)
        for (int col = -20; col < -1 && !teleported; ++col) {
            const gmath::Vec2 target{(col + 0.5f) * 100, (row + 0.5f) * 100};
            if (!application.teleport_clear(target, player.Radius))
                continue;
            commands.teleport_target = dotio::to_world(target);
            application.world().GetGhost().Position = target;
            application.advance(0, commands, camera);
            require(player.Position + application.world().WorldOffset() == target + offset,
                    "Distant paused teleport lost absolute coordinates");
            teleported = true;
        }
    require(teleported, "No free distant Ghost destination found");
    const auto closed = application.debug_options();
    require(closed[1].label == "DEBUG MODE" && closed[2].label == "GHOST CAMERA" &&
                closed[2].enabled == &application.ghost,
            "Ghost Camera must directly follow Debug Mode");
    application.paths[0] = true;
    const auto expanded = application.debug_options();
    require(expanded.size() == closed.size() + 4 && !application.paths[1] && !application.paths[2],
            "Paths expansion enabled child filters or omitted children");
    open_map();
}
void independent_debug_camera() {
    dotio::Application application(42);
    for (auto &enemy : application.world().GetEnemies())
        enemy.IsActive = false;
    kinetra::Camera2D camera;
    auto &player = application.world().GetPlayer();
    const auto start = player.Position;
    dotio::Commands commands;
    commands.camera_pan = {1, 0};
    application.advance(1.0 / 60, commands, camera);
    require(player.Position == start && !application.ghost, "Arrows moved player outside debug");
    application.debug = true;
    application.ghost = true;
    application.world().GetGhost().Position = player.Position;
    application.paused = true;
    application.advance(1.0 / 60, commands, camera);
    require(application.ghost && camera.position.x > dotio::to_world(start).x && player.Position == start,
            "Debug arrows did not detach camera while paused");
    const auto detached = camera.position;
    commands = {};
    commands.direction = {1, 0};
    application.paused = false;
    application.advance(1.0 / 60, commands, camera);
    require(player.Position == start && camera.position.x > detached.x,
            "WASD stopped controlling player with detached camera");
    application.debug = false;
    application.advance(0, {}, camera);
    require(!application.ghost && camera.position.x == dotio::to_world(player.Position).x,
            "Disabling debug did not restore player camera");
}

void application_commands() {
    dotio::Application application;
    kinetra::Camera2D camera;
    auto &player = application.world().GetPlayer();
    const auto start = player.Position;
    dotio::Commands commands;
    require(!application.help && !application.menu.visible, "Dotio menus must start hidden");
    commands.help = true;
    application.advance(0, commands, camera);
    require(application.help, "H did not show Dotio help");
    application.advance(0, commands, camera);
    require(!application.help, "H did not hide Dotio help");
    commands = {};
    commands.pause = true;
    commands.direction = {1, 0};
    application.advance(1, commands, camera);
    require(application.paused && player.Position == start, "Pause advanced game");
    commands = {};
    commands.debug = true;
    commands.ghost = true;
    commands.camera_pan = {1, 0};
    application.advance(0.1, commands, camera);
    require(application.ghost && player.Position == start && application.world().GetGhost().Position != start,
            "Ghost must move while player is paused");
    commands = {};
    commands.pause = true;
    commands.ghost = true;
    application.advance(0, commands, camera);
    for (int frame = 0; frame < 180; ++frame) {
        commands = {};
        commands.direction = {frame % 60 < 30 ? 1.f : -1.f, 0};
        commands.shoot = true;
        commands.aim = dotio::to_world(player.Position + gmath::Vec2{500, 0});
        application.advance(1.0 / 60, commands, camera);
        require(std::isfinite(player.Position.X) && std::isfinite(player.Position.Y),
                "Nonfinite player pose");
        for (auto &enemy : application.world().GetEnemies())
            if (enemy.IsActive)
                require(std::isfinite(enemy.Position.X) && std::isfinite(enemy.Position.Y) &&
                            std::isfinite(enemy.Velocity.X) && std::isfinite(enemy.Velocity.Y),
                        "Nonfinite enemy state");
    }
}
void fuzzy_population() {
    require(logic::Fuzzy::SpawnRate(0) == 1 && logic::Fuzzy::SpawnRate(3) == 0.25f &&
                logic::Fuzzy::SpawnRate(5) == 0 && logic::Fuzzy::SpawnRate(100) == 0,
            "Fuzzy spawn rules lost sparse/occupied/crowded outputs");
    float previous = 1;
    for (int step = 0; step <= 100; ++step) {
        const float rate = logic::Fuzzy::SpawnRate(step / 10.f);
        require(rate >= 0 && rate <= previous && previous - rate < 0.1f,
                "Fuzzy spawn rate is discontinuous or increases with crowding");
        previous = rate;
    }
    const auto population = [](float dt, float resources, gmath::Vec2 active = {1120, 700},
                               logic::Difficulty difficulty = logic::Difficulty::easy, float duration = 20) {
        auto world = std::make_unique<logic::World>();
        world->Initialize(42, difficulty);
        auto &player = world->GetPlayer();
        const auto center = player.Position;
        const gmath::Vec2 retained{8000, 8000};
        world->StreamEnemies(center, active / 1.75f, active * (9.f / 1.75f), retained);
        open_map();
        player.Health = player.Mana = resources;
        for (int frame = 0; frame < duration / dt; ++frame)
            world->StreamEnemies(center, active / 1.75f, active * (9.f / 1.75f), retained, dt);
        std::vector<gmath::Vec2> positions;
        for (const auto &enemy : world->GetEnemies())
            if (enemy.IsActive) {
                const auto delta = enemy.Position - center;
                require(std::abs(delta.X) - enemy.Radius > active.X / 1.75f * 1.1f ||
                            std::abs(delta.Y) - enemy.Radius > active.Y / 1.75f * 1.1f,
                        "Spawned enemy body overlaps 1.1x maximum normal player view");
                for (const auto position : positions)
                    require((enemy.Position - position).GetLength() >=
                                logic::settings_for(difficulty).spawn_spacing,
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
            require(std::abs(delta.X) <= spawn_half.X && std::abs(delta.Y) <= spawn_half.Y,
                    "Spawn escaped its time-dependent placement rectangle");
        }
        require(!positions.empty() && local_count <= logic::settings_for(difficulty).active_spawn_limit,
                "Spawn admission did not bound the local population");
        if (duration >= 120)
            require(expanded, "Survival did not place enemies beyond the initial spawn range");
        return positions;
    };
    require(population(0.125f, 100) == population(0.125f, 1), "Spawn population depends on health or mana");
    require(std::abs(int(population(0.125f, 100).size()) - int(population(0.03125f, 100).size())) <= 3,
            "Spawn pacing changes substantially with frame rate");

    const auto easy = population(0.125f, 100).size();
    const auto medium = population(0.125f, 100, {1120, 700}, logic::Difficulty::medium).size();
    const auto hard = population(0.125f, 100, {1120, 700}, logic::Difficulty::hard).size();
    require(easy < medium && medium < hard, "Difficulty did not increase actual spawn population");
    population(0.125f, 100, {1120, 700}, logic::Difficulty::easy, 120);
    population(0.125f, 100, {700, 1120}); // Portrait viewport uses the same proportional exclusion.

    auto world = std::make_unique<logic::World>();
    world->Initialize(42, logic::Difficulty::easy);
    const auto center = world->GetPlayer().Position;
    for (int frame = 0; frame < 1000; ++frame)
        world->StreamEnemies(center, {640, 400}, {5760, 3600}, {8000, 5000}, 0);
    for (const auto &enemy : world->GetEnemies())
        require(!enemy.IsActive, "Paused/render-only frames created enemies");
    for (int i = 0; i < 5; ++i) {
        auto &enemy = world->GetEnemies()[i];
        enemy.Position = center + gmath::Vec2{float(100 + i * 50), 0};
        enemy.IsActive = true;
    }
    for (int second = 0; second < 120; ++second)
        world->StreamEnemies(center, {640, 400}, {5760, 3600}, {8000, 5000}, 1);
    int count = 0;
    for (const auto &enemy : world->GetEnemies())
        count += enemy.IsActive;
    require(count == 5, "Crowded player area continued receiving new enemies");
}

void survival_range_and_hit_points() {
    for (auto difficulty : {logic::Difficulty::easy, logic::Difficulty::medium, logic::Difficulty::hard}) {
        auto world = std::make_unique<logic::World>();
        world->Initialize(42, difficulty);
        open_map();
        auto &enemy = world->GetEnemies()[0];
        enemy.Reset();
        enemy.Position = {850, 550};
        world->GetPlayer().Position = {550, 550};
        world->GetPlayer().Health = 50;
        const int kills = world->GetPlayer().Kills;
        const int hits = logic::settings_for(difficulty).enemy_hits;
        for (int shot = 1; shot <= hits; ++shot) {
            world->AddBolt({550, 550}, {600, 0});
            world->UpdateBolts(1);
            require(enemy.IsActive == (shot < hits) && enemy.HitsRemaining == hits - shot,
                    "Difficulty hit points do not consume exactly one hit per bolt");
            require(world->GetPlayer().Kills == kills + (shot == hits),
                    "A nonlethal hit awarded a kill or lethal hit lost reward");
        }
        enemy.Reset();
        require(enemy.HitsRemaining == hits, "Reused enemy retained previous damage");
        const gmath::Vec2 normal{640, 400}, active{5760, 3600}, retained{8000, 5000};
        const auto initial = world->SpawnHalf(normal, active);
        require(initial == normal * 1.75f, "Initial spawn range changed");
        world->StreamEnemies(world->GetPlayer().Position, normal, active, retained, 150);
        const auto middle = world->SpawnHalf(normal, active);
        require(middle == (initial + active) * 0.5f, "Spawn range did not grow linearly");
        world->StreamEnemies(world->GetPlayer().Position, normal, active, retained, 0);
        require(world->SpawnHalf(normal, active) == middle, "Paused time expands spawn range");
        world->StreamEnemies(world->GetPlayer().Position, normal, active, retained, 150);
        require(world->SpawnHalf(normal, active) == active, "Five minutes did not reach active debug range");
        world->StreamEnemies(world->GetPlayer().Position, normal, active, retained, 1000);
        require(world->SpawnHalf(normal, active) == active, "Spawn range exceeded debug activity bounds");
        world->Reset();
        require(world->survival_seconds() == 0 && world->SpawnHalf(normal, active) == initial,
                "New life retained survival progression");
    }
}

void difficulty_behavior_and_counts() {
    struct Result {
        int shots;
        float speed, acceleration, cooldown;
        bool pursuing;
    };
    const auto encounter = [](logic::Difficulty difficulty) {
        auto world = std::make_unique<logic::World>();
        world->Initialize(42, difficulty);
        open_map();
        auto &enemy = world->GetEnemies()[0];
        enemy.Position = world->GetPlayer().Position + gmath::Vec2{700, 0};
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
        return Result{shots, speed, acceleration, cooldown, !enemy.PursueCountdownPassed()};
    };
    const auto easy = encounter(logic::Difficulty::easy), medium = encounter(logic::Difficulty::medium),
               hard = encounter(logic::Difficulty::hard);
    require(easy.shots < hard.shots && easy.speed < medium.speed && medium.speed < hard.speed &&
                easy.acceleration < medium.acceleration && medium.acceleration < hard.acceleration &&
                easy.cooldown > medium.cooldown && medium.cooldown > hard.cooldown && !easy.pursuing &&
                hard.pursuing,
            "Difficulty did not change actual movement, firing or pursuit behavior");
    dotio::Application application(42);
    require(application.world().difficulty_settings().name == "MEDIUM", "Default mode is not Medium");
    require(application.agent_counts().loaded == 0, "Unused pool slots counted as map agents");
    auto &enemies = application.world().GetEnemies();
    enemies[0].IsActive = true;
    enemies[0].Frozen = false;
    enemies[1].IsActive = true;
    enemies[1].Frozen = true;
    require(application.agent_counts().loaded == 2 && application.agent_counts().active == 1,
            "Statistics omitted frozen residents or counted them as moving agents");
    enemies[0].IsActive = false;
    require(application.agent_counts().loaded == 1 && application.agent_counts().active == 0,
            "Statistics retained a retired enemy");
    application.world().Reset();
    require(application.agent_counts().loaded == 0 &&
                application.world().difficulty_settings().name == "MEDIUM",
            "Death/reset spawned the whole pool or lost difficulty");
}

void bounded_enemy_lifecycle() {
    open_map();
    auto world = fixture();
    auto &enemy = world->GetEnemies()[0];
    const auto before = enemy.Position;
    world->StreamEnemies({550, 550}, {50, 50}, {100, 100}, {1000, 1000});
    require(enemy.IsActive && enemy.Frozen, "Enemy beyond normal view did not freeze");
    const auto cooldown = enemy.GetCooldown();
    enemy.Tick();
    world->UpdateEnemies(1, false);
    require(enemy.Position == before && enemy.GetCooldown() == cooldown,
            "Frozen enemy advanced motion or AI timer");
    enemy.LastPartition[0] = {5, 8};
    world->EnemyPartitionMap[{5, 8}].insert(&enemy);
    world->StreamEnemies({10000, 10000}, {50, 50}, {100, 100}, {1000, 1000});
    require(!enemy.IsActive && enemy.Path.empty() && !world->EnemyPartitionMap[{5, 8}].contains(&enemy),
            "Distant enemy retained active state, route or partition reference");
    dotio::Application application(42);
    kinetra::Camera2D camera;
    dotio::Commands commands;
    commands.zoom_steps = -100;
    application.advance(0, commands, camera);
    require(camera.pixels_per_unit == 25, "Normal Dotio zoom escaped limit");
    const auto player_center = application.world().GetPlayer().Position;
    auto &population = application.world().GetEnemies();
    for (int i = 0; i < 3; ++i) {
        population[i].IsActive = true;
        population[i].Frozen = true;
        population[i].Position =
            player_center +
            gmath::Vec2{i == 0 ? 2000.f : camera.viewport.x * 4.5f + (i == 1 ? -1.f : 1.f), 0};
    }
    application.advance(0, {}, camera);
    require(!population[0].Frozen && !population[1].Frozen && population[2].Frozen,
            "Agents did not stay active until 90 percent of maximum debug view in normal mode");
    population[2].Path.push({80, 80});
    application.debug = true;
    application.ghost = true;
    application.world().GetGhost().Position =
        application.world().GetPlayer().Position + gmath::Vec2{5000, 5000};
    application.advance(0, commands, camera);
    for (const auto &agent : application.world().GetEnemies())
        if (agent.IsActive) {
            const auto delta = agent.Position - application.world().GetPlayer().Position;
            require(agent.Frozen == (std::abs(delta.X) > camera.viewport.x * 4.5f ||
                                     std::abs(delta.Y) > camera.viewport.y * 4.5f),
                    "Ghost changed the player-centered activity window");
            if (agent.Frozen)
                require(agent.Path.empty(), "Frozen agent retained a path");
        }
    require(camera.pixels_per_unit == 2.5f, "Debug Dotio zoom failed to extend view");
}

void legacy_math_contract() {
    const gmath::Vec2 vector{3, 4};
    require(vector.GetLength() == 5 && vector.DotProduct({2, -1}) == 2,
            "Legacy vector units or dot product changed");
    require(vector.Determinant({2, -1}) == -11, "Legacy cross product changed");
    require(gmath::Vec2{}.GetNormalized() == gmath::Vec2{}, "Legacy zero normalization changed");
    auto normalized = vector.GetNormalized();
    require(std::abs(normalized.X - 0.6f) < 0.000001f && std::abs(normalized.Y - 0.8f) < 0.000001f,
            "Legacy normalization changed");
    auto rotated = gmath::Vec2{1, 0}.Rotate(std::numbers::pi_v<float> / 2);
    require(std::abs(rotated.X) < 0.000001f && std::abs(rotated.Y - 1) < 0.000001f,
            "Legacy rotation orientation changed");
    require(std::abs(gmath::Vec2{1, 0}.SignedAngleDifference({0, 1}) - std::numbers::pi_v<float> / 2) <
                0.000001f,
            "Legacy signed angle changed");
    require(dotio::from_world(dotio::to_world({250, -125})) == gmath::Vec2{250, -125},
            "Legacy/world coordinate conversion changed");
}

} // namespace
int main() {
    try {
        legacy_math_contract();
        open_map();
        generated_arena();
        streamed_world_and_objects();
        geometry_and_routes();
        incremental_residency_graph();
        projectile_radius_and_motion();
        debug_menu_and_teleport();
        states_and_fuzzy();
        recovery_kamikaze_health_gate();
        combat_and_reset();
        independent_debug_camera();
        application_commands();
        bounded_enemy_lifecycle();
        fuzzy_population();
        survival_range_and_hit_points();
        difficulty_behavior_and_counts();
        std::cout << "PASS Dotio geometry, routes, fuzzy states, combat, reset, menu and session\n";
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}

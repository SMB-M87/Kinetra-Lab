#include "world.h"
#include "fuzzy.h"
#include "physics_adapter.h"
#include "portable.h"
#include <stdexcept>

namespace logic {
float World::GetTickRate() const {
    return _tickRate;
}

void World::Initialize(std::uint32_t seed, Difficulty difficulty) {
    settings_for(difficulty); // Validate before mutating the world.
    difficulty_ = difficulty;
    seed_ = seed;
    spawn_credit_ = 0;
    survival_seconds_ = 0;
    population_random_.seed(seed);
    offset_row_ = offset_column_ = 0;
    resident_cells_ = {-1, -1, -1, -1};
    Grid::Generate(seed);
    PopulateObjects();
    ExtractGraph();

    SetPlayer(logic::Player(this, {Grid::CellSize * 80.5f, Grid::CellSize * 80.5f}, 0.0f));
    SetGhost(logic::Player(this, logic::Grid::GeneratePosition(), 0.0f));

    for (std::size_t i = 0; i < GetEnemies().GetCapacity(); i++) {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<float> dis(2.0f, 10.0f);
        float fear = dis(gen);
        AddEnemy(logic::Grid::GeneratePosition(), GetPlayer().Position, fear, true);
        _enemies[i].Initialize();
        // Slots start dormant; player-centered streaming alone chooses spawn density.
        _enemies[i].IsActive = false;
        _enemies[i].Frozen = true;
    }
}

std::optional<std::pair<std::size_t, float>> World::FirstObjectHit(gmath::Segment segment,
                                                                   float radius) const {
    std::optional<std::pair<std::size_t, float>> nearest;
    for (std::size_t i = 0; i < props_.size(); ++i)
        if (const auto hit = kinetra::segment_circle(dotio::physics_vector(segment.Start),
                                                     dotio::physics_vector(segment.End),
                                                     props_[i].body.position, props_[i].body.radius + radius);
            hit && (!nearest || *hit < nearest->second))
            nearest = {{i, *hit}};
    return nearest;
}

void World::PopulateObjects() {
    // One crate in each loaded room; no AI or navigation state belongs to these bodies.
    for (int row = 6; row < Grid::Rows - 6; ++row)
        for (int col = 6; col < Grid::Columns - 6; ++col) {
            const int absolute_row = row + offset_row_, absolute_col = col + offset_column_;
            if ((absolute_row % 32 + 32) % 32 != 18 || (absolute_col % 32 + 32) % 32 != 18)
                continue;
            if (std::any_of(props_.begin(), props_.end(), [&](const Prop &prop) {
                    return prop.row == absolute_row && prop.column == absolute_col;
                }))
                continue;
            if (Grid::Array[row][col])
                continue;
            kinetra::Body body;
            body.position = {(col + 0.5f) * Grid::CellSize, (row + 0.5f) * Grid::CellSize};
            body.mass = 25;
            body.radius = 24;
            body.friction = 0.25f;
            body.restitution = 0.3f;
            props_.push_back({absolute_row, absolute_col, body});
        }
}

gmath::Vec2 World::StreamAroundPlayer() {
    const auto shift_axis = [](float position) {
        const int cell = int(std::floor(position / Grid::CellSize));
        return cell < 48 || cell > 106 ? int(std::floor((cell - 77) / 16.f)) * 16 : 0;
    };
    const int columns = shift_axis(_player.Position.X), rows = shift_axis(_player.Position.Y);
    if (!columns && !rows)
        return {};
    const gmath::Vec2 shift{columns * Grid::CellSize, rows * Grid::CellSize};
    offset_column_ += columns;
    offset_row_ += rows;
    Grid::Generate(seed_, offset_row_, offset_column_);
    resident_cells_ = {-1, -1, -1, -1};
    _player.Position -= shift;
    _ghost.Position -= shift;
    EnemyPartitionMap.clear();
    for (auto &enemy : GetEnemies()) {
        enemy.Position -= shift;
        enemy.Target -= shift;
        enemy.Path.clear();
        for (auto &partition : enemy.LastPartition)
            partition = {-1, -1};
        if (enemy.Position.X < Grid::CellSize * 3 || enemy.Position.Y < Grid::CellSize * 3 ||
            enemy.Position.X > Grid::CellSize * (Grid::Columns - 3) ||
            enemy.Position.Y > Grid::CellSize * (Grid::Rows - 3)) {
            enemy.IsActive = false;
            enemy.Path.clear();
        }
    }
    for (auto &arrow : GetArrows())
        arrow.Position -= shift;
    for (auto &bolt : GetBolts())
        bolt.Position -= shift;
    for (auto &prop : props_)
        prop.body.position -= dotio::physics_vector(shift);
    std::erase_if(props_, [](const Prop &prop) {
        return prop.body.position.x < Grid::CellSize * 3 || prop.body.position.y < Grid::CellSize * 3 ||
               prop.body.position.x > Grid::CellSize * (Grid::Columns - 3) ||
               prop.body.position.y > Grid::CellSize * (Grid::Rows - 3);
    });
    PopulateObjects();
    ExtractGraph();
    enemies_moved_ = false;
    return shift;
}

void World::StreamEnemies(gmath::Position center, gmath::Vec2 normal_half, gmath::Vec2 active_half,
                          gmath::Vec2 retained_half, float elapsed) {
    if (!std::isfinite(elapsed) || elapsed < 0)
        throw std::invalid_argument("Spawn elapsed time must be finite and nonnegative");
    const auto outside = [&](gmath::Position position, gmath::Vec2 half) {
        return std::abs(position.X - center.X) > half.X || std::abs(position.Y - center.Y) > half.Y;
    };
    // Trim the expensive graph at cell boundaries, independently of camera pan/zoom.
    const auto limit = [](float value, int maximum) {
        return std::clamp(int(std::floor(value / Grid::CellSize)), 0, maximum - 1);
    };
    const std::array<int, 4> region{
        limit(center.Y - retained_half.Y, Grid::Rows), limit(center.X - retained_half.X, Grid::Columns),
        limit(center.Y + retained_half.Y, Grid::Rows), limit(center.X + retained_half.X, Grid::Columns)};
    if (region != resident_cells_) {
        // Only the entering/leaving strips change occupancy. Rebuild adjacent vertices
        // too: diagonal links depend on the two orthogonal clearance cells.
        std::array<bool, Grid::Rows * Grid::Columns> dirty{};
        const auto inside = [](int row, int column, const std::array<int, 4> &bounds) {
            return row >= bounds[0] && column >= bounds[1] && row <= bounds[2] && column <= bounds[3];
        };
        for (int row = 0; row < Grid::Rows; ++row)
            for (int col = 0; col < Grid::Columns; ++col) {
                const bool admitted = inside(row, col, region);
                if (resident_cells_[0] >= 0 && admitted == inside(row, col, resident_cells_))
                    continue;
                const int cell = admitted ? TerrainCell(row, col) : 1;
                if (cell == Grid::Array[row][col])
                    continue;
                Grid::Array[row][col] = cell;
                for (int y = std::max(0, row - 1); y <= std::min(Grid::Rows - 1, row + 1); ++y)
                    for (int x = std::max(0, col - 1); x <= std::min(Grid::Columns - 1, col + 1); ++x)
                        dirty[y * Grid::Columns + x] = true;
            }
        resident_cells_ = region;
        for (int row = 0; row < Grid::Rows; ++row)
            for (int col = 0; col < Grid::Columns; ++col)
                if (dirty[row * Grid::Columns + col])
                    RefreshGraphCell(row, col);
        for (auto &enemy : GetEnemies())
            enemy.Path.clear();
    }
    std::erase_if(props_, [&](const Prop &prop) {
        return outside(dotio::legacy_vector(prop.body.position), retained_half);
    });
    // Distance memberships count nearby threats smoothly, independent of player resources.
    // An admission limit prevents new spawns when the near-player spawn area is already busy.
    const auto &settings = difficulty_settings();
    // Survival expands placement, while admission and fuzzy pressure stay local.
    survival_seconds_ += elapsed;
    const auto spawn_half = SpawnHalf(normal_half, active_half);
    const auto admission_half = normal_half * 1.75f;
    int nearby_count = 0;
    float nearby_load = 0;
    for (const auto &enemy : GetEnemies())
        if (enemy.IsActive && !outside(enemy.Position, retained_half)) {
            nearby_count += !outside(enemy.Position, admission_half);
            const float distance = (enemy.Position - _player.Position).GetLength();
            nearby_load += std::clamp((1200 - distance) / 700, 0.f, 1.f);
        }
    const float spawn_rate =
        nearby_count < settings.active_spawn_limit
            ? Fuzzy::SpawnRate(nearby_load * settings.crowd_load_multiplier) * settings.spawn_rate_multiplier
            : 0;
    spawn_credit_ = spawn_rate > 0 ? std::min(1.f, spawn_credit_ + elapsed * spawn_rate) : 0;
    bool spawn_due = elapsed > 0 && spawn_credit_ >= 1;
    if (spawn_due)
        spawn_credit_ = 0; // No catch-up burst after pauses, blocked attempts or crowded periods.
    int attempts = 0;
    for (auto &enemy : GetEnemies()) {
        if (enemy.IsActive && outside(enemy.Position, retained_half)) {
            for (auto cell : enemy.LastPartition)
                if (auto found = EnemyPartitionMap.find(cell); found != EnemyPartitionMap.end())
                    found->second.erase(&enemy);
            enemy.IsActive = false;
            enemy.Path.clear();
            enemy.Frozen = true;
            continue;
        }
        if (!enemy.IsActive && spawn_due && attempts++ < 8) {
            const auto unit = [&]() { return float(population_random_() >> 8) / 16777216.f; };
            const gmath::Vec2 position{center.X + (unit() * 2 - 1) * spawn_half.X,
                                       center.Y + (unit() * 2 - 1) * spawn_half.Y};
            gmath::GridEntry cell;
            if (!gmath::GridEntry::FromWorld(position, Grid::Origin, Grid::CellSize, Grid::GridSize, cell) ||
                Grid::Array[cell.Row][cell.Column])
                continue;
            const gmath::Vec2 spawn{(cell.Column + 0.5f) * Grid::CellSize,
                                    (cell.Row + 0.5f) * Grid::CellSize};
            const bool occupied =
                std::any_of(GetEnemies().GetInstances(),
                            GetEnemies().GetInstances() + GetEnemies().GetCapacity(),
                            [&](const Enemy &other) {
                                return other.IsActive &&
                                       (other.Position - spawn).GetLength() < settings.spawn_spacing;
                            }) ||
                std::any_of(props_.begin(), props_.end(), [&](const Prop &prop) {
                    return (dotio::legacy_vector(prop.body.position) - spawn).GetLength() < 50;
                });
            const auto delta = spawn - _player.Position;
            // Keep the complete body outside 1.1x maximum normal view, at every zoom.
            const auto excluded_half = normal_half * 1.1f;
            const bool offscreen = std::abs(delta.X) - enemy.Radius > excluded_half.X ||
                                   std::abs(delta.Y) - enemy.Radius > excluded_half.Y;
            if (!occupied && !outside(spawn, spawn_half) && !outside(spawn, active_half) && offscreen) {
                spawn_due = false;
                enemy.Position = spawn;
                enemy.Target = _player.Position;
                enemy.Velocity = {};
                enemy.IsActive = true;
                enemy.Reset();
                for (auto &cell : enemy.LastPartition) {
                    if (auto found = EnemyPartitionMap.find(cell); found != EnemyPartitionMap.end())
                        found->second.erase(&enemy);
                    cell = {-1, -1};
                }
            }
        }
        enemy.Frozen = outside(enemy.Position, active_half);
        if (enemy.Frozen) {
            enemy.Path.clear();
            enemy.Velocity = {};
        }
    }
}

void World::UpdateObjects(float delta) {
    for (auto &prop : props_)
        kinetra::integrate(prop.body, delta, 0.015f);
    // Iterative contacts let crates transfer momentum and settle against walls.
    for (int pass = 0; pass < 6; ++pass) {
        for (std::size_t i = 0; i < props_.size(); ++i) {
            auto &body = props_[i].body;
            auto player = dotio::physics_body(_player);
            if (const auto hit = kinetra::collide(player, body)) {
                kinetra::resolve_contact(player, body, *hit);
                dotio::apply_body(_player, player);
            }
            for (auto &enemy : GetEnemies()) {
                if (!enemy.IsActive || enemy.Frozen ||
                    (enemy.Position - dotio::legacy_vector(body.position)).GetLength() > 60)
                    continue;
                auto actor = dotio::physics_body(enemy);
                if (const auto hit = kinetra::collide(actor, body)) {
                    kinetra::resolve_contact(actor, body, *hit);
                    dotio::apply_body(enemy, actor);
                }
            }
            for (std::size_t j = i + 1; j < props_.size(); ++j)
                if (const auto hit = kinetra::collide(body, props_[j].body))
                    kinetra::resolve_contact(body, props_[j].body, *hit);
            kinetra::collide_grid(body, Grid::CollisionView());
        }
        Grid::HandleGridCollision(_player, 0, &Grid::Array[0][0]);
    }
}

void World::Reset() {
    spawn_credit_ = 0;
    survival_seconds_ = 0;
    enemies_moved_ = false;
    EnemyPartitionMap.clear();
    for (auto &arrow : GetArrows())
        arrow.IsActive = false;
    for (auto &bolt : GetBolts())
        bolt.IsActive = false;
    GetPlayer().Velocity = {};
    GetPlayer().Cooldown = 0;

    GetPlayer().Position = logic::Grid::GeneratePosition();
    GetPlayer().Health = GetPlayer().MaxHealth;
    GetPlayer().Mana = GetPlayer().MaxMana;
    GetPlayer().Kills = 1;
    // Restart through the same offscreen population controller; resetting the
    // whole fixed pool must not materialize hundreds of enemies near the player.
    for (auto &enemy : GetEnemies()) {
        if (enemy.IsActive)
            enemy.Reset();
        enemy.IsActive = false;
        enemy.Frozen = true;
        enemy.Path.clear();
        enemy.Velocity = {};
        for (auto &partition : enemy.LastPartition)
            partition = {-1, -1};
    }
}

void World::ExtractGraph() {
    _graph.vertexMap.clear();
    for (int row = 0; row < Grid::Rows; ++row)
        for (int column = 0; column < Grid::Columns; ++column)
            RefreshGraphCell(row, column);
}

void World::RefreshGraphCell(int row, int column) {
    const gmath::GridEntry cell{row, column};
    if (Grid::Array[row][column] != 0) {
        _graph.vertexMap.erase(cell);
        return;
    }
    auto &vertex = _graph.vertexMap.insert_or_assign(cell, Vertex(cell)).first->second;
    if (row > 0 && logic::Grid::Array[row - 1][column] == 0)
        vertex.AddAdjacent(vertex, {row - 1, column});

    if (row < logic::Grid::GridSize.Rows - 1 && logic::Grid::Array[row + 1][column] == 0)
        vertex.AddAdjacent(vertex, {row + 1, column});

    if (column > 0 && logic::Grid::Array[row][column - 1] == 0)
        vertex.AddAdjacent(vertex, {row, column - 1});

    if (column < logic::Grid::GridSize.Columns - 1 && logic::Grid::Array[row][column + 1] == 0)
        vertex.AddAdjacent(vertex, {row, column + 1});

    if (row > 0 && column > 0 && logic::Grid::Array[row - 1][column - 1] == 0 &&
        logic::Grid::Array[row][column - 1] == 0 && logic::Grid::Array[row - 1][column] == 0)
        vertex.AddAdjacent(vertex, {row - 1, column - 1});

    if (row > 0 && column < logic::Grid::GridSize.Columns - 1 &&
        logic::Grid::Array[row - 1][column + 1] == 0 && logic::Grid::Array[row][column + 1] == 0 &&
        logic::Grid::Array[row - 1][column] == 0)
        vertex.AddAdjacent(vertex, {row - 1, column + 1});

    if (row < logic::Grid::GridSize.Rows - 1 && column > 0 && logic::Grid::Array[row + 1][column - 1] == 0 &&
        logic::Grid::Array[row][column - 1] == 0 && logic::Grid::Array[row + 1][column] == 0)
        vertex.AddAdjacent(vertex, {row + 1, column - 1});

    if (row < logic::Grid::GridSize.Rows - 1 && column < logic::Grid::GridSize.Columns - 1 &&
        logic::Grid::Array[row + 1][column + 1] == 0 && logic::Grid::Array[row][column + 1] == 0 &&
        logic::Grid::Array[row + 1][column] == 0)
        vertex.AddAdjacent(vertex, {row + 1, column + 1});
}

void World::SetPlayer(logic::Player player) {
    _player = player;
}

logic::Player &World::GetPlayer() {
    return _player;
}

void World::SetGhost(logic::Player ghost) {
    _ghost = ghost;
}

logic::Player &World::GetGhost() {
    return _ghost;
}

void World::AddEnemy(const gmath::Position position, gmath::Position target, const float fear, bool) {
    _enemies += Enemy(this, position, target, 0.0f, fear);
}

void World::AddArrow(const gmath::Position position, const gmath::Vector2 velocity) {
    _arrows += Arrow(position, velocity);
}

void World::AddBolt(const gmath::Position position, const gmath::Vector2 velocity) {
    _bolts += Bolt(position, velocity);
}

void World::UpdateEnemies(const float delta, const bool debug) {
    enemies_moved_ = true;
    std::size_t index = 0;
    // Snapshot the entire population first: neighbor separation may move another
    // enemy before its own update, and that displacement also belongs to the sweep.
    for (const auto &enemy : GetEnemies())
        previous_enemy_positions_[index++] = enemy.Position;
    for (auto &enemy : GetEnemies()) {
        if (!enemy.IsActive || enemy.Frozen)
            continue;
        enemy.Target = GetPlayer().Position;
        enemy.TargetVelocity = GetPlayer().Velocity;
        enemy.Update(delta);
        enemy.Debug = debug;
        logic::Grid::HandleGridCollision(enemy, delta, reinterpret_cast<const int *>(logic::Grid::Array));
    }
}

// Sweep the actual movement segment, resolving only the earliest wall/body hit.
// This prevents vertical shots tunneling and bolts damaging enemies behind walls.
void World::UpdateArrows(const float delta, const bool debug) {
    for (auto &arrow : GetArrows()) {
        if (!arrow.IsActive)
            continue;
        const gmath::Segment segment{arrow.Position, arrow.Position + arrow.Velocity * delta};
        gmath::Point2 wall{}, hit{};
        const bool blocked = Grid::IsIntersectingAnyCell(segment, &Grid::Array[0][0], wall);
        const bool hits_player =
            Grid::SegementCircleIntersection(segment, GetPlayer().Position, GetPlayer().Radius, hit) & 1;
        const auto object = FirstObjectHit(segment, 0);
        const float object_distance = object ? object->second * (segment.End - segment.Start).GetLength()
                                             : std::numeric_limits<float>::infinity();
        if (object && (!blocked || object_distance < (wall - segment.Start).GetLength()) &&
            (!hits_player || object_distance < (hit - segment.Start).GetLength())) {
            const auto point = segment.Start + (segment.End - segment.Start) * object->second;
            kinetra::apply_body_impulse(props_[object->first].body,
                                        dotio::physics_vector(arrow.Velocity) * 0.2f,
                                        dotio::physics_vector(point));
            arrow.IsActive = false;
        } else if (hits_player &&
                   (!blocked || (hit - segment.Start).GetLength() < (wall - segment.Start).GetLength())) {
            arrow.IsActive = false;
            if (!debug)
                GetPlayer().Health -= 10;
            if (GetPlayer().Health <= 0) {
                Reset();
                return;
            }
        } else if (blocked) {
            arrow.IsActive = false;
        } else {
            arrow.Update(delta);
        }
    }
}

void World::UpdateBolts(const float delta) {
    for (auto &bolt : GetBolts()) {
        if (!bolt.IsActive)
            continue;
        const gmath::Segment segment{bolt.Position, bolt.Position + bolt.Velocity * delta};
        gmath::Point2 wall{};
        const bool blocked = Grid::IsIntersectingAnyCell(segment, &Grid::Array[0][0], wall);
        float nearest = blocked ? (wall - segment.Start).GetLength() : std::numeric_limits<float>::infinity();
        Enemy *victim = nullptr;
        std::size_t index = 0;
        for (auto &enemy : GetEnemies()) {
            const auto previous = enemies_moved_ ? previous_enemy_positions_[index] : enemy.Position;
            ++index;
            // Sweep relative motion, including the visible projectile radius. Convert
            // the relative hit parameter back to world distance for wall ordering.
            const gmath::Segment relative{segment.Start, segment.End - (enemy.Position - previous)};
            if (!enemy.IsActive)
                continue;
            const auto fraction = kinetra::segment_circle(
                dotio::physics_vector(relative.Start), dotio::physics_vector(relative.End),
                dotio::physics_vector(previous), enemy.Radius + Bolt::Radius);
            if (!fraction)
                continue;
            const float distance = *fraction * (segment.End - segment.Start).GetLength();
            if (distance < nearest) {
                nearest = distance;
                victim = &enemy;
            }
        }
        const auto object = FirstObjectHit(segment, Bolt::Radius);
        if (object && object->second * (segment.End - segment.Start).GetLength() < nearest) {
            const auto point = segment.Start + (segment.End - segment.Start) * object->second;
            kinetra::apply_body_impulse(props_[object->first].body,
                                        dotio::physics_vector(bolt.Velocity) * 0.5f,
                                        dotio::physics_vector(point));
            bolt.IsActive = false;
        } else if (victim) {
            bolt.IsActive = false;
            if (--victim->HitsRemaining > 0)
                continue;
            auto &player = GetPlayer();
            const float health = victim->IsRecovering() ? 40.f : victim->IsKamikazing() ? 20.f : 10.f;
            player.Health = std::min(player.MaxHealth, player.Health + health);
            if (victim->IsRecovering() || victim->IsKamikazing())
                player.Mana = std::min(player.MaxMana, player.Mana + health);
            player.Kills = std::min(int(player.MaxBulletKills) + 1, player.Kills + 1);
            victim->IsActive = false;
        } else if (blocked) {
            bolt.IsActive = false;
        } else {
            bolt.Update(delta);
        }
    }
    enemies_moved_ = false;
}

void World::Draw(gamef::Renderer &renderer) {
    for (auto &enemy : GetEnemies()) {
        if (!enemy.IsActive)
            continue;
        enemy.Draw(renderer);
    }

    for (auto &arrow : GetArrows()) {
        if (!arrow.IsActive)
            continue;
        arrow.Draw(renderer);
    }

    for (auto &bolt : GetBolts()) {
        if (!bolt.IsActive)
            continue;
        bolt.Draw(renderer);
    }

    GetPlayer().Draw(renderer);
}

Tracker<Enemy> &World::GetEnemies() {
    return _enemies.GetTracker();
}

Tracker<Arrow> &World::GetArrows() {
    return _arrows.GetTracker();
}

Tracker<Bolt> &World::GetBolts() {
    return _bolts.GetTracker();
}

logic::Graph &World::GetGraph() {
    return _graph;
}
} // namespace logic

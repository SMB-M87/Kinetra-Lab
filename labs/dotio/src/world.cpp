#include "dotio/world.h"
#include "dotio/ai/fuzzy.h"
#include "dotio/entities/physics_adapter.h"
#include "dotio/math/random.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <random>
#include <stdexcept>

namespace dotio {
float World::get_tick_rate() const {
    return tickRate_;
}

namespace {
/// Second word of the seed sequence of World::wander_random(), so that stream differs from
/// the population stream that the same world seed starts.
constexpr std::uint32_t wander_stream = 0x57414E44; // "WAND"
/// Second word of the seed sequences of World::behavior_random() and of the placement stream.
constexpr std::uint32_t behavior_stream = 0x42485652;  // "BHVR"
constexpr std::uint32_t placement_stream = 0x504C4345; // "PLCE"
} // namespace

void World::initialize(std::uint32_t seed, Difficulty difficulty) {
    settings_ = settings_for(difficulty); // Throws before anything else in the world changes.
    enemy_tuning_ = {};
    difficulty_ = difficulty;
    seed_ = seed;
    spawn_credit_ = 0;
    survival_seconds_ = 0;
    population_random_.seed(seed);
    std::seed_seq wander_seed{seed, wander_stream};
    wander_random_.seed(wander_seed);
    std::seed_seq behavior_seed{seed, behavior_stream};
    behavior_random_.seed(behavior_seed);
    std::seed_seq placement_seed{seed, placement_stream};
    placement_random_.seed(placement_seed);
    offset_row_ = offset_column_ = 0;
    resident_cells_ = {-1, -1, -1, -1};
    grid_.generate(seed);
    populate_objects();

    set_player(dotio::Player(this, {Grid::cell_size * 80.5f, Grid::cell_size * 80.5f}, 0.0f));
    set_ghost(dotio::Player(this, grid_.generate_position(placement_random_), 0.0f));

    for (std::size_t i = 0; i < get_enemies().get_capacity(); i++) {
        const float fear =
            dotio::math::uniform(behavior_random_, Enemy::spawn_fear_min, Enemy::spawn_fear_max);
        add_enemy(grid_.generate_position(placement_random_), get_player().Position, fear);
        enemies_[i].initialize();
        // Slots start dormant; player-centered streaming alone chooses spawn density.
        enemies_[i].IsActive = false;
        enemies_[i].Frozen = true;
    }
}

std::optional<std::pair<std::size_t, float>> World::first_object_hit(dotio::math::Segment segment,
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

void World::populate_objects() {
    // One crate in each loaded room; no AI or navigation state belongs to these bodies.
    for (int row = 6; row < Grid::rows - 6; ++row)
        for (int col = 6; col < Grid::columns - 6; ++col) {
            const int absolute_row = row + offset_row_, absolute_col = col + offset_column_;
            if ((absolute_row % 32 + 32) % 32 != 18 || (absolute_col % 32 + 32) % 32 != 18)
                continue;
            if (std::any_of(props_.begin(), props_.end(), [&](const Prop &prop) {
                    return prop.row == absolute_row && prop.column == absolute_col;
                }))
                continue;
            if (grid_.cell(row, col))
                continue;
            kinetra::Body body;
            body.position = {(col + 0.5f) * Grid::cell_size, (row + 0.5f) * Grid::cell_size};
            body.mass = 25;
            body.radius = 24;
            body.friction = 0.25f;
            body.restitution = 0.3f;
            props_.push_back({absolute_row, absolute_col, body});
        }
}

dotio::math::Vec2 World::stream_around_player() {
    const auto shift_axis = [](float position) {
        const int cell = int(std::floor(position / Grid::cell_size));
        return cell < 48 || cell > 106 ? int(std::floor((cell - 77) / 16.f)) * 16 : 0;
    };
    const int columns = shift_axis(player_.Position.X), rows = shift_axis(player_.Position.Y);
    if (!columns && !rows)
        return {};
    const dotio::math::Vec2 shift{columns * Grid::cell_size, rows * Grid::cell_size};
    offset_column_ += columns;
    offset_row_ += rows;
    grid_.generate(seed_, offset_row_, offset_column_);
    resident_cells_ = {-1, -1, -1, -1};
    player_.Position -= shift;
    ghost_.Position -= shift;
    EnemyPartitionMap.clear();
    for (auto &enemy : get_enemies()) {
        enemy.Position -= shift;
        enemy.Target -= shift;
        enemy.Path.clear();
        for (auto &partition : enemy.LastPartition)
            partition = {-1, -1};
        if (enemy.Position.X < Grid::cell_size * 3 || enemy.Position.Y < Grid::cell_size * 3 ||
            enemy.Position.X > Grid::cell_size * (Grid::columns - 3) ||
            enemy.Position.Y > Grid::cell_size * (Grid::rows - 3)) {
            enemy.IsActive = false;
            enemy.Path.clear();
        }
    }
    for (auto &arrow : get_arrows())
        arrow.Position -= shift;
    for (auto &bolt : get_bolts())
        bolt.Position -= shift;
    for (auto &prop : props_)
        prop.body.position -= dotio::physics_vector(shift);
    std::erase_if(props_, [](const Prop &prop) {
        return prop.body.position.x < Grid::cell_size * 3 || prop.body.position.y < Grid::cell_size * 3 ||
               prop.body.position.x > Grid::cell_size * (Grid::columns - 3) ||
               prop.body.position.y > Grid::cell_size * (Grid::rows - 3);
    });
    populate_objects();
    enemies_moved_ = false;
    return shift;
}

void World::stream_enemies(dotio::math::Position center, dotio::math::Vec2 normal_half,
                           dotio::math::Vec2 active_half, dotio::math::Vec2 retained_half, float elapsed) {
    if (!std::isfinite(elapsed) || elapsed < 0)
        throw std::invalid_argument("Spawn elapsed time must be finite and nonnegative");
    const auto outside = [&](dotio::math::Position position, dotio::math::Vec2 half) {
        return std::abs(position.X - center.X) > half.X || std::abs(position.Y - center.Y) > half.Y;
    };
    // Trim residency at cell boundaries, independently of camera pan/zoom.
    const auto limit = [](float value, int maximum) {
        return std::clamp(int(std::floor(value / Grid::cell_size)), 0, maximum - 1);
    };
    const std::array<int, 4> region{
        limit(center.Y - retained_half.Y, Grid::rows), limit(center.X - retained_half.X, Grid::columns),
        limit(center.Y + retained_half.Y, Grid::rows), limit(center.X + retained_half.X, Grid::columns)};
    if (region != resident_cells_) {
        // Only the entering and leaving strips change occupancy; SetCell() keeps the route
        // planner in step, and routes planned on the old cells are dropped.
        const auto inside = [](int row, int column, const std::array<int, 4> &bounds) {
            return row >= bounds[0] && column >= bounds[1] && row <= bounds[2] && column <= bounds[3];
        };
        for (int row = 0; row < Grid::rows; ++row)
            for (int col = 0; col < Grid::columns; ++col) {
                const bool admitted = inside(row, col, region);
                if (resident_cells_[0] >= 0 && admitted == inside(row, col, resident_cells_))
                    continue;
                const int cell = admitted ? terrain_cell(row, col) : 1;
                if (cell != grid_.cell(row, col))
                    grid_.set_cell(row, col, cell);
            }
        resident_cells_ = region;
        for (auto &enemy : get_enemies())
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
    const auto placement_half = spawn_half(normal_half, active_half);
    const auto admission_half = normal_half * 1.75f;
    int nearby_count = 0;
    float nearby_load = 0;
    for (const auto &enemy : get_enemies())
        if (enemy.IsActive && !outside(enemy.Position, retained_half)) {
            nearby_count += !outside(enemy.Position, admission_half);
            const float distance = (enemy.Position - player_.Position).get_length();
            nearby_load += std::clamp((1200 - distance) / 700, 0.f, 1.f);
        }
    const float spawn_rate =
        nearby_count < settings.active_spawn_limit
            ? Fuzzy::spawn_rate(nearby_load * settings.crowd_load_multiplier) * settings.spawn_rate_multiplier
            : 0;
    spawn_credit_ = spawn_rate > 0 ? std::min(1.f, spawn_credit_ + elapsed * spawn_rate) : 0;
    bool spawn_due = elapsed > 0 && spawn_credit_ >= 1;
    if (spawn_due)
        spawn_credit_ = 0; // No catch-up burst after pauses, blocked attempts or crowded periods.
    int attempts = 0;
    for (auto &enemy : get_enemies()) {
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
            const auto unit = [&]() { return dotio::math::unit(population_random_); };
            const dotio::math::Vec2 position{center.X + (unit() * 2 - 1) * placement_half.X,
                                             center.Y + (unit() * 2 - 1) * placement_half.Y};
            dotio::math::GridEntry cell;
            if (!dotio::math::GridEntry::from_world(position, Grid::origin, Grid::cell_size, Grid::grid_size,
                                                    cell) ||
                grid_.cell(cell.Row, cell.Column))
                continue;
            const dotio::math::Vec2 spawn{(cell.Column + 0.5f) * Grid::cell_size,
                                          (cell.Row + 0.5f) * Grid::cell_size};
            const bool occupied =
                std::any_of(get_enemies().get_instances(),
                            get_enemies().get_instances() + get_enemies().get_capacity(),
                            [&](const Enemy &other) {
                                return other.IsActive &&
                                       (other.Position - spawn).get_length() < settings.spawn_spacing;
                            }) ||
                std::any_of(props_.begin(), props_.end(), [&](const Prop &prop) {
                    return (dotio::legacy_vector(prop.body.position) - spawn).get_length() < 50;
                });
            const auto delta = spawn - player_.Position;
            // Keep the complete body outside 1.1x maximum normal view, at every zoom.
            const auto excluded_half = normal_half * 1.1f;
            const bool offscreen = std::abs(delta.X) - enemy.Radius > excluded_half.X ||
                                   std::abs(delta.Y) - enemy.Radius > excluded_half.Y;
            if (!occupied && !outside(spawn, placement_half) && !outside(spawn, active_half) && offscreen) {
                spawn_due = false;
                enemy.Position = spawn;
                enemy.Target = player_.Position;
                enemy.Velocity = {};
                enemy.IsActive = true;
                enemy.reset();
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

void World::update_objects(float delta) {
    for (auto &prop : props_)
        kinetra::integrate(prop.body, delta, 0.015f);
    // Iterative contacts let crates transfer momentum and settle against walls.
    for (int pass = 0; pass < 6; ++pass) {
        for (std::size_t i = 0; i < props_.size(); ++i) {
            auto &body = props_[i].body;
            auto player = dotio::physics_body(player_);
            if (const auto hit = kinetra::collide(player, body)) {
                kinetra::resolve_contact(player, body, *hit);
                dotio::apply_body(player_, player);
            }
            for (auto &enemy : get_enemies()) {
                if (!enemy.IsActive || enemy.Frozen ||
                    (enemy.Position - dotio::legacy_vector(body.position)).get_length() > 60)
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
            kinetra::collide_grid(body, grid_.collision_view());
        }
        grid_.handle_grid_collision(player_);
    }
}

void World::reset_tuning() {
    settings_ = settings_for(difficulty_);
    enemy_tuning_ = {};
}

void World::reset() {
    ++resets_;
    spawn_credit_ = 0;
    survival_seconds_ = 0;
    enemies_moved_ = false;
    EnemyPartitionMap.clear();
    for (auto &arrow : get_arrows())
        arrow.IsActive = false;
    for (auto &bolt : get_bolts())
        bolt.IsActive = false;
    get_player().Velocity = {};
    get_player().Cooldown = 0;

    get_player().Position = grid_.generate_position(placement_random_);
    get_player().Health = get_player().MaxHealth;
    get_player().Mana = get_player().MaxMana;
    get_player().Kills = 1;
    // Restart through the same offscreen population controller; resetting the
    // whole fixed pool must not materialize hundreds of enemies near the player.
    for (auto &enemy : get_enemies()) {
        if (enemy.IsActive)
            enemy.reset();
        enemy.IsActive = false;
        enemy.Frozen = true;
        enemy.Path.clear();
        enemy.Velocity = {};
        for (auto &partition : enemy.LastPartition)
            partition = {-1, -1};
    }
}

void World::set_player(dotio::Player player) {
    player_ = player;
}

dotio::Player &World::get_player() {
    return player_;
}

void World::set_ghost(dotio::Player ghost) {
    ghost_ = ghost;
}

dotio::Player &World::get_ghost() {
    return ghost_;
}

void World::add_enemy(const dotio::math::Position position, dotio::math::Position target, const float fear) {
    enemies_ += Enemy(this, position, target, 0.0f, fear);
}

void World::add_arrow(const dotio::math::Position position, const dotio::math::Vector2 velocity) {
    arrows_ += Arrow(position, velocity);
}

void World::add_bolt(const dotio::math::Position position, const dotio::math::Vector2 velocity) {
    bolts_ += Bolt(position, velocity);
}

void World::update_enemies(const float delta, const bool debug) {
    enemies_moved_ = true;
    std::size_t index = 0;
    // Snapshot the entire population first: neighbor separation may move another
    // enemy before its own update, and that displacement also belongs to the sweep.
    for (const auto &enemy : get_enemies())
        previous_enemy_positions_[index++] = enemy.Position;
    for (auto &enemy : get_enemies()) {
        if (!enemy.IsActive || enemy.Frozen)
            continue;
        enemy.Target = get_player().Position;
        enemy.TargetVelocity = get_player().Velocity;
        enemy.update(delta);
        enemy.Debug = debug;
        grid_.handle_grid_collision(enemy);
    }
}

// Sweep the actual movement segment, resolving only the earliest wall/body hit.
// This prevents vertical shots tunneling and bolts damaging enemies behind walls.
void World::update_arrows(const float delta, const bool debug) {
    for (auto &arrow : get_arrows()) {
        if (!arrow.IsActive)
            continue;
        const dotio::math::Segment segment{arrow.Position, arrow.Position + arrow.Velocity * delta};
        dotio::math::Point2 wall{}, hit{};
        const bool blocked = grid_.is_intersecting_any_cell(segment, wall);
        const bool hits_player =
            Grid::segement_circle_intersection(segment, get_player().Position, get_player().Radius, hit) & 1;
        const auto object = first_object_hit(segment, 0);
        const float object_distance = object ? object->second * (segment.End - segment.Start).get_length()
                                             : std::numeric_limits<float>::infinity();
        if (object && (!blocked || object_distance < (wall - segment.Start).get_length()) &&
            (!hits_player || object_distance < (hit - segment.Start).get_length())) {
            const auto point = segment.Start + (segment.End - segment.Start) * object->second;
            kinetra::apply_body_impulse(props_[object->first].body,
                                        dotio::physics_vector(arrow.Velocity) * 0.2f,
                                        dotio::physics_vector(point));
            arrow.IsActive = false;
        } else if (hits_player &&
                   (!blocked || (hit - segment.Start).get_length() < (wall - segment.Start).get_length())) {
            arrow.IsActive = false;
            if (!debug)
                get_player().Health -= 10;
            if (get_player().Health <= 0) {
                reset();
                return;
            }
        } else if (blocked) {
            arrow.IsActive = false;
        } else {
            arrow.update(delta);
        }
    }
}

void World::update_bolts(const float delta) {
    for (auto &bolt : get_bolts()) {
        if (!bolt.IsActive)
            continue;
        const dotio::math::Segment segment{bolt.Position, bolt.Position + bolt.Velocity * delta};
        dotio::math::Point2 wall{};
        const bool blocked = grid_.is_intersecting_any_cell(segment, wall);
        float nearest =
            blocked ? (wall - segment.Start).get_length() : std::numeric_limits<float>::infinity();
        Enemy *victim = nullptr;
        std::size_t index = 0;
        for (auto &enemy : get_enemies()) {
            const auto previous = enemies_moved_ ? previous_enemy_positions_[index] : enemy.Position;
            ++index;
            // Sweep relative motion, including the visible projectile radius. Convert
            // the relative hit parameter back to world distance for wall ordering.
            const dotio::math::Segment relative{segment.Start, segment.End - (enemy.Position - previous)};
            if (!enemy.IsActive)
                continue;
            const auto fraction = kinetra::segment_circle(
                dotio::physics_vector(relative.Start), dotio::physics_vector(relative.End),
                dotio::physics_vector(previous), enemy.Radius + Bolt::radius);
            if (!fraction)
                continue;
            const float distance = *fraction * (segment.End - segment.Start).get_length();
            if (distance < nearest) {
                nearest = distance;
                victim = &enemy;
            }
        }
        const auto object = first_object_hit(segment, Bolt::radius);
        if (object && object->second * (segment.End - segment.Start).get_length() < nearest) {
            const auto point = segment.Start + (segment.End - segment.Start) * object->second;
            kinetra::apply_body_impulse(props_[object->first].body,
                                        dotio::physics_vector(bolt.Velocity) * 0.5f,
                                        dotio::physics_vector(point));
            bolt.IsActive = false;
        } else if (victim) {
            bolt.IsActive = false;
            if (--victim->HitsRemaining > 0)
                continue;
            auto &player = get_player();
            const float health = victim->is_recovering() ? 40.f : victim->is_kamikazing() ? 20.f : 10.f;
            player.Health = std::min(player.MaxHealth, player.Health + health);
            if (victim->is_recovering() || victim->is_kamikazing())
                player.Mana = std::min(player.MaxMana, player.Mana + health);
            player.Kills = std::min(int(player.MaxBulletKills) + 1, player.Kills + 1);
            victim->IsActive = false;
        } else if (blocked) {
            bolt.IsActive = false;
        } else {
            bolt.update(delta);
        }
    }
    enemies_moved_ = false;
}

void World::draw(dotio::Renderer &renderer) {
    for (auto &enemy : get_enemies()) {
        if (!enemy.IsActive)
            continue;
        enemy.draw(renderer);
    }

    for (auto &arrow : get_arrows()) {
        if (!arrow.IsActive)
            continue;
        arrow.draw(renderer);
    }

    for (auto &bolt : get_bolts()) {
        if (!bolt.IsActive)
            continue;
        bolt.draw(renderer);
    }

    get_player().draw(renderer);
}

Tracker<Enemy> &World::get_enemies() {
    return enemies_.get_tracker();
}

Tracker<Arrow> &World::get_arrows() {
    return arrows_.get_tracker();
}

Tracker<Bolt> &World::get_bolts() {
    return bolts_.get_tracker();
}

} // namespace dotio

#include "dotio/world.h"
#include "dotio/ai/fuzzy.h"
#include "dotio/entities/physics_adapter.h"
#include "dotio/math/random.h"
#include "dotio/navigation/route.h"
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
    return tick_rate_;
}

namespace {
/// Second word of the seed sequence of World::wander_random(), so that stream differs from
/// the population stream that the same world seed starts.
constexpr std::uint32_t wander_stream = 0x57414E44; // "WAND"
/// Second word of the seed sequences of World::behavior_random() and of the placement stream.
constexpr std::uint32_t behavior_stream = 0x42485652;  // "BHVR"
constexpr std::uint32_t placement_stream = 0x504C4345; // "PLCE"

/// Passing geometry in legacy pixels, for enemies of radius 20: the gap kept between two
/// passing enemies and before a destination, how far past the midpoint the exits lie, the
/// extra room along the line a plan needs, and the clearance added to each footprint.
constexpr float passing_gap = 6, passing_exit_offset = 12, passing_formation_room = 24, passing_clearance = 2;
/// Distance from a lane target within which an enemy has reached it, in legacy pixels.
constexpr float passing_arrival = 15;
/// Share of the top speed below which an enemy at a lane target counts as standing there.
constexpr float passing_stop_fraction = 0.25f;
/// Updates over which conflicts are predicted (one second at 60 Hz), and between two
/// predicted positions of another mover in the lane guard.
constexpr int passing_horizon = 60, passing_sample_updates = 4;
/// Updates the lane ahead of a waiting enemy stays clear before it moves on; the longest
/// wait and stretch without progress; and the pause before a pair whose plan failed may
/// plan again.
constexpr std::uint64_t passing_resume = 20, passing_timeout = 240, passing_retry = 60;

/// Whether `enemy` follows its route in its current state, so it can join a passing plan.
bool follows_route(Enemy &enemy, WanderMode wander) {
    return enemy.is_pursueing() || enemy.is_recovering() ||
           (enemy.is_wandering() && wander == WanderMode::path);
}
} // namespace

void World::initialize(std::uint32_t seed, Difficulty difficulty) {
    settings_ = settings_for(difficulty); // Throws before anything else in the world changes.
    enemy_tuning_ = {};
    end_passing(false);
    passing_ = false;
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
        add_enemy(grid_.generate_position(placement_random_), get_player().position, fear);
        enemies_[i].initialize();
        // Slots start dormant; player-centered streaming alone chooses spawn density.
        enemies_[i].is_active = false;
        enemies_[i].frozen = true;
    }
}

std::optional<std::pair<std::size_t, float>> World::first_object_hit(kinetra::Segment segment,
                                                                     float radius) const {
    std::optional<std::pair<std::size_t, float>> nearest;
    for (std::size_t i = 0; i < props_.size(); ++i)
        if (const auto hit = kinetra::segment_circle(segment.start, segment.end, props_[i].body.position,
                                                     props_[i].body.radius + radius);
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
            body.position = {(static_cast<float>(col) + 0.5f) * Grid::cell_size,
                             (static_cast<float>(row) + 0.5f) * Grid::cell_size};
            body.mass = 25;
            body.radius = 24;
            body.friction = 0.25f;
            body.restitution = 0.3f;
            props_.push_back({absolute_row, absolute_col, body});
        }
}

kinetra::Vec2 World::stream_around_player() {
    const auto shift_axis = [](float position) {
        const int cell = int(std::floor(position / Grid::cell_size));
        return cell < 48 || cell > 106 ? int(std::floor(static_cast<float>(cell - 77) / 16.f)) * 16 : 0;
    };
    const int columns = shift_axis(player_.position.x), rows = shift_axis(player_.position.y);
    if (!columns && !rows)
        return {};
    const kinetra::Vec2 shift{static_cast<float>(columns) * Grid::cell_size,
                              static_cast<float>(rows) * Grid::cell_size};
    offset_column_ += columns;
    offset_row_ += rows;
    // Every route is planned again after the shift, and the lanes would lie elsewhere.
    end_passing(false);
    grid_.generate(seed_, offset_row_, offset_column_);
    resident_cells_ = {-1, -1, -1, -1};
    player_.position -= shift;
    ghost_.position -= shift;
    enemy_partition_map.clear();
    for (auto &enemy : get_enemies()) {
        enemy.position -= shift;
        enemy.target -= shift;
        enemy.path.clear();
        for (auto &partition : enemy.last_partition)
            partition = {-1, -1};
        if (enemy.position.x < Grid::cell_size * 3 || enemy.position.y < Grid::cell_size * 3 ||
            enemy.position.x > Grid::cell_size * (Grid::columns - 3) ||
            enemy.position.y > Grid::cell_size * (Grid::rows - 3)) {
            enemy.is_active = false;
            enemy.path.clear();
        }
    }
    for (auto &arrow : get_arrows())
        arrow.position -= shift;
    for (auto &bolt : get_bolts())
        bolt.position -= shift;
    for (auto &prop : props_)
        prop.body.position -= shift;
    std::erase_if(props_, [](const Prop &prop) {
        return prop.body.position.x < Grid::cell_size * 3 || prop.body.position.y < Grid::cell_size * 3 ||
               prop.body.position.x > Grid::cell_size * (Grid::columns - 3) ||
               prop.body.position.y > Grid::cell_size * (Grid::rows - 3);
    });
    populate_objects();
    enemies_moved_ = false;
    return shift;
}

void World::stream_enemies(kinetra::Vec2 center, kinetra::Vec2 normal_half, kinetra::Vec2 active_half,
                           kinetra::Vec2 retained_half, float elapsed) {
    if (!std::isfinite(elapsed) || elapsed < 0)
        throw std::invalid_argument("Spawn elapsed time must be finite and nonnegative");
    const auto outside = [&](kinetra::Vec2 position, kinetra::Vec2 half) {
        return std::abs(position.x - center.x) > half.x || std::abs(position.y - center.y) > half.y;
    };
    // Trim residency at cell boundaries, independently of camera pan/zoom.
    const auto limit = [](float value, int maximum) {
        return std::clamp(int(std::floor(value / Grid::cell_size)), 0, maximum - 1);
    };
    const std::array<int, 4> region{
        limit(center.y - retained_half.y, Grid::rows), limit(center.x - retained_half.x, Grid::columns),
        limit(center.y + retained_half.y, Grid::rows), limit(center.x + retained_half.x, Grid::columns)};
    if (region != resident_cells_) {
        // Only the entering and leaving strips change occupancy; set_cell() keeps the route
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
            enemy.path.clear();
    }
    std::erase_if(props_, [&](const Prop &prop) { return outside(prop.body.position, retained_half); });
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
        if (enemy.is_active && !outside(enemy.position, retained_half)) {
            nearby_count += !outside(enemy.position, admission_half);
            const float distance = kinetra::length(enemy.position - player_.position);
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
        if (enemy.is_active && outside(enemy.position, retained_half)) {
            for (auto cell : enemy.last_partition)
                if (auto found = enemy_partition_map.find(cell); found != enemy_partition_map.end())
                    found->second.erase(&enemy);
            enemy.is_active = false;
            enemy.path.clear();
            enemy.frozen = true;
            continue;
        }
        if (!enemy.is_active && spawn_due && attempts++ < 8) {
            const auto unit = [&]() { return dotio::math::unit(population_random_); };
            const kinetra::Vec2 position{center.x + (unit() * 2 - 1) * placement_half.x,
                                         center.y + (unit() * 2 - 1) * placement_half.y};
            dotio::math::GridEntry cell;
            if (!dotio::math::GridEntry::from_world(position, Grid::origin, Grid::cell_size, Grid::grid_size,
                                                    cell) ||
                grid_.cell(cell.row, cell.column))
                continue;
            const kinetra::Vec2 spawn{(static_cast<float>(cell.column) + 0.5f) * Grid::cell_size,
                                      (static_cast<float>(cell.row) + 0.5f) * Grid::cell_size};
            const bool occupied =
                std::any_of(get_enemies().get_instances(),
                            get_enemies().get_instances() + get_enemies().get_capacity(),
                            [&](const Enemy &other) {
                                return other.is_active &&
                                       kinetra::length(other.position - spawn) < settings.spawn_spacing;
                            }) ||
                std::any_of(props_.begin(), props_.end(), [&](const Prop &prop) {
                    return kinetra::length(prop.body.position - spawn) < 50;
                });
            const auto delta = spawn - player_.position;
            // Keep the complete body outside 1.1x maximum normal view, at every zoom.
            const auto excluded_half = normal_half * 1.1f;
            const bool offscreen = std::abs(delta.x) - enemy.radius > excluded_half.x ||
                                   std::abs(delta.y) - enemy.radius > excluded_half.y;
            if (!occupied && !outside(spawn, placement_half) && !outside(spawn, active_half) && offscreen) {
                spawn_due = false;
                enemy.position = spawn;
                enemy.target = player_.position;
                enemy.velocity = {};
                enemy.is_active = true;
                enemy.reset();
                for (auto &partition : enemy.last_partition) {
                    if (auto found = enemy_partition_map.find(partition); found != enemy_partition_map.end())
                        found->second.erase(&enemy);
                    partition = {-1, -1};
                }
            }
        }
        enemy.frozen = outside(enemy.position, active_half);
        if (enemy.frozen) {
            enemy.path.clear();
            enemy.velocity = {};
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
                if (!enemy.is_active || enemy.frozen || kinetra::length(enemy.position - body.position) > 60)
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
    end_passing(false);
    spawn_credit_ = 0;
    survival_seconds_ = 0;
    enemies_moved_ = false;
    enemy_partition_map.clear();
    for (auto &arrow : get_arrows())
        arrow.is_active = false;
    for (auto &bolt : get_bolts())
        bolt.is_active = false;
    get_player().velocity = {};
    get_player().cooldown = 0;

    get_player().position = grid_.generate_position(placement_random_);
    get_player().health = get_player().max_health;
    get_player().mana = get_player().max_mana;
    get_player().kills = 1;
    // Restart through the same offscreen population controller; resetting the
    // whole fixed pool must not materialize hundreds of enemies near the player.
    for (auto &enemy : get_enemies()) {
        if (enemy.is_active)
            enemy.reset();
        enemy.is_active = false;
        enemy.frozen = true;
        enemy.path.clear();
        enemy.velocity = {};
        for (auto &partition : enemy.last_partition)
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

void World::add_enemy(const kinetra::Vec2 position, kinetra::Vec2 target, const float fear) {
    enemies_ += Enemy(this, position, target, 0.0f, fear);
}

void World::add_arrow(const kinetra::Vec2 position, const kinetra::Vec2 velocity) {
    arrows_ += Arrow(position, velocity);
}

void World::add_bolt(const kinetra::Vec2 position, const kinetra::Vec2 velocity) {
    bolts_ += Bolt(position, velocity);
}

void World::set_passing(bool passing) {
    if (passing_ && !passing)
        end_passing(true);
    passing_ = passing;
}

void World::end_passing(bool replan) {
    passing_events_ = passing_coordinator_.clear();
    std::size_t slot = 0;
    for (auto &enemy : get_enemies()) {
        enemy.passing_target.reset();
        if (replan)
            for (const auto &event : passing_events_)
                if ((event.movers[0] == slot || event.movers[1] == slot) && !enemy.path.empty())
                    enemy.path = plan_route(grid_, enemy.position, enemy.path.destination());
        ++slot;
    }
}

void World::coordinate_passing() {
    const float force =
        std::max({enemy_tuning_.wander_force, enemy_tuning_.pursue_force, enemy_tuning_.recover_force}) *
        settings_.movement_multiplier;
    // Enemies that cannot move pass nobody.
    if (!(force > 0)) {
        end_passing(true);
        return;
    }
    // A constant push settles at four times its size per update: the top speed.
    const float speed = 4 * force;
    const kinetra::PassingCoordination coordination{
        .margins = {passing_gap, passing_exit_offset, passing_formation_room},
        .prediction = {speed, force, passing_horizon, passing_sample_updates},
        .clearance = {passing_clearance, passing_clearance},
        .arrival_distance = passing_arrival,
        .stop_speed = speed * passing_stop_fraction,
        .resume_steps = passing_resume,
        .timeout_steps = passing_timeout,
        .retry_steps = passing_retry};
    // One mover per pool slot, so plans keep their indices while slots fill and empty, and
    // the player after them as a mover that never joins a plan.
    auto &enemies = get_enemies();
    const std::size_t slots = enemies.get_size();
    std::vector<kinetra::PassingMover> movers(slots + 1);
    std::size_t slot = 0;
    for (auto &enemy : enemies) {
        auto &mover = movers[slot++];
        if (!enemy.is_active) {
            mover.present = false; // An empty slot keeps the default body, which is valid.
            continue;
        }
        mover.body = physics_body(enemy);
        mover.available = false;
        mover.stationary = enemy.frozen;
        if (!enemy.frozen && !enemy.path.empty() && follows_route(enemy, enemy_tuning_.wander)) {
            mover.available = true;
            mover.heading = kinetra::normalized(enemy.path.next() - enemy.position);
            mover.destination = enemy.path.destination();
        }
    }
    auto &player = movers[slots];
    player.body = physics_body(player_);
    player.available = false;
    player.stationary = kinetra::length(player_.velocity) < coordination.stop_speed;
    passing_events_ = passing_coordinator_.step(
        movers, coordination, [this](std::size_t, kinetra::Vec2 from, kinetra::Vec2 to, kinetra::Vec2 half) {
            return grid_.navigation().clear(from, to, half);
        });
    slot = 0;
    for (auto &enemy : enemies) {
        // An enemy whose plan ended resumes its trip on a route from where it stands.
        for (const auto &event : passing_events_)
            if (event.kind == kinetra::PassingEventKind::ended &&
                (event.movers[0] == slot || event.movers[1] == slot) && !enemy.path.empty())
                enemy.path = plan_route(grid_, enemy.position, enemy.path.destination());
        const auto target = passing_coordinator_.target(slot);
        enemy.passing_target = target ? std::optional{target->point} : std::nullopt;
        ++slot;
    }
}

void World::update_enemies(const float delta, const bool debug) {
    enemies_moved_ = true;
    std::size_t index = 0;
    // Snapshot the entire population first: neighbor separation may move another
    // enemy before its own update, and that displacement also belongs to the sweep.
    for (const auto &enemy : get_enemies())
        previous_enemy_positions_[index++] = enemy.position;
    if (passing_)
        coordinate_passing();
    for (auto &enemy : get_enemies()) {
        if (!enemy.is_active || enemy.frozen)
            continue;
        enemy.target = get_player().position;
        enemy.target_velocity = get_player().velocity;
        enemy.update(delta);
        enemy.debug = debug;
        grid_.handle_grid_collision(enemy);
    }
}

// Sweep the actual movement segment, resolving only the earliest wall/body hit.
// This prevents vertical shots tunneling and bolts damaging enemies behind walls.
void World::update_arrows(const float delta, const bool debug) {
    for (auto &arrow : get_arrows()) {
        if (!arrow.is_active)
            continue;
        const kinetra::Segment segment{arrow.position, arrow.position + arrow.velocity * delta};
        kinetra::Vec2 wall{}, hit{};
        const bool blocked = grid_.is_intersecting_any_cell(segment, wall);
        const bool hits_player =
            Grid::segment_circle_intersection(segment, get_player().position, get_player().radius, hit) & 1;
        const auto object = first_object_hit(segment, 0);
        const float object_distance = object ? object->second * kinetra::length(segment.end - segment.start)
                                             : std::numeric_limits<float>::infinity();
        if (object && (!blocked || object_distance < kinetra::length(wall - segment.start)) &&
            (!hits_player || object_distance < kinetra::length(hit - segment.start))) {
            const auto point = segment.start + (segment.end - segment.start) * object->second;
            kinetra::apply_body_impulse(props_[object->first].body, arrow.velocity * 0.2f, point);
            arrow.is_active = false;
        } else if (hits_player && (!blocked || kinetra::length(hit - segment.start) <
                                                   kinetra::length(wall - segment.start))) {
            arrow.is_active = false;
            if (!debug)
                get_player().health -= 10;
            if (get_player().health <= 0) {
                reset();
                return;
            }
        } else if (blocked) {
            arrow.is_active = false;
        } else {
            arrow.update(delta);
        }
    }
}

void World::update_bolts(const float delta) {
    for (auto &bolt : get_bolts()) {
        if (!bolt.is_active)
            continue;
        const kinetra::Segment segment{bolt.position, bolt.position + bolt.velocity * delta};
        kinetra::Vec2 wall{};
        const bool blocked = grid_.is_intersecting_any_cell(segment, wall);
        float nearest =
            blocked ? kinetra::length(wall - segment.start) : std::numeric_limits<float>::infinity();
        Enemy *victim = nullptr;
        std::size_t index = 0;
        for (auto &enemy : get_enemies()) {
            const auto previous = enemies_moved_ ? previous_enemy_positions_[index] : enemy.position;
            ++index;
            // Sweep relative motion, including the visible projectile radius. Convert
            // the relative hit parameter back to world distance for wall ordering.
            const kinetra::Segment relative{segment.start, segment.end - (enemy.position - previous)};
            if (!enemy.is_active)
                continue;
            const auto fraction =
                kinetra::segment_circle(relative.start, relative.end, previous, enemy.radius + Bolt::radius);
            if (!fraction)
                continue;
            const float distance = *fraction * kinetra::length(segment.end - segment.start);
            if (distance < nearest) {
                nearest = distance;
                victim = &enemy;
            }
        }
        const auto object = first_object_hit(segment, Bolt::radius);
        if (object && object->second * kinetra::length(segment.end - segment.start) < nearest) {
            const auto point = segment.start + (segment.end - segment.start) * object->second;
            kinetra::apply_body_impulse(props_[object->first].body, bolt.velocity * 0.5f, point);
            bolt.is_active = false;
        } else if (victim) {
            bolt.is_active = false;
            if (--victim->hits_remaining > 0)
                continue;
            auto &player = get_player();
            const float health = victim->is_recovering() ? 40.f : victim->is_kamikazing() ? 20.f : 10.f;
            player.health = std::min(player.max_health, player.health + health);
            if (victim->is_recovering() || victim->is_kamikazing())
                player.mana = std::min(player.max_mana, player.mana + health);
            player.kills = std::min(int(player.max_bullet_kills) + 1, player.kills + 1);
            victim->is_active = false;
        } else if (blocked) {
            bolt.is_active = false;
        } else {
            bolt.update(delta);
        }
    }
    enemies_moved_ = false;
}

void World::draw(dotio::Renderer &renderer) {
    for (auto &enemy : get_enemies()) {
        if (!enemy.is_active)
            continue;
        enemy.draw(renderer);
    }

    for (auto &arrow : get_arrows()) {
        if (!arrow.is_active)
            continue;
        arrow.draw(renderer);
    }

    for (auto &bolt : get_bolts()) {
        if (!bolt.is_active)
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

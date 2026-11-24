#include "dotio/entities/enemy.h"
#include "dotio/ai/state.h"
#include "dotio/ai/wander.h"
#include "dotio/entities/physics_adapter.h"
#include "dotio/math/random.h"
#include "dotio/navigation/grid.h"
#include "dotio/world.h"
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace dotio {
Enemy::Enemy() : PhysicsObject() {
}

Enemy::Enemy(World *world, kinetra::Vec2 start, kinetra::Vec2 first_target, const float start_direction,
             const float start_fear)
    : PhysicsObject(60.0f, dotio::Grid::cell_size * 0.2, 0.2f, 0.2f, start, start_direction, true),
      world_(world), target(first_target), path{}, fear(start_fear) {
}

void Enemy::update(const float delta) {
    steering_ = {};
    if (frozen || target == position)
        return;

    const auto initial_velocity = velocity;
    state_->execute(delta);
    steering_ = velocity - initial_velocity;
    spatial_partitioning(delta);
}

void Enemy::tick() {
    if (frozen || target == position)
        return;

    if (cooldown_ > 0.0f)
        cooldown_ -= world_->get_tick_rate();
    if (cooldown_ < 0.0f)
        cooldown_ = 0.0f;

    player_bumped_into_enemy();
    if (!is_active || frozen)
        return;
    line_of_sight = is_in_line_of_sight();
    state_->update();
}

void Enemy::draw(dotio::Renderer &renderer) {
    kinetra::Color red = {1.0f, 0.0f, 0.0f, 1.0f};
    kinetra::Color orange = {1.0f, 0.65f, 0.0f, 1.0f};
    kinetra::Color kamikaze = {1.0f, 0.3f, 0.3f, 1.0f};
    kinetra::Color green = {0.0f, 1.0f, 0.0f, 1.0f};
    kinetra::Color blue = {0.0f, 0.5f, 1.0f, 1.0f};
    renderer.draw_circle(position, radius,
                         is_kamikazing()   ? kamikaze
                         : !debug          ? red
                         : is_recovering() ? green
                         : is_attacking()  ? red
                         : is_pursueing()  ? orange
                                           : blue);
}

void Enemy::player_bumped_into_enemy() {
    if (sqrt(pow(position.x - world_->get_player().position.x, 2) +
             pow(position.y - world_->get_player().position.y, 2)) < (radius * 2)) {
        if (is_kamikazing()) {
            if (!debug)
                world_->get_player().health -= 30.0f;
            if (debug && world_->get_player().health <= 0.0f)
                world_->get_player().health = 0.1f;
            else if (world_->get_player().health <= 0.0f)
                world_->reset();
        } else {
            if (world_->get_player().mana < world_->get_player().max_mana)
                world_->get_player().mana += is_recovering() ? 40.0f : 20.0f;
            if (is_recovering() && world_->get_player().health < world_->get_player().max_health)
                world_->get_player().health += 40.0f;
        }
        is_active = false;
    }

    if (!is_active) {
        // All replacements go through timed offscreen spawning, including contact kills.
        frozen = true;
        path.clear();
        velocity = {};
    }
}

void Enemy::spatial_partitioning(const float delta) {
    auto motion = dotio::physics_body(*this);
    kinetra::integrate(motion, delta, friction_);
    velocity = motion.velocity;

    kinetra::Vec2 next{position + velocity};
    uint8_t p = dotio::math::GridEntry::get_square_partition(next, radius, dotio::Grid::origin,
                                                             dotio::Grid::cell_size);
    dotio::math::GridEntry partitions[4]{{-1, -1}, {-1, -1}, {-1, -1}, {-1, -1}};

    dotio::math::GridEntry::get_partitions(p, next, dotio::Grid::origin, dotio::Grid::cell_size,
                                           dotio::Grid::grid_size, partitions);

    bool keep[4]{false, false, false, false};

    for (int i = 0; i < 4; i++) {
        dotio::math::GridEntry *np = &partitions[i];

        if (world_->enemy_partition_map.contains(*np))
            for (auto o : world_->enemy_partition_map[*np]) {
                dotio::Enemy &other = *(dotio::Enemy *)o;
                if (!other.is_active || other.frozen)
                    continue;

                if (&other == this)
                    continue;

                auto body = dotio::physics_body(*this);
                body.position = next;
                auto neighbor = dotio::physics_body(other);
                if (const auto contact = kinetra::collide(body, neighbor)) {
                    kinetra::resolve_contact(body, neighbor, *contact);
                    next = body.position;
                    dotio::apply_body(other, neighbor);
                }
            }
    }

    velocity = next - position;

    motion = dotio::physics_body(*this);
    kinetra::integrate(motion, delta);
    dotio::apply_body(*this, motion);

    p = dotio::math::GridEntry::get_square_partition(position, radius, dotio::Grid::origin,
                                                     dotio::Grid::cell_size);
    partitions[0] = {-1, -1};
    partitions[1] = {-1, -1};
    partitions[2] = {-1, -1};
    partitions[3] = {-1, -1};

    dotio::math::GridEntry::get_partitions(p, position, dotio::Grid::origin, dotio::Grid::cell_size,
                                           dotio::Grid::grid_size, partitions);

    keep[0] = false;
    keep[1] = false;
    keep[2] = false;
    keep[3] = false;

    for (int i = 0; i < 4; i++) {
        dotio::math::GridEntry *np = &partitions[i];

        if (np->row == -1)
            continue;

        if (world_->enemy_partition_map[*np].contains(this))
            keep[i] = true;
    }

    for (int i = 0; i < 4; i++) {
        dotio::math::GridEntry *np = &partitions[i];
        dotio::math::GridEntry *lp = &last_partition[i];

        if (np->row != lp->row || np->column != lp->column) {
            if (np->row == -1 || np->column == -1) {
                if (world_->enemy_partition_map.contains(*lp))
                    world_->enemy_partition_map[*lp].erase(this);

                if (world_->enemy_partition_map[*lp].empty())
                    world_->enemy_partition_map.erase(*lp);

                *lp = *np;
                continue;
            }

            if (!keep[i]) {
                if (world_->enemy_partition_map.contains(*lp))
                    world_->enemy_partition_map[*lp].erase(this);

                world_->enemy_partition_map[*np].insert(this);

                if (world_->enemy_partition_map[*lp].empty())
                    world_->enemy_partition_map.erase(*lp);
            }

            *lp = *np;
        }
    }
}

void Enemy::initialize() {
    hits_remaining = world_->difficulty_settings().enemy_hits;
    randomize_wander(*this);
    if (state_ == nullptr) {
        state_ = &wander_;
        attack_.set_enemy(*this);
        wander_.set_enemy(*this);
        pursue_.set_enemy(*this);
        recover_.set_enemy(*this);
        kamikaze_.set_enemy(*this);
    }
}

World &Enemy::get_world() {
    return *world_;
}

State &Enemy::get_state() {
    return *state_;
}
void Enemy::set_state(State &state) {
    state_ = &state;
}

State &Enemy::get_attack_state() {
    return attack_;
}
State &Enemy::get_wander_state() {
    return wander_;
}
State &Enemy::get_pursue_state() {
    return pursue_;
}
State &Enemy::get_recover_state() {
    return recover_;
}
State &Enemy::get_kamikaze_state() {
    return kamikaze_;
}

bool Enemy::is_attacking() {
    return &get_state() == &get_attack_state();
}
bool Enemy::is_wandering() {
    return &get_state() == &get_wander_state();
}
bool Enemy::is_pursueing() {
    return &get_state() == &get_pursue_state();
}
bool Enemy::is_recovering() {
    return &get_state() == &get_recover_state();
}
bool Enemy::is_kamikazing() {
    return &get_state() == &get_kamikaze_state();
}
bool Enemy::is_in_line_of_sight() {
    if (kinetra::Vec2 _;
        world_->grid().is_intersecting_any_cell({position, world_->get_player().position}, _))
        return false;
    else {
        return true;
    }
}
bool Enemy::is_out_of_ammo() const {
    return ammo_ >= ammo_capacity;
}
bool Enemy::has_reloaded_ammo() const {
    return ammo_ <= 1;
}
bool Enemy::pursue_countdown_passed() const {
    return countdown_ >= world_->difficulty_settings().pursuit_cycles;
}

int Enemy::get_ammo() const {
    return ammo_;
}
float Enemy::get_cooldown() const {
    return cooldown_;
}
float Enemy::ammo_used_ratio() const {
    return static_cast<float>(ammo_) / static_cast<float>(ammo_capacity);
}
float Enemy::distance() const {
    return std::max(std::abs(position.x - target.x), std::abs(position.y - target.y));
}

void Enemy::ammo_fired() {
    ammo_ += 1;
}
void Enemy::ammo_recovered() {
    ammo_ -= 2;
}
void Enemy::pursue_countdown() {
    countdown_ += 1;
}
void Enemy::reset_cooldown() {
    cooldown_ = max_cooldown_ * world_->difficulty_settings().fire_interval_multiplier;
}
void Enemy::reset_pursue() {
    countdown_ = 0;
}
void Enemy::reset() {
    ++generation_;
    steering_ = {};
    hits_remaining = world_->difficulty_settings().enemy_hits;
    fear = dotio::math::uniform(world_->behavior_random(), spawn_fear_min, spawn_fear_max);
    state_ = &wander_;
    randomize_wander(*this);
    is_active = true;
    reset_cooldown();
    ammo_ = 1;
    countdown_ = 0;
    path.clear();
}
} // namespace dotio

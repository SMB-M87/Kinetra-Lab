#include "dotio/ai/state.h"
#include "dotio/ai/fuzzy.h"
#include "dotio/ai/wander.h"
#include "dotio/entities/enemy.h"
#include "dotio/math/random.h"
#include "dotio/navigation/grid.h"
#include "dotio/navigation/route.h"
#include "dotio/world.h"

#include <cmath>
#include <numbers>
#include <optional>

namespace dotio {
namespace {
/// Distance at which an enemy passes a waypoint, in grid cells: it turns toward the next one
/// before it reaches the cell center, so it keeps moving.
constexpr float waypoint_reach_cells = 0.666f;
/// Distance at which a recovering enemy passes the last waypoint of its shelter route, in
/// grid cells: it hides at the shelter's center.
constexpr float shelter_reach_cells = 0.1f;

/// The point `enemy` steers toward on its route, or nothing for an empty route. A waypoint
/// closer than `reach` legacy pixels is passed and the next one taken; after the last one
/// is passed, the enemy still steers toward it for this update.
std::optional<kinetra::Vec2> steering_point(Enemy &enemy, float reach) {
    auto &route = enemy.path;
    if (route.empty())
        return std::nullopt;
    auto point = route.next();
    if (kinetra::length(point - enemy.position) < reach) {
        route.advance();
        if (!route.empty())
            point = route.next();
    }
    return point;
}

/// Accelerates `enemy` toward `point` by `force` legacy pixels per update, over `delta`
/// updates and scaled by the difficulty's movement multiplier, as every state does.
void accelerate_toward(Enemy &enemy, kinetra::Vec2 point, float force, float delta) {
    if (point == enemy.position)
        return;
    const kinetra::Vec2 acceleration = kinetra::normalized(point - enemy.position) * force;
    enemy.apply_force(acceleration * (delta * enemy.get_world().difficulty_settings().movement_multiplier),
                      0.5f * enemy.get_mass());
}

/// Lead angle for an arrow of speed `arrow_speed` to meet a target that moves at
/// `target_speed`, `angle` off the line from the target to the shooter:
/// asin(sin(angle) * target_speed / arrow_speed), with the sine clamped to [-1, 1] when the
/// arrow is too slow.
float intercept(float target_speed, float arrow_speed, float angle) {
    float sine = std::sin(angle) * target_speed / arrow_speed;
    if (sine > 1)
        sine = 1;
    else if (sine < -1)
        sine = -1;
    return kinetra::asin(sine);
}

/// Velocity of an arrow that `enemy` fires at its target: `arrow_speed` long, aimed at the
/// target and led by `prediction` times the lead angle of the target's motion.
kinetra::Vec2 aimed_arrow(const Enemy &enemy, float arrow_speed, float prediction) {
    const kinetra::Vec2 heading{arrow_speed, 0.0f};
    const float lead = intercept(kinetra::length(enemy.target_velocity), arrow_speed,
                                 kinetra::signed_angle(enemy.target_velocity, enemy.position - enemy.target));
    return kinetra::rotated(heading, kinetra::signed_angle(heading, enemy.target - enemy.position) +
                                         prediction * lead);
}

/// Cell of `point`, or {-1, -1} outside the grid.
dotio::math::GridEntry cell_of(kinetra::Vec2 point) {
    dotio::math::GridEntry cell;
    if (!dotio::math::GridEntry::from_world(point, Grid::origin, Grid::cell_size, Grid::grid_size, cell))
        return {-1, -1};
    return cell;
}

/// Plans a new route to the enemy's target when it has none or its route ends in another
/// cell than the target's: the target moved since the route was planned. A failed plan keeps
/// the old route, as the original game did.
void follow_target(Enemy &enemy) {
    if (!enemy.path.empty() && cell_of(enemy.path.destination()) == cell_of(enemy.target))
        return;
    if (auto route = plan_route(enemy.get_world().grid(), enemy.position, enemy.target); !route.empty())
        enemy.path = std::move(route);
}

/// Center of `cell` in legacy pixels.
kinetra::Vec2 center_of(dotio::math::GridEntry cell) {
    return {(float(cell.column) + 0.5f) * Grid::cell_size, (float(cell.row) + 0.5f) * Grid::cell_size};
}
} // namespace

void WanderState::execute(const float delta) {
    if (enemy_->get_world().enemy_tuning().wander == WanderMode::steering) {
        steer_wander(*enemy_, delta);
        return;
    }
    const auto point = steering_point(*enemy_, Grid::cell_size * waypoint_reach_cells);
    if (!point)
        return;
    // The original game aims at a random point around the waypoint, wider than tall, every
    // update, so path wanderers sway along their route.
    const float angle =
        dotio::math::unit(enemy_->get_world().behavior_random()) * 2 * std::numbers::pi_v<float>;
    const kinetra::Vec2 displacement{Grid::cell_size * std::cos(angle),
                                     Grid::cell_size * 0.4f * std::sin(angle)};
    const kinetra::Vec2 force = kinetra::normalized(*point + displacement - enemy_->position) *
                                enemy_->get_world().enemy_tuning().wander_force;
    enemy_->apply_force(force * (delta * enemy_->get_world().difficulty_settings().movement_multiplier),
                        0.5f * enemy_->get_mass());
}

void WanderState::update() {
    if (enemy_->line_of_sight) {
        enemy_->path.clear();
        enemy_->set_state(enemy_->get_attack_state());
        return;
    }

    if (enemy_->get_world().enemy_tuning().wander == WanderMode::steering) {
        drift_wander_heading(*enemy_);
        return;
    }

    if (enemy_->path.empty()) {
        if (const auto cell = cell_of(enemy_->position); cell.row >= 0) {
            auto &world = enemy_->get_world();
            const auto goal = world.grid().generate_grid_entry(cell, world.behavior_random());
            enemy_->path = plan_route(world.grid(), enemy_->position, center_of(goal));
        }
    }
}

void AttackState::execute(const float delta) {
    bool boundary =
        sqrt(pow(enemy_->position.x - enemy_->target.x, 2) + pow(enemy_->position.y - enemy_->target.y, 2)) <
        (dotio::Grid::cell_size * (enemy_->fear * (enemy_->ammo_used_ratio())) +
         dotio::Grid::cell_size * 0.5f);

    kinetra::Vec2 force = {kinetra::normalized(boundary ? enemy_->position - enemy_->target
                                                        : enemy_->target - enemy_->position) *
                           enemy_->get_world().enemy_tuning().attack_force};

    enemy_->apply_force(force * (delta * enemy_->get_world().difficulty_settings().movement_multiplier),
                        0.5f * enemy_->get_mass());
}

void AttackState::update() {
    // Kamikaze is a last-health behavior; fuzzy combat rules still choose when to commit.
    if (enemy_->hits_remaining == 1 &&
        dotio::Fuzzy::evaluate(enemy_->distance(), enemy_->fear, static_cast<float>(enemy_->get_ammo()))) {
        enemy_->set_state(enemy_->get_kamikaze_state());
        return;
    }
    if (enemy_->is_out_of_ammo()) {
        enemy_->set_state(enemy_->get_recover_state());
        return;
    }
    if (!enemy_->line_of_sight) {
        enemy_->reset_pursue();
        enemy_->set_state(enemy_->get_pursue_state());
        return;
    }

    if (enemy_->get_cooldown() <= 0.0f) {
        enemy_->reset_cooldown();
        enemy_->ammo_fired();
        const auto &settings = enemy_->get_world().difficulty_settings();
        enemy_->get_world().add_arrow(enemy_->position,
                                      aimed_arrow(*enemy_, settings.projectile_speed, settings.prediction));
    }
}

void PursueState::execute(const float delta) {
    if (const auto point = steering_point(*enemy_, Grid::cell_size * waypoint_reach_cells))
        accelerate_toward(*enemy_, *point, enemy_->get_world().enemy_tuning().pursue_force, delta);
}

void PursueState::update() {
    if (enemy_->line_of_sight) {
        enemy_->path.clear();
        enemy_->set_state(enemy_->get_attack_state());
        return;
    }
    if (enemy_->pursue_countdown_passed()) {
        enemy_->path.clear();
        enemy_->set_state(enemy_->get_wander_state());
        return;
    }

    if (enemy_->get_cooldown() <= 0.0f) {
        enemy_->reset_cooldown();
        enemy_->pursue_countdown();
        if (enemy_->get_ammo() > 6)
            enemy_->ammo_recovered();
    }
    follow_target(*enemy_);
}

void RecoverState::execute(const float delta) {
    // The last waypoint is the shelter itself: the enemy goes all the way to its center.
    const float reach =
        (enemy_->path.size() > 1 ? waypoint_reach_cells : shelter_reach_cells) * Grid::cell_size;
    if (const auto point = steering_point(*enemy_, reach))
        accelerate_toward(*enemy_, *point, enemy_->get_world().enemy_tuning().recover_force, delta);
}

void RecoverState::update() {
    if (enemy_->has_reloaded_ammo()) {
        enemy_->fear = dotio::math::uniform(enemy_->get_world().behavior_random(), Enemy::spawn_fear_min,
                                            Enemy::reloaded_fear_max);
        enemy_->path.clear();
        enemy_->reset_pursue();
        enemy_->set_state(enemy_->get_pursue_state());
        return;
    }

    if (!enemy_->line_of_sight && enemy_->get_cooldown() <= 0.0f) {
        enemy_->reset_cooldown();
        enemy_->ammo_recovered();
    }

    // Seen without a shelter to go to, or caught near the player: look for a (new) shelter.
    const bool seen_without_route = enemy_->line_of_sight && enemy_->path.empty();
    const bool player_close = std::abs(enemy_->position.x - enemy_->target.x) <= Grid::cell_size * 2.5f &&
                              std::abs(enemy_->position.y - enemy_->target.y) <= Grid::cell_size * 2.5f;
    if (!seen_without_route && !player_close)
        return;
    if (cell_of(enemy_->position).row < 0)
        return;
    if (auto shelter =
            plan_shelter(enemy_->get_world().grid(), enemy_->position, enemy_->target, enemy_->fear);
        !shelter.empty())
        enemy_->path = std::move(shelter);
    else if (enemy_->hits_remaining == 1)
        enemy_->set_state(enemy_->get_kamikaze_state());
}

void KamikazeState::execute(const float delta) {
    const float force = enemy_->get_world().enemy_tuning().kamikaze_force;
    if (enemy_->line_of_sight) {
        accelerate_toward(*enemy_, enemy_->target, force, delta);
        enemy_->path.clear();
        return;
    }
    if (const auto point = steering_point(*enemy_, Grid::cell_size * waypoint_reach_cells))
        accelerate_toward(*enemy_, *point, force, delta);
}

void KamikazeState::update() {
    if (enemy_->line_of_sight && !enemy_->is_out_of_ammo() && enemy_->get_cooldown() <= 0.0f) {
        enemy_->reset_cooldown();
        enemy_->ammo_fired();
        const auto &settings = enemy_->get_world().difficulty_settings();
        enemy_->get_world().add_arrow(enemy_->position,
                                      aimed_arrow(*enemy_, settings.projectile_speed, settings.prediction));
    } else if (!enemy_->line_of_sight && enemy_->get_ammo() > 5 && enemy_->get_cooldown() <= 0.0f) {
        enemy_->ammo_recovered();
    }
    if (!enemy_->line_of_sight)
        follow_target(*enemy_);
}
} // namespace dotio

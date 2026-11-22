#include "dotio/application.h"

#include <limits>
#include <utility>

namespace dotio {
void Application::select_enemy(kinetra::Vec2 point) {
    const auto local = from_world(point);
    auto &enemies = world_->get_enemies();
    float nearest = std::numeric_limits<float>::infinity();
    for (std::size_t slot = 0; slot < enemies.get_size(); ++slot) {
        const auto &enemy = enemies[slot];
        if (!enemy.is_active)
            continue;
        const auto delta = enemy.position - local;
        const float distance_squared = delta.x * delta.x + delta.y * delta.y;
        if (distance_squared <= enemy.radius * enemy.radius && distance_squared < nearest) {
            nearest = distance_squared;
            selected_enemy_ = EnemySelection{slot, enemy.generation()};
        }
    }
}

const Enemy *Application::selected_enemy() const {
    if (!selected_enemy_)
        return nullptr;
    const auto &enemy = world_->get_enemies()[selected_enemy_->slot];
    return enemy.is_active && enemy.generation() == selected_enemy_->generation ? &enemy : nullptr;
}

std::optional<kinetra::InspectorSnapshot> Application::inspect_enemy() const {
    return selected_enemy_ ? inspect_enemy(selected_enemy_->slot, selected_enemy_->generation) : std::nullopt;
}

std::optional<kinetra::InspectorSnapshot> Application::inspect_enemy(std::size_t slot,
                                                                     std::uint64_t generation) const {
    auto &enemies = world_->get_enemies();
    if (slot >= enemies.get_size())
        return std::nullopt;
    auto &state = enemies[slot];
    if (!state.is_active || state.generation() != generation)
        return std::nullopt;
    const auto *enemy = &state;
    using kinetra::inspector_number;
    using kinetra::inspector_vector;
    const auto &tuning = world_->enemy_tuning();
    // Enemy's state predicates predate const access. The pool still owns the live object.
    const char *activity = "WANDER";
    float force = tuning.wander_force;
    if (state.is_kamikazing()) {
        activity = "KAMIKAZE";
        force = tuning.kamikaze_force;
    } else if (state.is_recovering()) {
        activity = "RECOVER";
        force = tuning.recover_force;
    } else if (state.is_pursueing()) {
        activity = "PURSUE";
        force = tuning.pursue_force;
    } else if (state.is_attacking()) {
        activity = "ATTACK";
        force = tuning.attack_force;
    }
    const auto offset = world_->world_offset();
    const auto location = [&](dotio::math::Vec2 point) {
        point += offset;
        return inspector_vector({point.x, point.y}, " PX");
    };
    const auto tuning_multiplier = world_->difficulty_settings().movement_multiplier;
    const auto steering = enemy->steering();
    kinetra::InspectorGroup route{
        "ROUTE",
        {{"GOAL", enemy->path.empty() ? "NONE" : location(enemy->path.destination())},
         {"WAYPOINTS LEFT", std::to_string(enemy->path.size())}}};
    if (state.is_attacking() || state.is_pursueing() || state.is_kamikazing())
        route.rows.push_back({"PLAYER TARGET", location(enemy->target)});
    if (state.is_wandering() && tuning.wander == WanderMode::steering)
        route.rows.push_back({"HEADING", inspector_number(enemy->wander_heading) + " RAD"});
    std::size_t i = 0;
    for (const auto point : enemy->path.remaining())
        route.rows.push_back({std::to_string(++i), location(point)});
    return kinetra::InspectorSnapshot{
        "ENEMY " + std::to_string(slot) + "-" + std::to_string(generation),
        {{"STATE",
          {{"ACTIVITY", activity},
           {"FROZEN", enemy->frozen ? "YES" : "NO"},
           {"HITS LEFT", std::to_string(enemy->hits_remaining)},
           {"POSITION", location(enemy->position)}}},
         {"MOTION",
          {{"WANDER MODE", tuning.wander == WanderMode::steering ? "STEERING" : "ROUTE"},
           {"MOVEMENT MULTIPLIER", inspector_number(tuning_multiplier)},
           {"STATE FORCE", inspector_number(force) + " PX/UPDATE/UPDATE"},
           {"FRICTION", inspector_number(enemy->get_friction())},
           {"SPEED",
            inspector_number(kinetra::length({enemy->velocity.x, enemy->velocity.y})) + " PX/UPDATE"},
           {"STEERING", inspector_vector({steering.x, steering.y}, " PX/UPDATE")}}},
         std::move(route)}};
}
} // namespace dotio

#include "dotio/ai/state.h"
#include "dotio/ai/fuzzy.h"
#include "dotio/ai/wander.h"
#include "dotio/entities/enemy.h"
#include "dotio/math/portable.h"
#include "dotio/math/random.h"
#include "dotio/navigation/grid.h"
#include "dotio/navigation/route.h"
#include "dotio/world.h"

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
std::optional<dotio::math::Vec2> steering_point(Enemy &enemy, float reach) {
    auto &route = enemy.Path;
    if (route.empty())
        return std::nullopt;
    auto point = route.next();
    if ((point - enemy.Position) < reach) {
        route.advance();
        if (!route.empty())
            point = route.next();
    }
    return point;
}

/// Accelerates `enemy` toward `point` by `force` legacy pixels per update, over `delta`
/// updates and scaled by the difficulty's movement multiplier, as every state does.
void accelerate_toward(Enemy &enemy, dotio::math::Vec2 point, float force, float delta) {
    if (point == enemy.Position)
        return;
    const dotio::math::Vec2 acceleration = (point - enemy.Position).GetNormalized() * force;
    enemy.ApplyForce(acceleration * (delta * enemy.GetWorld().difficulty_settings().movement_multiplier),
                     0.5f * enemy.GetMass());
}

/// Cell of `point`, or {-1, -1} outside the grid.
dotio::math::GridEntry cell_of(dotio::math::Vec2 point) {
    dotio::math::GridEntry cell;
    if (!dotio::math::GridEntry::FromWorld(point, Grid::Origin, Grid::CellSize, Grid::GridSize, cell))
        return {-1, -1};
    return cell;
}

/// Plans a new route to the enemy's target when it has none or its route ends in another
/// cell than the target's: the target moved since the route was planned. A failed plan keeps
/// the old route, as the original game did.
void follow_target(Enemy &enemy) {
    if (!enemy.Path.empty() && cell_of(enemy.Path.destination()) == cell_of(enemy.Target))
        return;
    if (auto route = plan_route(enemy.GetWorld().grid(), enemy.Position, enemy.Target); !route.empty())
        enemy.Path = std::move(route);
}

/// Center of `cell` in legacy pixels.
dotio::math::Vec2 center_of(dotio::math::GridEntry cell) {
    return {(float(cell.Column) + 0.5f) * Grid::CellSize, (float(cell.Row) + 0.5f) * Grid::CellSize};
}
} // namespace

void WanderState::Execute(const float delta) {
    if (_enemy->GetWorld().enemy_tuning().wander == WanderMode::steering) {
        SteerWander(*_enemy, delta);
        return;
    }
    const auto point = steering_point(*_enemy, Grid::CellSize * waypoint_reach_cells);
    if (!point)
        return;
    // The original game aims at a random point around the waypoint, wider than tall, every
    // update, so path wanderers sway along their route.
    const float angle =
        dotio::math::unit(_enemy->GetWorld().behavior_random()) * 2 * std::numbers::pi_v<float>;
    const dotio::math::Vec2 displacement(Grid::CellSize * std::cos(angle),
                                         Grid::CellSize * 0.4f * std::sin(angle));
    const dotio::math::Vec2 force = (*point + displacement - _enemy->Position).GetNormalized() *
                                    _enemy->GetWorld().enemy_tuning().wander_force;
    _enemy->ApplyForce(force * (delta * _enemy->GetWorld().difficulty_settings().movement_multiplier),
                       0.5f * _enemy->GetMass());
}

void WanderState::Update() {
    if (_enemy->LineOfSight) {
        _enemy->Path.clear();
        _enemy->SetState(_enemy->GetAttackState());
        return;
    }

    if (_enemy->GetWorld().enemy_tuning().wander == WanderMode::steering) {
        DriftWanderHeading(*_enemy);
        return;
    }

    if (_enemy->Path.empty()) {
        if (const auto cell = cell_of(_enemy->Position); cell.Row >= 0) {
            auto &world = _enemy->GetWorld();
            const auto goal = world.grid().GenerateGridEntry(cell, world.behavior_random());
            _enemy->Path = plan_route(world.grid(), _enemy->Position, center_of(goal));
        }
    }
}

void AttackState::Execute(const float delta) {
    bool boundary =
        sqrt(pow(_enemy->Position.X - _enemy->Target.X, 2) + pow(_enemy->Position.Y - _enemy->Target.Y, 2)) <
        (dotio::Grid::CellSize * (_enemy->Fear * (_enemy->AmmoUsedRatio())) + dotio::Grid::CellSize * 0.5f);

    dotio::math::Vec2 force = {
        (boundary ? _enemy->Position - _enemy->Target : _enemy->Target - _enemy->Position).GetNormalized() *
        _enemy->GetWorld().enemy_tuning().attack_force};

    _enemy->ApplyForce(force * (delta * _enemy->GetWorld().difficulty_settings().movement_multiplier),
                       0.5f * _enemy->GetMass());
}

void AttackState::Update() {
    // Kamikaze is a last-health behavior; fuzzy combat rules still choose when to commit.
    if (_enemy->HitsRemaining == 1 &&
        dotio::Fuzzy::Evaluate(_enemy->Distance(), _enemy->Fear, _enemy->GetAmmo())) {
        _enemy->SetState(_enemy->GetKamikazeState());
        return;
    }
    if (_enemy->IsOutOfAmmo()) {
        _enemy->SetState(_enemy->GetRecoverState());
        return;
    }
    if (!_enemy->LineOfSight) {
        _enemy->ResetPursue();
        _enemy->SetState(_enemy->GetPursueState());
        return;
    }

    if (_enemy->GetCooldown() <= 0.0f) {
        _enemy->ResetCooldown();
        _enemy->AmmoFired();
        const auto &settings = _enemy->GetWorld().difficulty_settings();
        const float arrowSpeed = settings.projectile_speed;
        dotio::math::Vector2 shoot = {arrowSpeed, 0.0f};
        shoot.Rotate(_enemy->Position.SignedAngleDifference(shoot, _enemy->Target) +
                     settings.prediction *
                         dotio::math::Vec2::Intercept(
                             _enemy->TargetVelocity.GetLength(), arrowSpeed,
                             _enemy->Target.SignedAngleDifference(_enemy->TargetVelocity, _enemy->Position)));
        _enemy->GetWorld().AddArrow(_enemy->Position, shoot);
    }
}

void PursueState::Execute(const float delta) {
    if (const auto point = steering_point(*_enemy, Grid::CellSize * waypoint_reach_cells))
        accelerate_toward(*_enemy, *point, _enemy->GetWorld().enemy_tuning().pursue_force, delta);
}

void PursueState::Update() {
    if (_enemy->LineOfSight) {
        _enemy->Path.clear();
        _enemy->SetState(_enemy->GetAttackState());
        return;
    }
    if (_enemy->PursueCountdownPassed()) {
        _enemy->Path.clear();
        _enemy->SetState(_enemy->GetWanderState());
        return;
    }

    if (_enemy->GetCooldown() <= 0.0f) {
        _enemy->ResetCooldown();
        _enemy->PursueCountdown();
        if (_enemy->GetAmmo() > 6)
            _enemy->AmmoRecovered();
    }
    follow_target(*_enemy);
}

void RecoverState::Execute(const float delta) {
    // The last waypoint is the shelter itself: the enemy goes all the way to its center.
    const float reach =
        (_enemy->Path.size() > 1 ? waypoint_reach_cells : shelter_reach_cells) * Grid::CellSize;
    if (const auto point = steering_point(*_enemy, reach))
        accelerate_toward(*_enemy, *point, _enemy->GetWorld().enemy_tuning().recover_force, delta);
}

void RecoverState::Update() {
    if (_enemy->HasReloadedAmmo()) {
        _enemy->Fear = dotio::math::uniform(_enemy->GetWorld().behavior_random(), Enemy::spawn_fear_min,
                                            Enemy::reloaded_fear_max);
        _enemy->Path.clear();
        _enemy->ResetPursue();
        _enemy->SetState(_enemy->GetPursueState());
        return;
    }

    if (!_enemy->LineOfSight && _enemy->GetCooldown() <= 0.0f) {
        _enemy->ResetCooldown();
        _enemy->AmmoRecovered();
    }

    // Seen without a shelter to go to, or caught near the player: look for a (new) shelter.
    const bool seen_without_route = _enemy->LineOfSight && _enemy->Path.empty();
    const bool player_close = std::abs(_enemy->Position.X - _enemy->Target.X) <= Grid::CellSize * 2.5f &&
                              std::abs(_enemy->Position.Y - _enemy->Target.Y) <= Grid::CellSize * 2.5f;
    if (!seen_without_route && !player_close)
        return;
    if (cell_of(_enemy->Position).Row < 0)
        return;
    if (auto shelter =
            plan_shelter(_enemy->GetWorld().grid(), _enemy->Position, _enemy->Target, _enemy->Fear);
        !shelter.empty())
        _enemy->Path = std::move(shelter);
    else if (_enemy->HitsRemaining == 1)
        _enemy->SetState(_enemy->GetKamikazeState());
}

void KamikazeState::Execute(const float delta) {
    const float force = _enemy->GetWorld().enemy_tuning().kamikaze_force;
    if (_enemy->LineOfSight) {
        accelerate_toward(*_enemy, _enemy->Target, force, delta);
        _enemy->Path.clear();
        return;
    }
    if (const auto point = steering_point(*_enemy, Grid::CellSize * waypoint_reach_cells))
        accelerate_toward(*_enemy, *point, force, delta);
}

void KamikazeState::Update() {
    if (_enemy->LineOfSight && !_enemy->IsOutOfAmmo() && _enemy->GetCooldown() <= 0.0f) {
        _enemy->ResetCooldown();
        _enemy->AmmoFired();
        const auto &settings = _enemy->GetWorld().difficulty_settings();
        const float arrowSpeed = settings.projectile_speed;
        dotio::math::Vector2 shoot = {arrowSpeed, 0.0f};
        shoot.Rotate(_enemy->Position.SignedAngleDifference(shoot, _enemy->Target) +
                     settings.prediction *
                         dotio::math::Vec2::Intercept(
                             _enemy->TargetVelocity.GetLength(), arrowSpeed,
                             _enemy->Target.SignedAngleDifference(_enemy->TargetVelocity, _enemy->Position)));
        _enemy->GetWorld().AddArrow(_enemy->Position, shoot);
    } else if (!_enemy->LineOfSight && _enemy->GetAmmo() > 5 && _enemy->GetCooldown() <= 0.0f) {
        _enemy->AmmoRecovered();
    }
    if (!_enemy->LineOfSight)
        follow_target(*_enemy);
}
} // namespace dotio

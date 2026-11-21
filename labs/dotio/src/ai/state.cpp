#include "dotio/ai/state.h"
#include "dotio/ai/fuzzy.h"
#include "dotio/ai/wander.h"
#include "dotio/entities/enemy.h"
#include "dotio/math/portable.h"
#include "dotio/navigation/grid.h"
#include "dotio/world.h"

namespace dotio {
void WanderState::Execute(const float delta) {
    if (_enemy->GetWorld().enemy_tuning().wander == WanderMode::steering) {
        SteerWander(*_enemy, delta);
        return;
    }

    if (_enemy->Path.empty())
        return;

    dotio::math::GridEntry cell = _enemy->Path.top();
    dotio::math::Coordinates pos =
        dotio::math::GridEntry::ToWorld(cell.Row, cell.Column, dotio::Grid::CellSize, dotio::Grid::Origin);
    pos.X += dotio::Grid::CellSize / 2;
    pos.Y += dotio::Grid::CellSize / 2;

    if ((pos - _enemy->Position) < dotio::Grid::CellSize * 0.666) {
        _enemy->Path.pop();
        if (!_enemy->Path.empty()) {
            cell = _enemy->Path.top();
            pos = dotio::math::GridEntry::ToWorld(cell.Row, cell.Column, dotio::Grid::CellSize,
                                                  dotio::Grid::Origin);
            pos.X += dotio::Grid::CellSize / 2;
            pos.Y += dotio::Grid::CellSize / 2;
        }
    }

    float angle = static_cast<float>(std::rand()) / RAND_MAX * 2 * std::numbers::pi_v<float>;
    dotio::math::Vec2 displacement((dotio::Grid::CellSize)*std::cos(angle),
                                   (dotio::Grid::CellSize * 0.4f) * std::sin(angle));
    dotio::math::Vec2 target = pos + displacement;
    dotio::math::Vec2 force =
        (target - _enemy->Position).GetNormalized() * _enemy->GetWorld().enemy_tuning().wander_force;
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
        dotio::math::GridEntry enemy_output;
        if (dotio::math::GridEntry::FromWorld(_enemy->Position, dotio::Grid::Origin, dotio::Grid::CellSize,
                                              dotio::Grid::GridSize, enemy_output)) {
            dotio::math::GridEntry enemy_grid = {enemy_output.Row, enemy_output.Column};
            if (dotio::Vertex _; _enemy->GetWorld().GetGraph().AStar(
                    enemy_grid, _enemy->GetWorld().grid().GenerateGridEntry(enemy_grid), _))
                _enemy->Path = _enemy->GetWorld().GetGraph().GetPath(enemy_grid, _);
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
    if (_enemy->Path.empty())
        return;

    dotio::math::GridEntry cell = _enemy->Path.top();
    dotio::math::Coordinates pos =
        dotio::math::GridEntry::ToWorld(cell.Row, cell.Column, dotio::Grid::CellSize, dotio::Grid::Origin);
    pos.X += dotio::Grid::CellSize / 2;
    pos.Y += dotio::Grid::CellSize / 2;

    if ((pos - _enemy->Position) < dotio::Grid::CellSize * 0.666) {
        _enemy->Path.pop();
        if (!_enemy->Path.empty()) {
            cell = _enemy->Path.top();
            pos = dotio::math::GridEntry::ToWorld(cell.Row, cell.Column, dotio::Grid::CellSize,
                                                  dotio::Grid::Origin);
            pos.X += dotio::Grid::CellSize / 2;
            pos.Y += dotio::Grid::CellSize / 2;
        }
    }

    if (pos != _enemy->Position) {
        dotio::math::Vec2 force = {(pos - _enemy->Position).GetNormalized() *
                                   _enemy->GetWorld().enemy_tuning().pursue_force};
        _enemy->ApplyForce(force * (delta * _enemy->GetWorld().difficulty_settings().movement_multiplier),
                           0.5f * _enemy->GetMass());
    }
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

    dotio::math::GridEntry target_output{};
    dotio::math::GridEntry::FromWorld(_enemy->Target, dotio::Grid::Origin, dotio::Grid::CellSize,
                                      dotio::Grid::GridSize, target_output);
    if (_enemy->Path.empty() ||
        dotio::math::GridEntry::PathRefresh(_enemy->Path.get(0), {target_output.Row, target_output.Column})) {
        dotio::math::GridEntry enemy_output;
        if (dotio::math::GridEntry::FromWorld(_enemy->Target, dotio::Grid::Origin, dotio::Grid::CellSize,
                                              dotio::Grid::GridSize, target_output) &&
            dotio::math::GridEntry::FromWorld(_enemy->Position, dotio::Grid::Origin, dotio::Grid::CellSize,
                                              dotio::Grid::GridSize, enemy_output)) {
            if (dotio::Vertex _; _enemy->GetWorld().GetGraph().AStar(
                    {enemy_output.Row, enemy_output.Column}, {target_output.Row, target_output.Column}, _))
                _enemy->Path =
                    _enemy->GetWorld().GetGraph().GetPath({enemy_output.Row, enemy_output.Column}, _);
        }
    }
}

void RecoverState::Execute(const float delta) {
    if (_enemy->Path.empty())
        return;

    dotio::math::GridEntry cell = _enemy->Path.top();
    dotio::math::Coordinates pos =
        dotio::math::GridEntry::ToWorld(cell.Row, cell.Column, dotio::Grid::CellSize, dotio::Grid::Origin);
    pos.X += dotio::Grid::CellSize / 2;
    pos.Y += dotio::Grid::CellSize / 2;

    if ((pos - _enemy->Position) < dotio::Grid::CellSize * (_enemy->Path.size() > 1 ? 0.666 : 0.1)) {
        _enemy->Path.pop();
        if (!_enemy->Path.empty()) {
            cell = _enemy->Path.top();
            pos = dotio::math::GridEntry::ToWorld(cell.Row, cell.Column, dotio::Grid::CellSize,
                                                  dotio::Grid::Origin);
            pos.X += dotio::Grid::CellSize / 2;
            pos.Y += dotio::Grid::CellSize / 2;
        }
    }

    if (pos != _enemy->Position) {
        dotio::math::Vec2 force = {(pos - _enemy->Position).GetNormalized() *
                                   _enemy->GetWorld().enemy_tuning().recover_force};
        _enemy->ApplyForce(force * (delta * _enemy->GetWorld().difficulty_settings().movement_multiplier),
                           0.5f * _enemy->GetMass());
    }
}

void RecoverState::Update() {
    if (_enemy->HasReloadedAmmo()) {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<float> dis(2.0f, 6.0f);
        _enemy->Fear = dis(gen);
        _enemy->Path.clear();
        _enemy->ResetPursue();
        _enemy->SetState(_enemy->GetPursueState());
        return;
    }

    if (!_enemy->LineOfSight && _enemy->GetCooldown() <= 0.0f) {
        _enemy->ResetCooldown();
        _enemy->AmmoRecovered();
    }

    if ((_enemy->LineOfSight && _enemy->Path.empty()) ||
        ((abs(_enemy->Position.X - _enemy->Target.X) <= dotio::Grid::CellSize * 2.5f) &&
         ((abs(_enemy->Position.Y - _enemy->Target.Y) <= dotio::Grid::CellSize * 2.5f)))) {
        dotio::math::GridEntry enemy_output;
        if (dotio::math::GridEntry::FromWorld(_enemy->Position, dotio::Grid::Origin, dotio::Grid::CellSize,
                                              dotio::Grid::GridSize, enemy_output)) {
            if (dotio::Vertex _; _enemy->GetWorld().GetGraph().HideStar(
                    _enemy->GetWorld().grid(), {enemy_output.Row, enemy_output.Column}, _enemy->Target,
                    _enemy->Fear, _))
                _enemy->Path =
                    _enemy->GetWorld().GetGraph().GetPath({enemy_output.Row, enemy_output.Column}, _);
            else if (_enemy->HitsRemaining == 1)
                _enemy->SetState(_enemy->GetKamikazeState());
        }
    }
}

void KamikazeState::Execute(const float delta) {
    if (_enemy->LineOfSight) {
        dotio::math::Vec2 force = {(_enemy->Target - _enemy->Position).GetNormalized() *
                                   _enemy->GetWorld().enemy_tuning().kamikaze_force};
        _enemy->ApplyForce(force * (delta * _enemy->GetWorld().difficulty_settings().movement_multiplier),
                           0.5f * _enemy->GetMass());
        _enemy->Path.clear();
        return;
    }

    if (_enemy->Path.empty())
        return;

    dotio::math::GridEntry cell = _enemy->Path.top();
    dotio::math::Coordinates pos =
        dotio::math::GridEntry::ToWorld(cell.Row, cell.Column, dotio::Grid::CellSize, dotio::Grid::Origin);
    pos.X += dotio::Grid::CellSize / 2;
    pos.Y += dotio::Grid::CellSize / 2;

    if ((pos - _enemy->Position) < dotio::Grid::CellSize * 0.666) {
        _enemy->Path.pop();
        if (!_enemy->Path.empty()) {
            cell = _enemy->Path.top();
            pos = dotio::math::GridEntry::ToWorld(cell.Row, cell.Column, dotio::Grid::CellSize,
                                                  dotio::Grid::Origin);
            pos.X += dotio::Grid::CellSize / 2;
            pos.Y += dotio::Grid::CellSize / 2;
        }
    }

    if (pos != _enemy->Position) {
        dotio::math::Vec2 force = {(pos - _enemy->Position).GetNormalized() *
                                   _enemy->GetWorld().enemy_tuning().kamikaze_force};
        _enemy->ApplyForce(force * (delta * _enemy->GetWorld().difficulty_settings().movement_multiplier),
                           0.5f * _enemy->GetMass());
    }
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

    dotio::math::GridEntry target_output{};
    dotio::math::GridEntry::FromWorld(_enemy->Target, dotio::Grid::Origin, dotio::Grid::CellSize,
                                      dotio::Grid::GridSize, target_output);
    if (!_enemy->LineOfSight &&
        (_enemy->Path.empty() || dotio::math::GridEntry::PathRefresh(
                                     _enemy->Path.get(0), {target_output.Row, target_output.Column}))) {
        dotio::math::GridEntry enemy_output;
        if (dotio::math::GridEntry::FromWorld(_enemy->Target, dotio::Grid::Origin, dotio::Grid::CellSize,
                                              dotio::Grid::GridSize, target_output) &&
            dotio::math::GridEntry::FromWorld(_enemy->Position, dotio::Grid::Origin, dotio::Grid::CellSize,
                                              dotio::Grid::GridSize, enemy_output)) {
            if (dotio::Vertex _; _enemy->GetWorld().GetGraph().AStar(
                    {enemy_output.Row, enemy_output.Column}, {target_output.Row, target_output.Column}, _))
                _enemy->Path =
                    _enemy->GetWorld().GetGraph().GetPath({enemy_output.Row, enemy_output.Column}, _);
        }
    }
}
} // namespace dotio
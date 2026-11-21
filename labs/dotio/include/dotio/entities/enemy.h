#ifndef ENEMY_H
#define ENEMY_H

#include "dotio/ai/state.h"
#include "dotio/entities/physics_object.h"
#include "dotio/navigation/stack.h"

namespace dotio {
class World;

/// Stateful game enemy; embedded state objects share one stable owning enemy.
class Enemy final : public PhysicsObject {
    float _cooldown{};
    float _maxCooldown{1.0f};
    int _ammo{1};
    int _countdown{0};
    static constexpr int ammo_capacity = 10;
    World *_world{};
    State *_state{};
    AttackState _attack;
    WanderState _wander;
    PursueState _pursue;
    RecoverState _recover;
    KamikazeState _kamikaze;

    void PlayerBumpedIntoEnemy();
    void SpatialPartitioning(const float delta);

  public:
    /// Range of Fear for a spawned or reset enemy, drawn from World::behavior_random(). Fear
    /// is in grid cells: the distance an attacking enemy keeps grows with it.
    static constexpr float spawn_fear_min = 2.0f, spawn_fear_max = 10.0f;
    /// Largest Fear after a reload: a reloaded enemy is bolder than a fresh one.
    static constexpr float reloaded_fear_max = 6.0f;

    dotio::math::Vec2 Target{};
    dotio::math::Vec2 TargetVelocity{};
    dotio::Stack Path;
    float Fear{};
    int HitsRemaining{1};
    bool LineOfSight{false}, Debug{false}, Frozen{};
    /// Direction this enemy wanders in under WanderMode::steering, in radians in legacy
    /// coordinates: it points along (cos, sin), and +Y is down the screen. Kept in
    /// [-pi, pi]; Initialize() and Reset() draw a new one from World::wander_random().
    float WanderHeading{};
    /// Side this enemy turns to first when a wall blocks its wander heading: +1 adds to
    /// WanderHeading, -1 subtracts. It changes only when the other side is the free one, so
    /// the enemy does not turn back and forth in front of a wall.
    float WanderSide{1.f};

    Enemy();

    explicit Enemy(World *world, dotio::math::Position position, dotio::math::Position target,
                   float direction, float fear);

    ~Enemy() override = default;

    void Update(float delta) override;

    void Tick() override;

    void Draw(dotio::Renderer &renderer) override;

    void Initialize();

    World &GetWorld();

    State &GetState();
    void SetState(State &state);

    State &GetWanderState();
    State &GetAttackState();
    State &GetPursueState();
    State &GetRecoverState();
    State &GetKamikazeState();

    bool IsAttacking();
    bool IsWandering();
    bool IsPursueing();
    bool IsRecovering();
    bool IsKamikazing();
    bool IsInLineOfSight();
    bool IsOutOfAmmo() const;
    bool HasReloadedAmmo() const;
    bool PursueCountdownPassed() const;

    int GetAmmo() const;
    float GetCooldown() const;
    float AmmoUsedRatio() const;
    float Distance() const;

    void AmmoFired();
    void AmmoRecovered();
    void PursueCountdown();
    void ResetCooldown();
    void ResetPursue();
    void Reset();
};
} // namespace dotio

#endif // ENEMY_H

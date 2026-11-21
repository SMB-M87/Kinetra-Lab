#pragma once

namespace dotio {
class Enemy;

/// One AI state of an enemy (State pattern). Each Enemy embeds one object of every state and
/// points at the current one; a state switches by calling Enemy::set_state.
class State {
  protected:
    /// The enemy this state drives; borrowed, the enemy embeds this state.
    Enemy *enemy_{};

  public:
    /// Binds the state to the enemy that embeds it; Enemy::initialize() calls it.
    void set_enemy(Enemy &enemy) { enemy_ = &enemy; }
    /// Movement of one update (60 Hz) over `delta` updates: accelerates the enemy.
    virtual void execute(const float delta) = 0;
    /// The decision tick (20 Hz): fires, plans routes and chooses the next state.
    virtual void update() {};
};

/// Holds a fear-dependent distance from a visible player and shoots ahead of it. Loses sight:
/// pursue; out of ammunition: recover; close, warrior fear and midway ammunition at one hit
/// left (fuzzy rule): kamikaze.
struct AttackState : State {
    void execute(const float delta) override;
    void update() override;
};

/// Roams until the player comes into sight, then attacks. EnemyTuning::wander chooses how:
/// by steering along Enemy::wander_heading (wander.h), or along a route (plan_route) to a
/// random free cell at most five cells away.
struct WanderState : State {
    void execute(const float delta) override;
    void update() override;
};

/// Follows a route to the player after losing sight of it, replanned when the player leaves
/// the route's last cell. Sight: attack; DifficultySettings::pursuit_cycles cooldowns without
/// sight: wander. Recovers ammunition above six.
struct PursueState : State {
    void execute(const float delta) override;
    void update() override;
};

/// Out of ammunition: runs to a shelter out of the player's sight (plan_shelter) and reloads
/// two arrows per cooldown while unseen. Reloaded: pursue with a new, lower fear. No shelter
/// at one hit left: kamikaze.
struct RecoverState : State {
    void execute(const float delta) override;
    void update() override;
};

/// Runs straight at a visible player, or along a route to a hidden one, shoots what it has
/// left, and explodes on contact. Has no way out.
struct KamikazeState : State {
    void execute(const float delta) override;
    void update() override;
};
} // namespace dotio

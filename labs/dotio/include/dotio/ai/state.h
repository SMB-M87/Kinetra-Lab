#pragma once

namespace dotio {
class Enemy;

class State {
  protected:
    Enemy *enemy_{};

  public:
    void set_enemy(Enemy &enemy) { enemy_ = &enemy; }
    virtual void execute(const float delta) = 0;
    virtual void update() {};
};

struct AttackState : State {
    void execute(const float delta) override;
    void update() override;
};

/// Roams until the player comes into sight, then attacks. EnemyTuning::wander chooses how:
/// by steering along Enemy::WanderHeading (wander.h) or along an A* path to a random cell.
struct WanderState : State {
    void execute(const float delta) override;
    void update() override;
};

struct PursueState : State {
    void execute(const float delta) override;
    void update() override;
};

struct RecoverState : State {
    void execute(const float delta) override;
    void update() override;
};

struct KamikazeState : State {
    void execute(const float delta) override;
    void update() override;
};
} // namespace dotio

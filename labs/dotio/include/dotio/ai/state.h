#ifndef STATE_H
#define STATE_H

namespace dotio {
class Enemy;

class State {
  protected:
    Enemy *_enemy{};

  public:
    void SetEnemy(Enemy &enemy) { _enemy = &enemy; }
    virtual void Execute(const float delta) = 0;
    virtual void Update() {};
};

struct AttackState : State {
    void Execute(const float delta) override;
    void Update() override;
};

/// Roams until the player comes into sight, then attacks. EnemyTuning::wander chooses how:
/// by steering along Enemy::WanderHeading (wander.h) or along an A* path to a random cell.
struct WanderState : State {
    void Execute(const float delta) override;
    void Update() override;
};

struct PursueState : State {
    void Execute(const float delta) override;
    void Update() override;
};

struct RecoverState : State {
    void Execute(const float delta) override;
    void Update() override;
};

struct KamikazeState : State {
    void Execute(const float delta) override;
    void Update() override;
};
} // namespace dotio

#endif // STATE_H
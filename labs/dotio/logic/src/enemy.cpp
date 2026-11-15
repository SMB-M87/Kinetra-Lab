#include "enemy.h"
#include "grid.h"
#include "physics_adapter.h"
#include "portable.h"
#include "state.h"
#include "world.h"

namespace logic {
Enemy::Enemy() : PhysicsObject() {
}

Enemy::Enemy(World *world, gmath::Position position, gmath::Position target, const float direction,
             const float fear)
    : PhysicsObject(60.0f, logic::Grid::CellSize * 0.2, 0.2f, 0.2f, position, direction, true), _world(world),
      Target(target), Path{}, Fear(fear) {
}

void Enemy::Update(const float delta) {
    if (Frozen || Target == Position)
        return;

    _state->Execute(delta);
    SpatialPartitioning(delta);
}

void Enemy::Tick() {
    if (Frozen || Target == Position)
        return;

    if (_cooldown > 0.0f)
        _cooldown -= _world->GetTickRate();
    if (_cooldown < 0.0f)
        _cooldown = 0.0f;

    PlayerBumpedIntoEnemy();
    if (!IsActive || Frozen)
        return;
    LineOfSight = IsInLineOfSight();
    _state->Update();
}

void Enemy::Draw(gamef::Renderer &renderer) {
    gmath::RGBA red = {1.0f, 0.0f, 0.0f, 1.0f};
    gmath::RGBA orange = {1.0f, 0.65f, 0.0f, 1.0f};
    gmath::RGBA kamikaze = {1.0f, 0.3f, 0.3f, 1.0f};
    gmath::RGBA green = {0.0f, 1.0f, 0.0f, 1.0f};
    gmath::RGBA blue = {0.0f, 0.5f, 1.0f, 1.0f};
    renderer.DrawCircle(Position, Radius,
                        IsKamikazing()   ? kamikaze
                        : !Debug         ? red
                        : IsRecovering() ? green
                        : IsAttacking()  ? red
                        : IsPursueing()  ? orange
                                         : blue);
}

void Enemy::PlayerBumpedIntoEnemy() {
    if (sqrt(pow(Position.X - _world->GetPlayer().Position.X, 2) +
             pow(Position.Y - _world->GetPlayer().Position.Y, 2)) < (Radius * 2)) {
        if (IsKamikazing()) {
            if (!Debug)
                _world->GetPlayer().Health -= 30.0f;
            if (Debug && _world->GetPlayer().Health <= 0.0f)
                _world->GetPlayer().Health = 0.1f;
            else if (_world->GetPlayer().Health <= 0.0f)
                _world->Reset();
        } else {
            if (_world->GetPlayer().Mana < _world->GetPlayer().MaxMana)
                _world->GetPlayer().Mana += IsRecovering() ? 40.0f : 20.0f;
            if (IsRecovering() && _world->GetPlayer().Health < _world->GetPlayer().MaxHealth)
                _world->GetPlayer().Health += 40.0f;
        }
        IsActive = false;
    }

    if (!IsActive) {
        // All replacements go through timed offscreen spawning, including contact kills.
        Frozen = true;
        Path.clear();
        Velocity = {};
    }
}

void Enemy::SpatialPartitioning(const float delta) {
    auto motion = dotio::physics_body(*this);
    kinetra::integrate(motion, delta, _friction);
    Velocity = dotio::legacy_vector(motion.velocity);

    gmath::Vector2 next{Position + Velocity};
    uint8_t p =
        gmath::GridEntry::GetSquarePartition(next, Radius, logic::Grid::Origin, logic::Grid::CellSize);
    gmath::GridEntry partitions[4]{{-1, -1}, {-1, -1}, {-1, -1}, {-1, -1}};

    gmath::GridEntry::GetPartitions(p, next, logic::Grid::Origin, logic::Grid::CellSize,
                                    logic::Grid::GridSize, partitions);

    bool keep[4]{false, false, false, false};

    for (int i = 0; i < 4; i++) {
        gmath::GridEntry *np = &partitions[i];

        if (_world->EnemyPartitionMap.contains(*np))
            for (auto o : _world->EnemyPartitionMap[*np]) {
                logic::Enemy &other = *(logic::Enemy *)o;
                if (!other.IsActive || other.Frozen)
                    continue;

                if (&other == this)
                    continue;

                auto body = dotio::physics_body(*this);
                body.position = dotio::physics_vector(next);
                auto neighbor = dotio::physics_body(other);
                if (const auto contact = kinetra::collide(body, neighbor)) {
                    kinetra::resolve_contact(body, neighbor, *contact);
                    next = dotio::legacy_vector(body.position);
                    dotio::apply_body(other, neighbor);
                }
            }
    }

    Velocity = next - Position;

    motion = dotio::physics_body(*this);
    kinetra::integrate(motion, delta);
    dotio::apply_body(*this, motion);

    p = gmath::GridEntry::GetSquarePartition(Position, Radius, logic::Grid::Origin, logic::Grid::CellSize);
    partitions[0] = {-1, -1};
    partitions[1] = {-1, -1};
    partitions[2] = {-1, -1};
    partitions[3] = {-1, -1};

    gmath::GridEntry::GetPartitions(p, Position, logic::Grid::Origin, logic::Grid::CellSize,
                                    logic::Grid::GridSize, partitions);

    keep[0] = false;
    keep[1] = false;
    keep[2] = false;
    keep[3] = false;

    for (int i = 0; i < 4; i++) {
        gmath::GridEntry *np = &partitions[i];

        if (np->Row == -1)
            continue;

        if (_world->EnemyPartitionMap[*np].contains(this))
            keep[i] = true;
    }

    for (int i = 0; i < 4; i++) {
        gmath::GridEntry *np = &partitions[i];
        gmath::GridEntry *lp = &LastPartition[i];

        if (np->Row != lp->Row || np->Column != lp->Column) {
            if (np->Row == -1 || np->Column == -1) {
                if (_world->EnemyPartitionMap.contains(*lp))
                    _world->EnemyPartitionMap[*lp].erase(this);

                if (_world->EnemyPartitionMap[*lp].empty())
                    _world->EnemyPartitionMap.erase(*lp);

                *lp = *np;
                continue;
            }

            if (!keep[i]) {
                if (_world->EnemyPartitionMap.contains(*lp))
                    _world->EnemyPartitionMap[*lp].erase(this);

                _world->EnemyPartitionMap[*np].insert(this);

                if (_world->EnemyPartitionMap[*lp].empty())
                    _world->EnemyPartitionMap.erase(*lp);
            }

            *lp = *np;
        }
    }
}

void Enemy::Initialize() {
    HitsRemaining = _world->difficulty_settings().enemy_hits;
    if (_state == nullptr) {
        _state = &_wander;
        _attack.SetEnemy(*this);
        _wander.SetEnemy(*this);
        _pursue.SetEnemy(*this);
        _recover.SetEnemy(*this);
        _kamikaze.SetEnemy(*this);
    }
}

World &Enemy::GetWorld() {
    return *_world;
}

State &Enemy::GetState() {
    return *_state;
}
void Enemy::SetState(State &state) {
    _state = &state;
}

State &Enemy::GetAttackState() {
    return _attack;
}
State &Enemy::GetWanderState() {
    return _wander;
}
State &Enemy::GetPursueState() {
    return _pursue;
}
State &Enemy::GetRecoverState() {
    return _recover;
}
State &Enemy::GetKamikazeState() {
    return _kamikaze;
}

bool Enemy::IsAttacking() {
    return &GetState() == &GetAttackState();
}
bool Enemy::IsWandering() {
    return &GetState() == &GetWanderState();
}
bool Enemy::IsPursueing() {
    return &GetState() == &GetPursueState();
}
bool Enemy::IsRecovering() {
    return &GetState() == &GetRecoverState();
}
bool Enemy::IsKamikazing() {
    return &GetState() == &GetKamikazeState();
}
bool Enemy::IsInLineOfSight() {
    if (gmath::Point2 _; logic::Grid::IsIntersectingAnyCell(
            {Position, _world->GetPlayer().Position}, reinterpret_cast<const int *>(logic::Grid::Array), _))
        return false;
    else {
        return true;
    }
}
bool Enemy::IsOutOfAmmo() const {
    return _ammo >= ammo_capacity;
}
bool Enemy::HasReloadedAmmo() const {
    return _ammo <= 1;
}
bool Enemy::PursueCountdownPassed() const {
    return _countdown >= _world->difficulty_settings().pursuit_cycles;
}

int Enemy::GetAmmo() const {
    return _ammo;
}
float Enemy::GetCooldown() const {
    return _cooldown;
}
float Enemy::AmmoUsedRatio() const {
    return _ammo / static_cast<float>(ammo_capacity);
}
float Enemy::Distance() const {
    return max(abs(Position.X - Target.X), abs(Position.Y - Target.Y));
}

void Enemy::AmmoFired() {
    _ammo += 1;
}
void Enemy::AmmoRecovered() {
    _ammo -= 2;
}
void Enemy::PursueCountdown() {
    _countdown += 1;
}
void Enemy::ResetCooldown() {
    _cooldown = _maxCooldown * _world->difficulty_settings().fire_interval_multiplier;
}
void Enemy::ResetPursue() {
    _countdown = 0;
}
void Enemy::Reset() {
    HitsRemaining = _world->difficulty_settings().enemy_hits;
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> dis(2.0f, 10.0f);
    Fear = dis(gen);
    _state = &_wander;
    IsActive = true;
    ResetCooldown();
    _ammo = 1;
    _countdown = 0;
    Path.clear();
}
} // namespace logic

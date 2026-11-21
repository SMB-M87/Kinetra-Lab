#pragma once

#include "dotio/ai/state.h"
#include "dotio/entities/physics_object.h"
#include "dotio/navigation/route.h"

namespace dotio {
class World;

/// Stateful game enemy; embedded state objects share one stable owning enemy.
class Enemy final : public PhysicsObject {
    float cooldown_{};
    float max_cooldown_{1.0f};
    int ammo_{1};
    int countdown_{0};
    static constexpr int ammo_capacity = 10;
    World *world_{};
    State *state_{};
    AttackState attack_;
    WanderState wander_;
    PursueState pursue_;
    RecoverState recover_;
    KamikazeState kamikaze_;

    void player_bumped_into_enemy();
    void spatial_partitioning(const float delta);

  public:
    /// Range of Fear for a spawned or reset enemy, drawn from World::behavior_random(). Fear
    /// is in grid cells: the distance an attacking enemy keeps grows with it.
    static constexpr float spawn_fear_min = 2.0f, spawn_fear_max = 10.0f;
    /// Largest Fear after a reload: a reloaded enemy is bolder than a fresh one.
    static constexpr float reloaded_fear_max = 6.0f;

    dotio::math::Vec2 target{};
    dotio::math::Vec2 target_velocity{};
    /// The route the current state follows; empty while it needs none or found none.
    dotio::Route path;
    float fear{};
    int hits_remaining{1};
    bool line_of_sight{false}, debug{false}, frozen{};
    /// Direction this enemy wanders in under WanderMode::steering, in radians in legacy
    /// coordinates: it points along (cos, sin), and +Y is down the screen. Kept in
    /// [-pi, pi]; Initialize() and Reset() draw a new one from World::wander_random().
    float wander_heading{};
    /// Side this enemy turns to first when a wall blocks its wander heading: +1 adds to
    /// WanderHeading, -1 subtracts. It changes only when the other side is the free one, so
    /// the enemy does not turn back and forth in front of a wall.
    float wander_side{1.f};

    Enemy();

    explicit Enemy(World *world, dotio::math::Position position, dotio::math::Position target,
                   float direction, float fear);

    ~Enemy() override = default;

    void update(float delta) override;

    void tick() override;

    void draw(dotio::Renderer &renderer) override;

    void initialize();

    World &get_world();

    State &get_state();
    void set_state(State &state);

    State &get_wander_state();
    State &get_attack_state();
    State &get_pursue_state();
    State &get_recover_state();
    State &get_kamikaze_state();

    bool is_attacking();
    bool is_wandering();
    bool is_pursueing();
    bool is_recovering();
    bool is_kamikazing();
    bool is_in_line_of_sight();
    bool is_out_of_ammo() const;
    bool has_reloaded_ammo() const;
    bool pursue_countdown_passed() const;

    int get_ammo() const;
    float get_cooldown() const;
    float ammo_used_ratio() const;
    float distance() const;

    void ammo_fired();
    void ammo_recovered();
    void pursue_countdown();
    void reset_cooldown();
    void reset_pursue();
    void reset();
};
} // namespace dotio

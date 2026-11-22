#pragma once

#include "dotio/ai/state.h"
#include "dotio/entities/physics_object.h"
#include "dotio/navigation/route.h"

#include <cstdint>

namespace dotio {
class World;

/// An enemy: a body in a pool slot of the World, driven by the AI state it points at. It
/// embeds one object of every state; the states point back at it, so an enemy must not be
/// copied once initialize() bound them (the pool copies enemies only before that).
///
/// Every update the World calls update() (movement of the current state, separation from
/// neighbors); every third update the Application calls tick() (sight, contact, decisions).
class Enemy final : public PhysicsObject {
    /// Seconds until the next shot or reload step; counts down by the 20 Hz decision period.
    float cooldown_{};
    /// Seconds between shots before DifficultySettings::fire_interval_multiplier scales it.
    float max_cooldown_{1.0f};
    /// Arrows fired since the last reload, counted from 1; ammo_capacity means empty.
    int ammo_{1};
    /// Cooldowns spent pursuing without sight; compared with the pursuit cycles.
    int countdown_{0};
    static constexpr int ammo_capacity = 10;
    /// The world this enemy belongs to; borrowed, it owns the enemy.
    World *world_{};
    /// The current state: one of the embedded states below.
    State *state_{};
    AttackState attack_;
    WanderState wander_;
    PursueState pursue_;
    RecoverState recover_;
    KamikazeState kamikaze_;
    std::uint64_t generation_{};
    dotio::math::Vec2 steering_{};

    void player_bumped_into_enemy();
    void spatial_partitioning(const float delta);

  public:
    /// Range of `fear` for a spawned or reset enemy, drawn from World::behavior_random(). Fear
    /// is in grid cells: the distance an attacking enemy keeps grows with it.
    static constexpr float spawn_fear_min = 2.0f, spawn_fear_max = 10.0f;
    /// Largest `fear` after a reload: a reloaded enemy is bolder than a fresh one.
    static constexpr float reloaded_fear_max = 6.0f;

    /// The player's position and velocity, set by the World before every update; legacy
    /// pixels and legacy pixels per update.
    dotio::math::Vec2 target{};
    dotio::math::Vec2 target_velocity{};
    /// The route the current state follows; empty while it needs none or found none.
    dotio::Route path;
    /// How far the enemy keeps from the player when it attacks, in grid cells, scaled by the
    /// ammunition used; also the number of hidden cells it passes before taking a shelter.
    float fear{};
    /// Bolt hits it still takes; DifficultySettings::enemy_hits on spawn and reset.
    int hits_remaining{1};
    /// Whether the player was in sight at the last decision tick (no wall between them).
    bool line_of_sight{false};
    /// Invulnerable debug mode: a kamikaze contact does no damage.
    bool debug{false};
    /// Outside the activity window: the enemy stands still and keeps no route.
    bool frozen{};
    /// Direction this enemy wanders in under WanderMode::steering, in radians in legacy
    /// coordinates: it points along (cos, sin), and +Y is down the screen. Kept in
    /// [-pi, pi]; initialize() and reset() draw a new one from World::wander_random().
    float wander_heading{};
    /// Side this enemy turns to first when a wall blocks its wander heading: +1 adds to
    /// wander_heading, -1 subtracts. It changes only when the other side is the free one, so
    /// the enemy does not turn back and forth in front of a wall.
    float wander_side{1.f};

    Enemy();

    explicit Enemy(World *world, dotio::math::Position position, dotio::math::Position target,
                   float direction, float fear);

    ~Enemy() override = default;
    /// Lifetime counter for this pool slot. reset() begins a new lifetime.
    std::uint64_t generation() const { return generation_; }
    /// Velocity change applied by the AI state during the latest update, in legacy pixels/update.
    dotio::math::Vec2 steering() const { return steering_; }

    /// Movement of one update: the current state's execute(), then separation from the
    /// neighbors in its partition cells. Does nothing while frozen.
    void update(float delta) override;

    /// The decision tick: counts the cooldown down, handles contact with the player (a
    /// kamikaze explodes, any other enemy is caught and rewards mana), updates
    /// line_of_sight and runs the current state's update(). Does nothing while frozen.
    void tick() override;

    /// Draws the body in its state's color in Debug Mode, red otherwise.
    void draw(dotio::Renderer &renderer) override;

    /// Binds the embedded states to this enemy, starts in wander and draws a wander heading.
    /// Call once the enemy sits in its pool slot.
    void initialize();

    World &get_world();

    State &get_state();
    /// Makes `state`, one of this enemy's embedded states, the current one.
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
    /// Whether no wall lies on the segment to the player.
    bool is_in_line_of_sight();
    /// Ten arrows fired since the last reload.
    bool is_out_of_ammo() const;
    /// At most one arrow left to reload.
    bool has_reloaded_ammo() const;
    /// Whether the pursuit has used all its cycles (DifficultySettings::pursuit_cycles).
    bool pursue_countdown_passed() const;

    int get_ammo() const;
    float get_cooldown() const;
    /// Arrows fired since the last reload as a fraction of the capacity.
    float ammo_used_ratio() const;
    /// The larger axis distance to the player, in legacy pixels.
    float distance() const;

    void ammo_fired();
    /// Reloads two arrows.
    void ammo_recovered();
    void pursue_countdown();
    /// Starts a cooldown of max_cooldown_ times the difficulty's fire interval multiplier.
    void reset_cooldown();
    void reset_pursue();
    /// Prepares a spawn: full hits, a new fear and wander heading, wander state, full
    /// ammunition, no route; activates the enemy.
    void reset();
};
} // namespace dotio

#pragma once

namespace dotio {
/// Distance class of an enemy from the player: close up to about 4 cells, far from 10.
enum class Distance { close, average, far };
/// Fear class of an enemy: warrior below about 4.5, anxious above about 6.5.
enum class Fear { anxious, neutral, warrior };
/// Ammunition class from the arrows an enemy has fired: full near 1, empty near 10.
enum class Ammo { full, midway, empty };

/// The original game's fuzzy decisions: the kamikaze rule of the attack state, and the spawn
/// rate of the population. Membership functions are triangles and trapezoids; a value takes
/// the class with the highest membership.
class Fuzzy {
  public:
    /// Whether an attacking enemy turns kamikaze: true only for a close, warrior enemy with
    /// midway ammunition. `distance` is the larger axis distance to the player in legacy
    /// pixels (Enemy::distance), `fear` the enemy's fear, `ammo` the arrows it fired.
    static bool evaluate(float distance, float fear, float ammo);
    /// Sugeno spawn rate (0..1 enemies/second) from finite nonnegative nearby load.
    /// Sparse, occupied and crowded memberships blend rates 1, 0.25 and 0.
    static float spawn_rate(float nearby_load);

  private:
    /// Membership of `x` in the triangle rising from `a` to 1 at `b` and falling to `c`.
    static float triangle(float x, float a, float b, float c);
    /// Membership of `x` in the trapezoid rising from `a`, 1 from `b` to `c`, falling to `d`.
    static float trapezoid(float x, float a, float b, float c, float d);
    /// Each classifier stores the class with the highest membership in `result` and returns
    /// that membership.
    static float classify_distance(float distance, Distance &result);
    static float classify_fear(float fear, Fear &result);
    static float classify_ammo(int ammo, Ammo &result);
};
} // namespace dotio

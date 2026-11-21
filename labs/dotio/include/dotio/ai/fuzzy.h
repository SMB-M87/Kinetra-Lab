#pragma once

namespace dotio {
enum class Distance { close, average, far };
enum class Fear { anxious, neutral, warrior };
enum class Ammo { full, midway, empty };

/// Original distance/fear/ammunition membership classifications and kamikaze rule.
class Fuzzy {
  public:
    static bool evaluate(float distance, float fear, float ammo);
    /// Sugeno spawn rate (0..1 enemies/second) from finite nonnegative nearby load.
    /// Sparse, occupied and crowded memberships blend rates 1, 0.25 and 0.
    static float spawn_rate(float nearby_load);

  private:
    static float triangle(float x, float a, float b, float c);
    static float trapezoid(float x, float a, float b, float c, float d);
    static float classify_distance(float distance, Distance &result);
    static float classify_fear(float fear, Fear &result);
    static float classify_ammo(int ammo, Ammo &result);
};
} // namespace dotio

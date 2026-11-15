#ifndef FUZZY_H
#define FUZZY_H

namespace logic {
enum class Variable { Distance, Fear, Ammo };

enum class Distance { Close, Average, Far };
enum class Fear { Anxious, Neutral, Warrior };
enum class Ammo { Full, Midway, Empty };

/// Original distance/fear/ammunition membership classifications and kamikaze rule.
class Fuzzy {
  public:
    static bool Evaluate(float distance, float fear, float ammo);
    /// Sugeno spawn rate (0..1 enemies/second) from finite nonnegative nearby load.
    /// Sparse, occupied and crowded memberships blend rates 1, 0.25 and 0.
    static float SpawnRate(float nearby_load);

  private:
    static float Triangle(float x, float a, float b, float c);
    static float Trapezoid(float x, float a, float b, float c, float d);
    static float Distance(float distance, Distance &result);
    static float Fear(float fear, Fear &result);
    static float Ammo(int ammo, Ammo &result);
};
} // namespace logic
#endif
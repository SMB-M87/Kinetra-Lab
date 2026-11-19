#pragma once
// How the tour's pointer gets from one point to another: the way a hand moves a mouse,
// not the way a script sets a position.
#include <kinetra/math.h>

#include <random>

namespace capture {
/// Progress along a move after the fraction `u` of its duration, both in [0, 1]: the
/// minimum-jerk profile, which starts and ends at rest and is fastest halfway. Hands move
/// like this; a constant speed is what makes a scripted pointer look scripted. Values
/// outside [0, 1] are clamped.
double minimum_jerk(double u);

/// One move of the pointer along a slightly bent curve.
struct PointerPath {
    /// Start and end of the move, and the control point of the quadratic curve between
    /// them, in logical window pixels.
    kinetra::Vec2 from, to, control;
    /// Seconds the move takes; zero when `from` equals `to`.
    double duration{};
    /// The pointer's position `time` seconds into the move: `from` before it starts, `to`
    /// once it is over.
    kinetra::Vec2 at(double time) const;
};

/// Largest sideways bend of a path, as a fraction of its length.
inline constexpr float largest_bend = 0.12f;
/// Shortest and longest move, in seconds.
inline constexpr double shortest_move = 0.18, longest_move = 1.3;

/// Plans the move from `from` to `to`. The duration grows with the logarithm of the distance
/// (Fitts' law): about half a second for 100 pixels, 0.9 s across a 1080p window. The curve
/// bends to one side by a random amount of at most largest_bend, drawn from `random`.
PointerPath plan_pointer_path(kinetra::Vec2 from, kinetra::Vec2 to, std::mt19937 &random);
} // namespace capture

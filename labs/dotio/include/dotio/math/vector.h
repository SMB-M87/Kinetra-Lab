#pragma once

#include <cstdint>

namespace dotio::math {
/// A point or vector in legacy coordinates (pixels at 25 per world unit, +Y down the screen),
/// with the operators of the original game. The aliases (Position, Point2, ...) are the same
/// type. Its arithmetic delegates to kinetra::Vec2 where Kinetra has the operation.
typedef struct Vec2 {
    /// Horizontal coordinate; in legacy coordinates it grows to the right.
    float x;
    /// Vertical coordinate; in legacy coordinates it grows down the screen.
    float y;

    Vec2(float x_value, float y_value);
    /// (s, s).

    explicit Vec2(float s);

    /// (0, 0).
    Vec2();

    void set(float x_value, float y_value);
    void set(const Vec2 &&vector);
    void set(const Vec2 &vector);

    [[nodiscard]] float dot_product(const Vec2 &other) const;

    /// The 2D cross product x * other.y - y * other.x.
    [[nodiscard]] float determinant(const Vec2 &other) const;

    /// Angle from this direction to `other`, in radians in (-pi, pi].
    [[nodiscard]] float signed_angle_difference(const Vec2 &other) const;

    /// Angle from `heading` to the direction from this point to `other`, in radians.
    [[nodiscard]] float signed_angle_difference(const Vec2 &heading, const Vec2 &other) const;

    /// Lead angle for a projectile of speed `velocity_b` to meet a target moving at
    /// `velocity_a`, `angle_difference` off the line of fire: asin(sin(angle) * a / b), with
    /// the sine clamped to [-1, 1] when the projectile is too slow.
    static float intercept(float velocity_a, float velocity_b, float angle_difference);

    /// The absolute difference per axis.
    [[nodiscard]] Vec2 get_difference(const Vec2 &other) const;

    [[nodiscard]] float get_distance(const Vec2 &other) const;

    [[nodiscard]] Vec2 get_abs() const;

    /// Subtracts or adds `n` on both axes.
    Vec2 operator-(float n) const;

    Vec2 operator+(float n) const;

    Vec2 operator+(const Vec2 &other) const;

    Vec2 operator-(const Vec2 &other) const;

    bool operator==(const Vec2 &other) const;

    Vec2 &operator+=(float n);

    Vec2 &operator+=(const Vec2 &vec2);

    Vec2 &operator-=(float n);

    Vec2 &operator-=(const Vec2 &vec2);

    Vec2 &operator*=(float n);

    /// Multiplies or divides per axis.
    Vec2 operator*(const Vec2 &other) const;

    Vec2 operator/(const Vec2 &other) const;

    /// Compares the length with `value`: `(a - b) < 2` reads "a is closer to b than 2".
    bool operator<(float value) const;
    bool operator>(float value) const;
    bool operator<=(float value) const;
    bool operator>=(float value) const;

    friend Vec2 operator*(float n, const Vec2 &vec2);

    friend Vec2 operator*(const Vec2 &vec2, float n);

    /// (n / x, n / y).
    friend Vec2 operator/(float n, const Vec2 &vec2);

    friend Vec2 operator/(const Vec2 &vec2, float n);

    Vec2 operator-() const;

    Vec2 &round();
    [[nodiscard]] Vec2 get_rounded() const;

    Vec2 &ceil();
    [[nodiscard]] Vec2 get_ceiled() const;

    Vec2 &floor();
    [[nodiscard]] Vec2 get_floored() const;

    [[nodiscard]] float get_length() const;

    /// Direction in radians, atan2(y, x).
    [[nodiscard]] float get_direction() const;

    Vec2 &translate(const Vec2 &vector);

    /// The absolute value per axis, despite the name (as in the original game).
    [[nodiscard]] Vec2 get_signed() const;

    /// The remainder per axis (std::fmod: same sign as the value).
    [[nodiscard]] Vec2 get_modulo(float divisor) const;
    Vec2 &modulo(float divisor);
    /// The unit vector in this direction; zero for a zero vector.
    [[nodiscard]] Vec2 get_normalized() const;
    Vec2 &normalize();

    /// Rotates by `radians` (counterclockwise in a Y-up frame, clockwise on screen).
    Vec2 &rotate(float radians);
} Vector2, Size2, Point2, Coord, Coordinates, Pos, Position;

/// A color: red, green, blue and opacity, each in [0, 1].
struct RGBA {
    float r, g, b, a;
};

/// A line segment from Start to End.
struct Segment {
    Point2 start;
    Point2 end;

    Segment();
    Segment(Point2 from, Point2 to);

    float get_slope() const;
};
} // namespace dotio::math

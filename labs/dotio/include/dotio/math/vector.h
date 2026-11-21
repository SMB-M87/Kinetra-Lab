#pragma once

#include <cstdint>

namespace dotio::math {
/// Legacy field/coordinate compatibility facade; shared vector calculations use kinetra::Vec2.
typedef struct Vec2 {
    /// Horizontal coordinate; in legacy coordinates it grows to the right.
    float x;
    /// Vertical coordinate; in legacy coordinates it grows down the screen.
    float y;

    Vec2(float x_value, float y_value);

    explicit Vec2(float s);

    Vec2();

    void set(float x_value, float y_value);
    void set(const Vec2 &&vector);
    void set(const Vec2 &vector);

    [[nodiscard]] float dot_product(const Vec2 &other) const;

    [[nodiscard]] float determinant(const Vec2 &other) const;

    [[nodiscard]] float signed_angle_difference(const Vec2 &other) const;

    [[nodiscard]] float signed_angle_difference(const Vec2 &heading, const Vec2 &other) const;

    static float intercept(float velocity_a, float velocity_b, float angle_difference);

    [[nodiscard]] Vec2 get_difference(const Vec2 &other) const;

    [[nodiscard]] float get_distance(const Vec2 &other) const;

    [[nodiscard]] Vec2 get_abs() const;

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

    Vec2 operator*(const Vec2 &other) const;

    Vec2 operator/(const Vec2 &other) const;

    bool operator<(float value) const;
    bool operator>(float value) const;
    bool operator<=(float value) const;
    bool operator>=(float value) const;

    friend Vec2 operator*(float n, const Vec2 &vec2);

    friend Vec2 operator*(const Vec2 &vec2, float n);

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

    [[nodiscard]] float get_direction() const;

    Vec2 &translate(const Vec2 &vector);

    [[nodiscard]] Vec2 get_signed() const;

    [[nodiscard]] Vec2 get_modulo(float divisor) const;
    Vec2 &modulo(float divisor);
    [[nodiscard]] Vec2 get_normalized() const;
    Vec2 &normalize();

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

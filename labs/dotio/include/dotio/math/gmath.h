#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>

namespace dotio::math {
/// Legacy field/coordinate compatibility facade; shared vector calculations use kinetra::Vec2.
typedef struct Vec2 {
    /// Horizontal coordinate; in legacy coordinates it grows to the right.
    float X;
    /// Vertical coordinate; in legacy coordinates it grows down the screen.
    float Y;

    Vec2(float x, float y);

    explicit Vec2(float s);

    Vec2();

    void set(float x, float y);
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
    float R, G, B, A;
};

/// A line segment from Start to End.
struct Segment {
    Point2 Start;
    Point2 End;

    Segment();
    Segment(Point2 from, Point2 to);

    float get_slope() const;
};

typedef struct GridEntry {
  public:
    int Row;
    int Column;

    GridEntry();
    explicit GridEntry(const int size);
    GridEntry(const int row, const int column);
    static Coordinates to_world(const int row, const int column, const float cell_size,
                                const Coordinates &origin);
    static bool in_bounds(const GridEntry &entry, const GridEntry &size);
    static bool from_world(const Position &position, const Coordinates &origin, const float cell_size,
                           const GridEntry &grid_size, GridEntry &output);
    static Position get_cell_position(const Position &position, const Coordinates &origin,
                                      const float cell_size);
    static unsigned char get_square_partition(const Position &position, float semi_width,
                                              const Coordinates &origin, float cell_size);
    static void get_partitions(uint8_t partition, const Position &position, const Coordinates &origin,
                               float cell_size, const GridEntry &grid_size, GridEntry output[4]);
    bool operator==(const GridEntry &other) const;
    static GridEntry null_grid_entry;
} GridSize, GridPosition, GridCoordinates;
} // namespace dotio::math

namespace std {
template <> struct hash<dotio::math::GridEntry> {
    std::size_t operator()(const dotio::math::GridEntry &entry) const noexcept {
        std::hash<float> hasher;
        std::size_t hash_value = 0;
        hash_value ^= hasher(float(entry.Row)) + 0x9e3779b9 + (hash_value << 6) + (hash_value >> 2);
        hash_value ^= hasher(float(entry.Column)) + 0x9e3779b9 + (hash_value << 6) + (hash_value >> 2);
        return hash_value;
    }
};
} // namespace std

#include "portable.h"
#ifndef GMATH_H
#define GMATH_H

namespace gmath {
/// Legacy field/coordinate compatibility facade; shared vector calculations use kinetra::Vec2.
typedef struct Vec2 {
    union {
        float X;
        float Width;
        float U;

        float Left;
        float Top;
        float Start;
    };

    union {
        float Y;
        float Height;
        float V;

        float Right;
        float Bottom;
        float End;
    };

    Vec2(float x, float y);

    explicit Vec2(float s);

    Vec2();

    void Set(float x, float y);
    void Set(const Vec2 &&vector);
    void Set(const Vec2 &vector);

    [[nodiscard]] float DotProduct(const Vec2 &other) const;

    [[nodiscard]] float Determinant(const Vec2 &other) const;

    [[nodiscard]] float SignedAngleDifference(const Vec2 &other) const;

    [[nodiscard]] float SignedAngleDifference(const Vec2 &heading, const Vec2 &other) const;

    static float Intercept(float velocityA, float velocityB, float angleDifference);

    [[nodiscard]] Vec2 GetDifference(const Vec2 &other) const;

    [[nodiscard]] float GetDistance(const Vec2 &other) const;

    [[nodiscard]] Vec2 GetAbs() const;

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

    Vec2 &Round();
    [[nodiscard]] Vec2 GetRounded() const;

    Vec2 &Ceil();
    [[nodiscard]] Vec2 GetCeiled() const;

    Vec2 &Floor();
    [[nodiscard]] Vec2 GetFloored() const;

    [[nodiscard]] float GetLength() const;

    [[nodiscard]] float GetDirection() const;

    Vec2 &Translate(const Vec2 &vector);

    [[nodiscard]] Vec2 GetSigned() const;

    [[nodiscard]] Vec2 GetModulo(float divisor) const;
    Vec2 &Modulo(float divisor);
    [[nodiscard]] Vec2 GetNormalized() const;
    Vec2 &Normalize();

    Vec2 &Rotate(float radians);
} Vector2, Size2, Point2, Coord, Coordinates, Pos, Position;

typedef struct Vec4 {
    union {
        float X;
        float R;
        float Red;
    };

    union {
        float Y;
        float G;
        float Green;
    };

    union {
        float Z;
        float B;
        float Blue;
    };

    union {
        float W;
        float A;
        float Alpha;
    };
} Vector4, RGBA;

struct Segment {
    union {
        Point2 From;
        Point2 Start;
    };

    union {
        Point2 To;
        Point2 End;
    };

    Segment();
    Segment(Point2 from, Point2 to);

    float GetSlope() const;
};

typedef struct GridEntry {
  public:
    union {
        int Row;
        int Rows;
    };

    union {
        int Column;
        int Columns;
    };

    GridEntry();
    explicit GridEntry(const int size);
    GridEntry(const int row, const int column);
    static Coordinates ToWorld(const int row, const int column, const float cellSize,
                               const Coordinates &origin);
    static bool InBounds(const GridEntry &entry, const GridEntry &size);
    static bool FromWorld(const Position &position, const Coordinates &origin, const float cellSize,
                          const GridEntry &gridSize, GridEntry &output);
    static Position GetCellPosition(const Position &position, const Coordinates &origin,
                                    const float cellSize);
    static bool ActivatePath(const gmath::GridEntry &start, const gmath::GridEntry &candidate);
    static bool PathRefresh(const gmath::GridEntry &start, const gmath::GridEntry &candidate);
    static unsigned char GetSquarePartition(const Position &position, float semiWidth,
                                            const Coordinates &origin, float cellSize);
    static void GetPartitions(uint8_t partition, const Position &position, const Coordinates &origin,
                              float cellSize, const GridEntry &gridSize, GridEntry output[4]);
    bool operator==(const GridEntry &other) const;
    bool operator!=(const GridEntry &other) const;
    static GridEntry NullGridEntry;
} GridSize, GridPosition, GridCoordinates;
} // namespace gmath

namespace std {
template <> struct hash<gmath::GridEntry> {
    std::size_t operator()(const gmath::GridEntry &entry) const noexcept {
        std::hash<float> hasher;
        std::size_t hashValue = 0;
        hashValue ^= hasher(entry.Row) + 0x9e3779b9 + (hashValue << 6) + (hashValue >> 2);
        hashValue ^= hasher(entry.Column) + 0x9e3779b9 + (hashValue << 6) + (hashValue >> 2);
        return hashValue;
    }
};
} // namespace std

#endif // GMATH_H

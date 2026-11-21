#include "dotio/math/gmath.h"
#include <kinetra/simulation/math.h>

#include <bitset>
#include <cmath>
#include <cstdint>
#include <iostream>

namespace dotio::math {
namespace {
kinetra::Vec2 canonical(const Vec2 &value) {
    return {value.X, value.Y};
}
Vec2 legacy(kinetra::Vec2 value) {
    return {value.x, value.y};
}
} // namespace

Vec2::Vec2(const float x, const float y) : X(x), Y(y) {
}

Vec2::Vec2(const float s) : Vec2(s, s) {
}

Vec2::Vec2() : Vec2(0.0f) {
}

void Vec2::set(const float x, const float y) {
    X = x;
    Y = y;
}
void Vec2::set(const Vec2 &&vector) {
    set(vector.X, vector.Y);
}

void Vec2::set(const Vec2 &vector) {
    set(vector.X, vector.Y);
}

float Vec2::dot_product(const Vec2 &other) const {
    return kinetra::dot({X, Y}, {other.X, other.Y});
}

float Vec2::determinant(const Vec2 &other) const {
    return kinetra::cross({X, Y}, {other.X, other.Y});
}

float Vec2::signed_angle_difference(const Vec2 &other) const {
    return kinetra::signed_angle({X, Y}, {other.X, other.Y});
}

float Vec2::signed_angle_difference(const Vec2 &heading, const Vec2 &other) const {
    return heading.signed_angle_difference(other - *this);
}

float Vec2::intercept(const float velocity_a, const float velocity_b, const float angle_difference) {
    float a = sinf(angle_difference) * velocity_a / velocity_b;

    if (a > 1)
        a = 1;

    else if (a < -1)
        a = -1;

    return asinf(a);
}

Vec2 Vec2::get_difference(const Vec2 &other) const {
    return (*this - other).get_abs();
}

Vec2 Vec2::get_abs() const {
    return {std::abs(X), std::abs(Y)};
}

Vec2 Vec2::operator-(const float n) const {
    return {X - n, Y - n};
}

Vec2 Vec2::operator+(const float n) const {
    return {X + n, Y + n};
}

Vec2 Vec2::operator+(const Vec2 &other) const {
    return legacy(canonical(*this) + canonical(other));
}

Vec2 Vec2::operator-(const Vec2 &other) const {
    return legacy(canonical(*this) - canonical(other));
}

bool Vec2::operator==(const Vec2 &other) const {
    return X == other.X && Y == other.Y;
}

Vec2 &Vec2::operator+=(float n) {
    X += n;
    Y += n;
    return *this;
}

Vec2 &Vec2::operator+=(const Vec2 &vec2) {
    X += vec2.X;
    Y += vec2.Y;
    return *this;
}

Vec2 &Vec2::operator-=(float n) {
    X -= n;
    Y -= n;
    return *this;
}

Vec2 &Vec2::operator-=(const Vec2 &vec2) {
    X -= vec2.X;
    Y -= vec2.Y;
    return *this;
}

Vec2 &Vec2::operator*=(float n) {
    X *= n;
    Y *= n;
    return *this;
}

bool Vec2::operator<(float value) const {
    return (get_length() < value);
}

bool Vec2::operator>(float value) const {
    return (get_length() > value);
}

bool Vec2::operator<=(float value) const {
    return (get_length() <= value);
}

bool Vec2::operator>=(float value) const {
    return (get_length() >= value);
}

Vec2 Vec2::operator*(const Vec2 &other) const {
    return {X * other.X, Y * other.Y};
}

Vec2 Vec2::operator/(const Vec2 &other) const {
    return {X / other.X, Y / other.Y};
}

Vec2 operator*(const float n, const Vec2 &vec2) {
    return legacy(canonical(vec2) * n);
}

Vec2 operator*(const Vec2 &vec2, const float n) {
    return n * vec2;
}

Vec2 operator/(const float n, const Vec2 &vec2) {
    return {n / vec2.X, n / vec2.Y};
}

Vec2 operator/(const Vec2 &vec2, const float n) {
    return legacy(canonical(vec2) / n);
}

Vec2 Vec2::operator-() const {
    return legacy(-canonical(*this));
}

Vec2 &Vec2::round() {
    X = roundf(X);
    Y = roundf(Y);
    return *this;
}

Vec2 Vec2::get_rounded() const {
    return {roundf(X), roundf(Y)};
}

Vec2 &Vec2::ceil() {
    X = ceilf(X);
    Y = ceilf(Y);
    return *this;
}
Vec2 Vec2::get_ceiled() const {
    return {ceilf(X), ceilf(Y)};
}

Vec2 &Vec2::floor() {
    X = floorf(X);
    Y = floorf(Y);
    return *this;
}
Vec2 Vec2::get_floored() const {
    return {floorf(X), floorf(Y)};
}

float Vec2::get_length() const {
    return kinetra::length({X, Y});
}

float Vec2::get_direction() const {
    return atan2f(Y, X);
}

Vec2 &Vec2::translate(const Vec2 &vector) {
    set(*this + vector);
    return *this;
}

Vec2 Vec2::get_signed() const {
    return {fabsf(X), fabsf(Y)};
}

Vec2 Vec2::get_modulo(const float divisor) const {
    return {fmodf(X, divisor), fmodf(Y, divisor)};
}

Vec2 &Vec2::modulo(const float divisor) {
    X = fmodf(X, divisor);
    Y = fmodf(Y, divisor);
    return *this;
}

Vec2 Vec2::get_normalized() const {
    const auto value = kinetra::normalized({X, Y});
    return {value.x, value.y};
}

Vec2 &Vec2::normalize() {
    if (get_length() > 0)
        set(get_normalized());
    return *this;
}

Vec2 &Vec2::rotate(const float radians) {
    const auto value = kinetra::rotated({X, Y}, radians);
    X = value.x;
    Y = value.y;
    return *this;
}

float Vec2::get_distance(const Vec2 &other) const {
    return kinetra::distance(canonical(*this), canonical(other));
}

Segment::Segment() : Start({}), End({}) {
}
Segment::Segment(Point2 from, Point2 to) : Start(from), End(to) {
}
float Segment::get_slope() const {
    return (End.Y - Start.Y) / (End.X - Start.X);
}

GridEntry::GridEntry() : GridEntry(0) {
}
GridEntry::GridEntry(const int row, const int column) : Row(row), Column(column) {
}
GridEntry::GridEntry(const int size) : GridEntry(size, size) {
}
Coordinates GridEntry::to_world(const int row, const int column, const float cell_size,
                                const Coordinates &origin) {
    return {(static_cast<float>(column) * cell_size) + origin.X,
            (static_cast<float>(row) * cell_size) + origin.Y};
}

bool GridEntry::in_bounds(const GridEntry &entry, const GridEntry &size) {
    return !(entry.Row >= size.Row || entry.Column >= size.Column || entry.Row < 0 || entry.Column < 0);
}

bool GridEntry::from_world(const Position &position, const Coordinates &origin, const float cell_size,
                           const GridEntry &grid_size, GridEntry &output) {
    // clang-format off
        const GridEntry temp = {
            static_cast<int>(floorf((position.Y - origin.Y) / cell_size)),
            static_cast<int>(floorf((position.X - origin.X) / cell_size))
        };
    // clang-format on

    output = temp;

    return in_bounds(temp, grid_size);
}

Position GridEntry::get_cell_position(const Position &position, const Coordinates &origin,
                                      const float cell_size) {
    return Vec2{position - origin}.modulo(cell_size);
}

uint8_t GridEntry::get_square_partition(const Position &position, const float semi_width,
                                        const Coordinates &origin, float cell_size) {
    const Position cell_position = get_cell_position(position, origin, cell_size);
    uint8_t result{};

    if (cell_position.X > cell_size - semi_width) {
        result |= 0b00000001;
    }

    else if (cell_position.X < semi_width) {
        result |= 0b00000011;
    }

    if (cell_position.Y > cell_size - semi_width) {
        result |= 0b0000100;

        if (cell_position.X >= cell_size - semi_width) {
            result |= 0b00010000;
        }

        else if (cell_position.X < semi_width) {
            result |= 0b00110000;
        }
    }

    else if (cell_position.Y < semi_width) {
        result |= 0b00001100;

        if (cell_position.X > cell_size - semi_width) {
            result |= 0b01110000;
        }

        else if (cell_position.X < semi_width) {
            result |= 0b01010000;
        }
    }

    return result;
}
void GridEntry::get_partitions(uint8_t partition, const Position &position, const Coordinates &origin,
                               const float cell_size, const GridEntry &grid_size, GridEntry output[4]) {
    GridEntry cell;

    if (!from_world(position, origin, cell_size, grid_size, cell))
        return;

    output[0] = cell;

    if (partition & 0b00000001) {
        if ((partition & 0b00000010) == 0b00000010) {
            if (const GridEntry t{cell.Row, cell.Column - 1}; in_bounds(t, grid_size))
                output[1] = t;
        }

        else {
            if (const GridEntry t{cell.Row, cell.Column + 1}; in_bounds(t, grid_size))
                output[1] = t;
        }
    }

    if (partition & 0b00000100) {
        if ((partition & 0b00001000) == 0b00001000) {
            if (const GridEntry t{cell.Row - 1, cell.Column}; in_bounds(t, grid_size))
                output[2] = t;
        }

        else {
            if (const GridEntry t{cell.Row + 1, cell.Column}; in_bounds(t, grid_size))
                output[2] = t;
        }
    }

    if (partition & 0b00010000) {
        if ((partition & 0b01100000) == 0b01100000) {
            if (const GridEntry t{cell.Row - 1, cell.Column + 1}; in_bounds(t, grid_size)) {
                output[3] = t;
            }
        }

        else if ((partition & 0b01000000) == 0b01000000) {
            if (const GridEntry t{cell.Row - 1, cell.Column - 1}; in_bounds(t, grid_size))
                output[3] = t;

        }

        else if ((partition & 0b00100000) == 0b00100000) {
            if (const GridEntry t{cell.Row + 1, cell.Column - 1}; in_bounds(t, grid_size))
                output[3] = t;

        }

        else {
            if (const GridEntry t{cell.Row + 1, cell.Column + 1}; in_bounds(t, grid_size))
                output[3] = t;
        }
    }
}

bool GridEntry::operator==(const GridEntry &other) const {
    return this->Row == other.Row && this->Column == other.Column;
}

GridEntry GridEntry::null_grid_entry = {-1, -1};
} // namespace dotio::math

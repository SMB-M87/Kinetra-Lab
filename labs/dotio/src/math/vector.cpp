#include "dotio/math/vector.h"
#include <kinetra/simulation/math.h>

#include <cmath>
#include <cstdint>

namespace dotio::math {
namespace {
kinetra::Vec2 canonical(const Vec2 &value) {
    return {value.x, value.y};
}
Vec2 legacy(kinetra::Vec2 value) {
    return {value.x, value.y};
}
} // namespace

Vec2::Vec2(const float x_value, const float y_value) : x(x_value), y(y_value) {
}

Vec2::Vec2(const float s) : Vec2(s, s) {
}

Vec2::Vec2() : Vec2(0.0f) {
}

void Vec2::set(const float x_value, const float y_value) {
    x = x_value;
    y = y_value;
}
void Vec2::set(const Vec2 &&vector) {
    set(vector.x, vector.y);
}

void Vec2::set(const Vec2 &vector) {
    set(vector.x, vector.y);
}

float Vec2::dot_product(const Vec2 &other) const {
    return kinetra::dot({x, y}, {other.x, other.y});
}

float Vec2::determinant(const Vec2 &other) const {
    return kinetra::cross({x, y}, {other.x, other.y});
}

float Vec2::signed_angle_difference(const Vec2 &other) const {
    return kinetra::signed_angle({x, y}, {other.x, other.y});
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
    return {std::abs(x), std::abs(y)};
}

Vec2 Vec2::operator-(const float n) const {
    return {x - n, y - n};
}

Vec2 Vec2::operator+(const float n) const {
    return {x + n, y + n};
}

Vec2 Vec2::operator+(const Vec2 &other) const {
    return legacy(canonical(*this) + canonical(other));
}

Vec2 Vec2::operator-(const Vec2 &other) const {
    return legacy(canonical(*this) - canonical(other));
}

bool Vec2::operator==(const Vec2 &other) const {
    return x == other.x && y == other.y;
}

Vec2 &Vec2::operator+=(float n) {
    x += n;
    y += n;
    return *this;
}

Vec2 &Vec2::operator+=(const Vec2 &vec2) {
    x += vec2.x;
    y += vec2.y;
    return *this;
}

Vec2 &Vec2::operator-=(float n) {
    x -= n;
    y -= n;
    return *this;
}

Vec2 &Vec2::operator-=(const Vec2 &vec2) {
    x -= vec2.x;
    y -= vec2.y;
    return *this;
}

Vec2 &Vec2::operator*=(float n) {
    x *= n;
    y *= n;
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
    return {x * other.x, y * other.y};
}

Vec2 Vec2::operator/(const Vec2 &other) const {
    return {x / other.x, y / other.y};
}

Vec2 operator*(const float n, const Vec2 &vec2) {
    return legacy(canonical(vec2) * n);
}

Vec2 operator*(const Vec2 &vec2, const float n) {
    return n * vec2;
}

Vec2 operator/(const float n, const Vec2 &vec2) {
    return {n / vec2.x, n / vec2.y};
}

Vec2 operator/(const Vec2 &vec2, const float n) {
    return legacy(canonical(vec2) / n);
}

Vec2 Vec2::operator-() const {
    return legacy(-canonical(*this));
}

Vec2 &Vec2::round() {
    x = roundf(x);
    y = roundf(y);
    return *this;
}

Vec2 Vec2::get_rounded() const {
    return {roundf(x), roundf(y)};
}

Vec2 &Vec2::ceil() {
    x = ceilf(x);
    y = ceilf(y);
    return *this;
}
Vec2 Vec2::get_ceiled() const {
    return {ceilf(x), ceilf(y)};
}

Vec2 &Vec2::floor() {
    x = floorf(x);
    y = floorf(y);
    return *this;
}
Vec2 Vec2::get_floored() const {
    return {floorf(x), floorf(y)};
}

float Vec2::get_length() const {
    return kinetra::length({x, y});
}

float Vec2::get_direction() const {
    return atan2f(y, x);
}

Vec2 &Vec2::translate(const Vec2 &vector) {
    set(*this + vector);
    return *this;
}

Vec2 Vec2::get_signed() const {
    return {fabsf(x), fabsf(y)};
}

Vec2 Vec2::get_modulo(const float divisor) const {
    return {fmodf(x, divisor), fmodf(y, divisor)};
}

Vec2 &Vec2::modulo(const float divisor) {
    x = fmodf(x, divisor);
    y = fmodf(y, divisor);
    return *this;
}

Vec2 Vec2::get_normalized() const {
    const auto value = kinetra::normalized({x, y});
    return {value.x, value.y};
}

Vec2 &Vec2::normalize() {
    if (get_length() > 0)
        set(get_normalized());
    return *this;
}

Vec2 &Vec2::rotate(const float radians) {
    const auto value = kinetra::rotated({x, y}, radians);
    x = value.x;
    y = value.y;
    return *this;
}

float Vec2::get_distance(const Vec2 &other) const {
    return kinetra::distance(canonical(*this), canonical(other));
}

Segment::Segment() : start({}), end({}) {
}
Segment::Segment(Point2 from, Point2 to) : start(from), end(to) {
}
float Segment::get_slope() const {
    return (end.y - start.y) / (end.x - start.x);
}
} // namespace dotio::math

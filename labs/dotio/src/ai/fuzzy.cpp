#include "dotio/ai/fuzzy.h"
#include "dotio/navigation/grid.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace dotio {
float Fuzzy::triangle(float x, float a, float b, float c) {
    if (x <= a || x >= c)
        return 0.0;
    if (x == b)
        return 1.0;
    if (x < b)
        return (x - a) / (b - a);
    return (c - x) / (c - b);
}

float Fuzzy::trapezoid(float x, float a, float b, float c, float d) {
    if (x <= a || x >= d)
        return 0.0;
    if (x >= b && x <= c)
        return 1.0;
    if (x < b)
        return (x - a) / (b - a);
    return (d - x) / (d - c);
}

float Fuzzy::classify_distance(float distance, dotio::Distance &result) {
    float close = triangle(distance, 0, dotio::Grid::cell_size * 4.0f, dotio::Grid::cell_size * 5.0f);
    float neutral = trapezoid(distance, dotio::Grid::cell_size * 3.0f, dotio::Grid::cell_size * 5.0f,
                              dotio::Grid::cell_size * 8.0f, dotio::Grid::cell_size * 10.0f);
    float farr = triangle(distance, dotio::Grid::cell_size * 10.0f, dotio::Grid::cell_size * 20.0f,
                          dotio::Grid::cell_size * 100.0f);

    if (close > neutral && close > farr) {
        result = Distance::close;
        return close;
    } else if (neutral > farr) {
        result = Distance::average;
        return neutral;
    } else {
        result = Distance::far;
        return farr;
    }
}

float Fuzzy::classify_fear(float fear, dotio::Fear &result) {
    float anxious = triangle(fear, 6.5f, 10.0f, 10.0f);
    float neutral = trapezoid(fear, 4.0f, 4.5f, 6.0f, 7.0f);
    float chad = triangle(fear, 1.5f, 4.5f, 4.5f);

    if (anxious > neutral && anxious > chad) {
        result = Fear::anxious;
        return anxious;
    } else if (neutral > chad) {
        result = Fear::neutral;
        return neutral;
    } else {
        result = Fear::warrior;
        return chad;
    }
}

float Fuzzy::classify_ammo(int ammo, dotio::Ammo &result) {
    float full = triangle(static_cast<float>(ammo), 0.0f, 1.0f, 2.0f);
    float midway = trapezoid(static_cast<float>(ammo), 1.0f, 4.0f, 6.0f, 7.0f);
    float empty = triangle(static_cast<float>(ammo), 6.0f, 10.0f, 10.0f);

    if (full > midway && full > empty) {
        result = Ammo::full;
        return full;
    } else if (midway > empty) {
        result = Ammo::midway;
        return midway;
    } else {
        result = Ammo::empty;
        return empty;
    }
}

float Fuzzy::spawn_rate(float nearby_load) {
    if (!std::isfinite(nearby_load) || nearby_load < 0)
        throw std::invalid_argument("Nearby enemy load must be finite and nonnegative");
    const float sparse = std::clamp((3 - nearby_load) / 2, 0.f, 1.f);
    const float occupied = triangle(nearby_load, 1, 3, 5);
    const float crowded = std::clamp((nearby_load - 3) / 2, 0.f, 1.f);
    return (sparse + occupied * 0.25f) / (sparse + occupied + crowded);
}

bool Fuzzy::evaluate(float distance, float fear, float ammo) {
    dotio::Distance distance_result;
    dotio::Fear fear_result;
    dotio::Ammo ammo_result;

    classify_distance(distance, distance_result);
    classify_fear(fear, fear_result);
    classify_ammo(static_cast<int>(ammo), ammo_result);

    if (distance_result == dotio::Distance::close && fear_result == Fear::warrior &&
        ammo_result == Ammo::midway) {
        return true;
    }

    return false;
}
} // namespace dotio
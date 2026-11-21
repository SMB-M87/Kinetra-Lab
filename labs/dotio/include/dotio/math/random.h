#pragma once

#include <random>

/// Random values for Dotio's simulation, drawn from a seeded generator the caller owns.
///
/// Every value comes from the raw bits of std::mt19937, whose sequence the C++ standard
/// fixes. The standard distributions are not fixed and differ between standard libraries,
/// so they would give one seed different runs on Linux and Windows.
namespace dotio::math {
/// Uniform value in [0, 1) from the top 24 bits of one draw: exact in a float.
inline float unit(std::mt19937 &random) {
    constexpr float range = 16777216.f; // 2^24
    return static_cast<float>(random() >> 8) / range;
}

/// Uniform value in [low, high) from one draw.
inline float uniform(std::mt19937 &random, float low, float high) {
    return low + (high - low) * unit(random);
}

/// Uniform index in [0, count) from one draw; `count` must be positive.
inline int index(std::mt19937 &random, int count) {
    const int drawn = static_cast<int>(unit(random) * static_cast<float>(count));
    return drawn < count ? drawn : count - 1;
}
} // namespace dotio::math

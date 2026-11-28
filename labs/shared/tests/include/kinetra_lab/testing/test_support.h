#pragma once

#include <kinetra/simulation/math.h>

#include <bit>
#include <cstdint>

/// Helpers shared by Lab tests and benchmarks through the `kinetra_lab_testing` target.
/// Applications can use this header independently of one another.
namespace kinetra_lab::testing {

/// Default number of fixed simulation updates in a sustained headless workload.
inline constexpr int long_run_ticks = 100'000;

/// 64-bit FNV-1a hash over the exact bits of the values added, in order.
///
/// Characterization tests use it to pin a seeded run: any change in arithmetic or in the
/// order of updates changes the value. `-0.0f` and `0.0f` hash differently on purpose.
class Fingerprint {
  public:
    void add(std::uint64_t value) {
        for (int byte = 0; byte < 8; ++byte) {
            hash_ ^= (value >> (byte * 8)) & 0xffu;
            hash_ *= prime;
        }
    }
    void add(float value) { add(std::uint64_t{std::bit_cast<std::uint32_t>(value)}); }
    void add(kinetra::Vec2 value) {
        add(value.x);
        add(value.y);
    }
    std::uint64_t value() const { return hash_; }

  private:
    static constexpr std::uint64_t prime = 0x100000001b3;
    std::uint64_t hash_{0xcbf29ce484222325};
};

} // namespace kinetra_lab::testing

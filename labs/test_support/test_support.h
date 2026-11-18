#pragma once

#include <kinetra/math.h>

#include <bit>
#include <cstdint>
#include <stdexcept>

/// Helpers shared by Lab test programs. Application-neutral: no application header is
/// included here, so every application's tests can link the `kinetra_lab_test_support`
/// target. The plain `require` helper is replaced when the tests move to doctest.
namespace lab::test {

/// Throws `std::runtime_error(message)` when `condition` is false. Test programs catch it
/// in `main`, report the message and return a nonzero exit code.
inline void require(bool condition, const char *message) {
    if (!condition)
        throw std::runtime_error(message);
}

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

} // namespace lab::test

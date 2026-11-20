#pragma once
// Records a seeded i4sim run as one comparable line, for the golden and pressure suites.
//
// The fingerprint depends on exact floating-point results. Builds without fast-math and
// without FMA contraction (the x86-64 default) reproduce it. A mismatch in the
// fingerprint alone, with equal totals, usually means a changed operation order.
#include "i4sim/simulation/simulation.h"
#include "kinetra_lab/testing/test_support.h"

#include <doctest/doctest.h>

#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <sstream>
#include <string>
#include <string_view>

namespace i4sim::test {
/// Advances a simulation and fingerprints every mover's motion after each tick, so a
/// changed route shows even when the movers end at the same parking places.
class Recorder {
  public:
    /// Runs one tick and adds the position and velocity of every mover.
    void step(Simulation &simulation) {
        simulation.tick();
        for (const auto &mover : simulation.movers()) {
            trajectory_.add(mover.body.position);
            trajectory_.add(mover.body.velocity);
        }
    }

    /// Runs `ticks` steps.
    void run(Simulation &simulation, std::uint64_t ticks) {
        for (std::uint64_t tick = 0; tick < ticks; ++tick)
            step(simulation);
    }

    /// The fingerprint of the trajectory recorded so far.
    kinetra_lab::testing::Fingerprint trajectory() const { return trajectory_; }

    /// Reduces the totals, the recorded trajectory and the open work to one line.
    std::string summary(const Simulation &simulation) const {
        auto state = trajectory_;
        double distance_mm = 0;
        for (const auto &mover : simulation.movers()) {
            distance_mm += mover.distance;
            state.add(std::uint64_t{mover.moving()});
            state.add(mover.product.value_or(0));
        }
        std::uint64_t interactions = 0;
        for (const auto &producer : simulation.producers())
            interactions += producer.interactions;
        for (const auto &product : simulation.products()) {
            state.add(product.id);
            state.add(std::uint64_t(product.state));
            state.add(std::uint64_t{product.step});
        }
        std::ostringstream line;
        line << "ticks=" << simulation.ticks() << " completed=" << simulation.completed()
             << " collisions=" << simulation.collisions() << " contact_ticks=" << simulation.contact_ticks()
             << " interactions=" << interactions << " distance_mm=" << std::fixed << std::setprecision(3)
             << distance_mm << " trajectory=" << std::hex << std::setw(16) << std::setfill('0')
             << state.value();
        return line.str();
    }

  private:
    kinetra_lab::testing::Fingerprint trajectory_;
};

/// True when KINETRA_PRINT_GOLDEN=1 asks for the current lines.
inline bool print_lines() {
    const char *print = std::getenv("KINETRA_PRINT_GOLDEN");
    return print && std::string_view(print) == "1";
}

/// Compares the line of a finished scenario with `expected`; a mismatch shows both
/// lines. With KINETRA_PRINT_GOLDEN=1 it also prints the line, still comparing.
inline void check_line(std::string_view name, const std::string &actual, const std::string &expected) {
    if (print_lines())
        MESSAGE(name << "\n    \"" << actual << "\"");
    CHECK(actual == expected);
}
} // namespace i4sim::test

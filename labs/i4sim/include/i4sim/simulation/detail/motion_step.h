#pragma once
#include "i4sim/simulation/detail/goals.h"
#include "i4sim/simulation/detail/production.h"

#include <kinetra/simulation/avoidance.h>
#include <kinetra/simulation/math.h>
#include <kinetra/simulation/physics.h>
#include <kinetra/simulation/spatial_grid.h>

#include <cstddef>
#include <cstdint>
#include <set>
#include <utility>
#include <vector>

namespace i4sim::detail {
/// One movement tick for every mover: route following, traffic and passing coordination,
/// steering under the selected profile, integration, arrival, and contact resolution.
///
/// Every mover steers against the same snapshot of the bodies taken before anyone moves,
/// so the result does not depend on mover order. Contacts are counted before they are
/// resolved: a pair that keeps touching counts as one contact for as long as it lasts.
class MotionStep {
  public:
    /// Moves all movers by one tick. Adds each mover's travelled distance to its product.
    void run(Goals &goals, Production &production);

    /// Contacts that started, over the whole run.
    std::uint64_t collisions() const { return collisions_; }
    /// Ticks summed over all touching pairs.
    std::uint64_t contact_ticks() const { return contact_ticks_; }
    /// Movers whose reciprocal avoidance was infeasible or overridden in the last tick.
    std::size_t avoidance_limited() const { return avoidance_limited_; }

  private:
    /// What one mover steers around: nearby movers from the snapshot, the same movers as
    /// avoidance agents (Reciprocal only), and nearby borders and stop lines.
    struct Surroundings {
        std::vector<kinetra::Body> neighbors;
        std::vector<kinetra::AvoidanceAgent> agents;
        std::vector<kinetra::Segment> edges;
    };
    /// Touching pairs of the last tick; a border is `movers + border index`.
    std::set<std::pair<std::size_t, std::size_t>> contacts_;
    std::uint64_t contact_ticks_{}, collisions_{};
    std::size_t avoidance_limited_{};

    static bool follow_route(Goals &goals, std::size_t mover);
    static Surroundings surroundings(const Goals &goals, std::size_t mover,
                                     const std::vector<kinetra::Body> &snapshot,
                                     const kinetra::SpatialGrid &index);
    void choose_acceleration(const Goals &goals, Mover &mover, const kinetra::Body &start,
                             const Surroundings &around, Vec2 target, bool arrival);
    static void integrate(Goals &goals, Production &production, std::size_t mover, const Surroundings &around,
                          bool controlled);
    void resolve_contacts(Goals &goals);
};
} // namespace i4sim::detail

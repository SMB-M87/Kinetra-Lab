#pragma once
#include "i4sim/simulation/detail/goals.h"
#include "i4sim/simulation/detail/production.h"

#include <kinetra/simulation/avoidance.h>
#include <kinetra/simulation/math.h>
#include <kinetra/simulation/physics.h>
#include <kinetra/simulation/right_of_way.h>
#include <kinetra/simulation/spatial_grid.h>
#include <kinetra/simulation/stall_detector.h>

#include <cstddef>
#include <cstdint>
#include <optional>
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
    /// Ends every right of way at once, with an event per holder; for turning RIGHT OF WAY off
    /// or choosing a profile that ignores it.
    void end_right_of_way(Fleet &fleet, std::uint64_t tick, EventLog &log);

  private:
    /// What one mover steers around: nearby movers from the snapshot, the same movers as
    /// avoidance agents (Reciprocal only), and nearby borders and stop lines.
    struct Surroundings {
        std::vector<kinetra::Body> neighbors;
        std::vector<kinetra::AvoidanceAgent> agents;
        std::vector<kinetra::Segment> edges;
    };
    /// Touching pairs of the last tick, ascending; a border is `movers + border index`.
    std::vector<std::pair<std::size_t, std::size_t>> contacts_;
    std::uint64_t contact_ticks_{}, collisions_{};
    std::size_t avoidance_limited_{};
    /// RIGHT OF WAY: which pushed-back movers the others do not count on to make room.
    kinetra::RightOfWay right_of_way_;

    /// Grants and ends rights of way from the state at the end of the last tick, and logs it.
    void update_right_of_way(Goals &goals);

    static bool follow_route(Goals &goals, std::size_t mover);
    /// Fills `around` with mover `mover`'s neighbors and nearby borders, reusing its storage
    /// and `ids`, the buffer of spatial queries: one mover's lists replace the last one's.
    static void surroundings(const Goals &goals, std::size_t mover,
                             const std::vector<kinetra::Body> &snapshot, const kinetra::SpatialGrid &index,
                             Surroundings &around, std::vector<std::size_t> &ids);
    /// Storage that surroundings() reuses from mover to mover and tick to tick, so a tick
    /// allocates no lists per mover.
    Surroundings around_;
    std::vector<std::size_t> ids_;
    /// Storage that run() and resolve_contacts() refill every tick, so a tick allocates no
    /// snapshot, bounds, mover index or contact list: the bodies and their bounds before the
    /// movers move, the index over those bounds (made for the floor's cell size), and the
    /// bodies, bounds and touching pairs after they moved.
    std::vector<kinetra::Body> snapshot_, integrated_;
    std::vector<kinetra::Aabb> boxes_, contact_boxes_;
    std::optional<kinetra::SpatialGrid> index_;
    float index_cell_{};
    std::vector<std::pair<std::size_t, std::size_t>> current_;
    /// The movers as RIGHT OF WAY sees them, refilled every tick.
    std::vector<kinetra::StallMover> stall_movers_;
    void choose_acceleration(const Goals &goals, Mover &mover, const kinetra::Body &start,
                             const Surroundings &around, Vec2 target, bool arrival);
    static void integrate(Goals &goals, Production &production, std::size_t mover, const Surroundings &around,
                          bool controlled);
    /// `kinetra::bounds(body)` for mover `mover`'s body, remembered per mover until a box's
    /// position, extents or angle change.
    struct KnownBox {
        bool valid{};
        Vec2 position, half_extents;
        float angle{};
        kinetra::Aabb box;
    };
    std::vector<KnownBox> boxes_of_;
    kinetra::Aabb box_of(std::size_t mover, const kinetra::Body &body);
    /// Borders near enough to `body`, mover `mover`'s body before contact resolution, to
    /// touch it, ascending; remembered per mover until its position or extents change.
    struct NearBorders {
        bool valid{};
        Vec2 position, half_extents;
        std::vector<std::size_t> borders;
    };
    std::vector<NearBorders> near_;
    const std::vector<std::size_t> &near_borders(const Goals &goals, std::size_t mover,
                                                 const kinetra::Body &body);
    void resolve_contacts(Goals &goals);
};
} // namespace i4sim::detail

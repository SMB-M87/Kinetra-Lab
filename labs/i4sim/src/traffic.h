#pragma once
#include "fleet.h"
#include "floor_plan.h"
#include "simulation.h"

#include <kinetra/math.h>
#include <kinetra/spatial_grid.h>
#include <kinetra/traffic.h>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace i4sim::detail {
/// Junction and corridor traffic: admission through each zone's gate, exit reservations
/// behind admitted movers, and corridor recovery when two movers block each other inside a
/// corridor. See docs/factory-simulation-architecture.md for the policy.
///
/// Holds one state per zone of the floor plan it was built for, in the same order. Its
/// output for a tick is written into the fleet: stop lines, traffic controls and each
/// mover's `Coordination`. It never moves a body.
class Traffic {
  public:
    explicit Traffic(const FloorPlan &floor);

    /// Clears the gates, exit reservations and recoveries of every zone whose feature
    /// `settings` turns off.
    void configure(const FloorPlan &floor, const MotionSettings &settings);

    /// Starts a tick's coordination: no stop lines, no controls and no coordination flags.
    static void begin_tick(Fleet &fleet);

    /// Advances corridor recoveries: ends those whose movers stopped or got through,
    /// starts one where two stalled movers face each other inside a corridor, and steers
    /// the yielding mover to its refuge while the other waits. `index` holds the movers'
    /// bounds. Call only with corridor traffic on.
    void recover_corridors(const FloorPlan &floor, Fleet &fleet, std::uint64_t tick,
                           const kinetra::SpatialGrid &index);

    /// Admits at most one mover per zone and adds stop lines for the others, and keeps
    /// approaching movers out of admitted exit pockets. `index` and `bodies` hold the movers'
    /// bounds. Does nothing when neither junction nor corridor traffic is on.
    void admit(const FloorPlan &floor, Fleet &fleet, const MotionSettings &motion,
               const kinetra::SpatialGrid &index, std::span<const kinetra::Aabb> bodies);

    /// Ends every corridor recovery that `mover` takes part in, releasing both movers, and
    /// delays the next attempt in that corridor.
    void cancel_recovery(Fleet &fleet, std::size_t mover, std::uint64_t tick);

    /// The exit space reserved behind the mover admitted to `zone`, if any.
    std::optional<kinetra::Aabb> exit_reservation(std::size_t zone) const;
    /// Number of corridors with an active recovery.
    std::size_t recovering() const;
    /// Where `mover` retreats in an active corridor recovery, if it is the yielding mover.
    std::optional<Vec2> retreat_target(std::size_t mover) const;

  private:
    /// Exit space reserved for the admitted mover `agent`.
    struct ExitReservation {
        std::uint64_t agent;
        kinetra::Aabb area;
    };
    /// One corridor standstill being resolved: `yielding` backs out to `refuge` along
    /// `route` while `passing` waits at `hold`; once `yielded`, `passing` drives through.
    struct CorridorRecovery {
        std::size_t yielding, passing;
        Vec2 refuge, hold;
        std::vector<Vec2> route;
        bool yielded{};
    };
    /// Runtime state of one zone.
    struct ZoneState {
        kinetra::TrafficGate gate;
        std::optional<ExitReservation> exit;
        std::optional<CorridorRecovery> recovery;
        /// First tick at which a new recovery may start in this corridor.
        std::uint64_t retry_at{};
    };
    /// Mover requests and occupants per zone, built at the start of admission.
    struct Requests {
        std::vector<std::vector<std::uint64_t>> requests, occupants;
        /// Per mover: whether it occupies any zone.
        std::vector<bool> occupied;
    };
    std::vector<ZoneState> zones_;

    void release_recovery(std::size_t zone, Fleet &fleet, std::uint64_t tick, const kinetra::Body &corridor);
    void start_recovery(const FloorPlan &floor, std::size_t zone, const Fleet &fleet, std::uint64_t tick,
                        const kinetra::SpatialGrid &index, const kinetra::Body &corridor);
    bool refuge_clear(std::size_t zone, std::size_t mover, const std::vector<Vec2> &route, const Fleet &fleet,
                      const kinetra::SpatialGrid &index) const;
    void steer_recovery(const FloorPlan &floor, std::size_t zone, Fleet &fleet);
    Requests find_requests(const FloorPlan &floor, const Fleet &fleet, const MotionSettings &motion,
                           float reach) const;
    void admit_zone(const FloorPlan &floor, std::size_t zone, const Requests &requests, Fleet &fleet,
                    const MotionSettings &motion, const kinetra::SpatialGrid &index,
                    std::span<const kinetra::Aabb> bodies);
    bool exit_free(std::size_t zone, std::size_t mover, kinetra::Aabb exit, const Fleet &fleet,
                   const MotionSettings &motion, const kinetra::SpatialGrid &index,
                   std::span<const kinetra::Aabb> bodies) const;
    void protect_exits(const Requests &requests, Fleet &fleet, const MotionSettings &motion,
                       const kinetra::SpatialGrid &index, std::span<const kinetra::Aabb> bodies, float reach,
                       const FloorPlan &floor) const;
    std::optional<kinetra::Aabb> exit_space(const FloorPlan &floor, const Mover &mover,
                                            const ZoneGeometry &zone, const MotionSettings &motion) const;
};
} // namespace i4sim::detail

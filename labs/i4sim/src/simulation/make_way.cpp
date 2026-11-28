#include "i4sim/simulation/detail/make_way.h"
#include "i4sim/simulation/detail/geometry.h"
#include "i4sim/simulation/detail/routing.h"
#include "i4sim/simulation/detail/simulation_constants.h"

#include <algorithm>
#include <array>
#include <exception>
#include <utility>

namespace i4sim::detail {
namespace {
using namespace kinetra;

/// Search a bounded neighborhood around the blocker for a refuge.
constexpr int refuge_cell_radius = 2;
/// Ticks a yielding mover may stand still on its own after its trip ended before it is given
/// back to the factory.
constexpr std::uint64_t stranded_ticks = 2 * stall_ticks;
constexpr MakeWaySettings settings{
    .timeout_steps = 10 * stall_ticks,
    .retry_steps = 30,
    .max_searches = 2,
    .home_tolerance = same_point_mm,
};

bool coordinated(const Mover &m) {
    const auto &c = m.coordination;
    return c.corridor_retreating || c.corridor_waiting || c.passing || c.junction_waiting || c.exit_waiting;
}

bool at_parking(const Mover &m) {
    return m.goal == GoalKind::automatic && m.lifecycle == MoverLifecycle::idle &&
           distance(m.body.position, m.parking) < same_point_mm;
}

bool crosses_region(Vec2 from, std::span<const Vec2> route, Vec2 extent, const Body &body) {
    for (const auto to : route) {
        if (swept_box_overlaps(from, to, extent, body))
            return true;
        from = to;
    }
    return false;
}

bool crosses_route(const Mover &m, std::span<const Vec2> route, const Body &body) {
    return crosses_region(m.body.position, route, m.body.half_extents + clearance, body);
}

/// A live policy change can enclose a mover. Allow it to leave by moving outward
/// along one face of the expanded box, then keep off the reserved area.
bool can_leave_region(const Mover &m, std::span<const Vec2> route, const Body &region) {
    const Vec2 extent = m.body.half_extents + clearance;
    const Vec2 from = m.body.position;
    if (!swept_box_overlaps(from, from, extent, region))
        return !crosses_region(from, route, extent, region);

    const Vec2 half = region.half_extents + extent;
    constexpr std::array faces{Vec2{-1, 0}, Vec2{1, 0}, Vec2{0, -1}, Vec2{0, 1}};
    return std::any_of(faces.begin(), faces.end(), [&](Vec2 outward) {
        float previous = dot(from - region.position, outward);
        if (previous < 0)
            return false;
        const float face = outward.x != 0 ? half.x : half.y;
        for (std::size_t i = 0; i < route.size(); ++i) {
            const float next = dot(route[i] - region.position, outward);
            if (next < previous)
                return false;
            if (next >= face)
                return !crosses_region(route[i], route.subspan(i + 1), extent, region);
            previous = next;
        }
        return false;
    });
}

std::optional<std::size_t> parked_blocker(const Goals &goals, std::size_t index) {
    const auto &fleet = goals.fleet;
    const auto &m = fleet.movers[index];
    const float reach =
        length(m.body.half_extents + fleet.max_half) + goals.motion.speed * goals.motion.horizon;
    std::optional<std::size_t> blocker;
    float nearest = reach;
    for (std::size_t other = 0; other < fleet.movers.size(); ++other) {
        const auto &candidate = fleet.movers[other];
        if (other == index || !at_parking(candidate) || candidate.held)
            continue;
        const float separation = distance(m.body.position, candidate.body.position);
        if (separation < nearest && crosses_route(m, m.route, candidate.body)) {
            nearest = separation;
            blocker = other;
        }
    }
    return blocker;
}

/// A proposed path clears the static floor and all current footprints. A hypothetical
/// move substitutes the blocker's footprint at its proposed refuge.
std::vector<Vec2> checked_route(const Goals &goals, std::size_t index, Vec2 target, MakeWayRoute purpose,
                                std::optional<MakeWayRelocation> relocated) {
    const auto &movers = goals.fleet.movers;
    std::vector<const Body *> obstacles;
    obstacles.reserve(movers.size());
    Body hypothetical;
    for (std::size_t other = 0; other < movers.size(); ++other) {
        if (other == index)
            continue;
        if (relocated && other == relocated->mover) {
            hypothetical = movers[other].body;
            hypothetical.position = relocated->position;
            obstacles.push_back(&hypothetical);
        } else {
            obstacles.push_back(&movers[other].body);
        }
    }
    auto route = clearance_route(goals.floor, movers[index], target, obstacles, goals.motion.path_planning);
    // The beneficiary still uses ordinary traffic admission. The yielding mover
    // bypasses it, so only the yielding leg must steer clear of its reservations.
    if (purpose == MakeWayRoute::yielding &&
        !i4sim::detail::MakeWay::traffic_clear(goals, movers[index], route))
        return {};
    return route;
}

bool refuge_free(const Goals &goals, std::size_t index, int cell) {
    const auto &m = goals.fleet.movers[index];
    const auto &grid = goals.floor.grid();
    if (grid.blocked(cell) || goals.floor.processing_cell(cell) || cell == grid.cell(m.parking))
        return false;
    const auto center = grid.center(cell);
    const auto extent = m.body.half_extents + clearance;
    if (!goals.floor.processing_clear({center - extent, center + extent}))
        return false;
    for (const auto &other : goals.fleet.movers) {
        if (&other == &m)
            continue;
        auto reserved = other.body;
        reserved.position = other.parking;
        if (swept_box_overlaps(center, center, extent, reserved))
            return false;
        reserved.position = other.destination;
        if (swept_box_overlaps(center, center, extent, reserved))
            return false;
    }
    return true;
}

std::vector<Vec2> refuge_candidates(const Goals &goals, std::size_t index) {
    const auto &grid = goals.floor.grid();
    const int origin = grid.cell(goals.fleet.movers[index].parking);
    if (origin < 0)
        return {};
    const int column = origin % grid.width(), row = origin / grid.width();
    std::vector<Vec2> candidates;
    for (int y = std::max(0, row - refuge_cell_radius);
         y <= std::min(grid.height() - 1, row + refuge_cell_radius); ++y)
        for (int x = std::max(0, column - refuge_cell_radius);
             x <= std::min(grid.width() - 1, column + refuge_cell_radius); ++x) {
            const int cell = y * grid.width() + x;
            if (refuge_free(goals, index, cell))
                candidates.push_back(grid.center(cell));
        }
    return candidates;
}

void install_route(Goals &goals, std::size_t index, std::vector<Vec2> route, GoalKind kind) {
    auto &m = goals.fleet.movers[index];
    goals.set(m, route.back());
    m.goal = kind;
    m.route = std::move(route);
    m.lifecycle = MoverLifecycle::moving;
    auto &progress = goals.fleet.progress[index];
    progress.best_remaining = route_remaining(m.body.position, m.route);
}
MakeWayMotion motion(const Mover &mover) {
    if (mover.held)
        return MakeWayMotion::held;
    if (mover.moving())
        return MakeWayMotion::moving;
    if (mover.lifecycle == MoverLifecycle::idle)
        return MakeWayMotion::idle;
    return MakeWayMotion::unreachable;
}

MakeWayEnd local_outcome(kinetra::MakeWayEnd outcome) {
    switch (outcome) {
    case kinetra::MakeWayEnd::cleared:
        return MakeWayEnd::cleared;
    case kinetra::MakeWayEnd::cancelled:
        return MakeWayEnd::cancelled;
    case kinetra::MakeWayEnd::timed_out:
        return MakeWayEnd::timed_out;
    case kinetra::MakeWayEnd::obstructed:
        return MakeWayEnd::obstructed;
    }
    std::terminate();
}

// The engine decides when to request and return. Lab guards routes and retains the
// mover's parking reservation, product and queue entry throughout a trip.
class FactoryMakeWayWorld : public MakeWayWorld {
  public:
    FactoryMakeWayWorld(Goals &goals, std::span<const Product> products, const Shuttle &shuttle,
                        const StallDetector &stalls, std::vector<std::optional<std::uint64_t>> &ended)
        : goals_(goals), products_(products), shuttle_(shuttle), stalls_(stalls), ended_(ended) {}

    MakeWayMover mover(std::size_t index) const override {
        const auto &m = goals_.fleet.movers[index];
        return {m.body.position, m.destination, m.parking, motion(m), m.goal == GoalKind::making_way};
    }

    std::optional<std::uint64_t> stall(std::size_t beneficiary) const override {
        if (const auto episode = stalls_.details(beneficiary))
            return episode->detected_at;
        return std::nullopt;
    }

    std::optional<MakeWayBlocker> blocker(std::size_t beneficiary) const override {
        const auto index = parked_blocker(goals_, beneficiary);
        if (!index)
            return std::nullopt;
        const auto &m = goals_.fleet.movers[*index];
        const auto product = std::find_if(products_.begin(), products_.end(), [&](const Product &candidate) {
            return candidate.id == m.product;
        });
        return MakeWayBlocker{*index, !coordinated(m) && !shuttle_.contains(*index) &&
                                          (!m.product || (product != products_.end() &&
                                                          product->state != ProductState::processing))};
    }

    std::vector<Vec2> refuges(std::size_t index) const override { return refuge_candidates(goals_, index); }

    std::vector<Vec2> route(std::size_t index, Vec2 target, MakeWayRoute purpose,
                            std::optional<MakeWayRelocation> relocated) const override {
        return checked_route(goals_, index, target, purpose, relocated);
    }

    bool needs_home(std::size_t beneficiary, std::size_t blocker) const override {
        auto parking = goals_.fleet.movers[blocker].body;
        parking.position = goals_.fleet.movers[blocker].parking;
        const auto &m = goals_.fleet.movers[beneficiary];
        return crosses_route(m, m.route, parking);
    }

    bool route_forbidden(std::size_t blocker) const override {
        const auto &m = goals_.fleet.movers[blocker];
        return !i4sim::detail::MakeWay::traffic_clear(goals_, m, m.route);
    }

    bool route_obstructed(std::size_t blocker) const override {
        const auto &m = goals_.fleet.movers[blocker];
        return std::any_of(goals_.fleet.movers.begin(), goals_.fleet.movers.end(), [&](const Mover &other) {
            return &other != &m && (!other.moving() || other.held) && crosses_route(m, m.route, other.body);
        });
    }

    void assign(std::size_t index, std::vector<Vec2> route, MakeWayRoute purpose) override {
        const auto kind =
            purpose == MakeWayRoute::yielding ? GoalKind::making_way : goals_.fleet.movers[index].goal;
        install_route(goals_, index, std::move(route), kind);
    }

    void brake(std::size_t blocker) override {
        install_route(goals_, blocker, {goals_.fleet.movers[blocker].body.position}, GoalKind::making_way);
        goals_.fleet.progress[blocker].make_way_braking = true;
    }

    void returned(std::size_t blocker) override { goals_.fleet.movers[blocker].goal = GoalKind::automatic; }

    void event(MakeWayEvent event) override {
        if (event.kind == MakeWayEvent::Kind::ended && event.blocker < ended_.size())
            ended_[event.blocker] = goals_.tick;
        if (!goals_.log.recording())
            return;
        constexpr std::array kinds{EventKind::make_way_started, EventKind::make_way_unavailable,
                                   EventKind::make_way_ended};
        goals_.log.add(
            {.tick = goals_.tick,
             .kind = kinds[static_cast<std::size_t>(event.kind)],
             .movers = {event.beneficiary, event.blocker},
             .make_way_end = event.outcome ? std::optional{local_outcome(*event.outcome)} : std::nullopt});
    }

  private:
    Goals &goals_;
    std::span<const Product> products_;
    const Shuttle &shuttle_;
    const StallDetector &stalls_;
    std::vector<std::optional<std::uint64_t>> &ended_;
};
} // namespace

bool MakeWay::traffic_clear(const Goals &goals, const Mover &mover, std::span<const Vec2> route) {
    // Leave enabled traffic zones and their exit pockets to the existing coordinator.
    for (std::size_t i = 0; i < goals.floor.zones().size(); ++i) {
        const auto &zone = goals.floor.zones()[i];
        if (!(zone.corridor ? goals.motion.corridor_traffic : goals.motion.junction_traffic))
            continue;
        if (!can_leave_region(
                mover, route,
                Body{.position = zone.center, .half_extents = zone.half_extents, .vertices = {}}))
            return false;
        if (const auto exit = goals.traffic.exit_reservation(i);
            exit && !can_leave_region(mover, route,
                                      Body{.position = (exit->max + exit->min) / 2,
                                           .half_extents = (exit->max - exit->min) / 2,
                                           .vertices = {}}))
            return false;
    }
    return true;
}

bool MakeWay::shortcut_clear(const Goals &goals, const Mover &mover, std::span<const Vec2> remaining) {
    const auto leg = remaining.first(1);
    return traffic_clear(goals, mover, remaining) &&
           std::none_of(goals.fleet.movers.begin(), goals.fleet.movers.end(), [&](const Mover &other) {
               return &other != &mover && crosses_route(mover, leg, other.body);
           });
}

void MakeWay::release_stranded(Goals &goals) {
    auto &movers = goals.fleet.movers;
    ended_.resize(movers.size());
    for (std::size_t i = 0; i < movers.size(); ++i) {
        auto &m = movers[i];
        if (m.goal != GoalKind::making_way) {
            ended_[i].reset();
            continue;
        }
        if (!ended_[i])
            continue;
        // Only standing still on its own counts: a mover on its way home or held by the
        // operator starts the count again.
        if (m.moving() || m.held) {
            ended_[i] = goals.tick;
            continue;
        }
        if (goals.tick - *ended_[i] < stranded_ticks)
            continue;
        // The lifecycle sees a mover that no longer yields and forgets its trip.
        ended_[i].reset();
        m.goal = GoalKind::automatic;
        if (!m.product)
            goals.set(m, m.parking);
    }
}

void MakeWay::update(Goals &goals, std::span<const Product> products, const Shuttle &shuttle,
                     const StallDetector &stalls) {
    release_stranded(goals);
    FactoryMakeWayWorld world(goals, products, shuttle, stalls, ended_);
    lifecycle_.step(world, goals.fleet.movers.size(), goals.tick, goals.motion.make_way, settings);
}
} // namespace i4sim::detail

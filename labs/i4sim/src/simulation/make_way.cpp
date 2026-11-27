#include "i4sim/simulation/detail/make_way.h"
#include "i4sim/simulation/detail/geometry.h"
#include "i4sim/simulation/detail/routing.h"
#include "i4sim/simulation/detail/simulation_constants.h"

#include <algorithm>
#include <array>
#include <limits>

namespace i4sim::detail {
namespace {
using namespace kinetra;

/// Allow several stall windows for the beneficiary to clear the refuge.
constexpr std::uint64_t make_way_timeout = 10 * stall_ticks;
/// Search a bounded neighborhood around the blocker for a refuge.
constexpr int refuge_cell_radius = 2;
/// One retry lets a nearby mover finish parking before declaring the route unavailable.
constexpr unsigned max_make_way_searches = 2;
constexpr std::uint64_t make_way_retry_ticks = 30;

bool coordinated(const Mover &m) {
    const auto &c = m.coordination;
    return c.corridor_retreating || c.corridor_waiting || c.passing || c.junction_waiting || c.exit_waiting;
}

bool at_parking(const Mover &m) {
    return m.goal == GoalKind::automatic && m.lifecycle == MoverLifecycle::idle &&
           distance(m.body.position, m.parking) < same_point_mm;
}

void record(Goals &goals, EventKind kind, std::size_t beneficiary, std::size_t blocker,
            std::optional<MakeWayEnd> outcome = {}) {
    if (goals.log.recording())
        goals.log.add(
            {.tick = goals.tick, .kind = kind, .movers = {beneficiary, blocker}, .make_way_end = outcome});
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
std::vector<Vec2> checked_route(const Goals &goals, std::size_t index, Vec2 target,
                                std::optional<std::pair<std::size_t, Vec2>> relocated = {}) {
    const auto &movers = goals.fleet.movers;
    std::vector<const Body *> obstacles;
    obstacles.reserve(movers.size());
    Body hypothetical;
    for (std::size_t other = 0; other < movers.size(); ++other) {
        if (other == index)
            continue;
        if (relocated && other == relocated->first) {
            hypothetical = movers[other].body;
            hypothetical.position = relocated->second;
            obstacles.push_back(&hypothetical);
        } else {
            obstacles.push_back(&movers[other].body);
        }
    }
    auto route = clearance_route(goals.floor, movers[index], target, obstacles, goals.motion.path_planning);
    if (!MakeWay::traffic_clear(goals, movers[index], route))
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

std::vector<Vec2> refuge_route(const Goals &goals, std::size_t index, std::size_t beneficiary) {
    const auto &grid = goals.floor.grid();
    const auto &m = goals.fleet.movers[index];
    const int origin = grid.cell(m.parking);
    const int column = origin % grid.width(), row = origin / grid.width();
    float nearest = std::numeric_limits<float>::infinity();
    std::vector<Vec2> best;
    for (int y = std::max(0, row - refuge_cell_radius);
         y <= std::min(grid.height() - 1, row + refuge_cell_radius); ++y)
        for (int x = std::max(0, column - refuge_cell_radius);
             x <= std::min(grid.width() - 1, column + refuge_cell_radius); ++x) {
            const int cell = y * grid.width() + x;
            if (!refuge_free(goals, index, cell))
                continue;
            const auto target = grid.center(cell);
            const auto distance_to_refuge = distance(m.body.position, target);
            if (distance_to_refuge >= nearest)
                continue;
            auto route = checked_route(goals, index, target);
            if (!route.empty() &&
                !checked_route(goals, beneficiary, goals.fleet.movers[beneficiary].destination,
                               std::pair{index, target})
                     .empty()) {
                best = std::move(route);
                nearest = distance_to_refuge;
            }
        }
    return best;
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

void MakeWay::advance_trip(Goals &goals, std::size_t index) {
    auto &entry = trips_[index];
    auto &m = goals.fleet.movers[index];
    auto &trip = *entry;
    auto &beneficiary = goals.fleet.movers[trip.beneficiary];
    const auto finish = [&](MakeWayEnd outcome) {
        if (!trip.outcome) {
            record(goals, EventKind::make_way_ended, trip.beneficiary, index, outcome);
            trip.outcome = outcome;
        }
    };
    if (m.goal != GoalKind::making_way) {
        finish(MakeWayEnd::cancelled);
        entry.reset();
        return;
    }
    if (m.held) {
        finish(MakeWayEnd::cancelled);
        return;
    }
    if (trip.phase == Phase::returning && m.lifecycle == MoverLifecycle::idle) {
        if (distance(m.body.position, m.parking) < same_point_mm) {
            m.goal = GoalKind::automatic;
            entry.reset();
        } else {
            trip.phase = Phase::waiting;
        }
        return;
    }

    const bool cancelled =
        !goals.motion.make_way || beneficiary.held || distance(beneficiary.destination, trip.goal) > 0;
    auto parking = m.body;
    parking.position = m.parking;
    const bool protects_beneficiary =
        beneficiary.moving() && crosses_route(beneficiary, beneficiary.route, parking);
    if (cancelled)
        finish(MakeWayEnd::cancelled);
    else if (beneficiary.lifecycle == MoverLifecycle::idle ||
             (trip.phase != Phase::outbound && !protects_beneficiary))
        finish(MakeWayEnd::cleared);
    else if (goals.tick >= trip.deadline)
        finish(MakeWayEnd::timed_out);
    const bool route_hits_traffic = m.moving() && !traffic_clear(goals, m, m.route);
    if (route_hits_traffic)
        finish(MakeWayEnd::obstructed);
    // A failed or cancelled request cannot reserve the parking space forever.
    // The return planner still checks the beneficiary's current footprint.
    const bool can_return = !protects_beneficiary || cancelled || route_hits_traffic ||
                            goals.tick >= trip.deadline || trip.outcome == MakeWayEnd::cancelled;
    const auto return_home = [&] {
        if (auto home = checked_route(goals, index, m.parking); !home.empty()) {
            install_route(goals, index, std::move(home), GoalKind::making_way);
            trip.phase = Phase::returning;
            return true;
        }
        return false;
    };
    if (m.moving()) {
        if (trip.phase == Phase::outbound && trip.outcome && can_return && return_home())
            return;

        // A newly stationary body may occupy the checked trip. Brake via the
        // current position, then wait until a checked return is available.
        const bool obstructed =
            std::any_of(goals.fleet.movers.begin(), goals.fleet.movers.end(), [&](const Mover &other) {
                return &other != &m && (!other.moving() || other.held) &&
                       crosses_route(m, m.route, other.body);
            });
        if (trip.phase != Phase::waiting && (obstructed || route_hits_traffic)) {
            finish(MakeWayEnd::obstructed);
            install_route(goals, index, {m.body.position}, GoalKind::making_way);
            trip.phase = Phase::waiting;
        }
        return;
    }

    if (trip.phase == Phase::outbound) {
        if (!trip.outcome)
            if (auto route = checked_route(goals, trip.beneficiary, trip.goal); !route.empty())
                install_route(goals, trip.beneficiary, std::move(route), beneficiary.goal);
        trip.phase = Phase::waiting;
        return;
    }
    if (trip.outcome && can_return)
        return_home();
}

void MakeWay::try_make_way(Goals &goals, std::span<const Product> products, const Shuttle &shuttle,
                           const StallDetector &stalls, std::size_t beneficiary) {
    const auto episode = stalls.details(beneficiary);
    if (!episode)
        return;
    auto &attempt = attempts_[beneficiary];
    if (attempt.episode != episode->detected_at)
        attempt = {.episode = episode->detected_at};
    if (attempt.searches >= max_make_way_searches || goals.tick < attempt.retry_at)
        return;

    auto &fleet = goals.fleet;
    const auto blocker = parked_blocker(goals, beneficiary);
    if (!blocker)
        return;
    const auto index = *blocker;
    const auto &m = fleet.movers[index];
    if (trips_[index] || std::any_of(trips_.begin(), trips_.end(), [&](const auto &trip) {
            return trip && trip->beneficiary == beneficiary;
        }))
        return;

    ++attempt.searches;
    attempt.retry_at = goals.tick + make_way_retry_ticks;
    const auto destination = fleet.movers[beneficiary].destination;
    if (auto alternate = checked_route(goals, beneficiary, destination); !alternate.empty()) {
        install_route(goals, beneficiary, std::move(alternate), fleet.movers[beneficiary].goal);
        attempt.searches = max_make_way_searches;
        return;
    }
    const auto product = std::find_if(products.begin(), products.end(),
                                      [&](const Product &candidate) { return candidate.id == m.product; });
    if (at_parking(m) && !m.held && !coordinated(m) && !shuttle.contains(index) &&
        (!m.product || (product != products.end() && product->state != ProductState::processing))) {
        if (auto route = refuge_route(goals, index, beneficiary); !route.empty()) {
            install_route(goals, index, std::move(route), GoalKind::making_way);
            trips_[index] = Trip{beneficiary, destination, goals.tick + make_way_timeout};
            attempt.searches = max_make_way_searches;
            record(goals, EventKind::make_way_started, beneficiary, index);
            return;
        }
    }
    record(goals, EventKind::make_way_unavailable, beneficiary, index);
}

void MakeWay::update(Goals &goals, std::span<const Product> products, const Shuttle &shuttle,
                     const StallDetector &stalls) {
    if (trips_.empty() && !goals.motion.make_way)
        return;
    trips_.resize(goals.fleet.movers.size());
    attempts_.resize(trips_.size());
    for (std::size_t i = 0; i < trips_.size(); ++i)
        if (trips_[i])
            advance_trip(goals, i);
    if (goals.motion.make_way)
        for (std::size_t i = 0; i < trips_.size(); ++i)
            try_make_way(goals, products, shuttle, stalls, i);
}
} // namespace i4sim::detail

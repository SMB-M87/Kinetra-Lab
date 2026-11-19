#include "routing.h"
#include "geometry.h"
#include "simulation_constants.h"

#include <kinetra/navigation.h>

#include <algorithm>
#include <limits>

namespace i4sim::detail {
using namespace kinetra;
std::vector<Vec2> plan_route(const FloorPlan &floor, const Fleet &fleet, std::size_t mover, Vec2 target,
                             bool require_departure) {
    const auto &m = fleet.movers[mover];
    auto route = floor.grid().path(m.body.position, target, m.body.half_extents);
    if (route.empty() || (!require_departure && distance(target, m.parking) > same_point_mm &&
                          fleet.progress[mover].no_progress < obstruction_ticks))
        return route;
    std::vector<const Body *> obstacles;
    for (std::size_t j = 0; j < fleet.movers.size(); ++j) {
        const auto &other = fleet.movers[j];
        if (j != mover &&
            (!other.moving() || other.held || fleet.progress[j].no_progress >= obstruction_ticks))
            obstacles.push_back(&other.body);
    }
    auto clear = [&](Vec2 from, Vec2 to) {
        if (!floor.grid().clear(from, to, m.body.half_extents))
            return false;
        for (const auto *other : obstacles)
            if (crosses_mover(from, to, m.body.half_extents + clearance, *other))
                return false;
        return true;
    };
    Vec2 previous = m.body.position;
    bool obstructed = false;
    for (const auto point : route) {
        obstructed = obstructed || !clear(previous, point);
        previous = point;
    }
    if (!obstructed)
        return route;
    // Refine parking/stalled routes around rectangular stationary footprints.
    // Grid waypoints retain topology; obstacle corners allow tight aisle entries
    // that a cell center or a conservative circular recovery envelope cannot fit.
    std::vector<Vec2> nodes{m.body.position, target};
    nodes.insert(nodes.end(), route.begin(), route.end());
    for (const auto *other : obstacles) {
        const Vec2 extent = m.body.half_extents + other->half_extents + Vec2{3, 3};
        bool nearby = false;
        Vec2 from = m.body.position;
        for (const auto to : route) {
            nearby = nearby || distance_to_segment(other->position, from, to) <
                                   length(extent) + std::max(floor.cell_size().x, floor.cell_size().y);
            from = to;
        }
        if (!nearby)
            continue;
        for (float x : {-extent.x, extent.x})
            for (float y : {-extent.y, extent.y}) {
                const Vec2 corner = other->position + Vec2{x, y};
                if (clear(corner, corner))
                    nodes.push_back(corner);
            }
        // Keep pathological dense blueprints from creating an unbounded graph.
        if (nodes.size() > 256)
            return require_departure ? std::vector<Vec2>{} : route;
    }
    const float infinity = std::numeric_limits<float>::infinity();
    std::vector<float> costs(nodes.size(), infinity);
    std::vector<std::size_t> parent(nodes.size(), nodes.size());
    std::vector<bool> visited(nodes.size());
    costs[0] = 0;
    for (std::size_t count = 0; count < nodes.size(); ++count) {
        std::size_t current = nodes.size();
        float nearest = infinity;
        for (std::size_t i = 0; i < nodes.size(); ++i)
            if (!visited[i] && costs[i] < nearest) {
                current = i;
                nearest = costs[i];
            }
        if (current == nodes.size())
            break;
        if (current == 1) {
            std::vector<Vec2> refined;
            for (std::size_t i = 1; i != 0; i = parent[i])
                refined.push_back(nodes[i]);
            std::reverse(refined.begin(), refined.end());
            return refined;
        }
        visited[current] = true;
        for (std::size_t next = 1; next < nodes.size(); ++next) {
            const float candidate = nearest + distance(nodes[current], nodes[next]);
            if (!visited[next] && candidate < costs[next] && clear(nodes[current], nodes[next])) {
                costs[next] = candidate;
                parent[next] = current;
            }
        }
    }
    return require_departure ? std::vector<Vec2>{} : route;
}
void recover_route(const FloorPlan &floor, Fleet &fleet, std::size_t mover) {
    auto &m = fleet.movers[mover];
    std::vector<Body> obstacles;
    for (int id = 0; id < floor.grid().width() * floor.grid().height(); ++id) {
        if (!floor.grid().blocked(id))
            continue;
        Body b;
        b.shape = Shape::box;
        b.motion = Motion::fixed;
        b.position = floor.grid().center(id);
        b.half_extents = floor.cell_size() / 2;
        obstacles.push_back(b);
    }
    for (const auto &other : fleet.movers)
        if (&other != &m) {
            auto b = other.body;
            b.motion = Motion::fixed;
            obstacles.push_back(b);
        }
    const float radius = std::max(m.body.half_extents.x, m.body.half_extents.y) + 3;
    // Plan in resolution-sized units: millimeter-scale factory routes otherwise
    // exceed Navigation's spatial query budget even on a modest factory grid.
    const float scale = std::min(floor.cell_size().x, floor.cell_size().y) / 4;
    for (auto &obstacle : obstacles) {
        obstacle.position = obstacle.position / scale;
        obstacle.half_extents = obstacle.half_extents / scale;
    }
    Navigation navigation(1, radius / scale);
    navigation.rebuild({{0, 0}, floor.dimension() / scale}, obstacles);
    auto route = navigation.path(m.body.position / scale, m.destination / scale);
    for (auto &point : route)
        point = point * scale;
    if (!route.empty() && distance(route.back(), m.destination) < same_point_mm) {
        m.route = std::move(route);
        m.detour = true;
    }
    fleet.progress[mover].no_progress = 0;
    fleet.progress[mover].best_remaining = route_remaining(m.body.position, m.route);
}
} // namespace i4sim::detail

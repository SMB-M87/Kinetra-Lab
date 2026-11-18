#include "simulation.h"
#include <fstream>
#include <limits>
#include <kinetra/avoidance.h>
#include <kinetra/navigation.h>
#include <nlohmann/json.hpp>
#include <stdexcept>

namespace i4sim {
using namespace kinetra;
namespace {
float route_remaining(Vec2 position, const std::vector<Vec2> &route) {
    float remaining = 0;
    for (const auto point : route) {
        remaining += distance(position, point);
        position = point;
    }
    return remaining;
}
// Strict interior slab test against the Minkowski box of two movers.
bool crosses_mover(Vec2 start, Vec2 end, Vec2 extent, const Body &other) {
    const Vec2 low = other.position - other.half_extents - extent;
    const Vec2 high = other.position + other.half_extents + extent;
    const Vec2 travel = end - start;
    float enter = 0, exit = 1;
    for (int axis = 0; axis < 2; ++axis) {
        const float p = axis == 0 ? start.x : start.y;
        const float v = axis == 0 ? travel.x : travel.y;
        const float lo = axis == 0 ? low.x : low.y;
        const float hi = axis == 0 ? high.x : high.y;
        if (v == 0) {
            if (p <= lo || p >= hi)
                return false;
        } else {
            float near = (lo - p) / v, far = (hi - p) / v;
            if (near > far)
                std::swap(near, far);
            enter = std::max(enter, near);
            exit = std::min(exit, far);
            if (enter >= exit)
                return false;
        }
    }
    return true;
}
Segment stop_before(const Body &body, Aabb area) {
    const Vec2 center = (area.min + area.max) / 2;
    const Vec2 half = (area.max - area.min) / 2;
    const Vec2 offset = body.position - center;
    const Vec2 gap{std::abs(offset.x) - half.x - body.half_extents.x,
                   std::abs(offset.y) - half.y - body.half_extents.y};
    Vec2 start = area.min, end = area.max;
    if (gap.x >= gap.y) start.x = end.x = offset.x > 0 ? area.max.x : area.min.x;
    else start.y = end.y = offset.y > 0 ? area.max.y : area.min.y;
    return {start, end};
}
} // namespace
std::vector<Interaction> recipe(ProductKind kind) {
    using enum Interaction;
    if (kind == ProductKind::personalized)
        return {place_housing, place_trimmer, place_lever, place_card, personalize_card, remove_assy};
    return {place_housing, place_trimmer, place_lever, remove_assy};
}
Simulation::Simulation(Blueprint b, std::uint32_t seed)
    : blueprint_(std::move(b)), grid_(blueprint_.dimension, blueprint_.cell_size),
      traffic_index_(std::max(blueprint_.cell_size.x, blueprint_.cell_size.y)), random_(seed) {
    auto block_area = [&](Vec2 p, Vec2 size) {
        for (float y = p.y; y < p.y + size.y; y += blueprint_.cell_size.y)
            for (float x = p.x; x < p.x + size.x; x += blueprint_.cell_size.x) {
                const int id = grid_.cell(Vec2{x, y} + blueprint_.cell_size / 2);
                if (id >= 0)
                    grid_.block(id);
            }
    };
    for (const auto &z : blueprint_.zones)
        block_area(z.position, z.dimension);
    const float radius = std::max(blueprint_.cell_size.x, blueprint_.cell_size.y) / 2;
    for (const auto &definition : blueprint_.producers) {
        Producer p;
        p.definition = definition;
        producers_.push_back(p);
        // A producer already in a forbidden cell does not add another footprint.
        if (!grid_.blocked(grid_.cell(definition.position)))
            block_area(definition.position - Vec2{radius, radius}, {radius * 2, radius * 2});
    }
    borders_ = grid_.borders();
    auto open = [&](int x, int y) {
        return x >= 0 && y >= 0 && x < grid_.width() && y < grid_.height() &&
               !grid_.blocked(y * grid_.width() + x);
    };
    auto degree = [&](int x, int y) {
        return int(open(x - 1, y)) + int(open(x + 1, y)) +
               int(open(x, y - 1)) + int(open(x, y + 1));
    };
    // Only isolated T/+ junctions with one-cell corridor arms. Adjacent junctions
    // and open factory floors need a richer zone/exit reservation policy.
    for (int y = 0; y < grid_.height(); ++y)
        for (int x = 0; x < grid_.width(); ++x) {
            if (!open(x, y) || degree(x, y) < 3) continue;
            bool corridors = true;
            for (auto delta : {Vec2{-1, 0}, Vec2{1, 0}, Vec2{0, -1}, Vec2{0, 1}}) {
                const int nx = x + int(delta.x), ny = y + int(delta.y);
                if (open(nx, ny) && degree(nx, ny) != 2) corridors = false;
            }
            if (corridors) traffic_zones_.push_back({grid_.center(y * grid_.width() + x), blueprint_.cell_size / 2, false, false, {}, std::nullopt});
        }
    junction_count_ = traffic_zones_.size();
    // Straight one-cell corridors are exclusive zones only when both ends open
    // into a two-cell-deep, three-cell-wide room. Dead ends and bends are excluded.
    for (bool vertical : {false, true}) {
        const int along_limit = vertical ? grid_.height() : grid_.width();
        const int across_limit = vertical ? grid_.width() : grid_.height();
        auto free = [&](int along, int across) {
            return vertical ? open(across, along) : open(along, across);
        };
        auto narrow = [&](int along, int across) {
            return free(along, across) && free(along - 1, across) && free(along + 1, across) &&
                   !free(along, across - 1) && !free(along, across + 1);
        };
        auto room = [&](int mouth, int direction, int across) {
            for (int depth = 0; depth < 2; ++depth)
                for (int side = -1; side <= 1; ++side)
                    if (!free(mouth + depth * direction, across + side)) return false;
            return true;
        };
        for (int across = 0; across < across_limit; ++across)
            for (int along = 0; along < along_limit; ++along) {
                if (!narrow(along, across) || narrow(along - 1, across)) continue;
                const int first = along;
                while (along + 1 < along_limit && narrow(along + 1, across)) ++along;
                const int last = along;
                // Bound zone indexing work for unusually large custom layouts.
                if (last - first + 1 > 256 ||
                    !room(first - 1, -1, across) || !room(last + 1, 1, across)) continue;
                const Vec2 cell_start = vertical ? Vec2{float(across), float(first)}
                                                : Vec2{float(first), float(across)};
                const Vec2 cells = vertical ? Vec2{1, float(last - first + 1)}
                                           : Vec2{float(last - first + 1), 1};
                const Vec2 size{cells.x * blueprint_.cell_size.x, cells.y * blueprint_.cell_size.y};
                const Vec2 low{cell_start.x * blueprint_.cell_size.x, cell_start.y * blueprint_.cell_size.y};
                traffic_zones_.push_back({low + size / 2, size / 2, true, vertical, {}, std::nullopt});
            }
    }
    std::vector<Aabb> junction_boxes;
    for (const auto &junction : traffic_zones_) {
        const auto extent = junction.half_extents + Vec2{2, 2};
        junction_boxes.push_back({junction.center - extent, junction.center + extent});
    }
    traffic_index_.rebuild(junction_boxes);
    for (const auto &definition : blueprint_.movers) {
        Mover m;
        m.definition = definition;
        m.body.shape = Shape::box;
        m.body.motion = Motion::controlled;
        m.body.half_extents = mover_spec(definition.model).dimension / 2;
        mover_max_half_.x = std::max(mover_max_half_.x, m.body.half_extents.x);
        mover_max_half_.y = std::max(mover_max_half_.y, m.body.half_extents.y);
        m.body.position = definition.position + m.body.half_extents;
        m.parking = m.destination = m.body.position;
        if (grid_.blocked(grid_.cell(m.parking)))
            throw std::runtime_error("Mover starts in a forbidden cell: " + definition.id);
        movers_.push_back(m);
    }
    update_heatmap();
}
void Simulation::configure_motion(MotionSettings settings) {
    if (!paused)
        throw std::logic_error("Pause before changing motion settings");
    if (int(settings.profile) < int(SteeringProfile::improved) ||
        int(settings.profile) > int(SteeringProfile::reciprocal) ||
        !std::isfinite(settings.speed) || settings.speed <= 0 ||
        !std::isfinite(settings.force) || settings.force <= 0 ||
        !std::isfinite(settings.horizon) || settings.horizon <= 0 || settings.horizon > 64)
        throw std::invalid_argument("Invalid motion settings");
    for (auto &zone : traffic_zones_)
        if (!(zone.corridor ? settings.corridor_traffic : settings.junction_traffic)) {
            zone.gate.clear();
            zone.exit.reset();
            zone.recovery.reset();
        }
    for (auto &m : movers_) {
        if (!settings.junction_traffic) m.junction_waiting = false;
        if (!settings.corridor_traffic) m.corridor_waiting = m.corridor_retreating = false;
        if (!m.junction_waiting && !m.corridor_waiting) m.junction_exit_waiting = false;
    }
    traffic_stops_.clear();
    motion_ = settings;
    update_heatmap();
    for (auto &m : movers_) {
        m.body.velocity = limited(m.body.velocity, settings.speed);
        if (!settings.parking_traffic && m.parking_deferred)
            set_goal(m, m.parking);
        else if (m.moving) {
            const bool deferred = m.parking_deferred;
            set_goal(m, m.destination);
            m.parking_deferred = deferred;
        }
    }
}
void Simulation::single_step() {
    if (!paused)
        return;
    paused = false;
    tick();
    paused = true;
}
void Simulation::updates_per_second(unsigned rate) {
    if (rate < 1 || rate > 10000)
        throw std::invalid_argument("UPS must be in 1..10000");
    ups_ = rate;
    clock_ = FixedClock(1.0 / rate, 256);
}
void Simulation::advance(double elapsed) {
    if (!std::isfinite(elapsed) || elapsed < 0)
        throw std::invalid_argument("Invalid elapsed time");
    if (paused)
        return;
    const int count = clock_.advance(elapsed);
    for (int i = 0; i < count && !paused; ++i)
        tick();
}
std::uint64_t Simulation::add_product(ProductKind kind) {
    if (products_.size() >= std::max<std::size_t>(1, movers_.size()))
        throw std::runtime_error("Local workload capacity reached");
    const auto id = next_product_++;
    Product p{id, kind};
    p.created = ticks_;
    products_.push_back(p);
    return id;
}
void Simulation::toggle_mover(std::size_t index) {
    auto &m = movers_.at(index);
    m.blocked = !m.blocked;
    if (m.blocked)
        m.body.velocity = {};
}
void Simulation::toggle_producer(std::size_t index) {
    auto &p = producers_.at(index);
    p.blocked = !p.blocked;
}
std::vector<Vec2> Simulation::plan_route(const Mover &m, Vec2 target, bool require_departure) const {
    auto route = grid_.path(m.body.position, target, m.body.half_extents);
    if (route.empty() || (!require_departure && distance(target, m.parking) > .05f && m.no_progress < 100))
        return route;
    std::vector<const Body *> obstacles;
    for (const auto &other : movers_)
        if (&other != &m && (!other.moving || other.blocked || other.no_progress >= 100))
            obstacles.push_back(&other.body);
    auto clear = [&](Vec2 from, Vec2 to) {
        if (!grid_.clear(from, to, m.body.half_extents))
            return false;
        for (const auto *other : obstacles)
            if (crosses_mover(from, to, m.body.half_extents + Vec2{2, 2}, *other))
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
                                   length(extent) + std::max(blueprint_.cell_size.x, blueprint_.cell_size.y);
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
void Simulation::set_goal(Mover &m, Vec2 target) {
    cancel_corridor_recovery(m);
    m.parking_deferred = false;
    m.destination = target;
    m.route = plan_route(m, target);
    m.unreachable = m.route.empty();
    m.moving = !m.unreachable;
    m.stalled = m.replan = m.no_progress = 0;
    m.best_remaining = route_remaining(m.body.position, m.route);
    m.detour = false;
    m.recovery_attempts = 0;
}
bool Simulation::move_to(std::size_t index, Vec2 target) {
    auto &m = movers_.at(index);
    if (m.blocked || m.product || grid_.blocked(grid_.cell(target)))
        return false;
    auto route = grid_.path(m.body.position, target, m.body.half_extents);
    if (route.empty())
        return false;
    set_goal(m, target);
    m.manual = true;
    return true;
}
Vec2 Simulation::holding_position(const Mover &m, Vec2 target) const {
    // Each mover retains its own blueprint parking reservation. Prefer that
    // known safe waiting area over filling a narrow factory aisle with queues.
    if (grid_.cell(m.parking) != grid_.cell(target) &&
        distance(m.parking, target) > length(m.body.half_extents) * 2 &&
        (!motion_.parking_traffic || !parking_conflict(m, m.parking)) &&
        !grid_.path(m.body.position, m.parking, m.body.half_extents).empty())
        return m.parking;
    float best = std::numeric_limits<float>::infinity();
    Vec2 result = m.parking;
    for (int id = 0; id < grid_.width() * grid_.height(); ++id) {
        if (grid_.blocked(id))
            continue;
        const auto candidate = grid_.center(id);
        bool available = true;
        for (const auto &p : producers_) {
            const Vec2 goal = p.definition.processing_position +
                              Vec2{blueprint_.mover_max_extent, blueprint_.mover_max_extent} / 2;
            if (grid_.cell(goal) == id)
                available = false;
        }
        for (const auto &other : movers_)
            if (&other != &m &&
                (distance(candidate, other.body.position) <
                     length(m.body.half_extents + other.body.half_extents) + 10 ||
                 (other.moving && distance(candidate, other.destination) <
                                      length(m.body.half_extents + other.body.half_extents) + 10)))
                available = false;
        if (!available || (motion_.parking_traffic && parking_conflict(m, candidate)))
            continue;
        const float score = distance(candidate, target) +
                            grid_.weight(id) * std::max(blueprint_.cell_size.x, blueprint_.cell_size.y);
        if (score < best && !grid_.path(m.body.position, candidate, m.body.half_extents).empty()) {
            best = score;
            result = candidate;
        }
    }
    return result;
}
std::size_t Simulation::parking_waiting() const {
    return std::count_if(movers_.begin(), movers_.end(), [](const Mover &m) { return m.parking_deferred; });
}
bool Simulation::parking_conflict(const Mover &m, Vec2 position) const {
    Body parked = m.body;
    parked.position = position;
    for (const auto &other : movers_) {
        if (&other == &m || other.blocked || !other.moving || other.parking_deferred ||
            (!other.manual && distance(other.destination, other.parking) < 0.05f))
            continue;
        Vec2 from = other.body.position;
        for (const auto to : other.route) {
            if (crosses_mover(from, to, other.body.half_extents + Vec2{2, 2}, parked))
                return true;
            from = to;
        }
    }
    return false;
}
void Simulation::coordinate_parking() {
    if (!motion_.parking_traffic)
        return;
    for (auto &m : movers_) {
        if (m.blocked || m.manual || (!m.parking_deferred &&
            (!m.moving || distance(m.destination, m.parking) >= 0.05f)))
            continue;
        if (!parking_conflict(m, m.parking)) {
            if (m.parking_deferred)
                set_goal(m, m.parking);
            continue;
        }
        if (m.parking_deferred)
            continue;
        // Reuse footprint-aware staging selection, excluding protected routes.
        // If no staging place is reachable, retain the existing goal and let
        // local avoidance stop safely; retry the interlock on the next tick.
        const auto staging = holding_position(m, m.parking);
        if (distance(staging, m.parking) > 0.05f) {
            set_goal(m, staging);
            m.parking_deferred = true;
        }
    }
}
void Simulation::update_heatmap() {
    std::vector<float> weights(grid_.width() * grid_.height());
    for (const auto &m : movers_) {
        const Vec2 size = m.body.half_extents * 2;
        const float x = blueprint_.cell_size.x / size.x, y = blueprint_.cell_size.y / size.y;
        const float delta = (x < 2 && y < 2 ? 16.f : x < 2 || y < 2 ? 8.f : 4.f) / 4;
        for (Vec2 corner : {m.body.position - m.body.half_extents, m.body.position + m.body.half_extents,
                            m.body.position + Vec2{-m.body.half_extents.x, m.body.half_extents.y},
                            m.body.position + Vec2{m.body.half_extents.x, -m.body.half_extents.y}}) {
            const int id = grid_.cell(corner);
            if (id >= 0)
                weights[id] += delta;
        }
    }
    if (!motion_.heatmap)
        std::fill(weights.begin(), weights.end(), 0.f);
    grid_.weights(weights);
}
void Simulation::schedule(Product &product) {
    const auto interaction = recipe(product.kind).at(product.step);
    if (!product.producer) {
        double best = std::numeric_limits<double>::infinity();
        for (std::size_t i = 0; i < producers_.size(); ++i) {
            const auto &p = producers_[i];
            if (p.blocked || p.queue.size() >= blueprint_.producer_max_queue ||
                !supports(p.definition.model, interaction))
                continue;
            const auto goal = p.definition.processing_position +
                              Vec2{blueprint_.mover_max_extent, blueprint_.mover_max_extent} / 2;
            if (grid_.blocked(grid_.cell(goal)))
                continue;
            const double transport =
                product.mover ? 1 + distance(movers_[*product.mover].body.position, goal) : 1;
            const double cost = (3 + 5 * p.queue.size()) * transport;
            if (cost < best) {
                best = cost;
                product.producer = i;
            }
        }
        if (!product.producer)
            return;
        producers_[*product.producer].queue.push_back(product.id);
    }
    const auto goal = producers_[*product.producer].definition.processing_position +
                      Vec2{blueprint_.mover_max_extent, blueprint_.mover_max_extent} / 2;
    std::vector<Vec2> departure;
    if (!product.mover) {
        float best = std::numeric_limits<float>::infinity();
        for (std::size_t i = 0; i < movers_.size(); ++i) {
            const auto &m = movers_[i];
            if (m.blocked || m.product || m.manual || m.parking_deferred)
                continue;
            const float cost = distance(m.body.position, goal);
            if (cost < best) {
                auto candidate = plan_route(m, goal, true);
                if (!candidate.empty()) {
                    best = cost;
                    product.mover = i;
                    departure = std::move(candidate);
                }
            }
        }
        if (!product.mover)
            return;
        movers_[*product.mover].product = product.id;
    }
    auto &m = movers_[*product.mover];
    if (m.blocked)
        return;
    set_goal(m, goal);
    if (!departure.empty()) {
        m.route = std::move(departure);
        m.best_remaining = route_remaining(m.body.position, m.route);
    }
    product.state = ProductState::transporting;
}
void Simulation::recover_route(Mover &m) {
    std::vector<Body> obstacles;
    for (int id = 0; id < grid_.width() * grid_.height(); ++id) {
        if (!grid_.blocked(id))
            continue;
        Body b;
        b.shape = Shape::box;
        b.motion = Motion::fixed;
        b.position = grid_.center(id);
        b.half_extents = blueprint_.cell_size / 2;
        obstacles.push_back(b);
    }
    for (const auto &other : movers_)
        if (&other != &m) {
            auto b = other.body;
            b.motion = Motion::fixed;
            obstacles.push_back(b);
        }
    const float radius = std::max(m.body.half_extents.x, m.body.half_extents.y) + 3;
    // Plan in resolution-sized units: millimeter-scale factory routes otherwise
    // exceed Navigation's spatial query budget even on a modest factory grid.
    const float scale = std::min(blueprint_.cell_size.x, blueprint_.cell_size.y) / 4;
    for (auto &obstacle : obstacles) {
        obstacle.position = obstacle.position / scale;
        obstacle.half_extents = obstacle.half_extents / scale;
    }
    Navigation navigation(1, radius / scale);
    navigation.rebuild({{0, 0}, blueprint_.dimension / scale}, obstacles);
    auto route = navigation.path(m.body.position / scale, m.destination / scale);
    for (auto &point : route)
        point = point * scale;
    if (!route.empty() && distance(route.back(), m.destination) < 0.05f) {
        m.route = std::move(route);
        m.detour = true;
    }
    m.no_progress = 0;
    m.best_remaining = route_remaining(m.body.position, m.route);
}
bool Simulation::prepare_route(Mover &m) {
    if (m.blocked || !m.moving) return false;
    // Congestion includes this mover's footprint; it is not evidence that
    // its route is obstructed. Replan only after sustained lack of progress.
    if (!m.detour && ++m.replan >= 100 && m.no_progress >= 100) {
        auto route = plan_route(m, m.destination);
        if (!route.empty()) {
            m.route = std::move(route);
            m.best_remaining = route_remaining(m.body.position, m.route);
        }
        m.replan = 0;
    }
    if (m.route.empty()) {
        set_goal(m, m.destination);
        if (m.unreachable)
            return false;
    }
    if (++m.no_progress >= 120 + std::min(10u, m.recovery_attempts) * 30 && motion_.recovery) {
        recover_route(m);
        ++m.recovery_attempts;
    }
    while (m.route.size() > 1 && distance(m.body.position, m.route.front()) < 3 &&
           grid_.clear(m.body.position, m.route[1], m.body.half_extents))
        m.route.erase(m.route.begin());
    return true;
}
Vec2 Simulation::junction_center(std::size_t index) const {
    if (index >= junction_count_) throw std::out_of_range("Junction index");
    return traffic_zones_[index].center;
}
std::size_t Simulation::corridor_waiting() const {
    return std::count_if(movers_.begin(), movers_.end(), [](const Mover &m) { return m.corridor_waiting; });
}
Aabb Simulation::corridor_bounds(std::size_t index) const {
    if (index >= corridor_count()) throw std::out_of_range("Corridor index");
    const auto &zone = traffic_zones_[junction_count_ + index];
    return {zone.center - zone.half_extents, zone.center + zone.half_extents};
}
std::optional<Aabb> Simulation::corridor_exit_reservation(std::size_t index) const {
    if (index >= corridor_count()) throw std::out_of_range("Corridor index");
    const auto &exit = traffic_zones_[junction_count_ + index].exit;
    return exit ? std::optional(exit->area) : std::nullopt;
}
std::size_t Simulation::junction_waiting() const {
    return std::count_if(movers_.begin(), movers_.end(), [](const Mover &m) { return m.junction_waiting; });
}
std::size_t Simulation::junction_exit_waiting() const {
    return std::count_if(movers_.begin(), movers_.end(), [](const Mover &m) { return m.junction_exit_waiting; });
}
std::optional<Aabb> Simulation::junction_exit_reservation(std::size_t index) const {
    if (index >= junction_count_) throw std::out_of_range("Junction index");
    const auto &exit = traffic_zones_[index].exit;
    return exit ? std::optional(exit->area) : std::nullopt;
}
std::optional<Aabb> Simulation::traffic_exit(const Mover &m, const TrafficZone &junction) const {
    Body zone;
    zone.position = junction.center;
    zone.half_extents = junction.half_extents + Vec2{2, 2};
    const Vec2 extent = zone.half_extents + m.body.half_extents;
    const Vec2 low = zone.position - extent, high = zone.position + extent;
    bool entered = false;
    Vec2 from = m.body.position;
    for (auto to : m.route) {
        entered = entered || crosses_mover(from, to, m.body.half_extents, zone);
        const bool outside = to.x < low.x || to.x > high.x || to.y < low.y || to.y > high.y;
        if (entered && outside) {
            // Find where the whole body first clears the Minkowski-expanded zone,
            // then reserve enough space to brake outside, capped by the route goal.
            const auto travel = to - from;
            float leave = 1;
            if (travel.x > 0) leave = std::min(leave, (high.x - from.x) / travel.x);
            if (travel.x < 0) leave = std::min(leave, (low.x - from.x) / travel.x);
            if (travel.y > 0) leave = std::min(leave, (high.y - from.y) / travel.y);
            if (travel.y < 0) leave = std::min(leave, (low.y - from.y) / travel.y);
            const auto cleared = from + travel * std::clamp(leave, 0.f, 1.f);
            const float braking_space = motion_.speed * motion_.speed / motion_.force + 2;
            const auto end = cleared + limited(to - cleared, braking_space);
            const auto footprint = m.body.half_extents + Vec2{2, 2};
            if (!grid_.clear(cleared, end, footprint)) return std::nullopt;
            return Aabb{{std::min(cleared.x, end.x) - footprint.x, std::min(cleared.y, end.y) - footprint.y},
                        {std::max(cleared.x, end.x) + footprint.x, std::max(cleared.y, end.y) + footprint.y}};
        }
        from = to;
    }
    return std::nullopt;
}
std::size_t Simulation::corridor_recovering() const {
    return std::count_if(traffic_zones_.begin(), traffic_zones_.end(),
                         [](const TrafficZone &zone) { return zone.recovery.has_value(); });
}
std::optional<Vec2> Simulation::corridor_retreat_target(std::size_t mover) const {
    if (mover >= movers_.size()) throw std::out_of_range("Mover index");
    for (const auto &zone : traffic_zones_)
        if (zone.recovery && zone.recovery->yielding == mover) return zone.recovery->route.front();
    return std::nullopt;
}
void Simulation::cancel_corridor_recovery(Mover &m) {
    for (auto &zone : traffic_zones_) {
        if (!zone.recovery) continue;
        const auto &recovery = *zone.recovery;
        if (&m != &movers_[recovery.yielding] && &m != &movers_[recovery.passing]) continue;
        for (auto id : {recovery.yielding, recovery.passing}) {
            movers_[id].corridor_retreating = false;
            movers_[id].corridor_waiting = false;
            if (id < traffic_controls_.size()) traffic_controls_[id].reset();
        }
        zone.recovery.reset();
        zone.retry_at = ticks_ + 120;
    }
}
void Simulation::recover_corridors(const SpatialGrid &index) {
    for (std::size_t j = junction_count_; j < traffic_zones_.size(); ++j) {
        auto &zone = traffic_zones_[j];
        Body corridor;
        corridor.position = zone.center;
        corridor.half_extents = zone.half_extents + Vec2{2, 2};
        auto inside = [&](const Mover &m) {
            return crosses_mover(m.body.position, m.body.position, m.body.half_extents, corridor);
        };
        if (zone.recovery) {
            auto &recovery = *zone.recovery;
            if (!movers_[recovery.yielding].moving || !movers_[recovery.passing].moving ||
                (recovery.yielded && !inside(movers_[recovery.passing]))) {
                for (auto id : {recovery.yielding, recovery.passing}) {
                    movers_[id].no_progress = movers_[id].replan = 0;
                    movers_[id].best_remaining = route_remaining(movers_[id].body.position, movers_[id].route);
                }
                zone.recovery.reset();
                zone.retry_at = ticks_ + 120;
            }
        }
        if (!zone.recovery && ticks_ >= zone.retry_at) {
            std::vector<std::size_t> occupants;
            for (auto id : index.query({corridor.position - corridor.half_extents,
                                       corridor.position + corridor.half_extents}))
                if (inside(movers_[id])) occupants.push_back(id);
            if (occupants.size() != 2) continue;
            auto axis = [&](Vec2 value) { return zone.vertical ? value.y : value.x; };
            const auto &a = movers_[occupants[0]], &b = movers_[occupants[1]];
            if (a.blocked || b.blocked || !a.moving || !b.moving || a.no_progress < 120 || b.no_progress < 120 ||
                length(a.body.velocity) > 0.05f || length(b.body.velocity) > 0.05f ||
                axis(a.destination - a.body.position) * axis(b.destination - b.body.position) >= 0) continue;
            zone.retry_at = ticks_ + 120;
            float best = std::numeric_limits<float>::infinity();
            for (auto id : occupants) {
                bool claimed = false;
                for (const auto &other : traffic_zones_)
                    if (&other != &zone && other.recovery &&
                        (other.recovery->yielding == id || other.recovery->passing == id))
                        claimed = true;
                if (claimed) continue;
                const auto &m = movers_[id];
                const float backwards = axis(m.destination - m.body.position) > 0 ? -1.f : 1.f;
                for (float side : {-1.f, 1.f}) {
                    Vec2 refuge = zone.center;
                    if (zone.vertical) {
                        refuge.y += backwards * (zone.half_extents.y +
                            std::max(blueprint_.cell_size.y / 2, m.body.half_extents.y + 4));
                        refuge.x += side * blueprint_.cell_size.x;
                    } else {
                        refuge.x += backwards * (zone.half_extents.x +
                            std::max(blueprint_.cell_size.x / 2, m.body.half_extents.x + 4));
                        refuge.y += side * blueprint_.cell_size.y;
                    }
                    auto route = grid_.path(m.body.position, refuge, m.body.half_extents + Vec2{2, 2});
                    if (route.empty()) continue;
                    bool clear = true;
                    Vec2 from = m.body.position;
                    for (auto to : route) {
                        const auto extent = m.body.half_extents + Vec2{2, 2};
                        const Aabb area{{std::min(from.x, to.x) - extent.x, std::min(from.y, to.y) - extent.y},
                                        {std::max(from.x, to.x) + extent.x, std::max(from.y, to.y) + extent.y}};
                        for (auto other : index.query(area))
                            if (other != id && crosses_mover(from, to, extent, movers_[other].body)) clear = false;
                        for (std::size_t k = 0; k < traffic_zones_.size(); ++k) {
                            if (k == j || !traffic_zones_[k].exit) continue;
                            const auto area = traffic_zones_[k].exit->area;
                            Body reserved;
                            reserved.position = (area.min + area.max) / 2;
                            reserved.half_extents = (area.max - area.min) / 2;
                            if (crosses_mover(from, to, extent, reserved)) clear = false;
                        }
                        from = to;
                        if (!clear) break;
                    }
                    const float cost = route_remaining(m.body.position, route);
                    if (clear && (cost < best - 0.01f ||
                        (std::abs(cost - best) <= 0.01f && zone.recovery && id > zone.recovery->yielding))) {
                        const auto passing = id == occupants[0] ? occupants[1] : occupants[0];
                        zone.recovery = CorridorRecovery{id, passing, refuge, movers_[passing].body.position,
                                                         std::move(route), false};
                        best = cost;
                    }
                }
            }
            if (zone.recovery) zone.exit.reset();
        }
        if (!zone.recovery) continue;
        auto &recovery = *zone.recovery;
        auto &yielding = movers_[recovery.yielding];
        while (recovery.route.size() > 1 && distance(yielding.body.position, recovery.route.front()) < 3 &&
               grid_.clear(yielding.body.position, recovery.route[1], yielding.body.half_extents))
            recovery.route.erase(recovery.route.begin());
        recovery.yielded = recovery.yielded || (distance(yielding.body.position, recovery.refuge) < 0.1f &&
                                               length(yielding.body.velocity) < 0.05f);
        traffic_controls_[recovery.yielding] = TrafficControl{j, recovery.route.front(), recovery.route.size() == 1};
        yielding.corridor_retreating = true;
        if (recovery.yielded) yielding.corridor_waiting = true;
        else {
            traffic_controls_[recovery.passing] = TrafficControl{j, recovery.hold, true};
            movers_[recovery.passing].corridor_waiting = true;
        }
    }
}
void Simulation::coordinate_traffic(const SpatialGrid &index, std::span<const Aabb> bodies) {
    traffic_stops_.assign(movers_.size(), {});
    traffic_controls_.assign(movers_.size(), std::nullopt);
    for (auto &m : movers_) m.corridor_retreating = false;
    for (auto &m : movers_) m.junction_waiting = m.junction_exit_waiting = m.corridor_waiting = false;
    if (!motion_.junction_traffic && !motion_.corridor_traffic) return;
    if (motion_.corridor_traffic) recover_corridors(index);
    std::vector<std::vector<std::uint64_t>> requests(traffic_zones_.size()), occupants(traffic_zones_.size());
    std::vector<std::optional<std::size_t>> next(movers_.size());
    const float reach = 2 * std::max(blueprint_.cell_size.x, blueprint_.cell_size.y) +
                        motion_.speed * motion_.speed / motion_.force;
    for (std::size_t i = 0; i < movers_.size(); ++i) {
        const auto &m = movers_[i];
        float nearest = std::numeric_limits<float>::infinity();
        const auto range = Vec2{reach, reach} + m.body.half_extents;
        for (auto j : traffic_index_.query({m.body.position - range, m.body.position + range})) {
            const auto &junction = traffic_zones_[j];
            if (!(junction.corridor ? motion_.corridor_traffic : motion_.junction_traffic)) continue;
            const Vec2 half = junction.half_extents + Vec2{2, 2};
            const auto offset = m.body.position - traffic_zones_[j].center;
            const auto extent = m.body.half_extents + half;
            if (std::abs(offset.x) < extent.x && std::abs(offset.y) < extent.y)
                occupants[j].push_back(i);
            if (m.blocked || !m.moving || m.route.empty()) continue;
            Body zone;
            zone.position = traffic_zones_[j].center;
            zone.half_extents = half;
            // A new goal may cancel a route while the mover is still coasting
            // toward the zone. Retain the request until braking can clear it.
            const float braking_time = length(m.body.velocity) / motion_.force + 1;
            if (crosses_mover(m.body.position, m.body.position + m.body.velocity * braking_time,
                              m.body.half_extents, zone)) {
                if (nearest > 0) { nearest = 0; next[i] = j; }
                continue;
            }
            float travelled = 0;
            Vec2 from = m.body.position;
            for (auto to : m.route) {
                const float segment = distance(from, to);
                to = from + limited(to - from, std::max(0.f, reach - travelled));
                if (crosses_mover(from, to, m.body.half_extents, zone)) {
                    // Junctions have disjoint cells; route distance orders requests.
                    const float along = travelled + std::clamp(dot(zone.position - from, normalized(to - from)),
                                                               0.f, distance(from, to));
                    if (along < nearest) { nearest = along; next[i] = j; }
                    break;
                }
                travelled += segment;
                if (travelled >= reach) break;
                from = to;
            }
        }
        if (next[i]) requests[*next[i]].push_back(i);
    }
    // Release stale reservations before considering any new grant. Existing grants
    // keep their exit even after a goal change, until request and occupancy clear.
    std::vector<bool> occupied(movers_.size());
    for (std::size_t j = 0; j < traffic_zones_.size(); ++j) {
        for (auto id : occupants[j]) occupied[id] = true;
        auto &exit = traffic_zones_[j].exit;
        if (exit && std::find(requests[j].begin(), requests[j].end(), exit->agent) == requests[j].end() &&
            std::find(occupants[j].begin(), occupants[j].end(), exit->agent) == occupants[j].end())
            exit.reset();
    }
    for (std::size_t j = 0; j < traffic_zones_.size(); ++j) {
        auto &junction = traffic_zones_[j];
        std::vector<std::uint64_t> blocked;
        std::vector<std::optional<Aabb>> exits(requests[j].size());
        for (std::size_t n = 0; n < requests[j].size(); ++n) {
            const auto id = requests[j][n];
            if (junction.exit && junction.exit->agent == id) continue;
            exits[n] = traffic_exit(movers_[id], junction);
            bool available = exits[n].has_value();
            if (available) {
                Body pocket;
                pocket.position = (exits[n]->min + exits[n]->max) / 2;
                pocket.half_extents = (exits[n]->max - exits[n]->min) / 2;
                const float stopping = motion_.speed * (motion_.speed / motion_.force + 1);
                const Vec2 padding{stopping, stopping};
                for (auto other : index.query({exits[n]->min - padding, exits[n]->max + padding})) {
                    if (other == id) continue;
                    const auto &moving = movers_[other];
                    const auto velocity = moving.moving && !moving.blocked ? moving.body.velocity : Vec2{};
                    const auto coast = moving.body.position + velocity * (length(velocity) / motion_.force + 1);
                    if (bodies[other].overlaps(*exits[n]) ||
                        crosses_mover(moving.body.position, coast, moving.body.half_extents, pocket))
                        available = false;
                }
                for (std::size_t k = 0; k < traffic_zones_.size(); ++k)
                    if (k != j && traffic_zones_[k].exit && traffic_zones_[k].exit->area.overlaps(*exits[n]))
                        available = false;
            }
            if (!available) blocked.push_back(id);
        }
        const auto admission = junction.gate.update(requests[j], occupants[j], blocked);
        if (junction.exit && admission.agent != junction.exit->agent) junction.exit.reset();
        for (std::size_t n = 0; n < requests[j].size(); ++n) {
            const auto id = requests[j][n];
            if (admission.agent == id && !junction.exit && exits[n] &&
                std::find(blocked.begin(), blocked.end(), id) == blocked.end())
                junction.exit = ExitReservation{id, *exits[n]};
            if (admission.agent == id || occupied[id] ||
                (traffic_controls_[id] && traffic_controls_[id]->zone == j)) continue;
            auto half = junction.half_extents + Vec2{2, 2};
            if (junction.corridor) {
                // Leave a body-length exit pocket free at either mouth, even
                // when a parked occupant has no route indicating its exit yet.
                const float braking = motion_.speed * motion_.speed / motion_.force + 6;
                if (!junction.vertical)
                    half.x += 2 * mover_max_half_.x + braking;
                else
                    half.y += 2 * mover_max_half_.y + braking;
            }
            traffic_stops_[id].push_back(stop_before(movers_[id].body,
                {junction.center - half, junction.center + half}));
            if (junction.corridor) movers_[id].corridor_waiting = true;
            else movers_[id].junction_waiting = true;
            movers_[id].junction_exit_waiting = std::find(blocked.begin(), blocked.end(), id) != blocked.end();
        }
    }
    // Keep approaching traffic out of an admitted mover's exit pocket. Agents
    // already inside a pocket or junction may always leave; never trap an occupant.
    for (std::size_t j = 0; j < traffic_zones_.size(); ++j) {
        const auto &junction = traffic_zones_[j];
        if (!junction.exit) continue;
        const auto &exit = *junction.exit;
        Body pocket;
        pocket.position = (exit.area.min + exit.area.max) / 2;
        pocket.half_extents = (exit.area.max - exit.area.min) / 2;
        for (auto id : index.query({exit.area.min - Vec2{reach, reach}, exit.area.max + Vec2{reach, reach}})) {
            auto &m = movers_[id];
            if ((traffic_controls_[id] && traffic_controls_[id]->zone == j) ||
                id == exit.agent || occupied[id] || m.blocked || !m.moving || m.route.empty() ||
                bodies[id].overlaps(exit.area)) continue;
            const auto coast = m.body.position + m.body.velocity * (length(m.body.velocity) / motion_.force + 1);
            const auto target = m.body.position + limited(m.route.front() - m.body.position, reach);
            if (!crosses_mover(m.body.position, target, m.body.half_extents, pocket) &&
                !crosses_mover(m.body.position, coast, m.body.half_extents, pocket)) continue;
            traffic_stops_[id].push_back(stop_before(m.body, exit.area));
            if (junction.corridor) m.corridor_waiting = true;
            else m.junction_waiting = true;
            m.junction_exit_waiting = true;
        }
    }
}
void Simulation::move_agents() {
    const bool prepare_together = (motion_.junction_traffic && junction_count_ > 0) ||
                                  (motion_.corridor_traffic && corridor_count() > 0);
    if (prepare_together)
        for (auto &m : movers_) prepare_route(m);
    avoidance_limited_ = 0;
    std::vector<Body> snapshot;
    std::vector<Aabb> boxes;
    for (const auto &m : movers_) {
        snapshot.push_back(m.body);
        if (m.blocked || !m.moving) {
            snapshot.back().motion = Motion::fixed;
            snapshot.back().velocity = {};
        }
        boxes.push_back(bounds(m.body));
    }
    SpatialGrid index(std::max(blueprint_.cell_size.x, blueprint_.cell_size.y));
    index.rebuild(boxes);
    coordinate_traffic(index, boxes);
    for (std::size_t i = 0; i < movers_.size(); ++i) {
        auto &m = movers_[i];
        if (!m.product)
            ++m.idle_ticks;
        if (m.blocked || !m.moving)
            continue;
        if (!prepare_together && !prepare_route(m))
            continue;
        std::vector<Body> neighbors;
        std::vector<AvoidanceAgent> avoidance_neighbors;
        const float reach = length(m.body.half_extents) + blueprint_.mover_max_extent * 2 + 2 * motion_.speed * motion_.horizon;
        for (auto id :
             index.query({m.body.position - Vec2{reach, reach}, m.body.position + Vec2{reach, reach}}))
            if (id != i) {
                neighbors.push_back(snapshot[id]);
                if (motion_.profile == SteeringProfile::reciprocal) {
                    const auto &other = snapshot[id];
                    avoidance_neighbors.push_back({id, other.position, other.velocity, bounding_radius(other),
                                                   other.motion != Motion::fixed && !movers_[id].junction_waiting && !movers_[id].corridor_waiting, other.half_extents});
                }
            }
        std::vector<Segment> edges;
        for (const auto &edge : borders_)
            if (distance_to_segment(m.body.position, edge.start, edge.end) <=
                length(m.body.half_extents) + motion_.speed * motion_.horizon + 10)
                edges.push_back(edge);
        if (i < traffic_stops_.size())
            edges.insert(edges.end(), traffic_stops_[i].begin(), traffic_stops_[i].end());
        const auto &control = traffic_controls_[i];
        const auto target = control ? control->target : m.route.front();
        const bool arrival = control ? control->arrival : m.route.size() == 1;
        PrioritySteering settings;
        settings.max_speed = motion_.speed;
        settings.max_acceleration = motion_.force;
        settings.prediction_steps = int(std::ceil(motion_.horizon));
        settings.arrival_radius = length(m.body.half_extents * 2) * 0.45f;
        m.forces = priority_steer(m.body, target, neighbors, edges, settings, arrival);
        if (motion_.profile != SteeringProfile::original) {
            Steering recovery;
            recovery.max_speed = motion_.speed;
            recovery.max_acceleration = motion_.force;
            recovery.response_time = 1;
            recovery.slow_radius = arrival ? settings.arrival_radius : 3;
            recovery.stop_radius = 0.01f;
            recovery.lookahead = motion_.horizon;
            recovery.clearance = 2;
            recovery.rectangular_clearance = true;
            bool avoidance_limited = false;
            if (motion_.profile == SteeringProfile::reciprocal) {
                AvoidanceSettings avoidance;
                avoidance.max_speed = motion_.speed;
                avoidance.max_acceleration = motion_.force;
                avoidance.horizon = motion_.horizon;
                avoidance.clearance = recovery.clearance;
                const auto &body = snapshot[i];
                m.forces.seek = steer(body, target, {}, recovery);
                const auto preferred = body.velocity + m.forces.seek;
                // i4sim velocities/accelerations are per tick: one control step is dt=1.
                const auto result = avoid_collisions({i, body.position, body.velocity, bounding_radius(body), true, body.half_extents},
                                                      preferred, avoidance_neighbors, 1, avoidance);
                m.forces.acceleration = result.acceleration;
                m.forces.predictive = result.acceleration - m.forces.seek;
                avoidance_limited = !result.feasible;
            } else {
                m.forces.acceleration = motion_.profile == SteeringProfile::experimental_rvo
                    ? tangent_steer(m.body, target, neighbors, motion_.speed, motion_.force,
                                    motion_.horizon, arrival)
                    : steer(m.body, target, neighbors, recovery);
            }
            if (length(m.forces.collision) > 0)
                m.forces.acceleration = limited(m.forces.collision, motion_.force);
            const auto acceleration = m.forces.acceleration;
            m.forces.acceleration = constrain_borders(m.body, acceleration, edges, settings);
            m.forces.border = m.forces.acceleration - acceleration;
            if (motion_.profile == SteeringProfile::reciprocal &&
                (avoidance_limited || length(m.forces.collision) > 0 || length(m.forces.border) > 0.00001f))
                ++avoidance_limited_;
        }
        m.body.acceleration = m.forces.acceleration;
        m.body.velocity = limited(m.body.velocity + m.body.acceleration, settings.max_speed);
        const auto before = m.body.position;
        m.body.position += m.body.velocity;
        const float moved = distance(before, m.body.position);
        m.distance += moved;
        if (m.product)
            for (auto &product : products_)
                if (product.id == *m.product) {
                    product.distance += moved;
                    break;
                }
        const float remaining = route_remaining(m.body.position, m.route);
        // Arrival slows continuously; a fixed 2 mm threshold would classify
        // the final approach as stalled even while it is still converging.
        const float progress_step = std::clamp(remaining * 0.05f, 0.001f, 2.f);
        if (remaining + progress_step < m.best_remaining) {
            m.best_remaining = remaining;
            m.no_progress = 0;
        }
        m.stalled = moved < 0.1f ? m.stalled + 1 : 0;
        if (m.product)
            ++m.transport_ticks;
        if (!control && m.route.size() == 1 && distance(m.body.position, m.destination) < 0.1f &&
            length(m.body.velocity) < 0.05f &&
            grid_.clear(m.body.position, m.destination, m.body.half_extents) &&
            std::none_of(neighbors.begin(), neighbors.end(), [&](const Body &other) {
                Body parked = m.body;
                parked.position = m.destination;
                const auto contact = collide(parked, other);
                return contact && contact->penetration > 0;
            })) {
            m.body.position = m.destination;
            m.body.velocity = {};
            m.body.acceleration = {};
            m.moving = false;
            m.manual = false;
            m.route.clear();
        }
    }
    // Count contacts before correction and resolve each unordered pair once.
    std::set<std::pair<std::size_t, std::size_t>> current;
    std::vector<Body> integrated;
    for (const auto &m : movers_) integrated.push_back(m.body);
    for (auto &m : movers_) m.contacting = false;
    auto contact = [&](std::size_t a, std::size_t b) {
        movers_[a].contacting = true;
        if (b < movers_.size()) movers_[b].contacting = true;
        current.emplace(a, b);
        ++contact_ticks_;
        if (!contacts_.contains({a, b})) ++collisions_;
        ++movers_[a].collision_ticks;
    };
    for (std::size_t i = 0; i < movers_.size(); ++i) {
        for (std::size_t j = i + 1; j < movers_.size(); ++j)
            if (bounds(integrated[i]).overlaps(bounds(integrated[j])))
            if (auto c = collide(integrated[i], integrated[j]); c && c->penetration > 0) {
                contact(i, j);
                ++movers_[j].collision_ticks;
                auto &a = movers_[i].body;
                auto &b = movers_[j].body;
                if (movers_[i].blocked || !movers_[i].moving) a.motion = Motion::fixed;
                if (movers_[j].blocked || !movers_[j].moving) b.motion = Motion::fixed;
                resolve_contact(a, b, *c);
                a.motion = b.motion = Motion::controlled;
            }
        for (std::size_t j = 0; j < borders_.size(); ++j) {
            const auto &edge = borders_[j];
            if (distance_to_segment(integrated[i].position, edge.start, edge.end) >
                length(integrated[i].half_extents) + .01f) continue;
            Body wall;
            wall.shape = Shape::box;
            wall.motion = Motion::fixed;
            wall.position = (edge.start + edge.end) / 2;
            wall.half_extents = {std::max(0.001f, std::abs(edge.end.x-edge.start.x)/2),
                                 std::max(0.001f, std::abs(edge.end.y-edge.start.y)/2)};
            if (auto c = collide(integrated[i], wall); c && c->penetration > 0.0011f) {
                // The thin collision proxy extends 0.001 mm into free space;
                // touching that artificial skin is not a physical wall contact.
                c->penetration -= 0.001f;
                contact(i, movers_.size() + j);
                if (!movers_[i].blocked) resolve_contact(movers_[i].body, wall, *c);
            }
        }
    }
    contacts_ = std::move(current);
}
void Simulation::update_product(Product &p) {
    if (p.state == ProductState::waiting) {
        schedule(p);
        return;
    }
    auto &m = movers_.at(*p.mover);
    auto &producer = producers_.at(*p.producer);
    if (m.blocked)
        return;
    if (producer.blocked && p.state != ProductState::processing) {
        std::erase(producer.queue, p.id);
        p.producer.reset();
        p.state = ProductState::waiting;
        set_goal(m, m.parking);
        return;
    }
    if (p.state == ProductState::transporting || p.state == ProductState::queued) {
        const Vec2 goal = producer.definition.processing_position +
                          Vec2{blueprint_.mover_max_extent, blueprint_.mover_max_extent} / 2;
        const bool first = !producer.queue.empty() && producer.queue.front() == p.id;
        if (!first) {
            // Reserve a queue slot without allowing two movers to occupy the
            // same processor. Waiting agents stage outside processing cells.
            if (distance(m.destination, goal) < 0.05f)
                set_goal(m, holding_position(m, goal));
            return;
        }
        if (distance(m.destination, goal) > 0.05f) {
            set_goal(m, goal);
            p.state = ProductState::transporting;
        }
    }
    if (p.state == ProductState::transporting) {
        ++p.transport_ticks;
        if (!m.moving && !m.unreachable)
            p.state = ProductState::queued;
    }
    if (p.state == ProductState::queued && !producer.blocked && !producer.product &&
        !producer.queue.empty() && producer.queue.front() == p.id) {
        producer.product = p.id;
        producer.remaining = 1000;
        p.state = ProductState::processing;
    }
    if (p.state == ProductState::processing && !producer.blocked) {
        --producer.remaining;
        ++producer.processing_ticks;
        ++p.processing_ticks;
        if (producer.remaining == 0) {
            ++producer.interactions;
            ++m.interactions;
            ++p.step;
            const auto steps = recipe(p.kind);
            if (p.step < steps.size() && supports(producer.definition.model, steps[p.step])) {
                // Consecutive operations at the same station retain the
                // reservation instead of clearing and re-entering its approach.
                producer.remaining = 1000;
            } else {
                producer.product.reset();
                std::erase(producer.queue, p.id);
                p.producer.reset();
                if (p.step == steps.size()) {
                    ++completed_;
                    p.state = ProductState::completed;
                    p.finished = ticks_;
                    completed_products_.push_back(p);
                    m.product.reset();
                    set_goal(m, m.parking);
                } else {
                    p.state = ProductState::waiting;
                    schedule(p);
                    if (!p.producer)
                        set_goal(m, holding_position(m, m.destination));
                }
            }
        }
    }
}
void Simulation::tick() {
    if (paused)
        return;
    if (ticks_ >= blueprint_.tick_cap) {
        paused = true;
        return;
    }
    ++ticks_;
    if (workload && products_.size() < movers_.size())
        add_product((random_() % 2) ? ProductKind::trimmer : ProductKind::personalized);
    if (ticks_ % 10 == 1)
        update_heatmap();
    coordinate_parking();
    move_agents();
    for (auto &p : producers_)
        if (p.queue.empty())
            ++p.idle_ticks;
    for (auto &p : products_)
        update_product(p);
    std::erase_if(products_, [](const Product &p) { return p.state == ProductState::completed; });
    if (ticks_ >= blueprint_.tick_cap)
        paused = true;
}
void Simulation::dump(const std::filesystem::path &directory) const {
    using Json = nlohmann::json;
    std::filesystem::create_directories(directory);
    Json data{{"blueprint", blueprint_.name}, {"ticks", ticks_},         {"completed", completed_},
              {"contactTicks", contact_ticks_}, {"collisions", collisions_},    {"movers", Json::array()}, {"producers", Json::array()},
              {"products", Json::array()}};
    for (const auto &m : movers_)
        data["movers"].push_back({{"id", m.definition.id},
                                  {"distance", m.distance},
                                  {"idleTicks", m.idle_ticks},
                                  {"transportTicks", m.transport_ticks},
                                  {"interactions", m.interactions},
                                  {"blocked", m.blocked}});
    for (const auto &p : producers_)
        data["producers"].push_back({{"id", p.definition.id},
                                     {"interactions", p.interactions},
                                     {"processingTicks", p.processing_ticks},
                                     {"idleTicks", p.idle_ticks},
                                     {"queue", p.queue},
                                     {"blocked", p.blocked}});
    auto record = [&](const Product &p) {
        data["products"].push_back(
            {{"id", p.id},
             {"kind", p.kind == ProductKind::trimmer ? "Trimmer" : "TrimmerPersonalized"},
             {"step", p.step},
             {"state", int(p.state)},
             {"created", p.created},
             {"finished", p.finished},
             {"transportTicks", p.transport_ticks},
             {"processingTicks", p.processing_ticks},
             {"distance", p.distance}});
    };
    for (const auto &p : completed_products_)
        record(p);
    for (const auto &p : products_)
        record(p);
    std::ofstream out(directory / "summary.json");
    out << data.dump(2) << '\n';
    if (!out)
        throw std::runtime_error("Cannot write simulation summary: " + directory.string());
}
} // namespace i4sim

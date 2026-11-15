#include "simulation.h"
#include <fstream>
#include <kinetra/navigation.h>
#include <nlohmann/json.hpp>
#include <stdexcept>

namespace i4sim {
using namespace kinetra;
std::vector<Interaction> recipe(ProductKind kind) {
    using enum Interaction;
    if (kind == ProductKind::personalized)
        return {place_housing, place_trimmer, place_lever, place_card, personalize_card, remove_assy};
    return {place_housing, place_trimmer, place_lever, remove_assy};
}
Simulation::Simulation(Blueprint b, std::uint32_t seed)
    : blueprint_(std::move(b)), grid_(blueprint_.dimension, blueprint_.cell_size), random_(seed) {
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
    for (const auto &definition : blueprint_.movers) {
        Mover m;
        m.definition = definition;
        m.body.shape = Shape::box;
        m.body.motion = Motion::controlled;
        m.body.half_extents = mover_spec(definition.model).dimension / 2;
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
    if (!std::isfinite(settings.speed) || settings.speed <= 0 ||
        !std::isfinite(settings.force) || settings.force <= 0 ||
        !std::isfinite(settings.horizon) || settings.horizon <= 0 || settings.horizon > 64)
        throw std::invalid_argument("Invalid motion settings");
    motion_ = settings;
    update_heatmap();
    for (auto &m : movers_) {
        m.body.velocity = limited(m.body.velocity, settings.speed);
        if (m.moving)
            set_goal(m, m.destination);
    }
}
void Simulation::single_step() {
    if (!paused) return;
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
void Simulation::set_goal(Mover &m, Vec2 target) {
    m.destination = target;
    m.route = grid_.path(m.body.position, target);
    m.unreachable = m.route.empty();
    m.moving = !m.unreachable;
    m.stalled = m.replan = m.no_progress = 0;
    m.best_remaining = distance(m.body.position, target);
    m.detour = false;
    m.recovery_attempts = 0;
}
bool Simulation::move_to(std::size_t index, Vec2 target) {
    auto &m = movers_.at(index);
    if (m.blocked || m.product || grid_.blocked(grid_.cell(target)))
        return false;
    auto route = grid_.path(m.body.position, target);
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
        !grid_.path(m.body.position, m.parking).empty())
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
        if (!available)
            continue;
        const float score = distance(candidate, target) +
                            grid_.weight(id) * std::max(blueprint_.cell_size.x, blueprint_.cell_size.y);
        if (score < best && !grid_.path(m.body.position, candidate).empty()) {
            best = score;
            result = candidate;
        }
    }
    return result;
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
    if (!product.mover) {
        float best = std::numeric_limits<float>::infinity();
        for (std::size_t i = 0; i < movers_.size(); ++i) {
            const auto &m = movers_[i];
            if (m.blocked || m.product || m.manual)
                continue;
            const float cost = distance(m.body.position, goal);
            if (cost < best && !grid_.path(m.body.position, goal).empty()) {
                best = cost;
                product.mover = i;
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
    m.best_remaining = distance(m.body.position, m.destination);
}
void Simulation::move_agents() {
    std::vector<Body> snapshot;
    std::vector<Aabb> boxes;
    for (const auto &m : movers_) {
        snapshot.push_back(m.body);
        boxes.push_back(bounds(m.body));
    }
    SpatialGrid index(std::max(blueprint_.cell_size.x, blueprint_.cell_size.y));
    index.rebuild(boxes);
    for (std::size_t i = 0; i < movers_.size(); ++i) {
        auto &m = movers_[i];
        if (!m.product)
            ++m.idle_ticks;
        if (m.blocked || !m.moving)
            continue;
        if (!m.detour && ++m.replan >= 10 &&
            (m.stalled > 10 || grid_.weight(grid_.cell(m.body.position)) > 3)) {
            auto route = grid_.path(m.body.position, m.destination);
            if (!route.empty())
                m.route = std::move(route);
            m.replan = 0;
        }
        if (m.route.empty()) {
            set_goal(m, m.destination);
            if (m.unreachable)
                continue;
        }
        if (++m.no_progress >= 120 + std::min(10u, m.recovery_attempts) * 30 && motion_.recovery) {
            recover_route(m);
            ++m.recovery_attempts;
        }
        while (m.route.size() > 1 && distance(m.body.position, m.route.front()) < 3)
            m.route.erase(m.route.begin());
        std::vector<Body> neighbors;
        const float reach = length(m.body.half_extents) + blueprint_.mover_max_extent * 2 + 2 * motion_.speed * motion_.horizon;
        for (auto id :
             index.query({m.body.position - Vec2{reach, reach}, m.body.position + Vec2{reach, reach}}))
            if (id != i)
                neighbors.push_back(snapshot[id]);
        std::vector<Segment> edges;
        for (const auto &edge : borders_)
            if (distance_to_segment(m.body.position, edge.start, edge.end) <=
                length(m.body.half_extents) + motion_.speed * motion_.horizon + 10)
                edges.push_back(edge);
        PrioritySteering settings;
        settings.max_speed = motion_.speed;
        settings.max_acceleration = motion_.force;
        settings.prediction_steps = int(std::ceil(motion_.horizon));
        settings.arrival_radius = length(m.body.half_extents * 2) * 0.45f;
        m.forces = priority_steer(m.body, m.route.front(), neighbors, edges, settings, m.route.size() == 1);
        if (motion_.profile != SteeringProfile::original) {
            Steering recovery;
            recovery.max_speed = motion_.speed;
            recovery.max_acceleration = motion_.force;
            recovery.response_time = 1;
            recovery.slow_radius = m.route.size() == 1 ? settings.arrival_radius : 3;
            recovery.stop_radius = 0.01f;
            recovery.lookahead = motion_.horizon;
            recovery.clearance = 2;
            m.forces.acceleration = motion_.profile == SteeringProfile::experimental_rvo
                ? tangent_steer(m.body, m.route.front(), neighbors, motion_.speed, motion_.force,
                                motion_.horizon, m.route.size() == 1)
                : steer(m.body, m.route.front(), neighbors, recovery);
            if (length(m.forces.border) > 0 || length(m.forces.collision) > 0)
                m.forces.acceleration = limited(m.forces.border + m.forces.collision, motion_.force);
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
        const float remaining = distance(m.body.position, m.destination);
        if (remaining + 2 < m.best_remaining) {
            m.best_remaining = remaining;
            m.no_progress = 0;
        }
        m.stalled = moved < 0.1f ? m.stalled + 1 : 0;
        if (m.product)
            ++m.transport_ticks;
        if (m.route.size() == 1 && distance(m.body.position, m.destination) < 0.05f) {
            m.body.position = m.destination;
            m.body.velocity = {};
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
                if (movers_[i].blocked) a.motion = Motion::fixed;
                if (movers_[j].blocked) b.motion = Motion::fixed;
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
            if (auto c = collide(integrated[i], wall); c && c->penetration > 0) {
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

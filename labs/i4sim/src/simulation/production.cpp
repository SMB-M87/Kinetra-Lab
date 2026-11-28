#include "i4sim/simulation/detail/production.h"
#include "i4sim/simulation/detail/geometry.h"
#include "i4sim/simulation/detail/routing.h"
#include "i4sim/simulation/detail/simulation_constants.h"

#include <algorithm>
#include <limits>
#include <stdexcept>

namespace i4sim::detail {
using namespace kinetra;

Production::Production(const Blueprint &blueprint, std::uint32_t seed)
    : max_queue_(blueprint.producer_max_queue), random_(seed) {
    for (const auto &definition : blueprint.producers) {
        Producer p;
        p.definition = definition;
        producers_.push_back(p);
    }
    for (std::size_t i = 0; i < producers_.size(); ++i)
        for (std::size_t k = 0; k < interaction_count; ++k)
            if (supports(producers_[i].definition.model, Interaction(k)))
                supporting_[k].push_back(i);
}

std::uint64_t Production::add_product(ProductKind kind, std::size_t movers, std::uint64_t tick) {
    if (products_.size() >= std::max<std::size_t>(1, movers))
        throw std::runtime_error("Local workload capacity reached");
    const auto id = next_product_++;
    Product p;
    p.id = id;
    p.kind = kind;
    p.created = tick;
    products_.push_back(p);
    return id;
}

void Production::generate(std::size_t movers, std::uint64_t tick) {
    if (products_.size() < movers)
        add_product((random_() % 2) ? ProductKind::trimmer : ProductKind::personalized, movers, tick);
}

void Production::toggle_producer(std::size_t index) {
    auto &p = producers_.at(index);
    p.blocked = !p.blocked;
}

void Production::add_distance(std::uint64_t product, float moved) {
    for (auto &p : products_)
        if (p.id == product) {
            p.distance += moved;
            break;
        }
}

void Production::update(Goals &goals) {
    for (auto &p : producers_)
        if (p.queue.empty())
            ++p.idle_ticks;
    for (auto &p : products_)
        update_product(p, goals);
    std::erase_if(products_, [](const Product &p) { return p.state == ProductState::completed; });
}

void Production::schedule(Product &product, Goals &goals) {
    const auto &floor = goals.floor;
    auto &fleet = goals.fleet;
    if (product.mover && fleet.movers[*product.mover].goal == GoalKind::making_way)
        return;
    const auto steps = recipe(product.kind);
    if (product.step >= steps.size())
        throw std::out_of_range("Product recipe step out of range");
    const auto interaction = steps[product.step];
    if (!product.producer) {
        // The cheapest supporting producer, weighing its queue against the transport distance.
        double best = std::numeric_limits<double>::infinity();
        for (const std::size_t i : supporting_[std::size_t(interaction)]) {
            const auto &p = producers_[i];
            if (p.blocked || p.queue.size() >= max_queue_)
                continue;
            const auto goal = floor.processing_goal(p.definition.processing_position);
            if (floor.grid().blocked(floor.grid().cell(goal)))
                continue;
            const double transport =
                product.mover ? 1 + distance(fleet.movers[*product.mover].body.position, goal) : 1;
            const double cost = double(3 + 5 * p.queue.size()) * transport;
            if (cost < best) {
                best = cost;
                product.producer = i;
            }
        }
        if (!product.producer)
            return;
        producers_[*product.producer].queue.push_back(product.id);
    }
    const auto goal = floor.processing_goal(producers_[*product.producer].definition.processing_position);
    std::vector<Vec2> departure;
    if (!product.mover) {
        departure = allocate_mover(product, goal, goals);
        if (!product.mover)
            return;
        fleet.movers[*product.mover].product = product.id;
    }
    auto &m = fleet.movers[*product.mover];
    if (m.held)
        return;
    // A rescheduled product whose mover waits away from a processing place, at its parking
    // place after a blocked producer for example, leaves the way a queued one does: approach()
    // sends it once the product heads the queue, through DEPARTURE CHECK. A mover that just
    // finished at a processing place leaves at once, or it would block that producer.
    // A position off the grid (the strip a blueprint whose dimension is no multiple of its
    // cells leaves) is no processing place.
    const int cell = floor.grid().cell(m.body.position);
    if (departure.empty() && goals.motion.departure_check && (cell < 0 || !floor.processing_cell(cell))) {
        product.state = ProductState::transporting;
        // Not yet at the head, it waits at a holding place for its new producer, as a queued
        // product does.
        const auto &queue = producers_[*product.producer].queue;
        if (!queue.empty() && queue.front() != product.id) {
            const auto hold = goals.holding_position(m, goal);
            if (distance(hold, m.destination) > same_point_mm)
                goals.set(m, hold);
        }
        return;
    }
    goals.set(m, goal);
    if (!departure.empty()) {
        m.route = std::move(departure);
        fleet.progress[*product.mover].best_remaining = route_remaining(m.body.position, m.route);
    }
    product.state = ProductState::transporting;
}

std::vector<Vec2> Production::allocate_mover(Product &product, Vec2 goal, Goals &goals) const {
    // The nearest free automatic mover that can actually depart toward the goal.
    const auto &fleet = goals.fleet;
    std::vector<Vec2> departure;
    float best = std::numeric_limits<float>::infinity();
    for (std::size_t i = 0; i < fleet.movers.size(); ++i) {
        const auto &m = fleet.movers[i];
        if (m.held || m.product || m.goal != GoalKind::automatic)
            continue;
        const float cost = distance(m.body.position, goal);
        if (cost < best) {
            auto candidate = plan_route(goals.floor, fleet, goals.motion, i, goal, true);
            if (!candidate.empty()) {
                best = cost;
                product.mover = i;
                departure = std::move(candidate);
            }
        }
    }
    return departure;
}

void Production::update_product(Product &p, Goals &goals) {
    if (p.state == ProductState::waiting) {
        schedule(p, goals);
        return;
    }
    auto &m = goals.fleet.movers.at(*p.mover);
    auto &producer = producers_.at(*p.producer);
    if (m.held || m.goal == GoalKind::making_way) {
        // A held or yielding mover does not wait for its departure; its wait starts again
        // when it is back, so the limit never lets it leave unchecked at once.
        departure_waits_.erase(p.id);
        return;
    }
    if (producer.blocked && p.state != ProductState::processing) {
        departure_waits_.erase(p.id);
        std::erase(producer.queue, p.id);
        p.producer.reset();
        p.state = ProductState::waiting;
        goals.set(m, m.parking);
        return;
    }
    if ((p.state == ProductState::transporting || p.state == ProductState::queued) &&
        !approach(p, m, producer, goals))
        return;
    if (p.state == ProductState::transporting) {
        ++p.transport_ticks;
        if (m.lifecycle == MoverLifecycle::idle)
            p.state = ProductState::queued;
    }
    if (p.state == ProductState::queued && !producer.blocked && !producer.product &&
        !producer.queue.empty() && producer.queue.front() == p.id) {
        producer.product = p.id;
        producer.remaining = processing_ticks;
        p.state = ProductState::processing;
    }
    if (p.state == ProductState::processing && !producer.blocked)
        process(p, m, producer, goals);
}

bool Production::approach(Product &p, Mover &m, Producer &producer, Goals &goals) {
    const Vec2 goal = goals.floor.processing_goal(producer.definition.processing_position);
    const bool first = !producer.queue.empty() && producer.queue.front() == p.id;
    if (!first) {
        // Reserve a queue slot without allowing two movers to occupy the
        // same processor. Waiting agents stage outside processing cells.
        if (distance(m.destination, goal) < same_point_mm)
            goals.set(m, goals.holding_position(m, goal));
        return false;
    }
    if (distance(m.destination, goal) > same_point_mm) {
        auto route = departure_route(p, m, goal, goals);
        if (!route)
            return false;
        goals.set(m, goal);
        if (!route->empty()) {
            m.route = std::move(*route);
            goals.fleet.progress[goals.fleet.index(m)].best_remaining =
                route_remaining(m.body.position, m.route);
        }
        p.state = ProductState::transporting;
    }
    return true;
}

std::optional<std::vector<Vec2>> Production::departure_route(const Product &p, const Mover &m, Vec2 goal,
                                                             const Goals &goals) {
    if (!goals.motion.departure_check || m.lifecycle != MoverLifecycle::idle) {
        // A wait that the operator ended by turning the check off does not count later. A mover
        // still on its way to its holding place turns toward the producer as before: held back
        // in the aisle it would drive on into its parking row and could seal itself in.
        departure_waits_.erase(p.id);
        return std::vector<Vec2>{};
    }
    // A queued mover waits at its holding place, often a parking row, where parked
    // neighbors can stand on the plain grid route. Like a newly allocated mover it leaves
    // only along a route refined around stationary footprints.
    auto route = plan_route(goals.floor, goals.fleet, goals.motion, goals.fleet.index(m), goal, true);
    const auto since = departure_waits_.try_emplace(p.id, goals.tick).first->second;
    if (route.empty() && goals.tick - since < departure_wait_ticks)
        return std::nullopt;
    departure_waits_.erase(p.id);
    return route;
}

void Production::process(Product &p, Mover &m, Producer &producer, Goals &goals) {
    --producer.remaining;
    ++producer.processing_ticks;
    ++p.processing_ticks;
    if (producer.remaining != 0)
        return;
    ++producer.interactions;
    ++m.interactions;
    ++p.step;
    const auto steps = recipe(p.kind);
    if (p.step < steps.size() && supports(producer.definition.model, steps[p.step])) {
        // Consecutive operations at the same station retain the
        // reservation instead of clearing and re-entering its approach.
        producer.remaining = processing_ticks;
        return;
    }
    producer.product.reset();
    std::erase(producer.queue, p.id);
    p.producer.reset();
    if (p.step == steps.size()) {
        ++completed_;
        p.state = ProductState::completed;
        p.finished = goals.tick;
        completed_products_.push_back(p);
        m.product.reset();
        goals.set(m, m.parking);
    } else {
        p.state = ProductState::waiting;
        schedule(p, goals);
        if (!p.producer)
            goals.set(m, goals.holding_position(m, m.destination));
    }
}
} // namespace i4sim::detail

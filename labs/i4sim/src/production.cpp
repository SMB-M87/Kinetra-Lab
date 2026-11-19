#include "production.h"
#include "geometry.h"
#include "routing.h"
#include "simulation_constants.h"

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
    const auto interaction = recipe(product.kind).at(product.step);
    if (!product.producer) {
        // The cheapest supporting producer, weighing its queue against the transport distance.
        double best = std::numeric_limits<double>::infinity();
        for (std::size_t i = 0; i < producers_.size(); ++i) {
            const auto &p = producers_[i];
            if (p.blocked || p.queue.size() >= max_queue_ || !supports(p.definition.model, interaction))
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
            auto candidate = plan_route(goals.floor, fleet, i, goal, true);
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
    if (m.held)
        return;
    if (producer.blocked && p.state != ProductState::processing) {
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
        goals.set(m, goal);
        p.state = ProductState::transporting;
    }
    return true;
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

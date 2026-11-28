#pragma once
#include "i4sim/blueprint/blueprint.h"
#include "i4sim/simulation/detail/goals.h"
#include "i4sim/simulation/simulation.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <random>
#include <vector>

namespace i4sim::detail {
/// The factory's production: producers with their queues, open and completed products, and
/// the random workload. A product moves through `ProductState`: it waits for a producer
/// and a mover, is transported, queues, is processed one recipe step after another, and
/// completes when its recipe is done.
///
/// Products keep their mover until they complete. A producer queue holds at most the
/// blueprint's `producer_max_queue` products; only its head occupies the processing goal.
class Production {
  public:
    /// One producer per blueprint producer; `seed` seeds the workload generator.
    Production(const Blueprint &blueprint, std::uint32_t seed);

    const std::vector<Producer> &producers() const { return producers_; }
    const std::vector<Product> &products() const { return products_; }
    const std::vector<Product> &completed_products() const { return completed_products_; }
    std::uint64_t completed() const { return completed_; }

    /// Opens a product of `kind` at `tick` and returns its id. Throws std::runtime_error
    /// when as many products are open as there are movers (at least one).
    std::uint64_t add_product(ProductKind kind, std::size_t movers, std::uint64_t tick);
    /// Opens one random product while fewer products than `movers` are open.
    void generate(std::size_t movers, std::uint64_t tick);
    /// Blocks or unblocks a producer. Throws std::out_of_range for an unknown producer.
    void toggle_producer(std::size_t index);
    /// Adds `moved` millimeters to the transport distance of open product `product`.
    void add_distance(std::uint64_t product, float moved);
    /// One production tick: counts idle producers, advances every open product, and
    /// removes the completed ones.
    void update(Goals &goals);

  private:
    /// Every interaction of `Interaction`, which ends with `remove_assy`.
    static constexpr std::size_t interaction_count = std::size_t(Interaction::remove_assy) + 1;
    std::size_t max_queue_;
    std::vector<Producer> producers_;
    /// Per interaction, the producers whose model supports it, ascending. A producer's model
    /// never changes, so schedule() visits only these, in the order the loop over every
    /// producer would have chosen among them.
    std::array<std::vector<std::size_t>, interaction_count> supporting_;
    std::vector<Product> products_, completed_products_;
    std::mt19937 random_;
    std::uint64_t completed_{}, next_product_{1};
    /// DEPARTURE CHECK: tick since which the head product's mover, by product id, has
    /// waited for a checked departure route.
    std::map<std::uint64_t, std::uint64_t> departure_waits_;

    void schedule(Product &product, Goals &goals);
    std::vector<Vec2> allocate_mover(Product &product, Vec2 goal, Goals &goals) const;
    void update_product(Product &product, Goals &goals);
    /// Sends the mover of a queued product, or of one rescheduled away from a processing
    /// place, to the processing goal once the product heads the queue, or keeps it at its
    /// holding place; false while the product does not head it or DEPARTURE CHECK holds the
    /// mover back.
    bool approach(Product &product, Mover &mover, Producer &producer, Goals &goals);
    /// The route a product's mover takes to `goal` when the product heads the queue: nothing
    /// while DEPARTURE CHECK keeps it waiting, a route checked against stationary movers when
    /// one exists, and an empty route, which the caller plans as usual, without the setting,
    /// after the wait, or for a mover that is not idle (still driving, or unreachable). Only
    /// an idle mover's wait is kept; any other drops it.
    std::optional<std::vector<Vec2>> departure_route(const Product &product, const Mover &mover, Vec2 goal,
                                                     const Goals &goals);
    void process(Product &product, Mover &mover, Producer &producer, Goals &goals);
};
} // namespace i4sim::detail

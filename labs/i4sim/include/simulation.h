#pragma once
#include "blueprint.h"
#include <deque>
#include <kinetra/steering.h>
#include <kinetra/timing.h>
#include <kinetra/traffic.h>
#include <kinetra/weighted_grid.h>
#include <optional>
#include <random>
#include <set>

namespace i4sim {
enum class SteeringProfile { improved, original, experimental_rvo, reciprocal };
struct MotionSettings {
    SteeringProfile profile{SteeringProfile::improved};
    bool heatmap{true}, recovery{}, parking_traffic{}, junction_traffic{}, corridor_traffic{};
    float speed{2}, force{0.6f}, horizon{32};
};
enum class ProductKind { trimmer, personalized };
enum class ProductState { waiting, transporting, queued, processing, completed };
std::vector<Interaction> recipe(ProductKind kind);
struct Mover {
    MoverDefinition definition;
    kinetra::Body body;
    Vec2 parking, destination;
    bool blocked{}, moving{}, unreachable{};
    std::optional<std::uint64_t> product;
    std::vector<Vec2> route;
    kinetra::SteeringForces forces;
    std::uint64_t idle_ticks{}, transport_ticks{}, interactions{}, collision_ticks{};
    double distance{};
    unsigned stalled{}, replan{}, no_progress{};
    float best_remaining{};
    unsigned recovery_attempts{};
    bool detour{}, manual{}, contacting{}, parking_deferred{}, junction_waiting{}, junction_exit_waiting{}, corridor_waiting{}, corridor_retreating{};
};
struct Producer {
    ProducerDefinition definition;
    bool blocked{};
    std::deque<std::uint64_t> queue;
    std::optional<std::uint64_t> product;
    unsigned remaining{};
    std::uint64_t interactions{}, processing_ticks{}, idle_ticks{};
};
struct Product {
    std::uint64_t id;
    ProductKind kind;
    ProductState state{ProductState::waiting};
    std::size_t step{};
    std::optional<std::size_t> mover, producer;
    std::uint64_t created{}, finished{}, transport_ticks{}, processing_ticks{};
    double distance{};
};
/// Deterministic local replacement for the original actor/message orchestration.
/// Motion retains millimeters/tick (2 max speed, 0.6 max force); elapsed seconds
/// select how many ticks run and never change the per-tick physical constants.
class Simulation {
  public:
    explicit Simulation(Blueprint blueprint, std::uint32_t seed = 42);
    const Blueprint &blueprint() const { return blueprint_; }
    const std::vector<Mover> &movers() const { return movers_; }
    const std::vector<Producer> &producers() const { return producers_; }
    const std::vector<Product> &products() const { return products_; }
    const kinetra::WeightedGrid &grid() const { return grid_; }
    std::span<const kinetra::Segment> borders() const { return borders_; }
    std::uint64_t ticks() const { return ticks_; }
    std::uint64_t completed() const { return completed_; }
    std::uint64_t collisions() const { return collisions_; }
    /// Movers with infeasible reciprocal constraints or overridden avoidance in the last movement tick.
    std::size_t avoidance_limited() const { return avoidance_limited_; }
    /// Automatic parking returns temporarily staged outside active departure routes.
    std::size_t parking_waiting() const;
    std::size_t junction_count() const { return junction_count_; }
    Vec2 junction_center(std::size_t index) const;
    std::size_t junction_waiting() const;
    std::size_t junction_exit_waiting() const;
    std::optional<kinetra::Aabb> junction_exit_reservation(std::size_t index) const;
    std::size_t corridor_count() const { return traffic_zones_.size() - junction_count_; }
    std::size_t corridor_waiting() const;
    std::size_t corridor_recovering() const;
    std::optional<Vec2> corridor_retreat_target(std::size_t mover) const;
    kinetra::Aabb corridor_bounds(std::size_t index) const;
    std::optional<kinetra::Aabb> corridor_exit_reservation(std::size_t index) const;
    const MotionSettings &motion_settings() const { return motion_; }
    void configure_motion(MotionSettings settings);
    std::uint64_t contact_ticks() const { return contact_ticks_; }
    void single_step();
    bool paused{true}, workload{true};
    void advance(double elapsed);
    void tick();
    void updates_per_second(unsigned rate);
    unsigned updates_per_second() const { return ups_; }
    std::uint64_t add_product(ProductKind kind);
    void toggle_mover(std::size_t index);
    void toggle_producer(std::size_t index);
    /// Manual goals only apply to unallocated movers and free destination cells.
    bool move_to(std::size_t index, Vec2 destination);
    void dump(const std::filesystem::path &directory) const;

  private:
    struct ExitReservation {
        std::uint64_t agent;
        kinetra::Aabb area;
    };
    struct CorridorRecovery {
        std::size_t yielding, passing;
        Vec2 refuge, hold;
        std::vector<Vec2> route;
        bool yielded{};
    };
    struct TrafficControl {
        std::size_t zone;
        Vec2 target;
        bool arrival;
    };
    struct TrafficZone {
        Vec2 center, half_extents;
        bool corridor{}, vertical{};
        kinetra::TrafficGate gate;
        std::optional<ExitReservation> exit;
        std::optional<CorridorRecovery> recovery;
        std::uint64_t retry_at{};
    };
    std::vector<TrafficZone> traffic_zones_;
    std::size_t junction_count_{};
    std::vector<std::vector<kinetra::Segment>> traffic_stops_;
    std::vector<std::optional<TrafficControl>> traffic_controls_;
    MotionSettings motion_;
    std::size_t avoidance_limited_{};
    std::set<std::pair<std::size_t, std::size_t>> contacts_;
    std::uint64_t contact_ticks_{};
    Blueprint blueprint_;
    kinetra::WeightedGrid grid_;
    kinetra::SpatialGrid traffic_index_;
    std::vector<kinetra::Segment> borders_;
    std::vector<Mover> movers_;
    Vec2 mover_max_half_{};
    std::vector<Producer> producers_;
    std::vector<Product> products_, completed_products_;
    std::mt19937 random_;
    kinetra::FixedClock clock_{1.0 / 200, 256};
    unsigned ups_{200};
    std::uint64_t ticks_{}, completed_{}, collisions_{}, next_product_{1};
    std::vector<Vec2> plan_route(const Mover &mover, Vec2 destination, bool require_departure = false) const;
    void set_goal(Mover &mover, Vec2 destination);
    Vec2 holding_position(const Mover &mover, Vec2 destination) const;
    bool parking_conflict(const Mover &mover, Vec2 position) const;
    void coordinate_parking();
    bool prepare_route(Mover &mover);
    std::optional<kinetra::Aabb> traffic_exit(const Mover &mover, const TrafficZone &junction) const;
    void cancel_corridor_recovery(Mover &mover);
    void recover_corridors(const kinetra::SpatialGrid &index);
    void coordinate_traffic(const kinetra::SpatialGrid &index, std::span<const kinetra::Aabb> bodies);
    void update_heatmap();
    void schedule(Product &product);
    void recover_route(Mover &mover);
    void move_agents();
    void update_product(Product &product);
};
} // namespace i4sim

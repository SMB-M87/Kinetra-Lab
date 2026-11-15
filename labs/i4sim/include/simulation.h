#pragma once
#include "blueprint.h"
#include <deque>
#include <kinetra/steering.h>
#include <kinetra/timing.h>
#include <kinetra/weighted_grid.h>
#include <optional>
#include <random>
#include <set>

namespace i4sim {
enum class SteeringProfile { improved, original, experimental_rvo };
struct MotionSettings {
    SteeringProfile profile{SteeringProfile::improved};
    bool heatmap{true}, recovery{};
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
    bool detour{}, manual{}, contacting{};
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
    MotionSettings motion_;
    std::set<std::pair<std::size_t, std::size_t>> contacts_;
    std::uint64_t contact_ticks_{};
    Blueprint blueprint_;
    kinetra::WeightedGrid grid_;
    std::vector<kinetra::Segment> borders_;
    std::vector<Mover> movers_;
    std::vector<Producer> producers_;
    std::vector<Product> products_, completed_products_;
    std::mt19937 random_;
    kinetra::FixedClock clock_{1.0 / 200, 256};
    unsigned ups_{200};
    std::uint64_t ticks_{}, completed_{}, collisions_{}, next_product_{1};
    void set_goal(Mover &mover, Vec2 destination);
    Vec2 holding_position(const Mover &mover, Vec2 destination) const;
    void update_heatmap();
    void schedule(Product &product);
    void recover_route(Mover &mover);
    void move_agents();
    void update_product(Product &product);
};
} // namespace i4sim

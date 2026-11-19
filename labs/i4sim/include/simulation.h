#pragma once
#include "blueprint.h"
#include "events.h"
#include <array>
#include <deque>
#include <kinetra/steering.h>
#include <kinetra/timing.h>
#include <kinetra/traffic.h>
#include <kinetra/weighted_grid.h>
#include <optional>
#include <random>
#include <set>
#include <span>
#include <vector>

namespace i4sim {
enum class SteeringProfile { improved, original, experimental_rvo, reciprocal };
struct MotionSettings {
    SteeringProfile profile{SteeringProfile::improved};
    bool heatmap{true}, recovery{}, parking_traffic{}, junction_traffic{}, corridor_traffic{};
    bool predictive_passing{};
    float speed{2}, force{0.6f}, horizon{32};
};
/// Whether a simulation advances. Every simulation starts `paused`.
enum class RunState {
    /// Stopped by the operator; `Simulation::run` resumes it.
    paused,
    /// `tick` and `advance` move the simulation forward.
    running,
    /// The blueprint's tick cap was reached; no further tick runs.
    finished,
};
enum class ProductKind { trimmer, personalized };
enum class ProductState { waiting, transporting, queued, processing, completed };
std::vector<Interaction> recipe(ProductKind kind);
/// Where a mover is in reaching its goal.
enum class MoverLifecycle {
    /// No active goal: the mover arrived, or never had a goal.
    idle,
    /// The mover follows a route to `Mover::destination`.
    moving,
    /// The last goal had no route; the mover stays until it gets a new goal.
    unreachable,
};
/// Who chose a mover's current goal.
enum class GoalKind {
    /// The factory: production, parking, or none yet.
    automatic,
    /// An operator command (`Simulation::move_to`). The factory does not allocate the mover
    /// until it arrives; a new goal on the way keeps it manual.
    manual,
    /// Parking traffic: the mover waits at a staging place because its own parking place
    /// lies on another mover's route. Survives arrival; any new goal replaces it.
    parking_staging,
};
/// What traffic coordination decided for a mover in the last tick. Several fields can be
/// true at once; all are false again at the start of the next coordination.
struct Coordination {
    /// Stopped before a junction that admitted another mover.
    bool junction_waiting{};
    /// Waiting, before a junction or corridor, because its exit space is not free.
    bool exit_waiting{};
    /// Stopped before a corridor, or held by corridor recovery.
    bool corridor_waiting{};
    /// Backing out of a corridor to a refuge during corridor recovery.
    bool corridor_retreating{};
    /// Following the temporary targets of a predictive passing plan.
    bool passing{};
};
/// One transport mover: its definition, physical state, goal and statistics. Positions are
/// in millimeters, velocities in millimeters per tick. Only Simulation changes a mover.
struct Mover {
    MoverDefinition definition;
    kinetra::Body body;
    /// Center at the start of the last tick, in millimeters; drawing blends it with
    /// `body.position` by Simulation::interpolation(). Never read by the simulation.
    Vec2 previous_position;
    /// Center of the mover's own parking place, from the blueprint, and of its current goal.
    Vec2 parking, destination;
    MoverLifecycle lifecycle{MoverLifecycle::idle};
    GoalKind goal{GoalKind::automatic};
    /// Held by the operator (`Simulation::toggle_mover`): the mover stands still and keeps
    /// its goal and product.
    bool held{};
    /// Product the mover carries, by id.
    std::optional<std::uint64_t> product;
    /// Remaining waypoints to `destination`; the last one is the destination.
    std::vector<Vec2> route;
    /// Steering components of the last tick, for debug drawing.
    kinetra::SteeringForces forces;
    std::uint64_t idle_ticks{}, transport_ticks{}, interactions{}, collision_ticks{};
    /// Distance travelled, in millimeters.
    double distance{};
    /// The route came from route recovery (Navigation around blocked cells and movers).
    bool detour{};
    Coordination coordination;
    /// Touched another mover or a border in the last tick.
    bool contacting{};
    bool moving() const { return lifecycle == MoverLifecycle::moving; }
};
/// Summary of a mover's state for the HUD, in the order Simulation reports it.
enum class MoverStatus { held, corridor_retreat, passing_detour, unreachable, allocated, available };
/// The first status that applies to `mover`: held, then corridor retreat, passing detour,
/// unreachable, allocated to a product, and otherwise available for a manual goal.
MoverStatus mover_status(const Mover &mover);
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
    std::size_t predictive_pairs() const { return passing_plans_.size(); }
    std::optional<Vec2> passing_target(std::size_t mover) const;
    /// True when `mover` has made no route progress for 120 ticks while no
    /// corridor or passing coordination holds it back: the HUD's STALLED. Throws
    /// std::out_of_range for an unknown mover.
    bool stalled(std::size_t mover) const;
    const MotionSettings &motion_settings() const { return motion_; }
    /// Replaces the motion settings, in any run state. Clears the coordination state of the
    /// features it turns off and sets every moving mover's goal again, which ends passing
    /// plans and corridor recoveries; bodies do not move. Throws std::invalid_argument for
    /// an unknown profile or a speed, force or horizon that is not finite and positive
    /// (horizon at most 64 ticks); the settings are then unchanged.
    void configure_motion(MotionSettings settings);
    std::uint64_t contact_ticks() const { return contact_ticks_; }
    /// Seed of the workload generator. With the blueprint, the motion settings and the
    /// commands given, it reproduces a run tick for tick.
    std::uint32_t seed() const { return seed_; }
    /// Turns the event log on or off; it is off for a new simulation. Recording only
    /// observes: it never changes a decision. Turning it off keeps the events so far,
    /// and copying the simulation copies its log.
    void record_events(bool enabled) { recording_ = enabled; }
    bool recording_events() const { return recording_; }
    /// Passing and contact events recorded while recording was on, oldest first. The
    /// log grows without bound while recording is on. The span is valid until the next
    /// call that can record (a tick, a goal, a motion change); copy what must outlive it.
    std::span<const SimulationEvent> events() const { return events_; }
    /// Runs one tick when `paused` and stays `paused`, or ends `finished` at the tick cap.
    /// Does nothing in any other state.
    void single_step();
    /// Fraction of the next tick that advance() has accumulated, in [0, 1): how far drawing
    /// should blend from each mover's `previous_position` to its `body.position`. One unless
    /// `running`, so a paused, finished or single-stepped scene shows its exact state.
    double interpolation() const { return running() ? clock_.interpolation() : 1.0; }
    RunState state() const { return state_; }
    bool running() const { return state_ == RunState::running; }
    /// Moves a `paused` simulation to `running`; a `finished` one stays finished.
    void run();
    /// Moves a `running` simulation to `paused`; a `finished` one stays finished.
    void pause();
    /// Whether `tick` adds a random product while fewer products than movers are open.
    bool workload() const { return workload_; }
    void set_workload(bool enabled) { workload_ = enabled; }
    /// Runs as many ticks as `elapsed` seconds hold at the current update rate, while
    /// `running`; the remainder carries over. Throws std::invalid_argument for a negative or
    /// non-finite `elapsed`.
    void advance(double elapsed);
    /// Runs one tick while `running`, and does nothing otherwise. Reaching the blueprint's
    /// tick cap moves the simulation to `finished`.
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
    /// Route-following counters of one mover, kept beside `movers_` in the same order.
    struct RouteProgress {
        /// Ticks since the last replanning attempt.
        unsigned replan{};
        /// Ticks since the remaining route last shrank by a progress step.
        unsigned no_progress{};
        /// Shortest remaining route length seen since the route changed, in millimeters.
        float best_remaining{};
        /// Route recoveries since the last goal; lengthens the wait before the next one.
        unsigned recovery_attempts{};
    };
    std::vector<RouteProgress> progress_;
    RouteProgress &progress(const Mover &mover) { return progress_[std::size_t(&mover - movers_.data())]; }
    const RouteProgress &progress(const Mover &mover) const {
        return progress_[std::size_t(&mover - movers_.data())];
    }
    RunState state_{RunState::paused};
    bool workload_{true};
    struct PassingPlan {
        std::uint64_t id{};
        std::array<std::size_t, 2> movers;
        std::array<Vec2, 2> entry, exit;
        std::vector<kinetra::Aabb> space;
        bool crossing{};
        float best_remaining{};
        std::uint64_t progress_at{};
    };
    std::vector<PassingPlan> passing_plans_;
    std::vector<std::uint64_t> passing_retry_;
    // Advances whether or not recording is on, so plan ids never depend on it.
    std::uint64_t next_plan_{1};
    bool recording_{};
    std::vector<SimulationEvent> events_;
    void record_passing(EventKind kind, const PassingPlan &plan, std::optional<PassingEnd> end = {});
    void end_passing(std::size_t index, PassingEnd end);
    bool predictive_enabled() const;
    void restore_passing_routes(std::array<std::size_t, 2> movers);
    void cancel_predictive_passing(Mover &mover);
    void coordinate_predictive_passing(const kinetra::SpatialGrid &index);
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
    std::uint32_t seed_;
    /// Set while paused; the first running advance() then aligns previous positions with the
    /// current ones, so drawing does not step back behind the state shown while paused.
    bool resume_pending_{true};
    void settle_previous_positions();
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

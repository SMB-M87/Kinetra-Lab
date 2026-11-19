#pragma once
#include "blueprint.h"
#include "events.h"
#include <cstdint>
#include <deque>
#include <filesystem>
#include <kinetra/steering.h>
#include <kinetra/weighted_grid.h>
#include <memory>
#include <optional>
#include <span>
#include <vector>

namespace i4sim {
namespace detail {
class SimulationCore;
}
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
    const Blueprint &blueprint() const;
    const std::vector<Mover> &movers() const;
    const std::vector<Producer> &producers() const;
    const std::vector<Product> &products() const;
    const kinetra::WeightedGrid &grid() const;
    std::span<const kinetra::Segment> borders() const;
    std::uint64_t ticks() const;
    std::uint64_t completed() const;
    std::uint64_t collisions() const;
    /// Movers with infeasible reciprocal constraints or overridden avoidance in the last movement tick.
    std::size_t avoidance_limited() const;
    /// Automatic parking returns temporarily staged outside active departure routes.
    std::size_t parking_waiting() const;
    std::size_t junction_count() const;
    Vec2 junction_center(std::size_t index) const;
    std::size_t junction_waiting() const;
    std::size_t junction_exit_waiting() const;
    std::optional<kinetra::Aabb> junction_exit_reservation(std::size_t index) const;
    std::size_t corridor_count() const;
    std::size_t corridor_waiting() const;
    std::size_t corridor_recovering() const;
    std::optional<Vec2> corridor_retreat_target(std::size_t mover) const;
    kinetra::Aabb corridor_bounds(std::size_t index) const;
    std::optional<kinetra::Aabb> corridor_exit_reservation(std::size_t index) const;
    std::size_t predictive_pairs() const;
    std::optional<Vec2> passing_target(std::size_t mover) const;
    /// True when `mover` has made no route progress for 120 ticks while no
    /// corridor or passing coordination holds it back: the HUD's STALLED. Throws
    /// std::out_of_range for an unknown mover.
    bool stalled(std::size_t mover) const;
    const MotionSettings &motion_settings() const;
    /// Replaces the motion settings, in any run state. Clears the coordination state of the
    /// features it turns off and sets every moving mover's goal again, which ends passing
    /// plans and corridor recoveries; bodies do not move. Throws std::invalid_argument for
    /// an unknown profile or a speed, force or horizon that is not finite and positive
    /// (horizon at most 64 ticks); the settings are then unchanged.
    void configure_motion(MotionSettings settings);
    std::uint64_t contact_ticks() const;
    /// Seed of the workload generator. With the blueprint, the motion settings and the
    /// commands given, it reproduces a run tick for tick.
    std::uint32_t seed() const;
    /// Turns the event log on or off; it is off for a new simulation. Recording only
    /// observes: it never changes a decision. Turning it off keeps the events so far,
    /// and copying the simulation copies its log.
    void record_events(bool enabled);
    bool recording_events() const;
    /// Passing and contact events recorded while recording was on, oldest first. The
    /// log grows without bound while recording is on. The span is valid until the next
    /// call that can record (a tick, a goal, a motion change); copy what must outlive it.
    std::span<const SimulationEvent> events() const;
    /// Runs one tick when `paused` and stays `paused`, or ends `finished` at the tick cap.
    /// Does nothing in any other state.
    void single_step();
    /// Fraction of the next tick that advance() has accumulated, in [0, 1): how far drawing
    /// should blend from each mover's `previous_position` to its `body.position`. One unless
    /// `running`, so a paused, finished or single-stepped scene shows its exact state.
    double interpolation() const;
    RunState state() const;
    bool running() const;
    /// Moves a `paused` simulation to `running`; a `finished` one stays finished.
    void run();
    /// Moves a `running` simulation to `paused`; a `finished` one stays finished.
    void pause();
    /// Whether `tick` adds a random product while fewer products than movers are open.
    bool workload() const;
    void set_workload(bool enabled);
    /// Runs as many ticks as `elapsed` seconds hold at the current update rate, while
    /// `running`; the remainder carries over. Throws std::invalid_argument for a negative or
    /// non-finite `elapsed`.
    void advance(double elapsed);
    /// Runs one tick while `running`, and does nothing otherwise. Reaching the blueprint's
    /// tick cap moves the simulation to `finished`.
    void tick();
    void updates_per_second(unsigned rate);
    unsigned updates_per_second() const;
    std::uint64_t add_product(ProductKind kind);
    void toggle_mover(std::size_t index);
    void toggle_producer(std::size_t index);
    /// Manual goals only apply to unallocated movers and free destination cells.
    bool move_to(std::size_t index, Vec2 destination);
    void dump(const std::filesystem::path &directory) const;

    /// Copies the whole simulation state, including the event log; the copy runs on
    /// independently. A moved-from simulation may only be assigned to or destroyed.
    Simulation(const Simulation &other);
    Simulation &operator=(const Simulation &other);
    Simulation(Simulation &&other) noexcept;
    Simulation &operator=(Simulation &&other) noexcept;
    ~Simulation();

  private:
    // Copies are deep: tests copy a running simulation to branch off a replay.
    std::unique_ptr<detail::SimulationCore> core_;
};
} // namespace i4sim

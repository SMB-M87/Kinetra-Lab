#pragma once
// The headless i4sim factory simulation: movers carry products between producers on a
// grid factory floor, under one of four steering profiles and optional traffic policies.
// See docs/factory-simulation-architecture.md for the policies and labs/i4sim/README.md
// for the structure behind this facade.
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
/// How movers steer around each other and the walls.
enum class SteeringProfile {
    /// Priority steering followed by Kinetra's seek-and-avoid steering; the default.
    improved,
    /// Priority steering alone, as in the original factory.
    original,
    /// Tangent steering around predicted conflicts; experimental.
    experimental_rvo,
    /// Kinetra's reciprocal avoidance with rectangular envelopes.
    reciprocal,
};
/// Whether `profile` can run predictive passing. The other profiles ignore
/// `MotionSettings::predictive_passing`.
constexpr bool supports_predictive_passing(SteeringProfile profile) {
    return profile == SteeringProfile::improved || profile == SteeringProfile::reciprocal;
}
/// Longest prediction horizon `Simulation::configure_motion` accepts, in ticks.
inline constexpr float longest_motion_horizon = 64;
/// Motion and traffic settings of a simulation; see `Simulation::configure_motion`.
struct MotionSettings {
    /// How movers steer; Improved by default.
    SteeringProfile profile{SteeringProfile::improved};
    /// Movers follow an A* route over the grid cells. Off, each mover steers straight at
    /// its goal and only local steering and the borders keep it on the floor; a goal still
    /// needs a grid route to be accepted. Suits open floors: steering alone cannot leave a
    /// concave pocket.
    bool path_planning{true};
    /// Route costs grow where movers stand, so A* routes around crowds.
    bool heatmap{true};
    /// Movers that make no progress get a Navigation route around blocked cells and movers.
    bool recovery{};
    /// Movers returning to park wait at a staging place while their parking place lies on
    /// another mover's route.
    bool parking_traffic{};
    /// Junctions admit one mover at a time.
    bool junction_traffic{};
    /// Single-cell corridors admit one direction at a time and recover head-on standstills.
    bool corridor_traffic{};
    /// Oncoming movers on the open floor agree on side lanes (Improved and Reciprocal only).
    bool predictive_passing{};
    /// Maximum speed in millimeters per tick, maximum acceleration in millimeters per tick
    /// squared, and prediction horizon in ticks.
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
/// Ticks shuttling movers stand at their ends before they turn together: two seconds at
/// the default 200 ticks per second. See `Simulation::shuttle`.
inline constexpr std::uint64_t shuttle_pause_ticks = 400;
/// The two products of the original factory.
enum class ProductKind { trimmer, personalized };
/// Where a product is in its recipe.
enum class ProductState {
    /// Needs a producer for its next step, or a mover.
    waiting,
    /// On its mover, on the way to the producer.
    transporting,
    /// At the producer, waiting to become the head of its queue.
    queued,
    /// Being processed.
    processing,
    /// Recipe done; the product leaves the open products at the end of the tick.
    completed,
};
/// The interactions that make a product of `kind`, in order.
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
    /// The blueprint entry: id, model and start corner.
    MoverDefinition definition;
    /// The physical box: center, half extents, velocity and acceleration.
    kinetra::Body body;
    /// Center at the start of the last tick, in millimeters; drawing blends it with
    /// `body.position` by Simulation::interpolation(). Never read by the simulation.
    Vec2 previous_position;
    /// Center of the mover's own parking place, from the blueprint, and of its current goal.
    Vec2 parking, destination;
    /// Where the mover is in reaching `destination`.
    MoverLifecycle lifecycle{MoverLifecycle::idle};
    /// Who chose `destination`.
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
    /// Ticks without a product, ticks with one, producer interactions served, and ticks
    /// spent touching another mover or a border.
    std::uint64_t idle_ticks{}, transport_ticks{}, interactions{}, collision_ticks{};
    /// Distance travelled, in millimeters.
    double distance{};
    /// The route came from route recovery (Navigation around blocked cells and movers).
    bool detour{};
    Coordination coordination;
    /// Touched another mover or a border in the last tick.
    bool contacting{};
    /// True while the mover follows a route.
    bool moving() const { return lifecycle == MoverLifecycle::moving; }
};
/// Summary of a mover's state for the HUD, in the order Simulation reports it.
enum class MoverStatus { held, corridor_retreat, passing_detour, unreachable, allocated, available };
/// The first status that applies to `mover`: held, then corridor retreat, passing detour,
/// unreachable, allocated to a product, and otherwise available for a manual goal.
MoverStatus mover_status(const Mover &mover);
/// One producer (robot or manual station) and its queue.
struct Producer {
    /// The blueprint entry: id, model, position and processing corner.
    ProducerDefinition definition;
    /// Blocked by the operator (`Simulation::toggle_producer`): it accepts no product and
    /// pauses its processing.
    bool blocked{};
    /// Products assigned to this producer, by id; the head is processed next.
    std::deque<std::uint64_t> queue;
    /// Product being processed, by id.
    std::optional<std::uint64_t> product;
    /// Ticks left of the current interaction.
    unsigned remaining{};
    /// Interactions done, ticks spent processing, and ticks with an empty queue.
    std::uint64_t interactions{}, processing_ticks{}, idle_ticks{};
};
/// One product and its progress through its recipe.
struct Product {
    /// Unique per simulation, from 1.
    std::uint64_t id{};
    ProductKind kind{};
    ProductState state{ProductState::waiting};
    /// Index of the next recipe interaction.
    std::size_t step{};
    /// Indices of the mover carrying it and of its current producer.
    std::optional<std::size_t> mover, producer;
    /// Tick it was opened and tick it completed, and ticks spent in transport and processing.
    std::uint64_t created{}, finished{}, transport_ticks{}, processing_ticks{};
    /// Distance its mover travelled while carrying it, in millimeters.
    double distance{};
};
/// Deterministic local replacement for the original actor/message orchestration.
/// Motion retains millimeters/tick (2 max speed, 0.6 max force); elapsed seconds
/// select how many ticks run and never change the per-tick physical constants.
///
/// A facade: every call forwards to a private implementation that composes the floor plan,
/// traffic, predictive passing, production and motion components. With the same blueprint,
/// seed, settings and commands, two simulations run identically, tick for tick. Indices
/// of movers, producers, junctions and corridors are positions in the respective lists.
/// Not thread-safe.
class Simulation {
  public:
    /// Builds the floor, movers and producers of `blueprint`; `seed` seeds the workload.
    /// Starts `paused` with default motion settings and the workload on. Throws
    /// std::runtime_error when a mover starts in a forbidden cell.
    explicit Simulation(Blueprint blueprint, std::uint32_t seed = 42);
    /// The blueprint the simulation was built from.
    const Blueprint &blueprint() const;
    /// All movers, in blueprint order.
    const std::vector<Mover> &movers() const;
    /// All producers, in blueprint order.
    const std::vector<Producer> &producers() const;
    /// Open products; completed ones leave this list.
    const std::vector<Product> &products() const;
    /// The route grid with its blocked cells and current heatmap weights.
    const kinetra::WeightedGrid &grid() const;
    /// Segments between free and blocked space, including the outer edge.
    std::span<const kinetra::Segment> borders() const;
    /// Ticks run so far.
    std::uint64_t ticks() const;
    /// Products whose recipe completed.
    std::uint64_t completed() const;
    /// Contacts that started: a mover pair or mover-border pair that keeps touching counts once.
    std::uint64_t collisions() const;
    /// Movers with infeasible reciprocal constraints or overridden avoidance in the last movement tick.
    std::size_t avoidance_limited() const;
    /// Automatic parking returns temporarily staged outside active departure routes.
    std::size_t parking_waiting() const;
    /// Number of isolated single-cell junctions the floor has.
    std::size_t junction_count() const;
    /// Center of junction `index`. Throws std::out_of_range for an unknown junction.
    Vec2 junction_center(std::size_t index) const;
    /// Movers stopped before a junction in the last tick.
    std::size_t junction_waiting() const;
    /// Movers stopped in the last tick because the exit space behind a junction or
    /// corridor was not free.
    std::size_t junction_exit_waiting() const;
    /// Exit space reserved behind the mover admitted to junction `index`, if any. Throws
    /// std::out_of_range for an unknown junction.
    std::optional<kinetra::Aabb> junction_exit_reservation(std::size_t index) const;
    /// Number of straight single-cell corridors between rooms that the floor has.
    std::size_t corridor_count() const;
    /// Movers stopped before or held in a corridor in the last tick.
    std::size_t corridor_waiting() const;
    /// Corridors with an active recovery.
    std::size_t corridor_recovering() const;
    /// Where `mover` retreats in a corridor recovery, if it yields in one. Throws
    /// std::out_of_range for an unknown mover.
    std::optional<Vec2> corridor_retreat_target(std::size_t mover) const;
    /// Bounds of corridor `index`. Throws std::out_of_range for an unknown corridor.
    kinetra::Aabb corridor_bounds(std::size_t index) const;
    /// Exit space reserved behind the mover admitted to corridor `index`, if any. Throws
    /// std::out_of_range for an unknown corridor.
    std::optional<kinetra::Aabb> corridor_exit_reservation(std::size_t index) const;
    /// Active predictive passing plans.
    std::size_t predictive_pairs() const;
    /// The temporary target `mover` follows in a passing plan, if it is in one. Throws
    /// std::out_of_range for an unknown mover.
    std::optional<Vec2> passing_target(std::size_t mover) const;
    /// True when `mover` has made no route progress for 120 ticks while no
    /// corridor or passing coordination holds it back: the HUD's STALLED. Throws
    /// std::out_of_range for an unknown mover.
    bool stalled(std::size_t mover) const;
    /// The motion settings in use.
    const MotionSettings &motion_settings() const;
    /// Replaces the motion settings, in any run state. Clears the coordination state of the
    /// features it turns off and sets every moving mover's goal again, which ends passing
    /// plans and corridor recoveries; bodies do not move. Throws std::invalid_argument for
    /// an unknown profile or a speed, force or horizon that is not finite and positive
    /// (horizon at most 64 ticks); the settings are then unchanged.
    void configure_motion(MotionSettings settings);
    /// Ticks summed over all touching mover and mover-border pairs.
    std::uint64_t contact_ticks() const;
    /// Seed of the workload generator. With the blueprint, the motion settings and the
    /// commands given, it reproduces a run tick for tick.
    std::uint32_t seed() const;
    /// Turns the event log on or off; it is off for a new simulation. Recording only
    /// observes: it never changes a decision. Turning it off keeps the events so far,
    /// and copying the simulation copies its log.
    void record_events(bool enabled);
    /// Whether events are being recorded.
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
    /// The current run state.
    RunState state() const;
    /// True in `RunState::running`.
    bool running() const;
    /// Moves a `paused` simulation to `running`; a `finished` one stays finished.
    void run();
    /// Moves a `running` simulation to `paused`; a `finished` one stays finished.
    void pause();
    /// Whether `tick` adds a random product while fewer products than movers are open.
    bool workload() const;
    /// Turns the random workload on or off; open products stay.
    void set_workload(bool enabled);
    /// Runs as many ticks as `elapsed` seconds hold at the current update rate, while
    /// `running`; the remainder carries over. Throws std::invalid_argument for a negative or
    /// non-finite `elapsed`.
    void advance(double elapsed);
    /// Runs one tick while `running`, and does nothing otherwise. Reaching the blueprint's
    /// tick cap moves the simulation to `finished`.
    void tick();
    /// Sets how many ticks one simulated second holds, in 1..10000 (default 200), and
    /// drops the time accumulated so far. Throws std::invalid_argument outside that range.
    void updates_per_second(unsigned rate);
    /// Ticks per simulated second.
    unsigned updates_per_second() const;
    /// Opens a product of `kind` and returns its id. Throws std::runtime_error when as
    /// many products are open as there are movers.
    std::uint64_t add_product(ProductKind kind);
    /// Holds or releases mover `index`; holding stops it at once. Throws std::out_of_range
    /// for an unknown mover.
    void toggle_mover(std::size_t index);
    /// Blocks or unblocks producer `index`. Throws std::out_of_range for an unknown producer.
    void toggle_producer(std::size_t index);
    /// Sends mover `index` to `destination` (a mover center, in millimeters) as a manual
    /// goal. Returns false, and changes nothing, when the mover is held or carries a
    /// product, the destination cell is blocked, or no route exists. A mover that shuttles
    /// stops shuttling when the goal is accepted. Throws std::out_of_range for an unknown
    /// mover.
    bool move_to(std::size_t index, Vec2 destination);
    /// Sends mover `index` to `destination` like `move_to`, and then back and forth between
    /// its current position and `destination`. All shuttling movers turn together: once
    /// every one of them has arrived and none is held, they stand `shuttle_pause_ticks`
    /// ticks and then each heads for its other end. A mover stops shuttling when it gets a
    /// manual goal, a product or a factory goal, or when a leg has no route; the others
    /// carry on. Calling it again for a shuttling mover replaces both ends. Returns false,
    /// and changes nothing, when `move_to` would. Throws std::out_of_range for an unknown
    /// mover.
    bool shuttle(std::size_t index, Vec2 destination);
    /// True while mover `index` shuttles. Throws std::out_of_range for an unknown mover.
    bool shuttling(std::size_t index) const;
    /// Writes `summary.json` with run totals and per-mover, per-producer and per-product
    /// statistics into `directory`, creating it. Throws std::runtime_error when the file
    /// cannot be written.
    void dump(const std::filesystem::path &directory) const;

    /// Copies the whole simulation state, including the event log; the copy runs on
    /// independently. A moved-from simulation may only be assigned to or destroyed.
    Simulation(const Simulation &other);
    /// Replaces this simulation with a deep copy of `other`.
    Simulation &operator=(const Simulation &other);
    /// Takes over `other`'s state; `other` is left moved-from.
    Simulation(Simulation &&other) noexcept;
    /// Takes over `other`'s state; `other` is left moved-from.
    Simulation &operator=(Simulation &&other) noexcept;
    ~Simulation();

  private:
    // Copies are deep: tests copy a running simulation to branch off a replay.
    std::unique_ptr<detail::SimulationCore> core_;
};
} // namespace i4sim

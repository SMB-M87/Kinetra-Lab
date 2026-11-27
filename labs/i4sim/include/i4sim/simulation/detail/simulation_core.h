#pragma once
// The simulation behind the public `Simulation` facade. Private to the i4sim library.
#include "i4sim/simulation/detail/event_log.h"
#include "i4sim/simulation/detail/fleet.h"
#include "i4sim/simulation/detail/floor_plan.h"
#include "i4sim/simulation/detail/goals.h"
#include "i4sim/simulation/detail/make_way.h"
#include "i4sim/simulation/detail/motion_step.h"
#include "i4sim/simulation/detail/predictive_passing.h"
#include "i4sim/simulation/detail/production.h"
#include "i4sim/simulation/detail/shuttle.h"
#include "i4sim/simulation/detail/stall_detector.h"
#include "i4sim/simulation/detail/traffic.h"
#include "i4sim/simulation/simulation.h"

#include <kinetra/simulation/clock.h>

#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <vector>

namespace i4sim::detail {
/// Owns the whole simulation state and runs the tick. `Simulation` forwards every call to
/// one instance, so this class can use the private component headers; its public members
/// carry the same contracts as the `Simulation` members of the same name.
class SimulationCore {
  public:
    explicit SimulationCore(Blueprint blueprint, std::uint32_t seed = 42);
    const Blueprint &blueprint() const { return blueprint_; }
    const std::vector<Mover> &movers() const { return fleet_.movers; }
    const std::vector<Producer> &producers() const { return production_.producers(); }
    const std::vector<Product> &products() const { return production_.products(); }
    const kinetra::WeightedGrid &grid() const { return floor_.grid(); }
    std::span<const kinetra::Segment> borders() const { return floor_.borders(); }
    std::uint64_t ticks() const { return ticks_; }
    std::uint64_t completed() const { return production_.completed(); }
    std::uint64_t collisions() const { return motion_step_.collisions(); }
    /// Movers with infeasible reciprocal constraints or overridden avoidance in the last movement tick.
    std::size_t avoidance_limited() const { return motion_step_.avoidance_limited(); }
    /// Automatic parking returns temporarily staged outside active departure routes.
    std::size_t parking_waiting() const;
    std::size_t junction_count() const { return floor_.junction_count(); }
    Vec2 junction_center(std::size_t index) const;
    std::size_t junction_waiting() const;
    std::size_t junction_exit_waiting() const;
    std::optional<kinetra::Aabb> junction_exit_reservation(std::size_t index) const;
    std::size_t corridor_count() const { return floor_.corridor_count(); }
    std::size_t corridor_waiting() const;
    std::size_t corridor_recovering() const;
    std::optional<Vec2> corridor_retreat_target(std::size_t mover) const;
    kinetra::Aabb corridor_bounds(std::size_t index) const;
    std::optional<kinetra::Aabb> corridor_exit_reservation(std::size_t index) const;
    std::size_t predictive_pairs() const { return passing_.pairs(); }
    std::optional<Vec2> passing_target(std::size_t mover) const;
    std::optional<Simulation::PassingDetails> passing_details(std::size_t mover) const;
    bool stalled(std::size_t mover) const;
    std::optional<StallDetails> stall_details(std::size_t mover) const { return stalls_.details(mover); }
    const MotionSettings &motion_settings() const { return motion_; }
    /// Replaces the motion settings, in any run state. Clears the coordination state of the
    /// features it turns off and sets every moving mover's goal again, which ends passing
    /// plans and corridor recoveries; bodies do not move. Throws std::invalid_argument for
    /// an unknown profile or a speed, force or horizon that is not finite and positive
    /// (horizon at most 64 ticks); the settings are then unchanged.
    void configure_motion(MotionSettings settings);
    std::uint64_t contact_ticks() const { return motion_step_.contact_ticks(); }
    /// Seed of the workload generator. With the blueprint, the motion settings and the
    /// commands given, it reproduces a run tick for tick.
    std::uint32_t seed() const { return seed_; }
    /// Turns the event log on or off; it is off for a new simulation. Recording only
    /// observes: it never changes a decision. Turning it off keeps the events so far,
    /// and copying the simulation copies its log.
    void record_events(bool enabled) { log_.record(enabled); }
    bool recording_events() const { return log_.recording(); }
    /// Passing, contact, stall and make-way events recorded while recording was on, oldest first. The
    /// log grows without bound while recording is on. The span is valid until the next
    /// call that can record (a tick, a goal, a motion change); copy what must outlive it.
    std::span<const SimulationEvent> events() const { return log_.events(); }
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
    bool shuttle(std::size_t index, Vec2 destination);
    bool shuttling(std::size_t index) const;
    void dump(const std::filesystem::path &directory) const;

  private:
    /// The goal view over this simulation's components at the current tick.
    Goals goals() { return {floor_, fleet_, traffic_, passing_, log_, motion_, ticks_}; }
    /// Aligns every mover's previous position with its current one.
    void settle_previous_positions();
    /// Gives mover `index` the manual goal `target` when move_to's conditions allow it;
    /// leaves shuttle membership alone. Returns false, changing nothing, otherwise.
    bool send_manual(std::size_t index, Vec2 target);
    /// Sends the legs the shuttle asks for after this tick; a mover whose leg is
    /// rejected leaves the shuttle.
    void turn_shuttle();

    RunState state_{RunState::paused};
    bool workload_{true};
    MotionSettings motion_;
    // Declared before the components that are built from it.
    Blueprint blueprint_;
    FloorPlan floor_;
    Traffic traffic_;
    PredictivePassing passing_;
    EventLog log_;
    Fleet fleet_;
    Production production_;
    MotionStep motion_step_;
    Shuttle shuttle_;
    StallDetector stalls_;
    MakeWay make_way_;
    std::uint32_t seed_;
    /// Set while not running; the first running advance() then aligns previous positions
    /// with the current ones, so drawing does not step back behind the state shown.
    bool resume_pending_{true};
    kinetra::FixedClock clock_{1.0 / 200, 256};
    unsigned ups_{200};
    std::uint64_t ticks_{};
};
} // namespace i4sim::detail

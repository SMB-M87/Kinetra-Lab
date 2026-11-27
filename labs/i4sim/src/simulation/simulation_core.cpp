#include "i4sim/simulation/detail/simulation_core.h"
#include "i4sim/simulation/detail/simulation_constants.h"
#include "i4sim/simulation/flow_metrics.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <stdexcept>

namespace i4sim::detail {
using namespace kinetra;
bool SimulationCore::stalled(std::size_t index) const {
    const auto &m = fleet_.movers.at(index);
    return fleet_.progress[index].no_progress >= stall_ticks && !m.coordination.corridor_retreating &&
           !m.coordination.corridor_waiting && !m.coordination.passing;
}
SimulationCore::SimulationCore(Blueprint b, std::uint32_t seed)
    : blueprint_(std::move(b)), floor_(blueprint_), traffic_(floor_), production_(blueprint_, seed),
      stalls_(blueprint_.movers.size()), seed_(seed) {
    for (const auto &definition : blueprint_.movers) {
        Mover m;
        m.definition = definition;
        m.body.shape = Shape::box;
        m.body.motion = Motion::controlled;
        m.body.half_extents = mover_spec(definition.model).dimension / 2;
        fleet_.max_half.x = std::max(fleet_.max_half.x, m.body.half_extents.x);
        fleet_.max_half.y = std::max(fleet_.max_half.y, m.body.half_extents.y);
        m.body.position = definition.position + m.body.half_extents;
        m.parking = m.destination = m.previous_position = m.body.position;
        if (floor_.grid().blocked(floor_.grid().cell(m.parking)))
            throw std::runtime_error("Mover starts in a forbidden cell: " + definition.id);
        fleet_.movers.push_back(m);
    }
    fleet_.progress.resize(fleet_.movers.size());
    floor_.update_heatmap(fleet_.movers, motion_.heatmap);
}
void SimulationCore::configure_motion(MotionSettings settings) {
    if (int(settings.profile) < int(SteeringProfile::improved) ||
        int(settings.profile) > int(SteeringProfile::reciprocal) || !std::isfinite(settings.speed) ||
        settings.speed <= 0 || !std::isfinite(settings.force) || settings.force <= 0 ||
        !std::isfinite(settings.horizon) || settings.horizon <= 0 ||
        settings.horizon > longest_motion_horizon)
        throw std::invalid_argument("Invalid motion settings");
    // Only these two settings change what a route is. Replanning for any other change sent
    // movers onto routes planned from mid-aisle with the heatmap of that moment, sometimes
    // head-on into others, and reset the progress that lets a stuck mover replan.
    const bool heatmap_changes = settings.heatmap != motion_.heatmap;
    const bool routes_change = heatmap_changes || settings.path_planning != motion_.path_planning;
    const bool passing_ends = PredictivePassing::enabled(motion_) && !PredictivePassing::enabled(settings);
    traffic_.configure(floor_, settings);
    for (auto &m : fleet_.movers) {
        auto &coordination = m.coordination;
        if (!settings.junction_traffic)
            coordination.junction_waiting = false;
        if (!settings.corridor_traffic)
            coordination.corridor_waiting = coordination.corridor_retreating = false;
        if (!coordination.junction_waiting && !coordination.corridor_waiting)
            coordination.exit_waiting = false;
    }
    fleet_.stops.clear();
    motion_ = settings;
    // The tick refreshes the heatmap every heatmap_period_ticks; only switching it on or off
    // needs the new weights now.
    if (heatmap_changes)
        floor_.update_heatmap(fleet_.movers, motion_.heatmap);
    if (passing_ends)
        passing_.disable(floor_, fleet_, motion_, ticks_, log_);
    for (auto &m : fleet_.movers) {
        m.body.velocity = limited(m.body.velocity, settings.speed);
        if (!settings.parking_traffic && m.goal == GoalKind::parking_staging)
            goals().set(m, m.parking);
        else if (m.moving() && m.goal != GoalKind::making_way &&
                 (routes_change || (m.detour && !settings.recovery))) {
            // Setting the same goal again replans the route but keeps its origin. A recovery
            // detour ends with recovery: a mover keeping one would skip its own obstruction
            // replanning, and nothing else would free it.
            const auto goal = m.goal;
            goals().set(m, m.destination);
            m.goal = goal;
        }
    }
}
void SimulationCore::single_step() {
    if (state_ != RunState::paused)
        return;
    state_ = RunState::running;
    tick();
    if (state_ == RunState::running)
        state_ = RunState::paused;
}
void SimulationCore::run() {
    if (state_ == RunState::paused)
        state_ = RunState::running;
}
void SimulationCore::pause() {
    if (state_ == RunState::running)
        state_ = RunState::paused;
}
void SimulationCore::updates_per_second(unsigned rate) {
    if (rate < 1 || rate > most_updates_per_second)
        throw std::invalid_argument("UPS must be in 1.." + std::to_string(most_updates_per_second));
    ups_ = rate;
    clock_.rate(rate);
    // The new clock starts with no accumulated time; blending from the last tick's start
    // would draw the movers a tick back.
    settle_previous_positions();
}
void SimulationCore::settle_previous_positions() {
    for (auto &m : fleet_.movers)
        m.previous_position = m.body.position;
}
void SimulationCore::advance(double elapsed, std::uint64_t tick_limit) {
    if (!std::isfinite(elapsed) || elapsed < 0)
        throw std::invalid_argument("Invalid elapsed time");
    if (!running()) {
        resume_pending_ = true;
        return;
    }
    if (resume_pending_) {
        settle_previous_positions();
        resume_pending_ = false;
    }
    const auto count = clock_.advance(elapsed, tick_limit);
    for (std::uint64_t i = 0; i < count && running(); ++i)
        tick();
}
std::uint64_t SimulationCore::add_product(ProductKind kind) {
    return production_.add_product(kind, fleet_.movers.size(), ticks_);
}
void SimulationCore::toggle_mover(std::size_t index) {
    auto &m = fleet_.movers.at(index);
    m.held = !m.held;
    if (m.held)
        m.body.velocity = {};
}
void SimulationCore::toggle_producer(std::size_t index) {
    production_.toggle_producer(index);
}
bool SimulationCore::move_to(std::size_t index, Vec2 target) {
    if (!send_manual(index, target))
        return false;
    shuttle_.leave(index);
    return true;
}
bool SimulationCore::shuttle(std::size_t index, Vec2 target) {
    const auto start = fleet_.movers.at(index).body.position;
    if (!send_manual(index, target))
        return false;
    shuttle_.join(index, start, target);
    return true;
}
bool SimulationCore::shuttling(std::size_t index) const {
    if (index >= fleet_.movers.size())
        throw std::out_of_range("Mover index");
    return shuttle_.contains(index);
}
void SimulationCore::turn_shuttle() {
    for (const auto &leg : shuttle_.update(fleet_.movers, ticks_))
        if (!send_manual(leg.mover, leg.destination))
            shuttle_.leave(leg.mover);
}
bool SimulationCore::send_manual(std::size_t index, Vec2 target) {
    auto &m = fleet_.movers.at(index);
    if (m.held || m.product || floor_.grid().blocked(floor_.grid().cell(target)))
        return false;
    auto route = floor_.grid().path(m.body.position, target, m.body.half_extents);
    if (route.empty())
        return false;
    goals().set(m, target);
    m.goal = GoalKind::manual;
    return true;
}
std::size_t SimulationCore::parking_waiting() const {
    return std::count_if(fleet_.movers.begin(), fleet_.movers.end(),
                         [](const Mover &m) { return m.goal == GoalKind::parking_staging; });
}
Vec2 SimulationCore::junction_center(std::size_t index) const {
    if (index >= floor_.junction_count())
        throw std::out_of_range("Junction index");
    return floor_.zones()[index].center;
}
std::size_t SimulationCore::corridor_waiting() const {
    return std::count_if(fleet_.movers.begin(), fleet_.movers.end(),
                         [](const Mover &m) { return m.coordination.corridor_waiting; });
}
Aabb SimulationCore::corridor_bounds(std::size_t index) const {
    if (index >= corridor_count())
        throw std::out_of_range("Corridor index");
    const auto &zone = floor_.zones()[floor_.junction_count() + index];
    return {zone.center - zone.half_extents, zone.center + zone.half_extents};
}
std::optional<Aabb> SimulationCore::corridor_exit_reservation(std::size_t index) const {
    if (index >= corridor_count())
        throw std::out_of_range("Corridor index");
    return traffic_.exit_reservation(floor_.junction_count() + index);
}
std::size_t SimulationCore::junction_waiting() const {
    return std::count_if(fleet_.movers.begin(), fleet_.movers.end(),
                         [](const Mover &m) { return m.coordination.junction_waiting; });
}
std::size_t SimulationCore::junction_exit_waiting() const {
    return std::count_if(fleet_.movers.begin(), fleet_.movers.end(),
                         [](const Mover &m) { return m.coordination.exit_waiting; });
}
std::optional<Aabb> SimulationCore::junction_exit_reservation(std::size_t index) const {
    if (index >= floor_.junction_count())
        throw std::out_of_range("Junction index");
    return traffic_.exit_reservation(index);
}
std::optional<Vec2> SimulationCore::passing_target(std::size_t mover) const {
    if (mover >= fleet_.movers.size())
        throw std::out_of_range("Mover index");
    return passing_.target(mover);
}
std::optional<Simulation::PassingDetails> SimulationCore::passing_details(std::size_t mover) const {
    if (mover >= fleet_.movers.size())
        throw std::out_of_range("Mover index");
    return passing_.details(mover);
}
std::size_t SimulationCore::corridor_recovering() const {
    return traffic_.recovering();
}
std::optional<Vec2> SimulationCore::corridor_retreat_target(std::size_t mover) const {
    if (mover >= fleet_.movers.size())
        throw std::out_of_range("Mover index");
    return traffic_.retreat_target(mover);
}
void SimulationCore::tick() {
    if (!running())
        return;
    if (ticks_ >= blueprint_.tick_cap) {
        state_ = RunState::finished;
        return;
    }
    ++ticks_;
    for (auto &m : fleet_.movers)
        m.previous_position = m.body.position;
    if (workload_)
        production_.generate(fleet_.movers.size(), ticks_);
    if (ticks_ % heatmap_period_ticks == 1)
        floor_.update_heatmap(fleet_.movers, motion_.heatmap);
    auto goals_now = goals();
    goals_now.coordinate_parking();
    motion_step_.run(goals_now, production_);
    production_.update(goals_now);
    turn_shuttle();
    // Observe the completed tick, at the same point as the batch benchmark.
    stalls_.observe(fleet_, motion_, ticks_, log_);
    make_way_.update(goals_now, production_.products(), shuttle_, stalls_);
    for (auto &mover : fleet_.movers) {
        if (waiting_for_progress(mover)) {
            ++mover.current_wait_ticks;
            ++mover.total_wait_ticks;
        } else {
            mover.current_wait_ticks = 0;
        }
    }
    if (ticks_ >= blueprint_.tick_cap)
        state_ = RunState::finished;
}
void SimulationCore::dump(const std::filesystem::path &directory) const {
    using Json = nlohmann::json;
    std::filesystem::create_directories(directory);
    Json data{{"blueprint", blueprint_.name},
              {"ticks", ticks_},
              {"completed", production_.completed()},
              {"contactTicks", motion_step_.contact_ticks()},
              {"collisions", motion_step_.collisions()},
              {"movers", Json::array()},
              {"producers", Json::array()},
              {"products", Json::array()}};
    for (const auto &m : fleet_.movers)
        data["movers"].push_back({{"id", m.definition.id},
                                  {"distance", m.distance},
                                  {"idleTicks", m.idle_ticks},
                                  {"transportTicks", m.transport_ticks},
                                  {"interactions", m.interactions},
                                  {"blocked", m.held}});
    for (const auto &p : production_.producers())
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
    for (const auto &p : production_.completed_products())
        record(p);
    for (const auto &p : production_.products())
        record(p);
    std::ofstream out(directory / "summary.json");
    out << data.dump(2) << '\n';
    if (!out)
        throw std::runtime_error("Cannot write simulation summary: " + directory.string());
}
} // namespace i4sim::detail

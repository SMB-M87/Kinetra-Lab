#include "simulation_core.h"
#include "simulation_constants.h"

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
      seed_(seed) {
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
        !std::isfinite(settings.horizon) || settings.horizon <= 0 || settings.horizon > 64)
        throw std::invalid_argument("Invalid motion settings");
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
    floor_.update_heatmap(fleet_.movers, motion_.heatmap);
    for (auto &m : fleet_.movers) {
        m.body.velocity = limited(m.body.velocity, settings.speed);
        if (!settings.parking_traffic && m.goal == GoalKind::parking_staging)
            goals().set(m, m.parking);
        else if (m.moving()) {
            // Setting the same goal again replans the route but keeps its origin.
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
    if (rate < 1 || rate > 10000)
        throw std::invalid_argument("UPS must be in 1..10000");
    ups_ = rate;
    clock_ = FixedClock(1.0 / rate, 256);
    // The new clock starts with no accumulated time; blending from the last tick's start
    // would draw the movers a tick back.
    settle_previous_positions();
}
void SimulationCore::settle_previous_positions() {
    for (auto &m : fleet_.movers)
        m.previous_position = m.body.position;
}
void SimulationCore::advance(double elapsed) {
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
    const int count = clock_.advance(elapsed);
    for (int i = 0; i < count && running(); ++i)
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

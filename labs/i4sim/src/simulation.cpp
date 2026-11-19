#include "simulation.h"
#include "simulation_core.h"

#include <utility>

namespace i4sim {
MoverStatus mover_status(const Mover &m) {
    if (m.held)
        return MoverStatus::held;
    if (m.coordination.corridor_retreating)
        return MoverStatus::corridor_retreat;
    if (m.coordination.passing)
        return MoverStatus::passing_detour;
    if (m.lifecycle == MoverLifecycle::unreachable)
        return MoverStatus::unreachable;
    return m.product ? MoverStatus::allocated : MoverStatus::available;
}
std::vector<Interaction> recipe(ProductKind kind) {
    using enum Interaction;
    if (kind == ProductKind::personalized)
        return {place_housing, place_trimmer, place_lever, place_card, personalize_card, remove_assy};
    return {place_housing, place_trimmer, place_lever, remove_assy};
}
Simulation::Simulation(Blueprint blueprint, std::uint32_t seed)
    : core_(std::make_unique<detail::SimulationCore>(std::move(blueprint), seed)) {
}
Simulation::Simulation(const Simulation &other)
    : core_(std::make_unique<detail::SimulationCore>(*other.core_)) {
}
Simulation &Simulation::operator=(const Simulation &other) {
    if (this != &other)
        core_ = std::make_unique<detail::SimulationCore>(*other.core_);
    return *this;
}
Simulation::Simulation(Simulation &&other) noexcept = default;
Simulation &Simulation::operator=(Simulation &&other) noexcept = default;
Simulation::~Simulation() = default;
const Blueprint &Simulation::blueprint() const {
    return core_->blueprint();
}
const std::vector<Mover> &Simulation::movers() const {
    return core_->movers();
}
const std::vector<Producer> &Simulation::producers() const {
    return core_->producers();
}
const std::vector<Product> &Simulation::products() const {
    return core_->products();
}
const kinetra::WeightedGrid &Simulation::grid() const {
    return core_->grid();
}
std::span<const kinetra::Segment> Simulation::borders() const {
    return core_->borders();
}
std::uint64_t Simulation::ticks() const {
    return core_->ticks();
}
std::uint64_t Simulation::completed() const {
    return core_->completed();
}
std::uint64_t Simulation::collisions() const {
    return core_->collisions();
}
std::size_t Simulation::avoidance_limited() const {
    return core_->avoidance_limited();
}
std::size_t Simulation::parking_waiting() const {
    return core_->parking_waiting();
}
std::size_t Simulation::junction_count() const {
    return core_->junction_count();
}
Vec2 Simulation::junction_center(std::size_t index) const {
    return core_->junction_center(index);
}
std::size_t Simulation::junction_waiting() const {
    return core_->junction_waiting();
}
std::size_t Simulation::junction_exit_waiting() const {
    return core_->junction_exit_waiting();
}
std::optional<kinetra::Aabb> Simulation::junction_exit_reservation(std::size_t index) const {
    return core_->junction_exit_reservation(index);
}
std::size_t Simulation::corridor_count() const {
    return core_->corridor_count();
}
std::size_t Simulation::corridor_waiting() const {
    return core_->corridor_waiting();
}
std::size_t Simulation::corridor_recovering() const {
    return core_->corridor_recovering();
}
std::optional<Vec2> Simulation::corridor_retreat_target(std::size_t mover) const {
    return core_->corridor_retreat_target(mover);
}
kinetra::Aabb Simulation::corridor_bounds(std::size_t index) const {
    return core_->corridor_bounds(index);
}
std::optional<kinetra::Aabb> Simulation::corridor_exit_reservation(std::size_t index) const {
    return core_->corridor_exit_reservation(index);
}
std::size_t Simulation::predictive_pairs() const {
    return core_->predictive_pairs();
}
std::optional<Vec2> Simulation::passing_target(std::size_t mover) const {
    return core_->passing_target(mover);
}
bool Simulation::stalled(std::size_t mover) const {
    return core_->stalled(mover);
}
const MotionSettings &Simulation::motion_settings() const {
    return core_->motion_settings();
}
void Simulation::configure_motion(MotionSettings settings) {
    core_->configure_motion(settings);
}
std::uint64_t Simulation::contact_ticks() const {
    return core_->contact_ticks();
}
std::uint32_t Simulation::seed() const {
    return core_->seed();
}
void Simulation::record_events(bool enabled) {
    core_->record_events(enabled);
}
bool Simulation::recording_events() const {
    return core_->recording_events();
}
std::span<const SimulationEvent> Simulation::events() const {
    return core_->events();
}
void Simulation::single_step() {
    core_->single_step();
}
double Simulation::interpolation() const {
    return core_->interpolation();
}
RunState Simulation::state() const {
    return core_->state();
}
bool Simulation::running() const {
    return core_->running();
}
void Simulation::run() {
    core_->run();
}
void Simulation::pause() {
    core_->pause();
}
bool Simulation::workload() const {
    return core_->workload();
}
void Simulation::set_workload(bool enabled) {
    core_->set_workload(enabled);
}
void Simulation::advance(double elapsed) {
    core_->advance(elapsed);
}
void Simulation::tick() {
    core_->tick();
}
void Simulation::updates_per_second(unsigned rate) {
    core_->updates_per_second(rate);
}
unsigned Simulation::updates_per_second() const {
    return core_->updates_per_second();
}
std::uint64_t Simulation::add_product(ProductKind kind) {
    return core_->add_product(kind);
}
void Simulation::toggle_mover(std::size_t index) {
    core_->toggle_mover(index);
}
void Simulation::toggle_producer(std::size_t index) {
    core_->toggle_producer(index);
}
bool Simulation::move_to(std::size_t index, Vec2 destination) {
    return core_->move_to(index, destination);
}
bool Simulation::shuttle(std::size_t index, Vec2 destination) {
    return core_->shuttle(index, destination);
}
bool Simulation::shuttling(std::size_t index) const {
    return core_->shuttling(index);
}
void Simulation::dump(const std::filesystem::path &directory) const {
    core_->dump(directory);
}
} // namespace i4sim

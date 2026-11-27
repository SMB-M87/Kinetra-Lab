#include "i4sim/controller.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace i4sim {
namespace {
using kinetra::Camera2D;
using kinetra::Input;
using kinetra::Key;

/// Bounds of the update rate, in ticks per simulated second.
constexpr unsigned slowest_update_rate = 1, fastest_update_rate = 10000;
/// How long a rejected command stays on screen, in seconds.
constexpr double message_seconds = 4;
/// Screen height the fit keeps free for the toolbar and the HUD lines, in pixels, and the
/// share of the remaining viewport the floor fills.
constexpr float fit_reserved_height = 120, fit_fill = 0.9f;
/// Camera pan speed in screen pixels per second, so panning feels the same at every zoom.
constexpr float pan_speed = 400;
/// Longest frame time that still moves the camera its full distance, in seconds: after a
/// hitch the view does not jump.
constexpr double longest_pan_frame = 0.1;
/// Factor of one SPEED or FORCE step, and the shortest horizon in ticks; the longest is
/// `longest_motion_horizon`.
constexpr float parameter_step = 1.25f, shortest_horizon = 1;

/// The choices of the PLANNER list, in menu order: two settings of MotionSettings shown as
/// one list, because the heatmap only matters while routes are planned.
enum class Planner { heat_astar, astar, off };

Planner planner(const MotionSettings &motion) {
    if (!motion.path_planning)
        return Planner::off;
    return motion.heatmap ? Planner::heat_astar : Planner::astar;
}

const char *planner_name(Planner planner) {
    switch (planner) {
    case Planner::astar:
        return "A*";
    case Planner::off:
        return "OFF";
    default:
        return "HEAT A*";
    }
}

/// Sets `motion` to `choice`. OFF keeps the heatmap setting, so choosing a planner again
/// later restores the costs it had.
void choose_planner(MotionSettings &motion, Planner choice) {
    motion.path_planning = choice != Planner::off;
    if (choice != Planner::off)
        motion.heatmap = choice == Planner::heat_astar;
}

/// A motion parameter for a menu row: three significant digits, so even the largest float
/// ends before the row's [-] button.
std::string number_text(float value) {
    std::ostringstream text;
    text << std::setprecision(3) << value;
    return text.str();
}

/// The debug layers in menu order with their row labels, each layer followed by its parts.
constexpr std::pair<std::string_view, Layer> layer_rows[]{{"GRID", Layer::grid},
                                                          {"HEATMAP", Layer::heatmap},
                                                          {"PATHS", Layer::paths},
                                                          {"ROUTES", Layer::routes},
                                                          {"RETREAT TARGETS", Layer::retreat_targets},
                                                          {"PASSING TARGETS", Layer::passing_targets},
                                                          {"FORCES", Layer::forces},
                                                          {"SEEK", Layer::seek_force},
                                                          {"COLLISION", Layer::collision_force},
                                                          {"BORDER", Layer::border_force},
                                                          {"PREDICTIVE", Layer::predictive_force},
                                                          {"VELOCITY", Layer::velocity},
                                                          {"ACCELERATION", Layer::acceleration},
                                                          {"BORDERS", Layer::borders},
                                                          {"WALLS", Layer::walls},
                                                          {"TRAFFIC ZONES", Layer::traffic_zones},
                                                          {"DETECTION", Layer::detection},
                                                          {"CONTACT SHAPES", Layer::contact_shapes}};
} // namespace

FactoryController::FactoryController(std::uint32_t seed, bool running, unsigned updates_per_second)
    : seed_(seed), start_running_(running), updates_per_second_(updates_per_second) {
    if (updates_per_second < slowest_update_rate || updates_per_second > fastest_update_rate)
        throw std::invalid_argument("UPS must be in 1..10000");
}

bool FactoryController::load(const std::filesystem::path &path) {
    try {
        // Built aside, so a blueprint or scenario that fails leaves the loaded one intact.
        Simulation next(load_blueprint(path), seed_);
        auto scenario = load_scenario(path);
        if (scenario)
            apply_scenario(*scenario, next);
        simulation_ = std::move(next);
        scenario_ = std::move(scenario);
        restart_timer_ = {};
        if (start_running_)
            simulation_->run();
        simulation_->updates_per_second(updates_per_second_);
        selected_mover_.reset();
        selected_producer_.reset();
        clear_message();
        fit_pending_ = true;
        // A close_later() request was for the blueprint this load replaced.
        close_requested_ = false;
        return true;
    } catch (const std::exception &e) {
        report_error(e.what());
        return false;
    }
}

std::string_view FactoryController::description() const {
    return scenario_ ? std::string_view(scenario_->description) : std::string_view();
}

void FactoryController::report_error(const std::string &message) {
    message_ = message;
    message_remaining_ = 0;
    std::cerr << "i4sim: " << message_ << '\n';
}

void FactoryController::clear_message() {
    message_.clear();
    message_remaining_ = 0;
}

void FactoryController::show_for_a_while(std::string message) {
    message_ = std::move(message);
    message_remaining_ = message_seconds;
}

void FactoryController::close() {
    simulation_.reset();
    scenario_.reset();
    selected_mover_.reset();
    selected_producer_.reset();
    // The saved pause state belonged to this simulation; the next one starts fresh.
    modal_pause_ = {};
    close_requested_ = false;
}

bool FactoryController::begin_frame(double elapsed) {
    if (close_requested_)
        close();
    if (message_remaining_ > 0) {
        message_remaining_ = std::max(0.0, message_remaining_ - elapsed);
        if (message_remaining_ == 0)
            clear_message();
    }
    return loaded();
}

void FactoryController::suspend(bool open) {
    if (modal_pause_.update(open, !simulation_->running()))
        simulation_->pause();
    else
        simulation_->run();
}

void FactoryController::toggle_pause() {
    // A finished simulation ignores both and stays finished.
    if (simulation_->running())
        simulation_->pause();
    else
        simulation_->run();
}

void FactoryController::change_motion(const std::function<void(MotionSettings &)> &change) {
    auto settings = simulation_->motion_settings();
    change(settings);
    try {
        simulation_->configure_motion(settings);
        clear_message();
    } catch (const std::exception &e) {
        show_for_a_while(e.what());
    }
}

void FactoryController::change_update_rate(bool faster) {
    const unsigned rate = simulation_->updates_per_second();
    simulation_->updates_per_second(faster ? std::min(fastest_update_rate, rate * 2)
                                           : std::max(slowest_update_rate, rate / 2));
}

void FactoryController::restart() {
    const auto motion = simulation_->motion_settings();
    Simulation fresh(simulation_->blueprint(), seed_);
    fresh.updates_per_second(simulation_->updates_per_second());
    simulation_ = std::move(fresh);
    restart_timer_ = {};
    clear_message();
    if (scenario_) {
        // Replay the scene with the motion settings in use, so a changed setting can be
        // compared against the original run.
        auto replay = *scenario_;
        replay.motion = motion;
        try {
            apply_scenario(replay, *simulation_);
        } catch (const std::exception &e) {
            // A partly applied scene is not the scene; close the blueprint with the cause,
            // as a failed load leaves none.
            close();
            report_error(e.what());
            return;
        }
    } else
        simulation_->configure_motion(motion);
    selected_mover_.reset();
    selected_producer_.reset();
}

void FactoryController::restart_when_due() {
    if (!scenario_ || scenario_->repeat != ScenarioRepeat::restart || !restart_timer_.due(*simulation_))
        return;
    // A scheduled replay resumes a running show. A manual restart starts paused.
    const bool was_running = simulation_->running();
    restart();
    if (loaded() && was_running)
        simulation_->run();
}

void FactoryController::fit(Camera2D &camera) {
    const auto size = simulation_->blueprint().dimension;
    camera.position = flip_y(size / 2);
    camera.pixels_per_unit =
        std::min(camera.viewport.x / size.x, (camera.viewport.y - fit_reserved_height) / size.y) * fit_fill;
    camera.pixels_per_unit = std::max(camera.pixels_per_unit, zoom_limits.minimum);
    fit_pending_ = false;
}

void FactoryController::update_camera(Camera2D &camera) {
    if (fit_pending_)
        fit(camera);
}

void FactoryController::handle_keys(const Input &input, Camera2D &camera) {
    if (input.hit(Key::p) || input.hit(Key::space))
        toggle_pause();
    if (input.hit(Key::r)) {
        restart();
        if (!loaded())
            return;
    }
    if (input.hit(Key::g))
        simulation_->set_workload(!simulation_->workload());
    if (input.hit(Key::f))
        fit(camera);
    const std::pair<Key, Layer> layer_keys[]{{Key::one, Layer::grid},     {Key::two, Layer::heatmap},
                                             {Key::three, Layer::paths},  {Key::four, Layer::forces},
                                             {Key::five, Layer::borders}, {Key::six, Layer::detection}};
    for (const auto &[key, layer] : layer_keys)
        if (input.hit(key))
            layers_.toggle(layer);
    if (input.hit(Key::q))
        change_update_rate(false);
    if (input.hit(Key::e))
        change_update_rate(true);
}

void FactoryController::pick(Vec2 point, bool toggle) {
    for (std::size_t i = 0; i < simulation_->movers().size(); ++i)
        if (bounds(simulation_->movers()[i].body).overlaps({point, point})) {
            selected_mover_ = i;
            selected_producer_.reset();
            if (toggle)
                simulation_->toggle_mover(i);
            return;
        }
    // A producer is picked within its drawn symbol: half the larger cell side.
    const auto cell = simulation_->blueprint().cell_size;
    const float radius = std::max(cell.x, cell.y) / 2;
    for (std::size_t i = 0; i < simulation_->producers().size(); ++i)
        if (distance(point, simulation_->producers()[i].definition.position) <= radius) {
            selected_producer_ = i;
            if (toggle)
                simulation_->toggle_producer(i);
            return;
        }
    if (!toggle)
        selected_producer_.reset();
}

void FactoryController::handle_pointer(const Input &input, const Camera2D &camera) {
    if (!input.clicked && !input.right_clicked)
        return;
    const auto point = flip_y(camera.screen_to_world(input.mouse));
    const bool shift = input.down(Key::shift);
    if (shift && input.clicked && selected_mover_) {
        if (simulation_->move_to(*selected_mover_, point))
            clear_message();
        else
            show_for_a_while("GOAL REJECTED: MOVER BUSY, BLOCKED OR GOAL UNREACHABLE");
    } else {
        // Shift+right-click inspects in Dotio, so here it selects without holding or blocking.
        pick(point, input.right_clicked && !shift);
    }
}

void FactoryController::handle_input(double elapsed, const Input &input, Camera2D &camera,
                                     PanelState panels) {
    // Simulation::advance rejects the same values, but only after the input was applied.
    if (!std::isfinite(elapsed) || elapsed < 0)
        throw std::invalid_argument("Frame time must be finite and not negative");
    update_camera(camera);
    handle_keys(input, camera);
    if (!loaded())
        return;
    // Zero steps still clamp the zoom to its limits. The ZOOM row's steps are taken even
    // outside debug mode, so none is left over for the frame that switches it on.
    const bool wheel_zooms = panels.debug_mode && !panels.pointer_captured;
    kinetra::zoom(camera, (wheel_zooms ? input.wheel : 0) + zoom_.take_steps(), zoom_limits);
    zoom_.observe(camera);
    const float step = float(std::min(elapsed, longest_pan_frame)) * pan_speed / camera.pixels_per_unit;
    camera.position += kinetra_lab::direction(input) * step;
    if (!panels.pointer_captured)
        handle_pointer(input, camera);
    simulation_->advance(elapsed);
    restart_when_due();
}

kinetra::DebugEntry FactoryController::motion_check(std::string label, bool MotionSettings::*setting) {
    return {
        .label = std::move(label),
        .action = [this,
                   setting] { change_motion([setting](MotionSettings &m) { m.*setting = !(m.*setting); }); },
        .kind = kinetra::DebugEntryKind::check,
        .active = [this, setting] { return simulation_->motion_settings().*setting; },
        .depth = 1};
}

kinetra::DebugEntry FactoryController::motion_number(std::string label, std::function<std::string()> value,
                                                     std::function<void(MotionSettings &)> lower,
                                                     std::function<void(MotionSettings &)> raise) {
    return {.label = std::move(label),
            .value = std::move(value),
            .decrease = [this, lower = std::move(lower)] { change_motion(lower); },
            .increase = [this, raise = std::move(raise)] { change_motion(raise); },
            .depth = 1};
}

void FactoryController::add_simulation_entries(std::vector<kinetra::DebugEntry> &entries) {
    entries.push_back({.label = "RUNNING",
                       .action = [this] { toggle_pause(); },
                       .kind = kinetra::DebugEntryKind::check,
                       .active = [this] { return simulation_->running(); },
                       .depth = 1});
    entries.push_back({.label = "SINGLE TICK", .action = [this] { simulation_->single_step(); }, .depth = 1});
    entries.push_back({.label = "UPS",
                       .value = [this] { return std::to_string(simulation_->updates_per_second()); },
                       .decrease = [this] { change_update_rate(false); },
                       .increase = [this] { change_update_rate(true); },
                       .depth = 1});
    entries.push_back({.label = "WORKLOAD",
                       .action = [this] { simulation_->set_workload(!simulation_->workload()); },
                       .kind = kinetra::DebugEntryKind::check,
                       .active = [this] { return simulation_->workload(); },
                       .depth = 1});
}

void FactoryController::add_layer_entries(std::vector<kinetra::DebugEntry> &entries,
                                          const LayerColors &colors) {
    for (const auto &[label, layer] : layer_rows) {
        const Layer parent = parent_layer(layer);
        // As in Dotio, a layer lists its parts only while it is on.
        if (parent != layer && !layers_.selected(parent))
            continue;
        entries.push_back({.label = std::string(label),
                           .action = [this, layer] { layers_.toggle(layer); },
                           .kind = kinetra::DebugEntryKind::check,
                           .active = [this, layer] { return layers_.selected(layer); },
                           .depth = parent == layer ? 1 : 2,
                           .swatch = colors ? colors(layer) : std::nullopt});
    }
}

void FactoryController::add_motion_entries(std::vector<kinetra::DebugEntry> &entries) {
    const auto motion = [this]() -> const MotionSettings & { return simulation_->motion_settings(); };
    entries.push_back(
        folds_.list("PROFILE", [motion] { return std::string(profile_name(motion().profile)); }));
    if (folds_.list_open("PROFILE"))
        for (const auto profile : {SteeringProfile::improved, SteeringProfile::original,
                                   SteeringProfile::experimental_rvo, SteeringProfile::reciprocal})
            entries.push_back({.label = profile_name(profile),
                               .action =
                                   [this, profile] {
                                       // Configuring motion replans every route, so choosing the
                                       // current value only closes the list.
                                       if (simulation_->motion_settings().profile != profile)
                                           change_motion(
                                               [profile](MotionSettings &m) { m.profile = profile; });
                                       folds_.close_list();
                                   },
                               .kind = kinetra::DebugEntryKind::choice,
                               .active = [motion, profile] { return motion().profile == profile; },
                               .depth = 2});
    entries.push_back(
        folds_.list("PLANNER", [motion] { return std::string(planner_name(planner(motion()))); }));
    if (folds_.list_open("PLANNER"))
        for (const auto choice : {Planner::heat_astar, Planner::astar, Planner::off})
            entries.push_back({.label = planner_name(choice),
                               .action =
                                   [this, choice] {
                                       if (planner(simulation_->motion_settings()) != choice)
                                           change_motion(
                                               [choice](MotionSettings &m) { choose_planner(m, choice); });
                                       folds_.close_list();
                                   },
                               .kind = kinetra::DebugEntryKind::choice,
                               .active = [motion, choice] { return planner(motion()) == choice; },
                               .depth = 2});
    entries.push_back(motion_check("RECOVERY", &MotionSettings::recovery));
    entries.push_back(motion_number(
        "SPEED", [motion] { return number_text(motion().speed); },
        [](MotionSettings &m) { m.speed /= parameter_step; },
        [](MotionSettings &m) { m.speed *= parameter_step; }));
    entries.push_back(motion_number(
        "FORCE", [motion] { return number_text(motion().force); },
        [](MotionSettings &m) { m.force /= parameter_step; },
        [](MotionSettings &m) { m.force *= parameter_step; }));
    entries.push_back(motion_number(
        "HORIZON", [motion] { return number_text(motion().horizon); },
        [](MotionSettings &m) { m.horizon = std::max(shortest_horizon, m.horizon - 1); },
        [](MotionSettings &m) { m.horizon = std::min(longest_motion_horizon, m.horizon + 1); }));
    entries.push_back({.label = "RESET PARAMETERS",
                       .action =
                           [this] {
                               change_motion([](MotionSettings &m) {
                                   const MotionSettings defaults;
                                   m.speed = defaults.speed;
                                   m.force = defaults.force;
                                   m.horizon = defaults.horizon;
                               });
                           },
                       .depth = 1});
}

void FactoryController::add_traffic_entries(std::vector<kinetra::DebugEntry> &entries) {
    entries.push_back(motion_check("CORRIDOR TRAFFIC", &MotionSettings::corridor_traffic));
    entries.push_back(motion_check("JUNCTION TRAFFIC", &MotionSettings::junction_traffic));
    entries.push_back(motion_check("PARKING TRAFFIC", &MotionSettings::parking_traffic));
    auto passing = motion_check("PREDICTIVE PASS", &MotionSettings::predictive_passing);
    // The legacy profiles keep the setting but ignore it; say so instead of hiding the row.
    // Entries are built anew after every change, so the value can be decided here.
    if (!supports_predictive_passing(simulation_->motion_settings().profile))
        passing.value = [] { return std::string("LEGACY"); };
    entries.push_back(std::move(passing));
    entries.push_back(motion_check("MAKE WAY", &MotionSettings::make_way));
    entries.push_back(motion_check("DEPARTURE CHECK", &MotionSettings::departure_check));
    auto right_of_way = motion_check("RIGHT OF WAY", &MotionSettings::right_of_way);
    if (!supports_right_of_way(simulation_->motion_settings().profile))
        right_of_way.value = [] { return std::string("LEGACY"); };
    entries.push_back(std::move(right_of_way));
}

std::vector<kinetra::DebugEntry> FactoryController::menu(const LayerColors &colors) {
    std::vector<kinetra::DebugEntry> entries;
    for (const std::string label : {"SIMULATION", "LAYERS", "MOTION", "TRAFFIC"}) {
        entries.push_back(folds_.section(label));
        if (!folds_.section_open(label))
            continue;
        if (label == "SIMULATION")
            add_simulation_entries(entries);
        else if (label == "LAYERS")
            add_layer_entries(entries, colors);
        else if (label == "MOTION")
            add_motion_entries(entries);
        else
            add_traffic_entries(entries);
    }
    entries.push_back(zoom_.entry());
    entries.push_back({.label = "FRAME SCENE", .action = [this] { fit_pending_ = true; }});
    return entries;
}
} // namespace i4sim

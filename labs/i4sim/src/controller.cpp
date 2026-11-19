#include "controller.h"

#include <algorithm>
#include <cmath>
#include <iostream>
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
/// Factor of one SPEED or FORCE step, and the range of the horizon in ticks.
constexpr float parameter_step = 1.25f, shortest_horizon = 1, longest_horizon = 64;
/// What RESET PARAMETERS sets: the original factory's speed in millimeters per tick and
/// force in millimeters per tick squared, which are also the defaults of MotionSettings,
/// and a horizon in ticks. The horizon is shorter than the default of 32 a new simulation
/// starts with; which of the two is meant is an open point in the module README.
constexpr float reset_speed = 2, reset_force = .6f, reset_horizon = 8;

const char *profile_name(SteeringProfile profile) {
    switch (profile) {
    case SteeringProfile::original:
        return "ORIGINAL";
    case SteeringProfile::experimental_rvo:
        return "RVO EXPERIMENT";
    case SteeringProfile::reciprocal:
        return "RECIPROCAL";
    default:
        return "IMPROVED";
    }
}
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
        if (start_running_)
            simulation_->run();
        simulation_->updates_per_second(updates_per_second_);
        selected_mover_.reset();
        selected_producer_.reset();
        clear_message();
        fit_pending_ = true;
        // A BLUEPRINTS request was for the blueprint this load replaced.
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
    if (!loaded())
        return false;
    if (message_remaining_ > 0) {
        message_remaining_ = std::max(0.0, message_remaining_ - elapsed);
        if (message_remaining_ == 0)
            clear_message();
    }
    return true;
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
    simulation_ = std::move(fresh);
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
    const std::pair<Key, bool ViewLayers::*> layer_keys[]{
        {Key::one, &ViewLayers::grid},     {Key::two, &ViewLayers::heatmap},
        {Key::three, &ViewLayers::paths},  {Key::four, &ViewLayers::forces},
        {Key::five, &ViewLayers::borders}, {Key::six, &ViewLayers::detection}};
    for (const auto &[key, layer] : layer_keys)
        if (input.hit(key))
            layers_.*layer = !(layers_.*layer);
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
            selected_mover_.reset();
            if (toggle)
                simulation_->toggle_producer(i);
            return;
        }
    // A right click on empty floor keeps the selection, so a goal can still follow.
    if (!toggle) {
        selected_mover_.reset();
        selected_producer_.reset();
    }
}

void FactoryController::handle_pointer(const Input &input, const Camera2D &camera) {
    if (!input.clicked && !input.right_clicked)
        return;
    const auto point = flip_y(camera.screen_to_world(input.mouse));
    if (input.down(Key::shift) && input.clicked && selected_mover_) {
        if (simulation_->move_to(*selected_mover_, point))
            clear_message();
        else
            show_for_a_while("GOAL REJECTED: MOVER BUSY, BLOCKED OR GOAL UNREACHABLE");
    } else
        pick(point, input.right_clicked);
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
    // Zero steps still clamp the zoom to its limits.
    const bool wheel_zooms = panels.debug_mode && !panels.pointer_captured;
    kinetra::zoom(camera, wheel_zooms ? input.wheel : 0, zoom_limits);
    const float step = float(std::min(elapsed, longest_pan_frame)) * pan_speed / camera.pixels_per_unit;
    camera.position += lab::direction(input) * step;
    if (!panels.pointer_captured)
        handle_pointer(input, camera);
    simulation_->advance(elapsed);
}

MenuEntry FactoryController::motion_switch(std::string_view label, bool MotionSettings::*setting) {
    return {label,
            [this, setting] { change_motion([setting](MotionSettings &m) { m.*setting = !(m.*setting); }); },
            [this, setting] { return std::string(simulation_->motion_settings().*setting ? "ON" : "OFF"); }};
}

void FactoryController::add_view_entries(std::vector<MenuEntry> &entries) {
    const auto layer = [this](std::string_view label, bool ViewLayers::*flag) {
        return MenuEntry{label,
                         [this, flag] { layers_.*flag = !(layers_.*flag); },
                         {},
                         [this, flag] { return layers_.*flag; }};
    };
    entries.push_back(layer("GRID", &ViewLayers::grid));
    entries.push_back(layer("HEATMAP", &ViewLayers::heatmap));
    entries.push_back(layer("PATHS", &ViewLayers::paths));
    entries.push_back(layer("FORCES", &ViewLayers::forces));
    entries.push_back(layer("BORDERS", &ViewLayers::borders));
    entries.push_back(layer("DETECTION", &ViewLayers::detection));
    entries.push_back(layer("CONTACT SHAPES", &ViewLayers::contact_shapes));
    entries.push_back({"PAUSE / RUN", [this] { toggle_pause(); }});
    entries.push_back({"SINGLE TICK", [this] { simulation_->single_step(); }});
    entries.push_back({"WORKLOAD",
                       [this] { simulation_->set_workload(!simulation_->workload()); },
                       {},
                       [this] { return simulation_->workload(); }});
    entries.push_back({"FRAME SCENE", [this] { fit_pending_ = true; }});
    entries.push_back({"BLUEPRINTS", [this] { close_requested_ = true; }});
    // Kept on this page so the motion page stays above the statistics on short viewports.
    entries.push_back(motion_switch("CORRIDOR TRAFFIC", &MotionSettings::corridor_traffic));
    entries.push_back(
        {"PREDICTIVE PASS",
         [this] { change_motion([](MotionSettings &m) { m.predictive_passing = !m.predictive_passing; }); },
         [this] {
             const auto &motion = simulation_->motion_settings();
             if (!supports_predictive_passing(motion.profile))
                 return std::string("LEGACY");
             return std::string(motion.predictive_passing ? "ON" : "OFF");
         }});
}

void FactoryController::add_motion_entries(std::vector<MenuEntry> &entries) {
    const auto parameter = [this](std::string_view label, std::function<void(MotionSettings &)> change) {
        return MenuEntry{label, [this, change = std::move(change)] { change_motion(change); }};
    };
    entries.push_back(
        {"PROFILE",
         [this] {
             change_motion([](MotionSettings &m) { m.profile = SteeringProfile((int(m.profile) + 1) % 4); });
         },
         [this] { return std::string(profile_name(simulation_->motion_settings().profile)); }});
    entries.push_back(
        {"PLANNER", [this] { change_motion([](MotionSettings &m) { m.heatmap = !m.heatmap; }); },
         [this] { return std::string(simulation_->motion_settings().heatmap ? "HEAT A*" : "A*"); }});
    entries.push_back(motion_switch("RECOVERY", &MotionSettings::recovery));
    entries.push_back(parameter("SPEED +", [](MotionSettings &m) { m.speed *= parameter_step; }));
    entries.push_back(parameter("SPEED -", [](MotionSettings &m) { m.speed /= parameter_step; }));
    entries.push_back(parameter("FORCE +", [](MotionSettings &m) { m.force *= parameter_step; }));
    entries.push_back(parameter("FORCE -", [](MotionSettings &m) { m.force /= parameter_step; }));
    entries.push_back(parameter(
        "HORIZON +", [](MotionSettings &m) { m.horizon = std::min(longest_horizon, m.horizon + 1); }));
    entries.push_back(parameter(
        "HORIZON -", [](MotionSettings &m) { m.horizon = std::max(shortest_horizon, m.horizon - 1); }));
    entries.push_back(parameter("RESET PARAMETERS", [](MotionSettings &m) {
        m.speed = reset_speed;
        m.force = reset_force;
        m.horizon = reset_horizon;
    }));
    entries.push_back({"UPS +", [this] { change_update_rate(true); }});
    entries.push_back({"UPS -", [this] { change_update_rate(false); }});
    entries.push_back(motion_switch("PARKING TRAFFIC", &MotionSettings::parking_traffic));
    entries.push_back(motion_switch("JUNCTION TRAFFIC", &MotionSettings::junction_traffic));
}

std::vector<MenuEntry> FactoryController::menu() {
    std::vector<MenuEntry> entries;
    entries.push_back(
        {motion_page_ ? "PAGE: MOTION" : "PAGE: VIEW", [this] { motion_page_ = !motion_page_; }});
    if (motion_page_)
        add_motion_entries(entries);
    else
        add_view_entries(entries);
    return entries;
}
} // namespace i4sim

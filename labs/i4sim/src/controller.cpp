#include "controller.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <tuple>
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

const char *planner_name(bool heatmap) {
    return heatmap ? "HEAT A*" : "A*";
}

/// A motion parameter for a menu row: three significant digits, so even the largest float
/// ends before the row's [-] button.
std::string number_text(float value) {
    std::ostringstream text;
    text << std::setprecision(3) << value;
    return text.str();
}

/// The top-level debug layers in menu order, with their row labels. The keys 1 to 6 toggle
/// the first six (handle_keys).
constexpr std::pair<std::string_view, Layer> top_layers[]{{"GRID", Layer::grid},
                                                          {"HEATMAP", Layer::heatmap},
                                                          {"PATHS", Layer::paths},
                                                          {"FORCES", Layer::forces},
                                                          {"BORDERS", Layer::borders},
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
    // Unlike R, the restart continues the show: same update rate, same run state.
    const bool was_running = simulation_->running();
    const unsigned rate = simulation_->updates_per_second();
    restart();
    if (!loaded())
        return;
    simulation_->updates_per_second(rate);
    if (was_running)
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
    restart_when_due();
}

MenuEntry FactoryController::section_heading(std::string_view label, MenuSection section) {
    return {.label = label,
            .action =
                [this, section] {
                    open_section_ = open_section_ == section ? MenuSection::none : section;
                    // A list belongs to the section it was opened in.
                    open_list_ = MenuList::none;
                },
            .kind = EntryKind::group,
            .active = [this, section] { return open_section_ == section; }};
}

MenuEntry FactoryController::list_heading(std::string_view label, MenuList list,
                                          std::function<std::string()> value) {
    return {.label = label,
            .action = [this, list] { open_list_ = open_list_ == list ? MenuList::none : list; },
            .kind = EntryKind::group,
            .active = [this, list] { return open_list_ == list; },
            .value = std::move(value),
            .depth = 1};
}

MenuEntry FactoryController::motion_check(std::string_view label, bool MotionSettings::*setting) {
    return {
        .label = label,
        .action = [this,
                   setting] { change_motion([setting](MotionSettings &m) { m.*setting = !(m.*setting); }); },
        .kind = EntryKind::check,
        .active = [this, setting] { return simulation_->motion_settings().*setting; },
        .depth = 1};
}

MenuEntry FactoryController::motion_number(std::string_view label, std::function<std::string()> value,
                                           std::function<void(MotionSettings &)> lower,
                                           std::function<void(MotionSettings &)> raise) {
    return {.label = label,
            .value = std::move(value),
            .decrease = [this, lower = std::move(lower)] { change_motion(lower); },
            .increase = [this, raise = std::move(raise)] { change_motion(raise); },
            .depth = 1};
}

void FactoryController::add_simulation_entries(std::vector<MenuEntry> &entries) {
    entries.push_back({.label = "RUNNING",
                       .action = [this] { toggle_pause(); },
                       .kind = EntryKind::check,
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
                       .kind = EntryKind::check,
                       .active = [this] { return simulation_->workload(); },
                       .depth = 1});
}

void FactoryController::add_layer_entries(std::vector<MenuEntry> &entries) {
    for (const auto &[label, layer] : top_layers)
        entries.push_back({.label = label,
                           .action = [this, layer] { layers_.toggle(layer); },
                           .kind = EntryKind::check,
                           .active = [this, layer] { return layers_.selected(layer); },
                           .depth = 1,
                           .layer = layer});
}

void FactoryController::add_motion_entries(std::vector<MenuEntry> &entries) {
    const auto motion = [this]() -> const MotionSettings & { return simulation_->motion_settings(); };
    entries.push_back(list_heading("PROFILE", MenuList::profile,
                                   [motion] { return std::string(profile_name(motion().profile)); }));
    if (open_list_ == MenuList::profile)
        for (const auto profile : {SteeringProfile::improved, SteeringProfile::original,
                                   SteeringProfile::experimental_rvo, SteeringProfile::reciprocal})
            entries.push_back({.label = profile_name(profile),
                               .action =
                                   [this, profile] {
                                       change_motion([profile](MotionSettings &m) { m.profile = profile; });
                                       open_list_ = MenuList::none;
                                   },
                               .kind = EntryKind::choice,
                               .active = [motion, profile] { return motion().profile == profile; },
                               .depth = 2});
    entries.push_back(list_heading("PLANNER", MenuList::planner,
                                   [motion] { return std::string(planner_name(motion().heatmap)); }));
    if (open_list_ == MenuList::planner)
        for (const bool heatmap : {true, false})
            entries.push_back({.label = planner_name(heatmap),
                               .action =
                                   [this, heatmap] {
                                       change_motion([heatmap](MotionSettings &m) { m.heatmap = heatmap; });
                                       open_list_ = MenuList::none;
                                   },
                               .kind = EntryKind::choice,
                               .active = [motion, heatmap] { return motion().heatmap == heatmap; },
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
        [](MotionSettings &m) { m.horizon = std::min(longest_horizon, m.horizon + 1); }));
    entries.push_back({.label = "RESET PARAMETERS",
                       .action =
                           [this] {
                               change_motion([](MotionSettings &m) {
                                   m.speed = reset_speed;
                                   m.force = reset_force;
                                   m.horizon = reset_horizon;
                               });
                           },
                       .depth = 1});
}

void FactoryController::add_traffic_entries(std::vector<MenuEntry> &entries) {
    entries.push_back(motion_check("CORRIDOR TRAFFIC", &MotionSettings::corridor_traffic));
    entries.push_back(motion_check("JUNCTION TRAFFIC", &MotionSettings::junction_traffic));
    entries.push_back(motion_check("PARKING TRAFFIC", &MotionSettings::parking_traffic));
    auto passing = motion_check("PREDICTIVE PASS", &MotionSettings::predictive_passing);
    // The legacy profiles keep the setting but ignore it; say so instead of hiding the row.
    passing.value = [this] {
        return std::string(supports_predictive_passing(simulation_->motion_settings().profile) ? ""
                                                                                               : "LEGACY");
    };
    entries.push_back(std::move(passing));
}

std::vector<MenuEntry> FactoryController::menu() {
    std::vector<MenuEntry> entries;
    using Adder = void (FactoryController::*)(std::vector<MenuEntry> &);
    const std::tuple<std::string_view, MenuSection, Adder> sections[]{
        {"SIMULATION", MenuSection::simulation, &FactoryController::add_simulation_entries},
        {"LAYERS", MenuSection::layers, &FactoryController::add_layer_entries},
        {"MOTION", MenuSection::motion, &FactoryController::add_motion_entries},
        {"TRAFFIC", MenuSection::traffic, &FactoryController::add_traffic_entries}};
    for (const auto &[label, section, add] : sections) {
        entries.push_back(section_heading(label, section));
        if (open_section_ == section)
            (this->*add)(entries);
    }
    entries.push_back({.label = "FRAME SCENE", .action = [this] { fit_pending_ = true; }});
    entries.push_back({.label = "BLUEPRINTS", .action = [this] { close_requested_ = true; }});
    return entries;
}
} // namespace i4sim

#include "demo/debug_panel.h"

#include <string>
#include <utility>

namespace demo {
namespace {
using kinetra::DebugEntry;
using kinetra::DebugEntryKind;

const char *avoidance_name(bool reciprocal) {
    return reciprocal ? "RECIPROCAL" : "LEGACY";
}

const char *wander_name(WanderMode mode) {
    return mode == WanderMode::route ? "ROUTE" : "STEERING";
}

/// A check row for a debug layer, with the color the renderer draws it in.
DebugEntry layer_row(std::string label, bool &flag, kinetra::Color color) {
    auto row = kinetra::debug_check(std::move(label), flag);
    row.swatch = color;
    return row;
}
} // namespace

void DebugPanel::add_simulation_entries(std::vector<DebugEntry> &entries) {
    entries.push_back({.label = "RUNNING",
                       .action = [this] { pause_requested_ = true; },
                       .kind = DebugEntryKind::check,
                       .active = [this] { return !application_.paused(); },
                       .depth = 1});
    entries.push_back({.label = "SINGLE TICK",
                       .action =
                           [this] {
                               // A tick is only single in a scene that stands still.
                               if (!application_.paused())
                                   pause_requested_ = true;
                               step_requested_ = true;
                           },
                       .depth = 1});
    entries.push_back(kinetra_lab::time_scale_entry(time_scale_));
    entries.push_back(kinetra::debug_check("GRAVITY", gravity_));
}

void DebugPanel::add_layer_entries(std::vector<DebugEntry> &entries) {
    entries.push_back(layer_row("VECTORS", layers_.vectors, layer_color::vectors));
    entries.push_back(layer_row("STEERING PROBES", layers_.probes, layer_color::probes));
    entries.push_back(layer_row("PATHS", layers_.paths, layer_color::paths));
    entries.push_back(layer_row("COLLISION SHAPES", layers_.collisions, layer_color::collisions));
    entries.push_back(layer_row("ACTIVE AGENTS", layers_.active_agents, layer_color::active_agents));
    entries.push_back(layer_row("HOME RANGE", layers_.home_range, layer_color::home_range));
}

void DebugPanel::add_motion_entries(std::vector<DebugEntry> &entries) {
    auto &motion = application_.tuning().motion;
    entries.push_back(folds_.list("AVOIDANCE", [this] { return std::string(avoidance_name(reciprocal_)); }));
    if (folds_.list_open("AVOIDANCE"))
        for (const bool reciprocal : {true, false})
            entries.push_back({.label = avoidance_name(reciprocal),
                               .action =
                                   [this, reciprocal] {
                                       reciprocal_ = reciprocal;
                                       folds_.close_list();
                                   },
                               .kind = DebugEntryKind::choice,
                               .active = [this, reciprocal] { return reciprocal_ == reciprocal; },
                               .depth = 2});
    entries.push_back(kinetra::debug_number("MAX SPEED", motion.max_speed, tuning_step::max_speed));
    entries.push_back(
        kinetra::debug_number("ACCELERATION", motion.max_acceleration, tuning_step::max_acceleration));
    entries.push_back(kinetra::debug_number("RESPONSE", motion.response_time, tuning_step::response_time));
    entries.push_back(kinetra::debug_number("CLEARANCE", motion.clearance, tuning_step::clearance));
    // Each model reads its own values; the other model's rows would change nothing.
    if (reciprocal_) {
        entries.push_back(kinetra::debug_number("HORIZON", motion.horizon, tuning_step::horizon));
        entries.push_back(
            kinetra::debug_number("PASSING BIAS", motion.passing_bias, tuning_step::passing_bias));
    } else {
        entries.push_back(kinetra::debug_number("LOOKAHEAD", motion.lookahead, tuning_step::lookahead));
        entries.push_back(
            kinetra::debug_number("SEPARATION", motion.separation_radius, tuning_step::separation_radius));
        entries.push_back(
            kinetra::debug_number("SEP WEIGHT", motion.separation_weight, tuning_step::separation_weight));
    }
}

void DebugPanel::add_agent_entries(std::vector<DebugEntry> &entries) {
    auto &wander = application_.tuning().wander;
    entries.push_back(folds_.list("WANDER", [&wander] { return std::string(wander_name(wander.mode)); }));
    if (folds_.list_open("WANDER"))
        for (const WanderMode mode : {WanderMode::steering, WanderMode::route})
            entries.push_back({.label = wander_name(mode),
                               .action =
                                   [this, &wander, mode] {
                                       wander.mode = mode;
                                       folds_.close_list();
                                   },
                               .kind = DebugEntryKind::choice,
                               .active = [&wander, mode] { return wander.mode == mode; },
                               .depth = 2});
    entries.push_back(kinetra::debug_number("HOME RANGE", wander.range, tuning_step::home_range));
    if (wander.mode == WanderMode::steering) {
        entries.push_back(kinetra::debug_number("WANDER REACH", wander.reach, tuning_step::wander_reach));
        entries.push_back(kinetra::debug_number("WANDER JITTER", wander.jitter, tuning_step::wander_jitter));
    }
    entries.push_back(kinetra::debug_check("CLICK SETS GOAL", click_sets_goal_));
}

void DebugPanel::reset_parameters() {
    application_.tuning() = Tuning{};
    reciprocal_ = true;
    time_scale_ = 1;
}

std::vector<DebugEntry> DebugPanel::entries() {
    std::vector<DebugEntry> entries;
    for (const std::string label : {"SIMULATION", "LAYERS", "MOTION", "AGENTS"}) {
        entries.push_back(folds_.section(label));
        if (!folds_.section_open(label))
            continue;
        if (label == "SIMULATION")
            add_simulation_entries(entries);
        else if (label == "LAYERS")
            add_layer_entries(entries);
        else if (label == "MOTION")
            add_motion_entries(entries);
        else
            add_agent_entries(entries);
    }
    entries.push_back(zoom_.entry());
    auto ghost = kinetra::debug_check("GHOST CAMERA", ghost_, 0);
    entries.push_back(std::move(ghost));
    entries.push_back({.label = "RESET PARAMETERS", .action = [this] { reset_parameters(); }});
    entries.push_back({.label = "SCENARIOS", .action = [this] { scenarios_requested_ = true; }});
    return entries;
}

void DebugPanel::apply(FrameCommands &commands, bool debug_mode, bool pointer_captured, float wheel) {
    // The ghost camera is a debug view; leaving debug mode returns the camera to the player.
    if (!debug_mode)
        ghost_ = false;
    const auto &shown = application_.presentation();
    commands.debug = debug_mode;
    commands.ghost = ghost_;
    commands.probes = debug_mode && layers_.probes;
    commands.toggle_vectors = (debug_mode && layers_.vectors) != shown.vectors;
    commands.toggle_paths = (debug_mode && layers_.paths) != shown.paths;
    const auto &scene = application_.scene();
    const bool gravity_allowed = debug_mode || scene.scenario() == Scenario::gravity;
    commands.toggle_gravity = (gravity_allowed && gravity_) != (scene.world().gravity.y != 0);
    commands.reciprocal_avoidance = reciprocal_;
    commands.time_scale = time_scale_;
    // The ZOOM row's steps are taken even outside debug mode, so none is left over for the
    // frame that switches it on.
    commands.zoom_steps = (debug_mode && !pointer_captured ? wheel : 0) + zoom_.take_steps();
    if (std::exchange(pause_requested_, false))
        commands.toggle_pause = !commands.toggle_pause;
    commands.single_step = std::exchange(step_requested_, false);
    if (pointer_captured)
        commands.pointer.reset();
    else if (commands.pointer && debug_mode && click_sets_goal_ &&
             commands.pointer->action == PointerAction::walk)
        commands.pointer->action = PointerAction::target_agents;
}
} // namespace demo

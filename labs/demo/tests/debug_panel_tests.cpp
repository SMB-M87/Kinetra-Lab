// Window-free tests for the Demo's debug menu (labs/demo/include/debug_panel.h): which rows
// it lists, what they change in the application's tuning, and which commands a frame gets
// from them.
#include "demo/debug_panel.h"

#include <doctest/doctest.h>

#include <string>
#include <string_view>
#include <vector>

namespace {
using demo::Application;
using demo::DebugPanel;
using demo::FrameCommands;
using kinetra::DebugEntryKind;

/// An application with a few agents, its debug panel and a camera at the Demo's zoom.
struct Fixture {
    Application application{8, 42};
    DebugPanel panel{application};
    kinetra::Camera2D camera;

    Fixture() {
        camera.pixels_per_unit = demo::initial_zoom;
        advance();
    }
    /// One frame as the session runs it, in debug mode, with `commands` as the frame's input.
    void advance(FrameCommands commands = {}, double elapsed = 0) {
        panel.apply(commands, true, false, 0);
        application.advance(elapsed, commands, camera);
        panel.observe(camera);
    }
    std::vector<std::string> labels() {
        std::vector<std::string> result;
        for (const auto &row : panel.entries())
            result.push_back(row.label);
        return result;
    }
    /// The row `label` of the menu as it stands.
    kinetra::DebugEntry row(std::string_view label) {
        for (auto &candidate : panel.entries())
            if (candidate.label == label)
                return candidate;
        FAIL("The debug menu has no row " << label);
        return {};
    }
    /// Opens the section `label`, unless it is open already.
    void open(std::string_view label) {
        const auto heading = row(label);
        REQUIRE(heading.kind == DebugEntryKind::group);
        if (!heading.active())
            heading.action();
    }
};

/// The commands the panel makes of a frame without input.
FrameCommands applied(DebugPanel &panel, bool debug_mode = true, bool pointer_captured = false,
                      float wheel = 0) {
    FrameCommands commands;
    panel.apply(commands, debug_mode, pointer_captured, wheel);
    return commands;
}
} // namespace

TEST_CASE("the menu starts with the simulation section open and ends with zoom ghost camera and reset") {
    Fixture f;
    CHECK(f.labels() == std::vector<std::string>{"SIMULATION", "RUNNING", "SINGLE TICK", "UPS", "FPS",
                                                 "GRAVITY", "LAYERS", "MOTION", "AGENTS", "ZOOM",
                                                 "GHOST CAMERA", "RESET PARAMETERS"});
    const auto menu = f.panel.entries();
    CHECK((menu[0].kind == DebugEntryKind::group && menu[0].active() && menu[0].depth == 0));
    CHECK(menu[1].depth == 1);
    CHECK_MESSAGE((menu[9].rule_above && menu[9].depth == 0), "a rule sets the commands apart");
    CHECK((menu[10].kind == DebugEntryKind::check && menu[10].depth == 0 && !menu[10].active()));
}

TEST_CASE("a section heading opens its rows and closes the open section") {
    Fixture f;
    f.row("LAYERS").action();
    CHECK(f.labels() == std::vector<std::string>{"SIMULATION", "LAYERS", "VECTORS", "STEERING PROBES",
                                                 "PATHS", "COLLISION SHAPES", "ACTIVE AGENTS", "HOME RANGE",
                                                 "MOTION", "AGENTS", "ZOOM", "GHOST CAMERA",
                                                 "RESET PARAMETERS"});
    f.row("LAYERS").action();
    CHECK(f.labels() == std::vector<std::string>{"SIMULATION", "LAYERS", "MOTION", "AGENTS", "ZOOM",
                                                 "GHOST CAMERA", "RESET PARAMETERS"});
}

TEST_CASE("the layer rows carry their colors and start as the session shows them") {
    Fixture f;
    f.open("LAYERS");
    // A scenario opens with VECTORS and PATHS only.
    for (const char *label : {"VECTORS", "STEERING PROBES", "PATHS", "COLLISION SHAPES", "ACTIVE AGENTS"}) {
        CAPTURE(label);
        const auto row = f.row(label);
        const bool on = std::string(label) == "VECTORS" || std::string(label) == "PATHS";
        CHECK((row.kind == DebugEntryKind::check && row.depth == 1 && row.active() == on));
        CHECK(row.swatch);
    }
    CHECK_FALSE(f.row("HOME RANGE").active());
    const auto start = f.panel.layers(true);
    CHECK((start.vectors && start.paths && !start.probes && !start.collisions && !start.active_agents &&
           !start.home_range));
    const auto paths = f.row("PATHS");
    REQUIRE(paths.swatch);
    CHECK(paths.swatch->b == demo::layer_color::paths.b);
    f.row("HOME RANGE").action();
    f.row("VECTORS").action();
    const auto layers = f.panel.layers(true);
    CHECK(layers.home_range);
    CHECK_FALSE(layers.vectors);
    const auto hidden = f.panel.layers(false);
    CHECK_MESSAGE((!hidden.home_range && !hidden.paths && !hidden.collisions && !hidden.active_agents &&
                   !hidden.probes),
                  "outside debug mode no layer is drawn");
}

TEST_CASE("the motion section lists the rows of the chosen avoidance model") {
    Fixture f;
    f.open("MOTION");
    CHECK(f.labels() == std::vector<std::string>{"SIMULATION", "LAYERS", "MOTION", "AVOIDANCE", "MAX SPEED",
                                                 "ACCELERATION", "RESPONSE", "CLEARANCE", "HORIZON",
                                                 "PASSING BIAS", "AGENTS", "ZOOM", "GHOST CAMERA",
                                                 "RESET PARAMETERS"});
    CHECK(f.row("AVOIDANCE").value() == "RECIPROCAL");
    CHECK(applied(f.panel).reciprocal_avoidance);
    f.row("AVOIDANCE").action();
    const auto legacy = f.row("LEGACY");
    CHECK((legacy.kind == DebugEntryKind::choice && legacy.depth == 2 && !legacy.active()));
    CHECK(f.row("RECIPROCAL").active());
    legacy.action();
    CHECK(f.row("AVOIDANCE").value() == "LEGACY");
    CHECK_FALSE(applied(f.panel).reciprocal_avoidance);
    CHECK(f.labels() == std::vector<std::string>{"SIMULATION", "LAYERS", "MOTION", "AVOIDANCE", "MAX SPEED",
                                                 "ACCELERATION", "RESPONSE", "CLEARANCE", "LOOKAHEAD",
                                                 "SEPARATION", "SEP WEIGHT", "AGENTS", "ZOOM", "GHOST CAMERA",
                                                 "RESET PARAMETERS"});
}

TEST_CASE("the agents section chooses the wander mode and lists the steering rows only for steering") {
    Fixture f;
    f.open("AGENTS");
    CHECK(f.labels() == std::vector<std::string>{"SIMULATION", "LAYERS", "MOTION", "AGENTS", "WANDER",
                                                 "HOME RANGE", "WANDER REACH", "WANDER JITTER",
                                                 "CLICK SETS GOAL", "PASSING", "ZOOM", "GHOST CAMERA",
                                                 "RESET PARAMETERS"});
    CHECK(f.row("WANDER").value() == "STEERING");
    f.row("WANDER").action();
    f.row("ROUTE").action();
    CHECK(f.application.tuning().wander.mode == demo::WanderMode::route);
    CHECK(f.row("WANDER").value() == "ROUTE");
    CHECK(f.labels() == std::vector<std::string>{"SIMULATION", "LAYERS", "MOTION", "AGENTS", "WANDER",
                                                 "HOME RANGE", "CLICK SETS GOAL", "PASSING", "ZOOM",
                                                 "GHOST CAMERA", "RESET PARAMETERS"});
}

TEST_CASE("PASSING starts off, reaches the scene through the commands and is reset by RESET PARAMETERS") {
    Fixture f;
    f.open("AGENTS");
    const auto passing = f.row("PASSING");
    CHECK((passing.kind == DebugEntryKind::check && passing.depth == 1 && !passing.active()));
    CHECK_FALSE(applied(f.panel).passing);
    passing.action();
    CHECK(f.panel.passing());
    const auto commands = applied(f.panel);
    CHECK(commands.passing);
    kinetra::Camera2D camera;
    f.application.advance(0, commands, camera);
    CHECK(f.application.scene().passing());
    f.row("RESET PARAMETERS").action();
    CHECK_FALSE(f.panel.passing());
    CHECK_FALSE(applied(f.panel).passing);
}

TEST_CASE("every tuning row changes its value and stops at a value the scene can run with") {
    Fixture f;
    f.open("MOTION");
    f.row("MAX SPEED").increase();
    CHECK(f.application.tuning().motion.max_speed == 5);
    CHECK(f.row("MAX SPEED").value() == "5");
    f.row("CLEARANCE").decrease();
    CHECK(f.application.tuning().motion.clearance == 0);
    // However often a row is clicked, the tuning stays valid and the scene keeps running.
    const auto exhaust = [&](std::initializer_list<const char *> labels) {
        for (const char *label : labels)
            for (const bool up : {true, false}) {
                for (int click = 0; click < 200; ++click)
                    up ? f.row(label).increase() : f.row(label).decrease();
                CAPTURE(label);
                CAPTURE(up);
                CHECK_NOTHROW(demo::validate(f.application.tuning()));
                CHECK_NOTHROW(f.advance({}, 1.0 / 60));
            }
    };
    exhaust({"MAX SPEED", "ACCELERATION", "RESPONSE", "CLEARANCE", "HORIZON", "PASSING BIAS"});
    f.row("AVOIDANCE").action();
    f.row("LEGACY").action();
    exhaust({"LOOKAHEAD", "SEPARATION", "SEP WEIGHT"});
    f.open("AGENTS");
    exhaust({"HOME RANGE", "WANDER REACH", "WANDER JITTER"});
    CHECK(f.application.tuning().wander.range == demo::tuning_step::home_range.minimum);
}

TEST_CASE("reset parameters restores the tuning, the avoidance model and the time scale") {
    Fixture f;
    f.open("MOTION");
    f.row("MAX SPEED").increase();
    f.row("AVOIDANCE").action();
    f.row("LEGACY").action();
    f.open("AGENTS");
    f.row("HOME RANGE").increase();
    f.row("WANDER").action();
    f.row("ROUTE").action();
    f.open("SIMULATION");
    f.row("UPS").increase();
    REQUIRE(f.panel.ups() == 2 * demo::real_time_ups);
    f.row("RESET PARAMETERS").action();
    const demo::Tuning defaults;
    const auto &tuning = f.application.tuning();
    CHECK(tuning.motion.max_speed == defaults.motion.max_speed);
    CHECK(tuning.wander.range == defaults.wander.range);
    CHECK(tuning.wander.mode == defaults.wander.mode);
    CHECK(f.panel.reciprocal_avoidance());
    CHECK(f.panel.ups() == demo::real_time_ups);
    CHECK(applied(f.panel).ups == demo::real_time_ups);
}

TEST_CASE("running pauses and resumes the scene and a single tick runs exactly one") {
    Fixture f;
    CHECK(f.row("RUNNING").active());
    f.row("RUNNING").action();
    CHECK_MESSAGE(!f.application.paused(), "a row acts with the frame's commands, not at once");
    f.advance();
    CHECK(f.application.paused());
    CHECK_FALSE(f.row("RUNNING").active());
    f.advance();
    CHECK_MESSAGE(f.application.paused(), "one click toggles once");
    f.row("RUNNING").action();
    f.advance();
    CHECK_FALSE(f.application.paused());

    // A single tick pauses a running scene and moves it by one tick, whatever time passes.
    FrameCommands walk;
    walk.player.direction = {1, 0};
    const auto &player = f.application.scene().world().body(f.application.scene().player());
    f.row("SINGLE TICK").action();
    const float before = player.position.x;
    f.advance(walk, 1.0);
    CHECK(f.application.paused());
    const float after_tick = player.position.x;
    CHECK(after_tick > before);
    f.advance(walk, 1.0);
    CHECK_MESSAGE(player.position.x == after_tick, "a paused scene moved without a single tick");
    f.row("SINGLE TICK").action();
    f.advance(walk, 1.0);
    CHECK(player.position.x > after_tick);
    CHECK(f.application.paused());
}

TEST_CASE("the UPS row sets the ticks per second of the frame's commands") {
    Fixture f;
    CHECK(applied(f.panel).ups == demo::real_time_ups);
    f.row("UPS").decrease();
    f.row("UPS").decrease();
    CHECK(applied(f.panel).ups == 30);
    CHECK(f.row("UPS").value() == "30");
    f.row("UPS").set(250000);
    CHECK(applied(f.panel).ups == 250000);
    CHECK_MESSAGE(applied(f.panel, false).ups == 250000, "the UPS holds outside debug mode");
}

TEST_CASE("the FPS row follows the UPS row and changes only the frame-rate setting") {
    Fixture f;
    const auto entries = f.panel.entries();
    const auto ups =
        std::find_if(entries.begin(), entries.end(), [](const auto &e) { return e.label == "UPS"; });
    REQUIRE(ups != entries.end());
    REQUIRE(ups + 1 != entries.end());
    CHECK((ups + 1)->label == "FPS");
    f.row("FPS").set(30);
    CHECK(f.panel.frame_rate().mode() == kinetra::FrameRateMode::limit);
    CHECK(f.panel.frame_rate().limit() == 30);
    f.row("RESET PARAMETERS").action();
    CHECK_MESSAGE(f.panel.frame_rate().limit() == 30, "the frame rate is a view setting, not a parameter");
}

TEST_CASE("layers gravity and the ghost camera reach the commands only in debug mode") {
    Fixture f;
    f.row("GRAVITY").action();
    f.row("GHOST CAMERA").action();
    f.advance();
    CHECK(f.application.scene().world().gravity.y != 0);
    CHECK_FALSE(f.application.scene().following());
    CHECK(f.application.presentation().vectors);
    // Leaving debug mode: no layers, no gravity, and the camera returns to the player.
    FrameCommands commands;
    f.panel.apply(commands, false, false, 0);
    CHECK_FALSE(commands.ghost);
    CHECK_FALSE(commands.probes);
    f.application.advance(0, commands, f.camera);
    CHECK(f.application.scene().world().gravity.y == 0);
    CHECK(f.application.scene().following());
    CHECK_FALSE(f.application.presentation().vectors);
    CHECK_FALSE(f.application.presentation().paths);
    CHECK_MESSAGE(!f.row("GHOST CAMERA").active(), "the ghost camera is switched off, not only hidden");
    // Back in debug mode the layers and gravity return; the ghost camera stays off.
    f.advance();
    CHECK(f.application.presentation().vectors);
    CHECK(f.application.scene().world().gravity.y != 0);
    CHECK(f.application.scene().following());
}

TEST_CASE("the wheel and the zoom row zoom in debug mode and not over a panel") {
    Fixture f;
    CHECK(applied(f.panel, true, false, 2).zoom_steps == 2);
    CHECK_MESSAGE(applied(f.panel, false, false, 2).zoom_steps == 0, "the wheel zooms only in debug mode");
    CHECK_MESSAGE(applied(f.panel, true, true, 2).zoom_steps == 0, "the wheel does not zoom over a panel");
    CHECK(f.row("ZOOM").value() == "17");
    f.row("ZOOM").increase();
    f.row("ZOOM").increase();
    CHECK_MESSAGE(applied(f.panel, true, true, 1).zoom_steps == 2, "the row's steps count over the panel");
    CHECK_MESSAGE(applied(f.panel).zoom_steps == 0, "steps are used once");
    f.row("ZOOM").decrease();
    f.advance();
    CHECK(f.camera.pixels_per_unit == doctest::Approx(demo::initial_zoom / kinetra::zoom_factor));
    CHECK(f.row("ZOOM").value() == kinetra::debug_number_text(f.camera.pixels_per_unit));
}

TEST_CASE("a captured pointer is dropped and click sets goal turns a walk into goals for the agents") {
    Fixture f;
    FrameCommands click;
    click.pointer = demo::PointerCommand{demo::PointerAction::walk, {100, 100}};
    FrameCommands captured = click;
    f.panel.apply(captured, true, true, 0);
    CHECK_FALSE(captured.pointer);
    FrameCommands walk = click;
    f.panel.apply(walk, true, false, 0);
    REQUIRE(walk.pointer);
    CHECK(walk.pointer->action == demo::PointerAction::walk);
    f.open("AGENTS");
    f.row("CLICK SETS GOAL").action();
    FrameCommands goal = click;
    f.panel.apply(goal, true, false, 0);
    REQUIRE(goal.pointer);
    CHECK(goal.pointer->action == demo::PointerAction::target_agents);
    FrameCommands outside = click;
    f.panel.apply(outside, false, false, 0);
    REQUIRE(outside.pointer);
    CHECK_MESSAGE(outside.pointer->action == demo::PointerAction::walk,
                  "outside debug mode a click walks the player");
    FrameCommands clear;
    clear.pointer = demo::PointerCommand{demo::PointerAction::clear_selection, {100, 100}};
    f.panel.apply(clear, true, false, 0);
    CHECK_MESSAGE(clear.pointer->action == demo::PointerAction::clear_selection,
                  "a right click still clears the selection");
}

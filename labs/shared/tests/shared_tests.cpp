// Window-free tests for the policy shared by the Lab applications
// (labs/shared/include/kinetra_lab/): input mapping and modal pause (input.h), the debug-menu rows
// every application shows (debug.h) and the settings rows (settings.h).
#include "kinetra_lab/debug.h"
#include "kinetra_lab/input.h"
#include "kinetra_lab/settings.h"

#include <doctest/doctest.h>

#include <cstddef>
#include <initializer_list>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace {
kinetra::Input holding(std::initializer_list<kinetra::Key> keys) {
    kinetra::Input input;
    for (auto key : keys)
        input.held[static_cast<std::size_t>(key)] = true;
    return input;
}
} // namespace

TEST_CASE("wasd and arrows map to the same world direction") {
    using kinetra::Key;
    CHECK(kinetra_lab::direction(holding({Key::d})).x == 1);
    CHECK(kinetra_lab::direction(holding({Key::right})).x == 1);
    CHECK(kinetra_lab::direction(holding({Key::a})).x == -1);
    CHECK(kinetra_lab::direction(holding({Key::left})).x == -1);
    CHECK(kinetra_lab::direction(holding({Key::w})).y == 1);
    CHECK(kinetra_lab::direction(holding({Key::up})).y == 1);
    CHECK(kinetra_lab::direction(holding({Key::s})).y == -1);
    CHECK(kinetra_lab::direction(holding({Key::down})).y == -1);
}

TEST_CASE("opposite keys cancel and diagonals stay unnormalized") {
    using kinetra::Key;
    const auto cancelled = kinetra_lab::direction(holding({Key::a, Key::d, Key::w, Key::down}));
    CHECK(cancelled.x == 0);
    CHECK(cancelled.y == 0);
    const auto diagonal = kinetra_lab::direction(holding({Key::d, Key::up}));
    CHECK(diagonal.x == 1);
    CHECK(diagonal.y == 1);
    CHECK_MESSAGE(kinetra_lab::direction(holding({Key::d, Key::right})).x == 1,
                  "a key and its arrow do not add up");
}

TEST_CASE("a modal panel pauses and then restores a running application") {
    kinetra_lab::ModalPause pause;
    CHECK_FALSE(pause.update(false, false));
    CHECK(pause.update(true, false));
    CHECK_MESSAGE(pause.update(true, true), "the application stays paused while the panel is open");
    CHECK_FALSE(pause.update(false, true));
}

TEST_CASE("a modal panel keeps an application paused that was paused before") {
    kinetra_lab::ModalPause pause;
    CHECK(pause.update(true, true));
    CHECK(pause.update(false, true));
}

TEST_CASE("without a panel the pause state passes through") {
    kinetra_lab::ModalPause pause;
    CHECK(pause.update(false, true));
    CHECK_FALSE(pause.update(false, false));
}

TEST_CASE("the settings rows come in Lab's order without a zoom row") {
    bool statistics{}, debug{}, frame_times{}, frame_limit{};
    const auto rows =
        kinetra_lab::settings_rows(statistics, debug, frame_times, frame_limit, {"MOVE"}, {"TELEPORT"});
    REQUIRE(rows.size() == 6);
    const std::pair<const char *, bool *> toggles[]{{"STATISTICS", &statistics},
                                                    {"DEBUG MODE", &debug},
                                                    {"FRAME TIMES", &frame_times},
                                                    {"FRAME LIMIT", &frame_limit}};
    for (std::size_t i = 0; i < 4; ++i) {
        const auto *toggle = std::get_if<kinetra::ToggleRow>(&rows[i]);
        REQUIRE(toggle);
        CHECK(toggle->label == toggles[i].first);
        CHECK(toggle->value == toggles[i].second);
    }
    const auto &sim = std::get<kinetra::HelpSection>(rows[4]);
    CHECK(sim.label == "SIM CONTROLS");
    CHECK(sim.lines == std::vector<std::string>{"MOVE"});
    const auto &controls = std::get<kinetra::HelpSection>(rows[5]);
    CHECK(controls.label == "DEBUG CONTROLS");
    const std::vector<std::string> expected{kinetra_lab::debug_menu_help, kinetra_lab::wheel_zoom_help,
                                            "TELEPORT"};
    CHECK_MESSAGE(controls.lines == expected, "the shared lines come before the application's");
}

TEST_CASE("the zoom row collects the steps of its buttons and shows the observed zoom") {
    kinetra_lab::ZoomControl zoom;
    auto row = zoom.entry();
    CHECK(row.label == "ZOOM");
    CHECK(row.depth == 0);
    CHECK(row.rule_above);
    CHECK_MESSAGE(!row.action, "the row itself is not a command");
    CHECK(zoom.take_steps() == 0);
    row.increase();
    row.increase();
    row.decrease();
    CHECK(zoom.take_steps() == 1);
    CHECK_MESSAGE(zoom.take_steps() == 0, "steps are handed out once");
    kinetra::Camera2D camera;
    camera.pixels_per_unit = 17;
    zoom.observe(camera);
    CHECK(row.value() == "17");
    camera.pixels_per_unit = 0.25f;
    zoom.observe(camera);
    CHECK(row.value() == "0.25");
}

TEST_CASE("the speed row halves and doubles the time scale within its limits") {
    float scale = 1;
    auto row = kinetra_lab::time_scale_entry(scale);
    CHECK(row.label == "SPEED");
    CHECK(row.depth == 1);
    CHECK(row.value() == "1X");
    row.decrease();
    CHECK(scale == 0.5f);
    CHECK(row.value() == "0.5X");
    for (int i = 0; i < 10; ++i)
        row.decrease();
    CHECK(scale == kinetra_lab::slowest_time_scale);
    for (int i = 0; i < 20; ++i)
        row.increase();
    CHECK(scale == kinetra_lab::fastest_time_scale);
    CHECK(row.value() == "4X");
}

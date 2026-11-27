// Window-free tests for the policy shared by the Lab applications
// (labs/shared/include/kinetra_lab/): input mapping and modal pause (input.h), the debug-menu rows
// every application shows (debug.h), the settings rows (settings.h) and the start of a route
// drawn over its own body (route.h).
#include "kinetra_lab/debug.h"
#include "kinetra_lab/input.h"
#include "kinetra_lab/route.h"
#include "kinetra_lab/settings.h"

#include <doctest/doctest.h>

#include <cmath>
#include <cstddef>
#include <cstdint>
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

TEST_CASE("an input without keys keeps the pointer and drops keys and text") {
    kinetra::Input input;
    input.held[std::size_t(kinetra::Key::w)] = input.pressed[std::size_t(kinetra::Key::one)] = true;
    input.text = "1";
    input.mouse = {3, 4};
    input.clicked = true;
    input.wheel = 1;
    const auto stripped = kinetra_lab::without_keys(input);
    CHECK_FALSE(stripped.down(kinetra::Key::w));
    CHECK_FALSE(stripped.hit(kinetra::Key::one));
    CHECK(stripped.text.empty());
    CHECK(stripped.mouse == kinetra::Vec2{3, 4});
    CHECK(stripped.clicked);
    CHECK(stripped.wheel == 1);
}

TEST_CASE("the settings rows come in Lab's order without a zoom row") {
    bool statistics{}, debug{}, frame_times{};
    const auto rows = kinetra_lab::settings_rows(statistics, debug, frame_times, {"MOVE"}, {"TELEPORT"});
    REQUIRE(rows.size() == 5);
    const std::pair<const char *, bool *> toggles[]{
        {"STATISTICS", &statistics}, {"DEBUG MODE", &debug}, {"FRAME TIMES", &frame_times}};
    for (std::size_t i = 0; i < 3; ++i) {
        const auto *toggle = std::get_if<kinetra::ToggleRow>(&rows[i]);
        REQUIRE(toggle);
        CHECK(toggle->label == toggles[i].first);
        CHECK(toggle->value == toggles[i].second);
    }
    const auto &sim = std::get<kinetra::HelpSection>(rows[3]);
    CHECK(sim.label == "SIM CONTROLS");
    CHECK(sim.lines == std::vector<std::string>{"MOVE"});
    const auto &controls = std::get<kinetra::HelpSection>(rows[4]);
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

TEST_CASE("the UPS row halves, doubles and takes a typed rate within Lab's limits") {
    std::uint64_t ups = 120;
    auto row = kinetra_lab::ups_entry(ups);
    CHECK(row.label == "UPS");
    CHECK(row.depth == 1);
    CHECK(row.value() == "120");
    row.decrease();
    CHECK(ups == 60);
    row.set(750);
    row.increase();
    CHECK(ups == 1500);
    for (int i = 0; i < 20; ++i)
        row.decrease();
    CHECK(ups == kinetra_lab::slowest_ups);
    row.set(123456789);
    CHECK(ups == kinetra_lab::fastest_ups);
    CHECK(row.value() == "1000000");
}

TEST_CASE("a route's start ends where the route first leaves its body") {
    using kinetra::Vec2;
    const std::vector<Vec2> route{{0.25f, 0}, {0.25f, 3}, {5, 3}};
    const auto points = kinetra_lab::route_start({0, 0}, route, 1);
    REQUIRE(points.size() == 3);
    CHECK(points[0].x == 0);
    CHECK(points[0].y == 0);
    CHECK(points[1].x == 0.25f);
    CHECK(points[1].y == 0);
    // On the circle of radius 1: x stays 0.25, y = sqrt(1 - 0.25^2).
    CHECK(points[2].x == doctest::Approx(0.25f));
    CHECK(points[2].y == doctest::Approx(std::sqrt(1 - 0.0625f)));
}

TEST_CASE("a route's start keeps a route that stays inside its body") {
    using kinetra::Vec2;
    const std::vector<Vec2> inside{{2.5f, 2}, {2, 2.5f}};
    const auto points = kinetra_lab::route_start({2, 2}, inside, 1);
    REQUIRE(points.size() == 3);
    CHECK(points.back().x == 2);
    CHECK(points.back().y == 2.5f);
}

TEST_CASE("a route's start stops at the first exit of a route that comes back") {
    using kinetra::Vec2;
    const std::vector<Vec2> out_and_back{{3, 0}, {0, 0.2f}};
    const auto points = kinetra_lab::route_start({0, 0}, out_and_back, 1);
    REQUIRE(points.size() == 2);
    CHECK(points[1].x == doctest::Approx(1));
    CHECK(points[1].y == doctest::Approx(0));
}

TEST_CASE("a route's start ends at a waypoint on its body's edge without repeating it") {
    using kinetra::Vec2;
    const std::vector<Vec2> route{{0, -1}, {0, -4}};
    const auto points = kinetra_lab::route_start({0, 0}, route, 1);
    REQUIRE(points.size() == 2);
    CHECK(points[1].x == 0);
    CHECK(points[1].y == -1);
}

TEST_CASE("a route's start is only its first point without a route or a body") {
    using kinetra::Vec2;
    CHECK(kinetra_lab::route_start({1, 1}, {}, 1).size() == 1);
    CHECK(kinetra_lab::route_start({1, 1}, std::vector<Vec2>{{1.1f, 1}}, 0).size() == 1);
}

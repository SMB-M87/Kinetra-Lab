// Window-free tests for the shared settings overlay (labs/settings.h) and its i4sim
// copy (labs/i4sim/include/factory_settings.h, removed in roadmap phase 3).
// Every case runs on four layouts: two sim-control heights times two viewports.
#include "../../labs/i4sim/include/factory_settings.h"
#include "../../labs/settings.h"

#include <doctest/doctest.h>

#include <array>
#include <cstddef>
#include <functional>

namespace {
/// One settings overlay with its camera and the flags it toggles, for one layout.
struct Harness {
    lab::Settings settings;
    kinetra::Camera2D camera;
    kinetra::Vec2 viewport;
    float controls_height;
    bool statistics = false;
    bool debug = false;

    Harness(float height, kinetra::Vec2 size)
        : settings{.sim_controls_height = height}, viewport(size), controls_height(height) {
        camera.viewport = size;
        camera.pixels_per_unit = 25;
    }

    /// Feeds one frame of input; zoom is bounded to 25..50 pixels per unit.
    bool update(const kinetra::Input &input) { return settings.update(input, camera, statistics, debug, 25, 50); }

    /// Clicks at a position in the panel's own layout coordinates (centered modal).
    bool click(float x, float y) {
        kinetra::Input input;
        input.clicked = true;
        input.mouse = viewport / 2 + (kinetra::Vec2{x, y} - settings.half_extent()) * lab::Settings::scale(viewport);
        return update(input);
    }

    bool press(kinetra::Key key) {
        kinetra::Input input;
        input.pressed[std::size_t(key)] = true;
        return update(input);
    }

    /// Opens the modal through the settings button in the top-left corner.
    bool open_with_button() {
        kinetra::Input input;
        input.clicked = true;
        input.mouse = {10, 10};
        return update(input);
    }
};

/// Runs `check` once per layout, with the layout recorded for failure messages.
void for_each_layout(const std::function<void(Harness &)> &check) {
    for (float controls_height : {42.f, 28.f})
        for (auto viewport : {kinetra::Vec2{1280, 800}, kinetra::Vec2{160, 120}}) {
            CAPTURE(controls_height);
            CAPTURE(viewport.x);
            Harness harness(controls_height, viewport);
            check(harness);
        }
}

constexpr float controls_toggle_y = 134;
constexpr float button_column_x = 92;
} // namespace

TEST_CASE_TEMPLATE("debug button opens, captures and closes the debug panel", Settings, lab::Settings,
                   i4sim::FactorySettings) {
    Settings settings;
    kinetra::Camera2D camera;
    camera.viewport = {160, 120};
    kinetra::DebugMenu menu;
    bool enabled = false;
    std::array<kinetra::DebugOption, 1> options{{{"TEST", &enabled}}};
    kinetra::Input input;
    input.clicked = input.mouse_down = true;
    input.mouse = {80, 12};
    CHECK(settings.update_debug(input, camera, menu, options));
    CHECK(menu.visible);
    CHECK_FALSE(enabled);

    INFO("the debug panel leaves room for the top buttons");
    CHECK(menu.origin(camera.viewport).x == 8);
    CHECK(menu.origin(camera.viewport).y == 32);

    input.clicked = false;
    CHECK(settings.update_debug(input, camera, menu, options));
    CHECK_MESSAGE(menu.visible, "holding Debug must not repeat the toggle");

    input.clicked = true;
    input.mouse = menu.origin(camera.viewport) + kinetra::Vec2{10, 28} * menu.scale(camera.viewport);
    CHECK(settings.update_debug(input, camera, menu, options));
    CHECK_MESSAGE(enabled, "picking follows the upper-left panel");

    input.mouse = {110, 12};
    CHECK(settings.update_debug(input, camera, menu, options));
    CHECK_FALSE(menu.visible);
}

TEST_CASE("gameplay keys do not open settings") {
    for_each_layout([](Harness &h) {
        kinetra::Input input;
        input.pressed[std::size_t(kinetra::Key::space)] = true;
        input.pressed[std::size_t(kinetra::Key::m)] = true;
        CHECK_FALSE(h.update(input));
        CHECK_FALSE(h.settings.visible);
        CHECK_FALSE(h.settings.return_requested);

        input = {};
        input.pressed[std::size_t(kinetra::Key::h)] = true;
        input.pressed[std::size_t(kinetra::Key::escape)] = true;
        CHECK_FALSE(h.update(input));
    });
}

TEST_CASE("the settings button opens the modal with controls hidden") {
    for_each_layout([](Harness &h) {
        CHECK(h.open_with_button());
        CHECK(h.settings.visible);
        CHECK_FALSE(h.settings.controls_visible);
    });
}

TEST_CASE("control sections expand and collapse independently with fixed spacing") {
    for_each_layout([](Harness &h) {
        REQUIRE(h.open_with_button());
        const auto collapsed = h.settings.half_extent();

        h.click(button_column_x, controls_toggle_y);
        CHECK(h.settings.controls_visible);
        CHECK(h.settings.half_extent().y > collapsed.y);
        INFO("sim controls reserve exactly controls_height");
        CHECK(h.settings.debug_toggle_y() == 156 + h.controls_height);
        CHECK(h.settings.half_extent().y == collapsed.y + h.controls_height / 2);

        h.click(button_column_x, controls_toggle_y);
        CHECK_FALSE(h.settings.controls_visible);

        h.click(button_column_x, h.settings.debug_toggle_y());
        CHECK(h.settings.debug_controls_visible);
        CHECK_FALSE(h.settings.controls_visible);

        h.click(button_column_x, controls_toggle_y);
        CHECK(h.settings.controls_visible);
        CHECK(h.settings.debug_controls_visible);
        const float last_sim_y = h.controls_height == 42 ? 184.f : 170.f;
        CHECK_MESSAGE(h.settings.debug_toggle_y() - last_sim_y == 14,
                      "sim controls end one row before the debug toggle");
        CHECK_MESSAGE(h.settings.actions_y() - (h.settings.debug_toggle_y() + 28) == 28,
                      "debug controls keep the spacing before the launcher actions");

        h.click(button_column_x, h.settings.debug_toggle_y());
        CHECK_FALSE(h.settings.debug_controls_visible);
        CHECK_MESSAGE(h.settings.controls_visible, "hiding debug controls keeps sim controls open");
    });
}

TEST_CASE("zoom buttons stay within the configured bounds") {
    for_each_layout([](Harness &h) {
        REQUIRE(h.open_with_button());
        CHECK(h.click(56, 46));
        CHECK(h.camera.pixels_per_unit > 25);
        for (int i = 0; i < 30; ++i)
            h.click(56, 46);
        CHECK(h.camera.pixels_per_unit == 50);
        for (int i = 0; i < 30; ++i)
            h.click(124, 46);
        CHECK(h.camera.pixels_per_unit == 25);
    });
}

TEST_CASE("statistics and debug toggles switch their flags") {
    for_each_layout([](Harness &h) {
        REQUIRE(h.open_with_button());
        h.click(40, 74);
        h.click(40, 102);
        CHECK(h.statistics);
        CHECK(h.debug);
    });
}

TEST_CASE("resume closes the modal and captures the click") {
    for_each_layout([](Harness &h) {
        REQUIRE(h.open_with_button());
        CHECK(h.click(106, h.settings.actions_y()));
        CHECK_FALSE(h.settings.visible);
    });
}

TEST_CASE("the settings button does not overlap the debug controls") {
    for_each_layout([](Harness &h) {
        kinetra::Input input;
        input.clicked = true;
        const float debug_scale = kinetra::DebugMenu::scale(h.viewport);
        input.mouse = {h.viewport.x - 180 * debug_scale, 36 * debug_scale};
        CHECK_FALSE(h.update(input));
        CHECK_FALSE(h.settings.visible);
    });
}

TEST_CASE("CTRL toggles settings without leaking input") {
    for_each_layout([](Harness &h) {
        CHECK(h.press(kinetra::Key::control));
        CHECK(h.settings.visible);
        CHECK(h.press(kinetra::Key::control));
        CHECK_FALSE(h.settings.visible);
    });
}

TEST_CASE("launcher and exit actions raise their requests") {
    for_each_layout([](Harness &h) {
        REQUIRE(h.press(kinetra::Key::control));
        h.click(50, h.settings.actions_y());
        CHECK(h.settings.return_requested);
        h.click(146, h.settings.actions_y());
        CHECK(h.settings.exit_requested);
    });
}

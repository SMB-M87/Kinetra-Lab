#include "session.h"

#include "blueprint_screen.h"
#include "controller.h"
#include "hud.h"
#include "world_view.h"

#include <kinetra/debug_view.h>
#include <kinetra/settings_view.h>
#include <lab/settings.h>

#include <string>
#include <utility>
#include <vector>

namespace i4sim {
namespace {
using namespace kinetra;

/// No debug layer at all: what the world view draws outside debug mode.
constexpr ViewLayers hidden_layers{false, false, false, false, false, false, false};
// A layer added to ViewLayers starts on; it must be turned off here as well.
static_assert(sizeof(ViewLayers) == 7 * sizeof(bool), "hidden_layers must turn every layer off");

/// The controller's menu entries as rows of the engine's debug menu.
///
/// A check entry becomes a button whose label starts with the box, "[X] NAME", which is
/// the text the menu draws for a check box. The entry's action then flips the state where
/// it lives, so the menu needs no flag to point at. The rows view `labels`: build the rows
/// anew for every menu update and every draw, and do not copy them.
struct DebugRows {
    std::vector<std::string> labels;
    std::vector<DebugOption> options;

    explicit DebugRows(std::vector<MenuEntry> entries) {
        labels.reserve(entries.size());
        options.reserve(entries.size());
        for (auto &entry : entries) {
            labels.push_back(entry.checked ? (entry.checked() ? "[X] " : "[ ] ") + std::string(entry.label)
                                           : std::string(entry.label));
            options.push_back({labels.back(), nullptr, std::move(entry.action), std::move(entry.value)});
        }
    }
    DebugRows(const DebugRows &) = delete;
    DebugRows &operator=(const DebugRows &) = delete;
};

/// The graphical i4sim session: it orders a frame and owns the panels. What a frame decides
/// is the controller's; what it shows is drawn by the blueprint screen, the world view and
/// the HUD.
class FactorySession final : public lab::Session {
  public:
    FactorySession(std::uint32_t seed, const std::filesystem::path &directory,
                   const std::optional<std::filesystem::path> &initial, bool running, unsigned ups)
        : controller_(seed, running, ups), screen_(directory) {
        refresh_catalog();
        if (initial)
            controller_.load(*initial);
    }

    void advance(double elapsed, const Input &input, Camera2D &camera) override {
        if (!controller_.loaded()) {
            advance_screen(input, camera.viewport);
            return;
        }
        if (!controller_.begin_frame(elapsed))
            return;
        // The panels come first: an open settings panel takes the whole frame, and the
        // debug menu takes the pointer while it is over the menu.
        const bool modal = settings_.update(input, camera);
        controller_.suspend(settings_.visible());
        if (modal || !debug_)
            menu_.visible = false;
        if (modal)
            return;
        DebugRows rows(controller_.menu());
        const bool captured = debug_ && settings_.update_debug(input, camera.viewport, menu_, rows.options);
        controller_.handle_input(elapsed, input, camera, {captured, debug_});
    }

    void render(Runtime &runtime) override {
        if (!controller_.loaded()) {
            screen_.draw(runtime, controller_.message());
            return;
        }
        controller_.update_camera(runtime.camera);
        const auto &simulation = controller_.simulation();
        runtime.begin(world_background);
        // Outside debug mode the layers keep their selection but are not drawn.
        draw_world(runtime, simulation, debug_ ? controller_.layers() : hidden_layers,
                   controller_.selected_mover());
        draw_hud(runtime, simulation,
                 {statistics_, controller_.selected_mover(), controller_.selected_producer(),
                  controller_.description(), controller_.message()});
        const DebugRows rows(controller_.menu());
        menu_.draw(runtime, rows.options);
        if (frame_times_)
            draw_frame_times(runtime, runtime.frame_times());
        settings_.draw(runtime);
        runtime.end();
    }

    SessionRequest request() const override {
        if (exit_ || settings_.request() == SessionRequest::exit)
            return SessionRequest::exit;
        if (leave_ || settings_.request() == SessionRequest::menu)
            return SessionRequest::menu;
        return SessionRequest::none;
    }

    std::string title() override { return "i4sim"; }

    std::string status() override {
        if (!controller_.loaded())
            return controller_.message().empty() ? "Loadscreen" : "Loadscreen | " + controller_.message();
        const auto &simulation = controller_.simulation();
        return simulation.blueprint().name + (simulation.running() ? " | RUNNING" : " | PAUSED");
    }

  private:
    void refresh_catalog() {
        try {
            screen_.refresh();
            controller_.clear_message();
        } catch (const std::exception &e) {
            controller_.report_error(e.what());
        }
    }

    void advance_screen(const Input &input, Vec2 viewport) {
        const auto event = screen_.update(input, viewport);
        switch (event.action) {
        case ScreenAction::leave:
            leave_ = true;
            break;
        case ScreenAction::exit:
            exit_ = true;
            break;
        case ScreenAction::load:
            controller_.load(event.blueprint);
            break;
        case ScreenAction::rediscover:
            refresh_catalog();
            break;
        case ScreenAction::none:
            break;
        }
    }

    FactoryController controller_;
    BlueprintScreen screen_;
    /// Requests from the blueprint screen: back to the launcher, and end the program.
    bool leave_{}, exit_{};
    DebugMenu menu_;
    bool statistics_{true}, debug_{true}, frame_times_{};
    // Declared after the flags it borrows.
    SettingsOverlay settings_{lab::settings_rows([] { return zoom_limits; }, statistics_, debug_,
                                                 frame_times_,
                                                 {"WASD / ARROWS  MOVE", "CLICK SELECT / RIGHT BLOCK",
                                                  "SHIFT CLICK FREE MOVER GOAL", "SPACE PAUSE / R RESET"},
                                                 {"DEBUG PAGES: VIEW / MOTION"}),
                              &debug_};
};
} // namespace

std::unique_ptr<lab::Session> create_session(std::uint32_t seed, std::filesystem::path directory,
                                             std::optional<std::filesystem::path> initial, bool running,
                                             unsigned ups, std::optional<std::filesystem::path>) {
    return std::make_unique<FactorySession>(seed, directory, initial, running, ups);
}
} // namespace i4sim

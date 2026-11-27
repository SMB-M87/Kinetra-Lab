#include "i4sim/presentation/session.h"

#include "i4sim/controller.h"
#include "i4sim/presentation/detail/blueprint_screen.h"
#include "i4sim/presentation/detail/hud.h"
#include "i4sim/presentation/detail/world_view.h"
#include "i4sim/simulation/inspector.h"

#include "kinetra_lab/inspector.h"
#include "kinetra_lab/settings.h"
#include <kinetra/runtime/debug_view.h>
#include <kinetra/runtime/settings_view.h>

#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace i4sim {
namespace {
using namespace kinetra;

/// The graphical i4sim session: it orders a frame and owns the panels. What a frame decides
/// is the controller's; what it shows is drawn by the blueprint screen, the world view and
/// the HUD.
class FactorySession final : public kinetra_lab::Session {
  public:
    FactorySession(std::uint32_t seed, const std::filesystem::path &directory,
                   const std::optional<std::filesystem::path> &initial, bool running, unsigned ups)
        : controller_(seed, running, ups), screen_(directory) {
        refresh_catalog();
        if (initial)
            controller_.load(*initial);
    }

    void advance(double elapsed, const Input &input, Camera2D &camera) override {
        const bool on_blueprint_screen = !controller_.loaded();
        if (!controller_.begin_frame(elapsed)) {
            // Start accepting blueprint-screen input on the frame after a deferred close.
            if (on_blueprint_screen)
                advance_screen(input, camera.viewport);
            return;
        }
        // The panels come first: an open settings panel takes the whole frame, and the
        // debug menu takes the pointer while it is over the menu.
        const bool modal = settings_.update(input, camera);
        controller_.suspend(settings_.visible());
        if (settings_.request() == SessionRequest::configurations) {
            // BLUEPRINTS: i4sim's configuration page is its own blueprint screen, so the
            // session serves the request instead of its host. The overlay outlives the
            // blueprint, so the request is cleared for the next one.
            controller_.close_later();
            settings_.clear_request();
        }
        // A hidden menu takes no click, so it needs no rows.
        const auto entries = menu_.visible ? controller_.menu(layer_color) : std::vector<DebugEntry>{};
        const bool captured = settings_.update_debug(input, camera.viewport, menu_, entries);
        if (modal)
            return;
        const auto before = controller_.selected_mover();
        InspectorInput inspection;
        if (before && !captured) {
            const auto area = kinetra_lab::inspector_area(menu_, camera.viewport, frame_times_);
            inspection = inspector_.update(input, inspect_mover(controller_.simulation(), *before), area);
            if (inspection.close)
                controller_.clear_selected_mover();
        }
        controller_.handle_input(elapsed, input, camera, {captured || inspection.captured, debug_});
        if (before != controller_.selected_mover())
            inspector_.reset();
    }

    void render(Runtime &runtime) override {
        frame_limit_.apply(runtime);
        if (!controller_.loaded()) {
            screen_.draw(runtime, controller_.message());
            return;
        }
        controller_.update_camera(runtime.camera);
        const auto &simulation = controller_.simulation();
        runtime.begin(world_background);
        // Outside debug mode the layers keep their selection but are not drawn.
        draw_world(runtime, simulation, debug_ ? controller_.layers() : ViewLayers::none(),
                   controller_.selected_mover());
        draw_hud(
            runtime, simulation,
            {statistics_, controller_.selected_producer(), controller_.description(), controller_.message()});
        if (menu_.visible)
            menu_.draw(runtime, controller_.menu(layer_color));
        const auto selected = controller_.selected_mover();
        const auto snapshot = selected && !settings_.visible()
                                  ? std::optional{inspect_mover(simulation, *selected)}
                                  : std::nullopt;
        kinetra_lab::draw_inspector(runtime, menu_, &inspector_, snapshot ? &*snapshot : nullptr,
                                    frame_times_ ? &runtime.frame_times() : nullptr);
        settings_.draw(runtime);
        runtime.end();
    }

    SessionRequest request() const override {
        if (exit_ || settings_.request() == SessionRequest::exit)
            return SessionRequest::exit;
        // Only the blueprint screen asks for the menu: the settings panel's first button is
        // BLUEPRINTS, whose request the session serves itself in advance().
        if (leave_)
            return SessionRequest::menu;
        return SessionRequest::none;
    }

    std::string title() override { return "i4sim"; }

    std::vector<kinetra_lab::DebugRow> debug_rows() override {
        // The menu belongs to a loaded blueprint; the blueprint screen has none.
        return controller_.loaded() ? kinetra_lab::debug_rows_of(controller_.menu())
                                    : std::vector<kinetra_lab::DebugRow>{};
    }

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
    InspectorView inspector_;
    bool statistics_{true}, debug_{true}, frame_times_{};
    kinetra_lab::FrameLimitSwitch frame_limit_;
    // Declared after the flags it borrows.
    SettingsOverlay settings_{
        kinetra_lab::settings_rows(
            statistics_, debug_, frame_times_, frame_limit_.enabled,
            {"WASD / ARROWS  MOVE", "CLICK SELECT / RIGHT BLOCK", "SHIFT + RIGHT CLICK  INSPECT",
             "SHIFT CLICK FREE MOVER GOAL", "SPACE PAUSE / R RESET"},
            {"1 - 6  TOGGLE LAYERS", "Q / E  HALVE / DOUBLE UPS", "G WORKLOAD / F FIT"}),
        &debug_,
        {"BLUEPRINTS", SessionRequest::configurations}};
};
} // namespace

std::unique_ptr<kinetra_lab::Session> create_session(std::uint32_t seed, std::filesystem::path directory,
                                                     std::optional<std::filesystem::path> initial,
                                                     bool running, unsigned ups) {
    return std::make_unique<FactorySession>(seed, directory, initial, running, ups);
}
} // namespace i4sim

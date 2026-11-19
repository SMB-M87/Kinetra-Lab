#include "i4sim/presentation/session.h"

#include "i4sim/controller.h"
#include "i4sim/presentation/detail/blueprint_screen.h"
#include "i4sim/presentation/detail/hud.h"
#include "i4sim/presentation/detail/world_view.h"

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
        // A hidden menu takes no click, so it needs no rows.
        const auto entries = menu_.visible ? controller_.menu(layer_color) : std::vector<DebugEntry>{};
        const bool captured = settings_.update_debug(input, camera.viewport, menu_, entries);
        if (modal)
            return;
        controller_.handle_input(elapsed, input, camera, {captured, debug_});
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
        draw_hud(runtime, simulation,
                 {statistics_, controller_.selected_mover(), controller_.selected_producer(),
                  controller_.description(), controller_.message()});
        if (menu_.visible)
            menu_.draw(runtime, controller_.menu(layer_color));
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
    lab::FrameLimitSwitch frame_limit_;
    // Declared after the flags it borrows.
    SettingsOverlay settings_{
        lab::settings_rows(statistics_, debug_, frame_times_, frame_limit_.enabled,
                           {"WASD / ARROWS  MOVE", "CLICK SELECT / RIGHT BLOCK",
                            "SHIFT CLICK FREE MOVER GOAL", "SPACE PAUSE / R RESET"},
                           {"1 - 6  TOGGLE LAYERS", "Q / E  HALVE / DOUBLE UPS", "G WORKLOAD / F FIT"}),
        &debug_};
};
} // namespace

std::unique_ptr<lab::Session> create_session(std::uint32_t seed, std::filesystem::path directory,
                                             std::optional<std::filesystem::path> initial, bool running,
                                             unsigned ups, std::optional<std::filesystem::path>) {
    return std::make_unique<FactorySession>(seed, directory, initial, running, ups);
}
} // namespace i4sim

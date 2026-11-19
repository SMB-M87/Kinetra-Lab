#include "demo/presentation/session.h"
#include "demo/debug_panel.h"
#include "demo/input/sandbox_input.h"
#include "demo/presentation/sandbox_renderer.h"
#include "demo/sandbox_application.h"
#include "kinetra_lab/input.h"
#include "kinetra_lab/settings.h"
#include <kinetra/runtime/timing.h>

namespace sandbox {
namespace {
/// The graphical Demo session: it orders a frame and owns the panels. What the debug menu
/// holds and decides is the DebugPanel's; the scene is drawn by the renderer.
class DemoSession final : public kinetra_lab::Session {
    sandbox::SandboxApplication application_;
    // Declared after the application it borrows.
    sandbox::DebugPanel panel_{application_};
    kinetra::DebugMenu menu_;
    bool debug_{true}, statistics_{true}, frame_times_{};
    kinetra_lab::FrameLimitSwitch frame_limit_;
    // Declared after the flags it borrows.
    kinetra::SettingsOverlay settings_{
        kinetra_lab::settings_rows(statistics_, debug_, frame_times_, frame_limit_.enabled,
                           {"WASD / ARROWS  MOVE", "CLICK  WALK / SELECT / GOAL", "Q / E  ROTATE PLAYER",
                            "SHIFT + LEFT / RIGHT  ROTATE"},
                           {"GHOST CLICK  TELEPORT"}),
        &debug_};
    kinetra_lab::ModalPause pause_;
    kinetra::RateCounter fps_;
    /// The menu's rows while it shows. A hidden menu needs none: it takes no click and
    /// draws nothing, and the rows are rebuilt on the frame after the shortcut shows it.
    std::vector<kinetra::DebugEntry> menu_rows() {
        return menu_.visible ? panel_.entries() : std::vector<kinetra::DebugEntry>{};
    }

  public:
    DemoSession(int count, std::uint32_t seed) : application_(count, seed) {}
    DemoSession(const DemoSession &) = delete;
    DemoSession &operator=(const DemoSession &) = delete;
    void advance(double elapsed, const kinetra::Input &input, kinetra::Camera2D &camera) override {
        fps_.count();
        fps_.advance(elapsed);
        // The panels come first: an open settings panel takes the whole frame, and the debug
        // menu takes the pointer while it is over the menu.
        const bool modal = settings_.update(input, camera);
        const auto entries = menu_rows();
        const bool captured = settings_.update_debug(input, camera.viewport, menu_, entries);
        auto commands = sandbox::read_commands(modal ? kinetra::Input{} : input);
        commands.toggle_pause =
            pause_.update(settings_.visible(), application_.paused()) != application_.paused();
        panel_.apply(commands, debug_, captured, modal ? 0 : input.wheel);
        application_.advance(elapsed, commands, camera);
        panel_.observe(camera);
    }
    void render(kinetra::Runtime &runtime) override {
        frame_limit_.apply(runtime);
        const auto entries = menu_rows();
        sandbox::render(runtime, application_, panel_.layers(debug_),
                        {.menu = &menu_,
                         .entries = entries,
                         .statistics = statistics_,
                         .fps = fps_.rate(),
                         .time_scale = panel_.time_scale(),
                         .settings = &settings_,
                         .frame_times = frame_times_ ? &runtime.frame_times() : nullptr});
    }
    kinetra::SessionRequest request() const override { return settings_.request(); }
    std::string title() override { return "Demo"; }
    std::string status() override { return sandbox::status_text(application_); }
};
} // namespace
std::unique_ptr<kinetra_lab::Session> create_session(int count, std::uint32_t seed) {
    return std::make_unique<DemoSession>(count, seed);
}
} // namespace sandbox

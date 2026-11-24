#include "demo/presentation/session.h"
#include "demo/application.h"
#include "demo/debug_panel.h"
#include "demo/input/mapping.h"
#include "demo/presentation/renderer.h"
#include "demo/world/inspector.h"
#include "kinetra_lab/input.h"
#include "kinetra_lab/inspector.h"
#include "kinetra_lab/settings.h"
#include <kinetra/runtime/timing.h>

namespace demo {
namespace {
/// The graphical Demo session: it orders a frame and owns the panels. What the debug menu
/// holds and decides is the DebugPanel's; the scene is drawn by the renderer.
class DemoSession final : public Session {
    demo::Application application_;
    // Declared after the application it borrows.
    demo::DebugPanel panel_{application_};
    kinetra::DebugMenu menu_;
    kinetra::InspectorView inspector_;
    bool debug_{true}, statistics_{true}, frame_times_{};
    kinetra_lab::FrameLimitSwitch frame_limit_;
    // Declared after the flags it borrows.
    kinetra::SettingsOverlay settings_{
        kinetra_lab::settings_rows(statistics_, debug_, frame_times_, frame_limit_.enabled,
                                   {"WASD / ARROWS  MOVE", "CLICK  WALK / SELECT / GOAL",
                                    "Q / E  ROTATE PLAYER", "SHIFT + LEFT / RIGHT  ROTATE"},
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
    DemoSession(int count, std::uint32_t seed, Scenario scenario) : application_(count, seed, scenario) {}
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
        const auto before = application_.scene().selected_agent();
        kinetra::InspectorInput examination;
        if (before && !modal && !captured)
            if (const auto snapshot = inspect_agent(application_.scene()))
                examination = inspector_.update(
                    input, *snapshot, kinetra_lab::inspector_area(menu_, camera.viewport, frame_times_));
        auto commands = demo::read_commands(modal ? kinetra::Input{} : input);
        commands.toggle_pause =
            pause_.update(settings_.visible(), application_.paused()) != application_.paused();
        panel_.apply(commands, debug_, captured || examination.captured, modal ? 0 : input.wheel);
        if (examination.close)
            commands.pointer = PointerCommand{PointerAction::clear_selection, input.mouse};
        application_.advance(elapsed, commands, camera);
        const auto after = application_.scene().selected_agent();
        if (bool(before) != bool(after) ||
            (before && after && (before->x != after->x || before->y != after->y)))
            inspector_.reset();
        panel_.observe(camera);
    }
    void render(kinetra::Runtime &runtime) override {
        frame_limit_.apply(runtime);
        const auto entries = menu_rows();
        const auto snapshot = settings_.visible() ? std::nullopt : inspect_agent(application_.scene());
        demo::render(runtime, application_, panel_.layers(debug_),
                     {.menu = &menu_,
                      .entries = entries,
                      .statistics = statistics_,
                      .fps = fps_.rate(),
                      .time_scale = panel_.time_scale(),
                      .settings = &settings_,
                      .frame_times = frame_times_ ? &runtime.frame_times() : nullptr,
                      .inspector = &inspector_,
                      .inspection = snapshot ? &*snapshot : nullptr});
    }
    /// The settings panel's LAUNCHER and EXIT, or else the debug menu's SCENARIOS, which
    /// asks for the launcher's page of scenarios.
    kinetra::SessionRequest request() const override {
        const auto requested = settings_.request();
        if (requested == kinetra::SessionRequest::none && panel_.scenarios_requested())
            return kinetra::SessionRequest::configurations;
        return requested;
    }
    std::string title() override {
        const auto scenario = application_.scene().scenario();
        return scenario == Scenario::terrain ? "Demo" : "Demo - " + std::string(scenario_label(scenario));
    }
    std::string status() override { return demo::status_text(application_); }
    std::vector<kinetra_lab::DebugRow> debug_rows() override {
        return kinetra_lab::debug_rows_of(panel_.entries());
    }
    const Application &application() const override { return application_; }
};
} // namespace
std::unique_ptr<Session> create_session(int count, std::uint32_t seed, Scenario scenario) {
    return std::make_unique<DemoSession>(count, seed, scenario);
}
} // namespace demo

#include "session.h"
#include "sandbox_application.h"
#include "sandbox_input.h"
#include "sandbox_renderer.h"
#include <kinetra/timing.h>
#include <lab/input.h>
#include <lab/settings.h>

namespace sandbox {
namespace {
class DemoSession final : public lab::Session {
    sandbox::SandboxApplication application_;
    kinetra::DebugMenu menu_;
    bool vectors_{true}, paths_{true}, collisions_{true}, agents_{true}, debug_{true}, probes_{true};
    bool ghost_{}, gravity_{}, goals_{};
    bool statistics_{true};
    bool reciprocal_{true};
    // Declared after the flags it borrows.
    kinetra::SettingsOverlay settings_{
        lab::settings_rows([this] { return debug_ ? debug_zoom_limits : zoom_limits; }, statistics_, debug_,
                           {"WASD / ARROWS  MOVE", "CLICK  WALK / SELECT / GOAL", "Q / E  ROTATE PLAYER",
                            "SHIFT + LEFT / RIGHT  ROTATE"},
                           {"GHOST CLICK  TELEPORT"}),
        &debug_};
    lab::ModalPause pause_;
    kinetra::RateCounter fps_;
    auto options() {
        return std::array<kinetra::DebugOption, 9>{{{"GHOST CAMERA", &ghost_},
                                                    {"VECTORS", &vectors_},
                                                    {"STEERING PROBES", &probes_},
                                                    {"PATHS", &paths_},
                                                    {"COLLISION SHAPES", &collisions_},
                                                    {"ACTIVE AGENTS", &agents_},
                                                    {"AGENT GOAL", &goals_},
                                                    {"GRAVITY", &gravity_},
                                                    {"RECIPROCAL AVOID", &reciprocal_}}};
    }

  public:
    DemoSession(int count, std::uint32_t seed) : application_(count, seed) {}
    DemoSession(const DemoSession &) = delete;
    DemoSession &operator=(const DemoSession &) = delete;
    void advance(double elapsed, const kinetra::Input &input, kinetra::Camera2D &camera) override {
        fps_.count();
        fps_.advance(elapsed);
        auto entries = options();
        const bool modal = settings_.update(input, camera);
        if (!debug_ || modal)
            menu_.visible = false;
        const bool captured =
            debug_ && !modal && settings_.update_debug(input, camera.viewport, menu_, entries);
        auto commands = sandbox::read_commands(modal ? kinetra::Input{} : input);
        commands.zoom_steps = debug_ && !modal ? input.wheel : 0;
        commands.toggle_pause =
            pause_.update(settings_.visible(), application_.paused()) != application_.paused();
        commands.toggle_vectors = (debug_ && vectors_) != application_.presentation().vectors;
        commands.toggle_paths = (debug_ && paths_) != application_.presentation().paths;
        if (captured)
            commands.pointer.reset();
        if (!debug_)
            ghost_ = false;
        commands.debug = debug_;
        commands.ghost = ghost_;
        commands.probes = debug_ && probes_;
        commands.reciprocal_avoidance = reciprocal_;
        commands.toggle_gravity = (debug_ && gravity_) != (application_.scene().world.gravity.y != 0);
        if (commands.pointer && debug_ && goals_ && commands.pointer->action == sandbox::PointerAction::walk)
            commands.pointer->action = sandbox::PointerAction::target_agents;
        application_.advance(elapsed, commands, camera);
    }
    void render(kinetra::Runtime &runtime) override {
        const auto entries = options();
        sandbox::render(runtime, application_, &menu_, entries, debug_ && collisions_, debug_ && agents_,
                        statistics_, fps_.rate(), &settings_);
    }
    kinetra::SessionRequest request() const override { return settings_.request(); }
    std::string title() override { return "Demo"; }
    std::string status() override { return sandbox::status_text(application_); }
};
} // namespace
std::unique_ptr<lab::Session> create_session(int count, std::uint32_t seed) {
    return std::make_unique<DemoSession>(count, seed);
}
} // namespace sandbox

#include "session.h"
#include "sandbox_application.h"
#include "sandbox_input.h"
#include "sandbox_renderer.h"
namespace sandbox {
namespace {
using lab::Session;
class DemoSession final : public Session {
    sandbox::SandboxApplication application_;
    kinetra::DebugMenu menu_;
    bool vectors_{}, paths_{}, collisions_{}, agents_{}, debug_{}, ghost_{}, gravity_{}, probes_{}, goals_{};
    bool statistics_{};
    lab::Settings settings_;
    double statistics_elapsed_{}, fps_{};
    int frames_{};
    auto options() {
        return std::array<kinetra::DebugOption, 8>{{{"GHOST CAMERA", &ghost_},
                                                    {"VECTORS", &vectors_},
                                                    {"STEERING PROBES", &probes_},
                                                    {"PATHS", &paths_},
                                                    {"COLLISION SHAPES", &collisions_},
                                                    {"ACTIVE AGENTS", &agents_},
                                                    {"AGENT GOAL", &goals_},
                                                    {"GRAVITY", &gravity_}}};
    }

  public:
    DemoSession(int count, std::uint32_t seed) : application_(count, seed) {}
    void advance(double elapsed, const kinetra::Input &input, kinetra::Camera2D &camera) override {
        statistics_elapsed_ += elapsed;
        ++frames_;
        if (statistics_elapsed_ >= 0.5) {
            fps_ = frames_ / statistics_elapsed_;
            statistics_elapsed_ = 0;
            frames_ = 0;
        }
        auto entries = options();
        const bool modal = settings_.update(input, camera, statistics_, debug_, debug_ ? 5.f : 17.f, 100.f);
        if (!debug_ || modal)
            menu_.visible = false;
        const bool captured = debug_ && !modal && settings_.update_debug(input, camera, menu_, entries);
        auto commands = sandbox::read_commands(modal ? kinetra::Input{} : input);
        commands.zoom_steps = debug_ && !modal ? input.wheel : 0;
        commands.toggle_pause = application_.paused() != settings_.visible;
        commands.toggle_vectors = (debug_ && vectors_) != application_.presentation().vectors;
        commands.toggle_paths = (debug_ && paths_) != application_.presentation().paths;
        if (captured)
            commands.pointer.reset();
        if (!debug_)
            ghost_ = false;
        commands.debug = debug_;
        commands.ghost = ghost_;
        commands.probes = debug_ && probes_;
        commands.toggle_gravity = (debug_ && gravity_) != (application_.scene().world.gravity.y != 0);
        if (commands.pointer && debug_ && goals_ && commands.pointer->action == sandbox::PointerAction::walk)
            commands.pointer->action = sandbox::PointerAction::target_agents;
        application_.advance(elapsed, commands, camera);
    }
    void render(kinetra::Runtime &runtime) override {
        const auto entries = options();
        sandbox::render(runtime, application_, &menu_, entries, debug_ && collisions_, debug_ && agents_,
                        statistics_, fps_, &settings_, debug_);
    }
    bool return_to_menu() const override { return settings_.return_requested; }
    bool exit_requested() const override { return settings_.exit_requested; }
    std::string title() override { return sandbox::window_title(application_) + " | CTRL settings"; }
};
} // namespace
std::unique_ptr<lab::Session> create_session(int count, std::uint32_t seed) {
    return std::make_unique<DemoSession>(count, seed);
}
} // namespace sandbox

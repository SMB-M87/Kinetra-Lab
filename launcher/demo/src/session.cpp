#include "session.h"
#include "sandbox_application.h"
#include "sandbox_input.h"
#include "sandbox_renderer.h"
namespace sandbox {
namespace {
using kinetra::Session;
class DemoSession final : public Session {
    sandbox::SandboxApplication application_;
    kinetra::DebugMenu menu_;
    bool vectors_{}, paths_{}, collisions_{}, agents_{}, debug_{}, ghost_{}, gravity_{}, probes_{}, goals_{};
    bool statistics_{};
    double statistics_elapsed_{}, fps_{};
    int frames_{};
    auto options() {
        return std::array<kinetra::DebugOption, 10>{{{"STATISTICS", &statistics_},
                                                     {"DEBUG MODE", &debug_},
                                                     {"GHOST CAMERA", &ghost_},
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
        vectors_ = application_.presentation().vectors;
        paths_ = application_.presentation().paths;
        auto entries = options();
        const bool captured = menu_.update(input, camera.viewport, entries);
        auto commands = sandbox::read_commands(input);
        commands.toggle_vectors = commands.toggle_vectors || vectors_ != application_.presentation().vectors;
        commands.toggle_paths = commands.toggle_paths || paths_ != application_.presentation().paths;
        if (captured)
            commands.pointer.reset();
        if (!debug_)
            ghost_ = false;
        commands.debug = debug_;
        commands.ghost = ghost_;
        commands.probes = probes_;
        commands.toggle_gravity = gravity_ != (application_.scene().world.gravity.y != 0);
        if (commands.pointer && goals_ && commands.pointer->action == sandbox::PointerAction::walk)
            commands.pointer->action = sandbox::PointerAction::target_agents;
        application_.advance(elapsed, commands, camera);
    }
    void render(kinetra::Runtime &runtime) override {
        vectors_ = application_.presentation().vectors;
        paths_ = application_.presentation().paths;
        const auto entries = options();
        sandbox::render(runtime, application_, &menu_, entries, collisions_, agents_, statistics_, fps_);
    }
    std::string title() override { return sandbox::window_title(application_) + " | M menu"; }
};
} // namespace
std::unique_ptr<kinetra::Session> create_session(int count, std::uint32_t seed) {
    return std::make_unique<DemoSession>(count, seed);
}
} // namespace sandbox

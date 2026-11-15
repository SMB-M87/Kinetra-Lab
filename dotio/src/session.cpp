#include "session.h"
#include "presentation.h"
namespace dotio {
namespace {
using kinetra::Session;
class DotioSession final : public Session {
    dotio::Application application_;

  public:
    DotioSession(std::uint32_t seed, logic::Difficulty difficulty) : application_(seed, difficulty) {}
    void advance(double elapsed, const kinetra::Input &input, kinetra::Camera2D &camera) override {
        const bool was_ghost = application_.ghost;
        auto options = application_.debug_options();
        const bool captured = application_.menu.update(input, camera.viewport, options);
        if (!was_ghost && application_.ghost) {
            application_.world().GetGhost().Position = application_.world().GetPlayer().Position;
            application_.world().GetGhost().Velocity = {};
        }
        auto commands = dotio::read_commands(input, camera);
        if (captured) {
            commands.shoot = false;
            commands.teleport = false;
        }
        application_.advance(elapsed, commands, camera);
    }
    void render(kinetra::Runtime &runtime) override { dotio::render(runtime, application_); }
    std::string title() override { return dotio::window_title(application_); }
};
} // namespace
std::unique_ptr<kinetra::Session> create_session(std::uint32_t seed, logic::Difficulty difficulty) {
    return std::make_unique<DotioSession>(seed, difficulty);
}
} // namespace dotio

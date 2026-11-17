#include "session.h"
#include "presentation.h"
namespace dotio {
namespace {
using lab::Session;
class DotioSession final : public Session {
    dotio::Application application_;

  public:
    DotioSession(std::uint32_t seed, logic::Difficulty difficulty) : application_(seed, difficulty) {}
    void advance(double elapsed, const kinetra::Input &input, kinetra::Camera2D &camera) override {
        const bool was_ghost = application_.ghost;
        auto options = application_.debug_options();
        const bool modal =
            application_.settings.update(input, camera, application_.statistics, application_.debug,
                                         application_.debug ? 2.5f : 25.f, 50.f);
        if (!application_.debug || modal)
            application_.menu.visible = false;
        const bool captured = application_.debug && !modal &&
                              application_.settings.update_debug(input, camera, application_.menu, options);
        if (!was_ghost && application_.ghost) {
            application_.world().GetGhost().Position = application_.world().GetPlayer().Position;
            application_.world().GetGhost().Velocity = {};
        }
        auto commands = dotio::read_commands(modal ? kinetra::Input{} : input, camera);
        commands.zoom_steps = application_.debug && !modal ? input.wheel : 0;
        application_.paused = application_.settings.visible;
        if (captured) {
            commands.shoot = false;
            commands.teleport = false;
        }
        application_.advance(elapsed, commands, camera);
    }
    void render(kinetra::Runtime &runtime) override { dotio::render(runtime, application_); }
    bool return_to_menu() const override { return application_.settings.return_requested; }
    bool exit_requested() const override { return application_.settings.exit_requested; }
    std::string title() override { return dotio::window_title(application_); }
};
} // namespace
std::unique_ptr<lab::Session> create_session(std::uint32_t seed, logic::Difficulty difficulty) {
    return std::make_unique<DotioSession>(seed, difficulty);
}
} // namespace dotio

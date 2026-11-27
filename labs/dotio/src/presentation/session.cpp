#include "dotio/presentation/session.h"
#include "dotio/difficulty.h"
#include "dotio/presentation/presentation.h"
#include "kinetra_lab/input.h"
#include "kinetra_lab/inspector.h"
#include "kinetra_lab/settings.h"
#include <stdexcept>
namespace dotio {
namespace {
class DotioSession final : public Session {
    dotio::Application application_;
    kinetra_lab::FrameLimitSwitch frame_limit_;
    // Declared after the application and the switch whose flags it borrows.
    kinetra::SettingsOverlay settings_{
        kinetra_lab::settings_rows(application_.statistics, application_.debug, application_.frame_times,
                                   frame_limit_.enabled,
                                   {"WASD / ARROWS  MOVE", "HOLD CLICK  SHOOT", "MOUSE  AIM"},
                                   {"GHOST CLICK  TELEPORT", "SHIFT + RIGHT CLICK  INSPECT"}),
        &application_.debug,
        {"DIFFICULTY", kinetra::SessionRequest::configurations}};
    kinetra_lab::ModalPause pause_;
    /// The debug panel; the application supplies its rows.
    kinetra::DebugMenu menu_;
    kinetra::InspectorView inspector_;

  public:
    DotioSession(std::uint32_t seed, dotio::Difficulty difficulty) : application_(seed, difficulty) {}
    DotioSession(const DotioSession &) = delete;
    DotioSession &operator=(const DotioSession &) = delete;
    void advance(double elapsed, const kinetra::Input &input, kinetra::Camera2D &camera) override {
        const bool was_ghost = application_.ghost;
        // The panels come first: an open settings panel takes the whole frame, and the debug
        // menu takes the pointer while it is over the menu.
        const bool modal = settings_.update(input, camera);
        // A hidden menu takes no click, so it needs no rows.
        const auto entries =
            menu_.visible ? application_.debug_entries() : std::vector<kinetra::DebugEntry>{};
        const bool captured = settings_.update_debug(input, camera.viewport, menu_, entries);
        const auto *before = application_.selected_enemy();
        const auto generation = before ? before->generation() : 0;
        kinetra::InspectorInput examination;
        if (!modal && !captured) {
            if (const auto snapshot = application_.inspect_enemy())
                examination = inspector_.update(
                    input, *snapshot,
                    kinetra_lab::inspector_area(menu_, camera.viewport, application_.frame_times));
            if (examination.close)
                application_.clear_selected_enemy();
            else if (!examination.captured && input.right_clicked &&
                     (input.down(kinetra::Key::shift) || input.hit(kinetra::Key::shift))) {
                const auto simulated_player = to_world(application_.world().get_player().position);
                const PresentedPositions presented(application_);
                const auto offset =
                    application_.ghost
                        ? kinetra::Vec2{}
                        : to_world(application_.world().get_player().position) - simulated_player;
                application_.select_enemy(camera.screen_to_world(input.mouse) + offset);
            }
        }
        if (!was_ghost && application_.ghost) {
            application_.world().get_ghost().position = application_.world().get_player().position;
            application_.world().get_ghost().velocity = {};
        }
        auto commands = dotio::read_commands(modal ? kinetra::Input{} : input, camera);
        commands.zoom_steps =
            application_.debug && !modal && !captured && !examination.captured ? input.wheel : 0;
        application_.paused = pause_.update(settings_.visible(), application_.paused);
        if (captured || examination.captured) {
            commands.shoot = false;
            commands.teleport = false;
        }
        application_.advance(elapsed, commands, camera);
        const auto *after = application_.selected_enemy();
        if (before != after || (after && after->generation() != generation))
            inspector_.reset();
    }
    void render(kinetra::Runtime &runtime) override {
        frame_limit_.apply(runtime);
        dotio::render(runtime, application_, settings_, menu_, &inspector_);
    }
    /// The settings panel's DIFFICULTY, which asks for the launcher's page of difficulties,
    /// and EXIT.
    kinetra::SessionRequest request() const override { return settings_.request(); }
    std::string title() override { return "Dotio"; }
    std::string status() override { return dotio::status_text(application_); }
    std::vector<kinetra_lab::DebugRow> debug_rows() override {
        return kinetra_lab::debug_rows_of(application_.debug_entries());
    }
    Application &application() override { return application_; }
};
} // namespace
std::vector<std::string> difficulty_names() {
    std::vector<std::string> names;
    for (const auto &preset : dotio::difficulty_presets)
        names.emplace_back(preset.name);
    return names;
}
std::size_t difficulty_index(std::string_view name) {
    return static_cast<std::size_t>(dotio::parse_difficulty(name));
}
std::unique_ptr<Session> create_session(std::uint32_t seed, std::size_t difficulty) {
    if (difficulty >= dotio::difficulty_presets.size())
        throw std::invalid_argument("Unsupported Dotio difficulty");
    return std::make_unique<DotioSession>(seed, static_cast<dotio::Difficulty>(difficulty));
}
} // namespace dotio

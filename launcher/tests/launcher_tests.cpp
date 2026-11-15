#include <iostream>
#include <kinetra/debug_view.h>
#include <kinetra/launcher.h>
#include <stdexcept>

using namespace kinetra;
namespace {
void require(bool value, const char *message) {
    if (!value)
        throw std::runtime_error(message);
}
struct TestSession : Session {
    int &destroyed;
    double elapsed{-1};
    bool clicked{true};
    explicit TestSession(int &count) : destroyed(count) {}
    ~TestSession() override { ++destroyed; }
    void advance(double dt, const Input &input, Camera2D &) override {
        elapsed = dt;
        clicked = input.clicked;
    }
    void render(Runtime &) override {}
    std::string title() override { return "TEST"; }
};
} // namespace
int main() {
    try {
        LaunchMenu menu({{"EXTERNAL", "CUSTOM APP", {"SMALL", "LARGE"}, 1}});
        Input confirm;
        confirm.pressed[std::size_t(Key::enter)] = true;
        require(!menu.update(confirm, {800, 600}) && menu.configuring(), "Configuration skipped");
        const auto selection = menu.update(confirm, {800, 600});
        require(selection && selection->configuration == 1, "Default configuration lost");
        int destroyed = 0;
        bool fail = false;
        SessionHost host({{{"EXTERNAL", "CUSTOM APP", {}, 0},
                           [&](Camera2D &camera, std::size_t) -> std::unique_ptr<Session> {
                               camera.position.x = 42;
                               if (fail)
                                   throw std::runtime_error("Factory failure");
                               return std::make_unique<TestSession>(destroyed);
                           }}});
        Camera2D camera;
        host.start({0, 0}, camera);
        auto *session = static_cast<TestSession *>(host.session());
        Input click;
        click.clicked = true;
        host.advance(5, click, camera);
        require(session->elapsed == 0 && !session->clicked, "Selection input/time leaked");
        host.advance(0.25, click, camera);
        require(session->elapsed == 0.25 && session->clicked, "Normal input suppressed");
        fail = true;
        camera.position.x = 7;
        try {
            host.start({0, 0}, camera);
        } catch (const std::runtime_error &) {
        }
        require(host.session() == session && camera.position.x == 7 && destroyed == 0,
                "Failed transition changed session");
        host.return_to_menu();
        require(!host.session() && destroyed == 1, "Return did not release session");
        DebugMenu debug;
        debug.visible = true;
        int actions = 0;
        DebugOption action{"RESET", nullptr, [&] { ++actions; }};
        click.mouse = {1280 - 180 * DebugMenu::scale({1280, 800}), 36 * DebugMenu::scale({1280, 800})};
        require(debug.update(click, {1280, 800}, {&action, 1}) && actions == 1,
                "Debug action/capture failed");
        std::cout
            << "PASS generic registration, cleanup, failed transitions, input isolation and debug actions\n";
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}

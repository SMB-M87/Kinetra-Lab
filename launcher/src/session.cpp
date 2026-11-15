#include "session.h"
#include "../../labs/demo/include/session.h"
#include "../../labs/dotio/include/session.h"
#include <chrono>

namespace launcher {
std::unique_ptr<Session> start(Variant variant, int count, std::uint32_t seed, kinetra::Camera2D &camera,
                               logic::Difficulty difficulty) {
    camera.position = {};
    camera.pixels_per_unit = variant == Variant::demo ? 17 : 25;
    if (variant == Variant::demo)
        return sandbox::create_session(count, seed);
    return dotio::create_session(seed, difficulty);
}
void render_menu(kinetra::Runtime &runtime, const Menu &menu) {
    menu.draw(runtime);
}
int run(kinetra::Runtime &runtime, int count, std::uint32_t seed, logic::Difficulty difficulty,
        std::optional<Variant> initial, int frame_limit) {
    Menu menu(difficulty);
    std::unique_ptr<Session> session;
    if (initial)
        session = start(*initial, count, seed, runtime.camera, difficulty);
    using Clock = std::chrono::steady_clock;
    auto previous = Clock::now();
    bool first = true;
    int frames = 0;
    while (runtime.poll(false)) {
        const auto now = Clock::now();
        if (session) {
            session->advance(first ? 0 : std::chrono::duration<double>(now - previous).count(),
                             first ? kinetra::Input{} : runtime.input(), runtime.camera);
            first = false;
            if (session->exit_requested())
                break;
            if (session->return_to_menu()) {
                session.reset();
                render_menu(runtime, menu);
                runtime.title("Kinetra | Launcher");
            } else {
                session->render(runtime);
                runtime.title(session->title());
            }
        } else {
            if (auto selected = menu.update(runtime.input(), runtime.camera.viewport)) {
                session = start(*selected, count, seed, runtime.camera, menu.difficulty());
                first = true;
            }
            if (menu.exit_requested())
                break;
            render_menu(runtime, menu);
        }
        previous = now;
        if (++frames == frame_limit)
            break;
    }
    return frames;
}
} // namespace launcher

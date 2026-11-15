#include "session.h"
#include "../../demo/include/session.h"
#include "../../dotio/include/session.h"

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
} // namespace launcher

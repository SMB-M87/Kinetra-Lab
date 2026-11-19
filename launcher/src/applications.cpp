#include "applications.h"
#include "../../labs/demo/include/session.h"
#include "../../labs/demo/include/zoom.h"
#include "../../labs/dotio/include/session.h"
#include "../../labs/dotio/include/zoom.h"
#include "../../labs/i4sim/include/session.h"

namespace launcher {
namespace {
/// Starts every application from the world origin at its own initial zoom.
void reset(kinetra::Camera2D &camera, float zoom) {
    camera.position = {};
    camera.pixels_per_unit = zoom;
}
} // namespace

std::vector<kinetra::ApplicationRegistration> applications(const Options &options) {
    std::vector<kinetra::ApplicationRegistration> result;
    result.push_back({{"DEMO", "PHYSICS AND NAVIGATION", {}, 0},
                      [options](kinetra::Camera2D &camera, std::size_t) -> std::unique_ptr<kinetra::Session> {
                          reset(camera, sandbox::initial_zoom);
                          return sandbox::create_session(options.agents, options.seed);
                      }});
    result.push_back(
        {{"DOTIO", "SURVIVAL", dotio::difficulty_names(), options.dotio_difficulty},
         [options](kinetra::Camera2D &camera, std::size_t difficulty) -> std::unique_ptr<kinetra::Session> {
             reset(camera, dotio::initial_zoom);
             return dotio::create_session(options.seed, difficulty);
         }});
    result.push_back({{"I4SIM", "LOCAL FACTORY SIMULATION", {}, 0},
                      [options](kinetra::Camera2D &camera, std::size_t) -> std::unique_ptr<kinetra::Session> {
                          reset(camera, i4sim::initial_zoom);
                          return i4sim::create_session(options.seed);
                      }});
    return result;
}

kinetra::LaunchSelection selection(Variant variant, const Options &options) {
    return {static_cast<std::size_t>(variant), variant == Variant::dotio ? options.dotio_difficulty : 0};
}
} // namespace launcher

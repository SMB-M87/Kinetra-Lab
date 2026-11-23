#include "launcher/applications.h"
#include "demo/presentation/session.h"
#include "demo/zoom.h"
#include "dotio/presentation/session.h"
#include "dotio/zoom.h"
#include "i4sim/presentation/session.h"

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
    result.push_back(
        {{"DEMO", "PHYSICS AND NAVIGATION", demo::scenario_labels(), options.demo_scenario},
         [options](kinetra::Camera2D &camera, std::size_t index) -> std::unique_ptr<kinetra::Session> {
             reset(camera, demo::initial_zoom);
             return demo::create_session(options.agents, options.seed, demo::scenario_at(index));
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
    const auto application = static_cast<std::size_t>(variant);
    switch (variant) {
    case Variant::demo:
        return {application, options.demo_scenario};
    case Variant::dotio:
        return {application, options.dotio_difficulty};
    case Variant::i4sim:
        return {application, 0};
    }
    return {application, 0};
}
} // namespace launcher

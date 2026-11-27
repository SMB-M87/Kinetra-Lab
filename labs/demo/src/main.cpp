#include "demo/application.h"
#include "demo/presentation/session.h"
#include "demo/world/inspector.h"
#include "demo/zoom.h"
#include "kinetra_lab/inspector_trace.h"

#include <kinetra/runtime/engine.h>
#include <kinetra/runtime/session.h>

#include <cstdlib>
#include <iostream>
#include <optional>
#include <random>
#include <string>
#include <string_view>

namespace {
void trace_agent(demo::Scenario scenario, std::uint32_t seed, std::uint64_t ticks,
                 const std::string &coordinates) {
    using kinetra_lab::inspector_argument;
    const auto comma = coordinates.find(',');
    if (comma == std::string::npos)
        throw std::invalid_argument("Expected the agent's home coordinates X,Y");
    const kinetra::Vec2 home{inspector_argument<float>(coordinates.substr(0, comma)),
                             inspector_argument<float>(coordinates.substr(comma + 1))};
    if (!kinetra::finite(home))
        throw std::invalid_argument("Expected finite home coordinates");
    demo::Application application(demo::default_agent_count, seed, scenario);
    kinetra::Camera2D camera;
    camera.pixels_per_unit = demo::initial_zoom;
    const auto id = "AGENT " + coordinates;
    const double step = demo::tick_seconds;
    for (std::uint64_t tick = 0; tick < ticks; ++tick) {
        application.advance(step, {}, camera);
        const auto snapshot = demo::inspect_agent(application.scene(), home);
        kinetra_lab::write_inspector_tick(std::cout, tick + 1, id, snapshot ? &*snapshot : nullptr);
    }
}
} // namespace

int main(int argc, char **argv) {
    try {
        int frames = 0;
        std::uint64_t ticks = 0;
        std::optional<std::uint32_t> seed;
        std::optional<std::string> inspect;
        auto scenario = demo::Scenario::terrain;
        using kinetra_lab::inspector_argument;
        for (int i = 1; i < argc; ++i) {
            const std::string_view option = argv[i];
            if (i + 1 >= argc)
                throw std::invalid_argument("Missing argument for " + std::string(option));
            const std::string_view value = argv[++i];
            if (option == "--frames") {
                std::size_t used{};
                frames = std::stoi(std::string(value), &used);
                if (used != value.size() || frames < 1 || frames > 100000)
                    throw std::invalid_argument("Invalid frame limit");
            } else if (option == "--seed")
                seed = inspector_argument<std::uint32_t>(value);
            else if (option == "--ticks")
                ticks = inspector_argument<std::uint64_t>(value);
            else if (option == "--inspect")
                inspect = value;
            else if (option == "--scenario")
                scenario = demo::parse_scenario(value);
            else
                throw std::invalid_argument(
                    "Usage: kinetra_demo [--seed N] [--scenario terrain|gravity|maze|doorway] "
                    "[--frames N | --ticks N --inspect X,Y]");
        }
        if (inspect || ticks) {
            if (!inspect || ticks == 0 || !seed || frames)
                throw std::invalid_argument(
                    "--inspect X,Y requires --seed N and positive --ticks N instead of --frames");
            trace_agent(scenario, *seed, ticks, *inspect);
            return 0;
        }
        kinetra::Runtime runtime("Demo");
        runtime.camera.pixels_per_unit = demo::initial_zoom;
        const auto chosen = seed ? *seed : std::random_device{}();
        std::cout << "World seed: " << chosen << '\n';
        auto session = demo::create_session(demo::default_agent_count, chosen, scenario);
        kinetra::run_session(runtime, *session, frames);
    } catch (const std::exception &error) {
        std::cerr << "Demo: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}

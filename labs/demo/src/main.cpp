#include "demo/presentation/session.h"
#include "demo/presentation/zoom.h"
#include <cstdlib>
#include <iostream>
#include <kinetra/runtime/engine.h>
#include <kinetra/runtime/session.h>
#include <random>
#include <string>

int main(int argc, char **argv) {
    try {
        int frames = 0;
        if (argc != 1) {
            if (argc != 3 || std::string(argv[1]) != "--frames")
                throw std::invalid_argument("Usage: kinetra_demo [--frames 1..100000]");
            std::size_t used{};
            frames = std::stoi(argv[2], &used);
            if (used != std::string(argv[2]).size() || frames < 1 || frames > 100000)
                throw std::invalid_argument("Invalid frame limit");
        }
        kinetra::Runtime runtime("Demo");
        runtime.camera.pixels_per_unit = sandbox::initial_zoom;
        const std::uint32_t seed = std::random_device{}();
        std::cout << "World seed: " << seed << '\n';
        auto session = sandbox::create_session(sandbox::default_agent_count, seed);
        kinetra::run_session(runtime, *session, frames);
    } catch (const std::exception &error) {
        std::cerr << "Demo: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}

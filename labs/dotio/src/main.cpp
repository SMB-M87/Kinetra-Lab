#include "dotio/presentation/session.h"
#include "dotio/zoom.h"
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <optional>
#include <random>
#include <stdexcept>

int main(int argc, char **argv) {
    try {
        int frames = 0;
        auto difficulty = dotio::default_difficulty;
        std::optional<std::uint32_t> seed;
        for (int i = 1; i < argc; ++i) {
            const std::string option = argv[i];
            if (option == "--difficulty" && i + 1 < argc)
                difficulty = dotio::difficulty_index(argv[++i]);
            else if (option == "--frames" && i + 1 < argc) {
                const std::string value = argv[++i];
                std::size_t used{};
                frames = std::stoi(value, &used);
                if (used != value.size() || frames <= 0)
                    throw std::invalid_argument("Invalid frame count");
            } else if (option == "--seed" && i + 1 < argc) {
                const std::string value = argv[++i];
                std::size_t used{};
                const auto parsed = std::stoull(value, &used);
                if (used != value.size() || parsed > 0xFFFFFFFFull)
                    throw std::invalid_argument("Invalid seed");
                seed = static_cast<std::uint32_t>(parsed);
            } else
                throw std::invalid_argument(
                    "Usage: kinetra_dotio [--difficulty easy|medium|hard] [--seed N] [--frames N]");
        }
        kinetra::Runtime runtime("Dotio");
        runtime.camera.pixels_per_unit = dotio::initial_zoom;
        // The same seed replays the same world and enemies; print it so a run can be repeated.
        const auto chosen = seed ? *seed : std::random_device{}();
        std::cout << "World seed: " << chosen << '\n';
        auto session = dotio::create_session(chosen, difficulty);
        kinetra::run_session(runtime, *session, frames);
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}

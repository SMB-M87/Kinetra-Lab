#include "dotio/presentation/session.h"
#include "dotio/zoom.h"
#include <iostream>
#include <random>

int main(int argc, char **argv) {
    try {
        int frames = 0;
        auto difficulty = dotio::default_difficulty;
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
            } else
                throw std::invalid_argument(
                    "Usage: kinetra_dotio [--difficulty easy|medium|hard] [--frames N]");
        }
        kinetra::Runtime runtime("Dotio");
        runtime.camera.pixels_per_unit = dotio::initial_zoom;
        auto session = dotio::create_session(std::random_device{}(), difficulty);
        kinetra::run_session(runtime, *session, frames);
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}

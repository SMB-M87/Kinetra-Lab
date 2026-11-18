#include "../../labs/demo/include/session.h"
#include "../include/session.h"
#include <cstdlib>
#include <iostream>
#include <kinetra/engine.h>
#include <random>
#include <string>

using namespace kinetra;

namespace {

int argument(const char *value, int minimum, int maximum) {
    std::size_t used{};
    const int result = std::stoi(value, &used);
    if (used != std::string(value).size() || result < minimum || result > maximum)
        throw std::invalid_argument("Argument outside supported range");
    return result;
}

} // namespace

int main(int argc, char **argv) {
    try {
        int count = sandbox::default_agent_count, frame_limit = 0;
        std::optional<launcher::Variant> initial_variant;
        auto difficulty = logic::Difficulty::medium;
        for (int i = 1; i < argc; ++i) {
            const std::string option = argv[i];
            if (option == "--variant" && i + 1 < argc) {
                const std::string value = argv[++i];
                if (value == "demo")
                    initial_variant = launcher::Variant::demo;
                else if (value == "dotio")
                    initial_variant = launcher::Variant::dotio;
                else if (value == "i4sim")
                    initial_variant = launcher::Variant::i4sim;
                else
                    throw std::invalid_argument("Variant must be demo, dotio or i4sim");
            } else if (option == "--difficulty" && i + 1 < argc)
                difficulty = logic::parse_difficulty(argv[++i]);
            else if (option == "--agents" && i + 1 < argc)
                count = argument(argv[++i], 0, 300);
            else if (option == "--frames" && i + 1 < argc)
                frame_limit = argument(argv[++i], 1, 100000);
            else
                throw std::invalid_argument("Usage: kinetra [--variant demo|dotio|i4sim] [--difficulty "
                                            "easy|medium|hard] [--agents 0..300] [--frames 1..100000]");
        }
        Runtime runtime;
        runtime.camera.pixels_per_unit = 17;
        const std::uint32_t seed = std::random_device{}();
        std::cout << "World seed: " << seed << '\n';
        const int frames = launcher::run(runtime, count, seed, difficulty, initial_variant, frame_limit);
        std::cout << frames << " frames\n";
    } catch (const std::exception &error) {
        std::cerr << "Kinetra: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}

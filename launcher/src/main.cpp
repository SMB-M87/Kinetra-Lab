#include "demo/presentation/session.h"
#include "dotio/presentation/session.h"
#include "launcher/applications.h"
#include <cstdlib>
#include <iostream>
#include <kinetra/runtime/engine.h>
#include <kinetra/runtime/launcher.h>
#include <random>
#include <string>

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
        launcher::Options options{demo::default_agent_count, 0, dotio::default_difficulty};
        int frame_limit = 0;
        std::optional<launcher::Variant> initial_variant;
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
                options.dotio_difficulty = dotio::difficulty_index(argv[++i]);
            else if (option == "--agents" && i + 1 < argc)
                options.agents = argument(argv[++i], 0, 300);
            else if (option == "--frames" && i + 1 < argc)
                frame_limit = argument(argv[++i], 1, 100000);
            else
                throw std::invalid_argument("Usage: kinetra [--variant demo|dotio|i4sim] [--difficulty "
                                            "easy|medium|hard] [--agents 0..300] [--frames 1..100000]");
        }
        kinetra::Runtime runtime{std::string(kinetra::launcher_window_title)};
        options.seed = std::random_device{}();
        std::cout << "World seed: " << options.seed << '\n';
        std::optional<kinetra::LaunchSelection> initial;
        if (initial_variant)
            initial = launcher::selection(*initial_variant, options);
        const int frames = kinetra::run_launcher(runtime, launcher::applications(options), initial,
                                                 frame_limit, launcher::bindings);
        std::cout << frames << " frames\n";
    } catch (const std::exception &error) {
        std::cerr << "Kinetra: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}

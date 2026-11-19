#include "scenario.h"
#include "simulation.h"
#ifdef I4SIM_RUNTIME
#include "session.h"
#endif
#include <iostream>

namespace {
std::uint64_t argument(const std::string &text, std::uint64_t maximum) {
    std::size_t used{};
    if (text.empty() || text.front() == '-')
        throw std::invalid_argument("Invalid numeric argument");
    const auto value = std::stoull(text, &used);
    if (used != text.size() || value > maximum)
        throw std::invalid_argument("Argument outside supported range");
    return value;
}
} // namespace
int main(int argc, char **argv) {
    try {
        std::optional<std::filesystem::path> blueprint, output;
        auto directory = i4sim::default_blueprint_directory();
        std::uint64_t ticks = 0;
        std::uint32_t seed = 42;
        int frames = 0;
        unsigned ups = 200;
        bool running = false;
        for (int i = 1; i < argc; ++i) {
            const std::string option = argv[i];
            if (option == "--run") {
                running = true;
                continue;
            }
            if (i + 1 >= argc)
                throw std::invalid_argument("Missing argument for " + option);
            const std::string value = argv[++i];
            if (option == "--blueprint")
                blueprint = value;
            else if (option == "--blueprints")
                directory = value;
            else if (option == "--output")
                output = value;
            else if (option == "--seed")
                seed = std::uint32_t(argument(value, UINT32_MAX));
            else if (option == "--frames")
                frames = int(argument(value, 100000));
            else if (option == "--ticks")
                ticks = argument(value, 100000000);
            else if (option == "--ups")
                ups = unsigned(argument(value, 10000));
            else
                throw std::invalid_argument(
                    "Usage: kinetra_i4sim [--blueprint FILE] [--blueprints DIR] [--ticks N] [--output DIR] "
                    "[--seed N] [--frames N] [--run] [--ups N]");
        }
        if (ups == 0)
            throw std::invalid_argument("UPS must be in 1..10000");
        if (ticks) {
            if (!blueprint)
                throw std::invalid_argument("--ticks requires --blueprint FILE");
            i4sim::Simulation simulation(i4sim::load_blueprint(*blueprint), seed);
            if (const auto scenario = i4sim::load_scenario(*blueprint))
                i4sim::apply_scenario(*scenario, simulation);
            simulation.updates_per_second(ups);
            simulation.paused = false;
            for (std::uint64_t i = 0; i < ticks && !simulation.paused; ++i)
                simulation.tick();
            if (output)
                simulation.dump(*output);
            std::cout << "i4sim: " << simulation.ticks() << " ticks, " << simulation.completed()
                      << " completed products\n";
            return 0;
        }
#ifdef I4SIM_RUNTIME
        kinetra::Runtime runtime("i4sim");
        runtime.camera.pixels_per_unit = i4sim::initial_zoom;
        auto session = i4sim::create_session(seed, directory, blueprint, running, ups, output);
        // Without a launcher, LAUNCHER and the blueprint screen's Escape end the program.
        kinetra::run_session(runtime, *session, frames);
#else
        throw std::invalid_argument("Headless build requires --blueprint FILE --ticks N");
#endif
    } catch (const std::exception &e) {
        std::cerr << "i4sim: " << e.what() << '\n';
        return 1;
    }
}

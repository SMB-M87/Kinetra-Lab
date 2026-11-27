#include "i4sim/blueprint/scenario.h"
#include "i4sim/simulation/inspector.h"
#include "i4sim/simulation/simulation.h"
#include "kinetra_lab/inspector_trace.h"
#ifdef I4SIM_RUNTIME
#include "i4sim/presentation/session.h"
#endif
#include <algorithm>
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
        std::optional<std::string> inspect;
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
            else if (option == "--inspect")
                inspect = value;
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
                    "[--seed N] [--frames N] [--run] [--ups N] [--inspect MOVER_ID_OR_INDEX]");
        }
        if (ups == 0)
            throw std::invalid_argument("UPS must be in 1..10000");
        if (inspect && !ticks)
            throw std::invalid_argument("--inspect requires --blueprint FILE --ticks N");
        if (ticks) {
            if (!blueprint)
                throw std::invalid_argument("--ticks requires --blueprint FILE");
            i4sim::Simulation simulation(i4sim::load_blueprint(*blueprint), seed);
            if (const auto scenario = i4sim::load_scenario(*blueprint))
                i4sim::apply_scenario(*scenario, simulation);
            simulation.updates_per_second(ups);
            std::optional<std::size_t> mover;
            if (inspect) {
                const auto &movers = simulation.movers();
                const auto found = std::find_if(movers.begin(), movers.end(),
                                                [&](const auto &m) { return m.definition.id == *inspect; });
                if (found != movers.end()) {
                    mover = static_cast<std::size_t>(found - movers.begin());
                } else if (!inspect->empty() &&
                           std::ranges::all_of(*inspect, [](char c) { return c >= '0' && c <= '9'; })) {
                    const auto index = argument(*inspect, movers.size());
                    if (index >= movers.size())
                        throw std::invalid_argument("Unknown mover index: " + *inspect);
                    mover = static_cast<std::size_t>(index);
                } else {
                    throw std::invalid_argument("Unknown mover ID: " + *inspect);
                }
            }
            simulation.run();
            for (std::uint64_t i = 0; i < ticks && simulation.running(); ++i) {
                simulation.tick();
                if (mover) {
                    const auto snapshot = i4sim::inspect_mover(simulation, *mover);
                    kinetra_lab::write_inspector_tick(std::cout, simulation.ticks(),
                                                      simulation.movers()[*mover].definition.id, &snapshot);
                }
            }
            if (output)
                simulation.dump(*output);
            auto &summary = inspect ? std::cerr : std::cout;
            summary << "i4sim: " << simulation.ticks() << " ticks, " << simulation.completed()
                    << " completed products\n";
            return 0;
        }
#ifdef I4SIM_RUNTIME
        kinetra::Runtime runtime("i4sim");
        runtime.camera.pixels_per_unit = i4sim::initial_zoom;
        auto session = i4sim::create_session(seed, directory, blueprint, running, ups);
        // Without a launcher, the blueprint screen's LAUNCHER, Escape and M end the program; the
        // settings panel's BLUEPRINTS stays in i4sim.
        kinetra::run_session(runtime, *session, frames);
#else
        throw std::invalid_argument("Headless build requires --blueprint FILE --ticks N");
#endif
    } catch (const std::exception &e) {
        std::cerr << "i4sim: " << e.what() << '\n';
        return 1;
    }
}

// Runs the bundled large factories under one motion configuration and prints one CSV
// line per blueprint and seed: throughput, contacts, flow (waiting and stuck movers) and
// predictive passing outcomes. Outside CTest: comparisons are useful, but too slow for
// every test run.
#include "flow_metrics.h"
#include "simulation.h"
#include <algorithm>
#include <chrono>
#include <iostream>

int main(int argc, char **argv) {
    try {
        using namespace i4sim;
        bool recovery = false, parking_traffic = false, junction_traffic = false, corridor_traffic = false;
        bool predictive_passing = false;
        auto profile = SteeringProfile::improved;
        std::uint64_t ticks = 50000;
        for (int i = 1; i < argc; ++i) {
            const std::string option = argv[i];
            if (option == "--predictive-passing") {
                predictive_passing = true;
                continue;
            }
            if (option == "--no-predictive-passing") {
                predictive_passing = false;
                continue;
            }
            if (option == "--corridor-traffic") {
                corridor_traffic = true;
                continue;
            }
            if (option == "--junction-traffic") {
                junction_traffic = true;
                continue;
            }
            if (option == "--parking-traffic") {
                parking_traffic = true;
                continue;
            }
            if (option == "--recovery") {
                recovery = true;
                continue;
            }
            if (i + 1 >= argc)
                throw std::invalid_argument("Missing benchmark argument for " + option);
            const std::string value = argv[++i];
            if (option == "--profile") {
                if (value == "improved")
                    profile = SteeringProfile::improved;
                else if (value == "original")
                    profile = SteeringProfile::original;
                else if (value == "rvo")
                    profile = SteeringProfile::experimental_rvo;
                else if (value == "reciprocal")
                    profile = SteeringProfile::reciprocal;
                else
                    throw std::invalid_argument("Unknown steering profile: " + value);
            } else if (option == "--ticks") {
                std::size_t used{};
                if (value.empty() || value.front() == '-')
                    throw std::invalid_argument("Ticks must be in 1..1000000");
                ticks = std::stoull(value, &used);
                if (used != value.size() || ticks == 0 || ticks > 1000000)
                    throw std::invalid_argument("Ticks must be in 1..1000000");
            } else {
                throw std::invalid_argument("Usage: kinetra_i4sim_motion_benchmark [--recovery] "
                                            "[--parking-traffic] [--junction-traffic] [--corridor-traffic] "
                                            "[--predictive-passing|--no-predictive-passing] "
                                            "[--profile improved|original|rvo|reciprocal] [--ticks N]");
            }
        }
        std::cout
            << "blueprint,seed,completed,contacts,contact_ticks,acceleration_change,waiting_ticks,"
               "stuck_movers,stuck_at_end,passing_plans,passing_cancelled,passing_contacts,elapsed_ms\n";
        for (const char *name : {"MAS-demonstrator", "3840x2400", "8640x4800"}) {
            for (unsigned seed : {42u, 915839214u}) {
                Simulation simulation(
                    load_blueprint(default_blueprint_directory() / (std::string(name) + ".json")), seed);
                auto settings = simulation.motion_settings();
                settings.predictive_passing = predictive_passing;
                settings.recovery = recovery;
                settings.parking_traffic = parking_traffic;
                settings.junction_traffic = junction_traffic;
                settings.corridor_traffic = corridor_traffic;
                settings.profile = profile;
                simulation.configure_motion(settings);
                simulation.record_events(true);
                simulation.run();
                FlowMetrics flow;
                double acceleration_change = 0;
                const auto started = std::chrono::steady_clock::now();
                std::vector<Vec2> previous(simulation.movers().size());
                for (std::uint64_t tick = 0; tick < ticks && simulation.running(); ++tick) {
                    simulation.tick();
                    flow.observe(simulation);
                    for (std::size_t i = 0; i < previous.size(); ++i) {
                        const auto acceleration = simulation.movers()[i].body.acceleration;
                        acceleration_change += kinetra::distance(previous[i], acceleration);
                        previous[i] = acceleration;
                    }
                }
                const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                                         std::chrono::steady_clock::now() - started)
                                         .count();
                const auto events = simulation.events();
                const auto count = [&](auto predicate) {
                    return std::count_if(events.begin(), events.end(), predicate);
                };
                const auto plans =
                    count([](const SimulationEvent &e) { return e.kind == EventKind::passing_formed; });
                const auto cancelled = count([](const SimulationEvent &e) {
                    return e.kind == EventKind::passing_ended && e.end != PassingEnd::completed;
                });
                const auto passing_contacts =
                    count([](const SimulationEvent &e) { return e.kind == EventKind::contact && e.passing; });
                std::cout << name << ',' << seed << ',' << simulation.completed() << ','
                          << simulation.collisions() << ',' << simulation.contact_ticks() << ','
                          << acceleration_change << ',' << flow.waiting_ticks() << ',' << flow.stuck_movers()
                          << ',' << flow.stuck_now() << ',' << plans << ',' << cancelled << ','
                          << passing_contacts << ',' << elapsed << std::endl;
            }
        }
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}

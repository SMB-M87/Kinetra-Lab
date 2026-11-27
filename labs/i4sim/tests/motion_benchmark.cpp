// Runs the MAS demonstrator fixture and two bundled large factories under one motion
// configuration and prints one CSV line per blueprint and seed: throughput, contacts, flow
// (waiting and stuck movers) and predictive passing outcomes; `--events` also writes each
// run's event log. Outside CTest: comparisons are useful, but too slow for every test run.
#include "i4sim/simulation/flow_metrics.h"
#include "i4sim/simulation/simulation.h"
#include "i4sim/testing/event_counts.h"
#include "i4sim/testing/showcase.h"

#include <charconv>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
/// Writes every recorded event of one run as CSV, so a contact or a passing plan can be
/// replayed from its blueprint, seed and tick.
void write_events(const std::filesystem::path &file, std::span<const i4sim::SimulationEvent> events) {
    std::ofstream out(file);
    out << "tick,kind,plan,first,second,end,border,passing,stall_reason,stall_blocker,stall_detected_at\n";
    for (const auto &event : events) {
        out << event.tick << ',' << i4sim::name(event.kind) << ',' << event.plan << ',' << event.movers[0]
            << ',' << event.movers[1] << ',' << (event.end ? i4sim::name(*event.end) : "") << ','
            << event.border << ',' << event.passing << ',';
        if (event.stall) {
            out << i4sim::name(event.stall->reason) << ',';
            if (event.stall->blocker)
                out << *event.stall->blocker;
            out << ',' << event.stall->detected_at;
        } else {
            out << ",,";
        }
        out << '\n';
    }
    if (!out)
        throw std::runtime_error("Cannot write event log: " + file.string());
}
} // namespace

int main(int argc, char **argv) {
    try {
        using namespace i4sim;
        bool recovery = false, parking_traffic = false, junction_traffic = false, corridor_traffic = false;
        bool predictive_passing = false;
        auto profile = SteeringProfile::improved;
        std::uint64_t ticks = 50000;
        std::optional<std::filesystem::path> event_directory;
        std::vector<std::uint32_t> seeds;
        std::vector<std::filesystem::path> blueprints;
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
            } else if (option == "--events") {
                event_directory = value;
                std::filesystem::create_directories(*event_directory);
            } else if (option == "--blueprint") {
                blueprints.emplace_back(value);
            } else if (option == "--seed") {
                std::uint32_t seed{};
                const auto *end = value.data() + value.size();
                const auto parsed = std::from_chars(value.data(), end, seed);
                if (parsed.ec != std::errc{} || parsed.ptr != end)
                    throw std::invalid_argument("Invalid unsigned 32-bit seed: " + value);
                seeds.push_back(seed);
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
                                            "[--profile improved|original|rvo|reciprocal] [--ticks N] "
                                            "[--events DIRECTORY] [--blueprint FILE ...] [--seed N ...]");
            }
        }
        std::cout
            << "blueprint,seed,completed,contacts,contact_ticks,acceleration_change,waiting_ticks,"
               "stuck_movers,stuck_at_end,passing_plans,passing_cancelled,passing_contacts,contact_episodes,"
               "elapsed_ms\n";
        // MAS is a test fixture since it left the catalog; the CSV keeps its name, so runs
        // stay comparable with the recorded baselines.
        if (blueprints.empty())
            blueprints = {test::fixture_blueprint("MAS-demonstrator"),
                          default_blueprint_directory() / "3840x2400.json",
                          default_blueprint_directory() / "8640x4800.json"};
        if (seeds.empty())
            seeds = {42u, 915839214u};
        for (const auto &file : blueprints) {
            const auto name = file.stem().string();
            for (auto seed : seeds) {
                Simulation simulation(load_blueprint(file), seed);
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
                if (event_directory)
                    write_events(*event_directory / (name + '-' + std::to_string(seed) + ".csv"), events);
                std::cout << name << ',' << seed << ',' << simulation.completed() << ','
                          << simulation.collisions() << ',' << simulation.contact_ticks() << ','
                          << acceleration_change << ',' << flow.waiting_ticks() << ',' << flow.stuck_movers()
                          << ',' << flow.stuck_now() << ',' << test::count(events, EventKind::passing_formed)
                          << ',' << test::cancelled_plans(events) << ',' << test::passing_contacts(events)
                          << ',' << test::contact_episodes(events) << ',' << elapsed << std::endl;
            }
        }
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}

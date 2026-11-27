// Runs the MAS demonstrator fixture and two bundled large factories under one motion
// configuration and prints one CSV line per blueprint and seed: throughput, contacts, flow
// (waiting and stuck movers), predictive passing outcomes and ticks per second; `--events`
// also writes each run's event log. Outside CTest: comparisons are useful, but too slow for
// every test run.
#include "i4sim/simulation/flow_metrics.h"
#include "i4sim/simulation/simulation.h"
#include "i4sim/testing/event_counts.h"
#include "i4sim/testing/showcase.h"

#include <charconv>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <set>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {
/// Ticks of a run without `--ticks`: long enough to show a factory's throughput over hours of
/// simulated time, and the rate a long headless run reaches.
constexpr std::uint64_t default_ticks = 1000000;
/// Most ticks `--ticks` accepts.
constexpr std::uint64_t most_ticks = 100000000;

void write_csv_field(std::ostream &out, std::string_view field) {
    if (field.find_first_of(",\"\r\n") == std::string_view::npos) {
        out << field;
        return;
    }
    out << '"';
    for (char character : field) {
        if (character == '"')
            out << '"';
        out << character;
    }
    out << '"';
}

/// Writes every recorded event of one run as CSV, so a contact or a passing plan can be
/// replayed from its blueprint, seed and tick.
void write_events(const std::filesystem::path &file, std::span<const i4sim::SimulationEvent> events) {
    std::ofstream out(file);
    out << "tick,kind,plan,first,second,end,border,passing,stall_reason,stall_blocker,stall_detected_at,make_"
           "way_end\n";
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
        out << ',';
        if (event.make_way_end)
            out << i4sim::name(*event.make_way_end);
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
        bool predictive_passing = false, make_way = false;
        auto profile = SteeringProfile::improved;
        std::uint64_t ticks = default_ticks;
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
            if (option == "--make-way") {
                make_way = true;
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
                const auto *end = value.data() + value.size();
                const auto parsed = std::from_chars(value.data(), end, ticks);
                if (parsed.ec != std::errc{} || parsed.ptr != end || ticks == 0 || ticks > most_ticks)
                    throw std::invalid_argument("Ticks must be in 1.." + std::to_string(most_ticks));
            } else {
                throw std::invalid_argument("Usage: kinetra_i4sim_motion_benchmark [--recovery] "
                                            "[--parking-traffic] [--junction-traffic] [--corridor-traffic] "
                                            "[--predictive-passing|--no-predictive-passing] [--make-way] "
                                            "[--profile improved|original|rvo|reciprocal] [--ticks N] "
                                            "[--events DIRECTORY] [--blueprint FILE ...] [--seed N ...]");
            }
        }
        // MAS is a test fixture since it left the catalog; the CSV keeps its name, so runs
        // stay comparable with the recorded baselines.
        const bool numbered_logs = blueprints.size() > 1;
        if (blueprints.empty())
            blueprints = {test::fixture_blueprint("MAS-demonstrator"),
                          default_blueprint_directory() / "3840x2400.json",
                          default_blueprint_directory() / "8640x4800.json"};
        if (seeds.empty())
            seeds = {42u, 915839214u};
        if (event_directory) {
            if (std::set<std::uint32_t>(seeds.begin(), seeds.end()).size() != seeds.size())
                throw std::invalid_argument("Duplicate seed for event logs");
            std::filesystem::create_directories(*event_directory);
        }
        std::cout
            << "blueprint,seed,completed,contacts,contact_ticks,acceleration_change,waiting_ticks,"
               "stuck_movers,stuck_at_end,passing_plans,passing_cancelled,passing_contacts,contact_episodes,"
               "elapsed_ms,ticks_per_second\n";
        for (std::size_t index = 0; index < blueprints.size(); ++index) {
            const auto &file = blueprints[index];
            const auto name = file.stem().string();
            // Disjoint numeric prefixes separate explicit inputs even when
            // the volume treats some distinct Unicode stems as equivalent.
            const auto log_stem = numbered_logs ? std::to_string(index + 1) + '-' + name : name;
            for (auto seed : seeds) {
                Simulation simulation(load_blueprint(file), seed);
                auto settings = simulation.motion_settings();
                settings.predictive_passing = predictive_passing;
                settings.make_way = make_way;
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
                // Counted, because a simulation that stops running ends the loop early.
                std::uint64_t ran = 0;
                for (; ran < ticks && simulation.running(); ++ran) {
                    simulation.tick();
                    flow.observe(simulation);
                    for (std::size_t i = 0; i < previous.size(); ++i) {
                        const auto acceleration = simulation.movers()[i].body.acceleration;
                        acceleration_change += kinetra::distance(previous[i], acceleration);
                        previous[i] = acceleration;
                    }
                }
                const std::chrono::duration<double> seconds = std::chrono::steady_clock::now() - started;
                const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(seconds).count();
                const auto rate = seconds.count() > 0 ? std::llround(double(ran) / seconds.count()) : 0;
                const auto events = simulation.events();
                if (event_directory)
                    write_events(*event_directory / (log_stem + '-' + std::to_string(seed) + ".csv"), events);
                write_csv_field(std::cout, name);
                std::cout << ',' << seed << ',' << simulation.completed() << ',' << simulation.collisions()
                          << ',' << simulation.contact_ticks() << ',' << acceleration_change << ','
                          << flow.waiting_ticks() << ',' << flow.stuck_movers() << ',' << flow.stuck_now()
                          << ',' << test::count(events, EventKind::passing_formed) << ','
                          << test::cancelled_plans(events) << ',' << test::passing_contacts(events) << ','
                          << test::contact_episodes(events) << ',' << elapsed << ',' << rate << std::endl;
            }
        }
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}

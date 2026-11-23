#include "dotio/application.h"
#include "dotio/presentation/session.h"
#include "dotio/zoom.h"
#include "kinetra_lab/inspector_trace.h"
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <optional>
#include <random>
#include <stdexcept>

namespace {
void trace_enemy(std::uint32_t seed, std::size_t difficulty, std::uint64_t ticks, const std::string &id) {
    using kinetra_lab::inspector_argument;
    const auto dash = id.find('-');
    if (dash == std::string::npos)
        throw std::invalid_argument("Expected the enemy's slot and generation");
    const auto slot = inspector_argument<std::size_t>(id.substr(0, dash));
    const auto generation = inspector_argument<std::uint64_t>(id.substr(dash + 1));
    dotio::Application application(seed, static_cast<dotio::Difficulty>(difficulty));
    if (slot >= application.world().get_enemies().get_size())
        throw std::invalid_argument("Unknown enemy slot");
    kinetra::Camera2D camera;
    camera.pixels_per_unit = dotio::initial_zoom;
    const auto title = "ENEMY " + id;
    for (std::uint64_t tick = 0; tick < ticks; ++tick) {
        application.advance(dotio::Application::tick_seconds, {}, camera);
        const auto snapshot = application.inspect_enemy(slot, generation);
        kinetra_lab::write_inspector_tick(std::cout, tick + 1, title, snapshot ? &*snapshot : nullptr);
    }
}
} // namespace

int main(int argc, char **argv) {
    try {
        int frames = 0;
        auto difficulty = dotio::default_difficulty;
        std::optional<std::uint32_t> seed;
        std::uint64_t ticks = 0;
        std::optional<std::string> inspect;
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
            } else if (option == "--ticks" && i + 1 < argc)
                ticks = kinetra_lab::inspector_argument<std::uint64_t>(argv[++i]);
            else if (option == "--inspect" && i + 1 < argc)
                inspect = argv[++i];
            else
                throw std::invalid_argument("Usage: kinetra_dotio [--difficulty easy|medium|hard] [--seed N] "
                                            "[--frames N | --ticks N --inspect SLOT-GENERATION]");
        }
        if (inspect || ticks) {
            if (!inspect || ticks == 0 || !seed || frames)
                throw std::invalid_argument(
                    "--inspect requires --seed N and positive --ticks N instead of --frames");
            trace_enemy(*seed, difficulty, ticks, *inspect);
            return 0;
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

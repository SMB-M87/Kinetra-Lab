// Characterization of Demo crowds before the Demo code is restructured.
//
// Each scenario drives a seeded SandboxApplication frame by frame and checks crowd
// invariants after every frame: finite motion, no exhausted sweep budget, a consistent
// agent-to-body mapping and no deep overlap between moving bodies. It also records a
// 64-bit fingerprint of every moving body's motion, so the run as a whole is pinned. A
// behavior-preserving refactor leaves every expected line unchanged; a deliberate
// change updates the table and says why in the commit message.
//
// Usage: kinetra_demo_crowd_characterization_tests [--print]
//   --print  Write the current summary of every scenario, for a deliberate update.
#include "sandbox_application.h"
#include "test_support.h"

#include <kinetra/physics.h>

#include <cstdint>
#include <exception>
#include <functional>
#include <iomanip>
#include <iostream>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {
using lab::test::Fingerprint;
using lab::test::require;

/// Elapsed time per frame in seconds; the Demo's fixed clock steps at the same rate.
constexpr double frame_seconds = 1.0 / 120;
/// Largest allowed overlap between two moving bodies, in world units. Contact correction
/// leaves about 0.001; a larger overlap means bodies pass into each other.
constexpr float max_penetration = 0.01f;
/// The overlap check is quadratic in the number of moving bodies, so it runs every few frames.
constexpr int overlap_interval_frames = 20;

void check_invariants(const sandbox::Sandbox &scene, int frame) {
    const auto &world = scene.world;
    require(!world.sweep_budget_exhausted(), "Crowd contact exhausted the swept-event budget");
    const auto bodies = world.bodies();
    require(scene.moving_body_count <= bodies.size(), "More moving bodies than world bodies");
    for (const auto &body : bodies)
        require(kinetra::finite(body.position) && kinetra::finite(body.velocity), "Nonfinite body motion");
    require(scene.agents().size() <= sandbox::Sandbox::maximum_agents, "Population exceeded its cap");
    std::set<std::size_t> agent_bodies;
    for (const auto &agent : scene.agents()) {
        require(agent.body_id() < scene.moving_body_count, "Agent refers to a body outside the movers");
        require(agent_bodies.insert(agent.body_id()).second, "Two agents share one body");
    }
    require(scene.player < scene.moving_body_count && !agent_bodies.contains(scene.player),
            "Player body is not a separate mover");
    if (frame % overlap_interval_frames != 0)
        return;
    for (std::size_t a = 0; a < scene.moving_body_count; ++a)
        for (std::size_t b = a + 1; b < scene.moving_body_count; ++b)
            if (const auto contact = kinetra::collide(bodies[a], bodies[b]))
                require(contact->penetration <= max_penetration, "Moving bodies overlap too deeply");
}

struct Scenario {
    std::string_view name;
    int agent_count;
    std::uint32_t seed;
    int frames;
    /// Intent for one frame; the default is an idle player with a following camera.
    std::function<sandbox::FrameCommands(int frame)> commands;
    std::string_view expected;
};

std::string run(const Scenario &scenario) {
    sandbox::SandboxApplication application(scenario.agent_count, scenario.seed);
    kinetra::Camera2D camera;
    camera.viewport = {1280, 800};
    camera.pixels_per_unit = 17;
    Fingerprint trajectory;
    std::uint64_t avoidance_limited = 0;
    for (int frame = 0; frame < scenario.frames; ++frame) {
        application.advance(frame_seconds, scenario.commands(frame), camera);
        const auto &scene = application.scene();
        try {
            check_invariants(scene, frame);
        } catch (const std::exception &error) {
            throw std::runtime_error("frame " + std::to_string(frame) + ": " + error.what());
        }
        avoidance_limited += scene.avoidance_limited;
        const auto bodies = scene.world.bodies();
        for (std::size_t i = 0; i < scene.moving_body_count; ++i) {
            trajectory.add(bodies[i].position);
            trajectory.add(bodies[i].velocity);
            trajectory.add(bodies[i].angle);
            trajectory.add(bodies[i].angular_velocity);
        }
        trajectory.add(camera.position);
    }
    const auto &scene = application.scene();
    std::ostringstream line;
    line << "agents=" << scene.agents().size() << " moving_bodies=" << scene.moving_body_count
         << " bodies=" << scene.world.bodies().size() << " avoidance_limited=" << avoidance_limited
         << " trajectory=" << std::hex << std::setw(16) << std::setfill('0') << trajectory.value();
    return line.str();
}

sandbox::FrameCommands idle(int) {
    return {};
}

/// The player walks east, then north, so new chunks stream in and get populated. Early on,
/// the agents near a screen point get a commanded goal.
sandbox::FrameCommands walk_through_chunks(int frame) {
    sandbox::FrameCommands commands;
    commands.player.direction = frame < 600 ? kinetra::Vec2{1, 0} : kinetra::Vec2{0, 1};
    if (frame == 120)
        commands.pointer = sandbox::PointerCommand{sandbox::PointerAction::target_agents, {900, 300}};
    return commands;
}

std::vector<Scenario> scenarios() {
    return {
        {"dense idle crowd", 240, 123, 480, idle,
         "agents=312 moving_bodies=319 bodies=681 avoidance_limited=712 trajectory=8c5d12a5aad93398"},
        {"player walks through streamed chunks", 40, 456, 1200, walk_through_chunks,
         "agents=208 moving_bodies=215 bodies=591 avoidance_limited=3520 trajectory=cdaaac0c9bcdef5b"},
    };
}
} // namespace

int main(int argc, char **argv) {
    const bool print = argc == 2 && std::string_view(argv[1]) == "--print";
    if (argc > 2 || (argc == 2 && !print)) {
        std::cerr << "Usage: kinetra_demo_crowd_characterization_tests [--print]\n";
        return 2;
    }
    int failures = 0;
    for (const auto &scenario : scenarios()) {
        try {
            const auto actual = run(scenario);
            if (print) {
                std::cout << scenario.name << "\n    \"" << actual << "\"\n";
            } else if (actual != scenario.expected) {
                std::cerr << "FAIL " << scenario.name << "\n  expected: " << scenario.expected
                          << "\n  actual:   " << actual << '\n';
                ++failures;
            } else {
                std::cout << "PASS " << scenario.name << '\n';
            }
        } catch (const std::exception &error) {
            std::cerr << "FAIL " << scenario.name << ": " << error.what() << '\n';
            ++failures;
        }
    }
    return failures == 0 ? 0 : 1;
}

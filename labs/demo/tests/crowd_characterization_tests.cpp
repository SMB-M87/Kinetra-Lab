// Characterization of Demo crowds before the Demo code is restructured.
//
// Each scenario drives a seeded SandboxApplication frame by frame and checks crowd
// invariants after every frame: finite motion, no exhausted sweep budget, a consistent
// agent-to-body mapping and no deep overlap between moving bodies. It also records a
// 64-bit fingerprint of every moving body's motion, so the run as a whole is pinned. A
// behavior-preserving refactor leaves every expected line unchanged; a deliberate
// change updates the line and says why in the commit message.
//
// Every scenario runs in both wander modes: agents that steer without a route, which is
// the default, and agents that plan wander routes. The invariants hold for both.
//
// Set KINETRA_PRINT_GOLDEN=1 to print the current line of every scenario, for a
// deliberate update. The run still compares, so a changed line also fails its case
// and shows expected and actual.
#include "demo/sandbox_application.h"
#include "kinetra_lab/testing/test_support.h"

#include <kinetra/simulation/physics.h>

#include <doctest/doctest.h>

#include <cstdint>
#include <cstdlib>
#include <functional>
#include <iomanip>
#include <set>
#include <sstream>
#include <string>
#include <string_view>

namespace {
using kinetra_lab::testing::Fingerprint;

/// Elapsed time per frame in seconds; the Demo's fixed clock steps at the same rate.
constexpr double frame_seconds = 1.0 / 120;
/// Largest allowed overlap between two moving bodies, in world units. Contact correction
/// leaves about 0.001; a larger overlap means bodies pass into each other.
constexpr float max_penetration = 0.01f;
/// The overlap check is quadratic in the number of moving bodies, so it runs every few frames.
constexpr int overlap_interval_frames = 20;

/// Checks the crowd invariants after one frame. The first violation stops the scenario,
/// because later frames build on the broken state.
void check_invariants(const demo::Sandbox &scene, int frame) {
    const auto &world = scene.world;
    REQUIRE_MESSAGE(!world.sweep_budget_exhausted(), "Crowd contact exhausted the swept-event budget");
    const auto bodies = world.bodies();
    REQUIRE_MESSAGE(scene.moving_body_count <= bodies.size(), "More moving bodies than world bodies");
    for (const auto &body : bodies)
        REQUIRE_MESSAGE((kinetra::finite(body.position) && kinetra::finite(body.velocity)),
                        "Nonfinite body motion");
    REQUIRE_MESSAGE(scene.agents().size() <= demo::Sandbox::maximum_agents, "Population exceeded its cap");
    std::set<std::size_t> agent_bodies;
    for (const auto &agent : scene.agents()) {
        REQUIRE_MESSAGE(agent.body_id() < scene.moving_body_count,
                        "Agent refers to a body outside the movers");
        REQUIRE_MESSAGE(agent_bodies.insert(agent.body_id()).second, "Two agents share one body");
    }
    REQUIRE_MESSAGE((scene.player < scene.moving_body_count && !agent_bodies.contains(scene.player)),
                    "Player body is not a separate mover");
    if (frame % overlap_interval_frames != 0)
        return;
    for (std::size_t a = 0; a < scene.moving_body_count; ++a)
        for (std::size_t b = a + 1; b < scene.moving_body_count; ++b)
            if (const auto contact = kinetra::collide(bodies[a], bodies[b]))
                REQUIRE_MESSAGE(contact->penetration <= max_penetration, "Moving bodies overlap too deeply");
}

/// One seeded crowd run and the summary line it must reproduce.
struct Scenario {
    int agent_count;
    std::uint32_t seed;
    int frames;
    /// Intent for one frame; the default is an idle player with a following camera.
    std::function<demo::FrameCommands(int frame)> commands;
    /// How the agents wander for the whole run.
    demo::WanderMode wander;
};

/// Runs `scenario` with the invariant checks after every frame and returns its summary:
/// final population and body counts, the summed avoidance limits and the fingerprint.
std::string run(const Scenario &scenario) {
    demo::SandboxApplication application(scenario.agent_count, scenario.seed);
    application.tuning().wander.mode = scenario.wander;
    kinetra::Camera2D camera;
    camera.viewport = {1280, 800};
    camera.pixels_per_unit = 17;
    Fingerprint trajectory;
    std::uint64_t avoidance_limited = 0;
    for (int frame = 0; frame < scenario.frames; ++frame) {
        application.advance(frame_seconds, scenario.commands(frame), camera);
        const auto &scene = application.scene();
        INFO("frame " << frame);
        check_invariants(scene, frame);
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

demo::FrameCommands idle(int) {
    return {};
}

/// The player walks east, then north, so new chunks stream in and get populated. Early on,
/// the agents near a screen point get a commanded goal.
demo::FrameCommands walk_through_chunks(int frame) {
    demo::FrameCommands commands;
    commands.player.direction = frame < 600 ? kinetra::Vec2{1, 0} : kinetra::Vec2{0, 1};
    if (frame == 120)
        commands.pointer = demo::PointerCommand{demo::PointerAction::target_agents, {900, 300}};
    return commands;
}

/// True when KINETRA_PRINT_GOLDEN=1 asks for the current lines.
bool print_golden() {
    const char *print = std::getenv("KINETRA_PRINT_GOLDEN");
    return print && std::string_view(print) == "1";
}

/// Compares the summary of `scenario` with `expected`; a mismatch shows both lines.
void check_scenario(std::string_view name, const Scenario &scenario, const std::string &expected) {
    const std::string actual = run(scenario);
    if (print_golden())
        MESSAGE(name << "\n    \"" << actual << "\"");
    CHECK(actual == expected);
}
} // namespace

TEST_CASE("a dense idle crowd that wanders by steering reproduces its recorded summary") {
    check_scenario(
        "dense idle crowd, steering wander", {240, 123, 480, idle, demo::WanderMode::steering},
        "agents=312 moving_bodies=319 bodies=681 avoidance_limited=3047 trajectory=25eff655f7a75f9e");
}

TEST_CASE("a dense idle crowd that wanders by route reproduces its recorded summary") {
    check_scenario(
        "dense idle crowd, route wander", {240, 123, 480, idle, demo::WanderMode::route},
        "agents=312 moving_bodies=319 bodies=681 avoidance_limited=722 trajectory=17080ac73d99bce9");
}

TEST_CASE("a player walking through streamed chunks of steering wanderers reproduces its recorded summary") {
    check_scenario(
        "player walks through streamed chunks, steering wander",
        {40, 456, 1200, walk_through_chunks, demo::WanderMode::steering},
        "agents=208 moving_bodies=215 bodies=591 avoidance_limited=2623 trajectory=377eeb3999727f88");
}

TEST_CASE("a player walking through streamed chunks of route wanderers reproduces its recorded summary") {
    check_scenario(
        "player walks through streamed chunks, route wander",
        {40, 456, 1200, walk_through_chunks, demo::WanderMode::route},
        "agents=208 moving_bodies=215 bodies=591 avoidance_limited=2756 trajectory=37e262315f729cfb");
}

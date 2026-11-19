// Headless contract tests for the i4sim simulation: manual control, parking, steering
// profiles, contacts, junction, corridor, recovery, predictive passing, the event log,
// parking traffic, the bundled blueprints and showcase scenarios, recipes, workloads,
// determinism and export.
//
// The `the bundled catalog holds seven blueprints that all load` case doubles as the
// resource probe of relocation_tests.cmake, which runs it from a relocated copy.
#include "flow_metrics.h"
#include "recorder.h"
#include "scenario.h"
#include "showcase.h"
#include "simulation.h"

#include <doctest/doctest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace i4sim;

namespace {
/// A 2400 by 1600 factory with one APM4220 mover at (1000, 700) and four producers in
/// the corners; the Manuel producer is rectangular.
Blueprint fixture() {
    Blueprint b;
    b.name = "local factory";
    b.dimension = {2400, 1600};
    b.cell_size = {200, 200};
    b.mover_max_extent = 155;
    b.producer_max_queue = 2;
    b.tick_cap = 1000000;
    b.movers = {{"APM4220_1", Model::APM4220, {1000, 700}}};
    b.producers = {{"Kuka_1", Model::Kuka, {100, 100}, {300, 250}},
                   {"Staubli_1", Model::Staubli, {2300, 100}, {1800, 250}},
                   {"Viper_1", Model::Viper, {100, 1500}, {300, 1200}},
                   {"Manuel_1", Model::Manuel, {2300, 1500}, {1800, 1200}, true}};
    return b;
}
/// The junction fixture: a single-cell crossing with a west and a north mover.
Blueprint crossing_fixture() {
    return load_blueprint(i4sim::test::showcase_blueprint("junction-crossing"));
}

/// Starts the two crossing movers toward the opposite arms under reciprocal steering,
/// with or without junction traffic.
void start_crossing(Simulation &s, bool enabled) {
    s.set_workload(false);
    auto config = s.motion_settings();
    config.profile = SteeringProfile::reciprocal;
    config.junction_traffic = enabled;
    s.configure_motion(config);
    REQUIRE_MESSAGE((s.move_to(0, {1100, 700}) && s.move_to(1, {700, 1100})), "Crossing goals rejected");
    s.run();
}

/// The crossing fixture plus a third mover approaching the junction from the east.
Blueprint opposing_fixture() {
    auto b = crossing_fixture();
    b.movers.push_back({"east approach", Model::APM4220, {1143.5f, 643.5f}});
    return b;
}

/// Two rooms joined by a straight single-cell corridor, with one mover in each room.
Blueprint corridor_fixture() {
    return load_blueprint(i4sim::test::showcase_blueprint("corridor-passing"));
}

/// Starts both corridor movers toward their goals under reciprocal steering, with or
/// without corridor traffic.
void start_corridor(Simulation &s, bool enabled, Vec2 west_goal, Vec2 east_goal) {
    s.set_workload(false);
    auto config = s.motion_settings();
    config.profile = SteeringProfile::reciprocal;
    config.corridor_traffic = enabled;
    s.configure_motion(config);
    REQUIRE_MESSAGE((s.move_to(0, west_goal) && s.move_to(1, east_goal)), "Corridor goals rejected");
    s.run();
}

/// The corridor fixture with both movers already inside the corridor.
Blueprint recovery_fixture() {
    return load_blueprint(i4sim::test::showcase_blueprint("corridor-recovery"));
}

/// The open-floor passing fixture with both movers moved next to the north wall.
Blueprint near_wall_fixture() {
    auto b = load_blueprint(i4sim::test::showcase_blueprint("predictive-passing"));
    b.movers[0].position.y = b.movers[1].position.y = 43.5f;
    return b;
}

/// Default motion with predictive passing on and heatmap costs off, for `profile`.
MotionSettings passing_motion(SteeringProfile profile) {
    MotionSettings config;
    config.profile = profile;
    config.predictive_passing = true;
    config.heatmap = false;
    return config;
}

/// The open-floor passing fixture with both movers sent to the other's start under
/// `profile`, unpaused, with the event log on when `record` is set.
Simulation passing_pair(SteeringProfile profile, bool record = true) {
    Simulation pair(load_blueprint(i4sim::test::showcase_blueprint("predictive-passing")));
    pair.set_workload(false);
    pair.configure_motion(passing_motion(profile));
    REQUIRE_MESSAGE((pair.move_to(0, {1900, 800}) && pair.move_to(1, {500, 800})), "Passing goals rejected");
    pair.record_events(record);
    pair.run();
    return pair;
}

/// Ticks `simulation` until it holds a passing plan, for at most 5000 ticks.
void run_until_pair(Simulation &simulation) {
    while (simulation.predictive_pairs() == 0 && simulation.ticks() < 5000)
        simulation.tick();
    REQUIRE_MESSAGE(simulation.predictive_pairs() == 1, "No passing plan formed");
}

/// Runs a frontal or crossing two-mover encounter under the Original, Improved and
/// Reciprocal profiles. Improved and Reciprocal must reach both goals without contact.
void check_encounters(bool crossing) {
    std::uint64_t reference_contacts{};
    for (auto profile : {SteeringProfile::original, SteeringProfile::improved, SteeringProfile::reciprocal}) {
        CAPTURE(profile);
        auto b = fixture();
        b.producers.clear();
        b.movers = {{"A", Model::APM4220, {500, 700}},
                    {"B", Model::APM4220, crossing ? Vec2{1000, 200} : Vec2{1500, 700}}};
        Simulation trial(b);
        trial.set_workload(false);
        auto settings = trial.motion_settings();
        settings.profile = profile;
        trial.configure_motion(settings);
        const auto a = trial.movers()[0].body.position;
        const auto c = trial.movers()[1].body.position;
        REQUIRE_MESSAGE(trial.move_to(0, crossing ? Vec2{1700, a.y} : c), "Encounter goal A rejected");
        REQUIRE_MESSAGE(trial.move_to(1, crossing ? Vec2{c.x, 1300} : a), "Encounter goal B rejected");
        trial.run();
        for (int tick = 0; tick < 5000; ++tick)
            trial.tick();
        INFO("contacts " << trial.collisions() << " moving " << trial.movers()[0].moving()
                         << trial.movers()[1].moving());
        if (profile == SteeringProfile::original)
            reference_contacts = trial.collisions();
        else {
            CHECK_MESSAGE((!trial.movers()[0].moving() && !trial.movers()[1].moving()),
                          "Encounter did not reach both goals");
            CHECK_MESSAGE((trial.collisions() == 0 && trial.collisions() <= reference_contacts),
                          "Encounter increased contacts");
        }
    }
}

/// Runs a large bundled factory with seed 915839214 and route recovery for 50000 ticks.
/// Recovery must be exercised, poses must stay finite and products must complete.
void check_large_factory_recovery(const char *name) {
    const auto large = load_blueprint(default_blueprint_directory() / name);
    Simulation recovery(large, 915839214);
    auto recovery_settings = recovery.motion_settings();
    recovery_settings.recovery = true;
    recovery.configure_motion(recovery_settings);
    recovery.run();
    bool recovered = false;
    for (int i = 0; i < 50000; ++i) {
        recovery.tick();
        for (const auto &mover : recovery.movers()) {
            recovered = recovered || mover.detour;
            CHECK_MESSAGE(kinetra::finite(mover.body.position), "Route recovery produced an invalid pose");
        }
    }
    CHECK_MESSAGE(recovered, "Large factory regression never exercised route recovery");
    CHECK_MESSAGE(recovery.completed() > 0, "Large factory workload did not complete any products");
}

/// The fixture with seed 7, run unpaused for 2000 ticks.
Simulation seeded_run() {
    Simulation simulation(fixture(), 7);
    simulation.run();
    for (int i = 0; i < 2000; ++i)
        simulation.tick();
    return simulation;
}

/// Describes the open products and the first mover, for a recipe that did not complete.
std::string recipe_diagnostics(const Simulation &simulation) {
    std::ostringstream text;
    for (const auto &p : simulation.products())
        text << "stuck product step=" << p.step << " state=" << int(p.state) << '\n';
    const auto &m = simulation.movers()[0];
    text << "mover=" << m.body.position.x << "," << m.body.position.y << " dest=" << m.destination.x << ","
         << m.destination.y << " moving=" << m.moving();
    return text.str();
}

/// Describes every mover of a workload that completed too few products.
std::string workload_diagnostics(const Simulation &factory) {
    std::ostringstream text;
    text << "MAS completed=" << factory.completed();
    for (const auto &m : factory.movers())
        text << '\n'
             << m.definition.id << " pos=" << m.body.position.x << ',' << m.body.position.y
             << " goal=" << m.destination.x << ',' << m.destination.y << " moving=" << m.moving()
             << " unreachable=" << (m.lifecycle == MoverLifecycle::unreachable) << " route=" << m.route.size()
             << " blocked-start="
             << !factory.grid().clear(m.body.position, m.body.position, m.body.half_extents);
    return text.str();
}
} // namespace

TEST_CASE("reciprocal control keeps physical tick units at any update rate") {
    Simulation normal(fixture()), accelerated(fixture());
    for (auto *simulation : {&normal, &accelerated}) {
        simulation->set_workload(false);
        auto config = simulation->motion_settings();
        config.profile = SteeringProfile::reciprocal;
        simulation->configure_motion(config);
        REQUIRE_MESSAGE(simulation->move_to(0, {1300, 800}), "Reciprocal tick goal rejected");
    }
    accelerated.updates_per_second(1000);
    normal.single_step();
    accelerated.single_step();
    CHECK_MESSAGE(
        (kinetra::distance(normal.movers()[0].body.position, accelerated.movers()[0].body.position) == 0 &&
         std::abs(kinetra::length(normal.movers()[0].body.velocity) - normal.motion_settings().force) <
             0.00001f),
        "Reciprocal controller changed physical tick units with UPS");
    CHECK_MESSAGE(normal.avoidance_limited() == 0, "Unobstructed reciprocal control reported a limitation");
}

TEST_CASE("reciprocal departure through a narrow gap stays bounded and leaves parked neighbors in place") {
    auto b = fixture();
    b.producers.clear();
    const auto size = mover_spec(Model::APM4220).dimension;
    const Vec2 home{800, 500};
    b.movers = {{"moving", Model::APM4220, home},
                {"left", Model::APM4220, home - Vec2{size.x + 8, 0}},
                {"right", Model::APM4220, home + Vec2{size.x + 8, 0}}};
    Simulation narrow(b);
    narrow.set_workload(false);
    auto config = narrow.motion_settings();
    config.profile = SteeringProfile::reciprocal;
    narrow.configure_motion(config);
    const auto parked = narrow.movers()[1].body.position;
    REQUIRE_MESSAGE(narrow.move_to(0, Vec2{800, 950} + size / 2), "Reciprocal departure goal rejected");
    narrow.single_step();
    CHECK_MESSAGE((narrow.avoidance_limited() == 0 &&
                   kinetra::length(narrow.movers()[0].body.acceleration) <= config.force + 0.00001f),
                  "Valid rectangular gap was rejected or exceeded acceleration bounds");
    CHECK_MESSAGE(kinetra::distance(parked, narrow.movers()[1].body.position) == 0,
                  "Reciprocal avoidance displaced a parked mover");
}

TEST_CASE("junction admission resolves the narrow crossing exclusively and deterministically") {
    Simulation reference(crossing_fixture()), trial(crossing_fixture()), replay(crossing_fixture());
    start_crossing(reference, false);
    start_crossing(trial, true);
    start_crossing(replay, true);
    CHECK_MESSAGE((trial.junction_count() == 1 && trial.junction_center(0).x == 700 &&
                   trial.junction_center(0).y == 700),
                  "Isolated crossing detection failed");
    bool waited = false, checked_block = false, checked_late_cancel = false;
    for (int tick = 0; tick < 1000; ++tick) {
        trial.tick();
        replay.tick();
        int occupying = 0;
        for (std::size_t i = 0; i < trial.movers().size(); ++i) {
            const auto &m = trial.movers()[i];
            CHECK_MESSAGE(kinetra::distance(m.body.position, replay.movers()[i].body.position) == 0,
                          "Junction traffic was nondeterministic");
            CHECK_MESSAGE(kinetra::length(m.body.acceleration) <= trial.motion_settings().force + 0.00001f,
                          "Junction stopping exceeded acceleration bounds");
            CHECK_MESSAGE(
                (m.destination.x == (i == 0 ? 1100 : 700) && m.destination.y == (i == 0 ? 700 : 1100)),
                "Junction admission changed a manual goal");
            const auto delta = m.body.position - Vec2{700, 700};
            if (std::abs(delta.x) < 102 + m.body.half_extents.x &&
                std::abs(delta.y) < 102 + m.body.half_extents.y)
                ++occupying;
        }
        CHECK_MESSAGE(occupying <= 1, "Two movers entered an exclusively reserved crossing");
        const auto &approach = trial.movers()[0].body;
        const float front = approach.position.x + approach.half_extents.x;
        if (!checked_late_cancel && front > 596 && front < 598) {
            auto cancelled = trial;
            REQUIRE_MESSAGE(cancelled.move_to(0, {100, 700}), "Late cancellation goal rejected");
            for (int t = 0; t < 1000; ++t) {
                cancelled.tick();
                int present = 0;
                for (const auto &m : cancelled.movers()) {
                    const auto delta = m.body.position - Vec2{700, 700};
                    if (std::abs(delta.x) < 102 + m.body.half_extents.x &&
                        std::abs(delta.y) < 102 + m.body.half_extents.y)
                        ++present;
                }
                CHECK_MESSAGE(present <= 1, "Late goal change released a coasting approach too early");
            }
            CHECK_MESSAGE((!cancelled.movers()[0].moving() && !cancelled.movers()[1].moving() &&
                           cancelled.collisions() == 0),
                          "Late cancellation failed to release after braking");
            checked_late_cancel = true;
        }
        waited = waited || trial.junction_waiting() > 0;
        if (!checked_block && occupying == 1 && trial.junction_waiting() == 1) {
            auto blocked = trial;
            blocked.toggle_mover(0);
            const auto held = blocked.movers()[0].body.position;
            for (int t = 0; t < 200; ++t)
                blocked.tick();
            CHECK_MESSAGE(
                (kinetra::distance(held, blocked.movers()[0].body.position) == 0 &&
                 blocked.movers()[1].body.position.y + blocked.movers()[1].body.half_extents.y <= 598.001f),
                "Blocked physical occupant released the crossing");
            blocked.toggle_mover(0);
            for (int t = 0; t < 1000; ++t)
                blocked.tick();
            CHECK_MESSAGE((!blocked.movers()[0].moving() && !blocked.movers()[1].moving()),
                          "Crossing did not resume after its occupant was unblocked");
            auto disabled = trial;
            disabled.pause();
            auto config = disabled.motion_settings();
            config.junction_traffic = false;
            disabled.configure_motion(config);
            CHECK_MESSAGE((disabled.junction_waiting() == 0 &&
                           kinetra::distance(disabled.movers()[0].body.position, held) == 0),
                          "Disabling junction traffic retained a wait or teleported a mover");
            checked_block = true;
        }
    }
    for (int t = 0; t < 5000; ++t)
        reference.tick();
    CHECK_MESSAGE((reference.movers()[1].moving() && waited && checked_block && checked_late_cancel &&
                   trial.junction_waiting() == 0 && !trial.movers()[0].moving() &&
                   !trial.movers()[1].moving() && trial.collisions() == 0),
                  "Junction admission did not resolve the narrow crossing regression");
}

TEST_CASE("open floor is not mistaken for a corridor crossing") {
    CHECK_MESSAGE(Simulation(fixture()).junction_count() == 0,
                  "Open floor was mistaken for a corridor crossing");
}

TEST_CASE("crossing admission works for every approach orientation") {
    for (int orientation = 1; orientation <= 3; ++orientation) {
        CAPTURE(orientation);
        auto b = crossing_fixture();
        const float angle = float(orientation) * 1.57079632679f;
        const Vec2 half = mover_spec(Model::APM4220).dimension / 2;
        for (auto &m : b.movers)
            m.position = Vec2{700, 700} + kinetra::rotated(m.position + half - Vec2{700, 700}, angle) - half;
        Simulation rotated(b);
        rotated.set_workload(false);
        auto config = rotated.motion_settings();
        config.profile = SteeringProfile::reciprocal;
        config.junction_traffic = true;
        rotated.configure_motion(config);
        REQUIRE_MESSAGE((rotated.move_to(0, Vec2{700, 700} + kinetra::rotated({400, 0}, angle)) &&
                         rotated.move_to(1, Vec2{700, 700} + kinetra::rotated({0, 400}, angle))),
                        "Rotated crossing goal rejected");
        rotated.run();
        for (int t = 0; t < 1500; ++t)
            rotated.tick();
        CHECK_MESSAGE(
            (!rotated.movers()[0].moving() && !rotated.movers()[1].moving() && rotated.collisions() == 0),
            "Crossing admission depended on approach orientation");
    }
}

TEST_CASE("a T-junction admits a turning movement") {
    auto tee_blueprint = crossing_fixture();
    tee_blueprint.zones.push_back({"closed south", {600, 800}, {200, 600}});
    Simulation tee(tee_blueprint);
    tee.set_workload(false);
    auto config = tee.motion_settings();
    config.profile = SteeringProfile::reciprocal;
    config.junction_traffic = true;
    tee.configure_motion(config);
    CHECK_MESSAGE((tee.junction_count() == 1 && tee.move_to(0, {1100, 700}) && tee.move_to(1, {300, 700})),
                  "T-junction detection or goals failed");
    tee.run();
    for (int t = 0; t < 2000; ++t)
        tee.tick();
    CHECK_MESSAGE((!tee.movers()[0].moving() && !tee.movers()[1].moving() && tee.collisions() == 0),
                  "T-junction admission failed to clear a turning movement");
}

TEST_CASE("a cancelled approach releases its junction admission") {
    Simulation cancelled(crossing_fixture());
    start_crossing(cancelled, true);
    cancelled.tick();
    REQUIRE_MESSAGE(cancelled.move_to(0, {100, 700}), "Cancelled approach goal rejected");
    for (int t = 0; t < 1000; ++t)
        cancelled.tick();
    CHECK_MESSAGE(
        (!cancelled.movers()[0].moving() && !cancelled.movers()[1].moving() && cancelled.collisions() == 0),
        "Cancelled approach retained a stale junction admission");
}

TEST_CASE("a mover waits outside a junction until its blocked exit clears in every orientation") {
    for (int orientation = 0; orientation < 4; ++orientation) {
        CAPTURE(orientation);
        auto b = crossing_fixture();
        b.movers.push_back({"blocked east", Model::APM4220, {843.5f, 643.5f}});
        const float angle = float(orientation) * 1.57079632679f;
        const Vec2 half = mover_spec(Model::APM4220).dimension / 2;
        auto rotate = [&](Vec2 point) {
            return Vec2{700, 700} + kinetra::rotated(point - Vec2{700, 700}, angle);
        };
        for (auto &m : b.movers)
            m.position = rotate(m.position + half) - half;
        Simulation trial(b);
        trial.set_workload(false);
        auto config = trial.motion_settings();
        config.profile = SteeringProfile::reciprocal;
        config.junction_traffic = true;
        trial.configure_motion(config);
        trial.toggle_mover(2);
        REQUIRE_MESSAGE((trial.move_to(0, rotate({1100, 700})) && trial.move_to(1, rotate({700, 1100}))),
                        "Blocked-exit goals rejected");
        trial.run();
        bool saw_exit_wait = false, saw_free_exit = false;
        for (int t = 0; t < 1000; ++t) {
            trial.tick();
            saw_exit_wait = saw_exit_wait || trial.movers()[0].coordination.exit_waiting;
            saw_free_exit = saw_free_exit || !trial.movers()[1].moving();
            const auto &west = trial.movers()[0];
            const auto approach = kinetra::rotated({1, 0}, angle);
            CHECK_MESSAGE(kinetra::dot(west.body.position - Vec2{700, 700}, approach) + half.x <= -101.999f,
                          "Mover entered a junction with a blocked exit");
            CHECK_MESSAGE(kinetra::length(west.body.acceleration) <= config.force + 0.00001f,
                          "Blocked-exit wait bypassed bounded acceleration");
        }
        CHECK_MESSAGE((saw_exit_wait && saw_free_exit && !trial.junction_exit_reservation(0)),
                      "Blocked request prevented another exit from clearing or retained stale space");
        auto disabled = trial;
        disabled.pause();
        config.junction_traffic = false;
        disabled.configure_motion(config);
        CHECK_MESSAGE(
            (disabled.junction_exit_waiting() == 0 && !disabled.junction_exit_reservation(0) &&
             kinetra::distance(disabled.movers()[0].body.position, trial.movers()[0].body.position) == 0),
            "Disabling traffic left an exit wait or moved a body");
        trial.toggle_mover(2);
        CHECK_MESSAGE(trial.move_to(2, rotate({1250, 700})), "Exit blocker could not move away");
        bool reserved_after_release = false;
        for (int t = 0; t < 1500; ++t) {
            trial.tick();
            reserved_after_release = reserved_after_release || trial.junction_exit_reservation(0).has_value();
        }
        CHECK_MESSAGE((reserved_after_release && trial.collisions() == 0 && trial.contact_ticks() == 0 &&
                       !trial.movers()[0].moving() && !trial.movers()[1].moving() &&
                       !trial.movers()[2].moving() && trial.junction_exit_waiting() == 0 &&
                       !trial.junction_exit_reservation(0)),
                      "Clearing the exit did not resume and release crossing traffic");
    }
}

// An opposing approach must stop outside the admitted mover's exit pocket,
// so the admitted body can fully clear the junction before local avoidance.
TEST_CASE("an exit reservation keeps an opposing approach outside its pocket") {
    Simulation protected_exit(opposing_fixture());
    protected_exit.set_workload(false);
    auto config = protected_exit.motion_settings();
    config.profile = SteeringProfile::reciprocal;
    config.junction_traffic = true;
    protected_exit.configure_motion(config);
    REQUIRE_MESSAGE((protected_exit.move_to(0, {1100, 700}) && protected_exit.move_to(1, {700, 1100}) &&
                     protected_exit.move_to(2, {300, 700})),
                    "Opposing-approach goals rejected");
    protected_exit.run();
    bool held_outside_exit = false;
    for (int t = 0; t < 200; ++t) {
        protected_exit.tick();
        const auto area = protected_exit.junction_exit_reservation(0);
        REQUIRE_MESSAGE((area && area->min.x > 799), "First mover did not retain its east exit");
        const auto &other = protected_exit.movers()[2];
        CHECK_MESSAGE(other.body.position.x - other.body.half_extents.x >= area->max.x - 0.001f,
                      "Another approach occupied reserved exit space");
        CHECK_MESSAGE(kinetra::length(other.body.acceleration) <= config.force + 0.00001f,
                      "Reserved-exit stop exceeded the acceleration limit");
        held_outside_exit = held_outside_exit || other.coordination.exit_waiting;
    }
    CHECK_MESSAGE((held_outside_exit && protected_exit.collisions() == 0),
                  "Exit reservation did not constrain competing traffic");
}

// Enabling coordination while another mover is coasting toward a currently
// empty pocket must not treat that pocket as safely reservable.
TEST_CASE("a coasting approach prevents an unsafe exit reservation") {
    Simulation coasting(opposing_fixture());
    coasting.set_workload(false);
    auto config = coasting.motion_settings();
    config.profile = SteeringProfile::reciprocal;
    config.junction_traffic = false;
    coasting.configure_motion(config);
    REQUIRE_MESSAGE(coasting.move_to(2, {300, 700}), "Coasting approach goal rejected");
    coasting.run();
    for (int t = 0; t < 109; ++t)
        coasting.tick();
    CHECK_MESSAGE(coasting.movers()[2].body.position.x - coasting.movers()[2].body.half_extents.x > 925.2f,
                  "Coasting fixture already physically occupied the exit pocket");
    coasting.pause();
    config.junction_traffic = true;
    coasting.configure_motion(config);
    REQUIRE_MESSAGE((coasting.move_to(0, {1100, 700}) && coasting.move_to(1, {700, 1100})),
                    "Late-enabled crossing goals rejected");
    coasting.single_step();
    const auto admitted_exit = coasting.junction_exit_reservation(0);
    CHECK_MESSAGE(
        (coasting.movers()[0].coordination.exit_waiting && admitted_exit && admitted_exit->min.y > 799),
        "A coasting approach was ignored when admitting an exit reservation");
}

TEST_CASE("the corridor fixture stalls head-on without corridor traffic") {
    Simulation reference(corridor_fixture());
    start_corridor(reference, false, {1900, 700}, {300, 700});
    for (int t = 0; t < 5000; ++t)
        reference.tick();
    CHECK_MESSAGE((reference.movers()[0].moving() && reference.movers()[1].moving()),
                  "Corridor fixture no longer reproduces the head-on stall");
}

TEST_CASE("corridor admission passes opposing movers one at a time in every orientation") {
    const auto original = corridor_fixture();
    for (int orientation = 0; orientation < 4; ++orientation) {
        CAPTURE(orientation);
        auto b = original;
        const float angle = float(orientation) * 1.57079632679f;
        if (orientation % 2)
            b.dimension = {original.dimension.y, original.dimension.x};
        auto rotate = [&](Vec2 point) {
            return b.dimension / 2 + kinetra::rotated(point - original.dimension / 2, angle);
        };
        for (auto &m : b.movers) {
            const auto half = mover_spec(m.model).dimension / 2;
            m.position = rotate(m.position + half) - half;
        }
        for (auto &zone : b.zones) {
            const auto first = rotate(zone.position), opposite = rotate(zone.position + zone.dimension);
            zone.position = {std::min(first.x, opposite.x), std::min(first.y, opposite.y)};
            zone.dimension = {std::abs(first.x - opposite.x), std::abs(first.y - opposite.y)};
            // Quarter-turn roundoff must not turn exact grid edges into extra cells.
            zone.position.x = std::round(zone.position.x);
            zone.position.y = std::round(zone.position.y);
            zone.dimension.x = std::round(zone.dimension.x);
            zone.dimension.y = std::round(zone.dimension.y);
        }
        Simulation trial(b);
        start_corridor(trial, true, rotate({1900, 700}), rotate({300, 700}));
        CHECK_MESSAGE((trial.corridor_count() == 1 && trial.junction_count() == 0),
                      "Straight room-to-room corridor detection failed");
        auto replay = trial;
        const auto bounds = trial.corridor_bounds(0);
        bool saw_wait = false, saw_exit = false, checked_block = false;
        for (int t = 0; t < 5000; ++t) {
            trial.tick();
            replay.tick();
            int occupying = 0;
            for (std::size_t i = 0; i < trial.movers().size(); ++i) {
                const auto &m = trial.movers()[i];
                CHECK_MESSAGE(kinetra::distance(m.body.position, replay.movers()[i].body.position) == 0,
                              "Corridor coordination was nondeterministic");
                CHECK_MESSAGE(kinetra::length(m.body.acceleration) <=
                                  trial.motion_settings().force + 0.00001f,
                              "Corridor admission exceeded acceleration bounds");
                const auto goal = rotate(i == 0 ? Vec2{1900, 700} : Vec2{300, 700});
                CHECK_MESSAGE(kinetra::distance(m.destination, goal) == 0,
                              "Corridor admission changed a manual goal");
                if (m.body.position.x + m.body.half_extents.x > bounds.min.x - 2 &&
                    m.body.position.x - m.body.half_extents.x < bounds.max.x + 2 &&
                    m.body.position.y + m.body.half_extents.y > bounds.min.y - 2 &&
                    m.body.position.y - m.body.half_extents.y < bounds.max.y + 2)
                    ++occupying;
            }
            CHECK_MESSAGE(occupying <= 1, "Opposing movers entered the reserved corridor together");
            saw_wait = saw_wait || trial.corridor_waiting() > 0;
            saw_exit = saw_exit || trial.corridor_exit_reservation(0).has_value();
            if (!checked_block && occupying == 1 && trial.corridor_waiting() == 1) {
                auto blocked = trial;
                blocked.toggle_mover(0);
                const auto position = blocked.movers()[0].body.position;
                for (int step = 0; step < 200; ++step)
                    blocked.tick();
                CHECK_MESSAGE((kinetra::distance(position, blocked.movers()[0].body.position) == 0 &&
                               blocked.movers()[1].coordination.corridor_waiting),
                              "Blocked occupant released corridor admission");
                blocked.toggle_mover(0);
                for (int step = 0; step < 5000; ++step)
                    blocked.tick();
                CHECK_MESSAGE((!blocked.movers()[0].moving() && !blocked.movers()[1].moving() &&
                               blocked.collisions() == 0),
                              "Corridor traffic failed to resume after unblocking");
                auto independent = trial;
                independent.pause();
                auto config = independent.motion_settings();
                config.junction_traffic = true;
                independent.configure_motion(config);
                config.junction_traffic = false;
                independent.configure_motion(config);
                CHECK_MESSAGE(independent.corridor_exit_reservation(0).has_value(),
                              "Junction toggle cancelled an active corridor reservation");
                auto disabled = trial;
                disabled.pause();
                config = disabled.motion_settings();
                config.corridor_traffic = false;
                disabled.configure_motion(config);
                CHECK_MESSAGE((disabled.corridor_waiting() == 0 && !disabled.corridor_exit_reservation(0) &&
                               kinetra::distance(disabled.movers()[0].body.position, position) == 0),
                              "Disabling corridor traffic retained state or teleported a mover");
                checked_block = true;
            }
        }
        CHECK_MESSAGE((saw_wait && saw_exit && checked_block && !trial.movers()[0].moving() &&
                       !trial.movers()[1].moving() && trial.collisions() == 0 && trial.contact_ticks() == 0 &&
                       trial.corridor_waiting() == 0 && !trial.corridor_exit_reservation(0)),
                      "Corridor traffic did not clear both goals without contact");
    }
}

TEST_CASE("a parked corridor occupant keeps its exit space and can depart") {
    auto parked = corridor_fixture();
    parked.movers[0].position.x = 1043.5f;
    Simulation stopped(parked);
    stopped.set_workload(false);
    auto config = stopped.motion_settings();
    config.profile = SteeringProfile::reciprocal;
    config.corridor_traffic = true;
    stopped.configure_motion(config);
    stopped.toggle_mover(0);
    REQUIRE_MESSAGE(stopped.move_to(1, {300, 700}), "Occupied corridor goal rejected");
    stopped.run();
    for (int t = 0; t < 300; ++t)
        stopped.tick();
    CHECK_MESSAGE((stopped.movers()[0].body.position.x == 1100 &&
                   stopped.movers()[1].coordination.corridor_waiting &&
                   stopped.movers()[1].body.position.x - stopped.movers()[1].body.half_extents.x > 1725.7f),
                  "Waiting traffic consumed an unassigned occupant's exit space");
    stopped.toggle_mover(0);
    CHECK_MESSAGE(stopped.move_to(0, {1900, 700}), "Parked corridor occupant could not depart");
    for (int t = 0; t < 5000; ++t)
        stopped.tick();
    CHECK_MESSAGE(
        (!stopped.movers()[0].moving() && !stopped.movers()[1].moving() && stopped.collisions() == 0),
        "A previously parked occupant could not clear the corridor");
}

TEST_CASE("dead ends and overlong lanes are not registered as passing corridors") {
    const auto original = corridor_fixture();
    auto dead_end = original;
    dead_end.movers.resize(1);
    dead_end.zones.push_back({"closed room", {1600, 200}, {600, 1000}});
    CHECK_MESSAGE(Simulation(dead_end).corridor_count() == 0,
                  "Dead end was registered as a passing corridor");
    auto long_lane = original;
    long_lane.dimension.x = 52600;
    long_lane.zones[0].dimension.x = long_lane.zones[1].dimension.x = 52600;
    long_lane.zones[2].dimension.x = long_lane.zones[3].dimension.x = 51400;
    long_lane.movers[1].position.x = 52243.5f;
    CHECK_MESSAGE(Simulation(long_lane).corridor_count() == 0, "Corridor indexing budget was not bounded");
}

TEST_CASE("retreat recovery resolves a corridor standstill in every orientation") {
    const auto original = recovery_fixture();
    for (int orientation = 0; orientation < 4; ++orientation) {
        CAPTURE(orientation);
        auto b = original;
        const float angle = float(orientation) * 1.57079632679f;
        if (orientation % 2)
            b.dimension = {original.dimension.y, original.dimension.x};
        auto rotate = [&](Vec2 p) {
            return b.dimension / 2 + kinetra::rotated(p - original.dimension / 2, angle);
        };
        for (auto &m : b.movers)
            m.position = rotate(m.position + Vec2{56.5f, 56.5f}) - Vec2{56.5f, 56.5f};
        for (auto &zone : b.zones) {
            const auto a = rotate(zone.position), c = rotate(zone.position + zone.dimension);
            zone.position = {std::round(std::min(a.x, c.x)), std::round(std::min(a.y, c.y))};
            zone.dimension = {std::round(std::abs(a.x - c.x)), std::round(std::abs(a.y - c.y))};
        }
        Simulation trial(b);
        trial.set_workload(false);
        auto config = trial.motion_settings();
        config.profile = SteeringProfile::reciprocal;
        trial.configure_motion(config);
        REQUIRE_MESSAGE((trial.move_to(0, rotate({1900, 700})) && trial.move_to(1, rotate({300, 700}))),
                        "Recovery goals rejected");
        trial.run();
        for (int t = 0; t < 1500; ++t)
            trial.tick();
        CHECK_MESSAGE((trial.movers()[0].moving() && trial.movers()[1].moving()),
                      "Recovery fixture did not stall");
        trial.pause();
        config.corridor_traffic = true;
        trial.configure_motion(config);
        trial.run();
        auto replay = trial;
        bool saw_retreat = false, saw_reverse = false, checked_lifecycle = false;
        for (int t = 0; t < 6000; ++t) {
            trial.tick();
            replay.tick();
            for (std::size_t i = 0; i < trial.movers().size(); ++i) {
                const auto &m = trial.movers()[i];
                CHECK_MESSAGE(kinetra::distance(m.body.position, replay.movers()[i].body.position) == 0,
                              "Retreat recovery was nondeterministic");
                CHECK_MESSAGE((kinetra::length(m.body.acceleration) <= config.force + 0.00001f &&
                               kinetra::distance(m.destination,
                                                 rotate(i == 0 ? Vec2{1900, 700} : Vec2{300, 700})) == 0),
                              "Retreat exceeded acceleration limits or replaced a goal");
                if (!m.coordination.corridor_retreating)
                    continue;
                saw_retreat = true;
                saw_reverse =
                    saw_reverse || kinetra::dot(m.body.velocity, m.destination - m.body.position) < -1;
                CHECK_MESSAGE(trial.corridor_retreat_target(i).has_value(),
                              "Active retreat has no control target");
                if (checked_lifecycle)
                    continue;
                auto cancelled = trial;
                CHECK_MESSAGE((cancelled.move_to(i, m.destination) && cancelled.corridor_recovering() == 0 &&
                               !cancelled.corridor_retreat_target(i) &&
                               kinetra::distance(cancelled.movers()[i].body.position, m.body.position) == 0),
                              "New manual goal failed to cancel retreat without teleporting");
                auto disabled = trial;
                disabled.pause();
                auto off = disabled.motion_settings();
                off.corridor_traffic = false;
                disabled.configure_motion(off);
                CHECK_MESSAGE((disabled.corridor_recovering() == 0 &&
                               !disabled.movers()[i].coordination.corridor_retreating &&
                               kinetra::distance(disabled.movers()[i].body.position, m.body.position) == 0),
                              "Disabling traffic retained retreat state or moved a body");
                auto blocked = trial;
                blocked.toggle_mover(i);
                for (int step = 0; step < 200; ++step)
                    blocked.tick();
                CHECK_MESSAGE(kinetra::distance(blocked.movers()[i].body.position, m.body.position) == 0,
                              "Recovery moved a blocked yielding mover");
                blocked.toggle_mover(i);
                for (int step = 0; step < 6000; ++step)
                    blocked.tick();
                CHECK_MESSAGE((!blocked.movers()[0].moving() && !blocked.movers()[1].moving() &&
                               blocked.collisions() == 0),
                              "Retreat did not resume after a temporary blockage");
                checked_lifecycle = true;
            }
        }
        CHECK_MESSAGE((saw_retreat && saw_reverse && checked_lifecycle && trial.corridor_recovering() == 0 &&
                       !trial.movers()[0].moving() && !trial.movers()[1].moving() &&
                       trial.collisions() == 0 && trial.contact_ticks() == 0),
                      "Retreat failed to restore both original goals without contact");
    }
}

TEST_CASE("retreat refuges fit long rectangular movers") {
    auto rectangular = recovery_fixture();
    rectangular.dimension = {1400, 2200};
    for (auto &zone : rectangular.zones) {
        const auto low = zone.position, high = zone.position + zone.dimension;
        zone.position = {1400 - high.y, low.x};
        zone.dimension = {high.y - low.y, high.x - low.x};
    }
    rectangular.movers = {{"long north", Model::APM4350, {622.5f, 782.5f}},
                          {"long south", Model::APM4350, {622.5f, 1182.5f}}};
    Simulation long_movers(rectangular);
    long_movers.set_workload(false);
    auto long_config = long_movers.motion_settings();
    long_config.profile = SteeringProfile::reciprocal;
    long_config.corridor_traffic = true;
    long_movers.configure_motion(long_config);
    REQUIRE_MESSAGE((long_movers.move_to(0, {700, 2050}) && long_movers.move_to(1, {700, 300})),
                    "Long rectangular mover goals rejected");
    long_movers.run();
    bool long_recovery = false;
    for (int t = 0; t < 8000; ++t) {
        long_movers.tick();
        long_recovery = long_recovery || long_movers.corridor_recovering() > 0;
    }
    CHECK_MESSAGE((long_recovery && !long_movers.movers()[0].moving() && !long_movers.movers()[1].moving() &&
                   long_movers.collisions() == 0),
                  "Retreat refuge failed to fit long rectangular movers");
}

TEST_CASE("recovery avoids occupied refuges and retries once one is free") {
    auto b = recovery_fixture();
    for (auto point : {Vec2{500, 500}, Vec2{500, 900}, Vec2{1700, 500}, Vec2{1700, 900}})
        b.movers.push_back({"occupied refuge " + std::to_string(b.movers.size()), Model::APM4220,
                            point - Vec2{56.5f, 56.5f}});
    Simulation obstructed(b);
    obstructed.set_workload(false);
    auto config = obstructed.motion_settings();
    config.profile = SteeringProfile::reciprocal;
    config.corridor_traffic = true;
    obstructed.configure_motion(config);
    REQUIRE_MESSAGE((obstructed.move_to(0, {1900, 700}) && obstructed.move_to(1, {300, 700})),
                    "Obstructed recovery goals rejected");
    obstructed.run();
    for (int t = 0; t < 1500; ++t) {
        obstructed.tick();
        CHECK_MESSAGE(obstructed.corridor_recovering() == 0, "Recovery selected an occupied refuge");
    }
    CHECK_MESSAGE(obstructed.move_to(2, {100, 300}), "Refuge blocker could not move away");
    bool recovered = false;
    for (int t = 0; t < 8000; ++t) {
        obstructed.tick();
        recovered = recovered || obstructed.corridor_recovering() > 0;
    }
    CHECK_MESSAGE((recovered && !obstructed.movers()[0].moving() && !obstructed.movers()[1].moving() &&
                   obstructed.collisions() == 0),
                  "Freeing a refuge did not permit a safe retry");
}

TEST_CASE("predictive passing completes open-floor encounters in every orientation") {
    for (auto profile : {SteeringProfile::improved, SteeringProfile::reciprocal})
        for (int orientation = 0; orientation < 8; ++orientation) {
            CAPTURE(profile);
            CAPTURE(orientation);
            Blueprint b;
            b.name = "open-floor passing";
            b.dimension = {4000, 4000};
            b.cell_size = {200, 200};
            b.mover_max_extent = 235;
            const float angle = float(orientation) * 0.78539816339f;
            const auto forward = kinetra::rotated({1, 0}, angle);
            const Vec2 first = Vec2{2000, 2000} - forward * 600, second = Vec2{2000, 2000} + forward * 600;
            b.movers = {{"first", Model::APM4220, first - Vec2{56.5f, 56.5f}},
                        {"second", Model::APM4350, second - Vec2{77.5f, 117.5f}}};
            Simulation trial(b);
            trial.set_workload(false);
            auto config = trial.motion_settings();
            config.profile = profile;
            config.predictive_passing = true;
            config.heatmap = false;
            trial.configure_motion(config);
            REQUIRE_MESSAGE((trial.move_to(0, second) && trial.move_to(1, first)), "Passing goals rejected");
            trial.run();
            auto replay = trial;
            bool saw_pair = false, checked_lifecycle = false;
            for (int tick = 0; tick < 5000; ++tick) {
                trial.tick();
                replay.tick();
                for (std::size_t i = 0; i < 2; ++i) {
                    const auto &m = trial.movers()[i];
                    CHECK_MESSAGE(kinetra::distance(m.body.position, replay.movers()[i].body.position) == 0,
                                  "Predictive passing was nondeterministic");
                    CHECK_MESSAGE((kinetra::distance(m.destination, i == 0 ? second : first) == 0 &&
                                   kinetra::length(m.body.acceleration) <= config.force + 0.00001f),
                                  "Passing changed a goal or exceeded acceleration bounds");
                    CHECK_MESSAGE(!m.coordination.corridor_retreating,
                                  "Open-floor passing invoked corridor retreat");
                }
                if (!trial.predictive_pairs())
                    continue;
                if (!saw_pair)
                    CHECK_MESSAGE((!trial.stalled(0) && !trial.stalled(1) &&
                                   kinetra::distance(trial.movers()[0].body.position,
                                                     trial.movers()[1].body.position) > 300),
                                  "Passing was detected only after a packed standstill");
                saw_pair = true;
                CHECK_MESSAGE((trial.movers()[0].coordination.passing &&
                               trial.movers()[1].coordination.passing && trial.passing_target(0) &&
                               trial.passing_target(1)),
                              "Pair did not share temporary targets");
                if (checked_lifecycle)
                    continue;
                auto cancelled = trial;
                CHECK_MESSAGE((cancelled.move_to(0, second) && cancelled.predictive_pairs() == 0 &&
                               !cancelled.passing_target(1) &&
                               kinetra::distance(cancelled.movers()[0].body.position,
                                                 trial.movers()[0].body.position) == 0),
                              "Manual command did not cancel passing without teleportation");
                auto disabled = trial;
                disabled.pause();
                auto off = disabled.motion_settings();
                off.predictive_passing = false;
                disabled.configure_motion(off);
                CHECK_MESSAGE((disabled.predictive_pairs() == 0 &&
                               kinetra::distance(disabled.movers()[1].body.position,
                                                 trial.movers()[1].body.position) == 0),
                              "Disabling predictive passing retained a pair or moved a body");
                auto blocked = trial;
                blocked.toggle_mover(1);
                blocked.tick();
                CHECK_MESSAGE((blocked.predictive_pairs() == 0 &&
                               kinetra::distance(blocked.movers()[1].body.position,
                                                 trial.movers()[1].body.position) == 0),
                              "Blocked participant retained a stale passing commitment");
                checked_lifecycle = true;
            }
            CHECK_MESSAGE((saw_pair && checked_lifecycle && trial.predictive_pairs() == 0 &&
                           !trial.movers()[0].moving() && !trial.movers()[1].moving() &&
                           trial.collisions() == 0),
                          "Predictive passing failed to complete without contact");
        }
}

TEST_CASE("shared passing lanes shift away from a wall") {
    Simulation wall(near_wall_fixture());
    wall.set_workload(false);
    wall.configure_motion(passing_motion(SteeringProfile::reciprocal));
    REQUIRE_MESSAGE((wall.move_to(0, {1900, 100}) && wall.move_to(1, {500, 100})),
                    "Near-wall goals rejected");
    wall.run();
    bool shifted = false;
    for (int t = 0; t < 5000; ++t) {
        wall.tick();
        shifted = shifted || wall.predictive_pairs() > 0;
    }
    CHECK_MESSAGE(
        (shifted && !wall.movers()[0].moving() && !wall.movers()[1].moving() && wall.collisions() == 0),
        "Shared passing lanes did not shift away from a wall");
}

TEST_CASE("passing without free space keeps the original routes") {
    auto b = near_wall_fixture();
    b.dimension.y = 200;
    Simulation narrow(b);
    narrow.set_workload(false);
    narrow.configure_motion(passing_motion(SteeringProfile::reciprocal));
    REQUIRE_MESSAGE((narrow.move_to(0, {1900, 100}) && narrow.move_to(1, {500, 100})),
                    "Narrow goals rejected");
    narrow.run();
    for (int t = 0; t < 1000; ++t) {
        narrow.tick();
        CHECK_MESSAGE((narrow.predictive_pairs() == 0 && !narrow.movers()[0].route.empty() &&
                       !narrow.movers()[1].route.empty()),
                      "No-space passing created an unsafe detour or cleared the original path");
    }
}

TEST_CASE("legacy steering profiles never form passing pairs") {
    for (auto profile : {SteeringProfile::original, SteeringProfile::experimental_rvo}) {
        CAPTURE(profile);
        Simulation legacy(load_blueprint(i4sim::test::showcase_blueprint("predictive-passing")));
        legacy.set_workload(false);
        legacy.configure_motion(passing_motion(profile));
        REQUIRE_MESSAGE((legacy.move_to(0, {1900, 800}) && legacy.move_to(1, {500, 800})),
                        "Legacy goals rejected");
        legacy.run();
        for (int t = 0; t < 1000; ++t) {
            legacy.tick();
            CHECK_MESSAGE(legacy.predictive_pairs() == 0,
                          "Predictive policy changed a legacy steering profile");
        }
    }
}

TEST_CASE("the event log records a passing plan from formation to completion") {
    auto pair = passing_pair(SteeringProfile::reciprocal);
    run_until_pair(pair);
    const auto formed_at = pair.ticks();
    const std::array entries{*pair.passing_target(0), *pair.passing_target(1)};
    for (int tick = 0; tick < 5000; ++tick)
        pair.tick();
    REQUIRE_MESSAGE((!pair.movers()[0].moving() && !pair.movers()[1].moving() && pair.collisions() == 0),
                    "Passing fixture did not complete without contact");
    const auto events = pair.events();
    REQUIRE(events.size() == 3);
    CHECK(events[0].kind == EventKind::passing_formed);
    CHECK(events[1].kind == EventKind::passing_crossing);
    CHECK(events[2].kind == EventKind::passing_ended);
    CHECK(events[2].end == PassingEnd::completed);
    CHECK(events[0].tick == formed_at);
    CHECK((events[0].tick < events[1].tick && events[1].tick < events[2].tick));
    for (std::size_t side = 0; side < 2; ++side)
        CHECK(kinetra::distance(events[0].entry[side], entries[side]) == 0);
    for (const auto &event : events) {
        CHECK(event.plan == 1);
        CHECK(event.movers == std::array<std::size_t, 2>{0, 1});
        CHECK((event.kind == EventKind::passing_ended) == event.end.has_value());
    }
}

TEST_CASE("the event log records why a passing plan ends") {
    auto pair = passing_pair(SteeringProfile::reciprocal);
    run_until_pair(pair);

    auto commanded = pair;
    REQUIRE(commanded.move_to(0, {1900, 1000}));
    REQUIRE(!commanded.events().empty());
    const auto &goal = commanded.events().back();
    CHECK(goal.kind == EventKind::passing_ended);
    CHECK(goal.end == PassingEnd::goal_changed);
    CHECK(goal.tick == commanded.ticks());

    auto held = pair;
    held.toggle_mover(1);
    held.tick();
    REQUIRE(!held.events().empty());
    const auto &hold = held.events().back();
    CHECK(hold.kind == EventKind::passing_ended);
    CHECK(hold.end == PassingEnd::participant_held);
    CHECK(hold.tick == held.ticks());
}

TEST_CASE("the event log records each contact once with its tick and participants") {
    auto b = fixture();
    b.producers.clear();
    b.movers.push_back({"overlap", Model::APM4220, b.movers[0].position + Vec2{10, 0}});
    Simulation movers(b);
    movers.set_workload(false);
    movers.record_events(true);
    movers.single_step();
    movers.single_step();
    REQUIRE(movers.events().size() == 1);
    const auto &pair = movers.events().front();
    CHECK(pair.kind == EventKind::contact);
    CHECK(pair.tick == 1);
    CHECK(pair.movers == std::array<std::size_t, 2>{0, 1});
    CHECK((!pair.border && !pair.passing && pair.plan == 0));
    CHECK(movers.events().size() == movers.collisions());

    // The mover's right edge reaches 13 mm into the blocked cell that starts at x = 1200,
    // and its 113 mm height stays inside that cell's row, so it touches one border.
    b = fixture();
    b.producers.clear();
    b.movers[0].position = {1100, 650};
    b.zones.push_back({"wall", {1200, 600}, {200, 200}});
    Simulation wall(b);
    wall.set_workload(false);
    wall.record_events(true);
    wall.single_step();
    REQUIRE(wall.events().size() == 1);
    const auto &border = wall.events().front();
    CHECK(border.kind == EventKind::contact);
    CHECK(border.border);
    CHECK(border.movers[0] == 0);
    CHECK(border.movers[1] < wall.borders().size());
}

TEST_CASE("recording events is opt-in and does not change the run") {
    for (auto profile : {SteeringProfile::improved, SteeringProfile::reciprocal}) {
        CAPTURE(profile);
        auto quiet = passing_pair(profile, false);
        auto recorded = quiet;
        recorded.record_events(true);
        CHECK((!quiet.recording_events() && recorded.recording_events()));
        for (int tick = 0; tick < 5000; ++tick) {
            quiet.tick();
            recorded.tick();
            for (std::size_t i = 0; i < 2; ++i) {
                const auto &a = quiet.movers()[i].body, &b = recorded.movers()[i].body;
                REQUIRE((kinetra::distance(a.position, b.position) == 0 &&
                         kinetra::distance(a.velocity, b.velocity) == 0));
            }
        }
        CHECK(quiet.events().empty());
        CHECK(!recorded.events().empty());
    }
}

TEST_CASE("flow metrics count no waiting or stuck mover on a free trip") {
    auto b = fixture();
    b.producers.clear();
    b.movers[0].position = {300, 700};
    Simulation solo(b);
    solo.set_workload(false);
    REQUIRE(solo.move_to(0, {1900, 750}));
    solo.run();
    FlowMetrics flow;
    for (int tick = 0; tick < 5000 && solo.movers()[0].moving(); ++tick) {
        solo.tick();
        flow.observe(solo);
    }
    REQUIRE(!solo.movers()[0].moving());
    CHECK(flow.waiting_ticks() == 0);
    CHECK(flow.stuck_movers() == 0);
    CHECK(flow.stuck_now() == 0);
}

TEST_CASE("flow metrics count a head-on corridor standstill as waiting and stuck") {
    Simulation standstill(corridor_fixture());
    start_corridor(standstill, false, {1900, 700}, {300, 700});
    FlowMetrics flow;
    for (int tick = 0; tick < 5000; ++tick) {
        standstill.tick();
        flow.observe(standstill);
    }
    REQUIRE((standstill.movers()[0].moving() && standstill.movers()[1].moving()));
    CHECK(flow.waiting_ticks() > 0);
    CHECK(flow.stuck_movers() == 2);
    CHECK(flow.stuck_now() == 2);
}

TEST_CASE("flow metrics ignore a held mover") {
    auto b = fixture();
    b.producers.clear();
    Simulation held(b);
    held.set_workload(false);
    REQUIRE(held.move_to(0, {1900, 750}));
    held.toggle_mover(0);
    held.run();
    FlowMetrics flow({.stuck_ticks = 100});
    for (int tick = 0; tick < 1000; ++tick) {
        held.tick();
        flow.observe(held);
    }
    CHECK(flow.waiting_ticks() == 0);
    CHECK(flow.stuck_movers() == 0);
}

TEST_CASE("flow metrics reject invalid limits and a second simulation") {
    CHECK_THROWS_AS(FlowMetrics({.waiting_speed = 0}), std::invalid_argument);
    CHECK_THROWS_AS(FlowMetrics({.arrival_distance = -1}), std::invalid_argument);
    CHECK_THROWS_AS(FlowMetrics({.progress_distance = std::nanf("")}), std::invalid_argument);
    CHECK_THROWS_AS(FlowMetrics({.stuck_ticks = 0}), std::invalid_argument);
    FlowMetrics flow;
    flow.observe(Simulation(fixture()));
    CHECK_THROWS_AS(flow.observe(Simulation(corridor_fixture())), std::invalid_argument);
}

TEST_CASE("parking traffic keeps the MAS workload contact-free and its interlock consistent") {
    const auto blueprint = load_blueprint(default_blueprint_directory() / "MAS-demonstrator.json");
    Simulation simulation(blueprint, 42), replay(blueprint, 42);
    for (auto *trial : {&simulation, &replay}) {
        auto config = trial->motion_settings();
        config.profile = SteeringProfile::reciprocal;
        config.parking_traffic = true;
        trial->configure_motion(config);
        trial->run();
    }
    bool saw_wait = false, checked_release = false, checked_manual = false;
    bool saw_return = false;
    std::vector<bool> waiting(blueprint.movers.size());
    for (int tick = 0; tick < 50000; ++tick) {
        simulation.tick();
        replay.tick();
        for (std::size_t i = 0; i < simulation.movers().size(); ++i) {
            const auto &m = simulation.movers()[i];
            const auto &copy = replay.movers()[i];
            CHECK_MESSAGE(
                (kinetra::distance(m.body.position, copy.body.position) == 0 && m.goal == copy.goal),
                "Parking traffic replay was nondeterministic");
            CHECK_MESSAGE(kinetra::distance(m.parking, blueprint.movers[i].position + m.body.half_extents) ==
                              0,
                          "Parking traffic changed an owned reservation");
            CHECK_MESSAGE(kinetra::length(m.body.acceleration) <=
                              simulation.motion_settings().force + 0.00001f,
                          "Parking traffic bypassed bounded movement");
            if (waiting[i] && m.goal != GoalKind::parking_staging &&
                kinetra::distance(m.destination, m.parking) < 0.05f)
                saw_return = true;
            waiting[i] = m.goal == GoalKind::parking_staging;
            if (m.goal != GoalKind::parking_staging)
                continue;
            saw_wait = true;
            if (!checked_release) {
                auto disabled = simulation;
                disabled.pause();
                auto config = disabled.motion_settings();
                config.parking_traffic = false;
                disabled.configure_motion(config);
                CHECK_MESSAGE((disabled.parking_waiting() == 0 &&
                               kinetra::distance(disabled.movers()[i].destination, m.parking) == 0 &&
                               kinetra::distance(disabled.movers()[i].body.position, m.body.position) == 0),
                              "Disabling parking traffic lost the original goal or teleported a mover");
                auto blocked = simulation;
                blocked.set_workload(false);
                blocked.pause();
                for (std::size_t j = 0; j < blocked.movers().size(); ++j)
                    if (j != i && !blocked.movers()[j].held)
                        blocked.toggle_mover(j);
                blocked.single_step();
                CHECK_MESSAGE(blocked.movers()[i].goal != GoalKind::parking_staging,
                              "A blocked departure retained a stale parking interlock");
                checked_release = true;
            }
            if (!checked_manual && !m.product) {
                auto manual = simulation;
                CHECK_MESSAGE((manual.move_to(i, m.destination) &&
                               manual.movers()[i].goal == GoalKind::manual &&
                               manual.movers()[i].goal != GoalKind::parking_staging),
                              "Manual command did not replace an automatic parking wait");
                checked_manual = true;
            }
        }
        CHECK_MESSAGE(simulation.parking_waiting() == std::count(waiting.begin(), waiting.end(), true),
                      "Parking wait statistics did not match simulation state");
    }
    CHECK_MESSAGE((saw_wait && saw_return && checked_release && checked_manual),
                  "Parking traffic lifecycle was not exercised");
    CHECK_MESSAGE(
        (simulation.completed() >= 15 && simulation.collisions() == 0 && simulation.contact_ticks() == 0),
        "Parking traffic failed the MAS throughput/contact regression");
}

TEST_CASE("manual goals and single steps work under every profile and invalid motion settings are rejected") {
    Simulation probe(fixture());
    probe.set_workload(false);
    const auto start = probe.movers()[0].body.position;
    REQUIRE_MESSAGE(probe.move_to(0, {1300, 800}), "Manual goal rejected");
    CHECK_MESSAGE(probe.movers()[0].goal == GoalKind::manual, "Manual mover not reserved");
    probe.add_product(ProductKind::trimmer);
    probe.single_step();
    CHECK_MESSAGE(!probe.movers()[0].product, "Manual mover allocated by factory");
    CHECK_MESSAGE((probe.state() == RunState::paused && probe.ticks() == 1),
                  "Single step did not retain pause");
    CHECK_MESSAGE(kinetra::distance(start, probe.movers()[0].body.position) > 0, "Single step did not move");
    for (auto profile : {SteeringProfile::improved, SteeringProfile::original,
                         SteeringProfile::experimental_rvo, SteeringProfile::reciprocal}) {
        auto config = probe.motion_settings();
        config.profile = profile;
        config.heatmap = false;
        probe.configure_motion(config);
        probe.single_step();
        CHECK_MESSAGE(kinetra::finite(probe.movers()[0].body.position), "Nonfinite profile motion");
    }
    auto config = probe.motion_settings();
    config.profile = SteeringProfile(99);
    bool invalid_profile = false;
    try {
        probe.configure_motion(config);
    } catch (const std::invalid_argument &) {
        invalid_profile = true;
    }
    CHECK_MESSAGE(invalid_profile, "Unknown steering profile accepted");
    config.profile = SteeringProfile::improved;
    config.horizon = 65;
    bool rejected = false;
    try {
        probe.configure_motion(config);
    } catch (const std::invalid_argument &) {
        rejected = true;
    }
    CHECK_MESSAGE(rejected, "Unbounded horizon accepted");
}

// Return to an actual reservation after departing into the aisle.
// Include walls, a corner and a slot between two parked movers.
TEST_CASE("movers return to their reservations beside walls and in corners and between neighbors") {
    for (auto profile : {SteeringProfile::improved, SteeringProfile::original, SteeringProfile::reciprocal})
        for (int scenario = 0; scenario < 3; ++scenario) {
            CAPTURE(profile);
            CAPTURE(scenario);
            auto b = fixture();
            b.producers.clear();
            const Vec2 size = mover_spec(Model::APM4220).dimension;
            const Vec2 home = scenario == 0   ? Vec2{0, 500}
                              : scenario == 1 ? Vec2{.25f, .25f}
                                              : Vec2{800, 500};
            b.movers = {{"returning", Model::APM4220, home}};
            if (scenario == 2) {
                b.movers.push_back({"left", Model::APM4220, home - Vec2{size.x + 8, 0}});
                b.movers.push_back({"right", Model::APM4220, home + Vec2{size.x + 8, 0}});
            }
            Simulation parking(b);
            parking.set_workload(false);
            auto config = parking.motion_settings();
            config.profile = profile;
            parking.configure_motion(config);
            const auto reserved = parking.movers()[0].parking;
            CHECK_MESSAGE(!parking.move_to(0, {size.x / 2 - 1, 700}),
                          "Manual goal accepted a footprint outside the world");
            for (const auto goal : {Vec2{800, 950} + size / 2, reserved}) {
                REQUIRE_MESSAGE(parking.move_to(0, goal), "Parking approach rejected");
                parking.run();
                for (int tick = 0; tick < 8000 && parking.movers()[0].moving(); ++tick) {
                    parking.tick();
                    const auto &m = parking.movers()[0];
                    CHECK_MESSAGE((kinetra::finite(m.forces.border) &&
                                   kinetra::length(m.body.acceleration) <= config.force + 1e-5f),
                                  "Parking produced an invalid acceleration");
                }
                const auto &m = parking.movers()[0];
                INFO("pos=" << m.body.position.x << ',' << m.body.position.y << " goal=" << goal.x << ','
                            << goal.y << " route=" << m.route.size() << " velocity=" << m.body.velocity.x
                            << ',' << m.body.velocity.y);
                CHECK_MESSAGE(!parking.movers()[0].moving(), "Parking approach failed to settle");
                CHECK_MESSAGE(parking.collisions() == 0, "Parking approach contacted a wall or neighbor");
            }
            CHECK_MESSAGE((kinetra::distance(parking.movers()[0].body.position, reserved) == 0 &&
                           kinetra::length(parking.movers()[0].body.velocity) == 0),
                          "Mover failed to park at its reservation");
            for (int tick = 0; tick < 200; ++tick)
                parking.tick();
            CHECK_MESSAGE(kinetra::distance(parking.movers()[0].body.position, reserved) == 0,
                          "Parked mover drifted after arrival");
        }
}

TEST_CASE("allocation skips a mover trapped behind blocked neighbors") {
    auto b = fixture();
    b.producers = {{"Kuka", Model::Kuka, {2300, 100}, {300, 250}}};
    b.movers = {{"trapped", Model::APM4220, {5, 5}},
                {"front", Model::APM4220, {5, 123}},
                {"side", Model::APM4220, {123, 5}},
                {"available", Model::APM4220, {800, 500}}};
    Simulation departure(b);
    departure.set_workload(false);
    departure.toggle_mover(1);
    departure.toggle_mover(2);
    departure.add_product(ProductKind::trimmer);
    departure.single_step();
    CHECK_MESSAGE((!departure.movers()[0].product && departure.movers()[3].product.has_value()),
                  "Factory assigned a mover trapped behind parked neighbors");
}

TEST_CASE("a persistent contact pair counts once and accumulates its duration") {
    auto b = fixture();
    b.producers.clear();
    b.movers.push_back({"overlap", Model::APM4220, b.movers[0].position + Vec2{10, 0}});
    Simulation contacts(b);
    contacts.set_workload(false);
    contacts.single_step();
    CHECK_MESSAGE((contacts.collisions() == 1 && contacts.contact_ticks() == 1),
                  "Contact pair was counted more than once");
    contacts.single_step();
    CHECK_MESSAGE((contacts.collisions() == 1 && contacts.contact_ticks() == 2),
                  "Persistent contact was counted as a new event");
    CHECK_MESSAGE((contacts.movers()[0].collision_ticks == 2 && contacts.movers()[1].collision_ticks == 2),
                  "Per-mover contact duration is inconsistent");
}

TEST_CASE("a solo mover ignores its own heatmap footprint and arrives without contact") {
    auto b = fixture();
    b.producers.clear();
    b.movers[0].position = {300, 700};
    Simulation solo(b);
    solo.set_workload(false);
    REQUIRE_MESSAGE(solo.move_to(0, {1900, 750}), "Solo goal rejected");
    solo.run();
    for (int tick = 0; tick < 5000 && solo.movers()[0].moving(); ++tick) {
        solo.tick();
        CHECK_MESSAGE(!solo.stalled(0), "A solo mover stalled on its own heatmap footprint");
    }
    CHECK_MESSAGE((!solo.movers()[0].moving() && solo.collisions() == 0),
                  "Solo route failed to arrive without contact");
}

TEST_CASE("contact does not displace a parked mover") {
    auto b = fixture();
    b.producers.clear();
    b.movers = {{"moving", Model::APM4220, {500, 700}}, {"parked", Model::APM4220, {510, 700}}};
    Simulation parked(b);
    parked.set_workload(false);
    const auto position = parked.movers()[1].body.position;
    REQUIRE_MESSAGE(parked.move_to(0, {1700, 700}), "Departure goal rejected");
    parked.run();
    for (int tick = 0; tick < 1000; ++tick) {
        parked.tick();
        CHECK_MESSAGE((kinetra::distance(parked.movers()[1].body.position, position) == 0 &&
                       kinetra::length(parked.movers()[1].body.velocity) == 0),
                      "Contact displaced a parked mover");
    }
}

TEST_CASE("a valid detour counts as progress and arrives without contact") {
    auto b = fixture();
    b.producers.clear();
    b.movers[0].position = {650, 700};
    b.zones.push_back({"wall", {1000, 400}, {200, 800}});
    Simulation detour(b);
    detour.set_workload(false);
    REQUIRE_MESSAGE(detour.move_to(0, {1300, 750}), "Detour goal rejected");
    detour.run();
    for (int tick = 0; tick < 5000 && detour.movers()[0].moving(); ++tick) {
        detour.tick();
        CHECK_MESSAGE(!detour.stalled(0), "A valid detour was classified as lack of route progress");
    }
    CHECK_MESSAGE((!detour.movers()[0].moving() && detour.collisions() == 0),
                  "Detour failed to reach its goal without contact");
}

TEST_CASE("frontal encounters reach both goals without contacts") {
    check_encounters(false);
}

TEST_CASE("crossing encounters reach both goals without contacts") {
    check_encounters(true);
}

TEST_CASE("drawing interpolates between a mover's last two ticks") {
    Simulation simulation(load_blueprint(i4sim::test::showcase_blueprint("predictive-passing")));
    simulation.set_workload(false);
    REQUIRE(simulation.move_to(0, {1900, 800}));
    CHECK_MESSAGE(simulation.interpolation() == 1, "A paused simulation shows its exact state");
    CHECK(distance(simulation.movers()[0].previous_position, simulation.movers()[0].body.position) == 0);
    simulation.run();
    simulation.updates_per_second(4); // binary fractions keep the clock exact
    for (int tick = 0; tick < 20; ++tick)
        simulation.advance(0.25);
    const auto before = simulation.movers()[0].body.position;
    simulation.advance(0.375);
    const auto &mover = simulation.movers()[0];
    CHECK_MESSAGE(distance(mover.previous_position, before) == 0,
                  "previous_position is the start of the last tick");
    CHECK(distance(mover.body.position, before) > 0);
    CHECK(simulation.interpolation() == doctest::Approx(0.5));
    simulation.pause();
    CHECK(simulation.interpolation() == 1);
}

TEST_CASE("the bundled catalog holds seven blueprints that all load") {
    const auto files = discover_blueprints(default_blueprint_directory());
    REQUIRE_MESSAGE(files.size() == 7, "Original blueprint bundle is incomplete");
    for (const auto &file : files)
        load_blueprint(file);
}

TEST_CASE("the bundled showcase holds four scenes whose scenarios load and apply") {
    const auto files = discover_blueprints(default_blueprint_directory() / "showcase");
    REQUIRE_MESSAGE(files.size() == 4, "Showcase bundle is incomplete");
    for (const auto &file : files) {
        CAPTURE(file);
        const auto scenario = load_scenario(file);
        REQUIRE_MESSAGE(scenario, "Showcase blueprint has no scenario");
        CHECK_MESSAGE((!scenario->description.empty() && !scenario->workload && scenario->goals.size() == 2),
                      "Showcase scenario lacks a description, disables no workload or misses goals");
        Simulation simulation(load_blueprint(file));
        apply_scenario(*scenario, simulation);
        CHECK_MESSAGE(simulation.state() == RunState::paused,
                      "Applying a scenario must not start the simulation");
        Simulation running(load_blueprint(file));
        running.run();
        apply_scenario(*scenario, running);
        CHECK_MESSAGE(running.running(), "Applying a scenario must keep a running simulation running");
        CHECK_MESSAGE(!simulation.workload(), "Scenario workload not applied");
        CHECK_MESSAGE((simulation.movers()[0].goal == GoalKind::manual &&
                       simulation.movers()[1].goal == GoalKind::manual),
                      "Scenario goals were not sent");
        const bool shuttles = scenario->repeat == ScenarioRepeat::shuttle;
        CHECK_MESSAGE((simulation.shuttling(0) == shuttles && simulation.shuttling(1) == shuttles),
                      "Scenario repeat mode not applied");
    }
}

TEST_CASE("an original blueprint has no scenario") {
    CHECK_FALSE(load_scenario(default_blueprint_directory() / "MAS-demonstrator.json"));
}

TEST_CASE("each showcase scene runs exactly like its hand-configured setup") {
    struct Scene {
        const char *name;
        void (*enable)(MotionSettings &);
        Vec2 first_goal, second_goal;
    };
    const Scene scenes[]{
        {"junction-crossing", [](MotionSettings &m) { m.junction_traffic = true; }, {1100, 700}, {700, 1100}},
        {"corridor-passing", [](MotionSettings &m) { m.corridor_traffic = true; }, {1900, 700}, {300, 700}},
        {"corridor-recovery", [](MotionSettings &m) { m.corridor_traffic = true; }, {1900, 700}, {300, 700}},
        {"predictive-passing",
         [](MotionSettings &m) {
             m.predictive_passing = true;
             m.heatmap = false;
         },
         {1900, 800},
         {500, 800}},
    };
    // Long enough for the shuttling scenes to turn at least once.
    constexpr std::uint64_t ticks = 6000;
    for (const auto &scene : scenes) {
        CAPTURE(scene.name);
        const auto file = i4sim::test::showcase_blueprint(scene.name);
        // The setup the presentation and golden tests build by hand, shuttling where the
        // scene does.
        MotionSettings motion;
        motion.profile = SteeringProfile::reciprocal;
        scene.enable(motion);
        Simulation by_hand(load_blueprint(file));
        by_hand.set_workload(false);
        by_hand.configure_motion(motion);
        const auto send = [&](std::size_t mover, Vec2 goal) {
            return load_scenario(file)->repeat == ScenarioRepeat::shuttle ? by_hand.shuttle(mover, goal)
                                                                           : by_hand.move_to(mover, goal);
        };
        REQUIRE((send(0, scene.first_goal) && send(1, scene.second_goal)));
        by_hand.run();
        i4sim::test::Recorder hand_recorder;
        hand_recorder.run(by_hand, ticks);

        Simulation from_scenario(load_blueprint(file));
        apply_scenario(*load_scenario(file), from_scenario);
        from_scenario.run();
        i4sim::test::Recorder scenario_recorder;
        scenario_recorder.run(from_scenario, ticks);

        CHECK(scenario_recorder.summary(from_scenario) == hand_recorder.summary(by_hand));
    }
}

TEST_CASE("shuttling movers turn together after the pause") {
    Simulation s(crossing_fixture());
    s.set_workload(false);
    auto config = s.motion_settings();
    config.profile = SteeringProfile::reciprocal;
    config.junction_traffic = true;
    s.configure_motion(config);
    const auto west_start = s.movers()[0].body.position;
    const auto north_start = s.movers()[1].body.position;
    REQUIRE((s.shuttle(0, {1100, 700}) && s.shuttle(1, {700, 1100})));
    CHECK((s.shuttling(0) && s.shuttling(1)));
    s.run();
    const auto both_idle = [&] {
        return s.movers()[0].lifecycle == MoverLifecycle::idle && s.movers()[1].lifecycle == MoverLifecycle::idle;
    };
    // Runs until both movers have arrived; returns whether a junction wait happened on the way.
    const auto drive_leg = [&] {
        bool waited = false;
        for (int tick = 0; tick < 5000 && !both_idle(); ++tick) {
            s.tick();
            waited = waited || s.junction_waiting() > 0;
        }
        REQUIRE_MESSAGE(both_idle(), "Shuttling movers did not complete their leg");
        return waited;
    };
    std::uint64_t first_arrival = 0;
    while (!both_idle() && s.ticks() < 5000) {
        s.tick();
        if (!first_arrival && (s.movers()[0].lifecycle == MoverLifecycle::idle ||
                               s.movers()[1].lifecycle == MoverLifecycle::idle))
            first_arrival = s.ticks();
    }
    REQUIRE(both_idle());
    const auto last_arrival = s.ticks();
    CHECK_MESSAGE(first_arrival < last_arrival, "The junction should make one mover arrive later");
    for (std::uint64_t tick = 1; tick < shuttle_pause_ticks; ++tick) {
        s.tick();
        REQUIRE_MESSAGE(both_idle(), "A shuttling mover turned before the pause after the last arrival ended");
    }
    s.tick();
    CHECK_MESSAGE(s.ticks() == last_arrival + shuttle_pause_ticks, "Pause counted from the wrong tick");
    REQUIRE_MESSAGE((s.movers()[0].moving() && s.movers()[1].moving()), "Shuttling movers did not turn together");
    CHECK((s.movers()[0].destination.x == west_start.x && s.movers()[0].destination.y == west_start.y));
    CHECK((s.movers()[1].destination.x == north_start.x && s.movers()[1].destination.y == north_start.y));
    CHECK_MESSAGE(drive_leg(), "The return leg did not meet at the junction again");
    CHECK((s.movers()[0].body.position.x == west_start.x && s.movers()[0].body.position.y == west_start.y));
    CHECK((s.movers()[1].body.position.x == north_start.x && s.movers()[1].body.position.y == north_start.y));
    CHECK((s.shuttling(0) && s.shuttling(1)));
    CHECK(s.collisions() == 0);
}

TEST_CASE("a manual goal ends shuttling for that mover only") {
    Simulation s(crossing_fixture());
    s.set_workload(false);
    const auto west_start = s.movers()[0].body.position;
    // (300, 300) lies in the northwest forbidden zone.
    CHECK_FALSE(s.shuttle(0, {300, 300}));
    CHECK_FALSE(s.shuttling(0));
    CHECK(s.movers()[0].lifecycle == MoverLifecycle::idle);
    CHECK_THROWS_AS(s.shuttle(2, {1100, 700}), std::out_of_range);
    CHECK_THROWS_AS((void)s.shuttling(2), std::out_of_range);

    REQUIRE((s.shuttle(0, {1100, 700}) && s.shuttle(1, {700, 1100})));
    CHECK_FALSE(s.move_to(1, {300, 300})); // A rejected goal changes nothing.
    CHECK(s.shuttling(1));
    REQUIRE(s.move_to(1, {700, 1100}));
    CHECK_FALSE(s.shuttling(1));
    CHECK(s.shuttling(0));
    s.run();
    // Each mover is checked from its own arrival, so the test does not depend on which one
    // the junction lets through first.
    const auto run_until_idle = [&](std::size_t mover) {
        for (int tick = 0; tick < 5000 && s.movers()[mover].lifecycle != MoverLifecycle::idle; ++tick)
            s.tick();
        REQUIRE(s.movers()[mover].lifecycle == MoverLifecycle::idle);
    };
    // The west mover now shuttles alone, so it turns one pause after its own arrival.
    run_until_idle(0);
    for (std::uint64_t tick = 0; tick < shuttle_pause_ticks; ++tick)
        s.tick();
    CHECK_MESSAGE(s.movers()[0].moving(), "The remaining shuttling mover did not turn on its own");
    CHECK((s.movers()[0].destination.x == west_start.x && s.movers()[0].destination.y == west_start.y));
    run_until_idle(1);
    for (std::uint64_t tick = 0; tick <= shuttle_pause_ticks; ++tick)
        s.tick();
    CHECK_MESSAGE(s.movers()[1].lifecycle == MoverLifecycle::idle, "A mover with a manual goal kept shuttling");
}

TEST_CASE("every shuttling showcase scene repeats its traffic situation") {
    for (const auto &file : discover_blueprints(default_blueprint_directory() / "showcase")) {
        CAPTURE(file);
        const auto scenario = load_scenario(file);
        REQUIRE(scenario);
        if (scenario->repeat != ScenarioRepeat::shuttle)
            continue;
        Simulation s(load_blueprint(file));
        apply_scenario(*scenario, s);
        s.run();
        // A leg runs from one turn to the next; count the legs in which traffic coordination
        // stepped in, which is the situation the scene exists to show.
        constexpr int wanted_legs = 4;
        int legs = 0, coordinated_legs = 0;
        bool coordinated = false, under_way = true;
        while (legs < wanted_legs && s.ticks() < 40000) {
            s.tick();
            for (const auto &m : s.movers()) {
                const auto &c = m.coordination;
                coordinated = coordinated || c.junction_waiting || c.exit_waiting || c.corridor_waiting ||
                              c.corridor_retreating || c.passing;
            }
            const bool idle = std::all_of(s.movers().begin(), s.movers().end(),
                                          [](const Mover &m) { return m.lifecycle == MoverLifecycle::idle; });
            if (under_way && idle) {
                ++legs;
                coordinated_legs += coordinated;
                coordinated = false;
            }
            under_way = !idle;
        }
        CHECK_MESSAGE(legs == wanted_legs, "The scene did not complete its legs");
        CHECK_MESSAGE(coordinated_legs == wanted_legs, "A leg did not show the traffic situation again");
        CHECK(s.collisions() == 0);
        CHECK((s.shuttling(0) && s.shuttling(1)));
    }
}

TEST_CASE("a restarting scene is due once its movers have stood still for the pause") {
    const auto file = i4sim::test::showcase_blueprint("corridor-recovery");
    const auto scenario = load_scenario(file);
    REQUIRE(scenario);
    REQUIRE(scenario->repeat == ScenarioRepeat::restart);
    Simulation s(load_blueprint(file));
    apply_scenario(*scenario, s);
    CHECK_FALSE_MESSAGE((s.shuttling(0) || s.shuttling(1)), "A restarting scene must not shuttle");
    s.run();
    RestartTimer timer;
    const auto settled = [&] {
        return std::all_of(s.movers().begin(), s.movers().end(),
                           [](const Mover &m) { return m.lifecycle == MoverLifecycle::idle; });
    };
    bool retreated = false;
    while (!settled() && s.ticks() < 20000) {
        s.tick();
        retreated = retreated || s.corridor_recovering() > 0;
        REQUIRE_FALSE_MESSAGE(timer.due(s), "Restart due while a mover is under way");
    }
    REQUIRE_MESSAGE(settled(), "The corridor-recovery scene did not settle");
    CHECK_MESSAGE(retreated, "The scene did not show its corridor retreat");
    const auto arrival = s.ticks();
    CHECK_FALSE(timer.due(s));
    while (s.ticks() < arrival + shuttle_pause_ticks - 1) {
        s.tick();
        REQUIRE_FALSE_MESSAGE(timer.due(s), "Restart due before the pause ended");
    }
    s.tick();
    CHECK(timer.due(s));
    CHECK(s.collisions() == 0);
}

TEST_CASE("an invalid scenario is rejected with its cause") {
    const auto dir = std::filesystem::temp_directory_path() / "kinetra-i4sim-regression";
    std::filesystem::create_directories(dir);
    const auto with_scenario = [&](const std::string &scenario) {
        auto text = std::ifstream(i4sim::test::showcase_blueprint("junction-crossing"));
        auto json = std::string(std::istreambuf_iterator<char>(text), {});
        // Replace the scene's own section with the one under test.
        json = json.substr(0, json.find("\"scenario\"")) + "\"scenario\": " + scenario + "}";
        const auto file = dir / "scenario.json";
        std::ofstream(file) << json;
        return file;
    };
    const auto load_error = [](const std::filesystem::path &file) {
        try {
            load_scenario(file);
        } catch (const std::runtime_error &e) {
            return std::string(e.what());
        }
        return std::string();
    };
    const auto apply_error = [](const std::filesystem::path &file) {
        Simulation simulation(load_blueprint(file));
        try {
            apply_scenario(*load_scenario(file), simulation);
        } catch (const std::runtime_error &e) {
            return std::string(e.what());
        }
        return std::string();
    };
    const auto unknown_key = with_scenario(R"({"motion": {"junctionTrafic": true}})");
    CHECK(load_error(unknown_key).find("Unknown property in scenario.motion: junctiontrafic") !=
          std::string::npos);
    CHECK(load_error(unknown_key).find("scenario.json") != std::string::npos);
    CHECK(load_error(with_scenario(R"({"motion": {"profile": "fast"}})")).find("Unknown profile: fast") !=
          std::string::npos);
    CHECK(load_error(with_scenario(R"({"workload": 0})")).find("workload must be true or false") !=
          std::string::npos);
    CHECK(load_error(with_scenario(R"({"repeat": true})")).find("scenario.repeat must be a string") !=
          std::string::npos);
    CHECK(load_error(with_scenario(R"({"repeat": "forever"})")).find("Unknown repeat: forever") !=
          std::string::npos);
    CHECK(load_error(with_scenario(R"({"goals": [{"mover": "west", "position": {"x": 1, "y": 2, "z": 3}}]})"))
              .find("Unknown property in scenario.goals[].position: z") != std::string::npos);
    CHECK(apply_error(with_scenario(R"({"goals": [{"mover": "south", "position": {"x": 700, "y": 700}}]})"))
              .find("names no mover: south") != std::string::npos);
    // (300, 300) lies in the northwest forbidden zone.
    CHECK(apply_error(with_scenario(R"({"goals": [{"mover": "west", "position": {"x": 300, "y": 300}}]})"))
              .find("rejected for mover west") != std::string::npos);
    // Case-insensitive like the rest of the blueprint, and the RVO profile accepts both spellings.
    const auto loaded = load_scenario(with_scenario(R"({"Motion": {"Profile": "Experimental_RVO"}})"));
    REQUIRE(loaded);
    CHECK(loaded->motion.profile == SteeringProfile::experimental_rvo);
    CHECK(loaded->workload);
    CHECK(loaded->repeat == ScenarioRepeat::none);
    const auto restarting = load_scenario(with_scenario(R"({"Repeat": "Restart"})"));
    REQUIRE(restarting);
    CHECK(restarting->repeat == ScenarioRepeat::restart);
}

TEST_CASE("every bundled blueprint expands its groups and starts paused") {
    for (const auto &file : discover_blueprints(default_blueprint_directory())) {
        CAPTURE(file);
        const auto b = load_blueprint(file);
        Simulation simulation(b);
        CHECK_MESSAGE((!simulation.movers().empty() && !simulation.producers().empty()),
                      "Blueprint groups were not expanded");
        CHECK_MESSAGE(simulation.state() == RunState::paused, "Blueprint must start paused");
        simulation.set_workload(false);
        simulation.run();
        for (int i = 0; i < 20; ++i)
            simulation.tick();
    }
}

TEST_CASE("the MAS blueprint and mover models keep their original semantics") {
    const auto mas = load_blueprint(default_blueprint_directory() / "MAS-demonstrator.json");
    CHECK_MESSAGE((mas.movers.size() == 11 && mas.producers.size() == 4 && mas.cell_size.x == 240),
                  "Original blueprint semantics changed");
    CHECK_MESSAGE((mas.producers[0].id == "Kuka_1" && mas.producers[3].rectangular),
                  "IDs or rectangular producer lost");
    CHECK_MESSAGE(
        (mover_spec(Model::APM4350).dimension.y == 235 && mover_spec(Model::APM4550).payload == 4.5f),
        "Mover models changed");
}

TEST_CASE("both product recipes complete with the original production timing") {
    for (const auto kind : {ProductKind::trimmer, ProductKind::personalized}) {
        CAPTURE(kind);
        Simulation simulation(fixture());
        simulation.set_workload(false);
        simulation.advance(5);
        CHECK_MESSAGE(simulation.ticks() == 0, "Paused simulation advanced");
        simulation.add_product(kind);
        simulation.run();
        for (int i = 0; i < 30000 && !simulation.completed(); ++i)
            simulation.tick();
        INFO(recipe_diagnostics(simulation));
        CHECK_MESSAGE((simulation.completed() == 1 && simulation.products().empty()),
                      "Full local product recipe did not complete");
        const auto count = recipe(kind).size();
        CHECK_MESSAGE((simulation.movers()[0].interactions == count && !simulation.movers()[0].product),
                      "Mover not released after recipe completion");
        std::uint64_t processing = 0;
        for (const auto &p : simulation.producers())
            processing += p.processing_ticks;
        CHECK_MESSAGE(processing == count * 1000,
                      "Production duration differs from original 1000 ticks per interaction");
    }
}

TEST_CASE("the original MAS workload completes recipes within producer queue capacity") {
    const auto mas = load_blueprint(default_blueprint_directory() / "MAS-demonstrator.json");
    Simulation factory(mas, 42);
    factory.run();
    for (int i = 0; i < 50000; ++i)
        factory.tick();
    INFO(workload_diagnostics(factory));
    CHECK_MESSAGE(factory.completed() >= 5, "Original MAS workload deadlocked instead of completing recipes");
    for (const auto &producer : factory.producers())
        CHECK_MESSAGE(producer.queue.size() <= mas.producer_max_queue, "Producer queue capacity exceeded");
}

TEST_CASE("the 3840x2400 factory exercises route recovery and completes products") {
    check_large_factory_recovery("3840x2400.json");
}

TEST_CASE("the 8640x4800 factory exercises route recovery and completes products") {
    check_large_factory_recovery("8640x4800.json");
}

TEST_CASE("a blocked producer pauses its recipe and resumes it") {
    Simulation blocked(fixture());
    blocked.set_workload(false);
    blocked.add_product(ProductKind::trimmer);
    blocked.run();
    for (int i = 0; i < 5000 && blocked.producers()[0].remaining == 0; ++i)
        blocked.tick();
    REQUIRE_MESSAGE(blocked.producers()[0].remaining > 0, "Fixture never started production");
    blocked.toggle_producer(0);
    const auto remaining = blocked.producers()[0].remaining;
    for (int i = 0; i < 30; ++i)
        blocked.tick();
    CHECK_MESSAGE(blocked.producers()[0].remaining == remaining, "Blocked producer kept processing");
    blocked.toggle_producer(0);
    for (int i = 0; i < 30000 && !blocked.completed(); ++i)
        blocked.tick();
    CHECK_MESSAGE(blocked.completed() == 1, "Blocked producer could not resume its recipe");
}

TEST_CASE("seeded simulations are deterministic") {
    const auto a = seeded_run(), b = seeded_run();
    CHECK_MESSAGE((a.ticks() == b.ticks() &&
                   kinetra::distance(a.movers()[0].body.position, b.movers()[0].body.position) == 0),
                  "Seeded simulation is not deterministic");
}

TEST_CASE("a blocked mover stays in place") {
    auto a = seeded_run();
    a.toggle_mover(0);
    const auto p = a.movers()[0].body.position;
    for (int i = 0; i < 100; ++i)
        a.tick();
    CHECK_MESSAGE(kinetra::distance(p, a.movers()[0].body.position) == 0, "Blocked mover still moves");
}

TEST_CASE("the tick cap pauses the simulation") {
    auto limited = fixture();
    limited.tick_cap = 3;
    Simulation cap(limited);
    cap.run();
    for (int i = 0; i < 10; ++i)
        cap.tick();
    CHECK_MESSAGE((cap.ticks() == 3 && cap.state() == RunState::finished), "Tick cap ignored");
}

TEST_CASE("a manual goal stays manual until arrival and then returns the mover to the factory") {
    auto b = fixture();
    b.producers.clear();
    Simulation simulation(b);
    simulation.set_workload(false);
    const auto &mover = simulation.movers()[0];
    CHECK((mover.lifecycle == MoverLifecycle::idle && mover.goal == GoalKind::automatic));
    REQUIRE(simulation.move_to(0, {1300, 800}));
    CHECK((mover.lifecycle == MoverLifecycle::moving && mover.goal == GoalKind::manual));
    REQUIRE(simulation.move_to(0, {1500, 800}));
    CHECK_MESSAGE(mover.goal == GoalKind::manual, "A second manual goal lost its origin");
    simulation.run();
    for (int tick = 0; tick < 5000 && mover.moving(); ++tick)
        simulation.tick();
    CHECK((mover.lifecycle == MoverLifecycle::idle && mover.goal == GoalKind::automatic));
}

TEST_CASE("the mover status follows the HUD priority") {
    auto b = fixture();
    Simulation simulation(b);
    simulation.set_workload(false);
    CHECK(mover_status(simulation.movers()[0]) == MoverStatus::available);
    simulation.add_product(ProductKind::trimmer);
    simulation.single_step();
    REQUIRE(simulation.movers()[0].product);
    CHECK(mover_status(simulation.movers()[0]) == MoverStatus::allocated);
    simulation.toggle_mover(0);
    CHECK(mover_status(simulation.movers()[0]) == MoverStatus::held);

    auto pair = passing_pair(SteeringProfile::reciprocal, false);
    run_until_pair(pair);
    CHECK(mover_status(pair.movers()[0]) == MoverStatus::passing_detour);
    pair.toggle_mover(0);
    CHECK_MESSAGE(mover_status(pair.movers()[0]) == MoverStatus::held, "Held did not take priority");
}

TEST_CASE("run and pause move between the run states and the tick cap finishes a run") {
    auto limited = fixture();
    limited.tick_cap = 2;
    Simulation simulation(limited);
    simulation.set_workload(false);
    CHECK(simulation.state() == RunState::paused);
    simulation.tick();
    simulation.advance(1);
    CHECK_MESSAGE(simulation.ticks() == 0, "A paused simulation ran a tick");
    simulation.pause();
    CHECK(simulation.state() == RunState::paused);
    simulation.run();
    CHECK(simulation.running());
    simulation.run();
    CHECK(simulation.running());
    simulation.pause();
    CHECK(simulation.state() == RunState::paused);
    simulation.single_step();
    CHECK((simulation.state() == RunState::paused && simulation.ticks() == 1));
    simulation.single_step();
    CHECK_MESSAGE((simulation.state() == RunState::finished && simulation.ticks() == 2),
                  "The tick that reaches the cap did not finish the run");
    for (auto change : {&Simulation::run, &Simulation::pause, &Simulation::single_step}) {
        (simulation.*change)();
        CHECK(simulation.state() == RunState::finished);
    }
    simulation.tick();
    simulation.advance(1);
    CHECK_MESSAGE(simulation.ticks() == 2, "A finished simulation ran another tick");
    CHECK(simulation.interpolation() == 1);
}

TEST_CASE("changing motion settings while running equals pausing around the change") {
    auto paused_change = passing_pair(SteeringProfile::reciprocal, false);
    auto running_change = paused_change;
    for (int tick = 0; tick < 200; ++tick) {
        paused_change.tick();
        running_change.tick();
    }
    auto config = paused_change.motion_settings();
    config.profile = SteeringProfile::improved;
    paused_change.pause();
    paused_change.configure_motion(config);
    paused_change.run();
    running_change.configure_motion(config);
    CHECK(running_change.running());
    for (int tick = 0; tick < 2000; ++tick) {
        paused_change.tick();
        running_change.tick();
        for (std::size_t i = 0; i < 2; ++i)
            REQUIRE(kinetra::distance(paused_change.movers()[i].body.position,
                                      running_change.movers()[i].body.position) == 0);
    }
}

TEST_CASE("a run exports its summary") {
    auto a = seeded_run();
    a.toggle_mover(0);
    for (int i = 0; i < 100; ++i)
        a.tick();
    const auto dir = std::filesystem::temp_directory_path() / "kinetra-i4sim-regression";
    std::filesystem::create_directories(dir);
    a.dump(dir);
    CHECK_MESSAGE(std::filesystem::file_size(dir / "summary.json") > 100, "Summary export missing");
}

TEST_CASE("an invalid blueprint is rejected with its file name") {
    const auto dir = std::filesystem::temp_directory_path() / "kinetra-i4sim-regression";
    std::filesystem::create_directories(dir);
    const auto invalid = dir / "invalid.json";
    {
        std::ofstream f(invalid);
        f << "{\"name\":\"bad\",\"dimension\":{\"X\":100,\"Y\":100},\"cellSize\":{\"X\":0,\"Y\":10}}";
    }
    bool rejected = false;
    try {
        load_blueprint(invalid);
    } catch (const std::runtime_error &e) {
        rejected = std::string(e.what()).find("invalid.json") != std::string::npos;
    }
    CHECK_MESSAGE(rejected, "Invalid blueprint was accepted or error lacks file context");
}

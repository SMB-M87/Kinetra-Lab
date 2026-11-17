#include "simulation.h"
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace {
void require(bool condition, const char *message) {
    if (!condition)
        throw std::runtime_error(message);
}
i4sim::Blueprint fixture() {
    using namespace i4sim;
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
void reciprocal_motion_contracts() {
    using namespace i4sim;
    Simulation normal(fixture()), accelerated(fixture());
    for (auto *simulation : {&normal, &accelerated}) {
        simulation->workload = false;
        auto config = simulation->motion_settings();
        config.profile = SteeringProfile::reciprocal;
        simulation->configure_motion(config);
        require(simulation->move_to(0, {1300, 800}), "Reciprocal tick goal rejected");
    }
    accelerated.updates_per_second(1000);
    normal.single_step();
    accelerated.single_step();
    require(kinetra::distance(normal.movers()[0].body.position, accelerated.movers()[0].body.position) == 0 &&
                std::abs(kinetra::length(normal.movers()[0].body.velocity) - normal.motion_settings().force) < 0.00001f,
            "Reciprocal controller changed physical tick units with UPS");
    require(normal.avoidance_limited() == 0, "Unobstructed reciprocal control reported a limitation");

    auto b = fixture();
    b.producers.clear();
    const auto size = mover_spec(Model::APM4220).dimension;
    const Vec2 home{800, 500};
    b.movers = {{"moving", Model::APM4220, home},
                {"left", Model::APM4220, home - Vec2{size.x + 8, 0}},
                {"right", Model::APM4220, home + Vec2{size.x + 8, 0}}};
    Simulation narrow(b);
    narrow.workload = false;
    auto config = narrow.motion_settings();
    config.profile = SteeringProfile::reciprocal;
    narrow.configure_motion(config);
    const auto parked = narrow.movers()[1].body.position;
    require(narrow.move_to(0, Vec2{800, 950} + size / 2), "Reciprocal departure goal rejected");
    narrow.single_step();
    require(narrow.avoidance_limited() == 0 &&
                kinetra::length(narrow.movers()[0].body.acceleration) <= config.force + 0.00001f,
            "Valid rectangular gap was rejected or exceeded acceleration bounds");
    require(kinetra::distance(parked, narrow.movers()[1].body.position) == 0,
            "Reciprocal avoidance displaced a parked mover");
}
i4sim::Blueprint crossing_fixture() {
    return i4sim::load_blueprint(std::filesystem::path(__FILE__).parent_path() / "junction-crossing.json");
}
void junction_traffic_contracts() {
    using namespace i4sim;
    auto start = [](Simulation &s, bool enabled) {
        s.workload = false;
        auto config = s.motion_settings();
        config.profile = SteeringProfile::reciprocal;
        config.junction_traffic = enabled;
        s.configure_motion(config);
        require(s.move_to(0, {1100, 700}) && s.move_to(1, {700, 1100}), "Crossing goals rejected");
        s.paused = false;
    };
    Simulation reference(crossing_fixture()), trial(crossing_fixture()), replay(crossing_fixture());
    start(reference, false); start(trial, true); start(replay, true);
    require(trial.junction_count() == 1 && trial.junction_center(0).x == 700 &&
                trial.junction_center(0).y == 700, "Isolated crossing detection failed");
    require(Simulation(fixture()).junction_count() == 0, "Open floor was mistaken for a corridor crossing");
    bool waited = false, checked_block = false, checked_late_cancel = false;
    for (int tick = 0; tick < 1000; ++tick) {
        trial.tick(); replay.tick();
        int occupying = 0;
        for (std::size_t i = 0; i < trial.movers().size(); ++i) {
            const auto &m = trial.movers()[i];
            require(kinetra::distance(m.body.position, replay.movers()[i].body.position) == 0,
                    "Junction traffic was nondeterministic");
            require(kinetra::length(m.body.acceleration) <= trial.motion_settings().force + 0.00001f,
                    "Junction stopping exceeded acceleration bounds");
            require(m.destination.x == (i == 0 ? 1100 : 700) && m.destination.y == (i == 0 ? 700 : 1100),
                    "Junction admission changed a manual goal");
            const auto delta = m.body.position - Vec2{700, 700};
            if (std::abs(delta.x) < 102 + m.body.half_extents.x &&
                std::abs(delta.y) < 102 + m.body.half_extents.y) ++occupying;
        }
        require(occupying <= 1, "Two movers entered an exclusively reserved crossing");
        const auto &approach = trial.movers()[0].body;
        const float front = approach.position.x + approach.half_extents.x;
        if (!checked_late_cancel && front > 596 && front < 598) {
            auto cancelled = trial;
            require(cancelled.move_to(0, {100, 700}), "Late cancellation goal rejected");
            for (int t = 0; t < 1000; ++t) {
                cancelled.tick();
                int present = 0;
                for (const auto &m : cancelled.movers()) {
                    const auto delta = m.body.position - Vec2{700, 700};
                    if (std::abs(delta.x) < 102 + m.body.half_extents.x &&
                        std::abs(delta.y) < 102 + m.body.half_extents.y) ++present;
                }
                require(present <= 1, "Late goal change released a coasting approach too early");
            }
            require(!cancelled.movers()[0].moving && !cancelled.movers()[1].moving &&
                        cancelled.collisions() == 0, "Late cancellation failed to release after braking");
            checked_late_cancel = true;
        }
        waited = waited || trial.junction_waiting() > 0;
        if (!checked_block && occupying == 1 && trial.junction_waiting() == 1) {
            auto blocked = trial;
            blocked.toggle_mover(0);
            const auto held = blocked.movers()[0].body.position;
            for (int t = 0; t < 200; ++t) blocked.tick();
            require(kinetra::distance(held, blocked.movers()[0].body.position) == 0 &&
                        blocked.movers()[1].body.position.y + blocked.movers()[1].body.half_extents.y <= 598.001f,
                    "Blocked physical occupant released the crossing");
            blocked.toggle_mover(0);
            for (int t = 0; t < 1000; ++t) blocked.tick();
            require(!blocked.movers()[0].moving && !blocked.movers()[1].moving,
                    "Crossing did not resume after its occupant was unblocked");
            auto disabled = trial;
            disabled.paused = true;
            auto config = disabled.motion_settings();
            config.junction_traffic = false;
            disabled.configure_motion(config);
            require(disabled.junction_waiting() == 0 &&
                        kinetra::distance(disabled.movers()[0].body.position, held) == 0,
                    "Disabling junction traffic retained a wait or teleported a mover");
            checked_block = true;
        }
    }
    for (int t = 0; t < 5000; ++t) reference.tick();
    require(reference.movers()[1].moving && waited && checked_block && checked_late_cancel && trial.junction_waiting() == 0 &&
                !trial.movers()[0].moving && !trial.movers()[1].moving && trial.collisions() == 0,
            "Junction admission did not resolve the narrow crossing regression");
    for (int orientation = 1; orientation <= 3; ++orientation) {
        auto b = crossing_fixture();
        const float angle = orientation * 1.57079632679f;
        const Vec2 half = mover_spec(Model::APM4220).dimension / 2;
        for (auto &m : b.movers)
            m.position = Vec2{700, 700} + kinetra::rotated(m.position + half - Vec2{700, 700}, angle) - half;
        Simulation rotated(b);
        rotated.workload = false;
        auto config = rotated.motion_settings();
        config.profile = SteeringProfile::reciprocal;
        config.junction_traffic = true;
        rotated.configure_motion(config);
        require(rotated.move_to(0, Vec2{700, 700} + kinetra::rotated({400, 0}, angle)) &&
                    rotated.move_to(1, Vec2{700, 700} + kinetra::rotated({0, 400}, angle)),
                "Rotated crossing goal rejected");
        rotated.paused = false;
        for (int t = 0; t < 1500; ++t) rotated.tick();
        require(!rotated.movers()[0].moving && !rotated.movers()[1].moving && rotated.collisions() == 0,
                "Crossing admission depended on approach orientation");
    }
    auto tee_blueprint = crossing_fixture();
    tee_blueprint.zones.push_back({"closed south", {600, 800}, {200, 600}});
    Simulation tee(tee_blueprint);
    tee.workload = false;
    auto config = tee.motion_settings();
    config.profile = SteeringProfile::reciprocal;
    config.junction_traffic = true;
    tee.configure_motion(config);
    require(tee.junction_count() == 1 && tee.move_to(0, {1100, 700}) && tee.move_to(1, {300, 700}),
            "T-junction detection or goals failed");
    tee.paused = false;
    for (int t = 0; t < 2000; ++t) tee.tick();
    require(!tee.movers()[0].moving && !tee.movers()[1].moving && tee.collisions() == 0,
            "T-junction admission failed to clear a turning movement");
    Simulation cancelled(crossing_fixture());
    start(cancelled, true);
    cancelled.tick();
    require(cancelled.move_to(0, {100, 700}), "Cancelled approach goal rejected");
    for (int t = 0; t < 1000; ++t) cancelled.tick();
    require(!cancelled.movers()[0].moving && !cancelled.movers()[1].moving && cancelled.collisions() == 0,
            "Cancelled approach retained a stale junction admission");
}
void parking_traffic_contracts() {
    using namespace i4sim;
    const auto blueprint = load_blueprint(default_blueprint_directory() / "MAS-demonstrator.json");
    Simulation simulation(blueprint, 42), replay(blueprint, 42);
    for (auto *trial : {&simulation, &replay}) {
        auto config = trial->motion_settings();
        config.profile = SteeringProfile::reciprocal;
        config.parking_traffic = true;
        trial->configure_motion(config);
        trial->paused = false;
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
            require(kinetra::distance(m.body.position, copy.body.position) == 0 &&
                        m.parking_deferred == copy.parking_deferred,
                    "Parking traffic replay was nondeterministic");
            require(kinetra::distance(m.parking, blueprint.movers[i].position + m.body.half_extents) == 0,
                    "Parking traffic changed an owned reservation");
            require(kinetra::length(m.body.acceleration) <= simulation.motion_settings().force + 0.00001f,
                    "Parking traffic bypassed bounded movement");
            if (waiting[i] && !m.parking_deferred &&
                kinetra::distance(m.destination, m.parking) < 0.05f)
                saw_return = true;
            waiting[i] = m.parking_deferred;
            if (!m.parking_deferred) continue;
            saw_wait = true;
            if (!checked_release) {
                auto disabled = simulation;
                disabled.paused = true;
                auto config = disabled.motion_settings();
                config.parking_traffic = false;
                disabled.configure_motion(config);
                require(disabled.parking_waiting() == 0 &&
                            kinetra::distance(disabled.movers()[i].destination, m.parking) == 0 &&
                            kinetra::distance(disabled.movers()[i].body.position, m.body.position) == 0,
                        "Disabling parking traffic lost the original goal or teleported a mover");
                auto blocked = simulation;
                blocked.workload = false;
                blocked.paused = true;
                for (std::size_t j = 0; j < blocked.movers().size(); ++j)
                    if (j != i && !blocked.movers()[j].blocked) blocked.toggle_mover(j);
                blocked.single_step();
                require(!blocked.movers()[i].parking_deferred,
                        "A blocked departure retained a stale parking interlock");
                checked_release = true;
            }
            if (!checked_manual && !m.product) {
                auto manual = simulation;
                require(manual.move_to(i, m.destination) && manual.movers()[i].manual &&
                            !manual.movers()[i].parking_deferred,
                        "Manual command did not replace an automatic parking wait");
                checked_manual = true;
            }
        }
        require(simulation.parking_waiting() == std::count(waiting.begin(), waiting.end(), true),
                "Parking wait statistics did not match simulation state");
    }
    require(saw_wait && saw_return && checked_release && checked_manual,
            "Parking traffic lifecycle was not exercised");
    require(simulation.completed() >= 15 && simulation.collisions() == 0 && simulation.contact_ticks() == 0,
            "Parking traffic failed the MAS throughput/contact regression");
}
} // namespace
int main(int argc, char **argv) {
    try {
        using namespace i4sim;
        reciprocal_motion_contracts();
        {
            Simulation probe(fixture());
            probe.workload = false;
            const auto start = probe.movers()[0].body.position;
            require(probe.move_to(0, {1300, 800}), "Manual goal rejected");
            require(probe.movers()[0].manual, "Manual mover not reserved");
            probe.add_product(ProductKind::trimmer);
            probe.single_step();
            require(!probe.movers()[0].product, "Manual mover allocated by factory");
            require(probe.paused && probe.ticks() == 1, "Single step did not retain pause");
            require(kinetra::distance(start, probe.movers()[0].body.position) > 0, "Single step did not move");
            for (auto profile : {SteeringProfile::improved, SteeringProfile::original,
                                 SteeringProfile::experimental_rvo, SteeringProfile::reciprocal}) {
                auto config = probe.motion_settings();
                config.profile = profile;
                config.heatmap = false;
                probe.configure_motion(config);
                probe.single_step();
                require(kinetra::finite(probe.movers()[0].body.position), "Nonfinite profile motion");
            }
            auto config = probe.motion_settings();
            config.profile = SteeringProfile(99);
            bool invalid_profile = false;
            try { probe.configure_motion(config); } catch (const std::invalid_argument &) { invalid_profile = true; }
            require(invalid_profile, "Unknown steering profile accepted");
            config.profile = SteeringProfile::improved;
            config.horizon = 65;
            bool rejected = false;
            try { probe.configure_motion(config); } catch (const std::invalid_argument &) { rejected = true; }
            require(rejected, "Unbounded horizon accepted");
        }
        {
            // Return to an actual reservation after departing into the aisle.
            // Include walls, a corner and a slot between two parked movers.
            for (auto profile : {SteeringProfile::improved, SteeringProfile::original, SteeringProfile::reciprocal})
                for (int scenario = 0; scenario < 3; ++scenario) {
                    auto b = fixture();
                    b.producers.clear();
                    const Vec2 size = mover_spec(Model::APM4220).dimension;
                    const Vec2 home = scenario == 0 ? Vec2{0, 500}
                                    : scenario == 1 ? Vec2{.25f, .25f} : Vec2{800, 500};
                    b.movers = {{"returning", Model::APM4220, home}};
                    if (scenario == 2) {
                        b.movers.push_back({"left", Model::APM4220, home - Vec2{size.x + 8, 0}});
                        b.movers.push_back({"right", Model::APM4220, home + Vec2{size.x + 8, 0}});
                    }
                    Simulation parking(b);
                    parking.workload = false;
                    auto config = parking.motion_settings();
                    config.profile = profile;
                    parking.configure_motion(config);
                    const auto reserved = parking.movers()[0].parking;
                    require(!parking.move_to(0, {size.x / 2 - 1, 700}),
                            "Manual goal accepted a footprint outside the world");
                    for (const auto goal : {Vec2{800, 950} + size / 2, reserved}) {
                        require(parking.move_to(0, goal), "Parking approach rejected");
                        parking.paused = false;
                        for (int tick = 0; tick < 8000 && parking.movers()[0].moving; ++tick) {
                            parking.tick();
                            const auto &m = parking.movers()[0];
                            require(kinetra::finite(m.forces.border) &&
                                        kinetra::length(m.body.acceleration) <= config.force + 1e-5f,
                                    "Parking produced an invalid acceleration");
                        }
                        if (parking.movers()[0].moving) {
                            const auto &m = parking.movers()[0];
                            std::cerr << "parking profile=" << int(profile) << " scenario=" << scenario
                                      << " pos=" << m.body.position.x << ',' << m.body.position.y
                                      << " goal=" << goal.x << ',' << goal.y << " route=" << m.route.size()
                                      << " velocity=" << m.body.velocity.x << ',' << m.body.velocity.y << '\n';
                        }
                        require(!parking.movers()[0].moving, "Parking approach failed to settle");
                        require(parking.collisions() == 0, "Parking approach contacted a wall or neighbor");
                    }
                    require(kinetra::distance(parking.movers()[0].body.position, reserved) == 0 &&
                                kinetra::length(parking.movers()[0].body.velocity) == 0,
                            "Mover failed to park at its reservation");
                    for (int tick = 0; tick < 200; ++tick) parking.tick();
                    require(kinetra::distance(parking.movers()[0].body.position, reserved) == 0,
                            "Parked mover drifted after arrival");
                }
        }
        {
            auto b = fixture();
            b.producers = {{"Kuka", Model::Kuka, {2300, 100}, {300, 250}}};
            b.movers = {{"trapped", Model::APM4220, {5, 5}},
                        {"front", Model::APM4220, {5, 123}},
                        {"side", Model::APM4220, {123, 5}},
                        {"available", Model::APM4220, {800, 500}}};
            Simulation departure(b);
            departure.workload = false;
            departure.toggle_mover(1);
            departure.toggle_mover(2);
            departure.add_product(ProductKind::trimmer);
            departure.single_step();
            require(!departure.movers()[0].product && departure.movers()[3].product.has_value(),
                    "Factory assigned a mover trapped behind parked neighbors");
        }
        {
            auto b = fixture();
            b.producers.clear();
            b.movers.push_back({"overlap", Model::APM4220, b.movers[0].position + Vec2{10,0}});
            Simulation contacts(b);
            contacts.workload = false;
            contacts.single_step();
            require(contacts.collisions() == 1 && contacts.contact_ticks() == 1,
                    "Contact pair was counted more than once");
            contacts.single_step();
            require(contacts.collisions() == 1 && contacts.contact_ticks() == 2,
                    "Persistent contact was counted as a new event");
            require(contacts.movers()[0].collision_ticks == 2 && contacts.movers()[1].collision_ticks == 2,
                    "Per-mover contact duration is inconsistent");
        }
        {
            auto b = fixture();
            b.producers.clear();
            b.movers[0].position = {300, 700};
            Simulation solo(b);
            solo.workload = false;
            require(solo.move_to(0, {1900, 750}), "Solo goal rejected");
            solo.paused = false;
            for (int tick = 0; tick < 80; ++tick) solo.tick();
            require(solo.movers()[0].replan == 80,
                    "A mover replanned because of its own heatmap footprint");
            for (int tick = 0; tick < 5000 && solo.movers()[0].moving; ++tick) solo.tick();
            require(!solo.movers()[0].moving && solo.collisions() == 0,
                    "Solo route failed to arrive without contact");
        }
        {
            auto b = fixture();
            b.producers.clear();
            b.movers = {{"moving", Model::APM4220, {500, 700}},
                        {"parked", Model::APM4220, {510, 700}}};
            Simulation parked(b);
            parked.workload = false;
            const auto position = parked.movers()[1].body.position;
            require(parked.move_to(0, {1700, 700}), "Departure goal rejected");
            parked.paused = false;
            for (int tick = 0; tick < 1000; ++tick) {
                parked.tick();
                require(kinetra::distance(parked.movers()[1].body.position, position) == 0 &&
                            kinetra::length(parked.movers()[1].body.velocity) == 0,
                        "Contact displaced a parked mover");
            }
        }
        {
            auto b = fixture();
            b.producers.clear();
            b.movers[0].position = {650, 700};
            b.zones.push_back({"wall", {1000, 400}, {200, 800}});
            Simulation detour(b);
            detour.workload = false;
            require(detour.move_to(0, {1300, 750}), "Detour goal rejected");
            detour.paused = false;
            for (int tick = 0; tick < 5000 && detour.movers()[0].moving; ++tick) {
                detour.tick();
                require(detour.movers()[0].no_progress < 100,
                        "A valid detour was classified as lack of route progress");
            }
            require(!detour.movers()[0].moving && detour.collisions() == 0,
                    "Detour failed to reach its goal without contact");
        }
        for (bool crossing : {false, true}) {
            std::uint64_t reference_contacts{};
            for (auto profile : {SteeringProfile::original, SteeringProfile::improved, SteeringProfile::reciprocal}) {
                auto b = fixture();
                b.producers.clear();
                b.movers = {{"A", Model::APM4220, {500, 700}},
                            {"B", Model::APM4220, crossing ? Vec2{1000, 200} : Vec2{1500, 700}}};
                Simulation trial(b);
                trial.workload = false;
                auto settings = trial.motion_settings();
                settings.profile = profile;
                trial.configure_motion(settings);
                const auto a = trial.movers()[0].body.position;
                const auto c = trial.movers()[1].body.position;
                require(trial.move_to(0, crossing ? Vec2{1700, a.y} : c), "Encounter goal A rejected");
                require(trial.move_to(1, crossing ? Vec2{c.x, 1300} : a), "Encounter goal B rejected");
                trial.paused = false;
                for (int tick=0; tick<5000; ++tick) trial.tick();
                std::cout << "ENCOUNTER " << crossing << " PROFILE " << int(profile)
                          << " CONTACTS " << trial.collisions() << " MOVING "
                          << trial.movers()[0].moving << trial.movers()[1].moving << '\n';
                if (profile == SteeringProfile::original) reference_contacts = trial.collisions();
                else {
                    require(!trial.movers()[0].moving && !trial.movers()[1].moving,
                            "Encounter did not reach both goals");
                    require(trial.collisions() == 0 && trial.collisions() <= reference_contacts,
                            "Encounter increased contacts");
                }
            }
        }
        const auto files = discover_blueprints(default_blueprint_directory());
        require(files.size() == 14, "Original blueprint bundle is incomplete");
        if (argc == 2 && std::string(argv[1]) == "--check-resources") {
            for (const auto &file : files)
                load_blueprint(file);
            std::cout << "PASS relocated blueprint catalog\n";
            return 0;
        }
        junction_traffic_contracts();
        parking_traffic_contracts();
        for (const auto &file : files) {
            const auto b = load_blueprint(file);
            Simulation simulation(b);
            require(!simulation.movers().empty() && !simulation.producers().empty(),
                    "Blueprint groups were not expanded");
            require(simulation.paused, "Blueprint must start paused");
            simulation.workload = false;
            simulation.paused = false;
            for (int i = 0; i < 20; ++i)
                simulation.tick();
        }
        const auto mas = load_blueprint(default_blueprint_directory() / "MAS-demonstrator.json");
        require(mas.movers.size() == 11 && mas.producers.size() == 4 && mas.cell_size.x == 240,
                "Original blueprint semantics changed");
        require(mas.producers[0].id == "Kuka_1" && mas.producers[3].rectangular,
                "IDs or rectangular producer lost");
        require(mover_spec(Model::APM4350).dimension.y == 235 && mover_spec(Model::APM4550).payload == 4.5f,
                "Mover models changed");
        for (const auto kind : {ProductKind::trimmer, ProductKind::personalized}) {
            Simulation simulation(fixture());
            simulation.workload = false;
            simulation.advance(5);
            require(simulation.ticks() == 0, "Paused simulation advanced");
            simulation.add_product(kind);
            simulation.paused = false;
            for (int i = 0; i < 30000 && !simulation.completed(); ++i)
                simulation.tick();
            if (!simulation.completed()) {
                for (const auto &p : simulation.products())
                    std::cerr << "stuck product step=" << p.step << " state=" << int(p.state) << '\n';
                const auto &m = simulation.movers()[0];
                std::cerr << "mover=" << m.body.position.x << "," << m.body.position.y
                          << " dest=" << m.destination.x << "," << m.destination.y << " moving=" << m.moving
                          << '\n';
            }
            require(simulation.completed() == 1 && simulation.products().empty(),
                    "Full local product recipe did not complete");
            const auto count = recipe(kind).size();
            require(simulation.movers()[0].interactions == count && !simulation.movers()[0].product,
                    "Mover not released after recipe completion");
            std::uint64_t processing = 0;
            for (const auto &p : simulation.producers())
                processing += p.processing_ticks;
            require(processing == count * 1000,
                    "Production duration differs from original 1000 ticks per interaction");
        }
        Simulation factory(mas, 42);
        factory.paused = false;
        for (int i = 0; i < 50000; ++i)
            factory.tick();
        if (factory.completed() < 5) {
            std::cerr << "MAS completed=" << factory.completed() << '\n';
            for (const auto &m : factory.movers())
                std::cerr << m.definition.id << " pos=" << m.body.position.x << ',' << m.body.position.y
                          << " goal=" << m.destination.x << ',' << m.destination.y << " moving=" << m.moving
                          << " unreachable=" << m.unreachable << " route=" << m.route.size()
                          << " blocked-start=" << !factory.grid().clear(m.body.position, m.body.position, m.body.half_extents)
                          << '\n';
        }
        require(factory.completed() >= 5, "Original MAS workload deadlocked instead of completing recipes");
        for (const auto &producer : factory.producers())
            require(producer.queue.size() <= mas.producer_max_queue, "Producer queue capacity exceeded");
        for (const auto *name : {"3840x2400.json", "8640x4800.json"}) {
            const auto large = load_blueprint(default_blueprint_directory() / name);
            Simulation recovery(large, 915839214);
            auto recovery_settings = recovery.motion_settings();
            recovery_settings.recovery = true;
            recovery.configure_motion(recovery_settings);
            recovery.paused = false;
            bool recovered = false;
            for (int i = 0; i < 50000; ++i) {
                recovery.tick();
                for (const auto &mover : recovery.movers()) {
                    recovered = recovered || mover.detour;
                    require(kinetra::finite(mover.body.position), "Route recovery produced an invalid pose");
                }
            }
            require(recovered, "Large factory regression never exercised route recovery");
            require(recovery.completed() > 0, "Large factory workload did not complete any products");
        }
        Simulation blocked(fixture());
        blocked.workload = false;
        blocked.add_product(ProductKind::trimmer);
        blocked.paused = false;
        for (int i = 0; i < 5000 && blocked.producers()[0].remaining == 0; ++i)
            blocked.tick();
        require(blocked.producers()[0].remaining > 0, "Fixture never started production");
        blocked.toggle_producer(0);
        const auto remaining = blocked.producers()[0].remaining;
        for (int i = 0; i < 30; ++i)
            blocked.tick();
        require(blocked.producers()[0].remaining == remaining, "Blocked producer kept processing");
        blocked.toggle_producer(0);
        for (int i = 0; i < 30000 && !blocked.completed(); ++i)
            blocked.tick();
        require(blocked.completed() == 1, "Blocked producer could not resume its recipe");
        Simulation a(fixture(), 7), b(fixture(), 7);
        a.paused = b.paused = false;
        for (int i = 0; i < 2000; ++i) {
            a.tick();
            b.tick();
        }
        require(a.ticks() == b.ticks() &&
                    kinetra::distance(a.movers()[0].body.position, b.movers()[0].body.position) == 0,
                "Seeded simulation is not deterministic");
        a.toggle_mover(0);
        const auto p = a.movers()[0].body.position;
        for (int i = 0; i < 100; ++i)
            a.tick();
        require(kinetra::distance(p, a.movers()[0].body.position) == 0, "Blocked mover still moves");
        auto limited = fixture();
        limited.tick_cap = 3;
        Simulation cap(limited);
        cap.paused = false;
        for (int i = 0; i < 10; ++i)
            cap.tick();
        require(cap.ticks() == 3 && cap.paused, "Tick cap ignored");
        const auto dir = std::filesystem::temp_directory_path() / "kinetra-i4sim-regression";
        std::filesystem::create_directories(dir);
        a.dump(dir);
        require(std::filesystem::file_size(dir / "summary.json") > 100, "Summary export missing");
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
        require(rejected, "Invalid blueprint was accepted or error lacks file context");
        std::cout << "PASS all original blueprints, both recipes, MAS workload, timing, determinism, "
                     "blocking and export\n";
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}

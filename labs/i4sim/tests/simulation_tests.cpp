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
} // namespace
int main(int argc, char **argv) {
    try {
        using namespace i4sim;
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
                                 SteeringProfile::experimental_rvo}) {
                auto config = probe.motion_settings();
                config.profile = profile;
                config.heatmap = false;
                probe.configure_motion(config);
                probe.single_step();
                require(kinetra::finite(probe.movers()[0].body.position), "Nonfinite profile motion");
            }
            auto config = probe.motion_settings();
            config.horizon = 65;
            bool rejected = false;
            try { probe.configure_motion(config); } catch (const std::invalid_argument &) { rejected = true; }
            require(rejected, "Unbounded horizon accepted");
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
            for (auto profile : {SteeringProfile::original, SteeringProfile::improved}) {
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
                            "Improved encounter did not reach both goals");
                    require(trial.collisions() == 0 && trial.collisions() <= reference_contacts,
                            "Improved encounter increased contacts");
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

#include "simulation.h"
#include <chrono>
#include <iostream>

int main(int argc, char **argv) {
    try {
        using namespace i4sim;
        if (argc > 2 || (argc == 2 && std::string(argv[1]) != "--recovery"))
            throw std::invalid_argument("Usage: kinetra_i4sim_motion_benchmark [--recovery]");
        std::cout << "blueprint,seed,completed,contacts,contact_ticks,acceleration_change,elapsed_ms\n";
        for (const char *name : {"MAS-demonstrator", "3840x2400", "8640x4800"}) {
            for (unsigned seed : {42u, 915839214u}) {
                Simulation simulation(load_blueprint(default_blueprint_directory() /
                                                     (std::string(name) + ".json")), seed);
                auto settings = simulation.motion_settings();
                settings.recovery = argc == 2;
                simulation.configure_motion(settings);
                simulation.paused = false;
                double acceleration_change = 0;
                const auto started = std::chrono::steady_clock::now();
                std::vector<Vec2> previous(simulation.movers().size());
                for (int tick = 0; tick < 50000; ++tick) {
                    simulation.tick();
                    for (std::size_t i = 0; i < previous.size(); ++i) {
                        const auto acceleration = simulation.movers()[i].body.acceleration;
                        acceleration_change += kinetra::distance(previous[i], acceleration);
                        previous[i] = acceleration;
                    }
                }
                const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now() - started).count();
                std::cout << name << ',' << seed << ',' << simulation.completed() << ','
                          << simulation.collisions() << ',' << simulation.contact_ticks() << ','
                          << acceleration_change << ',' << elapsed << std::endl;
            }
        }
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}

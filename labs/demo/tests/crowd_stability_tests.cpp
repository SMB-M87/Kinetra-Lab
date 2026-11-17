#include "../include/sandbox_application.h"
#include <iostream>
#include <stdexcept>

int main(int argc, char **argv) {
    unsigned seed = 0;
    int tick = 0;
    try {
        if (argc != 2)
            throw std::invalid_argument("Expected a deterministic crowd seed");
        seed = unsigned(std::stoul(argv[1]));
        sandbox::SandboxApplication application(240, seed);
        kinetra::Camera2D camera;
        camera.viewport = {1280, 800};
        camera.pixels_per_unit = 17;
        // These seeds stalled only after sustained crowd motion, beyond the old
        // five-frame smoke checks. Test simulated time rather than rendering speed.
        for (; tick < 7200; ++tick) {
            application.advance(1.0 / 120, {}, camera);
            const auto &world = application.scene().world;
            if (world.sweep_budget_exhausted())
                throw std::runtime_error("Ordinary crowd contact exhausted the swept-event budget");
            for (const auto &body : world.bodies())
                if (!kinetra::finite(body.position) || !kinetra::finite(body.velocity))
                    throw std::runtime_error("Crowd motion produced nonfinite state");
        }
        std::cout << "PASS crowded Demo seed " << seed << ": 60 simulated seconds\n";
    } catch (const std::exception &error) {
        std::cerr << "Seed " << seed << ", tick " << tick << ": " << error.what() << '\n';
        return 1;
    }
}

// kinetra_capture: renders the screenshots and clip frames of docs/media. See README.md.
#include "scenes.h"
#include "stage.h"

#include <kinetra/engine.h>

#include <cstdlib>
#include <exception>
#include <filesystem>
#include <iostream>
#include <stdlib.h>
#include <string>
#include <vector>

int main(int argc, char **argv) {
    if (argc < 2 || std::string(argv[1]) == "--help") {
        std::cerr << "Usage: kinetra_capture <output-directory> [scene...]\nScenes:";
        for (const auto &scene : capture::scenes())
            std::cerr << ' ' << scene.name;
        std::cerr << '\n';
        return argc < 2 ? EXIT_FAILURE : EXIT_SUCCESS;
    }
#ifndef _WIN32
    // Offscreen software rendering, unless the caller chose otherwise: the frames are read
    // from the front buffer, which is only reliable offscreen, and software Mesa gives the
    // same pixels on every machine.
    setenv("SDL_VIDEODRIVER", "offscreen", 0);
    setenv("LIBGL_ALWAYS_SOFTWARE", "1", 0);
#endif
    try {
        const std::filesystem::path output = argv[1];
        const std::vector<std::string> names(argv + 2, argv + argc);
        const auto selected = capture::select_scenes(names);
        std::filesystem::create_directories(output);
        for (const auto *scene : selected) {
            // One Runtime per scene, at the scene's size; only one may exist at a time.
            kinetra::Runtime runtime("Kinetra capture", scene->width, scene->height);
            // Frames go to files, not to a viewer: nothing has to wait for a display.
            runtime.limit_frames(false);
            runtime.poll(false);
            capture::Stage stage(runtime, output, std::string(scene->name));
            scene->script(stage);
            std::cout << scene->name << (scene->kind == capture::SceneKind::clip ? "/" : ".png") << '\n';
        }
    } catch (const std::exception &error) {
        std::cerr << "kinetra_capture: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}

// kinetra_capture: renders the screenshots and clip frames of docs/media. See README.md.
#include "scenes.h"
#include "stage.h"
#include "tour.h"

#include <kinetra/engine.h>

#include <cstdio>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <stdlib.h>
#include <string>
#include <vector>

namespace {
/// Ends a tour that was asked for a limited number of frames.
struct FrameLimitReached {};

/// Plays the tour and writes its frames as raw RGB, eight bits per channel, rows top-down,
/// to the file `target`, to standard output for "-", or nowhere for an empty name. Stops
/// after `limit` frames when that is positive. Returns the number of frames played.
long write_tour(const std::string &target, long limit) {
    std::FILE *out = target.empty() ? nullptr : target == "-" ? stdout : std::fopen(target.c_str(), "wb");
    if (!target.empty() && !out)
        throw std::runtime_error("Cannot write " + target);
    long frames = 0;
    try {
        kinetra::Runtime runtime("Kinetra tour", capture::tour_width, capture::tour_height);
        // Frames go to an encoder, not to a viewer: nothing has to wait for a display.
        runtime.limit_frames(false);
        runtime.poll(false);
        capture::Tour tour(runtime, [&](const capture::Canvas &frame) {
            if (out && std::fwrite(frame.rgb.data(), 1, frame.rgb.size(), out) != frame.rgb.size())
                throw std::runtime_error("Could not write a tour frame; is the encoder still running?");
            if (++frames == limit)
                throw FrameLimitReached{};
        });
        capture::play_tour(tour);
    } catch (const FrameLimitReached &) {
    } catch (...) {
        if (out && out != stdout)
            std::fclose(out);
        throw;
    }
    if (out && out != stdout)
        std::fclose(out);
    return frames;
}
} // namespace

int main(int argc, char **argv) {
    if (argc < 2 || std::string(argv[1]) == "--help") {
        std::cerr << "Usage: kinetra_capture <output-directory> [scene...]\n"
                     "       kinetra_capture --tour <file | -> [frame-limit]\n"
                     "       kinetra_capture --tour-check <frames>\n"
                     "       kinetra_capture --tour-format\nScenes:";
        for (const auto &scene : capture::scenes())
            std::cerr << ' ' << scene.name;
        std::cerr << '\n';
        return argc < 2 ? EXIT_FAILURE : EXIT_SUCCESS;
    }
    const std::string mode = argv[1];
    if (mode == "--tour-format") {
        // What an encoder has to be told about the raw frames of --tour.
        std::cout << capture::tour_width << 'x' << capture::tour_height << ' '
                  << capture::tour_frames_per_second << '\n';
        return EXIT_SUCCESS;
    }
#ifndef _WIN32
    // Offscreen software rendering, unless the caller chose otherwise: the frames are read
    // from the front buffer, which is only reliable offscreen, and software Mesa gives the
    // same pixels on every machine.
    setenv("SDL_VIDEODRIVER", "offscreen", 0);
    setenv("LIBGL_ALWAYS_SOFTWARE", "1", 0);
#endif
    try {
        if (mode == "--tour" || mode == "--tour-check") {
            // --tour-check plays the first frames without writing them, for the smoke test.
            const bool check = mode == "--tour-check";
            if (argc < 3 || argc > (check ? 3 : 4))
                throw std::invalid_argument("--tour needs a file or -, --tour-check a number of frames");
            const long limit = check ? std::stol(argv[2]) : argc == 4 ? std::stol(argv[3]) : 0;
            if (limit < 0 || (check && limit == 0))
                throw std::invalid_argument("The frame limit must be positive");
            const long frames = write_tour(check ? std::string() : std::string(argv[2]), limit);
            // Standard output may carry the frames, so the report goes to standard error.
            std::cerr << "kinetra_capture: tour of " << frames << " frames, "
                      << frames / capture::tour_frames_per_second << " s\n";
            return EXIT_SUCCESS;
        }
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

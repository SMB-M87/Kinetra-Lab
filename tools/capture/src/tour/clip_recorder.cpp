#include "capture/tour/clip_recorder.h"

#include "capture/png.h"

#include <cstdio>
#include <fstream>
#include <stdexcept>
#include <utility>

namespace capture {
ClipRecorder::ClipRecorder(std::filesystem::path directory, int keep_every, int frames_per_second)
    : directory_(std::move(directory)), keep_every_(keep_every) {
    if (keep_every <= 0 || frames_per_second <= 0)
        throw std::invalid_argument("A clip keeps every n-th frame at a positive rate");
    std::filesystem::create_directories(directory_);
    std::ofstream rate(directory_ / "framerate");
    rate << frames_per_second << '\n';
    if (!rate)
        throw std::runtime_error("Cannot write " + (directory_ / "framerate").string());
}

void ClipRecorder::start() {
    // The count restarts, so the first frame after start() is kept, whatever came before.
    recording_ = true;
    seen_ = 0;
}

void ClipRecorder::record(const Canvas &frame) {
    if (!recording_)
        return;
    if (seen_++ % keep_every_ != 0)
        return;
    char name[32];
    std::snprintf(name, sizeof name, "frame-%04d.png", ++written_);
    write_png(directory_ / name, static_cast<std::uint32_t>(frame.width),
              static_cast<std::uint32_t>(frame.height), frame.rgb);
}
} // namespace capture

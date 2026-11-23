#pragma once
// Writes frames of a Tour as the numbered PNG files of a clip, which capture_media.sh turns
// into a GIF like the frames Stage::record writes.
#include "capture/tour/canvas.h"

#include <filesystem>

namespace capture {
/// A sink for Tour frames that records a clip: after start(), every `keep_every`-th frame,
/// the first one included, as `<directory>/frame-NNNN.png` counted from 1, and the clip's
/// rate in `<directory>/framerate`, as Stage::record writes them.
///
/// Frames before start() or after stop() are dropped, so a script can go through the
/// launcher to the scene it shows first. Not copyable; pass it to a Tour through a lambda
/// that calls record().
class ClipRecorder {
  public:
    /// Creates `directory` and writes `frames_per_second` into its `framerate` file. Throws
    /// std::invalid_argument when `keep_every` or `frames_per_second` is not positive, and
    /// std::runtime_error when the directory or file cannot be written.
    ClipRecorder(std::filesystem::path directory, int keep_every, int frames_per_second);
    ClipRecorder(const ClipRecorder &) = delete;
    ClipRecorder &operator=(const ClipRecorder &) = delete;

    /// Records from the next frame on; that frame is kept.
    void start();
    /// Drops the frames from the next one on, until start().
    void stop() { recording_ = false; }
    bool recording() const { return recording_; }
    /// Takes one frame of the tour: writes it when it is recorded and its turn.
    void record(const Canvas &frame);
    /// Number of frame files written.
    int written() const { return written_; }

  private:
    std::filesystem::path directory_;
    int keep_every_;
    bool recording_{};
    /// Frames seen since start().
    int seen_{};
    int written_{};
};
} // namespace capture

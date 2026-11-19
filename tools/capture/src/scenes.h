#pragma once
// The captures in docs/media: every screenshot and clip, with its size and its script.
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace capture {
class Stage;

/// What a scene writes.
enum class SceneKind {
    /// One PNG file, `<scene>.png`.
    screenshot,
    /// A directory of numbered PNG frames, `<scene>/frame-NNNN.png`, that the media script
    /// turns into `<scene>.gif`.
    clip,
};

/// One capture: a name that is also its file name, its window size and its script.
struct Scene {
    /// File name without extension, as used in docs/media.
    std::string_view name;
    /// Whether the scene writes a screenshot or the frames of a clip.
    SceneKind kind;
    /// Window size in logical pixels.
    int width, height;
    /// Starts a session on the stage, drives it and writes the scene's file or frames.
    void (*script)(Stage &stage);
};

/// Every scene, in the order the gallery shows them: the launcher pages, then Demo, Dotio
/// and i4sim with their screenshots and clips.
const std::vector<Scene> &scenes();

/// The scenes named in `names`, in that order, or all scenes when `names` is empty.
/// Throws std::invalid_argument for a name that is not a scene.
std::vector<const Scene *> select_scenes(std::span<const std::string> names);
} // namespace capture

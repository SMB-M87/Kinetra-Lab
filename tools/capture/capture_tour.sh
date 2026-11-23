#!/usr/bin/env bash
# Renders the tour video: kinetra_capture plays the tour through the launcher and the three
# applications, and ffmpeg encodes its frames as H.264. Then tour_thumbnail.sh makes the
# README thumbnail in docs/media. The video itself goes to YouTube, not into the repository:
# its default place is the ignored build/ directory, and .gitignore keeps *.mp4 out of Git.
#
# Usage: tools/capture/capture_tour.sh [build-directory] [output-file]
#   build-directory  a configured build with the runtime, relative to the current directory
#                    (default: build-graphics in the repository)
#   output-file      the video to write (default: build/kinetra-lab-tour.mp4)
# Needs bash, ffmpeg with libx264 and a kinetra_capture binary in the build directory. The
# tool renders offscreen with software Mesa, so it needs no display. Dotio's enemies are not
# seeded, so its part of the tour differs from run to run.
set -euo pipefail

repository=$(cd "$(dirname "$0")/../.." && pwd)
build=${1:-$repository/build-graphics}
output=${2:-$repository/build/kinetra-lab-tour.mp4}
tool="$build/kinetra_capture"

if [[ ! -x $tool ]]; then
    echo "capture_tour: $tool not found; build it with: cmake --build --preset graphics --target kinetra_capture" >&2
    exit 1
fi
if ! command -v ffmpeg > /dev/null; then
    echo "capture_tour: ffmpeg not found; install it to encode the video" >&2
    exit 1
fi

# The tool says how large its raw frames are and how many there are per second.
read -r size rate < <("$tool" --tour-format)

work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT

# Raw RGB frames through a pipe: some fifteen thousand of them, 6 MB each. yuv420p and fast
# start make the file play in browsers; flat colors compress well at this quality.
"$tool" --tour - 2> >(grep -v '^MESA' >&2) |
    ffmpeg -loglevel error -y -f rawvideo -pixel_format rgb24 -video_size "$size" -framerate "$rate" -i - \
        -c:v libx264 -preset slow -crf 20 -pix_fmt yuv420p -movflags +faststart "$work/tour.mp4"

mkdir -p "$(dirname "$output")"
mv "$work/tour.mp4" "$output"
echo "$output $(du -h "$output" | cut -f1)"

# A new video gets a matching thumbnail, so the README never shows frames of an older tour.
"$repository/tools/capture/tour_thumbnail.sh" "$output" "$repository/docs/media/kinetra-lab-tour-thumbnail.png"

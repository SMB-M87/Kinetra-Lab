#!/usr/bin/env bash
# Makes the README thumbnail of the tour video: four moments of the tour (the launcher, Demo,
# Dotio and i4sim) in a 2 x 2 mosaic, with a play button in the middle.
#
# Usage: tools/capture/tour_thumbnail.sh [video] [output-file]
#   video        the tour video (default: .docs/kinetra-lab-tour.mp4, where capture_tour.sh
#                writes it)
#   output-file  the PNG to write (default: the README's docs/media/kinetra-lab-tour-thumbnail.png
#                for the default video, and the video's name with -thumbnail.png for any other,
#                so a trial render leaves the README's picture alone)
# Needs bash and ffmpeg with ffprobe. capture_tour.sh runs it after encoding a new video; run it
# on its own to refresh only the thumbnail. The moments are times in the tour, so after a change
# to the tour script, look at the thumbnail: every tile should still show its application.
set -euo pipefail

repository=$(cd "$(dirname "$0")/../.." && pwd)
default_video=$repository/.docs/kinetra-lab-tour.mp4
video=${1:-$default_video}
video_name=$(basename "$video")
# Resolved paths, so a relative spelling of the default video still updates the README's picture.
if [[ $(realpath -m "$video") == "$(realpath -m "$default_video")" ]]; then
    default_output=$repository/docs/media/kinetra-lab-tour-thumbnail.png
else
    default_output=$(dirname "$video")/${video_name%.*}-thumbnail.png
fi
output=${2:-$default_output}

# Seconds into the tour, one per tile, left to right and top to bottom: the launcher under the
# opening caption, Demo's agents heading for one clicked goal (CLICK SETS GOAL), Dotio zoomed out
# with its AI states, and the i4sim floor with its debug layers.
moments=(4 94 158 276)

# The filter graph below stacks exactly four tiles.
if [[ ${#moments[@]} -ne 4 ]]; then
    echo "tour_thumbnail: the mosaic needs four moments, not ${#moments[@]}" >&2
    exit 1
fi

# Half the source's 1920 x 1080, so four tiles make a 1280 x 720 picture: sharp on high-density
# screens at GitHub's README width, and still small as a PNG.
tile_width=640
tile_height=360
width=$((2 * tile_width))
height=$((2 * tile_height))

if [[ ! -f $video ]]; then
    echo "tour_thumbnail: $video not found; render it with tools/capture/capture_tour.sh" >&2
    exit 1
fi
if ! command -v ffmpeg > /dev/null || ! command -v ffprobe > /dev/null; then
    echo "tour_thumbnail: ffmpeg and ffprobe not found; install ffmpeg" >&2
    exit 1
fi

# A seek past the end gives ffmpeg no frame, so check the moments against the video first.
duration=$(ffprobe -v error -show_entries format=duration -of csv=p=0 "$video")
for moment in "${moments[@]}"; do
    if ! awk -v moment="$moment" -v duration="$duration" 'BEGIN { exit !(moment < duration) }'; then
        echo "tour_thumbnail: moment ${moment} s lies past the end of $video (${duration} s)" >&2
        exit 1
    fi
done

# The play button: a dark translucent disc with a white triangle pointing right, drawn per pixel
# on a transparent layer the size of the mosaic and centered where the four tiles meet.
radius=90
disc="lte(hypot(X-$tile_width,Y-$tile_height),$radius)"
triangle="gte(X,$tile_width-0.32*$radius)*lte(abs(Y-$tile_height),0.5*$radius*($tile_width+0.55*$radius-X)/(0.87*$radius))"
button="color=c=black@0:s=${width}x${height},format=rgba,geq=r='255*$triangle':g='255*$triangle':b='255*$triangle':a='if($triangle,255,if($disc,170,0))'[button]"

inputs=()
tiles=""
for index in "${!moments[@]}"; do
    inputs+=(-ss "${moments[$index]}" -i "$video")
    tiles+="[$index:v]scale=$tile_width:$tile_height:flags=lanczos[tile$index];"
done
mosaic="[tile0][tile1][tile2][tile3]xstack=inputs=4:layout=0_0|w0_0|0_h0|w0_h0[mosaic]"

work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT

ffmpeg -loglevel error -y "${inputs[@]}" \
    -filter_complex "$tiles$mosaic;$button;[mosaic][button]overlay=format=auto" \
    -frames:v 1 "$work/thumbnail.png"

# Replace the output only with a picture, so a failed run leaves the previous thumbnail.
if [[ ! -s $work/thumbnail.png ]]; then
    echo "tour_thumbnail: ffmpeg wrote no picture" >&2
    exit 1
fi
mv "$work/thumbnail.png" "$output"
echo "$output $(du -h "$output" | cut -f1)"

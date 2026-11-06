#!/usr/bin/env bash
# Converts a video to the game_playvideo format (docs/VIDEO_PLAYER.md): Ogg Theora video + Ogg Vorbis sound (.ogv).
# Uses ffmpeg from the linuxserver/ffmpeg Docker image (ffmpeg 9.0.2, libtheora, libvorbis), pinned by digest so the
# result is the same on every run; nothing is installed. Two-pass Theora at a fixed bitrate, so the size is known
# before players download it.
#
#   scripts/convert_video.sh <input> <output.ogv> [width] [fps] [video kbit/s] [vorbis quality]
#     width   default 960 (height follows the input's proportions, even)
#     fps     default: the input's; e.g. 30 to halve a 60 fps input (less to decode, smaller file)
#     kbit/s  default 1100 (Theora video bitrate)
#     vorbis  default 3 (-q:a, about 112 kbit/s stereo)
# Examples:
#   scripts/convert_video.sh intro.mp4 video/mymap/intro.ogv
#   scripts/convert_video.sh clip_60fps.webm video/mymap/clip.ogv 960 30 1100 3
set -euo pipefail
IMAGE=${FFMPEG_IMAGE:-linuxserver/ffmpeg@sha256:f4191c1abe18d75c49a23f7224d1403d55334f80013a47cb096f5056cf19617b}
IN=$(readlink -f "${1:?input}"); OUT=${2:?output.ogv}; W=${3:-960}; FPS=${4:-}; KB=${5:-1100}; QA=${6:-3}
mkdir -p "$(dirname "$OUT")"; OUTD=$(cd "$(dirname "$OUT")" && pwd); OUTF=$(basename "$OUT")
WORK=$(mktemp -d); trap 'rm -rf "$WORK"' EXIT
VF="scale=${W}:-2:flags=lanczos${FPS:+,fps=$FPS}"
ff() { timeout 3600 docker run --rm --memory=3g --memory-swap=3g --cpus=2 -u "$(id -u):$(id -g)" \
  -v "$IN:/in:ro" -v "$OUTD:/out" -v "$WORK:/work" -w /work --entrypoint ffmpeg "$IMAGE" \
  -hide_banner -loglevel error -y "$@"; }
ff -i /in -vf "$VF" -c:v libtheora -b:v "${KB}k" -pass 1 -passlogfile /work/pass -an -f ogg /dev/null
ff -i /in -vf "$VF" -c:v libtheora -b:v "${KB}k" -pass 2 -passlogfile /work/pass \
  -c:a libvorbis -q:a "$QA" -ac 2 -ar 44100 "/out/$OUTF"
ls -l "$OUT"

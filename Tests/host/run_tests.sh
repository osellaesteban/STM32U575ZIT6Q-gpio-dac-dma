#!/bin/sh
# Builds and runs the host tests with any C compiler (CC=gcc by default).
# The firmware sources are copied next to the fake main.h so that
# st_hw_config.h includes the fake one instead of Core/Inc/main.h.
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
ROOT="$HERE/../.."
BUILD=$(mktemp -d)
trap 'rm -rf "$BUILD"' EXIT

cp "$HERE/fake/main.h" "$HERE/test_host.c" "$BUILD/"
cp "$ROOT"/Core/Inc/st_*.h "$ROOT"/Core/Src/st_stimulator.c "$ROOT"/Core/Src/st_protocol.c \
   "$ROOT"/Core/Src/st_engine.c "$ROOT"/Core/Src/st_pulse_stats.c "$BUILD/"
${CC:-gcc} -std=c99 -Wall -Wextra -Werror -g $CFLAGS -I"$BUILD" \
   -o "$BUILD/test_host" "$BUILD/test_host.c" "$BUILD/st_stimulator.c" \
   "$BUILD/st_protocol.c" "$BUILD/st_engine.c" "$BUILD/st_pulse_stats.c"
"$BUILD/test_host"

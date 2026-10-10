#!/bin/sh
# Builds the ROM with exactly the toolchain GitHub uses: the devkitpro/devkitarm
# Docker image pinned in .github/workflows/build.yml. Needs Docker running.
# Starts clean (as GitHub does), so the next native make rebuilds everything.
#   tools/build-ci.sh            build
#   tools/build-ci.sh --version  show the toolchain versions
set -e
cd "$(dirname "$0")/.."
IMAGE=$(sed -n 's/.*container: *\(devkitpro\/devkitarm[^ ]*\).*/\1/p' .github/workflows/build.yml)
[ -n "$IMAGE" ] || { echo "no devkitarm image in build.yml" >&2; exit 1; }
RUN="docker run --rm -v $PWD:/src/moonquake -w /src/moonquake $IMAGE"

if [ "$1" = --version ]; then
    $RUN sh -c '$DEVKITARM/bin/arm-none-eabi-gcc --version | head -1;
        dkp-pacman -Q | grep -E "^(devkitARM|libgba|maxmod-gba|gba-tools|grit) "'
    exit
fi
$RUN sh -c 'git config --global --add safe.directory /src/moonquake && make clean && make'

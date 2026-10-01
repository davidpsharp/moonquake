#!/bin/sh
# Play a linked two player game between two emulated Gameboys and check they stay in step.
#
#   tools/linktest/run.sh [linktest options...]
#
# Needs libmgba built from source, MGBA (default ~/gba/mgba) with the static library in
# $MGBA/build-lib. Extra options are passed to linktest, e.g. --seed 0:5 --shot 1:900
set -e
here=$(cd "$(dirname "$0")" && pwd)
top=$(cd "$here/../.." && pwd)
MGBA=${MGBA:-$HOME/gba/mgba}
DEVKITARM=${DEVKITARM:-/opt/devkitpro/devkitARM}
ROM=${ROM:-$top/$(basename "$top").gba}
ELF=${ELF:-${ROM%.gba}.elf}

if [ ! -x "$here/linktest" ] || [ "$here/linktest.c" -nt "$here/linktest" ]; then
    cc -O2 -std=gnu11 -o "$here/linktest" "$here/linktest.c" \
        -I"$MGBA/include" -I"$MGBA/build-lib/include" \
        "$MGBA/build-lib/libmgba.a" -lpthread -lm $( [ "$(uname)" = Darwin ] && echo "-framework CoreFoundation" )
fi

# game state compared between the two Gameboys, plus the symbols linktest needs
syms=""
for name in universalTimer frameDone numPlayers matchOver area bombVal bombOwner player \
        robot rubbleCount gameRandSeed robotMoveSeed mysteryTokenSeed robotsHalt nuked level; do
    line=$("$DEVKITARM/bin/arm-none-eabi-nm" -S "$ELF" | awk -v n="$name" '$4 == n { print $1 ":" $2 }')
    [ -n "$line" ] || { echo "no symbol $name in $ELF" >&2; exit 2; }
    syms="$syms --sym $name=$line"
done

# skip the credits, then on each Gameboy: up (to 2 PLAYER LINK, the last option), A
script="--keys 0:200:1:4 --keys 1:200:1:4 --keys 0:420:64:4 --keys 1:430:64:4 --keys 0:460:1:4 --keys 1:500:1:4"

exec "$here/linktest" $syms $script "$@" "$ROM"

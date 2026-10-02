#!/bin/sh
# Play a linked two player game between two emulated Gameboys and check they stay in step.
#
#   tools/linktest/run.sh [linktest options...]
#
# Needs libmgba built from source, MGBA (default ~/gba/mgba) with the static library in
# $MGBA/build-lib, e.g.
#
#   git clone --branch 0.10.2 https://github.com/mgba-emu/mgba.git ~/gba/mgba
#   mkdir ~/gba/mgba/build-lib && cd ~/gba/mgba/build-lib
#   cmake .. -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -DBUILD_STATIC=ON -DBUILD_SHARED=OFF \
#       -DBUILD_QT=OFF -DBUILD_SDL=OFF -DM_CORE_GB=OFF -DUSE_LUA=OFF -DENABLE_SCRIPTING=OFF
#   make
#
# Extra options are passed to linktest, e.g. --seed 0:5 --shot 1:900 (see linktest.c).
# EXTRA_SYMS="name ..." adds more game variables, e.g. for --trace.
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
for name in universalTimer frameDone linked matchOver area bombVal bombOwner player \
        robot rubbleCount gameRandSeed robotMoveSeed mysteryTokenSeed robotsHalt nuked level $EXTRA_SYMS; do
    line=$("$DEVKITARM/bin/arm-none-eabi-nm" -S "$ELF" | awk -v n="$name" '$4 == n { print $1 ":" $2 }')
    [ -n "$line" ] || { echo "no symbol $name in $ELF" >&2; exit 2; }
    syms="$syms --sym $name=$line"
done

# link state GBA 0 looks at to start the game once everyone's in the lobby
line=$("$DEVKITARM/bin/arm-none-eabi-nm" -S "$ELF" | awk '$4 == "receivedWord" { print $1 ":" $2 }')
syms="$syms --info receivedWord=$line"

# skip the credits, then on each Gameboy: up (to 2-4 PLAYER LINK, the last option), A
script=""
for g in 0 1 2 3; do
    script="$script --keys $g:200:1:4 --keys $g:$((420 + g * 10)):64:4 --keys $g:$((460 + g * 20)):1:4"
done

exec "$here/linktest" $syms $script "$@" "$ROM"

#!/bin/sh
# Run all the linked game tests: 2-4 Gameboys with cartridges, sending the game by multiboot
# to Gameboys without, pausing, leaving from the pause menu, a reactor explosion, a Gameboy being switched off mid-game, and players who are out
# of the game leaving, and players being unplugged mid-game.
#
#   tools/linktest/all.sh [path to GBA BIOS, needed for the multiboot tests]
here=$(cd "$(dirname "$0")" && pwd)
bios=${1:-$HOME/gba/gba_bios.bin}
seeds="--seed 0:5 --seed 1:6 --seed 2:7 --seed 3:8"
# as on real hardware, the other Gameboys' SI pin reads low at times
flicker="--si-flicker"
failed=0

run() {
    name=$1
    shift
    output=$("$here/run.sh" $seeds $flicker --frames 60000 --trace dropped --check-board 0 "$@" 2>&1)
    result=$(echo "$output" | grep -E 'IN SYNC|DESYNC|NOTHING|crashed')
    # unless someone's being unplugged, no one should be dropped from the game
    case "$*" in
        *--unplug*|*--reset*) ;;    # (switched off, transfers stop, player 1 drops everyone)
        *)
            if echo "$output" | grep -qE 'dropped = [1-9a-f]'; then
                result="$result, but a player was dropped"
            fi
            ;;
    esac
    # the board checks: no out player killed again, no rubble or flames from nowhere
    if echo "$output" | grep -qE 'was out|no flame next to it|screen shows'; then
        result="$result, but $(echo "$output" | grep -E 'was out|no flame next to it|screen shows' | head -1)"
    fi
    # the reactor test has to have blown one up
    case "$*" in
        *"--trace nuked"*)
            echo "$output" | grep -q 'nuked = 1' || result="$result, but no reactor went off"
            ;;
    esac
    echo "$name: ${result:-no result}"
    [ -n "$result" ] || echo "$output" | tail -5
    case "$result" in
        "IN SYNC") ;;
        *) failed=1 ;;
    esac
}

run "2 players" --gbas 2
run "3 players" --gbas 3
run "4 players" --gbas 4
run "3 players, level 6" --gbas 3 --level 6
run "3 players, out player in the way of flames" --gbas 3 --level 0 --seed 0:12 --seed 1:112 --seed 2:212
# a reactor explosion: on level 4, with one reactor that goes off with one hit at 9,6, put
# one of red's bombs about to go off next to it at 8,6 (area[8][6], 8 * 13 + 6 bytes in), with
# the players standing still so that nothing (a death) gets in the way
elf=${ELF:-$(cd "$here/../.." && pwd)/moonquake.elf}
addr() {
    printf %x $(( 0x$("${DEVKITARM:-/opt/devkitpro/devkitARM}/bin/arm-none-eabi-nm" "$elf" | awk -v n="$1" '$3 == n { print $1 }') + 8 * 13 + 6 ))
}
run "2 players, reactor explosion" --gbas 2 --level 4 --idle 0 --idle 1 --trace nuked \
    --poke-game 200:$(addr area):18 --poke-game 200:$(addr bombVal):1 --poke-game 200:$(addr bombOwner):1
run "4 players, pause" --gbas 4 --pause 2:400
run "4 players, player 3 leaves from the pause menu" --gbas 4 --quit 2:600
run "4 players, player 1 leaves (ending it)" --gbas 4 --quit 0:600
run "4 players, one switched off" --gbas 4 --reset 3:1500 --frames 2000
run "4 players, out players leave" --gbas 4 --leave 1 --leave 2 --leave 3
run "4 players, out player unplugged" --gbas 4 --unplug-out 3 --seed 0:25 --seed 1:26 --seed 2:27 --seed 3:28
run "4 players, live player unplugged" --gbas 4 --unplug 3:2000
run "4 players, 2 unplugged together" --gbas 4 --unplug 2:2000 --unplug 3:2000
run "4 players, all but player 1 unplugged" --gbas 4 --unplug 1:2000 --unplug 2:2000 --unplug 3:2000
run "4 players, unplugged while paused" --gbas 4 --pause 1:400 --unplug 3:1500
if [ -f "$bios" ]; then
    run "2 players, multiboot" --gbas 2 --bios "$bios" --cart 1:none
    run "4 players, multiboot to 3" --gbas 4 --bios "$bios" --cart 1:none --cart 2:none --cart 3:none
    run "4 players, 2 carts + multiboot to 2" --gbas 4 --bios "$bios" --cart 2:none --cart 3:none
    run "3 players, cartless in the middle" --gbas 3 --bios "$bios" --cart 1:none
else
    echo "no BIOS at $bios, skipping multiboot tests"
fi

exit $failed

#!/bin/sh
# Run all the linked game tests: 2-4 Gameboys with cartridges, sending the game by multiboot
# to Gameboys without, pausing, a Gameboy being switched off mid-game, and players who are out
# of the game leaving, and players being unplugged mid-game.
#
#   tools/linktest/all.sh [path to GBA BIOS, needed for the multiboot tests]
here=$(cd "$(dirname "$0")" && pwd)
bios=${1:-$HOME/gba/GBA.BIOS}
seeds="--seed 0:5 --seed 1:6 --seed 2:7 --seed 3:8"
failed=0

run() {
    name=$1
    shift
    output=$("$here/run.sh" $seeds --frames 60000 --trace dropped "$@" 2>&1)
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
run "4 players, pause" --gbas 4 --pause 2:400
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

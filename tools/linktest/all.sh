#!/bin/sh
# Run all the linked game tests: 2-4 Gameboys with cartridges, sending the game by multiboot
# to Gameboys without, pausing, and a Gameboy being switched off mid-game.
#
#   tools/linktest/all.sh [path to GBA BIOS, needed for the multiboot tests]
here=$(cd "$(dirname "$0")" && pwd)
bios=${1:-$HOME/gba/GBA.BIOS}
seeds="--seed 0:5 --seed 1:6 --seed 2:7 --seed 3:8"
failed=0

run() {
    name=$1
    shift
    result=$("$here/run.sh" $seeds --frames 60000 "$@" 2>&1 | grep -E 'IN SYNC|DESYNC|NOTHING|crashed')
    echo "$name: ${result:-no result}"
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
if [ -f "$bios" ]; then
    run "2 players, multiboot" --gbas 2 --bios "$bios" --cart 1:none
    run "4 players, multiboot to 3" --gbas 4 --bios "$bios" --cart 1:none --cart 2:none --cart 3:none
    run "4 players, 2 carts + multiboot to 2" --gbas 4 --bios "$bios" --cart 2:none --cart 3:none
    run "3 players, cartless in the middle" --gbas 3 --bios "$bios" --cart 1:none
else
    echo "no BIOS at $bios, skipping multiboot tests"
fi

exit $failed

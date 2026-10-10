# moonquake
A bomberman clone for the Game Boy Advance, written in low-level C for the Game Boy's embedded ARM7TDMI hardware environment.

Reimplementation of the original game by Paul Taylor on Acorn RISC OS computers.

![image](https://github.com/user-attachments/assets/a5209c00-f989-4563-90f3-a27486fe5845)
![image](https://github.com/user-attachments/assets/66f9dd8f-26fc-44cc-919c-c900eb3b1d34)
![image](https://github.com/user-attachments/assets/d9f06d92-4d4c-4883-a3a3-0f6f96e249ac)
![image](https://github.com/user-attachments/assets/fb16b95a-4aef-42df-9e06-9a7826ca2e8d)




[Homepage](https://davidsharp.com/gba/)

Download the ROM from [Releases](https://github.com/davidpsharp/moonquake/releases).

## cheat for testing
On the title menu press L, R, L, R, SELECT (a double blip says it's worked). Until the Game Boy's
switched off, START GAME then asks which level to start on; SELECT there plays each sound effect in
turn.

## pausing, saving and quitting
START pauses; the pause menu has CONTINUE, SAVE AND QUIT (single player, on a cartridge) and
QUIT GAME (LEAVE GAME in a linked game), which asks again before going back to the title menu.
START also carries on.

In a single player game, SAVE AND QUIT saves and goes back to the title
menu. CONTINUE SAVED GAME then carries on from exactly where you were; the save's used up when
you continue, so to stop again pause and save again. It's kept in the cartridge's battery backed
SRAM (a Game Boy that was sent the game by multiboot has nowhere to save).

## 2-4 player link game
Connect up to four Game Boy Advances with link cables and choose **2-4 PLAYER LINK** on each. The
Game Boy with the small purple plug in is player 1 (green): it shows who's connected and starts the
game with START, choosing the level to play on with LEFT and RIGHT first. Green starts bottom
right, red top left, orange top right and pink bottom left, and it's the normal game with everyone
dropping bombs: 3 lives each, run out and you're out, last one alive wins. The game stays on that
level until then, clearing the rubble doesn't end it; a reactor explosion ends the match with no
winner ("GREEN BLEW UP THE REACTOR!"), so play on a lower level if you'd rather not have them.

In a linked game only the player who paused works the pause menu, so the others pressing keys
can't do anything by accident. LEAVE GAME takes just that player out and the rest play on, except
for player 1 (the host, whose Game Boy runs the link): that ends the game for everyone with
nobody winning, so it warns first.

If you're out of lives you can keep watching, or press SELECT to leave (or just unplug) and the
others play on. If someone's Game Boy gets unplugged mid-game they're out and the rest carry on.
Player 1 has to stay connected: its Game Boy runs the link. Unplug rather than switching off
while still connected, as a switched-off Game Boy on the cable may stop everyone.

An Analogue Pocket (firmware 1.6, openFPGA or its own GBA mode) works as player 2, 3 or 4,
including being sent the game. As player 1 some link cables work better than others: with a
Gamster 4-player cable it never got a link going with GBA SPs (Mario Kart: Super Circuit didn't
either), while plainer cables such as Nintendo's own (AGB-005) or Analogue's Pocket link cable are
expected to work but haven't been tried yet. With a cable like the Gamster, use a GBA as player 1.

Only player 1 needs the game. Switch the others on with no cartridge in: player 1 sends them the
game over the cable (it takes around half a minute for one, longer for three) and they start up
ready to play. Game Boys with and without cartridges can be mixed.

## building
With devkitPro (devkitARM, libgba, maxmod) installed: `make`. That makes `moonquake.gba`, and a
copy named after the version, e.g. `moonquake-v0_9_1.gba`. The version comes from git: the last
tag, plus how many commits since it if there are any (`v0.9.1-2-g1a2b3c4`), and `-dirty` if built
with changes not yet committed. It's shown in the bottom right of the credits screen, before the
title. The game is linked to run from EWRAM so it can send itself by multiboot; started from a
cartridge it copies itself there first.

GitHub Actions builds it too (`.github/workflows/build.yml`), on every push to main or by hand
from the Actions tab ("Run workflow"), and runs the link tests on it; the ROM's kept on each
run's page. The multiboot tests need a GBA BIOS, kept as a secret (see the workflow). Pushing a
version tag makes a release with the ROM attached, once the tests pass:

    git tag -a v0.9.2 -m "v0.9.2" && git push origin v0.9.2

`tools/linktest/run.sh` plays random two player games between two emulated Game Boys linked
together and checks the games stay identical frame by frame. It needs libmgba built from source,
see the script. `tools/linktest/all.sh` runs the lot: 2-4 players, pausing, a Game Boy being
switched off, and sending the game by multiboot (that needs a GBA BIOS file).

## licence and credits
The source code is under the MIT licence, see [LICENSE](LICENSE).

The licence doesn't cover the original game's material: the graphics (`graphics/`), the sound
samples (`maxmod_data/`) and the story text come from Paul Taylor's Moonquake for Acorn RISC OS,
(c) 1992 Paul Taylor, and remain his property. The game design is his too.

Built with devkitPro's libgba (LGPL, with an exception for static linking) and maxmod (ISC).

## changes
* October 2026 - 2-4 player deathmatch over the link cable, with multiboot so only one Game Boy needs the game.
* May 2023 - Migrated to latest GBA toolchain (devKitArm) and early experiments with 2-player link play across two linked Game Boys.
* 2004 - Original release.

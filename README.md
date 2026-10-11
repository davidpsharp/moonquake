# moonquake
Moonquake Advance: a Game Boy Advance conversion of Paul Taylor's 1992 Acorn RISC OS game Moonquake,
written in C for the GBA's ARM7TDMI. The objective is to clear the rubble from each of the 10
levels of a lunar base using bombs, while avoiding the base's malfunctioning security robots and
not detonating the nuclear reactors that power them.

Download the ROM from [Releases](https://github.com/davidpsharp/moonquake/releases).
[Original project page](https://davidsharp.com/gba/).

## key features
* **Single player game:** 10 levels with robots, reactors and power-up tokens, following the Acorn original's rules.
  Robot AI and level generation are translated directly from the original's ARM assembly.
* **2-4 player link play:** last-player-standing match over the GBA link cable. Verified on GBA SP
  hardware and on the Analogue Pocket with 4-player link cables.
* **Multiboot:** only player 1 needs the cartridge. The other units boot with no cartridge
  inserted, and the game is sent to them over the link cable by the BIOS multiboot protocol.
* **Timing and audio matched to the original:** flame, rubble, halo and token durations and
  sample playback rates were measured against the Acorn original running in the Arculator emulator.
* **Pause menu and save game:** a single player game can be saved to battery-backed cartridge SRAM
  and continued later.
* **Automated build and test:** GitHub Actions builds each commit with a pinned devkitARM toolchain
  and runs 2-4 player link scenarios (including multiboot) on emulated GBAs before releasing.
  The build version is shown on the credits screen.

## gameplay
<img width="480" alt="4 player link game, level 3" src="https://github.com/user-attachments/assets/06c2b09e-57d2-4738-9a94-cec6aaf5c21c" />
<img width="480" alt="Single player game, level 8" src="https://github.com/user-attachments/assets/3c5fb3a9-8155-4a93-a788-137837a48057" />

Left: a 4 player link game on level 3, as seen on player 1's (green's) screen. Right: a single player game on level 8.

<img width="320" alt="Waiting screen: 4 players connected" src="https://github.com/user-attachments/assets/af6bba17-ce7e-42c6-b7ae-3c703ef6d317" />
<img width="320" alt="Match start on player 2's screen" src="https://github.com/user-attachments/assets/7f4d5057-0d7a-4424-95f2-8ab9c0b45a96" />
<img width="320" alt="Explosions with several players on screen" src="https://github.com/user-attachments/assets/9d1f7284-76f0-42b6-b1fd-301f9662ac88" />
<img width="320" alt="A player dies: lives left for each player" src="https://github.com/user-attachments/assets/d14d4673-967a-4042-9be6-6b655c91fbe6" />
<img width="320" alt="Match result" src="https://github.com/user-attachments/assets/a21b2cbf-7c16-42f3-82db-32f2e3789335" />
<img width="320" alt="Reactor explosion" src="https://github.com/user-attachments/assets/d0dd8c5f-680b-49cf-a88b-bd6dd083cb88" />

4 player link game: waiting screen on player 1, match start on player 2, explosions, a player's
death, and the result. Last: a reactor explosion in a single player game on level 8.

## background
* Moonquake was written by Paul Taylor for Acorn RISC OS computers. It was distributed on the cover
  disk of the December 1992 issue of Archimedes World magazine.
* The original includes a 2 player battle mode. Acorn machines were in wide use in UK
  schools at the time, and the game was distributed widely in that setting.

## conversion
The 2004 conversion (v0.1.0 and v0.1.1):
* **Code:** reimplemented in C. The routines that determine gameplay behaviour, robot AI and
  level generation, were translated directly from the RISC OS version's ARM assembly so that the
  behaviour is identical. The remaining code was written new to reproduce the original's behaviour.
* **Assets:** graphics and sound samples were extracted from the original unmodified.
* **Sound:** sample playback used Richard Heasman's routines from his GBA conversion of Defender,
  without modification.
* **Hardware testing:** tested on a GBA SP with an XG Flash rewritable cartridge.
* **Single player only:** the 2004 conversion did not include the 2 player mode. Link cable
  communication and multiboot were outside the scope of that release.

From 2023 the code was moved to the devkitARM toolchain, libgba and the maxmod sound library. The
2026 enhancements were developed with Claude AI:
* **Lockstep link protocol:**
  * The master starts a multiplayer-mode serial transfer about every 1 ms from a timer
    interrupt (16 × 1024 CPU cycles). Each transfer carries one 16-bit word from every unit to
    all the others.
  * A game word holds a 2-bit frame number, the 6-bit input for that frame, and the input for
    the previous frame.
  * A unit does not advance to frame n+1 until it has every other player's input for frame n.
    Units therefore never differ by more than one frame, and the game state is identical on all
    units because it is computed from the same inputs.
* **Dropouts:** the master detects a disconnected unit and broadcasts its removal as a reserved
  input value (up and down together, which a player cannot press) in place of its own input for
  the stalled frame. All units therefore drop the player at the same frame.
* **Multiboot:**
  * The game is linked to execute from the 256 KB EWRAM. When started from a cartridge it copies
    itself there first.
  * The ROM image is about 200 KB, which leaves room for the game's working data. Graphics are
    stored LZ77-compressed and are unpacked into VRAM by the game, not by the BIOS.
  * Transfer to one client takes about 30 s, and longer for three.
* **Verification:** `tools/linktest` runs 2-4 emulated GBAs (libmgba) on an emulated link cable.
  It hashes the game state on every unit every frame and compares the hashes to detect
  divergence, and checks the board for flames or rubble with no adjacent flame.

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

GitHub builds in the `devkitpro/devkitarm` Docker image, pinned by digest in the workflow so the
compiler only changes when the pin does. `tools/build-ci.sh` builds locally in that same image
(with Docker running), whatever devkitPro you have installed; `tools/build-ci.sh --version` shows
its compiler and library versions. It builds from clean, so a plain `make` afterwards rebuilds
everything with your own toolchain.

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

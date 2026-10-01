# moonquake
A bomberman clone for the Game Boy Advance, written in low-level C for the Game Boy's embedded ARM7TDMI hardware environment.

Reimplementation of the original game by Paul Taylor on Acorn RISC OS computers.

![image](https://github.com/user-attachments/assets/a5209c00-f989-4563-90f3-a27486fe5845)
![image](https://github.com/user-attachments/assets/66f9dd8f-26fc-44cc-919c-c900eb3b1d34)
![image](https://github.com/user-attachments/assets/d9f06d92-4d4c-4883-a3a3-0f6f96e249ac)
![image](https://github.com/user-attachments/assets/fb16b95a-4aef-42df-9e06-9a7826ca2e8d)




[Homepage](https://davidsharp.com/gba/)

## 2 player link game
Connect two Game Boy Advances with a link cable and choose **2 PLAYER LINK** on both. Green starts
bottom right, red top left, and it's the normal game with someone else dropping bombs too: 3 lives
each, last one alive wins.

Only one Game Boy needs the game. Plug the small purple end of the cable into the one with the
cartridge, switch the other on with no cartridge in, and choose **2 PLAYER LINK**: the game is sent
across the cable (it takes around half a minute) and the other Game Boy starts up ready to play.

## building
With devkitPro (devkitARM, libgba, maxmod) installed: `make`. The game is linked to run from
EWRAM so it can send itself by multiboot; started from a cartridge it copies itself there first.

`tools/linktest/run.sh` plays random two player games between two emulated Game Boys linked
together and checks the games stay identical frame by frame. It needs libmgba built from source,
see the script. Add `--bios GBA.BIOS --rom2 none` to test sending the game by multiboot.

## changes
* October 2026 - 2 player deathmatch over the link cable, with multiboot so only one Game Boy needs the game.
* May 2023 - Migrated to latest GBA toolchain (devKitArm) and early experiments with 2-player link play across two linked Game Boys.
* 2004 - Original release.

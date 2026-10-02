// Saving a game to the cartridge's battery backed SRAM, to continue later

#ifndef SAVEGAME_H
#define SAVEGAME_H

#include <gba_types.h>

// a piece of game state to save
struct SaveBlock
{
    void* data;
    u32 size;
};

// TRUE if there's somewhere to save (there isn't on a Gameboy sent the game by multiboot)
bool canSaveGames(void);
// save the blocks (terminated by one with no data), returns FALSE if there's nowhere to save
bool saveGame(const struct SaveBlock* blocks);
// TRUE if there's a saved game matching the blocks' sizes
bool savedGameExists(const struct SaveBlock* blocks);
// load a saved game into the blocks, returns FALSE if there isn't one
bool loadGame(const struct SaveBlock* blocks);
// forget the saved game
void eraseSavedGame(void);

#endif

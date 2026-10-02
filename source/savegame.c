// Saving a game to the cartridge's battery backed SRAM, see savegame.h
//
// SRAM can only be read and written a byte at a time. The game's saved after a short header
// whose magic number is written last and checked along with a checksum, so a save that was
// interrupted, or SRAM that's never been written, isn't taken for a saved game.

#include <gba_base.h>

#include "multiboot.h"
#include "savegame.h"

#define TRUE 1
#define FALSE 0

#define SRAM_BASE   ((volatile u8*)0x0E000000)
#define MAGIC       0x5153514D      // "MQSQ"
#define HEADER_SIZE 12              // magic, size, checksum

// emulators and flash carts look for this to know the game saves to SRAM
const char sramSaveType[] = "SRAM_V113";


static void writeWord(u32 offset, u32 value)
{
    int i;
    for(i=0; i<4; i++)
        SRAM_BASE[offset + i] = (value >> (i * 8)) & 255;
}

static u32 readWord(u32 offset)
{
    u32 value = 0;
    int i;
    for(i=0; i<4; i++)
        value |= SRAM_BASE[offset + i] << (i * 8);
    return value;
}

static u32 totalSize(const struct SaveBlock* blocks)
{
    u32 size = 0;
    for( ; blocks->data; blocks++)
        size += blocks->size;
    return size;
}

// checksum of what's in SRAM after the header
static u32 sramChecksum(u32 size)
{
    u32 sum = 0;
    u32 i;
    for(i=0; i<size; i++)
        sum = sum * 31 + SRAM_BASE[HEADER_SIZE + i];
    return sum;
}

// a Gameboy sent the game by multiboot has no cartridge, so no SRAM
bool canSaveGames(void)
{
    // (reading the save type string keeps it in the game for emulators to find)
    return !multibooted() && ((volatile const char*)sramSaveType)[0] == 'S';
}

bool saveGame(const struct SaveBlock* blocks)
{
    if( !canSaveGames() )
        return FALSE;

    eraseSavedGame();

    u32 offset = HEADER_SIZE;
    const struct SaveBlock* block;
    for(block = blocks; block->data; block++)
    {
        const u8* data = block->data;
        u32 i;
        for(i=0; i<block->size; i++)
            SRAM_BASE[offset++] = data[i];
    }

    u32 size = totalSize(blocks);
    writeWord(4, size);
    writeWord(8, sramChecksum(size));
    writeWord(0, MAGIC);
    return TRUE;
}

bool savedGameExists(const struct SaveBlock* blocks)
{
    if( !canSaveGames() || readWord(0) != MAGIC )
        return FALSE;

    u32 size = totalSize(blocks);
    return readWord(4) == size && readWord(8) == sramChecksum(size);
}

bool loadGame(const struct SaveBlock* blocks)
{
    if( !savedGameExists(blocks) )
        return FALSE;

    u32 offset = HEADER_SIZE;
    const struct SaveBlock* block;
    for(block = blocks; block->data; block++)
    {
        u8* data = block->data;
        u32 i;
        for(i=0; i<block->size; i++)
            data[i] = SRAM_BASE[offset++];
    }
    return TRUE;
}

void eraseSavedGame(void)
{
    if( canSaveGames() )
        writeWord(0, 0);
}

// Sending Moonquake to another Gameboy with no cartridge, over the link cable

#ifndef MULTIBOOT_H
#define MULTIBOOT_H

#include <gba_types.h>

#define MULTIBOOT_SENT          0
#define MULTIBOOT_CANCELLED     1
#define MULTIBOOT_FAILED        2

// send this game to a Gameboy waiting to be multibooted, cancel() is polled and should
// return TRUE to give up
int multibootSend(bool (*cancel)(void));

// TRUE if this Gameboy was started by multiboot rather than from a cartridge
bool multibooted(void);

#endif

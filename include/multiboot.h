// Sending Moonquake to another Gameboy with no cartridge, over the link cable

#ifndef MULTIBOOT_H
#define MULTIBOOT_H

#include <gba_types.h>

#define MULTIBOOT_SENT          0
#define MULTIBOOT_CANCELLED     1
#define MULTIBOOT_FAILED        2
#define MULTIBOOT_NOBODY        3       // no Gameboy answered, there's no one to send it to

// send this game to any Gameboys waiting to be multibooted (with the BIOS logo showing),
// cancel() is polled and should return TRUE to give up, found() (if given) is called once
// there's someone to send it to (else it returns MULTIBOOT_NOBODY quickly)
int multibootSend(bool (*cancel)(void), void (*found)(void));

// TRUE if this Gameboy was started by multiboot rather than from a cartridge
bool multibooted(void);

#endif

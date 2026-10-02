// Link cable support for Moonquake's 2-4 player game
//
// The Gameboys play in lockstep: every game frame each one sends the others its controller
// input for that frame and waits for theirs before running the frame. They all run exactly
// the same simulation from exactly the same inputs, so they stay in step without ever
// sending game state over the cable.
//
// Each Gameboy is in a slot 0-3 decided by where it is on the cable, slot 0 (the one with
// the small purple plug in) is the master and starts games.

#ifndef LINK_H
#define LINK_H

#include <gba_types.h>

#define LINK_MAX_PLAYERS    4

// controller input bits exchanged each frame (6 bits)
#define IN_BOMB     1       // A or B
#define IN_UP       2
#define IN_DOWN     4
#define IN_LEFT     8
#define IN_RIGHT    16
#define IN_START    32

#define LINK_LOST   (-1)    // returned when another Gameboy stops answering

// interrupt handlers, install on IRQ_SERIAL and IRQ_TIMER3
void linkOnSerial(void);
void linkOnTimer(void);
// call from the vblank interrupt, the link uses it to notice when the cable's been pulled
void linkOnVBlank(void);

// enter multiplayer serial mode and tell the other Gameboys we're waiting to play
void linkStart(void);
// leave multiplayer serial mode
void linkStop(void);

// this Gameboy's slot, only right once there's been a transfer
int linkSlot(void);
// TRUE if this Gameboy is the master
bool linkIsMaster(void);

// bitmask of slots waiting to play, including this one
u8 linkWaitingMask(void);
// TRUE if there's a Gameboy with no game waiting to be sent one by multiboot
bool linkMultibootWaiting(void);

// master: start a game with the Gameboys in the slots given, returns FALSE if they don't all
// join in
bool linkStartGame(u8 mask);
// others: the slots in the game if the master's started one including this Gameboy, else 0
u8 linkGameStarted(void);

// swap this frame's input for all the other players', which are put in inputs[slot]
// returns LINK_LOST if any of them stops answering, else 0
int linkExchange(u8 input, u8* inputs);
// end the game on all the Gameboys at the same frame, goes back to waiting
void linkEndGame(void);

#endif

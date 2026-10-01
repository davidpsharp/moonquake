// Two player link cable support for Moonquake
//
// The two Gameboys play in lockstep: every game frame each one sends the other its
// controller input for that frame and waits for the other's before running the frame.
// Both run exactly the same simulation from exactly the same inputs, so they stay in step
// without ever sending game state over the cable.

#ifndef LINK_H
#define LINK_H

#include <gba_types.h>

// controller input bits exchanged each frame (6 bits)
#define IN_BOMB     1       // A or B
#define IN_UP       2
#define IN_DOWN     4
#define IN_LEFT     8
#define IN_RIGHT    16
#define IN_START    32

#define LINK_LOST   (-1)    // returned by linkExchange() when the other Gameboy stops answering

// what the other Gameboy appears to be doing
#define LINK_PEER_NONE      0   // nothing connected, or not in two player mode
#define LINK_PEER_WAITING   1   // waiting for us to start a game
#define LINK_PEER_PLAYING   2   // already started a game
#define LINK_PEER_MULTIBOOT 3   // a Gameboy with no game, waiting to be sent one by multiboot

// interrupt handlers, install on IRQ_SERIAL and IRQ_TIMER3
void linkOnSerial(void);
void linkOnTimer(void);
// call from the vblank interrupt, the link uses it to notice when the cable's been pulled
void linkOnVBlank(void);

// enter multiplayer serial mode and tell the other Gameboy we're waiting to play
void linkStart(void);
// leave multiplayer serial mode
void linkStop(void);

int linkPeerState(void);
// TRUE if this Gameboy is player 1 (the one with the small purple plug in)
bool linkIsMaster(void);

// start a game: from now on call linkExchange() once per frame on both Gameboys
void linkBeginGame(void);
// swap this frame's input for the other player's, or LINK_LOST
int linkExchange(u8 input);
// end the game on both Gameboys at the same frame, goes back to waiting
void linkEndGame(void);

#endif

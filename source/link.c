// Two player link cable support for Moonquake, see link.h
//
// Uses the GBA's multiplayer serial mode. The master (player 1) starts a transfer roughly
// every millisecond from a timer interrupt, each transfer swaps one 16 bit word between the
// two Gameboys. Each Gameboy just keeps sending its current word and remembers the last
// word it received, so a transfer that's missed or repeated doesn't matter.
//
// During a game the word sent is
//
//   bit 15     0
//   bit 14     1         (so a game word is never 0x0000, nor 0xFFFF which means no Gameboy)
//   bits 12-13 frame number, modulo 4
//   bits 6-11  input for that frame
//   bits 0-5   input for the frame before
//
// A Gameboy can't start frame n+1 until it has the other's input for frame n, so the two are
// never more than one frame apart. If the other Gameboy has already moved on to frame n+1
// its word still carries its input for frame n in the low bits.

#include <gba_base.h>
#include <gba_interrupt.h>
#include <gba_sio.h>
#include <gba_systemcalls.h>
#include <gba_timers.h>

#include "link.h"

#define TRUE 1
#define FALSE 0

#define SIO_SI_HIGH         (1 << 2)    // set on the slave, clear on the master
#define SIO_ALL_READY       (1 << 3)
#define SIO_MULTI_ERROR     (1 << 6)
#define SIO_MULTI_BUSY      (1 << 7)

#define WORD_NONE           0xFFFF      // what's received from a Gameboy that isn't there
#define WORD_WAITING        0x8001      // in two player mode, waiting to start a game
#define WORD_BIOS_WAITING   0x0000      // what the BIOS sends while waiting to be multibooted
#define WORD_IS_GAME(w)     (((w) & 0xC000) == 0x4000)
#define WORD_FRAME(w)       (((w) >> 12) & 3)
#define WORD_INPUT(w)       (((w) >> 6) & 63)
#define WORD_PREV_INPUT(w)  ((w) & 63)

// timer 3 ticks every 1024 cycles (61us), 16 ticks is about 1ms between transfers
#define TRANSFER_INTERVAL   16

// how long without hearing from the other Gameboy before giving up, in vblanks
#define NO_TRANSFERS_TIMEOUT    60      // cable pulled out or other Gameboy switched off
#define NO_PROGRESS_TIMEOUT     (60*10) // other Gameboy stopped playing

static volatile u16 sendWord = WORD_WAITING;
static volatile u16 receivedWord = WORD_NONE;
static volatile u32 transfers;          // count of good transfers, to spot the cable being pulled
static volatile u32 vblanks;
static bool linkActive;

static u8 frame;                        // frame number modulo 4
static u8 previousInput;


static bool isMaster(void)
{
    return !(REG_SIOCNT & SIO_SI_HIGH);
}

void linkOnSerial(void)
{
    u16 cnt = REG_SIOCNT;
    if( cnt & SIO_MULTI_ERROR )
        return;

    // master's word arrives in slot 0, slave's in slot 1
    u16 word = (&REG_SIOMULTI0)[ (cnt & SIO_SI_HIGH) ? 0 : 1 ];
    receivedWord = word;
    if( word != WORD_NONE )
        transfers++;

    // the slave's word must be in place before the master starts the next transfer,
    // it's done here as there's a guaranteed gap after each transfer
    if( cnt & SIO_SI_HIGH )
        REG_SIOMLT_SEND = sendWord;
}

void linkOnTimer(void)
{
    u16 cnt = REG_SIOCNT;
    if( isMaster() && (cnt & SIO_ALL_READY) && !(cnt & SIO_MULTI_BUSY) )
    {
        REG_SIOMLT_SEND = sendWord;
        REG_SIOCNT = cnt | SIO_MULTI_BUSY;
    }
}

void linkOnVBlank(void)
{
    vblanks++;
}

void linkStart(void)
{
    sendWord = WORD_WAITING;
    receivedWord = WORD_NONE;

    REG_RCNT = R_MULTI;
    REG_SIOMLT_SEND = sendWord;     // before multiplayer mode so 0 is never sent, see WORD_BIOS_WAITING
    REG_SIOCNT = SIO_MULTI | SIO_115200 | SIO_IRQ;

    REG_TM3CNT_H = 0;
    REG_TM3CNT_L = -TRANSFER_INTERVAL;
    REG_TM3CNT_H = TIMER_START | TIMER_IRQ | 3;    // 3 = 1024 cycles per tick

    irqEnable(IRQ_SERIAL | IRQ_TIMER3);
    linkActive = TRUE;
}

void linkStop(void)
{
    if( !linkActive )
        return;

    irqDisable(IRQ_SERIAL | IRQ_TIMER3);
    REG_TM3CNT_H = 0;
    REG_SIOCNT = 0;
    REG_RCNT = R_GPIO;  // general purpose mode, all pins input
    linkActive = FALSE;
}

int linkPeerState(void)
{
    u16 word = receivedWord;
    if( word == WORD_WAITING )
        return LINK_PEER_WAITING;
    if( WORD_IS_GAME(word) )
        return LINK_PEER_PLAYING;
    if( word == WORD_BIOS_WAITING )
        return LINK_PEER_MULTIBOOT;
    return LINK_PEER_NONE;
}

bool linkIsMaster(void)
{
    return isMaster();
}

void linkBeginGame(void)
{
    frame = 0;
    previousInput = 0;
    sendWord = 0x4000;
}

// wait for the other Gameboy's input for the current frame, if finishing a game then the
// other Gameboy having already gone back to waiting counts too
static int waitForFrame(bool finishing)
{
    u32 lastTransfers = transfers;
    u32 lastTransferTime = vblanks;
    u32 startTime = vblanks;

    for( ; ; )
    {
        u16 word = receivedWord;

        if( WORD_IS_GAME(word) )
        {
            if( WORD_FRAME(word) == frame )
                return WORD_INPUT(word);
            if( WORD_FRAME(word) == ((frame + 1) & 3) )
                return WORD_PREV_INPUT(word);
            // else still on the frame before, wait
        }
        else if( finishing && word == WORD_WAITING )
            return 0;

        if( transfers != lastTransfers )
        {
            lastTransfers = transfers;
            lastTransferTime = vblanks;
        }
        if( vblanks - lastTransferTime > NO_TRANSFERS_TIMEOUT || vblanks - startTime > NO_PROGRESS_TIMEOUT )
            return LINK_LOST;

        // sleep until the next interrupt
        Halt();
    }
}

int linkExchange(u8 input)
{
    sendWord = 0x4000 | (frame << 12) | (input << 6) | previousInput;

    int otherInput = waitForFrame(FALSE);

    previousInput = input;
    frame = (frame + 1) & 3;
    return otherInput;
}

void linkEndGame(void)
{
    // Both Gameboys call this at the same frame. Go through one more frame, then once this
    // Gameboy has the other's word for that frame the other must have had everything it
    // needs from us, and if it's already finished it'll be sending WORD_WAITING.
    sendWord = 0x4000 | (frame << 12) | previousInput;
    waitForFrame(TRUE);
    sendWord = WORD_WAITING;

    // and give the other a moment to see that before anything stops the link
    u32 startTime = vblanks;
    while( WORD_IS_GAME(receivedWord) && vblanks - startTime < NO_TRANSFERS_TIMEOUT )
        Halt();
}

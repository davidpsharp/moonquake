// Link cable support for Moonquake's 2-4 player game, see link.h
//
// Uses the GBA's multiplayer serial mode. The master starts a transfer roughly every
// millisecond from a timer interrupt, each transfer sends one 16 bit word from every Gameboy
// to all the others. Each Gameboy just keeps sending its current word and remembers the
// last word received from each slot, so a transfer that's missed or repeated doesn't matter.
//
// During a game the word sent is
//
//   bit 15     0
//   bit 14     1         (so a game word is never 0x0000, nor 0xFFFF which means no Gameboy)
//   bits 12-13 frame number, modulo 4
//   bits 6-11  input for that frame
//   bits 0-5   input for the frame before
//
// A Gameboy can't start frame n+1 until it has every other player's input for frame n, so
// no two are ever more than one frame apart. If another has already moved on to frame n+1
// its word still carries its input for frame n in the low bits.
//
// If a player's Gameboy is unplugged mid-game the master decides that player is out, and says
// so in place of its own input for the frame everyone's stuck on (an input no one can press,
// up and down together, plus which slots to drop): no one can have got past that frame without
// the missing player's input. All the Gameboys see the same transfers, so they all have the
// same last word from the missing player, with its inputs up to then.
//
// Before a game the master sends WORD_START with the slots playing, each of those replies
// WORD_JOINED, and once they all have the game starts. (Not a game word: that would be taken
// as the input for the first frame.)

#include <gba_base.h>
#include <gba_interrupt.h>
#include <gba_sio.h>
#include <gba_systemcalls.h>
#include <gba_timers.h>

#include "link.h"

#define TRUE 1
#define FALSE 0

#define SIO_SI_HIGH         (1 << 2)    // set on the slaves, clear on the master
#define SIO_ALL_READY       (1 << 3)
#define SIO_ID_SHIFT        4
#define SIO_MULTI_ERROR     (1 << 6)
#define SIO_MULTI_BUSY      (1 << 7)

#define WORD_NONE           0xFFFF      // what's received from a Gameboy that isn't there
#define WORD_WAITING        0x8001      // waiting to start a game
#define WORD_START          0x9000      // master starting a game, low 4 bits are the slots playing
#define WORD_JOINED         0x8002      // joined the game the master's starting
#define WORD_BIOS_WAITING   0x0000      // what the BIOS sends while waiting to be multibooted
#define WORD_IS_START(w)    (((w) & 0xFFF0) == WORD_START)
#define WORD_IS_GAME(w)     (((w) & 0xC000) == 0x4000)
#define WORD_FRAME(w)       (((w) >> 12) & 3)
#define WORD_INPUT(w)       (((w) >> 6) & 63)
#define WORD_PREV_INPUT(w)  ((w) & 63)

// timer 3 ticks every 1024 cycles (61us), 16 ticks is about 1ms between transfers
// (with four Gameboys a transfer takes longer than that, the timer just skips a go)
#define TRANSFER_INTERVAL   16

// how long without hearing from another Gameboy before giving up, in vblanks
#define NO_TRANSFERS_TIMEOUT    60      // cable pulled out or a Gameboy switched off
#define NO_PROGRESS_TIMEOUT     (60*10) // a Gameboy stopped playing
#define OUT_PLAYER_TIMEOUT      (60*5)  // a Gameboy whose player is out of the game stopped
#define UNPLUGGED_TIMEOUT       30      // a slot's been empty this long, drop its player

// master's input saying it's dropping players: up and down together (see link.h), with the
// slots in the other bits
#define DROP_INPUT          (IN_UP | IN_DOWN)
#define IS_DROP_INPUT(i)    (((i) & DROP_INPUT) == DROP_INPUT)
static const u8 dropSlotBits[LINK_MAX_PLAYERS] = { 0, IN_BOMB, IN_LEFT, IN_RIGHT };

static volatile u16 sendWord = WORD_WAITING;
static volatile u16 receivedWord[LINK_MAX_PLAYERS];
static volatile u16 lastGameWord[LINK_MAX_PLAYERS];     // not overwritten when a slot empties
static volatile u32 emptySince[LINK_MAX_PLAYERS];       // when a slot emptied, 0 if it isn't
static volatile u8 slot;
static volatile s8 transferId = -1;     // player number from the last good transfer, -1 none yet
static volatile u8 masterRepeats;       // transfers in a row the master's word hasn't changed
static volatile u32 transfers;          // count of good transfers, to spot the cable being pulled
static volatile u32 failedTransfers;    // and of ones with the error flag set (for diagnostics)
static volatile u32 vblanks;
static bool linkActive;

static void (*idle)(void);              // called once a frame while waiting for the other Gameboys
static u8 playing;                      // slots in the game
static u8 outOfGame;                    // slots whose players are out, their input isn't needed
static u8 dropped;                      // slots dropped during the last exchange
static u8 gameSlot;                     // this Gameboy's slot, and whether it's the master,
static bool gameMaster;                 // fixed for a game (an unplugged slave can look like a master)
static u8 frame;                        // frame number modulo 4
static u8 previousInput;


// The master's SI pin is grounded by the cable, the others' is driven by the Gameboy before
// them on it and on real hardware reads low during transfers too (mGBA keeps it high), so SI is
// only a safe guide before there's been a transfer. After one, the player number the hardware
// gives each Gameboy (0 for the master) is.
static bool isMaster(void)
{
    s8 id = transferId;
    if( id >= 0 )
        return 0 == id;
    return !(REG_SIOCNT & SIO_SI_HIGH);
}

void linkOnSerial(void)
{
    u16 cnt = REG_SIOCNT;
    if( cnt & SIO_MULTI_ERROR )
    {
        failedTransfers++;
        return;
    }

    int i;
    u16 masterWord = REG_SIOMULTI0;
    if( masterWord == receivedWord[0] )
    {
        if( masterRepeats < 255 )
            masterRepeats++;
    }
    else
        masterRepeats = 0;
    for(i=0; i<LINK_MAX_PLAYERS; i++)
    {
        u16 word = (&REG_SIOMULTI0)[i];
        receivedWord[i] = word;
        if( WORD_IS_GAME(word) )
            lastGameWord[i] = word;
        if( word != WORD_NONE )
            emptySince[i] = 0;
        else if( !emptySince[i] )
            emptySince[i] = vblanks | 1;
    }
    slot = (cnt >> SIO_ID_SHIFT) & 3;
    transferId = slot;
    transfers++;

    // a slave's word must be in place before the master starts the next transfer,
    // it's done here as there's a guaranteed gap after each transfer
    if( !isMaster() )
    {
        u16 word = sendWord;
        // If waiting while the master's doing something else, it's sending the game to other
        // Gameboys by multiboot. Look like an empty slot until it's done, else the BIOS
        // waits for this Gameboy to answer too. (Can't just leave multiplayer mode, that
        // would cut off any Gameboys further along the cable.)
        if( word == WORD_WAITING && masterWord != WORD_WAITING && !WORD_IS_START(masterWord) )
            word = WORD_NONE;
        REG_SIOMLT_SEND = word;
    }
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
    int i;
    for(i=0; i<LINK_MAX_PLAYERS; i++)
        receivedWord[i] = WORD_NONE;
    sendWord = WORD_WAITING;
    playing = 0;

    // still running (see linkStop()), leave the hardware be so as not to upset a transfer
    if( linkActive )
        return;

    transferId = -1;

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

    // A slave that leaves multiplayer mode while still plugged in stops all the transfers,
    // spoiling any game the others are playing, so it stays answering but as an empty slot
    // would, until it's unplugged or starts linking again. (Only the master really stops,
    // no one can play without it anyway.)
    if( !isMaster() )
    {
        sendWord = WORD_NONE;
        playing = 0;
        return;
    }

    irqDisable(IRQ_SERIAL | IRQ_TIMER3);
    REG_TM3CNT_H = 0;
    REG_SIOCNT = 0;
    REG_RCNT = R_GPIO;  // general purpose mode, all pins input
    linkActive = FALSE;
}

int linkSlot(void)
{
    return isMaster() ? 0 : slot;
}

bool linkIsMaster(void)
{
    return isMaster();
}

u8 linkWaitingMask(void)
{
    u8 mask = 1 << linkSlot();
    int i;
    for(i=0; i<LINK_MAX_PLAYERS; i++)
        if( receivedWord[i] == WORD_WAITING )
            mask |= 1 << i;
    return mask;
}

u16 linkSlotWord(int slot)
{
    return receivedWord[slot];
}

bool linkAllReady(void)
{
    return (REG_SIOCNT & SIO_ALL_READY) != 0;
}

u32 linkTransfers(bool failed)
{
    return failed ? failedTransfers : transfers;
}

bool linkMultibootWaiting(void)
{
    int i;
    for(i=1; i<LINK_MAX_PLAYERS; i++)
        if( receivedWord[i] == WORD_BIOS_WAITING )
            return TRUE;
    return FALSE;
}

static void beginGame(u8 mask)
{
    gameSlot = linkSlot();
    gameMaster = isMaster();
    playing = mask;
    outOfGame = 0;
    int i;
    for(i=0; i<LINK_MAX_PLAYERS; i++)
        lastGameWord[i] = WORD_NONE;
    frame = 0;
    previousInput = 0;
}

bool linkStartGame(u8 mask)
{
    sendWord = WORD_START | mask;

    // wait for all the others to join (or, quick off the mark, already be sending game words)
    u32 startTime = vblanks;
    for( ; ; )
    {
        bool allStarted = TRUE;
        int i;
        for(i=1; i<LINK_MAX_PLAYERS; i++)
            if( (mask & (1 << i)) && receivedWord[i] != WORD_JOINED && !WORD_IS_GAME(receivedWord[i]) )
                allStarted = FALSE;

        if( allStarted )
            break;
        if( vblanks - startTime > NO_TRANSFERS_TIMEOUT )
        {
            sendWord = WORD_WAITING;
            return FALSE;
        }
        Halt();
    }

    beginGame(mask);
    return TRUE;
}

u8 linkGameStarted(void)
{
    // while the master's sending the game by multiboot anything could go past, so make sure
    // it's really saying start
    u16 word = receivedWord[0];
    if( WORD_IS_START(word) && (word & (1 << linkSlot())) && masterRepeats >= 16 )
    {
        sendWord = WORD_JOINED;
        beginGame(word & 15);
        return playing;
    }
    return 0;
}

// a player's input for the current frame from their latest game word, -1 if not there yet
static int inputForFrame(int i)
{
    u16 word = lastGameWord[i];
    if( !WORD_IS_GAME(word) )
        return -1;
    if( WORD_FRAME(word) == frame )
        return WORD_INPUT(word);
    if( WORD_FRAME(word) == ((frame + 1) & 3) )
        return WORD_PREV_INPUT(word);
    return -1;  // still on the frame before (or not started)
}

static void dropPlayers(u8 input);

// the master: drop players whose Gameboys have been unplugged and are holding everyone up,
// by changing its own input for this frame (see the top of the file)
static void dropUnpluggedPlayers(u32 lastTransferTime)
{
    // with no one at all left on the cable there may be no transfers, so nothing to say the
    // slots are empty
    bool allGone = vblanks - lastTransferTime > UNPLUGGED_TIMEOUT;

    u8 drop = 0;
    int i;
    for(i=1; i<LINK_MAX_PLAYERS; i++)
    {
        u32 since = emptySince[i];
        bool empty = allGone || (since && vblanks - since > UNPLUGGED_TIMEOUT);
        if( (playing & ~outOfGame & (1 << i)) && inputForFrame(i) < 0 && empty )
            drop |= 1 << i;
    }
    if( !drop )
        return;

    u8 input = DROP_INPUT;
    for(i=1; i<LINK_MAX_PLAYERS; i++)
        if( drop & (1 << i) )
            input |= dropSlotBits[i];
    sendWord = 0x4000 | (frame << 12) | (input << 6) | previousInput;
    dropPlayers(input);
}

// act on a drop input from the master
static void dropPlayers(u8 input)
{
    int i;
    for(i=1; i<LINK_MAX_PLAYERS; i++)
    {
        if( (input & dropSlotBits[i]) && (playing & (1 << i)) )
        {
            dropped |= 1 << i;
            playing &= ~(1 << i);
        }
    }
}

// wait for every other player's input for the current frame, if finishing a game then
// a Gameboy having already gone back to waiting counts too
static int waitForFrame(bool finishing, u8* inputs)
{
    u32 lastTransfers = transfers;
    u32 lastTransferTime = vblanks;
    u32 startTime = vblanks;
    u8 me = gameSlot;

    for( ; ; )
    {
        // unplugged, a slave can look to itself like a master
        if( isMaster() != gameMaster )
            return LINK_LOST;

        if( gameMaster && !finishing )
            dropUnpluggedPlayers(lastTransferTime);

        bool haveAll = TRUE;
        int i;
        for(i=0; i<LINK_MAX_PLAYERS; i++)
        {
            if( i == me || !(playing & (1 << i)) )
                continue;

            int input = inputForFrame(i);
            u16 word = receivedWord[i];
            if( input >= 0 )
            {
                if( 0 == i && IS_DROP_INPUT(input) )
                {
                    dropPlayers(input);
                    input = 0;
                }
                inputs[i] = input;
            }
            else if( finishing && (word == WORD_WAITING || word == WORD_NONE) )
                inputs[i] = 0;  // finished (one that's waiting may look like an empty slot, see linkOnSerial())
            else if( (outOfGame & (1 << i)) && (!WORD_IS_GAME(word) || vblanks - startTime > OUT_PLAYER_TIMEOUT) )
            {
                // A player who's out has left, or been unplugged: stop waiting for that
                // Gameboy. No need for the others to agree when, its input isn't used.
                playing &= ~(1 << i);
                inputs[i] = 0;
            }
            else
                haveAll = FALSE;
        }
        if( haveAll )
            return 0;

        if( transfers != lastTransfers )
        {
            lastTransfers = transfers;
            lastTransferTime = vblanks;
        }
        if( vblanks - lastTransferTime > NO_TRANSFERS_TIMEOUT || vblanks - startTime > NO_PROGRESS_TIMEOUT )
            return LINK_LOST;

        // sleep until the next interrupt, keeping whatever has to be done every frame going
        // if the wait goes past a vblank
        u32 vblankBefore = vblanks;
        Halt();
        if( idle && vblanks != vblankBefore )
            idle();
    }
}

void linkSetIdle(void (*function)(void))
{
    idle = function;
}

void linkPlayerOut(int slot)
{
    outOfGame |= 1 << slot;
}

void linkLeave(void)
{
    // looks to the others as though this Gameboy's been unplugged
    linkStop();
}

int linkExchange(u8 input, u8* inputs)
{
    // players not waited for (dropped, or out and gone) have no input
    int i;
    for(i=0; i<LINK_MAX_PLAYERS; i++)
        inputs[i] = 0;

    // up and down together is the master's drop signal, so no input may be that (it isn't
    // possible on a real Gameboy's d-pad, but whatever's sent may not be from the d-pad)
    if( IS_DROP_INPUT(input) )
        input &= ~IN_DOWN;

    dropped = 0;
    sendWord = 0x4000 | (frame << 12) | (input << 6) | previousInput;

    int result = waitForFrame(FALSE, inputs);

    // the master may have swapped its input for a drop (see dropUnpluggedPlayers())
    u8 sent = WORD_INPUT(sendWord);
    if( gameMaster && IS_DROP_INPUT(sent) )
    {
        input = 0;
        previousInput = sent;   // so a Gameboy a frame behind sees the drop too
    }
    else
        previousInput = input;
    inputs[gameSlot] = input;

    frame = (frame + 1) & 3;
    return result;
}

u8 linkDropped(void)
{
    return dropped;
}

void linkEndGame(void)
{
    // All the Gameboys call this at the same frame. Go through one more frame, then once this
    // Gameboy has the others' words for that frame they must have had everything they need
    // from us, and any already finished will be sending WORD_WAITING.
    u8 inputs[LINK_MAX_PLAYERS];
    sendWord = 0x4000 | (frame << 12) | previousInput;
    waitForFrame(TRUE, inputs);
    sendWord = WORD_WAITING;

    // and give the others a moment to see that before anything stops the link
    u32 startTime = vblanks;
    for( ; ; )
    {
        bool anyPlaying = FALSE;
        int i;
        for(i=0; i<LINK_MAX_PLAYERS; i++)
            if( i != gameSlot && (playing & (1 << i)) && WORD_IS_GAME(receivedWord[i]) )
                anyPlaying = TRUE;
        if( !anyPlaying || vblanks - startTime > NO_TRANSFERS_TIMEOUT )
            break;
        Halt();
    }
    playing = 0;
}

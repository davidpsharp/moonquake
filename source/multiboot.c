// Sending Moonquake to another Gameboy with no cartridge, over the link cable
//
// The game is linked to run from EWRAM (see Makefile) so the image in EWRAM is exactly what
// the other Gameboy needs, it's sent from there. The handshake before the BIOS's MultiBoot
// call follows LinkCableMultiboot from gba-link-connection, see also GBATEK "BIOS Multi
// Boot (Single Game Pak)".

#include <gba_base.h>
#include <gba_dma.h>
#include <gba_interrupt.h>
#include <gba_multiboot.h>
#include <gba_sio.h>
#include <gba_video.h>

#include <string.h>

#include "multiboot.h"

#define TRUE 1
#define FALSE 0

#define HEADER_SIZE             0xC0
#define PALETTE_DATA            0x93        // colour and speed of the logo on the other Gameboy
#define CLIENT_NO_DATA          0xFF
#define DETECTION_TRIES         16
#define WAIT_BEFORE_TRANSFER    50          // scanlines
#define WAIT_BEFORE_RETRY       ((160 + 68) * 60)

#define HANDSHAKE               0x6200
#define HANDSHAKE_RESPONSE      0x7200
#define CONFIRM_CLIENTS         0x6100
#define SEND_PALETTE            0x6300
#define HANDSHAKE_DATA          0x11
#define CONFIRM_HANDSHAKE_DATA  0x6400
#define ACK_RESPONSE            0x73

#define NUM_CLIENTS             3

// returned by each step of the handshake
#define STEP_DONE               0
#define STEP_RETRY              1
#define STEP_CANCELLED          2
#define STEP_ERROR              3

extern u8 __boot_method;     // in the cartridge header, set by the BIOS when multibooted
extern u8 __ewram_overlay_lma[];  // end of the image (the EWRAM overlays aren't used),
                                    // __rom_end__ also covers .sbss which needn't be sent

static const u8 clientIds[NUM_CLIENTS] = { 2, 4, 8 };

static bool (*cancelled)(void);
static u16 responses[NUM_CLIENTS];


bool multibooted(void)
{
    return __boot_method != 0;
}

static void waitScanlines(u32 lines)
{
    u32 count = 0;
    u16 vcount = REG_VCOUNT;
    while( count < lines )
    {
        if( REG_VCOUNT != vcount )
        {
            count++;
            vcount = REG_VCOUNT;
        }
    }
}

static void multiplayerMode(void)
{
    REG_RCNT = R_MULTI;
    REG_SIOCNT = SIO_115200;
    REG_SIOCNT |= SIO_MULTI;
}

static void generalPurposeMode(void)
{
    REG_RCNT = R_GPIO;
}

// one transfer, the clients' replies end up in responses[]
static void exchange(u16 data)
{
    int i;
    for(i=0; i<NUM_CLIENTS; i++)
        responses[i] = 0xFFFF;

    waitScanlines(WAIT_BEFORE_TRANSFER);

    while( REG_SIOCNT & SIO_START )
        if( cancelled() )
            return;

    REG_SIOMLT_SEND = data;
    REG_SIOCNT |= SIO_START;

    while( REG_SIOCNT & SIO_START )
        if( cancelled() )
            return;

    for(i=0; i<NUM_CLIENTS; i++)
        responses[i] = (&REG_SIOMULTI1)[i];
}

// send data and check every client found replies as expected
static int compare(MultiBootParam* mp, u16 data, u16 expected)
{
    exchange(data);
    if( cancelled() )
        return STEP_CANCELLED;

    int i;
    for(i=0; i<NUM_CLIENTS; i++)
        if( (mp->client_bit & clientIds[i]) && responses[i] != (expected | clientIds[i]) )
            return STEP_ERROR;

    return STEP_DONE;
}

static int detectClients(MultiBootParam* mp)
{
    multiplayerMode();

    u32 t;
    for(t=0; t<DETECTION_TRIES; t++)
    {
        exchange(HANDSHAKE);
        if( cancelled() )
            return STEP_CANCELLED;

        int i;
        for(i=0; i<NUM_CLIENTS; i++)
        {
            if( (responses[i] & 0xFFF0) == HANDSHAKE_RESPONSE )
            {
                u8 id = responses[i] & 0xF;
                if( id == 2 || id == 4 || id == 8 )
                    mp->client_bit |= id;
                else
                    return STEP_RETRY;
            }
        }
    }

    if( !mp->client_bit )
    {
        generalPurposeMode();
        waitScanlines(WAIT_BEFORE_RETRY);
        return STEP_RETRY;
    }

    return STEP_DONE;
}

static int sendHeader(void)
{
    const u16* header = (const u16*)EWRAM;
    int i;
    for(i=0; i<HEADER_SIZE; i+=2)
    {
        exchange(*header++);
        if( cancelled() )
            return STEP_CANCELLED;
    }
    return STEP_DONE;
}

static int sendPalette(MultiBootParam* mp)
{
    exchange(SEND_PALETTE | PALETTE_DATA);
    if( cancelled() )
        return STEP_CANCELLED;

    int i;
    for(i=0; i<NUM_CLIENTS; i++)
        if( responses[i] >> 8 == ACK_RESPONSE )
            mp->client_data[i] = responses[i] & 0xFF;

    for(i=0; i<NUM_CLIENTS; i++)
        if( (mp->client_bit & clientIds[i]) && mp->client_data[i] == CLIENT_NO_DATA )
            return STEP_RETRY;

    return STEP_DONE;
}

static int confirmHandshakeData(MultiBootParam* mp)
{
    exchange(CONFIRM_HANDSHAKE_DATA | mp->handshake_data);
    if( cancelled() )
        return STEP_CANCELLED;

    // (only the Gameboys being sent the game reply, others may be on the cable waiting to play)
    int i;
    for(i=0; i<NUM_CLIENTS; i++)
        if( (mp->client_bit & clientIds[i]) && (responses[i] >> 8) != ACK_RESPONSE )
            return STEP_ERROR;
    return STEP_DONE;
}

// repeat a step while it asks to be retried, then FALSE if it didn't work out
#define TRY(STEP) \
    do { result = (STEP); } while( STEP_RETRY == result ); \
    if( STEP_DONE != result ) \
        goto finished;

int multibootSend(bool (*cancel)(void))
{
    MultiBootParam mp;
    int result;

    cancelled = cancel;

    memset(&mp, 0, sizeof(mp));
    mp.client_data[0] = CLIENT_NO_DATA;
    mp.client_data[1] = CLIENT_NO_DATA;
    mp.client_data[2] = CLIENT_NO_DATA;
    mp.palette_data = PALETTE_DATA;
    mp.boot_srcp = (u8*)EWRAM + HEADER_SIZE;
    // the length must be a multiple of 16
    mp.boot_endp = (u8*)EWRAM + ((((u32)__ewram_overlay_lma - EWRAM) + 15) & ~15);

    TRY( detectClients(&mp) )
    TRY( compare(&mp, CONFIRM_CLIENTS | mp.client_bit, HANDSHAKE_RESPONSE) )
    TRY( sendHeader() )
    TRY( compare(&mp, HANDSHAKE, 0) )
    TRY( compare(&mp, HANDSHAKE, HANDSHAKE_RESPONSE) )
    TRY( sendPalette(&mp) )

    mp.handshake_data = (HANDSHAKE_DATA + mp.client_data[0] + mp.client_data[1] + mp.client_data[2]) & 0xFF;

    TRY( confirmHandshakeData(&mp) )

    // GBATEK: wait 1/16 second before the BIOS sends the program, and keep interrupts off
    // while it does
    waitScanlines(228 * 4);
    u16 ime = REG_IME;
    REG_IME = 0;
    // the sound DMA (maxmod re-arms it every vblank) would hold up the BIOS's transfers
    REG_DMA1CNT = 0;
    REG_DMA2CNT = 0;
    result = MultiBoot(&mp, MODE16_MULTI) ? STEP_ERROR : STEP_DONE;
    REG_IME = ime;

finished:
    generalPurposeMode();

    switch(result)
    {
        case STEP_DONE      : return MULTIBOOT_SENT;
        case STEP_CANCELLED : return MULTIBOOT_CANCELLED;
        default             : return MULTIBOOT_FAILED;
    }
}

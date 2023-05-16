// before release
// ==============
// timings with Acorn version, should feel similar speed on gba hardware
// fix slow down on later levels with more robots
// tidy up source code, check comments and split up files

// to do list
// =============
// display score
// cheat menu from the start menu for debugging the gameplay
// sometimes clear levels don't end, something to do with user dying??
// need text to type everything on that screen if button pressed
// player slows down when moving over a column full of monsters (with 3 or more) - is collision detection too intensive
// needs option to save game, password level or some way so player doesn't have to start from scratch
// compress graphics and samples so can multiboot game?
// compile with optimisation turned on
// length of token sample wrong?
// have a pling noise when press A on 1 player/instructions option on menu
// run with profiler to see where we can optimise - really need to test on the actual hardware
// make it so only start button starts game on title screen
// how can we get it to play 2 samples simulateously - need to decide on priority of them too since more than 2 channels in Arc version
// check size of variables to use, do u8's cause a lot of ANDing in generated code?
// use static declarations of drawtile() rather than variable so can statically declare all the way down inlined code
// loop unroll some generalisations
// playing area surrounded by indestructible blocks that are never shown on display to reduce range checks
// get rid of where have set area[][] value before calling drawobject()
// does changing man's direction variable from x and y change to a single direction variable like robots
// do the graphics need gamma correcting to look ok on GBA hardware? http://www.pineight.com/gba/
// would be good to have the 2 player mode but would need to load itself into multiboot zone of another gameboy advance
// should wait be be != 160 instead of >= so if gets called in vblank already it waits til the next time vblanks starts?
// need to get the timings of player and droid movement as well as all other timings more similar to Acorn, need real hardware
// should OAM be volatile in the sprites.h file?
// split up code into multiple files

// include graphics data
extern const unsigned short background_Palette[256];
extern const unsigned char background_Bitmap[15360];
extern const unsigned short sprites_Palette[256];
extern const unsigned char sprites_Bitmap[26112];
extern const unsigned short titlescreen_Palette[256];
extern const unsigned char titlescreen_Bitmap[38400];
extern const unsigned short credits_Palette[256];
extern const unsigned char credits_Bitmap[38400];



// include libraries and macros
// TODO: this is currently devKitAdvance era manually copied header, move to libgba ASAP
#include "gba.h"
#include "screenmodes.h"
#include "sprites.h"
#include "keypad.h"


#include <maxmod.h>

// include sound fx data (these files generated in build folder as per Makefile from assets in maxmod_data/ )
#include "soundbank.h"
#include "soundbank_bin.h"


// define tile names (multiples of 4)
#define T_BLOCK                     0
#define T_RUBBLE                    4
#define T_NUKE2                     8    // undamaged two hit and dead
#define T_NUKE1                     12   // already damaged two hit and dead
#define T_NUKE0                     16   // one hit and dead
#define T_BOMB_LARGE                20
#define T_BOMB_SMALL                48
#define T_RUBBLE_EXPLO_START        52
#define T_RUBBLE_EXPLO_END          80
#define T_SPACE_EXPLO_CENTRE_START  84
#define T_SPACE_EXPLO_CENTRE_END    100
#define T_SPACE_EXPLO_HORIZ_START   104
#define T_SPACE_EXPLO_HORIZ_END     120
#define T_SPACE_EXPLO_VERT_START    124
#define T_SPACE_EXPLO_VERT_END      140
#define T_SPACE_EXPLO_LEFT_START    144
#define T_SPACE_EXPLO_LEFT_END      160
#define T_SPACE_EXPLO_RIGHT_START   164
#define T_SPACE_EXPLO_RIGHT_END     180
#define T_SPACE_EXPLO_UP_START      184
#define T_SPACE_EXPLO_UP_END        200
#define T_SPACE_EXPLO_DOWN_START    204
#define T_SPACE_EXPLO_DOWN_END      220
#define T_SPACE                     224
#define T_GIFTBOMB                  228
#define T_GIFTFLAME                 232
#define T_GIFTSURPRISE              236
// ??? idea - if 2 players have 2 sets of bomb sprites are used one for each player so that when a bomb detonates, the appropriate player's
// count of the number of bombs that they can drop is adjusted, otherwise have to store separate dataset for every tile

// define sprite names (multiples of 8)
#define S_G_MAN_RIGHT           0
#define S_G_MAN_LEFT            40
#define S_G_MAN_UP              80
#define S_G_MAN_DOWN            120
#define S_MAN_EXPLO_START       160
#define S_MAN_EXPLO_END         200
#define S_R_MAN_RIGHT           208
#define S_R_MAN_LEFT            248
#define S_R_MAN_UP              288
#define S_R_MAN_DOWN            328
#define S_ROBOT_RIGHT           368
#define S_ROBOT_LEFT            376
#define S_ROBOT_UP              384
#define S_ROBOT_DOWN            392
#define S_HALO                  400
#define S_LETTER_A              408
#define S_LETTER_Z              608
#define S_LETTER_FULLSTOP       616
#define S_NUMBER_0              624
#define S_NUMBER_9              696
#define S_APOSTROPHE            704
#define S_COMMA                 712
#define S_COLON                 720
#define S_FORWARDSLASH          728
#define S_TELETYPE              736
#define S_OPENBRACKET           744
#define S_CLOSEBRACKET          752
#define S_HIPHEN                760
#define S_QUESTIONMARK          768
#define S_EXCLAMATIONMARK       776
#define S_AMPERSAND             784

// number of colours used in various palettes so not unnecessarily fading in/out more colours than necessary
#define NUM_COLOURS_USED_IN_BACKGROUND_PALETTE 100
#define NUM_COLOURS_USED_IN_SPRITE_PALETTE 256

#define MAX_ROBOTS 10       // max number of robots allowed on any level

// define OAM position numbers for characters sprites
#define OAM_LETTERS             0
#define OAM_LASTLETTER          99
#define OAM_ROBOTS              100
#define OAM_GMAN                OAM_ROBOTS+MAX_ROBOTS
#define OAM_GHALO               OAM_ROBOTS+MAX_ROBOTS + 1
#define OAM_RMAN                OAM_ROBOTS+MAX_ROBOTS + 2
#define OAM_RHALO               OAM_ROBOTS+MAX_ROBOTS + 3
#define OAM_TELETYPE            127

// define movement direction values for robots (and possibly later the man too???)
#define MOVE_RIGHT              0
#define MOVE_LEFT               1
#define MOVE_UP                 2
#define MOVE_DOWN               3
#define MOVE_STILL              4

#define ALIVE 0
#define DYING 1
#define DEAD 2

#define AREA_X 19    // width in tiles of playing area
#define AREA_Y 13   // height in tiles

#define DISPLAY_WIDTH 240
#define DISPLAY_HEIGHT 160
#define NUM_LEVELS 10

u16 *pal=(u16*)0x5000000;           // background palette
u16 *tiles=(u16*)0x6004000;         // background tiles
u16 *m0=(u16*)0x6000000;            // background map
#define OAMData			((u16*)0x6010000)

// width in pixels of letters (measured at their widest), excluding the rightmost edge of single black pixels since they overlap
u8 charWidth[] = {
    11, 11, 11, 11, 11, 8, 11, 11, 5, 5,
    11, 11, 14, 11, 11, 11, 14, 11, 11, 6,
    11, 11, 14, 11, 11, 11, 6, 10, 6, 11,
    11, 11, 11, 11, 11, 11, 11, 5, 5, 4,
    5, 15, 7, 7, 9, 11, 4, 10
};
const int spaceWidth = 8;


u8 numRobots;               // num of robots currently alive in level
u8 totalRobots;             // total number of robots this level started with
bool robotsHalt;            // set to true if robots are frozen
u16 robotsHaltCount;        // univeral time when robots started being frozen

struct RobotData
{
    bool dead;
    u16 x;
    u16 y;
    u8 direction;
    bool move;
    u8 frame;           // which frame of animation is currently display when dying
    // number and offset needed?
};

struct RobotData robot[MAX_ROBOTS];

// robot movement seed (as used in Acorn version)
u32 robotMoveSeed = 'P' | ('A' << 8) | ('U' << 16) | ('L' << 24);
// mystery token seed (as used in Acorn version)
u32 mysteryTokenSeed = 'T' | ('C' << 8) | ('E' << 16) | ('L' << 24);

bool autoPlantBombs;        // set to true if man automatically drops bombs (from having picked up a token)
u16 autoPlantTimer;         // univeral time when man started auto dropping bombs

bool halo;
u16 haloTimer;

int rubbleCount;            // amount of rubble in the level

u16 universalTimer;
u16 timeOfDeath;

u8 area[AREA_X][AREA_Y];        // memory of what tiles are drawn where on board
u8 bombVal[AREA_X][AREA_Y];     // counters for each tiles bomb value so can increment more often than animation frame suggests

s16 manX;
s16 manY;
u16 xOffset;    // screen offsets to be fed to hardware regs
u16 yOffset;
u16 manDirectionX;
u16 manDirectionY;
u16 manSprite;
u16 manFrame;
u8 manMaxBombsAllowed;
u8 bombsCurrentlyDropped;
u8 lifeStatus;
u8 flameLength;
bool nuked;
int level;          // game level
int startLevel;     // which level game started on (in case different)
int lives;
int score;
bool playerHasContinued;

mm_sound_effect explo = {
    { SFX_EXPLO } ,			// id
    (int)(1.0f * (1<<10)),	// rate
    0,		// handle
    255,	// volume
    255,	// panning
};

mm_sound_effect token = {
    { SFX_TOKEN } ,			// id
    (int)(1.0f * (1<<10)),	// rate
    0,		// handle
    255,	// volume
    255,	// panning
};

#define IRQ_VBLANK		0x0001	//!< Catch VBlank irq // added from libtonc - must be in libgba somewhere

//the interrupt handle from crt0.s
//void InterruptProcess(void) __attribute__ ((section(".iwram")));


// interrupt related registers
#define REG_TM0CNT_L	*(u16*)0x4000100	//Timer 0 count value
#define REG_TM0CNT_H    *(u16*)0x4000102	//Timer 0 Control
#define REG_DMA1CNT     *(u32*)0x40000C4	//DMA1 Control (Amount)
#define REG_DMA1CNT_L   *(u16*)0x40000C4	//DMA1 Control Low Value
#define REG_DMA1CNT_H   *(u16*)0x40000C6	//DMA1 Control High Value
#define REG_SOUNDCNT_L *(volatile u16*)0x4000080		//DMG sound control
#define REG_SOUNDCNT_H *(volatile u16*)0x4000082		//Direct sound control
#define REG_SOUNDCNT_X *(volatile u16*)0x4000084	    //Extended sound control
#define REG_DMA1SAD     *(u32*)0x40000BC	//DMA1 Source Address
#define REG_DMA1DAD     *(u32*)0x40000C0	//DMA1 Desination Address
#define REG_TM1CNT_L   *(u16*)0x4000104		//Timer 2 count value
#define REG_TM1CNT_H   *(u16*)0x4000106		//Timer 2 control

#define REG_DMA2SAD     *(u32*)0x40000C8	//DMA2 Source Address
#define REG_DMA2DAD     *(u32*)0x40000CC	//DMA2 Destination Address
#define REG_DMA2CNT_H   *(u16*)0x40000D2	//DMA2 Control High Value
#define REG_TM2D       *(u16*)0x4000108		//Timer 3?
#define REG_TM2CNT     *(u16*)0x400010A		//Timer 3 Control
#define REG_TM3D       *(u16*)0x400010C		//Timer 4?
#define REG_TM3CNT     *(u16*)0x400010E		//Timer 4 Control





// prototype the various ANSI functions to prevent implicit declaration warnings later
int rand(void);
int strlen(const char*);
void srand(int);

/*
// to use simple interrupt processing for this then need to go to crt0.s and uncomment __FastInterrupts
// and recomment SingleInterrupts on line ~101
void InterruptProcess(void)
{
    u16 intFlags = REG_IE & REG_IF;
    
    if( intFlags & 0x10 )
    {
        // timer1 overflows - sample finished so stop Direct sound
    
        REG_TM0CNT_H=0;	//disable timer 0
    	REG_DMA1CNT_H=0; //stop DMA		
    	REG_IF |= REG_IF; //clear the interrupt(s)
    }
    else if( intFlags & 0x20)
    {
        
        REG_TM0CNT_H=0;	//disable timer 0
    	REG_DMA2CNT_H=0; //stop DMA		
    	//clear the interrupt(s)
    	REG_IF |= REG_IF;
    }
}

void playSound(const char* sample, u16 length, u16 channel)
{
    //Play a mono sound at 16khz in DMA mode Direct Sound
	//uses timer 0 as sampling rate source
	//uses timer 1 to count the samples played in order to stop the sound 
	REG_SOUNDCNT_L=0;
	REG_SOUNDCNT_H=0x0b0F;  //DS A&B + fifo reset + timer0 + max volume to L and R
	REG_SOUNDCNT_X=0x0080;  //turn sound chip on
	
    if(1 == channel)
    {
    	REG_DMA1SAD=(unsigned long)sample;	//dma1 source
    	REG_DMA1DAD=0x040000a0; //write to FIFO A address
    	REG_DMA1CNT_H=0xb600;	//dma control: DMA enabled+ start on FIFO+32bit+repeat
    	
    	// sample sizes
    	// rarg 14111
    	// explo 19472
    	// arg 20213
    	// token 16720
    	REG_TM1CNT_L=65535 - length;	    //0xffff-the number of samples to play
    	REG_TM1CNT_H=0xC4;		//enable timer1 + irq and cascade from timer 0
    
    	REG_IE=0x10;	  	    //enable irq for timer 1 overflow
    	REG_IME=1;				//enable interrupt
    	
    	//Formula for playback frequency is: 0xFFFF-round(cpuFreq/playbackFreq)
    	// ??? samples are 10146Hz
    	// timer count=65535-round(2^24/10146)=63882
    	REG_TM0CNT_L=65535 - (16777216/10146);	    //10146Hz playback freq
    	REG_TM0CNT_H=0x0080; 	//enable timer at CPU freq
    }
    else
    {
        // play sound on other channel
        
        // seriously need to consider adopting the interrupt and sound handling from Defender
        // a lot neater than this, issues with which timers to use and the value for DMA2DAD
    }
}


void playSoundTest(const char* sample, u16 length, u16 channel, const u16 freq)
{
    //Play a mono sound at 16khz in DMA mode Direct Sound
	//uses timer 0 as sampling rate source
	//uses timer 1 to count the samples played in order to stop the sound 
	REG_SOUNDCNT_L=0;
	REG_SOUNDCNT_H=0x0b0F;  //DS A&B + fifo reset + timer0 + max volume to L and R
	REG_SOUNDCNT_X=0x0080;  //turn sound chip on
	
    if(1 == channel)
    {
    	REG_DMA1SAD=(unsigned long)sample;	//dma1 source
    	REG_DMA1DAD=0x040000a0; //write to FIFO A address
    	REG_DMA1CNT_H=0xb600;	//dma control: DMA enabled+ start on FIFO+32bit+repeat
    	
    	// sample sizes
    	// rarg 14111
    	// explo 19472
    	// arg 20213
    	// token 16720
    	REG_TM1CNT_L=65535 - length;	    //0xffff-the number of samples to play
    	REG_TM1CNT_H=0xC4;		//enable timer1 + irq and cascade from timer 0
    
    	REG_IE=0x10;	  	    //enable irq for timer 1 overflow
    	REG_IME=1;				//enable interrupt
    	
    	//Formula for playback frequency is: 0xFFFF-round(cpuFreq/playbackFreq)
    	// ??? samples are 10146Hz
    	// timer count=65535-round(2^24/10146)=63882
    	REG_TM0CNT_L=65535 - (16777216/freq);	    //10146Hz playback freq
    	REG_TM0CNT_H=0x0080; 	//enable timer at CPU freq
    }
    else
    {
        // play sound on other channel
        
        // seriously need to consider adopting the interrupt and sound handling from Defender
        // a lot neater than this, issues with which timers to use and the value for DMA2DAD
    }
}


void DmaPlaySound (void)
{
	//Play a mono sound at 16khz in DMA mode Direct Sound
	//uses timer 0 as sampling rate source
	//uses timer 1 to count the samples played in order to stop the sound 
	REG_SOUNDCNT_L=0;
	REG_SOUNDCNT_H=0x0b0F;  //DS A&B + fifo reset + timer0 + max volume to L and R
	REG_SOUNDCNT_X=0x0080;  //turn sound chip on
	
	REG_DMA1SAD=(unsigned long)token;	//dma1 source
	REG_DMA1DAD=0x040000a0; //write to FIFO A address
	REG_DMA1CNT_H=0xb600;	//dma control: DMA enabled+ start on FIFO+32bit+repeat
	
	// sample sizes
	// rarg 14111
	// explo 19472
	// arg 20213
	// token 16720
	REG_TM1CNT_L=65535 - 16720;	    //0xffff-the number of samples to play
	REG_TM1CNT_H=0xC4;		//enable timer1 + irq and cascade from timer 0

	REG_IE=0x10;	  	    //enable irq for timer 1 overflow
	REG_IME=1;				//enable interrupt
	
	//Formula for playback frequency is: 0xFFFF-round(cpuFreq/playbackFreq)
	// ??? samples are 10146Hz
	// timer count=65535-round(2^24/10146)=63882
	REG_TM0CNT_L=65535 - (16777216/10146);	    //10146Hz playback freq
	REG_TM0CNT_H=0x0080; 	//enable timer at CPU freq 
}
*/

/*
// these functions seem to break the build if compiler optimisations are turned on
// thumb code for printing text to emulators debugger
void dbprintthumb(char *s)
{
 asm volatile("mov r0, %0;"
              "swi 0xff;"
              : // no ouput
              : "r" (s)
              : "r0");
}

// ARM code build for printing text to emulators debugger
void dbprint(char *s)
{
 asm volatile("mov r0, %0;"
              "swi 0xff0000;"
              : // no ouput
              : "r" (s)
              : "r0");
}
*/

// wait for scanline to be off screen (before drawing to it)
void wait()
{
	while( REG_VCOUNT < 160 );
}

// put in a delay a certain number of vsyncs, note ~60 vsyncs per second so delay(120) waits for about 2 secs
void delay(int numOfVSyncs)
{
    int i;
    for(i=0; i<numOfVSyncs; i++)
    {
        while( REG_VCOUNT != 160 );
        // wait until off 160 again otherwise immediately exits as the for loop will cycle away 
        // during the one HSync
        while( REG_VCOUNT == 160 );
    }
}

// delay for a certain number of VSyncs or until A/B/Start is pressed
void delayOrKeypress(int numOfVSyncs)
{
    
    int i;
    for(i=0; i<numOfVSyncs; i++)
    {
    	// re-seed randomizer as many times through loop as user permits before pressing
    	// a button, will timeout too though so can't purely rely on this hence the additional
    	// randomization on the wait until the user presses start on the start screen.
    	srand(rand());
    	
        while( REG_VCOUNT != 160 )
        {
            if( ((~KEYS) & 0x3FF) )
            {
                if( KEY_DOWN( KEYA ) || KEY_DOWN( KEYB ) || KEY_DOWN( KEYSTART ) )
                return;
            }
        }
        
        // wait to be off 160 before testing first loop result again
        while( REG_VCOUNT == 160 );
    }
}

// set GBA tile
void setTile(u16 x, u16 y, u16 tileNum)
{
    // adjust for using 512x256 display which the GBA treats as two 256x256 pixel (32x32 tile) blocks
    
    if(x < 32)
        m0[ (y*32) + x] = tileNum;
    else
        m0[ (x-32) + (y * 32) + (32*32)] = tileNum;
}

// draw logical object in game space
void drawObject(u16 x, u16 y, u16 object)
{
    // ignore top row and left column of GBA tiles and start plotting
    
    setTile( (x<<1), (y<<1), object);
    setTile( 1 + (x<<1), (y<<1), object + 1);
    setTile( (x<<1), 1 + (y<<1), object + 2);
    setTile( 1 + (x<<1), 1 + (y<<1), object + 3);

    area[x][y] = object;
}

// just copies sprite data to OAM as used by the game i.e. men, halos and monsters
void copyGameOAM(void)
{
        
	u16 loop;
	u16* temp = (u16*)sprites;
	u16* oamTemp = OAM;
	
	// ??? check this optimised properly, can we reduce the number of sprites copied
	const int spriteNumToCopyFrom = 100;
	const int spriteNumToCopyTo = 128;
	
	oamTemp += spriteNumToCopyFrom*4;
	temp += spriteNumToCopyFrom*4;
	
	// ??? optimise, we use a lot less sprites than 28
	// ??? since so few sprites ~14 (check this) could use thumb code and loop unroll!
	for(loop = spriteNumToCopyFrom*4; loop < spriteNumToCopyTo*4; loop++)
	{
		*oamTemp++ = *temp++;
	}
	
}

// copies all 128 sprites across to OAM, ideal for displaying lots of text on screen
void copyAllOAM(void)
{
	u16 loop;
	u16* temp = (u16*)sprites;
	u16* oamTemp = OAM;
	
	for(loop = 0; loop < 128*4; loop++)
	{
		*oamTemp++ = *temp++;
	}	    
}

// copy from the start to the end of the specified sprite numbers (inclusive) to OAM
void copySelectOAM(int start, int end)
{   
	u16 loop;
	u16* temp = (u16*)sprites;
	u16* oamTemp = OAM;
	
	for(loop = start*4; loop < (end+1)*4; loop++)
	{
		*oamTemp++ = *temp++;
	}
}

// copy a single sprite number across to OAM RAM
void copySingleOAM(int spriteNum)
{
    // 16 bit quantities
    u16* oamTemp = OAM + (spriteNum*4);
    u16* temp = ((u16*)sprites) + (spriteNum*4);
    
    // unrolled loop for single case
    *oamTemp++ = *temp++;
    *oamTemp++ = *temp++;
    *oamTemp++ = *temp++;
    *oamTemp++ = *temp++;
}

// initialise sprites for when cart starts up
void initSprites(void)
{
	// set all sprites to be off display

	u16 loop;
	for(loop = 0; loop < 128; loop++)
	{
	    sprites[loop].attribute0 = COLOR_256 | 256; //y to > 159
        sprites[loop].attribute1 = SIZE_16 | 256;   //x to > 239
	}
	
	copyAllOAM();
}

// sets up sprite attributes for one sprite, ready to be copied to OAM RAM at end of frame
void drawSprite(u16 spriteNumber, u16 charNumber, s16 x, s16 y)
{

    // to use mosaic set REG_MOSAIC and then logical-or (1<<12) on attribute 0 of the sprites to appear mosaic'd  
    //#define SetMosaic(bh,bv,oh,ov) ((bh)+(bv<<4)+(oh<<8)+(ov<<12))
    //REG_MOSAIC = SetMosaic(0,0,1,1);

    // antialiasing    
    // NOTE also the object attribute0 has to be | 0x400 
    // REG_BLDMOD = 0x248; // bit 4 - use OBJ as 1st target, bit 6 - alphablending, bit 9 - use background 1 as 2nd target
    // REG_COLEV = 0xc07;
    
    // just use normal sprite
    sprites[spriteNumber].attribute0 = COLOR_256 | y | SQUARE;
    sprites[spriteNumber].attribute1 = SIZE_16 | x;
    sprites[spriteNumber].attribute2 = charNumber;
}

// set single sprite to be off display (don't update OAM)
void turnOffSprite(u8 spriteNumber)
{
    sprites[spriteNumber].attribute0 = COLOR_256 | 256;
    sprites[spriteNumber].attribute1 = SIZE_16 | 256;
}

// set all sprites to be off display (but don't update OAM)
void turnOffAllSprites()
{
    int i;
    for(i=0; i<128; i++)
        turnOffSprite(i);
}

// set a range of sprites to be off display (don't update OAM)
void turnOffSprites(int firstSprite, int lastSprite)
{
    int i;
    for(i=firstSprite; i<lastSprite; i++)
        turnOffSprite(i);
}

void BrightnessInit(void)
{
	REG_BLDMOD = 0x290; // use this but make sure there's no colour 0 (currently black) in background!!!
    REG_COLEV = 0x10;
}

void BrightnessSetLevel(int brightnessLevel)
{
	REG_COLEY = brightnessLevel;
}

void BrightnessEnd(void)
{
	// ??? should really turn the brightness flags off but this looks just the same
	    
    // turn brightness down to normal again before fading out
    BrightnessSetLevel(0);
}

void BrightnessSetSpritesInactive(int firstSprite, int lastSprite)
{
        
	int sprCount;
	for(sprCount = firstSprite; sprCount <= lastSprite; sprCount++)
    {
        sprites[sprCount].attribute0 &= ~(0xC00);	// clear OBJ mode flag to ensure Normal OBJ mode
        sprites[sprCount].attribute0 |= 1 << 10;	// set to semi-transparent mode
    }
}

void BrightnessSetSpritesActive(int firstSprite, int lastSprite)
{
	// for brightness adjust, it appears the sprites have to be in semi-transparent mode

	int sprCount;
    for(sprCount = firstSprite; sprCount <= lastSprite; sprCount++)
    {
        sprites[sprCount].attribute0 &= ~(0xC00);	// clear OBJ mode flag to ensure Normal OBJ mode
    }
}

// get number of pixels width of a given string in the game font
int getTextWidth(const char* text)
{
    
    // would be better if charWidth was just an 256 byte jump table but too much effort when
    // it's constructed manually and works fine as it is
    
    s16 x = 0;
    char* textPtr = (char*)text;
    char letter;
    while( (letter = *textPtr++) )
    {
        if( ' ' == letter )
            x += spaceWidth;
        if( letter >= 'A' && letter <= 'Z' )
            x += charWidth[letter - 'A'];
        if( letter >= 'a' && letter <= 'z' )
            x += charWidth[letter - 'a'];
        if( letter >= '0' && letter <= '9' )
            x += charWidth[27 + (letter - '0')];
        
        switch(letter)
        {
            case '.'    : x += charWidth[26]; break;
            case '\''   : x += charWidth[37]; break;
            case ','    : x += charWidth[38]; break;
            case ':'    : x += charWidth[39]; break;
            case '/'    : x += charWidth[40]; break;
        }
    }
    
    return x;
}

// write text to screen immediately without scrolling it out
int writeTextImmediately(s16 x, u16 y, const char* text, u32* spriteNum, int numLetters)
{
    // need to build: a way to set colour
    // perhaps some scroll text like film credits for completion of game
    // a lot of this could be calculated pre-compile time for static messages but too much effort for no noticable gain
      
    // A-Z is 65...
    // a-z is 97...
    // 0-9 is 48...
    
    // if text should be centred on display
    if( -1 == x )
    {
        x = getTextWidth(text);
        // x now contains the width of the characters so calculate where to start to centre message
        x = DISPLAY_WIDTH - x;
        x >>= 1;
    }
    
    char* textPtr = (char*)text;
    char letter;
    int letterCount = 0;
    
    // for each letter in string
    while( (letter = *textPtr++) )
    {
        
        // if we only want to print a substring of the text msg
        if( numLetters != -1)
        {
            // if have printed all the letters requested then stop
            if( letterCount >= numLetters )
                return x;
            else
                letterCount++;
        }    
         
        // if space then just jump forward on screen position  
        if( ' ' == letter )
        {
            x += spaceWidth;
            continue;
        }
        
        if( letter >= 'A' && letter <= 'Z' )
        {
            drawSprite((*spriteNum)++, S_LETTER_A + ((letter - 'A')<<3), x, y);
            x += charWidth[letter - 'A'];
            continue;
        }
        
        if( letter >= 'a' && letter <= 'z' )
        {
            drawSprite((*spriteNum)++, S_LETTER_A + ((letter - 'a')<<3), x, y);
            x += charWidth[letter - 'a'];
            continue;
        }
        
        if( letter >= '0' && letter <= '9' )
        {
            drawSprite((*spriteNum)++, S_NUMBER_0 + ((letter - '0')<<3), x, y);
            x += charWidth[27 + (letter - '0')];
            continue;
        }
        
        switch(letter)
        {
            case '.'    : drawSprite((*spriteNum)++, S_LETTER_FULLSTOP, x, y); x += charWidth[26]; continue;
            case '\''   : drawSprite((*spriteNum)++, S_APOSTROPHE, x, y); x += charWidth[37]; continue;
            case ','    : drawSprite((*spriteNum)++, S_COMMA, x, y); x += charWidth[38]; continue;
            case ':'    : drawSprite((*spriteNum)++, S_COLON, x, y); x += charWidth[39]; continue;
            case '/'    : drawSprite((*spriteNum)++, S_FORWARDSLASH, x, y); x += charWidth[40]; continue;
            // charWidth[41] not used as that's the teletype symbol
            case '('    : drawSprite((*spriteNum)++, S_OPENBRACKET, x, y); x += charWidth[42]; continue;
            case ')'    : drawSprite((*spriteNum)++, S_CLOSEBRACKET, x, y); x += charWidth[43]; continue;
            case '-'    : drawSprite((*spriteNum)++, S_HIPHEN, x, y); x += charWidth[44]; continue;
            case '?'    : drawSprite((*spriteNum)++, S_QUESTIONMARK, x, y); x += charWidth[45]; continue;
            case '!'    : drawSprite((*spriteNum)++, S_EXCLAMATIONMARK, x, y); x += charWidth[46]; continue;
            case '&'    : drawSprite((*spriteNum)++, S_AMPERSAND, x, y); x += charWidth[47]; continue;
        }
        
        // should never get this far - we're trying to draw an unexpected character, plot monster instead to show issue
        drawSprite((*spriteNum)++, S_ROBOT_LEFT, x, y);
        x += 16;
    }
    
    // return final X value in case it's of interest for continuing text
    return x;
}

// write text out printed letter by letter with sound fx
int writeText(s16 x, u16 y, const char* text, u32* spriteNum)
{
    u32 tempSpriteNum = *spriteNum;
    int stringLength = strlen(text);
   
    // prep, ensure all the OAM data is copied across
    wait();
    copyAllOAM();
   
    // write the message, one more letter each time    
    int letCount;
    u32 newX;
    for(letCount = 1; letCount <= stringLength; letCount++)
    {
        *spriteNum = tempSpriteNum;
        newX = writeTextImmediately(x, y, text, spriteNum, letCount);
        
        // append the teletype
        drawSprite(OAM_TELETYPE, S_TELETYPE, newX, y);
        
        // full volume, enable sound channel 1 to left and right
        // (values determined using BeLogic's sound1demo approximately to Acorn sound)
        REG_SOUNDCNT_L=0x1177;
        REG_SOUND1CNT_L=0x0000;
        REG_SOUND1CNT_H=0x30C0;
        REG_SOUND1CNT_X=0x8790; // should start a fresh noise and give the break in the constant tone that I need
        
        wait();
        copyAllOAM();
    }
    
    // turn off teletype at end of string
    turnOffSprite(OAM_TELETYPE);
    copyAllOAM();
    
    // disable sound on channel 1
    REG_SOUNDCNT_L &= ~( (1<<8) | (1<<0xC) );
    
    // return width in case of interest
    return newX;
}

// wait until any key pressed until return
void waitForKeyPress()
{
    for( ; !((~KEYS) & 0x3FF) ; );
}

// simple fade of the 256 colour palette to black (i.e. doesn't fade proportional to brightness, which would need LUT)
void fadeToBlack()
{
    u16 palCount;
    for(palCount = 0; palCount<32; palCount++)
    {   
        u16 i;
        
        // tile palette
        for(i=0; i<256; i++)
        {
            u16 colour = pal[i];
            
            u8 green = (colour & 0x3E0) >> 5;
            u8 blue = (colour & 0x7C00) >> 10;
            u8 red = colour & 0x1F;
            
            if(green)
                green--;
            if(red)
                red--;
            if(blue)
                blue--;

            pal[i] = red | (green << 5) | (blue << 10);
        }

        // sprite palette
        for(i=0; i<256; i++)
        {
            u16 colour = OBJPaletteMem[i];
            
            u8 green = (colour & 0x3E0) >> 5;
            u8 blue = (colour & 0x7C00) >> 10;
            u8 red = colour & 0x1F;
            
            if(green)
                green--;
            if(red)
                red--;
            if(blue)
                blue--;

            OBJPaletteMem[i] = red | (green << 5) | (blue << 10);
        }
        
    }
}

// take current palette and desired palette and increments the actual palette one 'step' towards the desired palette
void fadeInColoursOneStep(volatile u16* actualPalette, const u16* desiredPalette, const int numColours)
{
    // ??? this is a bit slow, look at it!!!
    // would need to find some way of using LUTs with relatively little memory if ever doing this interactively
    
    // for each colour in the palette
    u16 i;
    for (i=0; i<numColours; i++)
    {
        // get current displayed colour components
        u16 currentColour = actualPalette[i];
        u8 currentGreen = (currentColour & 0x3E0) >> 5;
        u8 currentBlue = (currentColour & 0x7C00) >> 10;
        u8 currentRed = currentColour & 0x1F;
        
        // get the colour components we're fading up to
        u16 desiredColour = desiredPalette[i];
        u8 desiredGreen = (desiredColour & 0x3E0) >> 5;
        u8 desiredBlue = (desiredColour & 0x7C00) >> 10;
        u8 desiredRed = desiredColour & 0x1F;
        
        // calculate an increment value for each colour
        u8 greenFactor = (desiredGreen - currentGreen) / 8;
        u8 redFactor = (desiredRed - currentRed) / 8;
        u8 blueFactor = (desiredBlue - currentBlue) / 8; 
        
        // ensure always move 1 level closer to the target colour
        if(currentGreen < desiredGreen)
            currentGreen += greenFactor+1;
        if(currentBlue < desiredBlue)
            currentBlue += blueFactor+1;
        if(currentRed < desiredRed)
            currentRed += redFactor+1;
        
        // don't increment past the target colour
        if(currentGreen > desiredGreen)
            currentGreen = desiredGreen;
        if(currentRed > desiredRed)
            currentRed = desiredRed;
        if(currentBlue > desiredBlue)
            currentBlue = desiredBlue;
        
        // apply the new colour to the palette
        actualPalette[i] = currentRed | (currentGreen << 5) | (currentBlue << 10);
    }
}

// fade colours of one palette in from black to light
void fadeBitmapPaletteIn(const unsigned short* palette)
{
    u16 shadeCount;
    for(shadeCount = 0; shadeCount<16; shadeCount++)
    {
        wait();
        fadeInColoursOneStep(pal, palette, 256);
    }   
}

// fade tile and bitmap palettes in
void fadePaletteIn()
{
    // ??? universal fade in looks good relative to fade out as colours that are very strong
    // in one of the primary colours tend to show before others very brightly
    
    // initially set all palettes to black, is this a good idea since probably already at black???
    u16 i;
    for(i=0; i<256; i++)
    {
        pal[i] = 0;
        OBJPaletteMem[i] = 0;
    }
    
    u16 shadeCount;
    for(shadeCount = 0; shadeCount<16; shadeCount++)
    {
        wait();
        
        // do for both OAM and tile palettes
        u8 paletteCount;
        for(paletteCount = 0; paletteCount < 2; paletteCount++)
        {        
            
            if(paletteCount)
            {
                fadeInColoursOneStep(pal, background_Palette, NUM_COLOURS_USED_IN_BACKGROUND_PALETTE);
            }
            else
            {
                fadeInColoursOneStep(OBJPaletteMem, sprites_Palette, NUM_COLOURS_USED_IN_SPRITE_PALETTE);
            }
            
        }
    }   
}



void fadeOutSprites(int firstSprite, int lastSprite)
{
    
    ////////// turn ON alpha blending //////////
    
    // set alphablend targets
    REG_BLDMOD = 0x248; // bit 4 - use OBJ as 1st target, bit 6 - alphablending, bit 9 - use background 1 as 2nd target

    // turn alphablend on for certain sprites
    //REG_COLEV = 0xc07;
    
    int sprCount;
    for(sprCount = firstSprite; sprCount <= lastSprite; sprCount++)
    {
        sprites[sprCount].attribute0 |= 0x400; // enable antialiasing
    }
    
    copySelectOAM(firstSprite, lastSprite);
    
    ////////// vary alpha coefficients //////////
    
    // according to no$cash doc resulting intensity for each of R,G and B is
    // I = MIN ( 31, I1st*EVA + I2nd*EVB )
    // so cannot just vary one factor like in brightness, have to vary ratio
    // algorithm below was determined by experimentation
    
    // fiddle alpha coefficients
    int firstAlphaCoeff = 15;        // sprites
    int secondAlphaCoeff = 4;       // background
        
    for( ; secondAlphaCoeff <= 15; secondAlphaCoeff++)
    {
        REG_COLEV = (secondAlphaCoeff << 8) | firstAlphaCoeff;
        wait();
        copySelectOAM(firstSprite, lastSprite);
        delay(2);
    }
    
    for( ; firstAlphaCoeff >= 3; firstAlphaCoeff--)
    {
        REG_COLEV = (secondAlphaCoeff << 8) | firstAlphaCoeff;
        wait();
        copySelectOAM(firstSprite, lastSprite);
        delay(2);
    }
      
    ////////// turn OFF alpha blending //////////
    
    REG_COLEV = 0x10;
    
}
        

// display a large bitmap image (not used because of mode limitations on char RAM)
void displayBitmap(const unsigned char* bitmap, const unsigned short* palette)
{
    // displays a simple mode 4 256 colour bitmap but this severely limits the number of sprites on display
    // only those with character data 512 and up (true of modes 3,4,5) so not used anymore
    
    u16* theVideoBuffer = (u16*)VideoBuffer;
 
    //Cast a 16 bit pointer to our data so we can read/write 16 bits at a time easily
    u16* tempData = (u16*)bitmap;
    
    //Write the data
    //Note we're using 120 instead of 240 because we're writing 16 bits (2 pixels) at a time.
    u16 x;
    for(x=0; x<120*160; x++)
    {
        theVideoBuffer[x] = tempData[x];
    }
    
    SetMode(SCREENMODE4|BG2ENABLE); // | OBJENABLE | OBJMAP1D
    
    fadeBitmapPaletteIn(palette);
}

// display a fullscreen bitmap as a set of tiles
void displayTiledBitmap(const unsigned char* bitmap, const unsigned short* palette)
{
    // use bitmap made up of tiles so can do more sprites which isn't possible in pure bitmap background mode
    
    // enable display
    REG_BG1CNT = 0x4084; // ??? i've forgotten why I had this in the first place
    REG_BG1HOFS = 0;
    REG_BG1VOFS = 0;
    SetMode(SCREENMODE1 | BG1ENABLE | OBJENABLE | OBJMAP1D );
    
    // load tile data (16 bits at a time)
    u16* tileData = (u16*)bitmap;
    int i;
    for(i=0; i<19200; i++) tiles[i]=tileData[i];
    
    // position tiles (optimised version of loop below)
    int x,y;
    i=0;
    for(y=0; y<20; y++)
    {
        for(x=0; x<30; x++)
        {
            // inlined, optimised version of setTile( x, y, i++)
            m0[ (y*32) + x] = i++;
        }
    }
    
    fadeBitmapPaletteIn(palette);
}

// validate whether the player can enter a particular tile
bool isTileEnterable(s8 x, s8 y)
{
    // check tile isn't a blockage
    if(area[x][y] <= T_RUBBLE_EXPLO_END)
        return FALSE;
        
    // range check to ensure don't try and walk off the board
    if(x < 0 || y < 0 || x >= AREA_X || y >= AREA_Y)
        return FALSE;
        
    // got to here to tile must be ok to enter
    return TRUE;
}

// drop a bomb
void plantBomb(s8 x, s8 y)
{
    // put a bomb in area
    area[x][y] = T_BOMB_SMALL;
    
    // adjust count of number of bombs player is allowed to drop
    bombsCurrentlyDropped++;
    
    // plant a bomb at the specified tile number on display
    drawObject(x, y, T_BOMB_SMALL);

	// set bomb countdown to total    
    bombVal[x][y] = 32;
}


// handle when player has pressed pause button
void pauseActivated()
{
    // called from main game loop when paused state identified and stays in this function for duration of pause
    
    // dim the tile palette
    int i=0;
    int palCount;
    for(palCount = 0; palCount<2; palCount++)
    {
        volatile u16* currentPalette;

        if(0 == palCount)
        {
            currentPalette = pal;
        }
        else
        {
            currentPalette = OBJPaletteMem;
        }
        
        /*
        // Alternative fade out that just dims the colours instead of greying them
        const int fadeFactor = 4;
        for(i=0; i< NUM_COLOURS_USED_IN_BACKGROUND_PALETTE; i++)
        {
            u16 currentColour = currentPalette[i];
            u8 currentGreen = (currentColour & 0x3E0) >> 5;
            u8 currentBlue = (currentColour & 0x7C00) >> 10;
            u8 currentRed = currentColour & 0x1F;
            // don't fade any colour components if they would go -ve (of course in an unsigned var goes high +ve)
            if(currentGreen >= fadeFactor) currentGreen -= fadeFactor;
            if(currentBlue >= fadeFactor) currentBlue -= fadeFactor;
            if(currentRed >= fadeFactor) currentRed -= fadeFactor;
            currentPalette[i] = currentRed | (currentGreen << 5) | (currentBlue << 10);
        }
        */
        
        // greyscale the tile palette
        
        int i=0;
        for(i=0; i<256; i++)
        {
            // don't greyscale the green colour used in text font sprites
            if( 214 == i )
                continue;

            // no need for LUT since no action going on
            u16 currentColour = currentPalette[i];
            u8 green = (currentColour & 0x3E0) >> 5;
            u8 blue = (currentColour & 0x7C00) >> 10;
            u8 red = currentColour & 0x1F;
            u8 greyLevel = (green + blue + red) / 3;
            currentPalette[i] = greyLevel | (greyLevel << 5) | (greyLevel << 10);
        }
        
    } // end palette change
    
    // display message            
    u32 spriteNum = OAM_LETTERS;
    writeText(-1, 72, "PAUSED", &spriteNum);
    
    copyAllOAM();
    
    // wait for player to release the start button that they pressed to trigger the pause
    for( ; KEY_DOWN( KEYSTART) ; );
    
    // wait for player to press (and release) the start button again to unpause
    for( ; !KEY_DOWN( KEYSTART ) ; );
    for( ; KEY_DOWN( KEYSTART ) ; );
    
    // remove the pause banner
    int letCount;
    for(letCount = 0; letCount<6; letCount++)
        turnOffSprite(OAM_LETTERS + letCount);
    copyAllOAM();
    
    // restore palettes (use irrespective of whether grey scaled or dimmed colours)
    volatile u16* thisPal = pal;
    const u16* sourcePal = background_Palette;
    for(i=0; i<NUM_COLOURS_USED_IN_BACKGROUND_PALETTE; i++)
    {
        *thisPal++ = *sourcePal++;
    }
    thisPal = OBJPaletteMem;
    sourcePal = sprites_Palette;
    for(i=0; i<NUM_COLOURS_USED_IN_SPRITE_PALETTE; i++)
    {
        *thisPal++ = *sourcePal++;
    }
    
    // return to main game loop
}

// check for keypresses that affect gameplay
void checkInGameKeyPresses()
{
    // assuming man is still alive
        
    // if not currently moving
    if(!manDirectionX && !manDirectionY)
    {
        // convert pixels of man position to current tile number
        s8 manTileX = manX / 16;
        s8 manTileY = manY / 16;
    
    	// if player wants to drop a bomb or they automatically drop a bomb at the moment
        if( KEY_DOWN( KEYA ) || KEY_DOWN( KEYB ) || autoPlantBombs)
        {
        	// if the user can drop bombs on this tile then
            if( area[manTileX][manTileY] == T_SPACE)
            {
            	// drop bomb if we can
                if(bombsCurrentlyDropped < manMaxBombsAllowed)
                    plantBomb(manTileX, manTileY);
            }
        }
    
    	// check for player pressing certain directions
    	
        if( KEY_DOWN( KEYUP ) )
        {
            if(isTileEnterable(manTileX, manTileY - 1) )
            {
                manDirectionY = -1;
                manSprite = S_G_MAN_UP;
                return;
            }
        }
              
        if( KEY_DOWN( KEYDOWN ) )
        {
            if(isTileEnterable(manTileX, manTileY + 1) )
            {
                manDirectionY = 1;
                manSprite = S_G_MAN_DOWN;
                return;
            }
        }
            
        if( KEY_DOWN( KEYLEFT ) )
        {
            if(isTileEnterable(manTileX - 1, manTileY) )
            {
                manDirectionX = -1;
                manSprite = S_G_MAN_LEFT;
                return;
            }
        }
        
        if( KEY_DOWN( KEYRIGHT ) )
        {
            if(isTileEnterable(manTileX + 1, manTileY) )
            {
                manDirectionX = 1;
                manSprite = S_G_MAN_RIGHT;
                return;
            }
        }
    }
    
    if( KEY_DOWN( KEYSTART ) )
    {
        // stays in this function until unpaused
        pauseActivated();
    }

}

// nuclear reactor explosion
void nuke()
{

    // for every square on game area
    int x,y;
    for(y = 0; y<AREA_Y; y++)
    {
        for(x = 0; x<AREA_X; x++)
        {
        	// blow up rubble
            if(T_RUBBLE == area[x][y])
            {
                drawObject(x,y,T_RUBBLE_EXPLO_START);
            }
            else if(T_BLOCK != area[x][y])
            {
            	// fill in all spaces with explosion (basically only do odd numbered spaces)
                if(y & 1)
                {
                    // every other tile is space
                    drawObject(x,y,T_SPACE_EXPLO_VERT_START);
                }
                else
                {
                    // every tile is space
                    if(x & 1)
                        drawObject(x,y,T_SPACE_EXPLO_HORIZ_START);
                    else
                        drawObject(x,y,T_SPACE_EXPLO_CENTRE_START);
                }
            }
        }
    }
    
    nuked = TRUE; // set flag to slow explosions
}

// upgrade the position of the background scrolling relative to the player's position on the game board
inline void updateBackgroundOffset(void)
{
    // scroll background in X
    if(manX >= 7 * 16)
    {   
        xOffset = (manX - (7 * 16));
        if(xOffset >= 4 * 16) xOffset = 4 * 16;
    }
    else
        xOffset = 0;
    
    // scroll background in Y
    if(manY >= 5 * 16)
    {   
        yOffset = (manY - (5 * 16));
        if(yOffset >= 3 * 16) yOffset = 3 * 16;
    }
    else
        yOffset = 0;
}

// draw the man sprite at the current position
void drawManSprite()
{
    updateBackgroundOffset();
    
    drawSprite(OAM_GMAN, manSprite + (manFrame * 8), manX - xOffset, manY - yOffset);
    // draw halo if there is one
    if(halo)
        drawSprite(OAM_GHALO, S_HALO, manX - xOffset, manY - yOffset);
}

void moveMan()
{
    // ??? should probably extract collision detection do a different function and then only call moveMan if alive
    // so can check collisions less frequently than every frame if necessary
    
    if(lifeStatus == DYING)
    {
        // slow down death of man and robots if nuke has just occurred
        if( !(universalTimer % 6) )
        {
            // next frame of dying animation
            manFrame++;
            // if have exceeded dying animation then dead
            if(manFrame > 5 )
            {
            	// player now dead
                lifeStatus = DEAD;
                turnOffSprite(OAM_GMAN);
                timeOfDeath = universalTimer + 1000; // add 1000 for when the screen should blank out
            }
            else
            {
                // plot next frame of dying animation
                drawSprite(OAM_GMAN, S_MAN_EXPLO_START + (manFrame * 8), manX - xOffset, manY - yOffset);
            }
        }
        return;
    }
    
    // dead men don't move
    if(lifeStatus == DEAD)
        return;
    // else alive
        
    // check for collision with explosion
    
    u8 tile1 = area[manX / 16][manY / 16];
    u8 tile2 = area[(manX / 16) + (manX % 16 ? 1 : 0)][(manY / 16) + (manY % 16 ? 1 : 0)];
    if( (tile1 >= T_SPACE_EXPLO_CENTRE_START && tile1 <= T_SPACE_EXPLO_DOWN_END)
     || (tile2 >= T_SPACE_EXPLO_CENTRE_START && tile2 <= T_SPACE_EXPLO_DOWN_END) )
    {
        // if we've not got a halo or if we've just blown up a reactor
        if(!halo || nuked)
        {
            // start dying
            
            //SoundFX_Make(SOUNDFX_CHANNEL_B, SOUNDFX_ARG);
            
            lifeStatus = DYING;
            manFrame = 0;           // set to first frame of dying animation
            drawSprite(OAM_GMAN, S_MAN_EXPLO_START, manX - xOffset, manY - yOffset);
            turnOffSprite(OAM_GHALO);
            return;
        }
    }
    
    // check for collision with robots
    
    int i;
    for(i=0; i<totalRobots; i++)
    {
    	// note, important to still poll all robots for collision when the player has a halo even if none of the robots
    	// can hurt the player, this is so the game doesn't speed up when we have a halo
    	
        if(robot[i].dead == ALIVE)
        {
            
            // check collision and trigger man death if collided
            // ??? very simple collision algorithm so may want to look at better methods
            
            // if robot to the left of man by a whole tile
            if(robot[i].x <= manX - 16)
                continue;
            // if robot to the right of man by a whole tile
            if(robot[i].x >= manX + 16)
                continue;
            // if robot above man by a whole tile
            if(robot[i].y <= manY - 16)
                continue;
            // if robot below man by a whole tile
            if(robot[i].y >= manY + 16)
                continue;
            
            // robot is in same area as man so start dying
            if(!halo)
            {      
                lifeStatus = DYING;
                //SoundFX_Make(SOUNDFX_CHANNEL_B, SOUNDFX_ARG);
                manFrame = 0;           // set to first frame of dying animation
                drawSprite(OAM_GMAN, S_MAN_EXPLO_START, manX - xOffset, manY - yOffset);
                return;
            }
        }
    }
    
    // if player moving (i.e. inbetween squares)
    if( manDirectionX || manDirectionY)
    { 
    	if( !(universalTimer % 2) )
    	{
    		
	        // if moving horizontally 
	        if(manDirectionX)
	        {
	            // adjust position 1 pixel
	            manX += manDirectionX;
	            
	            // if have reached the boundary of a tile then stop moving
	            if( !(manX % 16) )
	            {
	                manDirectionX = 0;
	                manFrame = 0;       // set sprite back to standing still
	            }
	            else
	                manFrame = (manFrame + 1) % 5; // next animation frame
	        }
	        else
	        {
	            // adjust position 1 pixel
	            manY += manDirectionY;
	            
	            // if have reached the boundary of a tile then stop moving
	            if( !(manY % 16) )
	            {
	                manDirectionY = 0;
	                manFrame = 0;       // set sprite back to standing still
	            }
	            else
	                manFrame = (manFrame + 1) % 5; // adjust animation frame
	        }
	        
	        drawManSprite();
    	}
    }
}

// explode a bomb
void detonateBomb(u8 x, u8 y)
{
    
    // note, end tip of north part of explosion seems to be a frame behind the rest, is it set up wrong at start of
    // explosion or just decays improperly?
    
    //SoundFX_Make(SOUNDFX_CHANNEL_A, SOUNDFX_EXPLO);
    mmEffectEx(&explo);
    
    bombsCurrentlyDropped--;
    
    // centre piece
    area[x][y] = T_SPACE_EXPLO_CENTRE_START;
    drawObject(x, y, T_SPACE_EXPLO_CENTRE_START);
    
    int direction;
    // for each of the four directions, left, right, up, down
    for(direction = 0; direction<4; direction++)
    {
        u8 dx, dy, endtile, midtile;
        // set up for the appropriate direction the flame's moving in
        switch(direction)
        {
            case 0 : // left
                dx = -1;
                dy = 0;
                midtile = T_SPACE_EXPLO_HORIZ_START;
                endtile = T_SPACE_EXPLO_LEFT_START;
                break;
            case 1 : // right
                dx = 1;
                dy = 0;
                midtile = T_SPACE_EXPLO_HORIZ_START;
                endtile = T_SPACE_EXPLO_RIGHT_START;
                break;
            case 2 : // up
                dx = 0;
                dy = -1;
                midtile = T_SPACE_EXPLO_VERT_START;
                endtile = T_SPACE_EXPLO_UP_START;
                break;
            case 3 : // down
                dx = 0;
                dy = 1;
                midtile = T_SPACE_EXPLO_VERT_START;
                endtile = T_SPACE_EXPLO_DOWN_START;
                break;
        }
        
        // go out for as long as explosions are currently defined to be
        int length;
        s8 nx = x;
        s8 ny = y;
        for(length = 1; length<=flameLength; length++)
        {
        	// adjust to next tile
            nx += dx;
            ny += dy;
            
            // range check on play area
            if(nx >= 0 && nx < AREA_X && ny >=0 && ny < AREA_Y)
            {                
                // if it's a block then discontinue the flame in this direction
                if(area[nx][ny] == T_BLOCK)
                    break;
                                    
                // if it's rubble then explode that
                if(area[nx][ny] == T_RUBBLE )
                {
                    area[nx][ny] = T_RUBBLE_EXPLO_START;
                    drawObject(nx, ny, T_RUBBLE_EXPLO_START);
                    // but hit rubble so discontinue explosion
                    break;
                }
                
                // if explosion hits another bomb
                if(area[nx][ny] >= T_BOMB_LARGE && area[nx][ny] <= T_BOMB_SMALL)
                {
                    // tile scan is from top left to bottom right so by setting bombval to 1 then any bombs
                    // below or right get detonated this scan and any above or left get done in the next
                    // scan, we want all to get done in the next scan.
                    
                    // if below
                    if(ny > y)
                    {
                        bombVal[nx][ny] = 2;   
                    }
                    else
                    {
                        // else above
                        
                        // if to right
                        if(nx > x)
                            bombVal[nx][ny] = 2;
                        else
                            bombVal[nx][ny] = 1;
                    }
                    
                    // hit bomb so don't go any further in this direction
                    break; 
                }
                
                if(T_NUKE2 == area[nx][ny])
                {
                    // change to damaged nuke
                    drawObject(nx, ny, T_NUKE1);
                    break; // don't go any further in this direction
                }
                
                if(T_NUKE1 == area[nx][ny] || T_NUKE0 == area[nx][ny])
                {
                	// explode nuclear reactor
                    nuke();
                    return; // whole screen blowing so no need for this explosion
                }
                
                if(area[nx][ny] >= T_SPACE_EXPLO_CENTRE_START && area[nx][ny] <= T_SPACE_EXPLO_DOWN_END )
                {
                    // handle existing explosions in spaces
                    // ??? nothing to be done so can be removed?
                    break; // look as though on arc version it doesn't overwrite existing explosions, draw L shape of 3 bombs to demonstrate
                }
                else
                {
                    // else just: T_SPACE or gifts
                    
                    // check for end of flame
                    if(length == flameLength)
                    {
                    	// draw end of flame
                        area[nx][ny] = endtile;
                        drawObject(nx, ny, endtile);
                    }
                    else
                    {
                    	// draw midflame tile
                        area[nx][ny] = midtile;
                        drawObject(nx, ny, midtile);
                    }
                }
                
            } // end of range check
        } // end of length loop
    } // end of direction loop    
}

// check for rubble explosions that need to be animated
void checkExplodingRubble(u8 x, u8 y)
{
    // ??? rubble explosion and normal space explosion can be treated identically !?!?
    // were previously, test out if still can be
    
    // if nuking them slow down explosion by not incrementing animation as often
    if(nuked)
    {
        if(universalTimer % 8)
            return;
    }
    
    if(!(universalTimer % 2) )
    {
    
        // increment rubble explosion sequence
        area[x][y]+=4;
        
        if( area[x][y] > T_RUBBLE_EXPLO_END )
        {
            rubbleCount--;
            
            // randomly decide whether to drop a gift or not
            if( (rand() % 10) >= 8 )
            {
                // randomly choose gift
                switch(rand() % 3)
                {
                    case 0 : area[x][y] = T_GIFTBOMB; break;
                    case 1 : area[x][y] = T_GIFTFLAME; break;
                    case 2 : area[x][y] = T_GIFTSURPRISE; break;
                }
            }
            else
            {
                area[x][y] = T_SPACE;
            }
        }
        
        drawObject(x, y, area[x][y]);
    }
}

// check whether a bomb needs to be detonated
void checkBomb(u8 x, u8 y)
{
    if(!(universalTimer % 10) )
    {
        bombVal[x][y]--; // next stage
        
        area[x][y] = T_BOMB_LARGE + ((bombVal[x][y] / 4) * 4); // every 4th stage change tile
                                    
        // if decremented past largest bomb tile
        if(0 == bombVal[x][y])
        {
            detonateBomb(x,y);
        }
        else
        {
            // not detonated so just draw bomb
            drawObject(x, y, area[x][y]);
        }
    }
}

// check for incrementing explosions in spaces
void checkExplosion(u8 x, u8 y)
{
    
    if(nuked)
    {
        // nukes should blow slower than normal explosions for effect
        // ??? looking through code, not sure this will have the desired effect though
        if(universalTimer % 8)
            return;
    }
    
    if(!(universalTimer % 2) )
    {
        // if the tile is an explosion tile
        area[x][y]+=4;
        
        // if strayed into sequence for next orientation of explosion then explosion over
        if( !((area[x][y] - T_SPACE_EXPLO_CENTRE_START) % 20 ) )
            area[x][y] = T_SPACE;
        
        drawObject(x, y, area[x][y]);
    }
}

// calculate robot movements
void robotAI()
{
    // ??? all this needs a lot of work - directly translated from ARM code, highly inefficient
    // critical as the game slows down with more robots in it so need to ensure that robotai takes
    // constant time irrespective of the number of robots actually still in play
    
    u8 directionPattern[] = { 0,2,3,1,  1,2,3,0,  2,1,0,3,  3,1,0,2 };
    
    // ??? why does dropping a bomb directly on a robot cause it to stop moving?
    // probably not a problem in the real thing since player will be dead 
    
    int i;
    for(i=0; i<totalRobots; i++)
    {
        if(robot[i].dead == ALIVE)
        {
        	
			// if robot is placed squarely on a tile (i.e. time to decide where to move again)
            if( (!(robot[i].x % 16)) && (!(robot[i].y % 16)) )
            {
                
                robot[i].move = FALSE;
                
                // check bounds and blockages
                
                // get next 'random' number
                // no real AI, robot movement is completely random but this is the same as the Acorn version
                // and is still surprisingly good at tracking the player down in tight spots sometimes
                robotMoveSeed = (robotMoveSeed + (robotMoveSeed >> 1) );
                u8 element = (u8)robotMoveSeed;
                
                // choose new direction according to value of random result
                u8 newDir;
                if(element <= 128)
                    newDir = directionPattern[ robot[i].direction << 2 ];
                else if(element <= 176)
                    newDir = directionPattern[ (robot[i].direction << 2) + 1 ];
                else if(element <= 224)
                    newDir = directionPattern[ (robot[i].direction << 2) + 2 ];
                else
                    newDir = directionPattern[ (robot[i].direction << 2) + 3 ];
                                
                // ??? robot off ?
                // ??? why adjust robot x etc. is sprite misplaced otherwise?
                // ??? this obviously simplifies quite nicely
                if( ((robot[i].x / 16) > 0) && (T_SPACE == area[(robot[i].x / 16) - 1][(robot[i].y / 16)]) && (MOVE_LEFT == newDir) )
                {
                    robot[i].direction = newDir;
                    robot[i].move = TRUE;
                }
                
                if( ((robot[i].x / 16) < (AREA_X-1)) && (T_SPACE == area[(robot[i].x / 16) + 1][(robot[i].y / 16)]) && (MOVE_RIGHT == newDir) )
                {
                    robot[i].direction = newDir;
                    robot[i].move = TRUE;
                }
                
                if( ((robot[i].y / 16) > 0)  && (T_SPACE == area[(robot[i].x / 16)][(robot[i].y / 16) - 1]) && (MOVE_UP == newDir) )
                {
                    robot[i].direction = newDir;
                    robot[i].move = TRUE;
                }
                
                if( ((robot[i].y / 16) < (AREA_Y-1))  && (T_SPACE == area[(robot[i].x / 16)][(robot[i].y / 16) + 1]) && (MOVE_DOWN == newDir) )
                {
                    robot[i].direction = newDir;
                    robot[i].move = TRUE;
                }
                
            }
        }
    }
        
}

// move robots according to predetermined AI
void moveRobots()
{
	// ??? could be that it's this code slowing up the game when there are more robots as it's quite intense
	
    // every other frame update robots
    if( !(universalTimer % 2) )
    {
        int i;
        for(i=0; i<totalRobots; i++)
        {
            // ??? needs converting from animation for man from whence it was copied
            // ??? note, man and monster dying animations should happen at same frame rate as explosion animations
            
            if(robot[i].dead == DYING)
            {
                // do robot dead animation
                
                if( !(universalTimer % 6) )
                {
                    // next frame of dying animation
                    robot[i].frame++;
                    // if have exceeded dying animation then dead
                    if(robot[i].frame > 5 )
                    {
                        robot[i].dead = DEAD;
                        turnOffSprite(OAM_ROBOTS + i);
                    }
                    else
                    {
                        u8 y = robot[i].y - yOffset;
                        s16 x = robot[i].x - xOffset;
                        // if sprite logically half off display then adjust coords so that it appears that way
                        // yCoord adjusted automatically by being unsigned 8 bits, normally calc is 255 - y
                        if( x < 0 )
                            x = 512 + x;
                        // plot next frame of dying animation
                        drawSprite(OAM_ROBOTS + i, S_MAN_EXPLO_START + (robot[i].frame * 8), x, y);
                    }
                }
            }
            else if(robot[i].dead == ALIVE)
            {
                // if robot alive
            
                // check for robot collision with explosion
                // check tile robot is standing on or moving away from
                u8 tile1 = area[robot[i].x / 16][robot[i].y / 16];   
                u8 tile2 = area[(robot[i].x / 16) + (robot[i].x % 16 ? 1 : 0)][(robot[i].y / 16) + (robot[i].y % 16 ? 1 : 0)];
                if( (tile1 >= T_SPACE_EXPLO_CENTRE_START && tile1 <= T_SPACE_EXPLO_DOWN_END)
                    || (tile2 >= T_SPACE_EXPLO_CENTRE_START && tile2 <= T_SPACE_EXPLO_DOWN_END) )
                {
                    // start robot dying animation
                    robot[i].move = FALSE;
                    robot[i].dead = DYING;
                    //SoundFX_Make(SOUNDFX_CHANNEL_B, SOUNDFX_RARG);
                }
                                
                // if robot moving then adjust coords
                if(robot[i].move && !robotsHalt)
                {
                    
                    switch(robot[i].direction)
                    {
                        case MOVE_LEFT  : robot[i].x -= 1; break;
                        case MOVE_RIGHT : robot[i].x += 1; break;
                        case MOVE_UP    : robot[i].y -= 1; break;
                        case MOVE_DOWN  : robot[i].y += 1; break;
                    }
                }
            }
        } // end of for each robot
    }
    
    // every frame draw live robots at offset to screen scroll (dying robots handled above as they are animated)
    int i;
    for(i=0; i<totalRobots; i++)
    {
        if( robot[i].dead == ALIVE )
        {
            u8 y = robot[i].y - yOffset;
            s16 x = robot[i].x - xOffset;
            
            // if sprite logically half off display then adjust coords so that it appears that way
            // yCoord adjusted automatically by being unsigned 8 bits, normally calc is 255 - y
            if( x < 0 )
                x = 512 + x;

            // draw sprite
            // ??? or draw dying animation
            switch(robot[i].direction)
            {
                case MOVE_LEFT  : drawSprite(OAM_ROBOTS + i, S_ROBOT_LEFT, x, y); break;
                case MOVE_RIGHT : drawSprite(OAM_ROBOTS + i, S_ROBOT_RIGHT, x, y); break;
                case MOVE_UP    : drawSprite(OAM_ROBOTS + i, S_ROBOT_UP, x, y); break;
                case MOVE_DOWN  : drawSprite(OAM_ROBOTS + i, S_ROBOT_DOWN, x, y); break;
            }
        }
    }
}

// generate new playing area
void generateLevel(void)
{
    
    // algorithm ripped exactly from the Acorn version in order to get the same level generation algorithm
    
    int x,y;
    rubbleCount = 0;
    
    // draw rubble
    
    for(y=0; y<AREA_Y; y++)
    {
        for(x=0; x<AREA_X; x++)
        {
            // keep corners clear for men and then cover 2/3 of map with rubble
            // ??? check this <29 logic works for different size play areas in case we ever want to change area
            // ??? should this be rand(16) or rand(15) check how RND works on Acorn compared to C
            // should be <10 for rand part
            
            if( (x+y) > 1 &&
                (x+y) < ((AREA_X + AREA_Y) - 3) &&
                (rand() % 16) < 10 )
            {
                drawObject(x,y,T_RUBBLE);
                rubbleCount++;
            }
            else
            {
                // else place space
                drawObject(x,y,T_SPACE);
            }
        }
    }
    
    // place immovable blocks
    
    for(y=1; y<=AREA_Y-2; y+=2)
    {
        for(x=1; x<=AREA_X-2; x+=2)
        {
            if( T_RUBBLE == area[x][y] )
            {
                rubbleCount--;
            }
            drawObject(x,y,T_BLOCK);
        }
    }
    
    // generate the reactors
    
    // reactor data for each level
    // stored in 2D array of ReactorData structs for each of 10 levels (upto 8 reactors on last level)
    struct ReactorData
    {
        int type;
        int x;
        int y;
    }
    reactorLayout[10][9] = {
        { {0,-1,0} },
        { {0,-1,0} },
        { {0,-1,0} },
        { {1,9,6}, {0,-1,0} },
        { {2,9,6}, {0,-1,0} },
        { {1,6,6}, {1,12,6}, {0,-1,0} },
        { {1,0,6}, {1,18,6}, {2,9,6}, {0,-1,0} },
        { {1,4,6}, {1,14,6}, {2,9,8}, {2,9,4}, {0,-1,0} },
        { {1,2,2}, {1,16,2}, {1,2,10}, {1,16,10}, {2,0,6}, {2,18,6}, {0,-1,0} },
        { {1,2,2}, {1,4,6}, {1,2,10}, {1,16,2}, {1,16,10}, {1,14,6}, {2,9,8}, {2,9,4}, {0,-1,0} }
    };
    
    int reactorCount = 0;
    while( (reactorLayout[level][reactorCount]).x != -1 )
    {
        int reactX = (reactorLayout[level][reactorCount]).x;
        int reactY = (reactorLayout[level][reactorCount]).y;
        int reactType = (reactorLayout[level][reactorCount]).type;
        
        // if rubble removed for reactor placing then decrement rubble count
        if( T_RUBBLE == area[reactX][reactY] )
        {
            rubbleCount--;
        }
        
        // determine reactor type
        if( 2 == reactType )
        {
            drawObject(reactX,reactY,T_NUKE0);
        }
        else
        {
            drawObject(reactX,reactY,T_NUKE2);
        }
        
        reactorCount++;
    }
    
    // place robots
    
    int numRobotsOnLevel[10] = { 1,2,4,2,4,4,5,6,8,10 };
    
    // set global variable
    totalRobots = numRobotsOnLevel[level];
    
    int robotsPlaced = 0;
    do
    {
        x = rand() % AREA_X;
        y = rand() % AREA_Y;
        
        // ??? work out what's happening with this AREA_X+AREA_Y - 9 to get 23 with diff size play areas
        
        if( (x+y) > 5 && (x+y) < (AREA_X + AREA_Y) - 9 && x > 0 )
        {

            if( T_RUBBLE == area[x][y] )
            {
                rubbleCount--;
                drawObject(x,y,T_SPACE);
            }
            
            if( T_SPACE == area[x][y] )
            {
                robot[robotsPlaced].x = x * 16;
                robot[robotsPlaced].y = y * 16;
                robot[robotsPlaced].dead = ALIVE;
                robot[robotsPlaced].direction = 0; //??? all robots start out heading right
                robot[robotsPlaced].move = FALSE;
                robot[robotsPlaced].frame = 0;
                
                robotsPlaced++;
            }
        }
    }
    while( robotsPlaced < totalRobots );
    
    // turn off sprites for unused robots
    int i;
    for( i = totalRobots; i < MAX_ROBOTS ; i++)
    {
        robot[i].dead = DEAD;
    }
    
    numRobots = totalRobots;
}

// called at start of new level to generate level etc
void initialiseLevel(void)
{
    
    halo = TRUE;
    haloTimer = universalTimer;
    flameLength = 2;
    bombsCurrentlyDropped = 0;
    manMaxBombsAllowed = 1;
    robotsHalt = FALSE;
    robotsHaltCount = 0;
    autoPlantBombs = FALSE;
    autoPlantTimer = 0;
        
    // place man in bottom right
    
    manX = (AREA_X-1) * 16;
    manY = (AREA_Y-1) * 16;
    manDirectionX = 0;
    manDirectionY = 0;
    manSprite = S_G_MAN_LEFT;
    manFrame = 0;
    
    updateBackgroundOffset();
    
    u16 i;
    
    // load graphics into VRAM
    
    // turn off all 128 sprites available to the GBA
    initSprites();
    
   	// load background tile data
    // tile data appears to have to be loaded 16 bits at a time, why????
    u16* tileData = (u16*)background_Bitmap;
    //const unsigned char background_Bitmap
    for(i=0; i<32*256; i++) tiles[i]=tileData[i];
    
    // load sprite palette
    for(i=0; i<256; i++) OBJPaletteMem[i] = sprites_Palette[i];

    // load sprite tile data
    u16* sprTileData = (u16*)sprites_Bitmap;
    for(i=0; i<(26112/2); i++) OAMdata[i] = sprTileData[i];
    
    generateLevel();
    
    // set all tiles bomb counters to be 0
    int x,y;
    for(x=0; x<AREA_X; x++)
    {
        for(y=0; y<AREA_Y; y++)
        {
            bombVal[x][y] = 0;
        }
    }
    
    // draw man
    drawSprite(OAM_GMAN, manSprite + (manFrame * 8), manX - xOffset, manY - yOffset);
    // should always be a halo anyway at start of level
    if(halo)
        drawSprite(OAM_GHALO, S_HALO, manX - xOffset, manY - yOffset);
        
    // draw robot sprites
    for(i=0; i<totalRobots; i++)
    {
        u8 y = robot[i].y - yOffset;
        s16 x = robot[i].x - xOffset;
        
        // if sprite logically half off display then adjust coords so that it appears that way
        // yCoord adjusted automatically by being unsigned 8 bits, normally calc is 255 - y
        if( x < 0 )
            x = 512 + x;
        
        drawSprite(OAM_ROBOTS + i, S_ROBOT_RIGHT + robot[i].direction, x, y);
    }    
    
    // set background to be at correct position    
    REG_BG1HOFS = xOffset;
    REG_BG1VOFS = yOffset;
    
    // draw display 
    wait();
    copyGameOAM();
    REG_BG1CNT = 0x4084;
    SetMode(SCREENMODE1 | BG1ENABLE | OBJENABLE | OBJMAP1D );
    
    fadePaletteIn();
}

// ask player whether to continue the game once they've lost all 3 lives
bool shouldGameContinue(void)
{
	
	// assume already faded to black
	
	displayTiledBitmap(titlescreen_Bitmap, titlescreen_Palette);
        
    // load sprite palette
    int i;
    for(i=0; i<256; i++) OBJPaletteMem[i] = sprites_Palette[i];    
    
    u32 spriteNum = OAM_LETTERS;
    writeText(-1, 40, "CONTINUE?", &spriteNum);
    u32 firstSpriteOfYes = spriteNum;
    writeText(-1, 70, "YES", &spriteNum);
    u32 firstSpriteOfNo = spriteNum;
    writeText(-1, 90, "NO", &spriteNum);
    
    // cycle the brightness of selected letters
    
    int direction = 1;
    int fadeValue = 0;
    BrightnessInit();
    
    // initialise the YES option to be the fading one
    int selected = 0;
    
    BrightnessSetSpritesInactive(OAM_LETTERS, firstSpriteOfYes - 1);
    BrightnessSetSpritesActive(firstSpriteOfYes, firstSpriteOfNo - 1);
    BrightnessSetSpritesInactive(firstSpriteOfNo, spriteNum - 1);
    
    // while nothing selected
    while( !( KEY_DOWN(KEYA) || KEY_DOWN(KEYSTART) ) )
    {
        
        // adjust fade one level

        fadeValue += direction;
        if(fadeValue > 13)
        {
            direction = -1;
            fadeValue = 12;
        }
        else if(fadeValue < 0)
        {
            direction = 1;
            fadeValue = 1;
        }
            
        // flip state if key pressed
        
        if( KEY_DOWN(KEYDOWN) )
        {
            if( 0 == selected )
            {
                selected = 1;
                fadeValue = 0;
                
                BrightnessSetSpritesInactive(firstSpriteOfYes, firstSpriteOfNo - 1);
    			BrightnessSetSpritesActive(firstSpriteOfNo, spriteNum - 1);
                
            }
        }
        else if( KEY_DOWN(KEYUP) )
        {
            if( 1 == selected )
            {
                selected = 0;
                fadeValue = 0;
                
				BrightnessSetSpritesActive(firstSpriteOfYes, firstSpriteOfNo - 1);
    			BrightnessSetSpritesInactive(firstSpriteOfNo, spriteNum - 1);
            }
        }
        
        // delay and update display
        
        delay(2);
        BrightnessSetLevel(fadeValue);
		copySelectOAM(OAM_LETTERS, spriteNum);
    }
    
    BrightnessEnd();
	
	turnOffSprites(OAM_LETTERS, spriteNum);
    copyAllOAM();
	
	if( 0 == selected )
		return TRUE;
	else
		return FALSE;	
	
}

// start new game
void initialiseGame(void)
{
    
    // for every game
    universalTimer = 0;
    timeOfDeath = 0;
    nuked = FALSE;
    lifeStatus = ALIVE;
    lives = 2;
    playerHasContinued = FALSE;
    
    // level dependent
    // startlevel determined when selecting from main menu
    level = startLevel-1;

}


// display instructions when player requests them
void displayStory()
{    
    
    // define constants for special actions in message
    const char wait100 = 1;
    const char wait50 = 2;
    const char cls = 0;
    const char waitKeypress = 3;
    const char end = 4;
    const char newLine = 5;
    
    const char *const messages[] = {
        "ACCESSING...",
        &wait100,
        &cls,
        "DATE : 08/05/2007",
        &wait50,
        "WORK BEGINS ON FIVE",
        "STRUCTURES ON THE",
        "LUNAR SURFACE.  EACH",
        "WILL HAVE ITS OWN",
        "FUNCTION AND WILL",
        "RELY ON THE OTHER",
        "FOUR TO BE OPERATIONAL.", // COMPLETELY
        &waitKeypress, &cls,
        "DATE : 21/06/2014",
        &wait50,
        "LUNAR BASES BECOME",
        "FULLY OPERATIONAL.",
        "FUNCTIONS:",
        "1-LANDING/LAUNCHING,",
        "  INCOMING & OUTGOING",
        "2-LIVING QUARTERS",
        &waitKeypress, &cls,
        "3-LABS AND WORKSHOP",
        "4-ENERGY AND AIR GEN.",
        "5-STORAGE AND BASE",
        "  MAINTENANCE",
        &waitKeypress, &cls,
        "DATE : 27/01/2026",
        &wait50,
        "A LARGE METEOR HITS",
        "THE SURFACE OF THE",
        "MOON 38.2 KM FROM THE",
        "LUNAR BASE.",
        &waitKeypress, &cls,
        "BASE SECTION 4 RECEIVES",
        "STUCTURAL DAMAGE FROM",
        "SHOCKWAVES, FORCING",
        "IT OFFLINE...",
        &waitKeypress, &cls,
        "DATE : 28/01/2026",
        &wait50,
        "...PRESENT DAY...",
        &wait50,
        &newLine,
        "AS AN EXPERT IN ROCK",
        "BLASTING AND SALVAGE",
        "WORK, YOU HAVE BEEN",
        "SUMMONED TO THE MOON.",
        &waitKeypress, &cls,
        "BASE 4 CONTROLLER :",
        "TO MAKE IT EASIER TO",
        "MAKE THE BASE AIR-",
        "TIGHT, MOST OF IT WAS",
        "BUILT UNDERGROUND, IN",
        "ARTIFICIAL CAVERNS.",
        &waitKeypress, &cls,
        "THE MOONQUAKE CAUSED",
        "SEVERE ROCKFALLS ON",
        "ALL TEN PROCESSING",
        "LEVELS.  WE ARE USING",
        "EMERGENCY AIR AND",
        "BACKUP GENERATORS.",
        &waitKeypress, &cls,
        "YOU MUST CLEAR THESE",
        "ROCKFALLS IN ORDER",
        "FOR US TO RETURN THE",
        "BASE TO FULL POWER.",
        &waitKeypress, &cls,
        "EACH LEVEL HAS 54 AIR",
        //"WHICH ARE",
        //"ACTUALLY LINKED TO",
        //"THE OTHER LEVELS.",
        "PURIFIERS.",
        "THESE ARE MADE FROM A",
        "TITO-METACRYSTALLINE",
        "STRUCTURE AND ARE",
        "RESISTANT TO DAMAGE.",
        &waitKeypress, &cls,
        "HOWEVER, YOU WILL",
        "ENCOUNTER TWO TYPES",
        "OF NUCLEAR REACTORS.",
        &newLine,
        "THE RED AND BLUE TYPE",
        "(A MK 2) CAN SURVIVE",
        "ONE DIRECT BLAST.",
        &waitKeypress, &cls,
        "THE YELLOW AND GREEN",
        "(DB/34) MUST NOT BE",
        "BE HIT AT ALL.",
        &newLine,
        "ONE HIT TOO MANY WILL",
        "CAUSE A LARGE ATOMIC",
        "EXPLOSION!",
        &waitKeypress, &cls,
        "OH, BY THE WAY,",
        "THE IMPACT OF THE",
        "METEOR CREATED AN", // A HIGH
        "ELECTROMAGNETIC PULSE",
        "DISABLING THE CONTROL",
        "CIRCUITS IN THE",
        "SECURITY DROIDS THAT",
        "PATROL THE LEVELS.",
        &waitKeypress, &cls,
        "THEY ARE ROAMING OUT",
        "OF CONTROL AND WILL",
        "KILL IF YOU COME INTO",
        "CONTACT WITH THEM.",
        &newLine,
        "YOU MAY DESTROY THEM",
        "IF YOU WISH, THEY ARE",
        "INSURED.",
        &waitKeypress, &cls,
        "EQUIPMENT:",
        "EXPLOSIVE GENERATING",
        "BACKPACK, SUPPLIED",
        "WITH ENERGY CELL FOR",
        "ONE LOW POWER BOMB",
        "PER POWER RECHARGE.",
        &waitKeypress, &cls,
        "HIGHER POWER AND MORE",
        "BOMBS CAN BE GAINED",
        "BY COLLECTING",
        "APPROPRIATELY MARKED",
        "EXTRA CELLS THAT YOU",
        "MAY FIND.",
        &waitKeypress, &cls,
        "THERE ARE OTHER",
        "VARIOUS UTILITY CELLS",
        "YOU CAN USE, THOUGH",
        "IT WILL BE IMPOSSIBLE",
        "TO IDENTIFY THEIR",
        "PURPOSE BEFORE USE.",
        &waitKeypress, &cls,
        "CONTROLS :",
        &newLine,
        "USE THE CONTROL PAD",
        "TO MOVE AROUND.",
        &newLine,
        "USE BUTTON A OR B",
        "TO DROP BOMBS.",
        &newLine,
        "PRESS START TO PAUSE.",
        
        //&waitKeypress, &cls,
        //"2 PLAYER GAME :",
        //&newLine,
        //"BLOW THE ER, LIVING",
        //"DAYLIGHTS OUT OF THE",
        //"OTHER PLAYER.",
        //&newLine,
        //"SIMPLE, INNIT?",
        
        &waitKeypress, &cls,
        "DATA RETRIEVAL",
        "COMPLETE...",
        &waitKeypress,
        &end    
    };
    
    int i; // loop counter
    int msgCount = 0;
    int row = 0;
    u32 spriteNum = OAM_LETTERS;
    
    // while more messages to come
    while(messages[msgCount][0] != end)
    {
        // check if the user has had enough story for one sitting
        if( KEY_DOWN( KEYSTART ) )
        {
	        fadeToBlack();
	        turnOffAllSprites();
	        copyAllOAM();
	        return;
        }
                
        switch( messages[msgCount][0] )
        {
            case 0 :
                // clear screen
                for(i=0; i<=127; i++)
                    turnOffSprite(i);
                wait();
                copyAllOAM();
                spriteNum = OAM_LETTERS;
                row = 0;
                break;
            case 1 : 
                // wait 100
                delayOrKeypress(120);
                break;
            case 2 :
                // wait 50
                delayOrKeypress(60);
                break;
            case 3 :
                // wait for key a to be pressed or start
                for( ; !KEY_DOWN( KEYA ) && !KEY_DOWN(KEYSTART); );
                for( ; KEY_DOWN( KEYA ) && !KEY_DOWN(KEYSTART); );
                // if start pressed then leave
                if( KEY_DOWN(KEYSTART) )
                {
                    fadeToBlack();
                    turnOffAllSprites();
                    copyAllOAM();
                    return;
                }
                break;
            case 5 :
                row += 16;
                break;
            default:
                // write message to display
                writeText(0, row, messages[msgCount], &spriteNum);
                row += 16;
                break;
        }
        msgCount++;
    }
    
    // message over so fade out and return
    fadeToBlack();
    turnOffAllSprites();
    copyAllOAM();
}

// handles player losing a life in gameloop
// returns TRUE if game should leave the gameloop() function (i.e. properly dead not just lost life)
int handleDeath(void)
{
	lives--;
            
    if( lives >= 0 && !nuked)
    {
        // on arc version the game freezes up once death sequence over and the number of lives remaining
        // is shown on screen then after X seconds the text vanishes and the game continues exactly as before
        // with the user reincarnated on the spot he died on with the standard duration halo, all bombs etc
        // from the last go remain on the game area
        
        // show banner
        
        // compose number of lives string
        char livesMessage[] = "LIFE X";
        // says life 2, then life 3, then you're dead
        livesMessage[5] = '0' + (3 - lives);
        u32 spriteNum = OAM_LETTERS;
        writeText(-1, 60, livesMessage, &spriteNum);
        writeText(-1, 80, "GET READY", &spriteNum);
                            
        delayOrKeypress(120);
        
        // remove lives banner etc and return to game loop
        int letCount;
        for(letCount = 0; letCount<20; letCount++)
            turnOffSprite(OAM_LETTERS + letCount);
        copyAllOAM();
        
        // disable any special features
        robotsHalt = FALSE;
        robotsHaltCount = 0;
        autoPlantBombs = FALSE;
        autoPlantTimer = 0;

        lifeStatus = ALIVE;

        halo = TRUE;
        haloTimer = universalTimer;
        manFrame = 0;
        
        // draw man
        drawSprite(OAM_GMAN, manSprite + (manFrame * 8), manX - xOffset, manY - yOffset);
        if(halo)
            drawSprite(OAM_GHALO, S_HALO, manX - xOffset, manY - yOffset);    
        
    }
    else
    {
        
        if( nuked )
        {
            u32 spriteNum = OAM_LETTERS;
            writeText(-1, 60, "REACTOR EXPLOSION", &spriteNum);
            writeText(-1, 80, "MISSION ABORTED", &spriteNum);
            
            lives = -1;
        }
        else
        {
            fadeToBlack();
            
            turnOffAllSprites();
            
            // load sprite palette
            int i;
            for(i=0; i<256; i++)
            	OBJPaletteMem[i] = sprites_Palette[i];
            
            u32 spriteNum = OAM_LETTERS;
            writeText(0, 0, "YOU'RE DEAD.", &spriteNum);
            writeText(0, 16, "YOU COULDN'T EVEN", &spriteNum);
            writeText(0, 32, "MANAGE WITH 3 LIVES.", &spriteNum);
            writeText(0, 48, "I'M GIVING THE CONTRACT", &spriteNum);
            writeText(0, 64, "TO SOMEBODY ELSE...", &spriteNum);
            writeText(0, 88, "LUNAR BASE 4 CONTROLLER", &spriteNum);
        }
        
        copyAllOAM();
        
        waitForKeyPress();
        
        // game over
        fadeToBlack();
        
        // remove banner etc now faded to black
        turnOffAllSprites();
        copyAllOAM();
        
        // give player option to continue
        
        if( shouldGameContinue() )
        {
        	// remember that player has continued (for game completion message)
        	playerHasContinued = TRUE;
        	
        	fadeToBlack();
        	
        	// set up game for user to continue playing from start of this level
        	initialiseLevel();
        	
        	// re-initialise some values to continue play
        	nuked = FALSE;
		    lifeStatus = ALIVE;
		    lives = 2;
    	}
    	else
    	{
    		// leave gameloop function
        	return TRUE;
        }
    }
    
    return FALSE;
}

// main game loop
void gameLoop(void)
{
    // infinite loop
    for( ; ; )
    {
        
        // check for all rubble gone
        if( rubbleCount <= 0 )
        {
            // level completed
            fadeToBlack();
            turnOffAllSprites();
            return;
        }
        
        // if dead then leave game loop (if user doesn't continue)
        if(lifeStatus == DEAD)
        {
            if( handleDeath() )
            	return;   
        }
        
        // increment universal timer variable used to synchronise various game functions
        // ??? should probably use a system timer instead, 2 timers at 16k KHz will loop every 72 hours - acceptable
        universalTimer++;
        
        int x,y;
                
        // only do these checks every nth cycle through the loop
        if( !(universalTimer % 5) )
        {
            
            // tile scan whole area
        
            // check for bombs to detonate
            // scanned before other animations to prevent some bombs and explosions being plotted before others
            // as detonated before the main scan
            for(y = 0; y < AREA_Y; y++)
                for(x = 0; x < AREA_X; x++)
                    if(area[x][y] >= T_BOMB_LARGE && area[x][y] <= T_BOMB_SMALL)
                        checkBomb(x,y);
        
            for(y = 0; y < AREA_Y; y++)
            {
                for(x = 0; x < AREA_X; x++)
                {
                    
                    u8 tile = area[x][y];
                    
                    // check explosion
                    if(tile >= T_SPACE_EXPLO_CENTRE_START && tile <= T_SPACE_EXPLO_DOWN_END)
                    {
                        checkExplosion(x,y);
                    }
                    else
                    {
                        // check exploding rubble
                        if(tile >= T_RUBBLE_EXPLO_START && tile <= T_RUBBLE_EXPLO_END)
                        {
                            checkExplodingRubble(x,y);
                        }
                    }                    

                }
            }
            
            // check gift time outs i.e. have they finished yet
            if(robotsHalt)
            {
                if(universalTimer - robotsHaltCount > 2000 )
                    robotsHalt = FALSE;
            }
            
            if(autoPlantBombs)
                if(universalTimer - autoPlantTimer > 2000 )
                    autoPlantBombs = FALSE;
            
            if(halo)
            {
                if(universalTimer - haloTimer > 700 )
                {
                    halo = FALSE;
                    turnOffSprite(OAM_GHALO);
                }
            }
            
        }
           
        // if robots are frozen then don't let them change direction
        if(!robotsHalt)
            robotAI();
        
        // check for keypresses
        checkInGameKeyPresses();
        
        // check for gifts colliding with man
        // ??? this is probably being checked too often, should we do it with other collision checks?
        // (note need only check whether man is standing completely on tile not half on it,
        // as is done with explosion collisions)
        if(area[manX / 16][manY / 16] >= T_GIFTBOMB)
        {
            // check man is exactly on tile before triggering gift (otherwise gift appears to disappear before man on square completely)
            // ??? this may be too precise, could disregard the bottom two bits? to give +/- 3 pixels
            if( !(manX & 15) && !(manY & 15) )
            {
                u8 giftType = area[manX / 16][manY / 16];
                // blank tile now we've got the gift
                drawObject(manX / 16, manY / 16, T_SPACE);
                
                //SoundFX_Make(SOUNDFX_CHANNEL_B, SOUNDFX_TOKEN);
                
                switch( giftType )
                {
                    case T_GIFTBOMB     : manMaxBombsAllowed++; break;
                    
                    case T_GIFTFLAME    : flameLength++; break;
                    
                    case T_GIFTSURPRISE :
                        // choose surprise gift (in same way Acorn version did)
                        mysteryTokenSeed += (mysteryTokenSeed >> 1);
                        // decode chosen surprise gift
                        if( (mysteryTokenSeed & 255) < 73 )
                        {
                            // explode all bombs
                            
                            for(y = 0; y < AREA_Y; y++)
                               {
                                for(x = 0; x < AREA_X; x++)
                                {
                                    if(area[x][y] >= T_BOMB_LARGE && area[x][y] <= T_BOMB_SMALL)
                                    {
                                        bombVal[x][y] = 0; // reset bomb countdown timer to detonate
                                        detonateBomb(x,y);
                                    }
                                }
                            }
                        }
                        else if( (mysteryTokenSeed & 255) < 146 )
                        {
                            // robot halt
                            robotsHalt = TRUE;
                            robotsHaltCount = universalTimer;
                        }
                        else if( (mysteryTokenSeed & 255) < 182 )
                        {
                            // auto drop bombs
                            autoPlantBombs = TRUE;
                            autoPlantTimer = universalTimer;
                        }
                        else
                        {
                            // aura (halo)
                            halo = TRUE;
                            haloTimer = universalTimer;
                            drawSprite(OAM_GHALO, S_HALO, manX - xOffset, manY - yOffset);
                        }
                        break; // end gift-type switch
                }
            }            
        }
        
        // increment man movement if any and increment animation frame (test for collision with robots)
        moveMan();
        
        // move robots after man so that offset vars have been updated
        moveRobots();
        
        // update display
        
        wait();
        // update background scrolling positions
        REG_BG1HOFS = xOffset;
        REG_BG1VOFS = yOffset;
        // copy main game sprites positions (man, halo, monsters) to the screen
        copyGameOAM();

        // maxmod soundfx update
        mmFrame();
        
    } // loop forever      
}




void displayText()
{
	
    initSprites();
    
    // load sprite palette
    int i;
    for(i=0; i<256; i++) OBJPaletteMem[i] = sprites_Palette[i];

    // load sprite tile data
    u16* sprTileData = (u16*)sprites_Bitmap;
    for(i=0; i<(26112/2); i++) OAMdata[i] = sprTileData[i];
    
    // tile palette black
    for(i=0; i<256; i++)
    {
        pal[i] = 0 | (0 << 5) | (0 << 10);
    }
    
    SetMode(SCREENMODE1 | BG1ENABLE | OBJENABLE | OBJMAP1D );
    
    /*
    // display pretty much all the text
    u32 spriteNum = 0;
    writeText(10,30, "ABCDEFGHIJKLMNOPQRS", &spriteNum);
    writeText(10,50, "TUVWXYZ1234567890", &spriteNum);
    writeText(10,70, ":,./", &spriteNum);
    */
    
    
    u32 spriteNum = 0;
    writeText(-1, 10, "DEVELOPMENT BUILD", &spriteNum);
    writeText(-1, 30, "NOT FOR PUBLIC RELEASE", &spriteNum);
    writeText(0, 60, "COMPILED:", &spriteNum);
    writeText(0, 76, __TIME__ " " __DATE__, &spriteNum);
    
    
    waitForKeyPress();
    
    // ??? experiment, playing around trying to save values to battery RAM, why doesn't it work?
    *SRAM = 1;
    (SRAM[1]) = 2;
    
    fadeToBlack();
    turnOffAllSprites();
    copyAllOAM();

}



void startGameAndManageContinues()
{
    
    rubbleCount = 0;
    initialiseGame();
    
    int i;
    u32 spriteNum = OAM_LETTERS;
    
    do
    {
        
        // if level cleared (might want to have this as the initial state and do initial level generation here too
        if( !rubbleCount )
        {
            // increment level, first time this goes from -1 to 0 as desired
            level++;
            
            if( level >= NUM_LEVELS )
            {
                // completed the game
                
                
                // load sprite palette
                for(i=0; i<256; i++)
                	OBJPaletteMem[i] = sprites_Palette[i];
                
                if( playerHasContinued )
                {
                	// display against black background
                	
                	u32 spriteNum = OAM_LETTERS;
				    writeText(-1, 20, "YOU MAY HAVE FINISHED", &spriteNum);
				    writeText(-1, 40, "ALL THE LEVELS, BUT...", &spriteNum);
				    writeText(-1, 90, "CAN YOU DO IT", &spriteNum);
					writeText(-1, 110, "WITHOUT USING", &spriteNum);
				    writeText(-1, 130, "ANY CONTINUES?", &spriteNum);
            	}
            	else if(startLevel > 0)
            	{
            	    // display against black background
                	
                	u32 spriteNum = OAM_LETTERS;
				    writeText(-1, 20, "YOU MAY HAVE FINISHED", &spriteNum);
				    writeText(-1, 40, "THE LAST FEW LEVELS", &spriteNum);
				    writeText(-1, 90, "BUT HOW ABOUT", &spriteNum);
					writeText(-1, 110, "TRYING IT FROM", &spriteNum);
				    writeText(-1, 130, "THE START?", &spriteNum);
            	}
            	else
            	{
            		// credits screen taken from http://www.spacedaily.com/news/nuclear-blackmarket-02c.html
                	displayTiledBitmap(credits_Bitmap, credits_Palette);
            		
                    u32 spriteNum = OAM_LETTERS;
                    writeText(-1, 30, "CONGRATULATIONS!", &spriteNum);
                    writeText(-1, 70, "YOU HAVE MANAGED", &spriteNum);
                    writeText(-1, 90, "TO CLEAR ALL", &spriteNum);
                    writeText(-1, 110, "TEN LEVELS!", &spriteNum);
                }
                
                delayOrKeypress(5*60);
                
                fadeToBlack();
                
                // out of gameplay loop and back to master loop
                break;

            }
            else
            {
                // next level
            
                // display level message
                
                // load sprite palette
                for(i=0; i<256; i++) OBJPaletteMem[i] = sprites_Palette[i];

                if( level == startLevel )
                {
                    // starting message
                    char levelMessage[] = "LEVEL X";
                    levelMessage[6] = '0' + level;
                    u32 spriteNum = OAM_LETTERS;
                    writeText(-1, 40, levelMessage, &spriteNum);
                    writeText(-1, 80, "GET READY...", &spriteNum);
                }
                else
                {
                    char levelMessage[] = "NEXT : LEVEL X";
                    levelMessage[13] = '0' + level;
                    u32 spriteNum = OAM_LETTERS;
                    writeText(-1, 40, "LEVEL COMPLETE", &spriteNum);
                    writeText(-1, 60, levelMessage, &spriteNum);
                }
                
                delayOrKeypress(120);
                
                fadeToBlack();
                
                turnOffSprites(OAM_LETTERS, spriteNum);
                copyAllOAM();
                
                initialiseLevel();
            }
        }
        
        
        // enter main game loop
        gameLoop();
        
        
    }
    while( lives >= 0 );
    
}

void onVBlank() {
  mmVBlank();
}

int main(void)
{
    
    
    // debug build text, comment this out in release builds
    //displayText();
        
    // only display credits when first run
    
    // turn all sprites off before displaying a mode where they're enabled
    initSprites();
    
    // load sprite tile data
    u16* sprTileData = (u16*)sprites_Bitmap;
    int i;
    for(i=0; i<(26112/2); i++) OAMdata[i] = sprTileData[i];
    
    // credits screen image taken from http://www.spacedaily.com/news/nuclear-blackmarket-02c.html
    // also available at http://www.staticfiends.com/galleries/government_galleries/0014.jpg
    displayTiledBitmap(credits_Bitmap, credits_Palette);

    // init sound fx
    irqInit();

	// Maxmod requires the vblank interrupt to reset sound DMA.
	// Link the VBlank interrupt to mmVBlank, and enable it. 
	irqSet( IRQ_VBLANK, onVBlank );

    irqEnable(IRQ_VBLANK);

    // initialise maxmod with soundbank and 8 channels
    mmInitDefault( (mm_addr)soundbank_bin, 8 );


    // load sprite palette
    for(i=0; i<256; i++) OBJPaletteMem[i] = sprites_Palette[i];

    // display 1st message        
    u32 spriteNum = OAM_LETTERS;
    writeText(-1, 10, "CONVERSION:", &spriteNum);
    writeText(-1, 30, "DAVID SHARP", &spriteNum);
    writeText(-1, 50, "ORIGINAL AND GRAPHICS:", &spriteNum);
    writeText(-1, 70, "PAUL TAYLOR", &spriteNum);
    writeText(-1, 120, "WWW.DAVIDSHARP.COM/GBA", &spriteNum);
    
    copyAllOAM();
        
    delayOrKeypress(300);
    
    // display 2nd message

    fadeOutSprites(OAM_LETTERS, spriteNum-1);
    
    turnOffAllSprites();
    
    // display 1st message        
    spriteNum = OAM_LETTERS;
    writeText(-1, 40, "COMPETITION ENTRY IN", &spriteNum);
    writeText(-1, 60, "GBAX.COM 2004", &spriteNum);
    writeText(-1, 110, "WWW.GBAEMU.COM", &spriteNum);
    
    copyAllOAM();
    
    delayOrKeypress(300);
        
    fadeToBlack();
        
    // infinite loop for menu and game
    for( ; ; )
    {
        
        // turn all sprites off before displaying a mode where they're enabled
        initSprites();
        
        // load sprite palette
        int i;
        for(i=0; i<256; i++) OBJPaletteMem[i] = sprites_Palette[i];
        // load sprite tile data
        u16* sprTileData = (u16*)sprites_Bitmap;
        for(i=0; i<(26112/2); i++) OAMdata[i] = sprTileData[i];
        
        displayTiledBitmap(titlescreen_Bitmap, titlescreen_Palette);
        
        u32 spriteNum = OAM_LETTERS;
        writeText(-1, 40, "START GAME", &spriteNum);
        u32 firstSpriteOfDeepEnd = spriteNum;
        writeText(-1, 60, "IN AT THE DEEP END", &spriteNum);
        u32 firstSpriteOfInstructions = spriteNum;
        writeText(-1, 80, "INSTRUCTIONS", &spriteNum);
        
        // cycle the brightness of selected letters
        // for brightness adjust the sprites appear to have to be in semi-transparent mode
        
        // initialise the start game option to be the fading one
        
        int direction = 1;
        int fadeValue = 0;
        int selected = 0;
        
        BrightnessInit();
        
        BrightnessSetSpritesActive(OAM_LETTERS, firstSpriteOfDeepEnd - 1);
        BrightnessSetSpritesInactive(firstSpriteOfDeepEnd, firstSpriteOfInstructions - 1);
        BrightnessSetSpritesInactive(firstSpriteOfInstructions, spriteNum - 1);
        
        // flags whether the last button press detected has been released (to force discrete button
        // press and not have to time for auto-repeat)
        int buttonReleased = 1;
        
        // while nothing selected
        while( !( KEY_DOWN(KEYA) || KEY_DOWN(KEYSTART) ) )
        {
        	
        	// re-seed randomizer as many times through loop as user permits before pressing
        	// a button to start, that way we should very rarely get the same random seed
        	// when generating the level
        	srand(rand());
            
            // adjust fade one level

            fadeValue += direction;
            if(fadeValue > 13)
            {
                direction = -1;
                fadeValue = 12;
            }
            else if(fadeValue < 0)
            {
                direction = 1;
                fadeValue = 1;
            }
                
            // flip state if key pressed

            
            int dirKeyPressed = 0;

            if(buttonReleased)
            {                        
                if( KEY_DOWN(KEYDOWN) )
                {
                    selected = (selected + 1) % 3;
                    
                    dirKeyPressed = 1;
                }
                else if( KEY_DOWN(KEYUP) )
                {
                    selected--;
                    
                    if(selected < 0)
                    {
                        selected = 2;
                    }
                    
                    dirKeyPressed = 1;
                }
            }
            
            if(dirKeyPressed)
            {
                buttonReleased = 0;
                
                fadeValue = 0;
                
                // turn all sprites off         
                BrightnessSetSpritesInactive(OAM_LETTERS, spriteNum-1);

                // turn on selected entry                
                switch(selected)
                {
                    case 0 : BrightnessSetSpritesActive(OAM_LETTERS, firstSpriteOfDeepEnd-1); break;
                    case 1 : BrightnessSetSpritesActive(firstSpriteOfDeepEnd, firstSpriteOfInstructions-1); break;
                    case 2 : BrightnessSetSpritesActive(firstSpriteOfInstructions, spriteNum-1); break;
                }
                
                dirKeyPressed = 0;
                
            }
            
            // delay and update display
            
            delay(2);
            BrightnessSetLevel(fadeValue);
			copySelectOAM(OAM_LETTERS, spriteNum);
			
			// debounce button press
                
            if( !KEY_DOWN(KEYDOWN) && !KEY_DOWN(KEYUP) )
            {
                buttonReleased = 1;
            }
        }
        
        BrightnessEnd();        
        
        fadeToBlack();
        
        turnOffAllSprites();
        copyAllOAM();
        
        // load sprite palette
        for(i=0; i<256; i++) OBJPaletteMem[i] = sprites_Palette[i];
        
        switch(selected)
        {
            case 0: startLevel = 0; startGameAndManageContinues(); break;
            case 1: startLevel = 7; startGameAndManageContinues(); break;
            case 2: displayStory(); break;
        }
        
    }
}






//SaveRaw (3, (u8*)&test, sizeof (Test));
//LoadRaw (3, (u8*)&test, sizeof (Test)); 

// by dagamer34
// from http://forum.gbadev.org/viewtopic.php?t=2550

// Saves raw data to SRAM
void SaveRaw (u16 offset, u8* rawdata, u16 size)
{
   u8* temp = (u8*)rawdata;
   u16 loop;

   for (loop = 0; loop < size; loop++)
   {
      *(u8 *)(SRAM + offset + loop) = temp [loop];
   }
}

// Loads raw data from SRAM
void LoadRaw (u16 offset, u8* rawdata, u16 size)
{
   u8* temp = (u8*)rawdata;
   u16 loop;

   for (loop = 0; loop < size; loop++)
   {
       temp [loop] = *(u8 *)(SRAM + offset + loop);
   }
} 










// before release
// ==============
// timings with Acorn version, should feel similar speed on gba hardware
// fix slow down on later levels with more robots
// tidy up source code, check comments and split up files

// to do list
// =============
// display score
// sometimes see dead person sprites appear periodically - never seen before, dodgy copy/init of OAM RAM having changed type
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



#include <gba.h>
#include <maxmod.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "soundbank.h"
#include "soundbank_bin.h"

#include "link.h"
 

// include graphics data
extern const unsigned short background_Palette[256];
extern const unsigned char background_Bitmap[15360];
extern const unsigned short sprites_Palette[256];
extern const unsigned char sprites_Bitmap[26112];
extern const unsigned short titlescreen_Palette[256];
extern const unsigned char titlescreen_Bitmap[38400];
extern const unsigned short credits_Palette[256];
extern const unsigned char credits_Bitmap[38400];





// definitions carried from old gba headers from devKitAdvance until makes sense to shift to the new
// gba
#define REG_BLDMOD     	(*(volatile u16*)0x4000050)
#define REG_COLEV      	(*(volatile u16*)0x4000052)
#define REG_COLEY      	(*(volatile u16*)0x4000054)
#define OBJPaletteMem 	((volatile u16*)0x5000200)
#define VideoBuffer 	((volatile u16*)0x6000000)
#define OAMdata 	    ((volatile u16*)0x6010000)
//#define SRAM            ((volatile u8*)0xE000000)
#define TRUE 1
#define FALSE 0
// keys
#define KEYA             1
#define KEYB             2
#define KEYSELECT        4
#define KEYSTART         8
#define KEYRIGHT         16
#define KEYLEFT          32
#define KEYUP            64
#define KEYDOWN          128
#define KEYR             256
#define KEYL             512
#define KEYS             (*(volatile u16*)0x4000130)
#define KEY_DOWN(k)       ( ! ( ( KEYS ) & k ) )
// sprites
#define COLOR_256		0x2000
#define SIZE_16			0x4000
// screenmodes
#define SCREENMODE0    0x0           //Enable screen mode 0
#define SCREENMODE1    0x1           //Enable screen mode 1
#define SCREENMODE2    0x2           //Enable screen mode 2
#define SCREENMODE3    0x3           //Enable screen mode 3
#define SCREENMODE4    0x4           //Enable screen mode 4
//#define SCREENMODE5    0x5           //Enable screen mode 5
//#define BACKBUFFER     0x10          //Determine backbuffer
//#define HBLANKOAM      0x20          //Update OAM during HBlank?
#define OBJMAP2D       0x0           //2D object (sprite) mapping
#define OBJMAP1D       0x40          //1D object(sprite) mapping
//#define FORCEBLANK     0x80          //Force a blank
#define BG0ENABLE      0x100         //Enable background 0
#define BG1ENABLE      0x200         //Enable background 1
#define BG2ENABLE      0x400         //Enable background 2
//#define BG3ENABLE      0x800         //Enable background 3
#define OBJENABLE      0x1000        //Enable sprites
//#define WIN1ENABLE     0x2000        //Enable window 1
//#define WIN2ENABLE     0x4000        //Enable window 2
//#define WINOBJENABLE   0x8000        //Enable object window
#define SetMode(mode)    (REG_DISPCNT = mode)




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

OBJATTR sprites[128];

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
#define ROBOT_MOVE_SEED ('P' | ('A' << 8) | ('U' << 16) | ('L' << 24))
u32 robotMoveSeed = ROBOT_MOVE_SEED;
// mystery token seed (as used in Acorn version)
#define MYSTERY_TOKEN_SEED ('T' | ('C' << 8) | ('E' << 16) | ('L' << 24))
u32 mysteryTokenSeed = MYSTERY_TOKEN_SEED;

int rubbleCount;            // amount of rubble in the level

u16 universalTimer;
volatile u16 frameDone;     // universalTimer once a frame's game logic has finished (for tools/linktest)
u16 timeOfDeath;

u8 area[AREA_X][AREA_Y];        // memory of what tiles are drawn where on board
u8 bombVal[AREA_X][AREA_Y];     // counters for each tiles bomb value so can increment more often than animation frame suggests
u8 bombOwner[AREA_X][AREA_Y];   // which player dropped the bomb on each tile

struct Player
{
    s16 x;                  // position on board in pixels
    s16 y;
    s16 directionX;         // direction moving in while between tiles
    s16 directionY;
    u16 sprite;             // sprite for direction facing, using the green man's sprite numbers
    u16 frame;              // animation frame, walking or dying
    u16 colour;             // offset from the green man's sprites to this player's
    u8 oamMan;              // OAM positions of the man and his halo
    u8 oamHalo;
    u8 maxBombsAllowed;
    u8 bombsCurrentlyDropped;
    u8 lifeStatus;
    u8 flameLength;
    bool halo;
    u16 haloTimer;
    bool autoPlantBombs;    // set to true if man automatically drops bombs (from having picked up a token)
    u16 autoPlantTimer;     // univeral time when man started auto dropping bombs
    int lives;
    u8 input;               // controller input this frame, IN_ bits from link.h
};

#define MAX_PLAYERS 2
struct Player player[MAX_PLAYERS];
int numPlayers = 1;         // 2 in a linked game
int localPlayer;            // which player this Gameboy controls, the screen follows him

u16 xOffset;    // screen offsets to be fed to hardware regs
u16 yOffset;
bool nuked;
int level;          // game level
int startLevel;     // which level game started on (in case different)
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

mm_sound_effect rarg = {
    { SFX_RARG } ,			// id
    (int)(1.0f * (1<<10)),	// rate
    0,		// handle
    255,	// volume
    255,	// panning
};

mm_sound_effect arg = {
    { SFX_ARG } ,			// id
    (int)(1.0f * (1<<10)),	// rate
    0,		// handle
    255,	// volume
    255,	// panning
};

bool linkLost;      // set when the other Gameboy stops answering in a linked game
bool matchOver;     // set when a linked game has been won

// random numbers for the game itself, kept apart from rand() so that two linked Gameboys
// can be given the same seed and make the same random choices
u32 gameRandSeed;

int gameRand(void)
{
    gameRandSeed = gameRandSeed * 1103515245 + 12345;
    return (gameRandSeed >> 16) & 0x7FFF;
}

int inDevelopment = 0; // remove title screens to get to action faster for code-test cycle

int vblankTest = 0;

#define IRQ_VBLANK		0x0001	//!< Catch VBlank irq // added from libtonc - must be defined in libgba somewhere


// prototype the various ANSI functions to prevent implicit declaration warnings later
//int rand(void);
//void srand(int);




// TODO: could replace these REG_VCOUNT==160 checks with SWI 0x05 which waits for vsync?

// wait for scanline to be off screen (before drawing to it)
void wait()
{
	while( REG_VCOUNT < 160 );
}

// wait until any key pressed until return
void waitForKeyPress()
{
    for( ; !((~KEYS) & 0x3FF) ; ) { mmFrame(); VBlankIntrWait(); }
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

        mmFrame();
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

        mmFrame();
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
	    sprites[loop].attr0 = COLOR_256 | 256; //y to > 159
        sprites[loop].attr1 = SIZE_16 | 256;   //x to > 239
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
    sprites[spriteNumber].attr0 = COLOR_256 | y | SQUARE;
    sprites[spriteNumber].attr1 = SIZE_16 | x;
    sprites[spriteNumber].attr2 = charNumber;
}

// set single sprite to be off display (don't update OAM)
void turnOffSprite(u8 spriteNumber)
{
    sprites[spriteNumber].attr0 = COLOR_256 | 256;
    sprites[spriteNumber].attr1 = SIZE_16 | 256;
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
        sprites[sprCount].attr0 &= ~(0xC00);	// clear OBJ mode flag to ensure Normal OBJ mode
        sprites[sprCount].attr0 |= 1 << 10;	// set to semi-transparent mode
    }
}

void BrightnessSetSpritesActive(int firstSprite, int lastSprite)
{
	// for brightness adjust, it appears the sprites have to be in semi-transparent mode

	int sprCount;
    for(sprCount = firstSprite; sprCount <= lastSprite; sprCount++)
    {
        sprites[sprCount].attr0 &= ~(0xC00);	// clear OBJ mode flag to ensure Normal OBJ mode
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
    u32 newX = x; // init in case of empty string
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
        sprites[sprCount].attr0 |= 0x400; // enable antialiasing
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
    // range check to ensure don't try and walk off the board
    if(x < 0 || y < 0 || x >= AREA_X || y >= AREA_Y)
        return FALSE;
        
    // check tile isn't a blockage
    if(area[x][y] <= T_RUBBLE_EXPLO_END)
        return FALSE;
        
    // got to here to tile must be ok to enter
    return TRUE;
}

// drop a bomb
void plantBomb(int playerNum, s8 x, s8 y)
{
    // put a bomb in area
    area[x][y] = T_BOMB_SMALL;
    
    // adjust count of number of bombs player is allowed to drop
    player[playerNum].bombsCurrentlyDropped++;
    bombOwner[x][y] = playerNum;
    
    // plant a bomb at the specified tile number on display
    drawObject(x, y, T_BOMB_SMALL);

	// set bomb countdown to total    
    bombVal[x][y] = 32;
}

// read this Gameboy's controller
u8 readLocalInput(void)
{
    u8 input = 0;
    
    if( KEY_DOWN( KEYA ) || KEY_DOWN( KEYB ) )
        input |= IN_BOMB;
    if( KEY_DOWN( KEYUP ) )
        input |= IN_UP;
    if( KEY_DOWN( KEYDOWN ) )
        input |= IN_DOWN;
    if( KEY_DOWN( KEYLEFT ) )
        input |= IN_LEFT;
    if( KEY_DOWN( KEYRIGHT ) )
        input |= IN_RIGHT;
    if( KEY_DOWN( KEYSTART ) )
        input |= IN_START;
    
    return input;
}

// get every player's input for this frame, in a linked game this waits for the other Gameboy
// returns FALSE if the link has been lost
bool readInputs(void)
{
    u8 input = readLocalInput();
    player[localPlayer].input = input;
    
    if( numPlayers > 1 )
    {
        int otherInput = linkExchange(input);
        if( LINK_LOST == otherInput )
        {
            linkLost = TRUE;
            return FALSE;
        }
        player[1 - localPlayer].input = otherInput;
    }
    
    return TRUE;
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
        
        
        // Alternative fade out that just dims the colours instead of greying them
        //const int fadeFactor = 4;
        //for(i=0; i< NUM_COLOURS_USED_IN_BACKGROUND_PALETTE; i++)
        //{
        //    u16 currentColour = currentPalette[i];
        //    u8 currentGreen = (currentColour & 0x3E0) >> 5;
        //    u8 currentBlue = (currentColour & 0x7C00) >> 10;
        //    u8 currentRed = currentColour & 0x1F;
        //    // don't fade any colour components if they would go -ve (of course in an unsigned var goes high +ve)
        //    if(currentGreen >= fadeFactor) currentGreen -= fadeFactor;
        //    if(currentBlue >= fadeFactor) currentBlue -= fadeFactor;
        //    if(currentRed >= fadeFactor) currentRed -= fadeFactor;
        //    currentPalette[i] = currentRed | (currentGreen << 5) | (currentBlue << 10);
        //}
        
        
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
    
    // wait for start to be released, pressed and released again, by either player in a linked
    // game (both Gameboys see the same presses so they unpause together)
    int stage = 0;
    while( stage < 3 )
    {
        bool startHeld;
        
        if( numPlayers > 1 )
        {
            if( !readInputs() )
                break;
            startHeld = ( (player[0].input | player[1].input) & IN_START ) != 0;
        }
        else
        {
            startHeld = KEY_DOWN( KEYSTART );
        }
        
        // stage 1 waits for a press, stages 0 and 2 for a release
        if( startHeld == (1 == stage) )
            stage++;
        else
        {
            mmFrame();
            VBlankIntrWait();
        }
    }
    
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

// act on a player's input for movement and dropping bombs
void checkInGameKeyPresses(int playerNum)
{
    struct Player* p = &player[playerNum];
    
    // dead men don't move
    if(p->lifeStatus != ALIVE)
        return;
        
    // if not currently moving
    if(!p->directionX && !p->directionY)
    {
        // convert pixels of man position to current tile number
        s8 manTileX = p->x / 16;
        s8 manTileY = p->y / 16;
    
    	// if player wants to drop a bomb or they automatically drop a bomb at the moment
        if( (p->input & IN_BOMB) || p->autoPlantBombs)
        {
        	// if the user can drop bombs on this tile then
            if( area[manTileX][manTileY] == T_SPACE)
            {
            	// drop bomb if we can
                if(p->bombsCurrentlyDropped < p->maxBombsAllowed)
                    plantBomb(playerNum, manTileX, manTileY);
            }
        }
    
    	// check for player pressing certain directions
    	
        if( p->input & IN_UP )
        {
            if(isTileEnterable(manTileX, manTileY - 1) )
            {
                p->directionY = -1;
                p->sprite = S_G_MAN_UP;
                return;
            }
        }
              
        if( p->input & IN_DOWN )
        {
            if(isTileEnterable(manTileX, manTileY + 1) )
            {
                p->directionY = 1;
                p->sprite = S_G_MAN_DOWN;
                return;
            }
        }
            
        if( p->input & IN_LEFT )
        {
            if(isTileEnterable(manTileX - 1, manTileY) )
            {
                p->directionX = -1;
                p->sprite = S_G_MAN_LEFT;
                return;
            }
        }
        
        if( p->input & IN_RIGHT )
        {
            if(isTileEnterable(manTileX + 1, manTileY) )
            {
                p->directionX = 1;
                p->sprite = S_G_MAN_RIGHT;
                return;
            }
        }
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
// previously had this function set to inline but seemed to break linker in debug build so removed
void updateBackgroundOffset(void)
{
    s16 manX = player[localPlayer].x;
    s16 manY = player[localPlayer].y;
    
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

// set up a sprite at a position on the game board, allowing for the screen scroll
void drawBoardSprite(u16 spriteNumber, u16 charNumber, s16 x, s16 y)
{
    // positions off the left or top of the display wrap round in sprite coordinates
    drawSprite(spriteNumber, charNumber, (x - xOffset) & 511, (y - yOffset) & 255);
}

// draw the men (and halos) at their current positions
void drawPlayers(void)
{
    int i;
    for(i=0; i<numPlayers; i++)
    {
        struct Player* p = &player[i];
        
        switch(p->lifeStatus)
        {
            case ALIVE :
                drawBoardSprite(p->oamMan, p->sprite + p->colour + (p->frame * 8), p->x, p->y);
                if(p->halo)
                    drawBoardSprite(p->oamHalo, S_HALO, p->x, p->y);
                else
                    turnOffSprite(p->oamHalo);
                break;
                
            case DYING :
                drawBoardSprite(p->oamMan, S_MAN_EXPLO_START + (p->frame * 8), p->x, p->y);
                turnOffSprite(p->oamHalo);
                break;
                
            default :
                turnOffSprite(p->oamMan);
                turnOffSprite(p->oamHalo);
                break;
        }
    }
}

// start a man dying
void killMan(struct Player* p)
{
    //SoundFX_Make(SOUNDFX_CHANNEL_B, SOUNDFX_ARG);
    mmEffectEx(&arg);
    
    p->lifeStatus = DYING;
    p->frame = 0;           // set to first frame of dying animation
}

void moveMan(struct Player* p)
{
    // ??? should probably extract collision detection do a different function and then only call moveMan if alive
    // so can check collisions less frequently than every frame if necessary
    
    if(p->lifeStatus == DYING)
    {
        // slow down death of man and robots if nuke has just occurred
        if( !(universalTimer % 6) )
        {
            // next frame of dying animation
            p->frame++;
            // if have exceeded dying animation then dead
            if(p->frame > 5 )
            {
            	// player now dead
                p->lifeStatus = DEAD;
                timeOfDeath = universalTimer + 1000; // add 1000 for when the screen should blank out
            }
        }
        return;
    }
    
    // dead men don't move
    if(p->lifeStatus == DEAD)
        return;
    // else alive
        
    // check for collision with explosion
    
    u8 tile1 = area[p->x / 16][p->y / 16];
    u8 tile2 = area[(p->x / 16) + (p->x % 16 ? 1 : 0)][(p->y / 16) + (p->y % 16 ? 1 : 0)];
    if( (tile1 >= T_SPACE_EXPLO_CENTRE_START && tile1 <= T_SPACE_EXPLO_DOWN_END)
     || (tile2 >= T_SPACE_EXPLO_CENTRE_START && tile2 <= T_SPACE_EXPLO_DOWN_END) )
    {
        // if we've not got a halo or if we've just blown up a reactor
        if(!p->halo || nuked)
        {
            killMan(p);
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
            if(robot[i].x <= p->x - 16)
                continue;
            // if robot to the right of man by a whole tile
            if(robot[i].x >= p->x + 16)
                continue;
            // if robot above man by a whole tile
            if(robot[i].y <= p->y - 16)
                continue;
            // if robot below man by a whole tile
            if(robot[i].y >= p->y + 16)
                continue;
            
            // robot is in same area as man so start dying
            if(!p->halo)
            {      
                killMan(p);
                return;
            }
        }
    }
    
    // if player moving (i.e. inbetween squares)
    if( p->directionX || p->directionY)
    { 
        // if moving horizontally 
        if(p->directionX)
        {
            // adjust position 2 pixel
            p->x += p->directionX*2;
            
            // if have reached the boundary of a tile then stop moving
            if( !(p->x % 16) )
            {
                p->directionX = 0;
                p->frame = 0;       // set sprite back to standing still
            }
            else
                p->frame = (p->frame + 1) % 5; // next animation frame
        }
        else
        {
            // adjust position 2 pixel
            p->y += p->directionY*2;
            
            // if have reached the boundary of a tile then stop moving
            if( !(p->y % 16) )
            {
                p->directionY = 0;
                p->frame = 0;       // set sprite back to standing still
            }
            else
                p->frame = (p->frame + 1) % 5; // adjust animation frame
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
    
    struct Player* owner = &player[ bombOwner[x][y] ];
    owner->bombsCurrentlyDropped--;
    u8 flameLength = owner->flameLength;
    
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
            if( (gameRand() % 10) >= 8 )
            {
                // randomly choose gift
                switch(gameRand() % 3)
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
    //if(!(universalTimer % 10) )
    if(!(universalTimer % 5) )
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
    
    //if(!(universalTimer % 2) )
    {
        // move to next stage of explosion
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
                    mmEffectEx(&rarg);
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
                (gameRand() % 16) < 10 )
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
        x = gameRand() % AREA_X;
        y = gameRand() % AREA_Y;
        
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

// put a player at the start of a level
void initialisePlayer(struct Player* p, s16 tileX, s16 tileY, u16 sprite)
{
    p->halo = TRUE;
    p->haloTimer = universalTimer;
    p->flameLength = 2;
    p->bombsCurrentlyDropped = 0;
    p->maxBombsAllowed = 1;
    p->autoPlantBombs = FALSE;
    p->autoPlantTimer = 0;
    
    p->x = tileX * 16;
    p->y = tileY * 16;
    p->directionX = 0;
    p->directionY = 0;
    p->sprite = sprite;
    p->frame = 0;
}

// called at start of new level to generate level etc
void initialiseLevel(void)
{
    
    robotsHalt = FALSE;
    robotsHaltCount = 0;
    
    // place man in bottom right, and the second player top left
    initialisePlayer(&player[0], AREA_X-1, AREA_Y-1, S_G_MAN_LEFT);
    if( numPlayers > 1 )
        initialisePlayer(&player[1], 0, 0, S_G_MAN_RIGHT);
    
    updateBackgroundOffset();
    
    u16 i;
    
    // load graphics into VRAM
    
    // turn off all 128 sprites available to the GBA
    initSprites();
    
   	// load background tile data
    // tile data appears to have to be loaded 16 bits at a time, why????
    u16* tileData = (u16*)background_Bitmap;
    //const unsigned char background_Bitmap
    for(i=0; i<15360/2; i++) tiles[i]=tileData[i];
    
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
            bombOwner[x][y] = 0;
        }
    }
    
    // draw men
    drawPlayers();
        
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
    playerHasContinued = FALSE;
    
    int i;
    for(i=0; i<MAX_PLAYERS; i++)
    {
        player[i].lifeStatus = ALIVE;
        player[i].lives = 2;
    }
    
    player[0].colour = 0;
    player[0].oamMan = OAM_GMAN;
    player[0].oamHalo = OAM_GHALO;
    player[1].colour = S_R_MAN_RIGHT - S_G_MAN_RIGHT;
    player[1].oamMan = OAM_RMAN;
    player[1].oamHalo = OAM_RHALO;
    
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
        
        &waitKeypress, &cls,
        "2 PLAYER GAME :",
        &newLine,
        "LINK UP TWO GAMEBOYS",
        "AND BLOW THE ER, LIVING",
        "DAYLIGHTS OUT OF THE",
        "OTHER PLAYER.",
        &newLine,
        "SIMPLE, INNIT?",
        
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
    struct Player* p = &player[0];
	p->lives--;
            
    if( p->lives >= 0 && !nuked)
    {
        // on arc version the game freezes up once death sequence over and the number of lives remaining
        // is shown on screen then after X seconds the text vanishes and the game continues exactly as before
        // with the user reincarnated on the spot he died on with the standard duration halo, all bombs etc
        // from the last go remain on the game area
        
        // show banner
        
        // compose number of lives string
        char livesMessage[] = "LIFE X";
        // says life 2, then life 3, then you're dead
        livesMessage[5] = '0' + (3 - p->lives);
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
        p->autoPlantBombs = FALSE;
        p->autoPlantTimer = 0;

        p->lifeStatus = ALIVE;

        p->halo = TRUE;
        p->haloTimer = universalTimer;
        p->frame = 0;
        
        // draw man
        drawPlayers();
        
    }
    else
    {
        
        if( nuked )
        {
            u32 spriteNum = OAM_LETTERS;
            writeText(-1, 60, "REACTOR EXPLOSION", &spriteNum);
            writeText(-1, 80, "MISSION ABORTED", &spriteNum);
            
            p->lives = -1;
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
		    p->lifeStatus = ALIVE;
		    p->lives = 2;
    	}
    	else
    	{
    		// leave gameloop function
        	return TRUE;
        }
    }
    
    return FALSE;
}

// TRUE if any player is in the given life status
bool anyPlayer(u8 lifeStatus)
{
    int i;
    for(i=0; i<numPlayers; i++)
        if(player[i].lifeStatus == lifeStatus)
            return TRUE;
    return FALSE;
}

const char* const playerName[MAX_PLAYERS] = { "GREEN", "RED" };

// show a banner of up to two lines over the game, wait a bit then remove it
void showGameBanner(const char* line1, const char* line2)
{
    u32 spriteNum = OAM_LETTERS;
    writeText(-1, 60, line1, &spriteNum);
    if(line2)
        writeText(-1, 80, line2, &spriteNum);
    
    delayOrKeypress(120);
    
    turnOffSprites(OAM_LETTERS, spriteNum);
    copyAllOAM();
}

// handles players dying in a linked game (called once no one is still in the middle of dying,
// so players dying together are dealt with together)
// returns TRUE if the game's over
int handleTwoPlayerDeaths(void)
{
    int i;
    int numDead = 0;
    int lastDead = 0;
    for(i=0; i<numPlayers; i++)
    {
        if(player[i].lifeStatus == DEAD)
        {
            player[i].lives--;
            numDead++;
            lastDead = i;
        }
    }
    
    // game over once someone's run out of lives
    int numOut = 0;
    int winner = -1;
    for(i=0; i<numPlayers; i++)
    {
        if(player[i].lives < 0)
            numOut++;
        else
            winner = i;
    }
    
    if(numOut)
    {
        // both Gameboys reach here on the same frame, finish with the link now so neither is
        // left waiting for the other while the result's displayed
        linkEndGame();
        matchOver = TRUE;
        
        u32 spriteNum = OAM_LETTERS;
        if(numOut > 1)
        {
            writeText(-1, 60, "IT'S A DRAW!", &spriteNum);
        }
        else
        {
            char winMessage[20];
            strcpy(winMessage, playerName[winner]);
            strcat(winMessage, " WINS!");
            writeText(-1, 60, winMessage, &spriteNum);
            writeText(-1, 80, winner == localPlayer ? "WELL DONE" : "UNLUCKY", &spriteNum);
        }
        copyAllOAM();
        
        // make sure a button held down during play doesn't skip the result
        delay(60);
        for( ; (~KEYS) & 0x3FF ; ) { mmFrame(); VBlankIntrWait(); }
        waitForKeyPress();
        
        fadeToBlack();
        turnOffAllSprites();
        copyAllOAM();
        return TRUE;
    }
    
    // say who died and how many lives are left
    char line1[24];
    if(nuked)
        strcpy(line1, "REACTOR EXPLOSION");
    else if(numDead > 1)
        strcpy(line1, "BOTH DIED");
    else
    {
        strcpy(line1, playerName[lastDead]);
        strcat(line1, " DIED");
    }
    char line2[] = "LIVES: GREEN X  RED X";
    line2[13] = '0' + player[0].lives + 1;
    line2[20] = '0' + player[1].lives + 1;
    showGameBanner(line1, line2);
    
    if(nuked)
    {
        // the blast has cleared the level so go on to the next
        rubbleCount = 0;
        for(i=0; i<numPlayers; i++)
            player[i].lifeStatus = ALIVE;
        return FALSE;
    }
    
    // as in a single player game, the dead come back to life where they died with a halo
    robotsHalt = FALSE;
    robotsHaltCount = 0;
    for(i=0; i<numPlayers; i++)
    {
        struct Player* p = &player[i];
        if(p->lifeStatus == DEAD)
        {
            p->autoPlantBombs = FALSE;
            p->autoPlantTimer = 0;
            p->lifeStatus = ALIVE;
            p->halo = TRUE;
            p->haloTimer = universalTimer;
            p->frame = 0;
        }
    }
    drawPlayers();
    
    return FALSE;
}

// check for a player picking up a gift
void collectGift(struct Player* p)
{
    int x,y;
    
    if(p->lifeStatus != ALIVE)
        return;
    
    // ??? this is probably being checked too often, should we do it with other collision checks?
    // (note need only check whether man is standing completely on tile not half on it,
    // as is done with explosion collisions)
    if(area[p->x / 16][p->y / 16] >= T_GIFTBOMB)
    {
        // check man is exactly on tile before triggering gift (otherwise gift appears to disappear before man on square completely)
        // ??? this may be too precise, could disregard the bottom two bits? to give +/- 3 pixels
        if( !(p->x & 15) && !(p->y & 15) )
        {
            u8 giftType = area[p->x / 16][p->y / 16];
            // blank tile now we've got the gift
            drawObject(p->x / 16, p->y / 16, T_SPACE);
            
            //SoundFX_Make(SOUNDFX_CHANNEL_B, SOUNDFX_TOKEN);
            mmEffectEx(&token);
            
            switch( giftType )
            {
                case T_GIFTBOMB     : p->maxBombsAllowed++; break;
                
                case T_GIFTFLAME    : p->flameLength++; break;
                
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
                        p->autoPlantBombs = TRUE;
                        p->autoPlantTimer = universalTimer;
                    }
                    else
                    {
                        // aura (halo)
                        p->halo = TRUE;
                        p->haloTimer = universalTimer;
                    }
                    break; // end gift-type switch
            }
        }            
    }
}

// main game loop
// In a linked game both Gameboys run this in step, everything that happens in the game must
// depend only on the players' inputs (exchanged each frame) and gameRand(), never on anything
// local to one Gameboy, or the two games will drift apart.
void gameLoop(void)
{
    int i;
    
    // infinite loop
    for( ; ; )
    {

        if(numPlayers == 1)
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
            if(player[0].lifeStatus == DEAD)
            {
                if( handleDeath() )
                	return;   
            }
        }
        else
        {
            // deal with deaths once no one's still dying, so that two players dying at
            // about the same time is a draw rather than whoever finished dying first losing
            if( !anyPlayer(DYING) )
            {
                if( anyPlayer(DEAD) )
                {
                    if( handleTwoPlayerDeaths() )
                        return;
                }
                
                if( rubbleCount <= 0 )
                {
                    // level completed
                    fadeToBlack();
                    turnOffAllSprites();
                    return;
                }
            }
        }
        
        // increment universal timer variable used to synchronise various game functions
        // ??? should probably use a system timer instead, 2 timers at 16k KHz will loop every 72 hours - acceptable
        universalTimer++;
        
        int x,y;
                
        // only do these checks every nth cycle through the loop
        //if( (universalTimer % 3) == 0)
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
            // (differences taken as u16 so they still work when universalTimer wraps round)
            if(robotsHalt)
            {
                if( (u16)(universalTimer - robotsHaltCount) > 2000 )
                    robotsHalt = FALSE;
            }
            
            for(i=0; i<numPlayers; i++)
            {
                struct Player* p = &player[i];
                
                if(p->autoPlantBombs)
                    if( (u16)(universalTimer - p->autoPlantTimer) > 2000 )
                        p->autoPlantBombs = FALSE;
                
                if(p->halo)
                    if( (u16)(universalTimer - p->haloTimer) > 700 )
                        p->halo = FALSE;
            }
            
        }
           
        // if robots are frozen then don't let them change direction
        if(!robotsHalt)
            robotAI();
        
        // check for keypresses, from both Gameboys in a linked game
        if( !readInputs() )
            return;
        
        bool pausePressed = FALSE;
        for(i=0; i<numPlayers; i++)
        {
            checkInGameKeyPresses(i);
            if(player[i].input & IN_START)
                pausePressed = TRUE;
        }
        
        if(pausePressed)
        {
            // stays in this function until unpaused
            pauseActivated();
            if(linkLost)
                return;
        }
        
        // check for gifts colliding with men
        for(i=0; i<numPlayers; i++)
            collectGift(&player[i]);
        
        // increment man movement if any and increment animation frame (test for collision with robots)
        for(i=0; i<numPlayers; i++)
            moveMan(&player[i]);
        
        updateBackgroundOffset();
        drawPlayers();
        
        // move robots after man so that offset vars have been updated
        moveRobots();
        
        // maxmod soundfx update
        mmFrame();

        // tells tools/linktest the game state is complete for this frame
        frameDone = universalTimer;

        // update display
        //wait();
        VBlankIntrWait(); // swapping this in for wait() kills speed but sound samples are great!

        // update background scrolling positions
        REG_BG1HOFS = xOffset;
        REG_BG1VOFS = yOffset;
        // copy main game sprites positions (man, halo, monsters) to the screen
        copyGameOAM();
        
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
    
    
    // display pretty much all the text
    //u32 spriteNum = 0;
    //writeText(10,30, "ABCDEFGHIJKLMNOPQRS", &spriteNum);
    //writeText(10,50, "TUVWXYZ1234567890", &spriteNum);
    //writeText(10,70, ":,./", &spriteNum);
    
    
    
    u32 spriteNum = 0;
    writeText(-1, 10, "DEVELOPMENT BUILD", &spriteNum);
    writeText(-1, 30, "NOT FOR PUBLIC RELEASE", &spriteNum);
    writeText(0, 60, "COMPILED:", &spriteNum);
    writeText(0, 76, __TIME__ " " __DATE__, &spriteNum);
    
    
    waitForKeyPress();
    
    // ??? experiment, playing around trying to save values to battery RAM, why doesn't it work?
    // *SRAM = 1;
    // (SRAM[1]) = 2;
    
    fadeToBlack();
    turnOffAllSprites();
    copyAllOAM();

}



void startGameAndManageContinues()
{
    
    numPlayers = 1;
    localPlayer = 0;
    gameRandSeed = rand();
    
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
    while( player[0].lives >= 0 );
    
}

void onVBlank() {
    //vblankTest++; // ??? i have tested and this is getting set
    mmVBlank(); // maxmod sound sample library update

    linkOnVBlank();
}

// show the waiting screen until the other Gameboy's ready to play too
// returns FALSE if the player gives up waiting
bool waitForOtherPlayer(void)
{
    displayTiledBitmap(titlescreen_Bitmap, titlescreen_Palette);
    
    int i;
    for(i=0; i<256; i++) OBJPaletteMem[i] = sprites_Palette[i];
    
    u32 spriteNum = OAM_LETTERS;
    writeText(-1, 30, "2 PLAYER LINK", &spriteNum);
    writeText(-1, 60, "WAITING FOR THE", &spriteNum);
    writeText(-1, 80, "OTHER PLAYER...", &spriteNum);
    writeText(-1, 120, "PRESS B TO CANCEL", &spriteNum);
    
    linkStart();
    
    bool ready;
    for( ; ; )
    {
        mmFrame();
        VBlankIntrWait();
        
        if( linkPeerState() != LINK_PEER_NONE )
        {
            ready = TRUE;
            break;
        }
        if( KEY_DOWN( KEYB ) )
        {
            ready = FALSE;
            break;
        }
    }
    
    fadeToBlack();
    turnOffAllSprites();
    copyAllOAM();
    
    if(!ready)
        linkStop();
    return ready;
}

// play a game against another Gameboy over the link cable
void twoPlayerGame(void)
{
    if( !waitForOtherPlayer() )
        return;
    
    numPlayers = 2;
    localPlayer = linkIsMaster() ? 0 : 1;
    linkLost = FALSE;
    matchOver = FALSE;
    
    linkBeginGame();
    
    // agree a random seed for the game, made up from both Gameboys' random numbers
    u32 seed = 0;
    int i;
    for(i=0; i<5 && !linkLost; i++)
    {
        u8 mine = rand() & 63;
        int other = linkExchange(mine);
        if( LINK_LOST == other )
            linkLost = TRUE;
        else if( localPlayer == 0 )
            seed = (seed << 12) | (mine << 6) | other;
        else
            seed = (seed << 12) | (other << 6) | mine;
    }
    
    // everything else the game uses must start the same on both Gameboys
    gameRandSeed = seed;
    robotMoveSeed = ROBOT_MOVE_SEED;
    mysteryTokenSeed = MYSTERY_TOKEN_SEED;
    startLevel = 0;
    initialiseGame();
    level = 0;
    
    bool firstLevel = TRUE;
    while( !linkLost )
    {
        for(i=0; i<256; i++) OBJPaletteMem[i] = sprites_Palette[i];
        
        u32 spriteNum = OAM_LETTERS;
        if(firstLevel)
        {
            writeText(-1, 40, 0 == localPlayer ? "YOU ARE GREEN" : "YOU ARE RED", &spriteNum);
            writeText(-1, 70, "LAST ONE ALIVE WINS", &spriteNum);
        }
        else
        {
            writeText(-1, 40, "LEVEL COMPLETE", &spriteNum);
        }
        char levelMessage[] = "LEVEL X";
        levelMessage[6] = '0' + level;
        writeText(-1, 100, levelMessage, &spriteNum);
        
        delayOrKeypress(120);
        
        fadeToBlack();
        turnOffSprites(OAM_LETTERS, spriteNum);
        copyAllOAM();
        
        nuked = FALSE;
        for(i=0; i<numPlayers; i++)
            player[i].lifeStatus = ALIVE;
        initialiseLevel();
        firstLevel = FALSE;
        
        gameLoop();
        
        if(matchOver)
            break;
        
        // level completed so on to the next, going round again after the last
        level = (level + 1) % NUM_LEVELS;
    }
    
    if(linkLost)
    {
        u32 spriteNum = OAM_LETTERS;
        writeText(-1, 60, "LINK LOST", &spriteNum);
        copyAllOAM();
        delay(180);
        fadeToBlack();
        turnOffAllSprites();
        copyAllOAM();
    }
    
    linkStop();
    numPlayers = 1;
    localPlayer = 0;
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
    
    // init sound fx
    irqInit();

	// Maxmod requires the vblank interrupt to reset sound DMA.
	// Link the VBlank interrupt to mmVBlank, and enable it. 
	irqSet( IRQ_VBLANK, onVBlank );

    irqEnable(IRQ_VBLANK);

    // link cable interrupts, only enabled while playing a linked game
    irqSet( IRQ_SERIAL, linkOnSerial );
    irqSet( IRQ_TIMER3, linkOnTimer );

    // initialise maxmod with soundbank and 8 channels
    mmInitDefault( (mm_addr)soundbank_bin, 8 );

    // load sprite palette
    for(i=0; i<256; i++) OBJPaletteMem[i] = sprites_Palette[i];

    // skip title screens if in development
    if(!inDevelopment)
    {
        // credits screen image taken from http://www.spacedaily.com/news/nuclear-blackmarket-02c.html
        // also available at http://www.staticfiends.com/galleries/government_galleries/0014.jpg
        displayTiledBitmap(credits_Bitmap, credits_Palette);

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
        
        
        //turnOffAllSprites();      
        //// remove old competition message now it's 19 years ago...
        //spriteNum = OAM_LETTERS;
        //writeText(-1, 40, "COMPETITION ENTRY IN", &spriteNum);
        //writeText(-1, 60, "GBAX.COM 2004", &spriteNum);
        //writeText(-1, 110, "WWW.GBAEMU.COM", &spriteNum);
        //copyAllOAM();
        //delayOrKeypress(300);
        

        fadeToBlack();
    }

    // infinite loop for menu and game
    for( ; ; )
    {
        int selected = 0;

        // go straight into game if dev-testing
        if(!inDevelopment)
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
            
            const int numberOfOptions = 4;

            u32 spriteNum = OAM_LETTERS;
            writeText(-1, 40, "START GAME", &spriteNum);
            u32 firstSpriteOfDeepEnd = spriteNum;
            writeText(-1, 60, "IN AT THE DEEP END", &spriteNum);
            u32 firstSpriteOfInstructions = spriteNum;
            writeText(-1, 80, "INSTRUCTIONS", &spriteNum);
            u32 firstSpriteOfMultiplayer = spriteNum;
            writeText(-1, 100, "2 PLAYER LINK", &spriteNum);
            
            // cycle the brightness of selected letters
            // for brightness adjust the sprites appear to have to be in semi-transparent mode
            
            // initialise the start game option to be the fading one
            
            int direction = 1;
            int fadeValue = 0;
                        
            BrightnessInit();
            
            BrightnessSetSpritesActive(OAM_LETTERS, firstSpriteOfDeepEnd - 1);
            BrightnessSetSpritesInactive(firstSpriteOfDeepEnd, firstSpriteOfInstructions - 1);
            BrightnessSetSpritesInactive(firstSpriteOfInstructions, firstSpriteOfMultiplayer - 1);
            BrightnessSetSpritesInactive(firstSpriteOfMultiplayer, spriteNum - 1);
            
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
                        selected = (selected + 1) % numberOfOptions;
                        
                        dirKeyPressed = 1;
                    }
                    else if( KEY_DOWN(KEYUP) )
                    {
                        selected--;
                        
                        if(selected < 0)
                        {
                            selected = numberOfOptions-1; // go to last option
                        }
                        
                        dirKeyPressed = 1;
                    }
                }
                
                if(dirKeyPressed)
                {

                    mmEffectEx(&explo);
                    
                    buttonReleased = 0;
                    
                    fadeValue = 0;
                    
                    // turn all sprites off         
                    BrightnessSetSpritesInactive(OAM_LETTERS, spriteNum-1);

                    // turn on selected entry                
                    switch(selected)
                    {
                        case 0 : BrightnessSetSpritesActive(OAM_LETTERS, firstSpriteOfDeepEnd-1); break;
                        case 1 : BrightnessSetSpritesActive(firstSpriteOfDeepEnd, firstSpriteOfInstructions-1); break;
                        case 2 : BrightnessSetSpritesActive(firstSpriteOfInstructions, firstSpriteOfMultiplayer-1); break;
                        case 3 : BrightnessSetSpritesActive(firstSpriteOfMultiplayer, spriteNum-1); break;
                    }
                    
                    dirKeyPressed = 0;
                    
                }
                
                // delay and update display
                
                delay(2);

                mmFrame();

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

        } // end of menu system, now act on it...
        else
        {
            selected = 1; // in at the deep end for development mode
        }

        switch(selected)
        {
            case 0: startLevel = 0; startGameAndManageContinues(); break;
            case 1: startLevel = 7; startGameAndManageContinues(); break;
            case 2: displayStory(); break;
            case 3: twoPlayerGame(); break;
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



















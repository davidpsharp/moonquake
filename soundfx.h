//------------------------------------------------------------------------

// sound routines
// Rich Heasman May 2002

//------------------------------------------------------------------------

#include	"gba.h"

//---------------------------------------------------------------------

// Definitions


enum 							// sounds defs
{
	SOUNDFX_EXPLO,	   	  		// explosion
	SOUNDFX_ARG,	   	  		// man death
	SOUNDFX_RARG,  	   	  		// robot death
	SOUNDFX_TOKEN,	  		    // player collects token

	SOUNDFX_MAX
};

enum
{
	SOUNDFX_CHANNEL_A,
	SOUNDFX_CHANNEL_B,

	SOUNDFX_CHANNEL_MAX
};

//---------------------------------------------------------------------

void	SoundFX_Init(void);
void	SoundFX_Update(int nChannel);
void	SoundFX_Make(int nChannel, int nSound);
void	SoundFX_HandlerDMA1(void);
void	SoundFX_HandlerDMA2(void);
void	SoundFX_Stop(int nChannel);
int		SoundFX_Playing(int nChannel);

//---------------------------------------------------------------------



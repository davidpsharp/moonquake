/////////////////////////////////////////////////////////////////////////////
//
// sound routines
// Rich Heasman May 2002
//
/////////////////////////////////////////////////////////////////////////////

#include	"soundfx.h"

#include	"dma.h"
//#include	"string.h"
//#include	"background.h"
#include	"interrupt.h"
#include	"timer.h"

#include "explo.h"
#include "arg.h"
#include "rarg.h"
#include "token.h"

//---------------------------------------------------------------------

//#include	"sounddata.c"		// raw wave data 
								// this sound data is built using makesnd.bat

//---------------------------------------------------------------------

//#define	SOUNDFX_FREQ	8000
#define	SOUNDFX_FREQ 10146

enum
{
	SOUNDFX_PRIORITY_LOW,
	SOUNDFX_PRIORITY_HI,
	SOUNDFX_LOOPING
};

typedef struct
{
	const unsigned char *pData;
	int					nSize;
	int					nType;
} SOUNDFX_TYPE;

typedef struct
{
	uint	uOutputRatio:2;
	uint	uOutputRatioA:1;
	uint	uOutputRatioB:1;
	uint	uUnused:4;
	uint	uOutputARight:1;
	uint	uOutputALeft:1;
	uint	uTimerA:1;
	uint	uFifoClearA:1;
	uint	uOutputBRight:1;
	uint	uOutputBLeft:1;
	uint	uTimerB:1;
	uint	uFifoClearB:1;
} SOUNDFX_CNT2_TYPE;

typedef struct
{
	uint	uEnable1:1;
	uint	uEnable2:1;
	uint	uEnable3:1;
	uint	uEnable4:1;
	uint	uUnuseda:3;
	uint	uEnable:1;
	uint	uUnusedb:8;
} SOUNDFX_CNT3_TYPE;

//---------------------------------------------------------------------

static BOOL	  	boSoundActive[SOUNDFX_CHANNEL_MAX];
static int	  	nSoundCurrent[SOUNDFX_CHANNEL_MAX];
static uint	  	nSoundSize[SOUNDFX_CHANNEL_MAX];

static SOUNDFX_CNT2_TYPE	SoundFXControl2;

//---------------------------------------------------------------------

SOUNDFX_TYPE pSound[SOUNDFX_MAX] =
{
	{ explo, 	sizeof(explo),		SOUNDFX_PRIORITY_HI},
	{ arg,		sizeof(arg),		SOUNDFX_PRIORITY_HI},
	{ rarg,		sizeof(rarg),		SOUNDFX_PRIORITY_LOW},
	{ token,	sizeof(token),	    SOUNDFX_PRIORITY_LOW}
};

//---------------------------------------------------------------------

void	SoundFX_Init(void)
{
	SOUNDFX_CNT3_TYPE	SoundFXControl3;
	TIMER_TYPE			TimerControl;

	TimerControl.uCount = 65536 - (16777216/SOUNDFX_FREQ);
	TimerControl.uType = 0;
	TimerControl.uCountUp = 0;
	TimerControl.uInterruptEnable = 0;
	TimerControl.uEnable = 1;
	R_TIMER0 = *(u32 *) &TimerControl;

    Interrupt_HandlerSet(INTERRUPT_TYPE_DMA1, SoundFX_HandlerDMA1);
    Interrupt_HandlerSet(INTERRUPT_TYPE_DMA2, SoundFX_HandlerDMA2);

	SoundFXControl3.uEnable1 = 0;
	SoundFXControl3.uEnable2 = 0;
	SoundFXControl3.uEnable3 = 0;
	SoundFXControl3.uEnable4 = 0;
	SoundFXControl3.uEnable = 1;
	R_SNDCNT3 = *(u16 *) &SoundFXControl3;

	SoundFXControl2.uOutputRatio = 1;
	SoundFXControl2.uOutputRatioA = 0;
	SoundFXControl2.uOutputRatioB = 0;
	SoundFXControl2.uOutputARight = 1;
	SoundFXControl2.uOutputALeft = 1;
	SoundFXControl2.uTimerA = 0;
	SoundFXControl2.uFifoClearA = 0;
	SoundFXControl2.uOutputBRight = 1;
	SoundFXControl2.uOutputBLeft = 1;
	SoundFXControl2.uTimerB = 0;
	SoundFXControl2.uFifoClearB = 0;

	boSoundActive[SOUNDFX_CHANNEL_A] = FALSE;
	boSoundActive[SOUNDFX_CHANNEL_B] = FALSE;
}	

//---------------------------------------------------------------------

void	SoundFX_HandlerDMA1(void)
{
	SoundFX_Update(SOUNDFX_CHANNEL_A);
}	

//---------------------------------------------------------------------

void	SoundFX_HandlerDMA2(void)
{
	SoundFX_Update(SOUNDFX_CHANNEL_B);
}	

//---------------------------------------------------------------------

void	SoundFX_Update(int nChannel)
{
	if (boSoundActive[nChannel])
	{
		if (nSoundSize[nChannel] >= 16 )
		{						     
			nSoundSize[nChannel] -= 16;
		}
		else	// finished
		{
			if (pSound[nSoundCurrent[nChannel]].nType == SOUNDFX_LOOPING)
			{
				SoundFX_Make(nChannel, nSoundCurrent[nChannel]);
			}
			else
			{
				SoundFX_Stop(nChannel);
			}
		} 
	}
}	

//---------------------------------------------------------------------

void	SoundFX_Make(int nChannel, int nSound)
{
	DMA_TYPE	Dma;
	uint		pSrc;
	uint		pDest;
	int			nDmaChannel;
	int			nSize;
	BOOL		boPlay;

	boPlay = TRUE;
	if (boSoundActive[nChannel])
	{
		if ((pSound[nSound].nType != SOUNDFX_PRIORITY_HI)
		 && (pSound[nSoundCurrent[nChannel]].nType == SOUNDFX_PRIORITY_HI))
		{
			boPlay = FALSE;
		}
	}

	if (boPlay)
	{
		SoundFX_Stop(nChannel);

		pSrc = (uint) pSound[nSound].pData;
		nSize = pSound[nSound].nSize;
		nSize -= nSize%16;

		nSoundCurrent[nChannel] = nSound;
		nSoundSize[nChannel] = nSize;

		if (nChannel == SOUNDFX_CHANNEL_A)
		{
			pDest = MEM_SNDFIFO_A;
			SoundFXControl2.uFifoClearA = 1;
			SoundFXControl2.uFifoClearB = 0;
		}
		else
		{
			pDest = MEM_SNDFIFO_B;
			SoundFXControl2.uFifoClearA = 0;
			SoundFXControl2.uFifoClearB = 1;
		}

		Dma.uSrc = pSrc;
		Dma.uDest = pDest;
		Dma.uCount = 4;
		Dma.uDestInc = DMA_LEAVE;
		Dma.uSrcInc = DMA_INC;
		Dma.uRepeat = 1;
		Dma.uSize = DMA_32_BIT;		  
		Dma.uMode = DMA_FIFO;
		Dma.uInterrupt = 1;
		Dma.uEnable = 1;

		nDmaChannel = nChannel + 1;
		Dma_Set(nDmaChannel, &Dma);

		R_SNDCNT2 = *(u16 *) &SoundFXControl2;

		boSoundActive[nChannel] = TRUE;
	}
}

//---------------------------------------------------------------------

void	SoundFX_Stop(int nChannel)
{
	DMA_TYPE	Dma;
	int			nDmaChannel;

	if (boSoundActive[nChannel])
	{
		nDmaChannel = nChannel + 1;
		Dma_Get(nDmaChannel, &Dma);
		Dma.uCount = 4;
		Dma.uDestInc = DMA_LEAVE;
		Dma.uSrcInc = DMA_INC;
		Dma.uRepeat = 0;
		Dma.uSize = DMA_32_BIT;		  
		Dma.uMode = DMA_FIFO;
		Dma.uInterrupt = 1;
		Dma.uEnable = 1;
		Dma_Set(nDmaChannel, &Dma);

		boSoundActive[nChannel] = FALSE;
		Dma.uEnable = 0;
		Dma_Set(nDmaChannel, &Dma);
	}	 
}	

//---------------------------------------------------------------------

int		SoundFX_Playing(int nChannel)
{
	int	nPlaying;

	nPlaying = SOUNDFX_MAX;
	if (boSoundActive[nChannel])
	{
		nPlaying = nSoundCurrent[nChannel];
	}

	return(nPlaying);
}	

//---------------------------------------------------------------------


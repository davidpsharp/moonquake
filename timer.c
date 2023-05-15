//------------------------------------------------------------------------------------

// timer
// Rich Heasman May 2002

//------------------------------------------------------------------------------------

#include "gba.h"
#include "timer.h"

//#include "vblank.h"

//------------------------------------------------------------------------------------

/*
// commented out by dave as I don't want to include vblank and looks like this stuff isn't used for soundfx anyway
void	Timer_Set(uint *uTimer, uint uLength)
{
	*uTimer = (VBlank_FrameCounterGet() + uLength);
}


//------------------------------------------------------------------------------------

BOOL	Timer_Mature(uint *uTimer)
{
	BOOL	boMature;

	boMature = FALSE;
	if (VBlank_FrameCounterGet() >= *uTimer)
	{
		boMature = TRUE;
	}

	return(boMature);
}

//------------------------------------------------------------------------------------

uint 	Timer_Get(uint *uTimer)
{
	int	nRemaining;

	nRemaining = *uTimer - VBlank_FrameCounterGet();
	if (nRemaining < 0)
	{
		nRemaining = 0;
	}

	return(nRemaining);
}	

//------------------------------------------------------------------------------------
*/
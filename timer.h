//------------------------------------------------------------------------------------

// timer
// Rich Heasman May 2002

//------------------------------------------------------------------------------------

#include	"gba.h"

//------------------------------------------------------------------------------------

typedef struct
{
	uint	uCount:16;
	uint	uType:2;
	uint	uCountUp:1;
	uint	uUnuseda:3;
	uint	uInterruptEnable:1;
	uint	uEnable:1;
	uint	uUnusedb:8;
} TIMER_TYPE;

//------------------------------------------------------------------------------------

void 	Timer_Set(uint *uTimer, uint uLength);
BOOL 	Timer_Mature(uint *uTimer);
uint 	Timer_Get(uint *uTimer);

//------------------------------------------------------------------------------------

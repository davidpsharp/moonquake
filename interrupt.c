//------------------------------------------------------------------------------------

// interrupt
// Rich Heasman April 2002

//------------------------------------------------------------------------------------

#include "gba.h"
#include "interrupt.h"

//------------------------------------------------------------------------------------

typedef struct
{
	uint	uVBlank:1;
	uint	uHBlank:1;
	uint	uVCount:1;
	uint	uTimer0:1;
	uint	uTimer1:1;
	uint	uTimer2:1;
	uint	uTimer3:1;
	uint	uSerialComms:1;
	uint	uDMA0:1; 
	uint	uDMA1:1;  
	uint	uDMA2:1;  
	uint	uDMA3:1;  
	uint	uKey:1;
	uint	uCassette:1;
	uint	uUnused:2;
} INTERRUPT_TYPE;

//------------------------------------------------------------------------------------

u32 IntrTable[14];

static void 			Interrupt_DefaultHandler(void);
static INTERRUPT_TYPE	InterruptControl;

//------------------------------------------------------------------------------------

void	Interrupt_Init(void)
{
    IntrTable[INTERRUPT_TYPE_VBL]  =(u32)&Interrupt_DefaultHandler;  
	IntrTable[INTERRUPT_TYPE_HBL]  =(u32)&Interrupt_DefaultHandler;  
	IntrTable[INTERRUPT_TYPE_VCNT] =(u32)&Interrupt_DefaultHandler; 
	IntrTable[INTERRUPT_TYPE_TIM0] =(u32)&Interrupt_DefaultHandler; 
	IntrTable[INTERRUPT_TYPE_TIM1] =(u32)&Interrupt_DefaultHandler; 
	IntrTable[INTERRUPT_TYPE_TIM2] =(u32)&Interrupt_DefaultHandler; 
	IntrTable[INTERRUPT_TYPE_TIM3] =(u32)&Interrupt_DefaultHandler; 
	IntrTable[INTERRUPT_TYPE_SIO]  =(u32)&Interrupt_DefaultHandler;  // (do not use when using MBV2)
	IntrTable[INTERRUPT_TYPE_DMA0] =(u32)&Interrupt_DefaultHandler; 
	IntrTable[INTERRUPT_TYPE_DMA1] =(u32)&Interrupt_DefaultHandler; 
	IntrTable[INTERRUPT_TYPE_DMA2] =(u32)&Interrupt_DefaultHandler;  
	IntrTable[INTERRUPT_TYPE_DMA3] =(u32)&Interrupt_DefaultHandler;  
	IntrTable[INTERRUPT_TYPE_KEY]  =(u32)&Interrupt_DefaultHandler; 
	IntrTable[INTERRUPT_TYPE_CART] =(u32)&Interrupt_DefaultHandler;  

	InterruptControl.uVBlank = 0;
	InterruptControl.uHBlank = 0;
	InterruptControl.uVCount = 0;
	InterruptControl.uTimer0 = 0;
	InterruptControl.uTimer1 = 0;
	InterruptControl.uTimer2 = 0;
	InterruptControl.uTimer3 = 0;
	InterruptControl.uSerialComms = 0;
	InterruptControl.uDMA0 = 0; 
	InterruptControl.uDMA1 = 0;  
	InterruptControl.uDMA2 = 0;  
	InterruptControl.uDMA3 = 0;  
	InterruptControl.uKey = 0;
	InterruptControl.uCassette = 0;

	R_INTENA = *(u16 *) &InterruptControl;
}

//------------------------------------------------------------------------------------

void 	Interrupt_HandlerSet(int nInterrupt, void *pFunction)
{
    IntrTable[nInterrupt]=(u32) pFunction;
}

//------------------------------------------------------------------------------------

void 	Interrupt_Enable(void)
{
	InterruptControl.uDMA1 = 1;
	InterruptControl.uDMA2 = 1;
	InterruptControl.uVBlank = 1;
	R_INTENA = *(u16 *) &InterruptControl;
	R_INTMST = 1;							// master interrupt enable
}

//------------------------------------------------------------------------------------

void 	Interrupt_HWReset(void)
{
	asm ("mov r0,#0x8c;" 
		"bx r0;");

	
	// original defender source used to be like this, adjusted to prevent multi-line string warning
	//asm ("
	//	mov r0,#0x8c 
	//	bx r0 
	//");
	
}

//------------------------------------------------------------------------------------

static void Interrupt_DefaultHandler(void)
{
}

//------------------------------------------------------------------------------------

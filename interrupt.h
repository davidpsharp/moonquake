//------------------------------------------------------------------------------------

// interrupt
// Rich Heasman April 2002

//------------------------------------------------------------------------------------

enum
{
	INTERRUPT_TYPE_VBL,    
	INTERRUPT_TYPE_HBL,    
	INTERRUPT_TYPE_VCNT,   
	INTERRUPT_TYPE_TIM0,   
	INTERRUPT_TYPE_TIM1,   
	INTERRUPT_TYPE_TIM2,   
	INTERRUPT_TYPE_TIM3,   
	INTERRUPT_TYPE_SIO,    
	INTERRUPT_TYPE_DMA0,   
	INTERRUPT_TYPE_DMA1,   
	INTERRUPT_TYPE_DMA2,   
	INTERRUPT_TYPE_DMA3,   
	INTERRUPT_TYPE_KEY,    
	INTERRUPT_TYPE_CART   
};

//------------------------------------------------------------------------------------

void	Interrupt_Init(void);
void 	Interrupt_HandlerSet(int nInterrupt, void *pFunction);
void 	Interrupt_Enable(void);
void 	Interrupt_HWReset(void);

//------------------------------------------------------------------------------------

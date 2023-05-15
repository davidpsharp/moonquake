//------------------------------------------------------------------------

// dma
// Rich Heasman May 2002

//------------------------------------------------------------------------

#include	"gba.h"

enum
{
	DMA_INC,
	DMA_DEC,
	DMA_LEAVE
};

enum
{
	DMA_16_BIT,
	DMA_32_BIT
};

enum
{
	DMA_NOW,
	DMA_ON_VBLANK,
	DMA_ON_HBLANK,
	DMA_FIFO
};

typedef struct
{
	uint	uSrc:28;			// only 27 for DMA0
	uint	uUnused1:4;
	uint	uDest:28;			// only 27 for DMA0,1,2
	uint	uUnused2:4;
	uint	uCount:14;
	uint	uUnused3:2;

	uint	uUnused4:5;
	uint	uDestInc:2;
	uint	uSrcInc:2;
	uint	uRepeat:1;
	uint	uSize:1;
	uint	uUnused5:1;
	uint	uMode:2;
	uint	uInterrupt:1;
	uint	uEnable:1;
} DMA_TYPE;

//-----------------------------------------------------------------------

void	Dma_Set(uint uChannel, DMA_TYPE *pDma);
void	Dma_Get(uint uChannel, DMA_TYPE *pDma);

//-----------------------------------------------------------------------

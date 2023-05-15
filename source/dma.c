//------------------------------------------------------------------------

// dma
// Rich Heasman May 2002

//------------------------------------------------------------------------

#include	"gba.h"
#include	"dma.h"

//-----------------------------------------------------------------------

void	Dma_Set(uint uChannel, DMA_TYPE *pDma)
{
	u32	*pDest;
	u32 *pSrc;

	pDma->uUnused1 = 0;
	pDma->uUnused2 = 0;
	pDma->uUnused3 = 0;
	pDma->uUnused4 = 0;
	pDma->uUnused5 = 0;

	pSrc = (u32 *) pDma;
	pDest = (u32 *) (MEM_DMABASE + ( uChannel * 0x0C));
	*pDest++ = *pSrc++;
	*pDest++ = *pSrc++;
	*pDest++ = *pSrc++;
}	

//-----------------------------------------------------------------------

void	Dma_Get(uint uChannel, DMA_TYPE *pDma)
{
	u32	*pDest;
	u32 *pSrc;

	pSrc = (u32 *) (MEM_DMABASE + ( uChannel * 0x0C));
	pDest = (u32 *) pDma;
	*pDest++ = *pSrc++;
	*pDest++ = *pSrc++;
	*pDest++ = *pSrc++;
}	

//-----------------------------------------------------------------------

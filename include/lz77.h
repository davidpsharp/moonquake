// LZ77 decompression straight into VRAM

#ifndef LZ77_H
#define LZ77_H

#include <gba_types.h>

// unpack data compressed in the GBA BIOS's LZ77 format (e.g. by gbalzss) to VRAM
void unLZ77Vram(const void* source, volatile void* dest);

#endif

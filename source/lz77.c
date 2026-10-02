// LZ77 decompression straight into VRAM
//
// Does the job of the BIOS's LZ77UnCompVram, but done here so as not to depend on the BIOS:
// some BIOS dumps used with emulators get it wrong (one turned scattered 0 bytes of the
// sprites into 1s, which showed as stray pixels).
//
// VRAM can only be written 16 bits at a time, so each even byte is held back until the odd
// byte after it is known, and bytes copied from earlier output are read back from VRAM
// (or from the held back byte).

#include "lz77.h"

void unLZ77Vram(const void* source, volatile void* dest)
{
    const u8* in = (const u8*)source;
    volatile u16* out = (volatile u16*)dest;

    // header: 0x10 then the unpacked size in 24 bits
    u32 size = in[1] | (in[2] << 8) | (in[3] << 16);
    in += 4;

    u32 pos = 0;
    u16 held = 0;   // the even byte waiting for its odd partner

    while( pos < size )
    {
        // each flag bit says whether the next item is a copy of earlier output or a byte
        u8 flags = *in++;
        int i;
        for(i=0; i<8 && pos<size; i++, flags <<= 1)
        {
            int length = 1;
            u32 from = 0;
            u8 byte = 0;

            if( flags & 0x80 )
            {
                // 4 bits of length (3-18 bytes), 12 bits of distance back (1-4096)
                length = (in[0] >> 4) + 3;
                from = pos - ((((in[0] & 15) << 8) | in[1]) + 1);
                in += 2;
            }
            else
                byte = *in++;

            for( ; length && pos < size; length--)
            {
                if( flags & 0x80 )
                {
                    if( from == pos - 1 && (pos & 1) )
                        byte = held & 255;  // the byte just held back
                    else
                        byte = (out[from >> 1] >> ((from & 1) * 8)) & 255;
                    from++;
                }

                if( pos & 1 )
                    out[pos >> 1] = held | (byte << 8);
                else
                    held = byte;
                pos++;
            }
        }
    }

    // an odd size leaves a byte held back
    if( pos & 1 )
        out[pos >> 1] = held;
}

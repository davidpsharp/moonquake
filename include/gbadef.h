
// extracts required for soundfx library taken from Defender's gba.h file

#define MEM_IO					0x04000000	// Register Base, all HW Registers are in here.

#define R_INTENA		*(volatile u16 *)(MEM_IO + 0x200)			// Int Enable
#define R_INTMST		*(volatile u16 *)(MEM_IO + 0x208)			// Int Enable Master


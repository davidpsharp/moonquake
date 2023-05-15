echo compile moonquake

path=D:\work\devkitadv\bin

@REM use the following to view options: gcc -v --help|more

@REM quick build
@REM gcc -o moonquake.elf moonquake.c

@REM slower build but much more control and more warnings previously -o
gcc -Wall -O1 -mthumb -mthumb-interwork -c moonquake.c -o moonquake.o
gcc -Wall -O1 -mthumb -mthumb-interwork -c interrupt.c -o interrupt.o
gcc -Wall -O1 -mthumb -mthumb-interwork -c soundfx.c -o soundfx.o
gcc -Wall -O1 -mthumb -mthumb-interwork -c dma.c -o dma.o

@REM get gcc to do the linking
@REM gcc -Wall -mthumb -mthumb-interwork -s moonquake.o -o moonquake.elf

@REM optimisations -O -o -O0 -O1 -O2 -O3 (3 buggy?)
@REM params passed as consts are not to be cast to non-const inside the function, volatile must be used if the variable is modified outside the code's mainline.
@REM '-mthumb'   // thumb only
@REM '-mthumb -mthumb-interwork' // thumb code, interworking (can be called from arm code)
@REM '-mthumb-interwork'  or '-marm -mthumb-interwork' // arm code, interworking (can be called from thumb code)
@REM '' (no mode setting) or '-marm'  // arm code no interworking.

as -I d:\work\devkitadv\include -I d:\work\devkitadv\arm-agb-elf\include -I d:\work\devkitadv\lib\gcc-lib\arm-agb-elf\3.0.2\include -mthumb-interwork crt0.s -ocrt0.o

@REM -lgcc on end links in libgcc.a which prevents getting _divsi3 linking probs when using division somewhere in code
ld -L d:\work\DevKitAdv\lib\gcc-lib\arm-agb-elf\3.0.2\interwork -L d:\work\devkitadv\arm-agb-elf\lib\interwork -T lnkscript -o moonquake.elf crt0.o   crtbegin.o  crtend.o   moonquake.o soundfx.o interrupt.o dma.o -lc -lgcc

objcopy -O binary moonquake.elf moonquake.gba

gbafix "-tMoonquake" -cNONE -mP8 moonquake.gba

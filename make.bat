echo make moonquake

path=d:\work\devkitadv\bin

echo converting graphics
gfx2gba -pbackground.pal -fsrc -T16 -t8 -c256 background.bmp
gfx2gba -psprites.pal -fsrc -T16 -t8 -x -c256 sprites.bmp
gfx2gba -ptitlescreen.pal -fsrc -t8 -c256 titlescreen.bmp
gfx2gba -pcredits.pal -fsrc -t8 -c256 credits.bmp

@REM old method - pure bitmap, no tiling (as for mode4)
@REM gfx2gba -ptitlescreen.pal -fsrc -c256 titlescreen.bmp

echo converting sound fx
b2x -c -t "const char" -n explo <explo.dat> explo.h
b2x -c -t "const char" -n arg <arg.dat> arg.h
b2x -c -t "const char" -n token <token.dat> token.h
b2x -c -t "const char" -n rarg <rarg.dat> rarg.h

compile.bat

rc mandelbrot.rc

cl.exe -nologo -Gm- -GR- -EHa- -Oi -DUNICODE -GS- mandelbrot.cpp mandelbrot.res -link -nodefaultlib user32.lib kernel32.lib gdi32.lib

@echo off

set build_dir=.\build\

if not exist %build_dir% (mkdir %build_dir%)

cl /nologo /Fo%build_dir%\ /Fe%build_dir%\ ^
/std:c++20 /permissive- /W3 ^
/Zi /DEBUG:FASTLINK /Fd%build_dir%\ ^
/MD ^
src\main.cpp ^
/I vendor\glfw-3.3.8\include\   vendor\glfw-3.3.8\lib-vc2022\glfw3.lib ^
/I vendor\glad\include\         vendor\glad\src\gl.c ^
/I vendor\stb\include\ ^
user32.lib gdi32.lib shell32.lib


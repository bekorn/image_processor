@echo off

if not exist build (mkdir build)

cl /Fobuild\ /Febuild\ ^
/std:c++20 /permissive- /W4 ^
/MD ^
src\main.cpp ^
/I vendor\glfw-3.3.8\include\   vendor\glfw-3.3.8\lib-vc2022\glfw3.lib ^
/I vendor\glad\include\         vendor\glad\src\gl.c ^
user32.lib gdi32.lib shell32.lib

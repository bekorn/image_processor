@echo off

set build_dir=.\build_dll\

if not exist %build_dir% (mkdir %build_dir%)

cl /Fo%build_dir% /Fe%build_dir% ^
/std:c++20 /permissive- /W3 ^
/Z7 /DEBUG:FASTLINK /Fd%build_dir% ^
/MD /LD ^
src\process.cpp

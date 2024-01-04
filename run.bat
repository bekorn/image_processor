@echo off

rem https://stackoverflow.com/questions/34439956/vc-crash-when-freeing-a-dll-built-with-openmp
rem This has to be set before the dll is run, here works well
set OMP_WAIT_POLICY=passive

build\main.exe MrIncredible.png

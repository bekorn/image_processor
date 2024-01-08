@echo off

set msvc_dir=%VS2017INSTALLDIR%
set msvc_dir=%VS2019INSTALLDIR%
set msvc_dir=%VS2022INSTALLDIR%
if "%msvc_dir%"=="" (echo No MSVC installation found && exit /b)

call "%msvc_dir%\VC\Auxiliary\Build\vcvars64.bat"

rem https://stackoverflow.com/questions/34439956/vc-crash-when-freeing-a-dll-built-with-openmp
rem This has to be set before a dll is run. Setting it in run.bat doesn't affect vscode debugging.
set OMP_WAIT_POLICY=passive

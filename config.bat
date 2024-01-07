@echo off

pushd "%VS2022INSTALLDIR%"
call "VC\Auxiliary\Build\vcvars64.bat"
popd

rem https://stackoverflow.com/questions/34439956/vc-crash-when-freeing-a-dll-built-with-openmp
rem This has to be set before a dll is run, setting it in run.bat doesn't affect vscode debugging
set OMP_WAIT_POLICY=passive

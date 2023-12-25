@echo off

set build_dir=.\build_dll\

if not exist %build_dir% (
    mkdir %build_dir%
)

set lang_args=/std:c++20 /permissive-
set file_args=/Fo%build_dir% /Fe%build_dir%
set warn_args=/W3
set common_args=%lang_args% %file_args% %warn_args%

rem https://stackoverflow.com/questions/34439956/vc-crash-when-freeing-a-dll-built-with-openmp
set OMP_WAIT_POLICY=passive
set rel_args=/O2 /Ob3 /DEBUG:NO /openmp

set deb_args=/Zi /JMC /DEBUG:FASTLINK /Fd%build_dir% /MD /O2

set common_args=%common_args% %rel_args%


rem Compile Precompiled Header
if not exist %build_dir%pch.cpp (
    echo #include "process.hpp" > %build_dir%pch.cpp
)
if not exist %build_dir%.pch (
    cl /nologo /c %common_args% ^
    /Ycprocess.hpp /Fp%build_dir%.pch ^
    %build_dir%pch.cpp /I src\
)

rem Build DLL
cl /nologo %common_args% ^
/Yuprocess.hpp /Fp%build_dir%.pch ^
/LD  ^
src\process.cpp %build_dir%pch.obj ^
/link /noimplib /noexp /debug:fastlink /incremental:no ^

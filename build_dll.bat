@echo off

set build_dir=.\build_dll\

if not exist %build_dir% (mkdir %build_dir%)

cl /nologo /Fo%build_dir% /Fe%build_dir% ^
/std:c++20 /permissive- /W3 ^
/Zi /JMC /DEBUG:FASTLINK /MD /LD /analyze- ^
src\process.cpp ^
/link /noimplib /noexp /debug:fastlink /incremental /ltcg:off /time ^

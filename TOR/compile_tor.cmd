@echo off
C:\Base\Compiler\Dev-Cpp\bin\gcc.exe -O3 -funroll-loops -fno-exceptions -fno-rtti -Wno-unknown-pragmas -Wno-sign-compare -Wno-conversion -march=i486 -mtune=pentiumpro -fomit-frame-pointer -fstrict-aliasing -ffast-math -fforce-addr %1 %2 %3 tor.cpp -otor.exe
::-DSTATS, -DDEBUG



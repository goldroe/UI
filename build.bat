@echo off

IF NOT EXIST build MKDIR build
SET includes=/Iext\freetype\include
SET sources=src\ui_demo.cpp src\UI.cpp
SET libs=freetype.lib user32.lib kernel32.lib winmm.lib

SET warning_flags=/W4 /wd4100 /wd4189 /wd4530 /wd4201
SET compiler_flags=/nologo /FC %warning_flags% %includes% /Fdbuild\ /Fobuild\
SET linker_flags=/OPT:REF -INCREMENTAL:NO /debug /IGNORE:4098 /IGNORE:4099 /LIBPATH:ext\freetype\ %libs% 

REM Debug flags
SET compiler_flags=%compiler_flags% /DDEBUG /Zi

CL %compiler_flags% %sources% /link %linker_flags%

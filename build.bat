@echo off
set "vctoolsdir=%programfiles(x86)%\Microsoft Visual Studio 14.0\VC\"
set "winkits10include=%programfiles(x86)%\Windows Kits\10\include\10.0.10240.0\"
set "winkits10lib=%programfiles(x86)%\Windows Kits\10\lib\10.0.10240.0\"
set "winkits8dir=%programfiles(x86)%\Windows Kits\8.1\"
set "INCLUDE=%vctoolsdir%\include;%winkits10include%\ucrt;%winkits8dir%\include\shared;%winkits8dir%\include\um;"
set "LIB=%vctoolsdir%\lib\amd64;%winkits8dir%\lib\winv6.3\um\x64;%winkits10lib%\ucrt\x64;"
set "PATH=%vctoolsdir%\bin\amd64;%PATH%"
::-d2cgsummary
cl -O2 -Z7 -nologo -W4 -WX packer.cpp && packer
if %errorlevel%==0 (
    cl -O2 -Z7 -nologo -W4 -WX main.cpp /link -SUBSYSTEM:windows
)

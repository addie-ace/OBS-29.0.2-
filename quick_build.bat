@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvarsall.bat" x64
set CMAKE=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe
cd /d H:\ceshiTRAE\obs-auto-switcher
echo === Incremental Build ===
"%CMAKE%" --build build --config Release
if %errorlevel% neq 0 ( echo Build FAILED & exit /b 1 )
echo === Deploying ===
copy /Y build\obs-auto-switcher.dll H:\obs29\obs-studio\obs-plugins\64bit\ >nul 2>&1
echo DONE

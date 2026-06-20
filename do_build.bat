@echo off
chcp 65001 >nul

echo Setting up VC environment...
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvarsall.bat" x64

echo.
echo Running CMake configure...
cd /d H:\ceshiTRAE\obs-auto-switcher
if exist build rmdir /s /q build

"C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" -B build -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="H:\Qt\6.3.1\msvc2019_64" -DOBS_INSTALL_DIR="H:\obs29\obs-studio" -DOBS_INCLUDE_DIR="H:\obs29\obs-studio\include\obs" -DOBS_LIB_DIR="H:\obs29\obs-studio\lib" -DOBS_BIN_DIR="H:\obs29\obs-studio\bin\64bit" .

if %errorlevel% neq 0 (
    echo CMake configure FAILED
    exit /b 1
)

echo.
echo Building Release...
"C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" --build build --config Release

if %errorlevel% neq 0 (
    echo Build FAILED
    exit /b 1
)

echo.
echo Build SUCCESS
copy /Y build\obs-auto-switcher.dll H:\obs29\obs-studio\obs-plugins\64bit\ >nul 2>&1
echo DLL deployed to OBS

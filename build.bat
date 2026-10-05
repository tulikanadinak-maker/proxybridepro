@echo off
echo ============================================
echo ProxyBridge Pro - Build Script
echo ============================================

:: Load Visual Studio environment
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
if errorlevel 1 (
    echo ERROR: Could not find Visual Studio 2022 BuildTools
    pause
    exit /b 1
)

echo [OK] Visual Studio environment loaded
echo Compiler: 
where cl.exe

:: Set vcpkg
set VCPKG_ROOT=C:\vcpkg

:: Clean build folder
if exist build rmdir /s /q build

:: Configure
echo.
echo [STEP 1] Configuring with CMake...
cmake -B build -S . -G "Visual Studio 17 2022" -A x64 -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake
if errorlevel 1 (
    echo ERROR: CMake configure failed!
    pause
    exit /b 1
)

:: Build
echo.
echo [STEP 2] Building Release...
cmake --build build --config Release
if errorlevel 1 (
    echo ERROR: Build failed!
    pause
    exit /b 1
)

echo.
echo ============================================
echo BUILD SUCCESSFUL!
echo Executable: build\bin\Release\ProxyBridgePro.exe
echo ============================================
pause

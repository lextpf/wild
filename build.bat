@echo off
REM ============================================================================
REM build.bat - Complete build pipeline for wild
REM ============================================================================
REM This script:
REM   1. Builds Debug and Release configurations
REM   2. Compiles shaders to SPIR-V
REM   3. Copies assets to build folders
REM   4. Cleans up shader binaries from shaders folder
REM   5. Generates Doxygen documentation
REM ============================================================================

setlocal enabledelayedexpansion

REM Get the directory where this script is located
set "SCRIPT_DIR=%~dp0"
cd /d "%SCRIPT_DIR%"

echo ============================================================================
echo                        WILD BUILD PIPELINE
echo ============================================================================
echo.

REM ============================================================================
REM STEP 1: Build the project
REM ============================================================================
echo [1/5] Building project (Debug and Release)...
echo ----------------------------------------------------------------------------

if not exist build mkdir build
cd build

echo Configuring CMake...
cmake ..
if %errorlevel% neq 0 (
    echo ERROR: CMake configuration failed!
    pause
    exit /b %errorlevel%
)

echo.
echo Building Debug configuration...
cmake --build . --config Debug
if %errorlevel% neq 0 (
    echo ERROR: Debug build failed!
    pause
    exit /b %errorlevel%
)

echo.
echo Building Release configuration...
cmake --build . --config Release
if %errorlevel% neq 0 (
    echo ERROR: Release build failed!
    pause
    exit /b %errorlevel%
)

cd /d "%SCRIPT_DIR%"
echo [1/5] Build complete!
echo.

REM ============================================================================
REM STEP 2: Compile shaders
REM ============================================================================
echo [2/5] Compiling shaders...
echo ----------------------------------------------------------------------------

REM Check if glslangValidator is available
where glslangValidator >nul 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo WARNING: glslangValidator not found!
    echo Skipping shader compilation.
    echo To compile shaders, install Vulkan SDK: https://vulkan.lunarg.com/
    goto :skip_shaders
)

echo Compiling sprite.vert...
glslangValidator -V -DUSE_VULKAN shaders/sprite.vert -o shaders/sprite.vert.spv
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: Failed to compile sprite.vert
    pause
    exit /b 1
)

echo Compiling sprite.frag...
glslangValidator -V -DUSE_VULKAN shaders/sprite.frag -o shaders/sprite.frag.spv
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: Failed to compile sprite.frag
    pause
    exit /b 1
)

echo [2/5] Shaders compiled successfully!
:skip_shaders
echo.

REM ============================================================================
REM STEP 3: Copy assets to build folders
REM ============================================================================
echo [3/5] Copying assets to build folders...
echo ----------------------------------------------------------------------------

REM Create directories if they don't exist
if not exist "build\Debug\assets" mkdir "build\Debug\assets"
if not exist "build\Debug\shaders" mkdir "build\Debug\shaders"
if not exist "build\Release\assets" mkdir "build\Release\assets"
if not exist "build\Release\shaders" mkdir "build\Release\shaders"

REM Copy assets to Debug
echo Copying assets to build\Debug\assets...
xcopy /E /Y /I "assets" "build\Debug\assets" >nul

REM Copy assets to Release
echo Copying assets to build\Release\assets...
xcopy /E /Y /I "assets" "build\Release\assets" >nul

REM Clean shader folders and copy only binaries
echo Copying shader binaries to build\Debug\shaders...
del /Q "build\Debug\shaders\*" >nul 2>&1
copy /Y "shaders\*.spv" "build\Debug\shaders\" >nul 2>&1

echo Copying shader binaries to build\Release\shaders...
del /Q "build\Release\shaders\*" >nul 2>&1
copy /Y "shaders\*.spv" "build\Release\shaders\" >nul 2>&1

REM Copy save files from root project folder to build folders (next to executable)
echo Copying save files to build folders...
set "SAVE_COPIED=0"
for %%f in (save*.json) do (
    copy /Y "%%f" "build\Debug\" >nul
    copy /Y "%%f" "build\Release\" >nul
    echo   Copied %%f
    set "SAVE_COPIED=1"
)
if "!SAVE_COPIED!"=="0" (
    echo No save*.json files found in project root.
) else (
    echo Save files copied to Debug and Release folders.
)

echo [3/5] Assets copied successfully!
echo.

REM ============================================================================
REM STEP 4: Clean up shader binaries from source folder
REM ============================================================================
echo [4/5] Cleaning up shader binaries from source folder...
echo ----------------------------------------------------------------------------

REM Delete .spv files from shaders folder (already copied to build folders)
if exist "shaders\*.spv" (
    del /Q "shaders\*.spv"
    echo Deleted .spv files from shaders\ folder.
) else (
    echo No .spv files to clean up.
)
echo Shader sources preserved in shaders\ folder.
echo.

REM ============================================================================
REM STEP 5: Generate Doxygen documentation
REM ============================================================================
echo [5/5] Generating Doxygen documentation...
echo ----------------------------------------------------------------------------

REM Check if doxygen is available
where doxygen >nul 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo WARNING: Doxygen not found!
    echo Skipping documentation generation.
    echo To generate docs, install Doxygen: https://www.doxygen.nl/download.html
    goto :skip_doxygen
)

REM Check if Doxyfile exists (generated by CMake from Doxyfile.in)
if not exist "build\Doxyfile" (
    echo WARNING: Doxyfile not found!
    echo Skipping documentation generation.
    goto :skip_doxygen
)

doxygen build\Doxyfile
if %ERRORLEVEL% NEQ 0 (
    echo WARNING: Doxygen generation had issues, but continuing...
) else (
    echo [5/5] Documentation generated successfully!
)
:skip_doxygen
echo.

REM ============================================================================
REM SUMMARY
REM ============================================================================
echo ============================================================================
echo                        BUILD PIPELINE COMPLETE
echo ============================================================================
echo.
echo Executables:
echo   Debug:   build\Debug\wild.exe
echo   Release: build\Release\wild.exe
echo.
echo Assets copied to:
echo   - build\Debug\assets
echo   - build\Release\assets
echo.
echo Shader binaries (.spv) copied to:
echo   - build\Debug\shaders
echo   - build\Release\shaders
echo.
echo Save files (save*.json) copied from project root to:
echo   - build\Debug\
echo   - build\Release\
echo.
echo Documentation:
echo   - docs\html\index.html (if Doxygen was available)
echo.
echo ============================================================================

endlocal
exit /b 0

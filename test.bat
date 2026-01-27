@echo off
setlocal

echo === Wild Engine Tests ===
echo.

:: Check if vcpkg is available
if defined VCPKG_ROOT (
    echo Using vcpkg from: %VCPKG_ROOT%
    set TOOLCHAIN=-DCMAKE_TOOLCHAIN_FILE=%VCPKG_ROOT%/scripts/buildsystems/vcpkg.cmake
) else if exist "%~dp0vcpkg\scripts\buildsystems\vcpkg.cmake" (
    echo Using vcpkg from project directory
    set TOOLCHAIN=-DCMAKE_TOOLCHAIN_FILE=%~dp0vcpkg/scripts/buildsystems/vcpkg.cmake
) else (
    echo Warning: vcpkg not found, using bundled dependencies from external/
    set TOOLCHAIN=
)

:: Create build directory for tests
if not exist "build_tests" mkdir build_tests
cd build_tests

:: Configure with CMake
echo.
echo Configuring CMake...
cmake .. -DBUILD_TESTS=ON %TOOLCHAIN%
if errorlevel 1 (
    echo CMake configuration failed!
    cd ..
    pause
    exit /b 1
)

:: Build tests
echo.
echo Building tests...
cmake --build . --config Release --target wild_tests
if errorlevel 1 (
    echo Build failed!
    cd ..
    pause
    exit /b 1
)

:: Run tests
echo.
echo Running tests...
echo =====================================
if exist "Release\wild_tests.exe" (
    Release\wild_tests.exe --gtest_color=yes
) else if exist "wild_tests.exe" (
    wild_tests.exe --gtest_color=yes
) else (
    echo Error: Test executable not found!
    cd ..
    pause
    exit /b 1
)

set TEST_RESULT=%errorlevel%
cd ..

echo.
if %TEST_RESULT%==0 (
    echo All tests passed!
) else (
    echo Some tests failed!
)

pause
exit /b %TEST_RESULT%

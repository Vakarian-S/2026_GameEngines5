@echo off
setlocal
REM Builds and runs MemoryMonitorTests and EcsTests in every configuration.
REM
REM Each suite is its own program. The monitor's tests make deliberate mistakes (double free,
REM writing before an allocation) and that output does not belong in the demo. The ECS tests
REM moved out of Main.cpp once Main had to become a real program rather than a test runner.
REM
REM   scripts\run-tests.bat            all four configurations
REM   scripts\run-tests.bat x64 Debug  just one

set "PROJ=%~dp0.."

REM cmake --preset and ctest --preset read CMakePresets.json from the current directory,
REM so run from the project root whatever directory the script was called from. setlocal
REM puts the old one back on the way out.
cd /d "%PROJ%"

where cmake >nul 2>&1
if errorlevel 1 (
    echo Could not find cmake on PATH. Install CMake, or open a Developer Prompt.
    exit /b 1
)

set FAILED=0

if "%~1"=="" (
    call :run x64   Debug
    call :run x64   Release
    call :run Win32 Debug
    call :run Win32 Release
) else (
    call :run %1 %2
)

echo.
if %FAILED%==0 (echo ALL TEST RUNS PASSED) else (echo SOME TEST RUNS FAILED)
exit /b %FAILED%

:run
set "PLAT=%~1"
set "CFG=%~2"
set "TREE=%PROJ%\build\cmake-%PLAT%"

echo.
echo ================= %CFG%-%PLAT% =================

REM One build tree per platform, each holding both configurations. Written once and reused.
REM The preset names are the platform names, and CMakePresets.json holds the generator, the
REM toolset and the build directory, so an IDE configures the same way this script does.
if exist "%TREE%\CMakeCache.txt" goto :configured

cmake --preset %PLAT%
set "RC=%errorlevel%"
if not "%RC%"=="0" (
    echo   CONFIGURE FAILED
    set FAILED=1
    goto :eof
)

:configured

cmake --build --preset %PLAT%-%CFG% --target MemoryMonitorTests EcsTests
set "RC=%errorlevel%"
if not "%RC%"=="0" (
    echo   BUILD FAILED
    set FAILED=1
    goto :eof
)

REM The test preset asks for verbose output, so the checks and their counts are printed
REM rather than just "Passed".
REM
REM CTest is what makes the exit code safe here. The tests return a large negative number
REM when they crash, and "if errorlevel 1" means "1 or more", so the old hand written check
REM had to compare against 0 instead. CTest treats a crash as a failure by itself.
ctest --preset %PLAT%-%CFG%
set "RC=%errorlevel%"
if not "%RC%"=="0" (
    echo   ^<-- %CFG%-%PLAT% did not finish cleanly, exit %RC%
    set FAILED=1
)
goto :eof

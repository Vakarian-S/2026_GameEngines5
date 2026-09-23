@echo off
setlocal
REM Builds and runs MemoryMonitorTests in every configuration.
REM
REM The tests are their own program on purpose: they make deliberate mistakes (double free,
REM writing before an allocation) and that output does not belong in the demo.
REM
REM   scripts\run-tests.bat            all four configurations
REM   scripts\run-tests.bat x64 Debug  just one

set "PROJ=%~dp0.."

REM The generator and the toolset are pinned. This machine has two sets of Build Tools
REM installed, and the saved snapshots were built with v143.
set "GENERATOR=Visual Studio 17 2022"
set "TOOLSET=v143"

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
if exist "%TREE%\CMakeCache.txt" goto :configured

cmake -S "%PROJ%" -B "%TREE%" -G "%GENERATOR%" -A %PLAT% -T %TOOLSET%
set "RC=%errorlevel%"
if not "%RC%"=="0" (
    echo   CONFIGURE FAILED
    set FAILED=1
    goto :eof
)

:configured

cmake --build "%TREE%" --config %CFG% --target MemoryMonitorTests
set "RC=%errorlevel%"
if not "%RC%"=="0" (
    echo   BUILD FAILED
    set FAILED=1
    goto :eof
)

REM -V so the checks and their counts are printed, not just "Passed".
REM
REM CTest is what makes the exit code safe here. The tests return a large negative number
REM when they crash, and "if errorlevel 1" means "1 or more", so the old hand written check
REM had to compare against 0 instead. CTest treats a crash as a failure by itself.
ctest --test-dir "%TREE%" -C %CFG% -V
set "RC=%errorlevel%"
if not "%RC%"=="0" (
    echo   ^<-- %CFG%-%PLAT% did not finish cleanly, exit %RC%
    set FAILED=1
)
goto :eof

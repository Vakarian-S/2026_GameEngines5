@echo off
setlocal
REM Builds and runs tests\MemoryMonitorTests.cpp against src\utils\MemoryMonitor.cpp.
REM
REM The tests are their own program on purpose: they make deliberate mistakes (double free,
REM writing before an allocation) and that output does not belong in the demo in Main.cpp.
REM
REM   scripts\run-tests.bat            all four configurations
REM   scripts\run-tests.bat x64 Debug  just one

set "VCROOT=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build"
set "PROJ=%~dp0.."
set "OUTROOT=%PROJ%\baselines\tests"

if not exist "%OUTROOT%" mkdir "%OUTROOT%"

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
set "NAME=%CFG%-%PLAT%"

if /i "%PLAT%"=="x64" (set "VCVARS=vcvars64.bat") else (set "VCVARS=vcvars32.bat")
if /i "%CFG%"=="Debug" (set "CRT=/MDd /D_DEBUG /Od") else (set "CRT=/MD /DNDEBUG /O2")

echo.
echo ================= %NAME% =================

set "WORK=%OUTROOT%\%NAME%"
if not exist "%WORK%" mkdir "%WORK%"
pushd "%WORK%"

call "%VCROOT%\%VCVARS%" >nul 2>&1
cl /nologo /std:c++20 /EHsc /W3 %CRT% /I"%PROJ%\include" ^
   "%PROJ%\tests\MemoryMonitorTests.cpp" "%PROJ%\src\utils\MemoryMonitor.cpp" ^
   /Fe:tests.exe > build.log 2>&1

if errorlevel 1 (
    echo   BUILD FAILED, see %WORK%\build.log
    type build.log
    set FAILED=1
    popd
    goto :eof
)

.\tests.exe
if errorlevel 1 (
    echo   ^<-- test failures in %NAME%
    set FAILED=1
)
popd
goto :eof

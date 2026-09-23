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

REM vswhere ships with every Visual Studio since 2017, so this finds MSBuild whether the
REM machine has the Build Tools or the full Visual Studio, and whatever year it is.
REM
REM -products * matters: without it vswhere skips Build Tools installs and finds nothing
REM on a machine that has no full Visual Studio.
REM
REM The answer goes through a file on the way. Reading it straight out of a for /f
REM backtick would need the quotes around the vswhere path escaped, which is easy
REM to get wrong, and silently finds nothing when it is.
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
set "FOUND=%TEMP%\GameEngines5-msbuild-path.txt"
set "MSBUILD="
if exist "%VSWHERE%" (
    "%VSWHERE%" -products * -latest -requires Microsoft.Component.MSBuild -find MSBuild\**\Bin\MSBuild.exe > "%FOUND%" 2>nul
    for /f "usebackq delims=" %%i in ("%FOUND%") do set "MSBUILD=%%i"
    del "%FOUND%" >nul 2>&1
)

if not defined MSBUILD (
    echo Could not find MSBuild. Is Visual Studio or the Build Tools installed?
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

echo.
echo ================= %CFG%-%PLAT% =================

"%MSBUILD%" "%PROJ%\MemoryMonitorTests.vcxproj" -t:Build ^
    -p:Configuration=%CFG% -p:Platform=%PLAT% -v:minimal -nologo
set "RC=%errorlevel%"
if not "%RC%"=="0" (
    echo   BUILD FAILED
    set FAILED=1
    goto :eof
)

"%PROJ%\bin\%PLAT%\%CFG%\MemoryMonitorTests.exe"
REM
REM Compared against 0, not with "if errorlevel 1". A crash comes back as a large
REM negative number, and "errorlevel 1" means "1 or more", so that form would call
REM a crashed run a pass.
set "RC=%errorlevel%"
if not "%RC%"=="0" (
    echo   ^<-- %CFG%-%PLAT% did not finish cleanly, exit %RC%
    set FAILED=1
)
goto :eof

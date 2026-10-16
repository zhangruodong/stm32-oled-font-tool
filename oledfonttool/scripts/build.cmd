@echo off
rem Build the OLED font tool.
rem
rem This is the portable entry point: unlike BUILD.lnk (which stores one
rem absolute path and only works on the machine that created it), this file
rem derives everything from its own location, so it works for anyone who
rem clones the repository.
rem
rem Double-click it, or run it from a terminal.
rem
rem Pure ASCII on purpose: cmd.exe parses .cmd using the system ANSI codepage.

setlocal
set "HERE=%~dp0"
set "PS=%SystemRoot%\System32\WindowsPowerShell\v1.0\powershell.exe"
set "SCRIPT=%HERE%build.ps1"

echo ============================================================
echo  Build: OLED font tool
echo ============================================================
echo.

if not exist "%SCRIPT%" (
    echo [ERROR] build.ps1 not found next to this file:
    echo         %SCRIPT%
    echo.
    pause
    exit /b 1
)

if not exist "%PS%" (
    echo [ERROR] Windows PowerShell not found:
    echo         %PS%
    echo.
    pause
    exit /b 1
)

rem -Wait keeps the console open at the end so the result stays readable.
"%PS%" -NoProfile -ExecutionPolicy Bypass -File "%SCRIPT%" -Wait

if errorlevel 1 (
    echo.
    echo [FAILED] build reported an error - see build-log.txt
    echo.
    pause
    exit /b 1
)

endlocal
exit /b 0

@echo off
rem Copy OLED_CFont.h into a Keil project's Hardware folder.
rem
rem The project is located by searching upward from this script for
rem     MyFreeRTOS_C8T6\Hardware
rem so the tool keeps working no matter where it is installed; it no longer
rem assumes the project sits at one fixed relative path.
rem
rem This file must stay PURE ASCII with no non-ASCII path literals:
rem cmd.exe parses .cmd using the system ANSI codepage.

setlocal enabledelayedexpansion
set "HERE=%~dp0"
set "BUILD=%HERE%..\build"

echo ============================================================
echo  Install OLED_CFont.h into the Keil project
echo ============================================================
echo.

rem ---- 1. walk upward looking for MyFreeRTOS_C8T6\Hardware ----
set "PROBE=%HERE%"
set "HW="
for /l %%i in (1,1,8) do (
    if not defined HW (
        if exist "!PROBE!MyFreeRTOS_C8T6\Hardware\" (
            set "HW=!PROBE!MyFreeRTOS_C8T6\Hardware"
        ) else (
            pushd "!PROBE!.." 2>nul
            if errorlevel 1 (
                rem reached the drive root, stop climbing
            ) else (
                set "PROBE=!CD!\"
                popd
            )
        )
    )
)

if not defined HW (
    echo [ERROR] Could not find the Keil project folder.
    echo.
    echo  Searched upward from this script for:
    echo      MyFreeRTOS_C8T6\Hardware
    echo.
    echo  If your project uses a different folder name, just copy
    echo  OLED_CFont.h into its Hardware folder by hand -- it is one file.
    echo.
    pause
    exit /b 1
)

echo  project found: %HW%
echo.

rem ---- 2. locate the font file to install ----
set "SRC="
if exist "%BUILD%\OLED_CFont.h" (
    set "SRC=%BUILD%\OLED_CFont.h"
    echo  source: Qt tool export  ^(oledfonttool\build^)
) else if exist "%HERE%OLED_CFont.h" (
    set "SRC=%HERE%OLED_CFont.h"
    echo  source: Python generator  ^(oledfonttool\src^)
)

if not defined SRC (
    echo [ERROR] no OLED_CFont.h found in either place:
    echo         %BUILD%\OLED_CFont.h
    echo         %HERE%OLED_CFont.h
    echo.
    echo  Export it from the Qt tool, or run build_cfont.py first.
    echo.
    pause
    exit /b 1
)

rem ---- 3. install, keeping a backup of the old file ----
if exist "%HW%\OLED_CFont.h" (
    copy /y "%HW%\OLED_CFont.h" "%HERE%OLED_CFont.old.h" >nul
    echo  old file backed up as oledfonttool\src\OLED_CFont.old.h
)

copy /y "%SRC%" "%HW%\OLED_CFont.h" >nul
if errorlevel 1 (
    echo [FAILED] copy failed - the target folder is not writable.
    echo.
    pause
    exit /b 1
)

echo [OK] installed to: %HW%\OLED_CFont.h
echo      Rebuild the Keil project to take effect.
echo.
pause
endlocal
exit /b 0

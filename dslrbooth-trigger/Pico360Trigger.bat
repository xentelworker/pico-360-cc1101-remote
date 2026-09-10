@echo off
setlocal EnableExtensions EnableDelayedExpansion

rem ============================================================
rem Pico360Trigger.bat
rem Called directly by LumaBooth for Windows (dslrBooth)
rem Settings > General > Triggers > Application/Script
rem Enter ONLY the full path to this BAT file in dslrBooth.
rem
rem dslrBooth calls:
rem   Pico360Trigger.bat event_type param1 param2 ...
rem ============================================================

set "SCRIPT_DIR=%~dp0"
set "CONFIG=%SCRIPT_DIR%pico360-config.txt"
set "LOG=%SCRIPT_DIR%Pico360Trigger.log"

rem Defaults. pico360-config.txt can override these.
set "COMPORT=COM7"
set "BAUD=115200"
set "COUNTDOWN_SECONDS=10"
set "LOGGING=1"

if exist "%CONFIG%" (
  for /f "usebackq tokens=1,* delims==" %%A in ("%CONFIG%") do (
    if not "%%A"=="" set "%%A=%%B"
  )
)

set "EVENT=%~1"
set "PARAM1=%~2"
set "PARAM2=%~3"
set "PARAM3=%~4"
set "PARAM4=%~5"

if not defined EVENT exit /b 0

if "%LOGGING%"=="1" echo [%date% %time%] EVENT %EVENT% %PARAM1% %PARAM2% %PARAM3% %PARAM4% >> "%LOG%"

set "COMMAND="

if /I "%EVENT%"=="session_start"     set "COMMAND=DSLR_SESSION_START"
if /I "%EVENT%"=="countdown_start"   set "COMMAND=DSLR_COUNTDOWN %PARAM1%"

rem dslrBooth sends countdown progress as percent complete.
rem Convert it to seconds remaining for the Pico OLED.
if /I "%EVENT%"=="countdown" (
  set /a "PCT=%PARAM1%" >nul 2>&1
  if !PCT! LSS 0 set "PCT=0"
  if !PCT! GTR 100 set "PCT=100"
  set /a "REMAINING=(COUNTDOWN_SECONDS*(100-PCT)+99)/100" >nul 2>&1
  if !REMAINING! LSS 0 set "REMAINING=0"
  set "COMMAND=DSLR_COUNTDOWN !REMAINING!"
)

if /I "%EVENT%"=="capture_start"     set "COMMAND=DSLR_GO"
if /I "%EVENT%"=="processing_start"  set "COMMAND=DSLR_PROCESSING"
if /I "%EVENT%"=="sharing_screen"    set "COMMAND=DSLR_SHARING"
if /I "%EVENT%"=="session_end"       set "COMMAND=DSLR_SESSION_END"

rem The current Pico firmware does not need file_download, printing or
rem file_upload to drive the main guest-facing OLED lifecycle. They are
rem logged above and intentionally ignored here.
if not defined COMMAND exit /b 0

mode %COMPORT% BAUD=%BAUD% PARITY=N DATA=8 STOP=1 >nul 2>&1
if errorlevel 1 (
  if "%LOGGING%"=="1" echo [%date% %time%] ERROR cannot configure %COMPORT% >> "%LOG%"
  exit /b 2
)

rem Win32 device path supports COM10 and higher as well as COM1-COM9.
>"\\.\%COMPORT%" echo %COMMAND%
if errorlevel 1 (
  if "%LOGGING%"=="1" echo [%date% %time%] ERROR cannot write to %COMPORT% >> "%LOG%"
  exit /b 3
)

if "%LOGGING%"=="1" echo [%date% %time%] SENT %COMPORT% : %COMMAND% >> "%LOG%"
exit /b 0

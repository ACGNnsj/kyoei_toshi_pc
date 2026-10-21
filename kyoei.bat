@echo off
rem City Shrouded in Shadow (Kyoei Toshi): start the game from this source tree.
rem Needs out\vwport.exe (bash build.sh in the MSYS2 CLANG64 shell) and Python on PATH.
chcp 65001 >nul
cd /d "%~dp0"
set "VW_GAME_DIR=D:\_PS4_PC\Kyoei Toshi(巨影都市) PC\CUSA03562"
set "VW_DATA_DIR=%~dp0run_kyoei"
set "VW_UPDATES=0"
if not exist "out\vwport.exe" (
    echo out\vwport.exe is missing: build it first ^(bash build.sh in MSYS2 CLANG64^).
    pause
    exit /b 1
)
python run.py
echo.
echo Exit code %ERRORLEVEL%. The log is the newest file in run_kyoei\logs.
pause

@echo off
rem City Shrouded in Shadow (Kyoei Toshi): start the game from this source tree.
rem Needs out\vwport.exe (bash build.sh in the MSYS2 CLANG64 shell) and Python on PATH.
chcp 65001 >nul
cd /d "%~dp0"
set "VW_GAME_DIR=D:\_PS4_PC\Kyoei Toshi(巨影都市) PC\CUSA03562"
set "VW_DATA_DIR=%~dp0run_kyoei"
set "VW_UPDATES=0"
rem Frame rate cap: 15, near the PS4 release (higher rates crash more often, see HANDOFF). 0 = no cap.
set "VW_FPS_LIMIT=15"
rem Test: GPU results in small (width 8 or less) or linear render targets written back for the
rem CPU, as for an autofocus depth probe (the blur in free movement, the menu background).
set "VW_READBACK_LINEAR=1"
rem Test: Precise buffer readbacks (2): a CPU read of a buffer the GPU wrote waits for the GPU's
rem data; Relaxed (1, the default) only on CPU writes (save icons read a GPU-made capture).
set "VW_READBACKS=2"
if not exist "out\vwport.exe" (
    echo out\vwport.exe is missing: build it first ^(bash build.sh in MSYS2 CLANG64^).
    pause
    exit /b 1
)
python run.py
echo.
echo Exit code %ERRORLEVEL%. The log is the newest file in run_kyoei\logs.
pause

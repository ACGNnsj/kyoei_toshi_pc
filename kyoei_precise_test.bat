@echo off
rem City Shrouded in Shadow (Kyoei Toshi): start the game from this source tree.
rem Needs out\vwport.exe (bash build.sh in the MSYS2 CLANG64 shell) and Python on PATH.
chcp 65001 >nul
cd /d "%~dp0"
set "VW_GAME_DIR=D:\_PS4_PC\Kyoei Toshi(巨影都市) PC\CUSA03562"
set "VW_DATA_DIR=%~dp0run_kyoei"
set "VW_UPDATES=0"
rem Frame rate cap. At 30 without PS4 scheduling two game threads wrote one draw context at
rem once (corrupted command buffers, see HANDOFF); 15 was stable. 0 = no cap.
set "VW_FPS_LIMIT=30"
rem Test: the game's thread priorities and core pinning applied as the PS4's strict priority
rem scheduler would (threads sharing a core never interleave). 0 = off.
set "VW_PS4_SCHEDULING=1"
rem Diagnostics: F11 writes this frame's passes, draws and shaders to run_kyoei\user\captures.
set "VW_FRAME_CAPTURE_KEY=1"
set "VW_CAPTURE_DETAIL=1"
rem Display passes are rare in this game: 120 of them took 80 s and 250 MB; 6 keeps F11 short.
set "VW_CAPTURE_FRAMES=6"
rem Live switches for GPU optimizations: the scripts in toggles\ write this file while the game
rem runs (re-read every 0.25 s). Every start begins with all of them on.
set "VW_TOGGLE_FILE=%~dp0run_kyoei\toggles.txt"
>"%VW_TOGGLE_FILE%" echo 0
rem Test: Precise buffer readbacks (the CPU sees GPU-written buffers as they are now). The boot
rem hang at about 3 s was a copy thread deadlocked with the GPU thread (fix untested). If the
rem game stops, every guest thread's location is printed every 5 s.
set "VW_READBACKS=2"
set "VW_HANG_DUMP=5"
rem Each GPU-written range copied back over game memory, and whether it replaced newer data.
set "VW_LOG_DOWNLOADS=1"
if not exist "out\vwport.exe" (
    echo out\vwport.exe is missing: build it first ^(bash build.sh in MSYS2 CLANG64^).
    pause
    exit /b 1
)
python run.py
echo.
echo Exit code %ERRORLEVEL%. The log is the newest file in run_kyoei\logs.
pause

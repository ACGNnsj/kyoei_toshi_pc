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
rem Test: when the game faults or a GPU command buffer is corrupt, list the file reads
rem (VW_READ_LOG) and the GPU's writes to game memory (VW_WRITE_LOG) near the damaged data.
rem The vertex explosions stay at 15 FPS and after a reload: likely not a race but data
rem that is wrong from the start.
set "VW_READ_LOG=1"
set "VW_WRITE_LOG=1"
rem A copy of each submitted command buffer: a corrupt one is compared with what the game
rem submitted (did the game write it again before the GPU thread read it?).
set "VW_DCB_CHECK=1"
rem Which game thread writes each block of command buffer memory: two threads writing the
rem same block at once are printed (the first 20, then a running count). 2 also records each
rem write's game call stack, which halves the frame rate.
set "VW_CB_THREADS=1"
rem When the game faults in its mesh walk (0x4753c0): the object, its index list and where its
rem element array really ends.
set "VW_FAULT_MESH=1"
rem Condition variables now wake a wait only for a signal (the game counts every wake as a
rem job token). VW_HOST_COND=1 restores the plain host condition variables, for comparison.
set "VW_HOST_COND=0"
if not exist "out\vwport.exe" (
    echo out\vwport.exe is missing: build it first ^(bash build.sh in MSYS2 CLANG64^).
    pause
    exit /b 1
)
python run.py
echo.
echo Exit code %ERRORLEVEL%. The log is the newest file in run_kyoei\logs.
pause

@echo off
rem City Shrouded in Shadow (Kyoei Toshi): start the game from this source tree.
rem Needs out\vwport.exe (bash build.sh in the MSYS2 CLANG64 shell) and Python on PATH.
chcp 65001 >nul
cd /d "%~dp0"
set "VW_GAME_DIR=D:\_PS4_PC\Kyoei Toshi(巨影都市) PC\CUSA03562"
set "VW_DATA_DIR=%~dp0run_kyoei"
set "VW_UPDATES=0"
rem Frame rate cap: 30 (15 was the PS4's rate in many scenes; the crashes higher rates exposed are
rem fixed or patched, see HANDOFF). 15 if it crashes, 0 = no cap.
set "VW_FPS_LIMIT=30"
rem Test: GPU results in small (width 8 or less) or linear render targets written back for the
rem CPU, as for an autofocus depth probe (the blur in free movement, the menu background).
set "VW_READBACK_LINEAR=1"
rem Test: one GPU stage (draws recorded where the commands are read), so a frame's guest data
rem is consumed before the game sees it done (fast-forward crash, white wedges).
set "VW_DRAW_PIPE=0"
rem Diagnostics: the GPU emulation's writes to game memory, listed when a command buffer turns
rem out to hold garbage (the fast-forward crash).
set "VW_WRITE_LOG=1"
rem Diagnostics: F11 writes this frame's passes, draws and shaders to run_kyoei\user\captures.
set "VW_FRAME_CAPTURE_KEY=1"
set "VW_CAPTURE_DETAIL=1"
if not exist "out\vwport.exe" (
    echo out\vwport.exe is missing: build it first ^(bash build.sh in MSYS2 CLANG64^).
    pause
    exit /b 1
)
python run.py
echo.
echo Exit code %ERRORLEVEL%. The log is the newest file in run_kyoei\logs.
pause

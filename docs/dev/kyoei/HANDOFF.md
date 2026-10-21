# City Shrouded in Shadow (巨影都市, CUSA03562) — handoff

Branch `kyoei/bringup` of this clone, on top of Unicorn Overlord PC `88404b6`. Same port
design: the eboot runs natively, `src/runtime_*.c` replaces the system libraries, shadPS4's
video core translates the GPU.

## The dump

Japanese release CUSA03562, `APP_VER` 01.00 (param.sfo `VERSION` 01.03 is another field),
SDK 4.50. Data in `DAT/*.psarc` (the game reads them itself), one `StageModule.prx` per stage
in `module/MasterORBIS/stage*/`, `libSceSecure.prx` in `module/prx/`.

## State (2026-10-21, Windows 11, RTX 3090)

- `vwport --check-imports`: **0 missing** (709 imports reach the host runtime).
- Boots: Bandai Namco logo, title flow (save data search, a MsgDialog answered OK), loads
  stage 201 through `sceKernelLoadStartModule`, plays the prologue cutscenes and reaches the
  name entry (IME dialog). The 3D scenes render correctly in frame dumps (rain, neon, characters,
  Japanese text). No GPU errors in the log.
- Not played yet: name entry and anything after it, input, audio quality, movies, saving,
  a stage change (unload of one stage module and load of another), frame rate.

## What the port needed

| Change | Where |
|---|---|
| Game registration (`kyoei` launcher, `CUSA03562` profile, updater off) | `launcher/vw_launcher.py`, `launcher/kyoei_launcher.py`, `scripts/game_profiles.py` |
| Bundled modules also from `module/prx/` (libSceSecure linked natively) | `scripts/link_modules.py` (`module_path`) |
| Older SDK libc: plain symbol names (`__start__Ztext`) | `scripts/link_libc.py` |
| Stage modules: placed and relocated offline, `out/stages.bin`; the runtime restores a module's writable segments, runs its init and points the eboot's `6rNUbZHCWM0#StageModule` import (a jump through `vw_stage_target`) at the loaded module's export | `scripts/link_modules.py`, `src/runtime_stage.c`, `src/probe.c` |
| Up to 256 image segments (19 stage modules add 57) | `src/probe.c` |
| VR, camera, Move, capture, streaming, Remote/Share Play, Razor, PNG encoder | `src/runtime_peripherals.c` (new) |
| PlayGo, HMD setup, custom data and Facebook dialogs | `src/runtime_dialogs.c` |
| sce::Json Array, Value(Array/String), set(Object), clear, notifyError | `src/runtime_json.c` |
| SaveData backup/progress, AppContent add-ons, PlayGo ETA/to-do, pad handle, sleep, POSIX sockets, NetCtl, NpSns | the matching `runtime_*.c` |
| 125 NID names | `src/import_names.inc` |

Return values and error codes of the new stubs follow shadPS4's libraries (hmd, vr_tracker,
camera, move, pngenc, ...). PngEnc writes uncompressed PNGs: a game buffer sized for compressed
output may get `DATA_OVERFLOW`.

## Running

MSYS2 CLANG64 shell: `bash build.sh`, then (any shell)

```
VW_GAME_DIR="D:/_PS4_PC/Kyoei Toshi(巨影都市) PC/CUSA03562" VW_DATA_DIR=run_kyoei VW_UPDATES=0 python run.py
```

Frame dumps: `VW_FRAME_DUMP_TRIGGER=<file> VW_DUMP_DIR=<dir>`, touch the file, convert with
`python tools/raw2png.py <dir>/output_*.raw`.

## Next

1. Play with a controller/keyboard past the name entry (the IME dialog takes keyboard text).
2. Watch the log for `sceKernelStopUnloadModule` and a second `LoadStartModule` at the first
   stage change; check the stage plays after it.
3. Audio (the UO audio downmix work applies), movies (`finalizedPS4/movie`), saving and loading.
4. Performance with `VW_FRAME_STATS=1 VW_VSYNC=0`; compare with bloodborne_pc's renderer work
   for 3D-specific problems if they show up.

## Open: crashes in the prologue (2026-10-21)

With a warm shader cache the game faults within 45-145 s, in different places each run, with or
without input (skipping the cutscene with Circle makes it earlier):

| Run | Where | What |
|---|---|---|
| skip | game `0x23c53b` (fn `0x23be90`) | iterating the list at `this+0x228` (count `+0x230`): an entry is NULL |
| skip | game `0x45ec82` | NULL+0xaa; same caller chain `0x372def` → `0x23cfee` |
| no input | host `0x07246cc3` (not a module) | NULL+0x30 on a host thread, after `StopRtdSkip()` |

**Cause found for the 0x23c53b fault:** the port's deferred/stale GPU readbacks (CPU reads of
GPU-written memory get the previous frame's copy, on by default for Unicorn Overlord). With both
off the scripted cutscene skip passed twice without a fault where it had faulted every time
(2026-10-21); the profile now forces them off (`forced` in `scripts/game_profiles.py`).
Still to check with them off: the 0x45ec82 and host faults, the blur that stays after the
mission title, the menu's black background with colored garbage, the fault after staying in
the menu (player's report, 0x23c53b with readbacks on).

**Cause found for the host faults (NULL+0x30 at 0x07xxxxxx):** that code is a shader's SRT
walker (`flatten_extended_userdata_pass.cpp`, xbyak). A walker reading a pointer the game left
NULL is meant to fault into `SrtWalkerSignalHandler`, which zeroes the load; the handler was only
registered by `GenerateSrtProgram`, and with a warm pipeline cache every walker comes from
`RegisterWalkerCode`, so it was never registered. Now registered by both (log: `GPU: SRT walker
fault handler registered`; each catch logs `Patched SRT walker`). Upstream shadPS4 has the same gap.

The first cold-cache runs (slower) got past these points. Enforcing the game's thread
affinity (each PS4 core on one host CPU) did not help: it faulted earlier. Only tested on an
NVIDIA RTX 3090; the port has mostly run on AMD. Next: name the faulting host thread, check
whether the host fault is in the NVIDIA driver, and try `VW_PIPELINE_BINARIES=0` and the GPU
readback options (deferred/stale readbacks may hand the game data a frame late).

## Frame rate and the remaining faults (2026-10-22)

The game sets flip rate 0 every frame and paces itself; on this PC it runs at 60. The PS4
release ran at about 15-20 FPS in many scenes (the owner remembers it, and reviews criticised
it). Same scripted run (new game, cutscene skipped), faults by frame cap (`VW_FPS_LIMIT`):
60 FPS about 1 in 2, 30 FPS 1 in 3 (0x45f4d2), 15 FPS 0 in 3. Game time follows real time
(the name entry comes at ~160 s at 15 and 30 FPS).

So the remaining NULL-resource faults (0x45ec82, 0x45f4d2, the vtable call to 0x1) may be the
game's own races, hidden on a PS4 by its low frame rate, rather than a runtime primitive. A
frame cap is a legitimate fix for that; the best default (20? 30?) needs more runs. A patch of
the game's code would be the alternative for higher rates. `kyoei.bat` caps at 30.

## Confirmed by the owner (2026-10-22)

- Name entry: the Japanese prompt and typed Japanese names show correctly with the merged CJK
  font (`f43a261`).
- `kyoei.bat` caps at 15 FPS for play testing (`f258c8f`); still open from earlier play: the blur
  after the mission title, the menu's black background with colored garbage.

## Play test, 2026-10-22 (owner, 15 FPS)

- Fixed and confirmed: the blur in free movement and the black menu background
  (`VW_READBACK_LINEAR=1`, now forced by the profile); no fault when an NPC hands over an item
  (GPU stall timeout patch, `patches/Kyoei_Toshi.xml`); saving at a save point; loading from the
  title; a stage unload and reload (DT_FINI now called with a NULL entry).
- Save icons: garbage. The game encodes a 228x128 capture from a GPU-written buffer (pitch 960,
  0x80-aligned); with Relaxed buffer readbacks a CPU read gets stale data. `kyoei.bat` tests
  `VW_READBACKS=2` (Precise); not confirmed yet.
- A flicker while the save list was open happened once, not again.

## Save icons: deferred (2026-10-22, owner's choice)

Still garbage after `vwgpu_read_back` (8721da4): the encoder's source (a heap buffer, pitch 960,
different address each save) is not GPU-modified in the buffer cache. Likely the game reads a
tiled render target on the CPU (thread 0x5d93b0 only swaps R/B and applies gamma; the job is
filled near 0xb4ee30), and the texture cache writes back only linear or narrow images. A fix
needs tiled write-back (re-tiling a downloaded image into guest memory). Saves themselves load
correctly. Fallback idea: build the icon from the last presented frame.

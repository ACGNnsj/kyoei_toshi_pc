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

The first cold-cache runs (slower) got past these points. Enforcing the game's thread
affinity (each PS4 core on one host CPU) did not help: it faulted earlier. Only tested on an
NVIDIA RTX 3090; the port has mostly run on AMD. Next: name the faulting host thread, check
whether the host fault is in the NVIDIA driver, and try `VW_PIPELINE_BINARIES=0` and the GPU
readback options (deferred/stale readbacks may hand the game data a frame late).

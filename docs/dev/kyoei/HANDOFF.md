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

## Confirmed 2026-10-22 (owner)

- Hair and eyelashes render correctly with `DB_ALPHA_TO_MASK` honored (`34dba1d`); they were
  opaque blocks.
- Save icons: wrong for the first saves of a session, right for later ones (four saves: two of
  each, none read back by `vwgpu_read_back`). Still deferred.
- No crash in a long play session at 15 FPS.

## Game patches (patches/Kyoei_Toshi.xml), 2026-10-22

| Patch | Fault it removes |
|---|---|
| No GPU Stall Timeout | the game's own int 0x41 after 10M spins on a GPU label (item handed over) |
| Skip Constants For Unbound Shader | NULL shader in the seven per-stage data writers 0x45eaa0-0x45f3c0 (QTE, Continue) |
| Skip Draw State For Unbound Shader | NULL shader at the seven per-stage blocks of the draw-state commit FUN_0045d060 (cutscene skip, 0x45dc7d) |
| Skip Unloaded NPC List Entries | NULL entries of the NPC list: three reads in FUN_0023be90 and the same three in its two copies (0x238xxx, 0x23bxxx); faults at 0x23c53b, 0x23cd42, 0x2386eb (stage loads at 30 FPS) |

The readback change made 0x23c53b rarer but did not remove it: these are the game's races,
exposed by the PC's speed. Patches may also write the rest of a segment's last page
(`patches.py` eboot_segments, `probe.c` patchable), and the loader maps the executable segment up
to the next one (`prepare.load_end`): 0x1249f6c-0x124c000 (8 KiB) is a code cave; 0x1249f70-0x1249fd9
and 0x124a000-0x124a0c1 are used.
Still unguarded: reads of ctx+0xc1b8 (stage 7) in the writers when ctx+0xc613 is set, and
0x45d62a (stage 5 at +0xa8). The third patch uses 0x433131-0x433191 (of 0x433131-0x4331bb) as a code cave: the stall
path the first patch makes dead (its jmp is repeated in the third so it stands alone).

## Root cause of the random crashes (2026-10-22)

With `VW_DRAW_PIPE=0` and only tiny images written back, the fast-forward crash came back as a
corrupted command buffer: `PM4: bad packet` with `VW_WRITE_LOG=1` showed **no** write by the
emulation near it (0 of 1.7M logged writes), and the words show two packets overlapping
(`c0016900 2aa ff` written over the first three dwords of a `c0027600 4e lo hi`, leaving `hi`).
Two game threads write one draw context at once. The same explains the NULL shader seen
mid-block (0x45ddf5), the NULL NPC list entries and the 1.0f index at 0x49fdd4: the NULL
patches only stop symptoms. The runtime's mutex, cond, sema and rwlock match PS4 semantics
(reviewed). At 15 FPS the owner played long sessions without a crash; the PS4 ran this game at
about 15-20 FPS. `kyoei.bat` is back at 15 FPS with the draw pipeline on.

Next for a real fix: find the two threads (a guard at the draw-state entry points that
records the owning thread per context and prints both stacks on overlap), then serialize them.

- `VW_READBACK_LINEAR=2` (profile): blur and menu background still right; 1 is not needed.
- `VW_DRAW_PIPE=0` changed nothing for the crashes or the white wedges and lengthened loads.
- F11 captures with the draw pipeline off held only the last two passes (present, clear): the
  capture tool needs a look before it can find the wedges' draw.

## Precise readbacks: the boot hang (2026-10-22)

`VW_READBACKS=2` hung the boot at about 3 s. The guest thread dump showed a thread in
`sceKernelWaitEqueue` (from 0x437988: the GPU's end-of-pipe event) and the main thread waiting on
it. The host thread dump (symbolized with `llvm-symbolizer --obj=out/vwport.exe`) showed the cause:
`shadPS4:GpuCommandProcessor` spinning in `Scheduler::WaitDeferredSignals` (liverpool.cpp:191), the
signal waiting for a copy-thread job, and `vw:Copy0` inside the fault handler, in
`BufferCache::ReadMemory` -> `SendCommand` to the GPU thread. Precise mode read-protects a range as
soon as a binding marks it GPU-modified, which can happen before a copy queued for it runs: the copy
thread faults on its own source. Fix (untested): queued copies (`CopyQueuedGuest`, buffer_cache.cpp) read
through the backing view in Precise mode; large copies no longer split across the copy threads
(the caller handles its faults inline); a speculative reader (draw preparation) gives up on a read
fault instead of waiting for the GPU thread (probe.c). `kyoei_precise_test.bat` is the test.

Owner's run with the fix (log run-20261022-054948): the boot hang is gone; a save loaded straight into
the driving stage (stage203). 4 s into its loading: read fault at 0x498bf6, in the second bone loop
of the function that also calls 0x4753c0 (the earlier driving-chapter crash). The int index read
from [rdi+0x60][44] was 0x08450390, the low half of a pointer next to rbx (0x100844c310): the index
list holds other data. Precise readbacks did not prevent it, so this crash is not stale GPU data:
it is the same class as the other garbage index lists (a game race or a freed and reused list).
Whether Precise removes the vertex explosions is still open (the run never reached driving).

Next test: patch "Skip Bad Bone Indices" (both loops of 0x498950: index above 0xffff skips the entry;
stubs 0x124a250-0x124a290) so the Precise run gets through the stage203 load and the owner can see
whether the vertex explosions remain with Precise readbacks.

Owner's Precise run with the bone patch (log run-20261022-060421): the stage203 load passed and driving
started; **the vertex explosions were still there**, and the game faulted soon after at 0x4753c0
(rsi=0x40: FUN_0049fce0's second loop, 0x49fec8, element index 7154 passed the 0xffff bound and its
+8 pointer was NULL). Precise readbacks change neither the explosions nor the bad index lists: stale
GPU data is ruled out for both. Next test: `kyoei_15fps_test.bat` (Relaxed, 15 FPS) in the driving
chapter: gone at 15 FPS means timing (a game race); still there means the emulation.

Owner's 15 FPS run (log run-20261022-061006): the vertex explosions were there while driving, after
getting off the truck, and again after saving and reloading there (Mothra circles the whole stage).
After 800 s: a corrupt command buffer (`Unknown PM4 type 3 opcode 0xd`, the words around it floats:
1.0f, 10.0f, 128.0f). Not a race then: data wrong from the start, or overwritten the same way each
time. Next test: `kyoei_readlog_test.bat` (30 FPS, `VW_READ_LOG=1` + `VW_WRITE_LOG=1`): a fault or a bad
packet lists the file reads (runtime_file.c) and GPU writes near the damaged data.

Read-log run (log run-20261022-063237, 30 FPS, driving): corrupt command buffer again; no file read
and no GPU write near it. **Every first bad packet in every log follows `c0016900 000002aa 000000ff`**
(SET_CONTEXT_REG IA_MULTI_VGT_PARAM = 0xff, written by sceGnmSetVgtControl, which is correct: 3 dwords).
Its only caller is the game's wrapper 0x4265a0 (writes at cb+0x10, `addq $0xc`), called from 0x237f16,
0x434cdf and 0x434ff1 (draw path, cb = ctx+0x80). The words after it are an older packet's tail or stale
data: the cmd pointer used was stale (another writer advanced cb+0x10). None of the stubbed Gnm
functions is called directly by the game. Next: read 0x434c00-0x435100 and 0x237f16 for a cached cmd
pointer or a second thread on the same cb (ctx+0x80).

0x426a10 is the game's allocateFromCommandBuffer (NOP header c0xx1000 + 0x6875xxxx marker, embedded data
after it): floats inside a command buffer are expected. Hypothesis that fits every case: the game writes
a command buffer again (the next frame) while the GPU thread is still reading the earlier submission; the
bad packet is where the new frame's writing had got to. If so, the same reuse would make the GPU read the
next frame's embedded constants (vertex explosions that do not depend on FPS). Test: `VW_DCB_CHECK=1`
(in `kyoei_readlog_test.bat`) compares a corrupt buffer with a copy taken at submission. Note
`VW_COPY_GPU_BUFFERS` is not a fix as it stands: SubmitDone resets its copy area while the GPU thread may
still read an earlier copy.

First `VW_DCB_CHECK` run (log run-20261022-074625): the owner saw **no vertex explosions, and fog and
lightning drawn for the first time**; a corrupt buffer after 983 s in a cutscene. The buffer
(0x105ab37844, 30998 words) had 3 submissions among the last 64; vs the oldest (170 ms, 55 submissions
earlier) 23027 words had changed. That compare was against the wrong submission (the GPU thread may read
the newest), so the check now marks the submission the GPU thread began (StartDcb), prints every logged
submission of the buffer and the backlog. Open: why the rendering got better with the check on (its
only side effect is a copy of each buffer at submission, i.e. a delay on the submitting thread).

Second `VW_DCB_CHECK` run (log run-20261022-081030): fog and lightning drawn, vertex explosions back
(so the earlier clean run was luck, not the check). The corrupt buffer was **exactly as submitted**
(0 words changed; 7 submissions queued behind it): the game itself submitted it, so it is not a reuse
race with the GPU thread. Layout: `c0012d00 1770 2` (DrawIndexAuto) + 4-word trailing NOP (correct, our
sceGnmDrawIndexAuto), then `c0016900 2aa ff` (SetVgtControl), then `68752000` + floats: an
allocateFromCommandBuffer block whose NOP header is missing (overwritten by the 0xff). The writes
overlap: two writers with different command pointers on one buffer. Next test: `VW_CB_THREADS=1`
(instrumented the 44 sceGnm functions that take cmdbuf) prints two guest threads writing the same
64 KiB block within 1 ms, with their call sites.

`VW_CB_THREADS` run (log run-20261022-082343): the 20 reports were all one benign pattern, every frame:
thread 7 calls SetVgtControl at 0x105AB37814 (and 0x105A29BC1C) and thread 6, 0.1-0.5 ms later, at
0x105AB37844 (0x105A29BC4C), 0x30 bytes on: a handoff between adjacent pieces of buffer, not an overlap
(most frames are fine). 0x105ab37844 is the buffer that gets corrupted, so threads 6 and 7 are the
candidates. The check now reports only a thread writing inside the range another thread wrote in the
last 5 ms. The run faulted in the stage203 load at 0x4753c0 (NULL element pointer, FUN_0049fce0 loop 2):
patch "Skip NULL Mesh Elements" (0x49febf, and the same read at 0x498c0a; stubs 0x124a2a0-0x124a2ca).

Next run (log run-20261022-083042) hung at "Loading": a fault in 0x4753c0 again (FUN_0049fce0 loop 2,
same element array 0x1007c5c558, index 3067, element +8 = 0xc432879a3f800000: two floats), and the fault
report itself faulted (the write log dumped memory around rax = 1.0f) and slept forever. Fixed: the dump
reads through the backing view. The NULL element guard also skips pointers above 40 bits. The bad index
differs per run (7154, 3067) for the same object: the list is wrong when read, not wrong in the file.

Run run-20261022-083630: fault in 0x4753c0 again before any explosion (object 0x1007d2b7b8, index 1227,
element +8 = 0x3f800000, below 40 bits). No `CB threads` overlap. Every one of these faults has the same
stack (0x23cfee -> 0x238d5c -> 0x477984 -> 0x47b834 -> 0x480fa1: the scene update, the family of the
unloaded NPC entries) and comes during or after a stage load, at any FPS. A timing difference FPS does
not change: file reads finish at once on a PC. Test: `kyoei_slowread_test.bat` with
`VW_READ_THROTTLE=60,3000` (runtime_file.c: each read sleeps 3 ms + size at 60 MB/s).

`kyoei_slowread_test.bat` (log run-20261022-084052): the same fault in the stage203 load (object
0x1007d2b7b8, index 6917, element +8 = 1.0f). Slow reads change nothing: ruled out. Next:
`VW_FAULT_MESH=1` (probe.c mesh_dump) prints the object at [rbp-0x78], its index list (count, min, max,
entries around r12) and the first element whose node pointer stops looking like an address.

**Caught** (log run-20261022-084355, explosions on, fault in a cutscene): `CB threads: thread 6
(sceGnmSetVgtControl from 0x4265eb) writes at 0x105a2c1668, inside 0x105a2c0090-0x105a2c16b4 that
thread 7 wrote 0.000 ms before (last sceGnmSetVsShader from 0x4260fe)`; 47 ms later the GPU thread hit
a bad packet at 0x105a2c1674 in the buffer at 0x105a29bc4c. Again at 245 s (thread 7: SetPsShader350
from 0x425fd5). Two game threads write one command buffer at the same time: the corrupt buffers (and
likely the explosions) come from that. The draw function at 0x434a10 takes its context (cb at
ctx+0x80) as an argument. The overlap report now prints both threads' guest stacks (frame-pointer chain)
to find where they get the same context. Also: the run without explosions had a very slow pipeline
warm-up (64 of 1613 in 10 s; normally all in ~4 s), i.e. different thread timing.

Run run-20261022-085328 (to the motorbike part, 1027 s): two more overlaps. At 1025.313 s thread 6 wrote
SetVgtControl at 0x105ab37bd4 and 0.001 ms later thread 7 wrote SetVsShader at **the same address**: both
threads used one command buffer with the same write pointer; 60 ms later a bad packet in that buffer.
Thread 7 calls come from 0x4260fe (SetVsShader), 0x425b48 (DrawIndex), 0x425fd5 (SetPsShader350); thread
6 from 0x4265eb (SetVgtControl). The frame-pointer stacks printed nothing useful (the chain does not
reach the guest frames from the sceGnm functions); probe.c runtime_stack_scan now scans the stack for
return addresses into the game instead.

Stacks (log run-20261022-092202, overlaps at 126 s): both threads draw through
0x425a4b/0x4260fe/0x425fd5 <- 0x23a7b2 <- 0x4819ee <- 0x477b15, reached from FUN_0023be90 by two paths:
0x239057 <- 0x23cfee <- 0x372def, and 0x23ce45 <- 0x3719d3 <- 0x372bf8 (each from the thread entry
0x321a80). The context is [r14+0x50] (r14 a renderer object, see 0x23a784): the two paths share it, so
the game assumes they never run at the same time. Our scePthreadGetaffinity returns the set masks
(not the cause). Open: how the PS4 keeps the two apart (same core with FIFO, or a lock/flag of ours).
The overlap report now names each thread with its priority and core mask.

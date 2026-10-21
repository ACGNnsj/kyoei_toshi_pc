# Porting another PS4 game with this code base: lessons

Written while bringing up City Shrouded in Shadow (CUSA03562) on the Unicorn Overlord port
(2026-10-21/22). Unicorn Overlord is a 2D game; this one is a full 3D engine, so most of what
broke here will break again for the next 3D game. Details: `docs/dev/kyoei/HANDOFF.md`.

## Day one: what to check before writing code

1. **Dump format.** `scripts/prepare.py` needs a decrypted (fake-signed) `eboot.bin`. Run it,
   then `link_libc.py` and `link_modules.py` into a scratch folder: they report imports, the
   modules that link natively, and what is left for the runtime. `APP_VER` in param.sfo is the
   version the launcher checks; `VERSION` is a different field.
2. **Imports.** Compare the leftover imports with the runtime (names come from
   `src/import_names.inc`; unknown NIDs: shadPS4's `src/core/aerolib/aerolib.inl`). The only
   authoritative number is `vwport.exe <boot-linked.bin> --check-imports` once built.
3. **Bundled modules.** Link what the game ships (`sce_module/`, also `module/prx/`) natively
   when possible: NpToolkit2 removed ~50 C++ imports. Modules the game loads at run time with
   `sceKernelLoadStartModule` (per-stage PRXs) need `stage_modules` in the profile.
4. **SDK age.** Older SDK libcs have plain symbol names (`__start__Ztext`); link_libc.py now
   skips them.

## Runtime pitfalls found

- `DT_INIT` and `DT_FINI` of a PRX take `(args, argp, entry)`: pass NULL as `entry`, or the
  module calls whatever was in rdx (crash on the first stage unload).
- Stub return values: take them from shadPS4's libraries (hmd, camera, move, pngenc, ...), not
  guesses. A device API that "succeeds" can send a game into a path for hardware it lacks.
- A Japanese game shows Japanese in port UI (IME prompt): merge a system CJK font into ImGui.
- PNG encoding: write real PNGs (stored deflate is enough); keep big buffers off the stack
  (guest threads can have 64 KiB stacks).

## GPU translation pitfalls found

- **Readbacks.** `VW_DEFERRED_READBACK`/`VW_STALE_READBACK` (on for Unicorn Overlord) hand the
  CPU a previous frame's copy: a 3D game that reads GPU results breaks (NULL list entries).
  Depth-of-field autofocus and menu backgrounds need small render targets written back:
  `VW_READBACK_LINEAR=2` (8x8 or less). Writing back every linear image (=1) risks late writes
  over memory the game reused.
- **Precise readbacks** (`VW_READBACKS=2`) read-protect GPU-written pages; a read fault sends
  a download to the GPU thread and waits. A helper thread (the copy threads) that reads guest
  memory while the GPU thread waits for it deadlocks: queued copies read the backing view.
  How it was found: guest dump (a thread in `sceKernelWaitEqueue`), then `VW_HANG_DUMP` host
  thread dump symbolized with `llvm-symbolizer --obj=out/vwport.exe` (GPU thread spinning in
  `WaitDeferredSignals`, `vw:Copy0` inside a fault handler in `SendCommand`).
- **Alpha to coverage** (`DB_ALPHA_TO_MASK`, 0xA2DC) was never read: hair and eyelashes
  rendered as opaque blocks. Now a pipeline key bit.
- **SRT walker fault handler** was registered only for freshly compiled shaders; with a warm
  pipeline cache every walker came from the cache and NULL resource pointers crashed the game.
- **Type-0 PM4 packets**: a game can leave stray words in its command buffer that the real
  command processor eats as type-0 register writes and resyncs after. Skip them as the
  hardware does (header + count + 1 words) instead of stopping.
- `VW_FRAME_CAPTURE_KEY=1` + F11 captures between "display passes"; a game that draws the
  display buffer rarely makes `VW_CAPTURE_FRAMES` span minutes and hundreds of MB.

## Game-side races exposed by a fast PC

- Many PS4 games ran at 15-30 FPS on 1.6 GHz Jaguar cores; their thread races never showed.
  On a PC they do: NULL shaders in the draw context, NULL list entries, garbage index lists.
  The frame rate cap is a cheap first diagnosis: faults that fall with the cap are races.
- **Spin counts as timeouts**: a loop that gives up after N spins (10M here, then `int 0x41`)
  is milliseconds on a PC. Patch it to wait.
- `VW_PS4_SCHEDULING=1` applies the game's core pinning *and* priorities (pinning alone made
  things worse: equal host priorities time-slice on one CPU).
- Runtime primitives (mutex, cond, sema, rwlock) were reviewed against PS4 semantics and were
  right: look at the game and the GPU path before blaming them.

## Patching game code

- Patches are GoldHEN/shadPS4 XML, checked against the eboot's sha256 (`scripts/patches.py`).
- Code caves: dead code a patch has made unreachable, the rest of the code segment's last
  page, and (since `prepare.load_end`) the unused pages up to the next segment (8 KiB here).
- Always: verify the original bytes, re-encode with the same semantics, disassemble the result
  (`llvm-objcopy -I binary` + `llvm-objdump`), check every branch target, and look for sibling
  functions with the same code (search the byte pattern): there were seven copies of one bug.

## Working with the owner

- The owner tests by playing; automated input runs collide with that. Prepare changes, check
  them offline (patch compile, disassembly, prepare/link dry runs), hand back a ready
  `kyoei.bat`, and read the newest `run_kyoei\logs\*.log` afterwards.
- Heredocs in the Bash tool here eat backslashes (`\n`, `\0`, paths): write scripts and C
  string literals with the Write/Edit tools.

# City Shrouded in Shadow PC (巨影都市, vwport)

[![License: GPL v2+](https://img.shields.io/badge/license-GPL--2.0--or--later-blue.svg)](LICENSE)

A native Windows port of the PlayStation 4 version of **City Shrouded in Shadow** (巨影都市,
Kyoei Toshi), built on [hachi23/unicorn_overlord_pc](https://github.com/hachi23/unicorn_overlord_pc).

| Dump | State |
|---|---|
| CUSA03562 (Japan, JP0700-CUSA03562_00-E060000000000001): `param.sfo` VERSION 01.03, APP_VER 01.00 | Played through the driving chapter and beyond at 60 FPS; saving and loading work |

Only this dump is supported. The port's code patches are checked against its `eboot.bin`
(SHA-256 `42b058e57358cdb0516fb39ddafe583634b03d9987a7950f8fd0c317a8f3af83`): other versions,
earlier or later than 1.03, are not guaranteed to work. The setup warns about another version
or `eboot.bin` build but lets you try it; the code patches are then left out.

> **Work in progress.** The game has not been played to the end on this port. Known issues are
> listed [below](#known-issues).

### What it is

Not an emulator, and not a decompilation. The PS4 and a PC use the same kind of processor
(x86-64), so the game's own executable from your dump runs **directly on the PC's CPU**. What a
PC doesn't have is the PS4's operating system and GPU, so this project supplies both:

1. **Preparation** (`scripts/`): reads the game's `eboot.bin`, fixes up its addresses and links
   the game's bundled system modules into one image.
2. **Loader** (`src/probe.c`): loads that image into memory, applies a few small code patches
   (`patches/Kyoei_Toshi.xml`) and jumps into the game's code.
3. **Runtime** (`src/runtime_*.c`): the PS4 system functions the game calls (memory, threads,
   files, audio, video decoding, controller, saves, and offline stand-ins for PSN).
4. **GPU** (`gpu/`): the [shadPS4](https://github.com/shadps4-emu/shadPS4) video core turns the
   PS4's GPU commands and shaders into Vulkan.
5. **Start program** (`launcher/`): a one-time setup that installs your dump, then starts the game.

> **No game files are included.** You need a dump of your own copy. This project is not
> affiliated with Sony Interactive Entertainment, Bandai Namco Entertainment or Granzella.

## Download

Download the Windows build from the [releases](https://github.com/ioritree/kyoei_toshi_pc/releases).
Extract `Kyoei-Toshi-PC-windows.zip` into its own folder and run `Kyoei Toshi.exe`; it keeps its
`data/` folder beside itself. Keep `LICENSE` and `licenses/` with it.

## Requirements

- Windows 10 (1903+) / 11, 64-bit.
- A Vulkan 1.3 GPU.
- Your own dump of the game (the version above): the folder `CUSA03562` containing `eboot.bin`, `sce_module`,
  `sce_sys`, `module` and `DAT` (about 17 GB).

## Playing

**First start:** a folder dialog asks for your dump (the `CUSA03562` folder). The start program
checks it (title and version), installs it into `data/game` and starts the game. On the same
drive the install takes a moment and needs no extra space (hard links); from another drive it
copies the files with a progress window, and a cancelled install continues on the next start.
**Later starts** go straight into the game.

The first start also compiles the game's shaders while you play (they are cached for later
starts); objects may appear a moment late the first time.

In game, **Insert** (R3+L2 on a controller) opens the port's menu: display mode, VSync, render
resolution, controls and the performance overlay. Settings are kept in `vw.ini` in `data/`. The
menu also has a **Change game folder** button.

### Controllers

Any controller Windows knows works (Xbox, DualShock 4, DualSense, ...); the game sees a PS4
controller. On an Xbox controller: A = ✕, B = ○, X = □, Y = △, Menu = Options, View = touchpad.
This is a Japanese release: **○ (B) confirms and ✕ (A) cancels**, also in the save and load list.

### Keyboard

| Key | Button |
|---|---|
| WASD | left stick |
| Arrows | right stick |
| Space | Cross |
| Left Shift | Circle |
| E | Square |
| Q | Triangle |
| 1 and 3 | L1 and R1 |
| R and F | L2 and R2 |
| Z and C | L3 and R3 |
| I, K, J, L | d-pad |
| Enter | Options |
| Tab | touchpad |
| Insert | the port's menu |

Every run writes a log to `data/logs/`. When the game stops with an error, the start program
offers to save a diagnostics ZIP (the run log with your folders redacted, shown for review
first) to attach to an issue. No game files or saves are included.

## Known issues

- The pause menu background is sometimes garbage (it comes and goes between starts).
- The game has not been played to the end on this port.

## Building

Build on Windows in an **MSYS2 CLANG64** shell
([packaging/windows/README.md](packaging/windows/README.md)):

```bash
git clone --recursive https://github.com/ioritree/kyoei_toshi_pc
cd kyoei_toshi_pc
bash build.sh                          # out/vwport.exe
```

For development, `kyoei.bat` starts the game from the source tree (edit `VW_GAME_DIR` in it to
point at your dump); `python launcher/kyoei_launcher.py` runs the start program instead. To
package a release: `bash packaging/windows/build.sh kyoei` writes `dist/Kyoei Toshi PC/` and its zip.

Developer notes (the state of the port, the bugs found and how) are in
[docs/dev/kyoei/HANDOFF.md](docs/dev/kyoei/HANDOFF.md), and lessons for porting another PS4 game
on this code base in [docs/dev/PS4_PORTING_LESSONS.md](docs/dev/PS4_PORTING_LESSONS.md).

## Credits

This port stands on the work of others:

- **hachi23**: [unicorn_overlord_pc](https://github.com/hachi23/unicorn_overlord_pc), the
  Unicorn Overlord port this repository is built on, and its contributors (yogesh1239 and others).
- **deadinside28**: [bloodborne_pc](https://github.com/deadinside28/bloodborne_pc), the original
  native port that this approach comes from.
- **Supermedo**: [bloodborne_pc for Windows](https://github.com/Supermedo/bloodborne_pc).
- **Felipe Prado**: [gow3_pc](https://github.com/FelipePrado0/gow3_pc).
- **cuesta4**: [shadPS4 fork](https://github.com/cuesta4/shadPS4), for its GNM blend rewrite,
  depth/stencil state and image aliasing fixes.
- **shadPS4 team**: the [shadPS4](https://github.com/shadps4-emu/shadPS4) video core and shader
  recompiler (vendored in `gpu/shadps4/`, see [gpu/VENDOR.txt](gpu/VENDOR.txt)), and its PS4
  function names and library layouts.

Libraries used include [LibAtrac9](https://github.com/Thealexbarney/LibAtrac9),
[FFmpeg](https://ffmpeg.org), [SDL3](https://github.com/libsdl-org/SDL),
[Dear ImGui](https://github.com/ocornut/imgui), [sirit](https://github.com/shadps4-emu/sirit),
[glslang](https://github.com/KhronosGroup/glslang),
[Vulkan Memory Allocator](https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator),
AMD FidelityFX FSR 1 and CAS, [Zydis](https://github.com/zyantific/zydis),
[xbyak](https://github.com/herumi/xbyak), [fmt](https://github.com/fmtlib/fmt),
[Boost](https://www.boost.org), [xxHash](https://github.com/Cyan4973/xxHash),
[zlib](https://zlib.net), [miniz](https://github.com/richgel999/miniz),
[Pillow](https://python-pillow.org) and [PyInstaller](https://pyinstaller.org). Their licenses
are in `licenses/` of each release.

## License

GNU GPL v2 or later ([LICENSE](LICENSE)). Third-party components keep their own licenses.

City Shrouded in Shadow is a trademark of its owners. This project contains no game code or
assets; it only runs a copy you dumped yourself.

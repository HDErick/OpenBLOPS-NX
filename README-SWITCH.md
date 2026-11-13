# OpenBLOPS-NX on Nintendo Switch

OpenBLOPS-NX runs the whole game on the Switch: the Campaign, Zombies (the base
game and the DLC maps), and Multiplayer. It is built with devkitPro and libnx
and renders through **DXVK**: the engine's Direct3D 9 runs on a native ARM64
DXVK over Vulkan (Mesa NVK), not through OpenGL. Every mode renders at
**1280x720**, handheld and docked.

You need your own copy of the retail PC game. No game data is in this
repository and none ever should be. See the [project README](README.md) for an
overview and the PC build.

## The two executables

| File | Modes |
|---|---|
| `OpenBLOPS-NX.nro` | Campaign and Zombies (the single-player executable) |
| `OpenBLOPS-NX-MP.nro` | Multiplayer |

Both are version 0.1.0 and live in `sdmc:/switch/OpenBLOPS-NX/` next to the game
data. Each can start the other (the SP <-> MP button in the menus).

## Installing

The [web installer](installer/README.md) does all of this in the browser: choose
your PC Black Ops folder and it produces `switch/OpenBLOPS-NX/` (a ZIP or a
folder) with both executables from the latest release, your game data and the
converted zones. By hand, the folder holds:

```
sdmc:/switch/OpenBLOPS-NX/
  OpenBLOPS-NX.nro
  OpenBLOPS-NX-MP.nro
  main/              from the PC game
  zone/              from the PC game
  localization.txt   from the PC game
  kbz/               the converted zones (see below)
```

**Launch in application mode**, not from the album applet: hold **R** while
opening any installed game to enter the Homebrew Menu, then start
`OpenBLOPS-NX` (Campaign, Zombies) or `OpenBLOPS-NX-MP` (Multiplayer). The
applet has too little memory for the game.

### Zones: KBZ

The PC fastfiles (`zone/*.ff`) are written for a 32-bit engine: pointers and
structures inside them have x86 sizes. The Switch build is 64-bit, so each zone
is converted once into a `.kbz` with the 64-bit layout (`tools/ffconv`, which
the web installer runs as WebAssembly). The game loads `kbz/` and never the
`.ff` files. To convert from a shell instead (UCRT64, after
`tools/ffconv/build.sh`):

```sh
GAME=/path/to/black-ops sh tools/nx/convert-zones.sh
```

## Settings

`sdmc:/switch/OpenBLOPS-NX/cmdline.txt` is appended to the command line at
start, so any `+set dvar value` there overrides the defaults (a map command in
it is moved to the end, so `+set` lines always apply first). Graphics settings
saved in `config.cfg` (in the same folder) also override the built-in defaults;
delete it to return to them.

- `playername.txt` (first line) is the player name; first start writes a
  random `playerNNNN` there.
- `dxvk-sarek.conf` next to the executables configures DXVK (a copy is in
  `switch-meta/`).
- Online play: the community server browser and public hosting need a
  directory endpoint, `+set bo1_directory IPv4:port` (or the build's default).
  Direct-IP and offline matches with bots need nothing.

## Controls

Face buttons are mapped by **position** (`src/nx/nx_xinput.cpp`): the game is
written for an Xbox pad, so in game the Switch's bottom button does what the
Xbox A does (jump). Menus swap the pair back to the Nintendo convention: **A
confirms, B goes back**. Prompts show the Switch's labels (`nx_switchglyphs 1`).

| Switch | Menus | In game |
|---|---|---|
| D-pad / left stick | move focus | move |
| A (right) | confirm | the Xbox B binding |
| B (bottom) | back | the Xbox A binding (jump) |
| Y (left) / X (top) | Y refreshes the server browser | Xbox X / Xbox Y |
| + / − | Start / Back | pause / scores |
| ZL / ZR | | aim / fire |

Text fields (server name, passwords) open the Switch keyboard.

## Building

devkitPro (devkitA64, libnx) under its MSYS2 shell, with the pinned Mesa and
DXVK dependencies prepared in `build-nx/deps/`:

```sh
sh tools/nx/build-sarek.sh
```

It builds the native DXVK once (`build-nx/NX-build/renderer`), then the two
executables into `build-nx/NX-build/`. The matching ELFs, which crash reports
are resolved against, stay in `build-nx/NX-build/sp/KisakBlack.elf` and
`build-nx/NX-build/mp/KisakBlack.elf`; keep them with the NROs they belong to.

The NRO names and NACP (title, author, version) are in `cmake/switch.cmake`.
Both NROs use libnx's default icon.

## Debugging

**Log.** Everything the game prints goes to
`sdmc:/switch/OpenBLOPS-NX/kisakblack.log` through an asynchronous ring, so
printing never waits on the SD card. Assertions print an `ASSERTBEGIN` /
`ASSERTEND` block before they stop the game.

**Crashes.** On a fault the log gets an `[nx-crash]` block: the exception, `pc`,
`lr`, the fault address, the registers, and the code addresses found on the
stack as offsets into the ELF. Resolve them with the ELF of the **same** build:

```sh
$DEVKITPRO/devkitA64/bin/aarch64-none-elf-addr2line -f -C -i -e build-nx/NX-build/sp/KisakBlack.elf 0x52b344 0x533250
```

The stack list is a scan, not a frame walk: read it top down, and expect some
stale addresses. A trap right after an `ASSERTBEGIN` block is an engine assert,
not a fault. A fault address that looks real with its top bits missing is a
pointer that went through 32 bits somewhere (an LP64 bug). Atmosphère reports
in `sdmc:/atmosphere/crash_reports/` resolve the same way.

**Profiling.** `+set nx_telemetry 1` adds periodic `[nx-prof]` reports (the
costliest scopes per frame, per thread) and an `[nx-hitch]` line for every slow
frame with the scopes that took its time. Off by default.

**Memory.** `[nx-mem]` lines report the heap every few seconds and every large
allocation or refusal. Large blocks end against a guard page, so an overrun
faults at the writing instruction instead of corrupting the heap.

**LP64 checks.** `sh tools/nx/lp64check.sh <file.cpp>` and
`sh tools/nx/lp64scan.sh <src subdirs>` list pointer truncations the way a
sanitised build would.

## Credits

See the [project README](README.md#credits).

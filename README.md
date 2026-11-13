# OpenBLOPS-NX

OpenBLOPS-NX is a source reconstruction of **Call of Duty: Black Ops (2010)** for Windows PC and Nintendo Switch. Campaign, Multiplayer, and Zombies are playable, including the base-game and DLC Zombies maps.

The port was made primarily by me and [NaGaa95](https://github.com/NaGaa95/)

- **Campaign** uses the single-player build and supports saving and resuming at checkpoints.
- **Multiplayer** uses a separate build. Direct-connect builds include a community server browser, public and password-protected hosting, and offline matches with bots.
- **Zombies** runs from the single-player build. It supports solo play and online co-op in direct-connect builds with Zombies networking.
- **Local split-screen** is experimental and requires a build with split-screen enabled.

You need your own copy of the PC game and the DLC files for any DLC you want to play. This repository contains source code and tools. Game data comes from your installation.

## Source and setup

- **Windows PC.** With Visual Studio 2022, CMake, and the June 2010 DirectX SDK, run `./tools/pc/build.ps1 -GameFolder "C:/path/to/Call of Duty Black Ops"` in PowerShell. It builds `OpenBLOPS.exe` (Campaign and Zombies) and `OpenBLOPS-MP.exe` (Multiplayer) in `build-nx/PC-build/`.
- **Nintendo Switch.** Use the [web installer](installer/README.md) to combine the [latest release](https://github.com/HDErick/OpenBLOPS-NX/releases/latest) with your PC game data and convert the installed zones. To build from source with devkitPro and the project’s Mesa/DXVK dependencies, run `sh tools/nx/build-sarek.sh`. Both NROs go to `build-nx/NX-build/`. [Switch development notes](README-SWITCH.md)

Install the Switch game data and both NROs in `sdmc:/switch/OpenBLOPS-NX/`. Open Homebrew Menu in application mode by holding **R** while opening an installed game. Choose `OpenBLOPS-NX.nro` for Campaign or Zombies or `OpenBLOPS-NX-MP.nro` for Multiplayer.

Online server discovery and public listings require a community directory endpoint: set `bo1_directory` to its `IPv4:port` (see [Switch notes](README-SWITCH.md#settings)). Direct-IP and offline play are also available.

## Credits

- **Multiplayer implementation** came from SwagSoftware's [KisakBlack](https://github.com/SwagSoftware/KisakBlack).
- **Initial base-game Zombies implementation** by [femboyvalve](https://github.com/femboyvalve).
- **Zombies DLC and the full single-player Campaign implementation** by [NaGaa95](https://github.com/NaGaa95/), who also contributed the original devkitPro scaffolding and mesa-switch work.
- **Installer idea and compression techniques** by [Chansey](https://github.com/ChanseyIsTheBest), [xsaja](https://github.com/xsaja) and [StevensND](https://github.com/StevensND).

Licensed under [GPL-3.0](LICENSE).
